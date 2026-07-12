#include "AiController.h"
#include "FileAgent.h"
#include "CommandRunner.h"
#include "ChatManager.h"
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QDebug>
#include <QSettings>
#include <QTextCodec>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QSslSocket>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>

AiController::AiController(QObject *parent)
    : QObject(parent),
      m_manager(new QNetworkAccessManager(this)),
      m_fileAgent(new FileAgent(this)),
      m_cmdRunner(new CommandRunner(this)),
      m_chatManager(new ChatManager(this))
{
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &AiController::onReplyFinished);

    // Bypass system proxy to avoid "proxy connection refused" on machines with proxy settings
    m_manager->setProxy(QNetworkProxy::NoProxy);

    // Auto-detect Qt/MinGW environment paths for portability
    QStringList qtBins;
    QString mingwBin = QStringLiteral("G:/QT/Tools/mingw730_32/bin");
    QString qtBin    = QStringLiteral("G:/QT/5.14.2/mingw73_32/bin");
    if (QDir(mingwBin).exists()) qtBins << mingwBin;
    if (QDir(qtBin).exists())    qtBins << qtBin;
    if (!qtBins.isEmpty()) {
        m_cmdRunner->setQtEnv(qtBins.join(";"), "");
    }
}

AiController::~AiController()
{
    if (m_currentReply) {
        m_currentReply->abort();
    }
}

// ==================== 发送消息（统一入口） ====================
void AiController::sendMessage(const QString &message)
{
    if (message.trimmed().isEmpty()) return;

    if (m_apiKey.isEmpty()) {
        emit errorOccurred(QStringLiteral("API Key not set"));
        return;
    }

    if (m_loading && m_currentReply) {
        m_currentReply->abort();
        m_loading = false;
        emit loadingChanged();
    }

    m_stoppedByUser = false;
    addUserMessage(message);

    if (m_agentMode) {
        if (m_projectDir.isEmpty()) {
            emit errorOccurred(QStringLiteral("Project directory not set"));
            setStatus(QStringLiteral("Please select project directory"));
            return;
        }

        m_loading = true;
        emit loadingChanged();
        setStatus(QStringLiteral("Agent thinking..."));

        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = message;
        m_agentMessages.append(userMsg);

        sendAgentRequest();
    } else {
        m_loading = true;
        emit loadingChanged();
        setStatus(QStringLiteral("Connecting..."));

        m_response.clear();
        m_streamBuffer.clear();

        QUrl url(m_apiUrl);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
        request.setRawHeader("Accept", "text/event-stream");

        QJsonObject body = buildRequestBody(message);
        QByteArray jsonData = QJsonDocument(body).toJson();

        m_currentReply = m_manager->post(request, jsonData);
        connect(m_currentReply, &QNetworkReply::readyRead, this, &AiController::onReadyRead);

        QTimer::singleShot(m_timeoutSec * 1000, this, [this]() {
            if (m_currentReply && m_currentReply->isRunning()) {
                m_currentReply->abort();
                setStatus(QStringLiteral("Timeout"));
                m_loading = false;
                emit loadingChanged();
                emit errorOccurred(QStringLiteral("Request timeout"));
            }
        });
    }
}

// ==================== 停止生成 ====================
void AiController::stopGeneration()
{
    m_pendingToolCalls.clear();
    m_toolResults.clear();

    if (!m_pendingAction.isEmpty()) {
        m_pendingAction.clear();
        emit pendingActionChanged();
        emit hasPendingActionChanged();
    }

    if (m_currentReply && m_currentReply->isRunning()) {
        m_stoppedByUser = true;
        m_currentReply->abort();
    }

    // Agent mode: if we stopped during a tool_call loop, remove the incomplete
    // assistant message (and any tool messages already appended) so the history
    // remains valid for the next request.
    if (m_agentMode && m_agentLoopStartIndex >= 0 &&
        m_agentLoopStartIndex < m_agentMessages.size()) {
        qDebug() << "Agent stop: truncating messages from index" << m_agentLoopStartIndex;
        while (m_agentMessages.size() > m_agentLoopStartIndex) {
            m_agentMessages.removeLast();
        }
        m_agentLoopStartIndex = -1;
    }

    setStatus(QStringLiteral("Stopped"));
    m_loading = false;
    emit loadingChanged();

    if (!m_response.isEmpty() && !m_agentMode) {
        finalizeAiMessage();
    }
}

// ==================== Agent 模式：发送请求 ====================
void AiController::sendAgentRequest()
{
    QJsonObject body;
    body["model"] = m_model;
    body["temperature"] = 0.0;
    body["max_tokens"] = 8192;
    body["stream"] = false;
    body["messages"] = m_agentMessages;
    body["tools"] = buildToolDefinitions();

    QUrl url(m_apiUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QJsonDocument doc(body);
    QByteArray jsonData = doc.toJson();
    qDebug() << "Agent request size:" << jsonData.size() / 1024.0 << "KB";
    setStatus(QStringLiteral("Sending %1 KB request...").arg(jsonData.size() / 1024.0, 0, 'f', 1));

    m_currentReply = m_manager->post(request, jsonData);
    // Agent requests also need a timeout
    QTimer::singleShot(m_timeoutSec * 1000, this, [this]() {
        if (m_currentReply && m_currentReply->isRunning()) {
            m_currentReply->abort();
            setStatus(QStringLiteral("Request timeout"));
            m_loading = false;
            emit loadingChanged();
            emit errorOccurred(QStringLiteral("Agent request timeout after %1 seconds").arg(m_timeoutSec));
        }
    });
}

// ==================== Agent 模式：处理响应 ====================
void AiController::onAgentReplyFinished(QNetworkReply *reply)
{
    if (m_stoppedByUser) {
        m_stoppedByUser = false;
        reply->deleteLater();
        m_currentReply = nullptr;
        m_loading = false;
        emit loadingChanged();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errMsg = reply->errorString();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray errBody = reply->readAll();
        QString bodyText = QString::fromUtf8(errBody);
        if (!bodyText.isEmpty()) {
            errMsg += QStringLiteral("\nHTTP %1: %2").arg(httpStatus).arg(bodyText);
        }
        qDebug() << "Agent request failed:" << httpStatus << bodyText;

        m_loading = false;
        emit loadingChanged();
        setStatus(QStringLiteral("Request failed"));

        QVariantMap errMap;
        errMap["role"] = "assistant";
        errMap["content"] = QStringLiteral("Network error: ") + errMsg;
        errMap["isError"] = true;
        m_displayMessages.append(errMap);
        emit messageHistoryChanged();

        reply->deleteLater();
        m_currentReply = nullptr;
        m_agentLoopStartIndex = -1;
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    m_currentReply = nullptr;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    if (root.contains("error")) {
        QString apiErr = root["error"].toObject()["message"].toString();
        m_loading = false;
        emit loadingChanged();
        setStatus(QStringLiteral("API error"));

        QVariantMap errMap;
        errMap["role"] = "assistant";
        errMap["content"] = QStringLiteral("API Error: ") + apiErr;
        errMap["isError"] = true;
        m_displayMessages.append(errMap);
        emit messageHistoryChanged();
        return;
    }

    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) {
        m_loading = false;
        emit loadingChanged();
        return;
    }

    QJsonObject choice = choices[0].toObject();
    QJsonObject message = choice["message"].toObject();
    QString finishReason = choice["finish_reason"].toString();

    m_agentMessages.append(message);

    QString content = message["content"].toString();
    if (!content.isEmpty()) {
        QVariantMap msg;
        msg["role"] = "assistant";
        msg["content"] = content;
        msg["isError"] = false;
        m_displayMessages.append(msg);
        emit messageHistoryChanged();
    }

    if (finishReason == "tool_calls" && message.contains("tool_calls")) {
        QJsonArray toolCalls = message["tool_calls"].toArray();
        // Remember where this tool loop started so we can roll back on Stop.
        m_agentLoopStartIndex = m_agentMessages.size() - 1;
        setStatus(QStringLiteral("Executing %1 tool calls...").arg(toolCalls.size()));
        executeToolCalls(toolCalls);
    } else {
        m_agentLoopStartIndex = -1;
        m_loading = false;
        emit loadingChanged();
        setStatus(QStringLiteral("Done"));
        // Auto-save chat after agent finishes
        saveCurrentChat();
    }
}

// ==================== Agent 模式：工具定义 ====================
QJsonArray AiController::buildToolDefinitions() const
{
    QJsonArray tools;

    auto makeTool = [](const QString &name, const QString &desc,
                       const QJsonObject &props, const QJsonArray &required) {
        QJsonObject tool, func, params;
        params["type"] = "object";
        params["properties"] = props;
        params["required"] = required;
        func["name"] = name;
        func["description"] = desc;
        func["parameters"] = params;
        tool["type"] = "function";
        tool["function"] = func;
        return tool;
    };

    QJsonObject emptyProps;

    // list_project_files
    tools.append(makeTool(
        "list_project_files",
        "List all source files in the project directory (.pro, .h, .cpp, .qml, etc.)",
        emptyProps, QJsonArray()));

    // list_files
    {
        QJsonObject props, pathProp;
        pathProp["type"] = "string";
        pathProp["description"] = "Directory path relative to project root (default: .)";
        props["path"] = pathProp;
        tools.append(makeTool("list_files", "List files and directories in a path", props, QJsonArray()));
    }

    // read_file
    {
        QJsonObject props, pathProp, startProp, endProp;
        pathProp["type"] = "string";
        pathProp["description"] = "File path relative to project root";
        startProp["type"] = "integer";
        startProp["description"] = "Start line number (1-based, inclusive). Default: 1";
        endProp["type"] = "integer";
        endProp["description"] = "End line number (1-based, inclusive). Default: end of file. Use this to read specific ranges of large files";
        props["path"] = pathProp;
        props["start_line"] = startProp;
        props["end_line"] = endProp;
        tools.append(makeTool("read_file",
            "Read file content. For large files (>1500 lines), the first call returns a truncated preview; then use start_line/end_line to read specific ranges.",
            props, QJsonArray{"path"}));
    }

    // create_file
    {
        QJsonObject props, pathProp, contentProp;
        pathProp["type"] = "string";
        pathProp["description"] = "File path relative to project root";
        contentProp["type"] = "string";
        contentProp["description"] = "Complete file content (do not abbreviate)";
        props["path"] = pathProp;
        props["content"] = contentProp;
        tools.append(makeTool("create_file", "Create a new file or overwrite an existing file with the given content",
                              props, QJsonArray{"path", "content"}));
    }

    // modify_file
    {
        QJsonObject props, pathProp, oldProp, newProp;
        pathProp["type"] = "string";
        pathProp["description"] = "File path relative to project root";
        oldProp["type"] = "string";
        oldProp["description"] = "Exact text to find in the file (must match exactly including whitespace)";
        newProp["type"] = "string";
        newProp["description"] = "Text to replace the old_text with";
        props["path"] = pathProp;
        props["old_text"] = oldProp;
        props["new_text"] = newProp;
        tools.append(makeTool("modify_file", "Modify a file by replacing old_text with new_text",
                              props, QJsonArray{"path", "old_text", "new_text"}));
    }

    // delete_file
    {
        QJsonObject props, pathProp;
        pathProp["type"] = "string";
        pathProp["description"] = "File path relative to project root";
        props["path"] = pathProp;
        tools.append(makeTool("delete_file", "Delete a file from the project", props, QJsonArray{"path"}));
    }

    // run_command
    {
        QJsonObject props, cmdProp, timeoutProp;
        cmdProp["type"] = "string";
        cmdProp["description"] = "Command to execute (e.g. 'qmake MyProject.pro && mingw32-make')";
        timeoutProp["type"] = "integer";
        timeoutProp["description"] = "Timeout in seconds (default: 120)";
        props["command"] = cmdProp;
        props["timeout_sec"] = timeoutProp;
        tools.append(makeTool("run_command", "Execute a shell command in the project directory",
                              props, QJsonArray{"command"}));
    }

    return tools;
}

// ==================== Agent 模式：系统提示词 ====================
QString AiController::buildAgentSystemPrompt() const
{
    return QStringLiteral(
        "You are a Qt/C++ coding agent integrated into a Qt development environment. "
        "You can directly read, create, and modify files in the user's project directory, "
        "and run build commands.\n\n"
        "## Environment\n"
        "- Qt 5.14.2 + MinGW 32-bit (Windows)\n"
        "- Project directory: %1\n\n"
        "## Available Tools\n"
        "- list_project_files: List all source files\n"
        "- list_files: List files in a directory\n"
        "- read_file: Read file content (use start_line/end_line for large files)\n"
        "- create_file: Create/overwrite a file\n"
        "- modify_file: Replace text in a file\n"
        "- delete_file: Delete a file\n"
        "- run_command: Execute a command (qmake, make, etc.)\n\n"
        "## Build Commands\n"
        "- To build: qmake <project>.pro && mingw32-make\n"
        "- Qt and MinGW binaries are already in PATH\n\n"
        "## Workflow\n"
        "1. Use list_project_files to understand the project structure\n"
        "2. Read relevant files to understand the codebase\n"
        "3. Create or modify files - give COMPLETE content, never abbreviate with '...'\n"
        "4. Run the build command to verify compilation\n"
        "5. If there are errors, fix them and rebuild\n"
        "6. Repeat until build succeeds\n"
        "7. Summarize what was done\n\n"
        "## Rules\n"
        "- Paths are relative to project directory\n"
        "- Always provide COMPLETE file content when creating files\n"
        "- When modifying, ensure old_text matches exactly\n"
        "- Explain briefly before each tool call (in Chinese)\n"
        "- Fix ALL compilation errors at once when possible\n"
    ).arg(m_projectDir);
}

// ==================== Agent 模式：执行工具调用 ====================
void AiController::executeToolCalls(const QJsonArray &toolCalls)
{
    m_pendingToolCalls.clear();
    for (const auto &tc : toolCalls) {
        m_pendingToolCalls.append(tc.toObject());
    }
    processNextPendingTool();
}

void AiController::processNextPendingTool()
{
    if (m_pendingToolCalls.isEmpty()) {
        continueAgentLoop();
        return;
    }

    QJsonObject toolCall = m_pendingToolCalls.takeFirst();
    QString toolCallId = toolCall["id"].toString();
    QJsonObject func = toolCall["function"].toObject();
    QString name = func["name"].toString();
    QString argsStr = func["arguments"].toString();

    QJsonParseError parseError;
    QJsonDocument argsDoc = QJsonDocument::fromJson(argsStr.toUtf8(), &parseError);
    QJsonObject args = (parseError.error == QJsonParseError::NoError) ?
                        argsDoc.object() : QJsonObject();

    QString desc = describeTool(name, args);

    // Permission-based approval logic
    bool needsApproval = false;
    if (m_permissionLevel == 0) {
        // Low: ask for everything except list_project_files and list_files
        needsApproval = (name != "list_project_files" && name != "list_files");
    } else if (m_permissionLevel == 1) {
        // Medium: ask only for destructive operations (delete, run command, modify)
        needsApproval = (name == "delete_file" || name == "run_command" || name == "modify_file");
    }
    // High (2): never ask, needsApproval stays false

    if (needsApproval) {
        m_pendingAction["toolCallId"] = toolCallId;
        m_pendingAction["type"] = name;       // <-- QML needs this
        m_pendingAction["name"] = name;
        m_pendingAction["args"] = argsStr;
        m_pendingAction["description"] = desc;
        emit pendingActionChanged();
        emit hasPendingActionChanged();
    } else {
        int logIdx = m_actionLog.size();
        addLogEntry(name, desc, "running", "");

        QVariantMap result = executeTool(name, args);

        QString status = result["success"].toBool() ? "done" : "error";
        QString detail;
        if (!result["success"].toBool()) {
            detail = result["error"].toString();
        } else if (name == "read_file") {
            detail = QStringLiteral("%1 lines").arg(result["lines"].toInt());
        } else if (name == "create_file") {
            detail = QStringLiteral("%1, %2 lines").arg(result["action"].toString()).arg(result["lines"].toInt());
        } else if (name == "modify_file") {
            detail = QStringLiteral("%1 replacements").arg(result["replacements"].toInt());
        } else if (name == "list_project_files" || name == "list_files") {
            detail = QStringLiteral("%1 files").arg(result["count"].toInt());
        }
        updateLogEntry(logIdx, status, detail);

        QJsonObject toolMsg;
        toolMsg["role"] = "tool";
        toolMsg["tool_call_id"] = toolCallId;
        toolMsg["content"] = formatToolResult(name, result);
        m_agentMessages.append(toolMsg);

        processNextPendingTool();
    }
}

QVariantMap AiController::executeTool(const QString &name, const QJsonObject &args)
{
    if (name == "list_project_files") {
        return m_fileAgent->listProjectFiles();
    } else if (name == "list_files") {
        return m_fileAgent->listFiles(args["path"].toString());
    } else if (name == "read_file") {
        int startLine = args["start_line"].toInt(0);
        int endLine   = args["end_line"].toInt(0);
        if (startLine > 0 && endLine > 0)
            return m_fileAgent->readFile(args["path"].toString(), startLine, endLine);
        else
            return m_fileAgent->readFile(args["path"].toString());
    } else if (name == "create_file") {
        return m_fileAgent->createFile(args["path"].toString(), args["content"].toString());
    } else if (name == "modify_file") {
        return m_fileAgent->modifyFile(args["path"].toString(),
                                        args["old_text"].toString(),
                                        args["new_text"].toString());
    } else if (name == "delete_file") {
        return m_fileAgent->deleteFile(args["path"].toString());
    } else if (name == "run_command") {
        int timeout = args["timeout_sec"].toInt(120);
        if (timeout <= 0) timeout = 120;
        return m_cmdRunner->execute(args["command"].toString(), m_projectDir, timeout);
    }

    QVariantMap err;
    err["success"] = false;
    err["error"] = "Unknown tool: " + name;
    return err;
}

void AiController::continueAgentLoop()
{
    setStatus(QStringLiteral("Agent working..."));
    sendAgentRequest();
}

void AiController::approvePendingAction()
{
    if (m_pendingAction.isEmpty()) return;

    QString toolCallId = m_pendingAction["toolCallId"].toString();
    QString name = m_pendingAction["name"].toString();
    QString argsStr = m_pendingAction["args"].toString();
    QString desc = m_pendingAction["description"].toString();

    QJsonDocument argsDoc = QJsonDocument::fromJson(argsStr.toUtf8());
    QJsonObject args = argsDoc.object();

    int logIdx = m_actionLog.size();
    addLogEntry(name, desc, "running", "");

    QVariantMap result = executeTool(name, args);

    QString status = result["success"].toBool() ? "done" : "error";
    QString detail;
    if (!result["success"].toBool()) {
        detail = result["error"].toString();
    } else if (name == "run_command") {
        detail = QStringLiteral("exit code: %1").arg(result["exitCode"].toInt());
        if (!result["output"].toString().isEmpty()) {
            detail += "\n" + result["output"].toString().left(500);
        }
    } else if (name == "delete_file") {
        detail = "deleted";
    }
    updateLogEntry(logIdx, status, detail);

    QJsonObject toolMsg;
    toolMsg["role"] = "tool";
    toolMsg["tool_call_id"] = toolCallId;
    toolMsg["content"] = formatToolResult(name, result);
    m_agentMessages.append(toolMsg);

    m_pendingAction.clear();
    emit pendingActionChanged();

    processNextPendingTool();
}

void AiController::rejectPendingAction()
{
    if (m_pendingAction.isEmpty()) return;

    QString toolCallId = m_pendingAction["toolCallId"].toString();
    QString name = m_pendingAction["name"].toString();
    QString desc = m_pendingAction["description"].toString();

    addLogEntry(name, desc, "rejected", "User rejected");

    QJsonObject toolMsg;
    toolMsg["role"] = "tool";
    toolMsg["tool_call_id"] = toolCallId;
    toolMsg["content"] = "Operation rejected by user.";
    m_agentMessages.append(toolMsg);

    m_pendingAction.clear();
    emit pendingActionChanged();

    processNextPendingTool();
}

// ==================== 工具描述和结果格式化 ====================
QString AiController::describeTool(const QString &name, const QJsonObject &args) const
{
    if (name == "list_project_files") return QStringLiteral("Scan project files");
    if (name == "list_files") return QStringLiteral("List dir: %1").arg(args["path"].toString());
    if (name == "read_file") return QStringLiteral("Read: %1").arg(args["path"].toString());
    if (name == "create_file") return QStringLiteral("Create: %1").arg(args["path"].toString());
    if (name == "modify_file") return QStringLiteral("Modify: %1").arg(args["path"].toString());
    if (name == "delete_file") return QStringLiteral("Delete: %1").arg(args["path"].toString());
    if (name == "run_command") return QStringLiteral("Run: %1").arg(args["command"].toString());
    return name;
}

QString AiController::formatToolResult(const QString &name, const QVariantMap &result) const
{
    if (!result["success"].toBool()) {
        return "Error: " + result["error"].toString();
    }

    if (name == "list_project_files" || name == "list_files") {
        return result["files"].toString();
    }
    if (name == "read_file") {
        return result["content"].toString();
    }
    if (name == "create_file") {
        return QStringLiteral("File %1 (%2 lines, %3 bytes)")
            .arg(result["action"].toString())
            .arg(result["lines"].toInt())
            .arg(result["bytes"].toInt());
    }
    if (name == "modify_file") {
        return QStringLiteral("Modified (%1 replacements)").arg(result["replacements"].toInt());
    }
    if (name == "delete_file") {
        return "File deleted";
    }
    if (name == "run_command") {
        return QStringLiteral("Exit code: %1\nOutput:\n%2")
            .arg(result["exitCode"].toInt())
            .arg(result["output"].toString());
    }
    return "OK";
}

// ==================== 操作日志 ====================
void AiController::addLogEntry(const QString &type, const QString &desc,
                                const QString &status, const QString &detail)
{
    QVariantMap entry;
    entry["type"] = type;
    entry["description"] = desc;
    entry["status"] = status;
    entry["detail"] = detail;
    m_actionLog.append(entry);
    emit actionLogChanged();
}

void AiController::updateLogEntry(int index, const QString &status, const QString &detail)
{
    if (index < 0 || index >= m_actionLog.size()) return;
    QVariantMap entry = m_actionLog[index].toMap();
    entry["status"] = status;
    entry["detail"] = detail;
    m_actionLog[index] = entry;
    emit actionLogChanged();
}

// ==================== Agent 模式 Setter ====================
void AiController::setAgentMode(bool enabled)
{
    if (m_agentMode == enabled) return;
    m_agentMode = enabled;
    emit agentModeChanged();

    if (enabled) {
        m_agentMessages = QJsonArray();
        m_agentLoopStartIndex = -1;
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = buildAgentSystemPrompt();
        m_agentMessages.append(sysMsg);
        setStatus(QStringLiteral("Agent mode enabled"));
    } else {
        setStatus(QStringLiteral("Chat mode"));
    }
}

void AiController::setProjectDir(const QString &dir)
{
    QString cleanDir = dir;
    cleanDir.replace("file:///", "").replace("/", "\\");
    if (cleanDir.endsWith("\\")) cleanDir.chop(1);
    cleanDir.replace("\\", "/");

    if (m_projectDir == cleanDir) return;
    m_projectDir = cleanDir;
    m_fileAgent->setProjectDir(cleanDir);
    emit projectDirChanged();

    if (m_agentMode && !m_agentMessages.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = buildAgentSystemPrompt();
        m_agentMessages[0] = sysMsg;
    }

    qDebug() << "Project dir set to:" << m_projectDir;
}

void AiController::setPermissionLevel(int level)
{
    level = qBound(0, level, 2);
    if (m_permissionLevel != level) {
        m_permissionLevel = level;
        emit permissionLevelChanged();
    }
}

// ==================== 聊天模式：流式接收 ====================
void AiController::onReadyRead()
{
    if (!m_currentReply || m_agentMode) return;

    QByteArray chunk = m_currentReply->readAll();
    m_streamBuffer += QString::fromUtf8(chunk);

    while (true) {
        int idx = m_streamBuffer.indexOf("\n\n");
        if (idx == -1) break;

        QString line = m_streamBuffer.left(idx).trimmed();
        m_streamBuffer.remove(0, idx + 2);

        if (line.isEmpty()) continue;

        if (line == "data: [DONE]") {
            setStatus(QStringLiteral("Ready"));
            finalizeAiMessage();
            return;
        }

        QString jsonStr = line;
        if (jsonStr.startsWith("data: ")) jsonStr = jsonStr.mid(6);
        else if (jsonStr.startsWith("data:")) jsonStr = jsonStr.mid(5);
        else continue;

        QJsonParseError parseError;
        QJsonDocument chunkDoc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) continue;

        QJsonObject root = chunkDoc.object();
        QJsonArray choices = root["choices"].toArray();
        if (choices.isEmpty()) continue;

        QJsonObject delta = choices[0].toObject()["delta"].toObject();
        QString content = delta["content"].toString();
        if (!content.isEmpty()) {
            m_response += content;
            emit responseChanged();
            emit streamChunkReceived(content);
        }
    }
}

// ==================== 统一回复处理 ====================
void AiController::onReplyFinished(QNetworkReply *reply)
{
    if (m_agentMode) {
        onAgentReplyFinished(reply);
        return;
    }

    if (m_stoppedByUser) {
        m_stoppedByUser = false;
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    if (m_loading) {
        m_loading = false;
        emit loadingChanged();
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errMsg = reply->errorString();
        int errCode = reply->error();
        QString urlStr = reply->url().toString();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "=== NETWORK ERROR ===";
        qDebug() << "  Error code:" << errCode << "Error string:" << errMsg;
        qDebug() << "  URL:" << urlStr;
        qDebug() << "  HTTP status:" << httpStatus;
        qDebug() << "  SSL supported:" << QSslSocket::supportsSsl();
        qDebug() << "  SSL version:" << QSslSocket::sslLibraryVersionString();
        qDebug() << "  API key length:" << m_apiKey.length();
        qDebug() << "=====================";

        // Write to log file for diagnosis
        {
            QString logPath = QCoreApplication::applicationDirPath() + "/network_debug.log";
            QFile logFile(logPath);
            if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                QTextStream ts(&logFile);
                ts << "=== " << QDateTime::currentDateTime().toString(Qt::ISODate) << " ===\n";
                ts << "Error code: " << errCode << "\n";
                ts << "Error string: " << errMsg << "\n";
                ts << "URL: " << urlStr << "\n";
                ts << "HTTP status: " << httpStatus << "\n";
                ts << "SSL supported: " << QSslSocket::supportsSsl() << "\n";
                ts << "SSL version: " << QSslSocket::sslLibraryVersionString() << "\n";
                ts << "API key length: " << m_apiKey.length() << "\n";
                ts << "Proxy type: " << m_manager->proxy().type() << "\n";
                ts << "Model: " << m_model << "\n";
                ts << "\n";
                logFile.close();
            }
        }

        setStatus(QStringLiteral("Request failed"));

        QString userMsg = QStringLiteral("Network error [") + QString::number(errCode) + QStringLiteral("]: ") + errMsg + "\n\n";

        if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
            userMsg += QStringLiteral("SSL failed. Need OpenSSL DLLs.");
        } else if (reply->error() == QNetworkReply::HostNotFoundError) {
            userMsg += QStringLiteral("DNS failed. Check network.");
        } else if (reply->error() == QNetworkReply::TimeoutError ||
                   reply->error() == QNetworkReply::OperationCanceledError) {
            userMsg += QStringLiteral("Timeout or cancelled.");
        } else if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
            userMsg += QStringLiteral("Auth failed. Check API Key.");
        }

        QVariantMap errMap;
        errMap["role"] = "assistant";
        errMap["content"] = userMsg;
        errMap["isError"] = true;
        m_displayMessages.append(errMap);
        emit messageHistoryChanged();

        m_response.clear();
        emit responseChanged();
        emit errorOccurred(errMsg);
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        if (!m_response.isEmpty()) {
            finalizeAiMessage();
        }
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    if (root.contains("error")) {
        QString apiErr = root["error"].toObject()["message"].toString();
        QString fullErr = QStringLiteral("API Error: ") + apiErr;
        setStatus(QStringLiteral("API error"));

        QVariantMap errMap;
        errMap["role"] = "assistant";
        errMap["content"] = fullErr;
        errMap["isError"] = true;
        m_displayMessages.append(errMap);
        emit messageHistoryChanged();

        m_response.clear();
        emit responseChanged();
        emit errorOccurred(apiErr);
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    QJsonArray choices = root["choices"].toArray();
    if (!choices.isEmpty()) {
        QJsonObject msg = choices[0].toObject()["message"].toObject();
        m_response = msg["content"].toString();
        setStatus(QStringLiteral("Ready"));
        finalizeAiMessage();
    }

    emit responseChanged();
    reply->deleteLater();
    m_currentReply = nullptr;
}

// ==================== 消息历史管理 ====================
void AiController::addUserMessage(const QString &content)
{
    QVariantMap msg;
    msg["role"] = "user";
    msg["content"] = content;
    msg["isError"] = false;
    m_displayMessages.append(msg);
    emit messageHistoryChanged();
}

void AiController::finalizeAiMessage()
{
    if (m_response.isEmpty()) return;

    QVariantMap msg;
    msg["role"] = "assistant";
    msg["content"] = m_response;
    msg["isError"] = false;
    m_displayMessages.append(msg);
    emit messageHistoryChanged();

    for (int i = m_displayMessages.size() - 2; i >= 0; --i) {
        if (m_displayMessages[i].toMap()["role"] == "user") {
            QJsonObject userMsg;
            userMsg["role"] = "user";
            userMsg["content"] = m_displayMessages[i].toMap()["content"].toString();
            m_history.append(userMsg);
            break;
        }
    }

    QJsonObject assistantMsg;
    assistantMsg["role"] = "assistant";
    assistantMsg["content"] = m_response;
    m_history.append(assistantMsg);

    m_response.clear();
    emit responseChanged();

    // Auto-save chat after AI response
    saveCurrentChat();
}

// ==================== 辅助函数 ====================
void AiController::clearHistory()
{
    m_history = QJsonArray();
    m_displayMessages.clear();
    m_response.clear();
    m_streamBuffer.clear();
    m_actionLog.clear();
    m_pendingAction.clear();
    m_pendingToolCalls.clear();
    m_agentLoopStartIndex = -1;

    if (m_agentMode) {
        m_agentMessages = QJsonArray();
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = buildAgentSystemPrompt();
        m_agentMessages.append(sysMsg);
    }

    // Reset chat id so next save creates a new session
    m_currentChatId.clear();
    m_currentChatTitle.clear();

    emit messageHistoryChanged();
    emit responseChanged();
    emit actionLogChanged();
    emit pendingActionChanged();
    emit hasPendingActionChanged();
    emit currentChatIdChanged();
    emit currentChatTitleChanged();
    setStatus(QStringLiteral("Cleared"));
}

void AiController::setApiKey(const QString &key)
{
    m_apiKey = key.trimmed();
    emit apiKeyChanged();
    setStatus(m_apiKey.isEmpty() ? QStringLiteral("API Key not set") : QStringLiteral("API Key ready"));
}

void AiController::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        emit modelChanged();
    }
}

void AiController::setApiUrl(const QString &url)
{
    if (m_apiUrl != url) {
        m_apiUrl = url;
        emit apiUrlChanged();
    }
}

void AiController::setSystemPrompt(const QString &prompt)
{
    m_systemPrompt = prompt;
}

void AiController::setStatus(const QString &status)
{
    if (m_statusText != status) {
        m_statusText = status;
        emit statusTextChanged();
    }
}

// ==================== 配置持久化 ====================
void AiController::loadConfig()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings config(configPath, QSettings::IniFormat);
    config.setIniCodec(QTextCodec::codecForName("UTF-8"));

    QString key = config.value("api_key", "").toString().trimmed();
    if (!key.isEmpty()) {
        setApiKey(key);
    }
    setModel(config.value("model", "deepseek-chat").toString().trimmed());
    setAgentMode(config.value("agent_mode", false).toBool());
    QString dir = config.value("project_dir", "").toString().trimmed();
    if (!dir.isEmpty()) {
        setProjectDir(dir);
    }
    setPermissionLevel(config.value("permission_level", 0).toInt());

    // Load role for PromptManager
    int role = config.value("role", 0).toInt();
    emit configLoaded(key, m_model, role);

    qDebug() << "Config loaded: model=" << m_model << "agent=" << m_agentMode
             << "project=" << m_projectDir << "perm=" << m_permissionLevel;
}

void AiController::saveConfig(const QString &key, const QString &model, int role)
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings config(configPath, QSettings::IniFormat);
    config.setIniCodec(QTextCodec::codecForName("UTF-8"));
    config.setValue("api_key", key);
    config.setValue("model", model);
    config.setValue("role", role);
    config.setValue("project_dir", m_projectDir);
    config.setValue("agent_mode", m_agentMode);
    config.setValue("permission_level", m_permissionLevel);
    config.sync();

    setApiKey(key);
    setModel(model);

    qDebug() << "Config saved: model=" << model << "role=" << role
             << "agentMode=" << m_agentMode << "projectDir=" << m_projectDir;
}

QString AiController::getConfigValue(const QString &key) const
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings config(configPath, QSettings::IniFormat);
    config.setIniCodec(QTextCodec::codecForName("UTF-8"));
    return config.value(key, "").toString();
}

void AiController::copyToClipboard(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

// ==================== 聊天模式请求构建 ====================
QJsonObject AiController::buildRequestBody(const QString &userMessage) const
{
    QJsonObject body;
    body["model"] = m_model;
    body["temperature"] = m_temperature;
    body["max_tokens"] = m_maxTokens;
    body["stream"] = true;

    QJsonArray messages;

    if (!m_systemPrompt.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = m_systemPrompt;
        messages.append(sysMsg);
    }

    for (const auto &msg : m_history) {
        messages.append(msg.toObject());
    }

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);

    body["messages"] = messages;
    return body;
}

// ==================== 快捷提示词 ====================
QVariantList AiController::quickPrompts() const
{
    QVariantList list;
    QVariantMap m1; m1["title"] = QStringLiteral("Qt code");    m1["prompt"] = QStringLiteral("Use Qt5/C++ to implement: ");
    QVariantMap m2; m2["title"] = QStringLiteral("QML comp");   m2["prompt"] = QStringLiteral("Write a QML component: ");
    QVariantMap m3; m3["title"] = QStringLiteral("Code review"); m3["prompt"] = QStringLiteral("Review this code: \n");
    QVariantMap m4; m4["title"] = QStringLiteral("Optimize");   m4["prompt"] = QStringLiteral("Optimize this Qt code: \n");
    QVariantMap m5; m5["title"] = QStringLiteral("Explain");    m5["prompt"] = QStringLiteral("Explain: ");
    QVariantMap m6; m6["title"] = QStringLiteral("Steps");      m6["prompt"] = QStringLiteral("Step by step guide: ");
    list << m1 << m2 << m3 << m4 << m5 << m6;
    return list;
}

// ==================== 聊天记录管理 ====================
QVariantList AiController::chatList() const
{
    return m_chatManager->listChats();
}

void AiController::saveCurrentChat()
{
    if (m_displayMessages.isEmpty()) return;

    QString id = m_chatManager->saveChat(
        m_currentChatId, m_currentChatTitle,
        m_projectDir, m_agentMode,
        m_displayMessages, m_history, m_agentMessages, m_actionLog);

    if (!id.isEmpty()) {
        m_currentChatId = id;
        // Derive title from first user message
        if (m_currentChatTitle.isEmpty()) {
            for (const auto &msg : m_displayMessages) {
                QVariantMap m = msg.toMap();
                if (m["role"].toString() == "user") {
                    m_currentChatTitle = m["content"].toString().left(30);
                    if (m["content"].toString().length() > 30)
                        m_currentChatTitle += "...";
                    break;
                }
            }
        }
        emit currentChatIdChanged();
        emit currentChatTitleChanged();
        emit chatSaved(id, m_currentChatTitle);
    }
}

void AiController::loadChat(const QString &id)
{
    QVariantMap data = m_chatManager->loadChat(id);
    if (data.isEmpty()) {
        qDebug() << "loadChat: chat not found" << id;
        return;
    }

    // First auto-save the current chat if it has messages
    if (!m_displayMessages.isEmpty() && m_currentChatId != id) {
        saveCurrentChat();
    }

    // Store the loaded data pending project dir check
    m_pendingLoadData = data;

    QString projectDir = data["projectDir"].toString();
    bool dirExists = ChatManager::projectDirExists(projectDir);

    if (dirExists) {
        // Project dir exists - restore everything directly
        m_currentChatId = data["id"].toString();
        m_currentChatTitle = data["title"].toString();
        m_projectDir = projectDir;
        m_fileAgent->setProjectDir(projectDir);
        m_agentMode = data["agentMode"].toBool();

        m_displayMessages = data["displayMessages"].toList();
        m_history = QJsonArray::fromVariantList(data["history"].toList());
        m_agentMessages = QJsonArray::fromVariantList(data["agentMessages"].toList());
        m_actionLog = data["actionLog"].toList();

        m_pendingAction.clear();
        m_pendingToolCalls.clear();
        m_agentLoopStartIndex = -1;
        m_pendingLoadData.clear();

        emit currentChatIdChanged();
        emit currentChatTitleChanged();
        emit projectDirChanged();
        emit agentModeChanged();
        emit messageHistoryChanged();
        emit actionLogChanged();
        emit pendingActionChanged();
        emit hasPendingActionChanged();

        emit chatLoaded(m_currentChatTitle, m_projectDir, m_displayMessages.size());
        setStatus(QStringLiteral("Loaded: %1").arg(m_currentChatTitle));
    } else {
        // Project dir does not exist - ask user to select a new path
        // Messages are already in m_pendingLoadData, will be applied after path selection
        m_currentChatId = data["id"].toString();
        m_currentChatTitle = data["title"].toString();
        emit currentChatIdChanged();
        emit currentChatTitleChanged();
        emit projectDirNotFound(projectDir);
        setStatus(QStringLiteral("Path not found: %1").arg(projectDir));
    }
}

void AiController::setProjectDirForLoadedChat(const QString &dir)
{
    // Set the new project directory
    setProjectDir(dir);

    if (m_pendingLoadData.isEmpty()) return;

    // Update the project dir in pending data
    m_pendingLoadData["projectDir"] = dir;

    // Now ask about keeping history
    int msgCount = m_pendingLoadData["displayMessages"].toList().size();
    QString title = m_pendingLoadData["title"].toString();
    emit askKeepHistory(msgCount, title);
}

void AiController::keepLoadedHistory(bool keep)
{
    if (m_pendingLoadData.isEmpty()) return;

    if (keep) {
        // Restore all messages from the loaded chat
        m_displayMessages = m_pendingLoadData["displayMessages"].toList();
        m_history = QJsonArray::fromVariantList(m_pendingLoadData["history"].toList());
        m_agentMessages = QJsonArray::fromVariantList(m_pendingLoadData["agentMessages"].toList());
        m_actionLog = m_pendingLoadData["actionLog"].toList();
        m_agentMode = m_pendingLoadData["agentMode"].toBool();
    } else {
        // Clear everything, start fresh with the new project dir
        m_displayMessages.clear();
        m_history = QJsonArray();
        m_actionLog.clear();

        if (m_agentMode) {
            m_agentMessages = QJsonArray();
            QJsonObject sysMsg;
            sysMsg["role"] = "system";
            sysMsg["content"] = buildAgentSystemPrompt();
            m_agentMessages.append(sysMsg);
        }
    }

    m_pendingAction.clear();
    m_pendingToolCalls.clear();
    m_agentLoopStartIndex = -1;
    m_pendingLoadData.clear();

    emit messageHistoryChanged();
    emit actionLogChanged();
    emit agentModeChanged();
    emit pendingActionChanged();
    emit hasPendingActionChanged();

    if (keep) {
        emit chatLoaded(m_currentChatTitle, m_projectDir, m_displayMessages.size());
        setStatus(QStringLiteral("Loaded: %1 (history kept)").arg(m_currentChatTitle));
    } else {
        m_currentChatId.clear();
        m_currentChatTitle.clear();
        emit currentChatIdChanged();
        emit currentChatTitleChanged();
        setStatus(QStringLiteral("New chat with new project dir"));
    }
}

void AiController::deleteChat(const QString &id)
{
    m_chatManager->deleteChat(id);
}

void AiController::newChat()
{
    // Auto-save current chat if it has messages
    if (!m_displayMessages.isEmpty()) {
        saveCurrentChat();
    }

    m_currentChatId.clear();
    m_currentChatTitle.clear();
    emit currentChatIdChanged();
    emit currentChatTitleChanged();

    clearHistory();
}

bool AiController::checkProjectDir(const QString &dir) const
{
    return ChatManager::projectDirExists(dir);
}

// ==================== @文件提及 ====================
QVariantList AiController::searchProjectFiles(const QString &prefix) const
{
    QVariantList result;
    if (m_projectDir.isEmpty()) return result;

    QVariantMap listResult = m_fileAgent->listProjectFiles();
    if (!listResult["success"].toBool()) return result;

    QString filesStr = listResult["files"].toString();
    QStringList files = filesStr.split("\n", QString::SkipEmptyParts);

    QString lowerPrefix = prefix.toLower();
    for (const auto &f : files) {
        // Skip directories (ending with /)
        if (f.endsWith("/")) continue;
        if (lowerPrefix.isEmpty() || f.toLower().contains(lowerPrefix)) {
            QVariantMap item;
            item["path"] = f;
            // Extract just the filename for display
            QString name = f;
            int lastSlash = name.lastIndexOf("/");
            if (lastSlash >= 0) name = name.mid(lastSlash + 1);
            item["name"] = name;
            item["dir"] = (lastSlash >= 0) ? f.left(lastSlash) : ".";
            result.append(item);
            if (result.size() >= 20) break;  // limit to 20 results
        }
    }
    return result;
}

QString AiController::readFileForMention(const QString &relativePath) const
{
    QVariantMap result = m_fileAgent->readFile(relativePath);
    if (!result["success"].toBool()) {
        return QStringLiteral("[Error reading file: %1]").arg(result["error"].toString());
    }
    return result["content"].toString();
}

void AiController::sendMessageWithFiles(const QString &message, const QStringList &filePaths)
{
    if (message.trimmed().isEmpty()) return;

    // Build the enriched message with file contents
    QString enrichedMessage = message;

    if (!filePaths.isEmpty()) {
        QString fileContext;
        for (const auto &relPath : filePaths) {
            QString content = readFileForMention(relPath);
            fileContext += QStringLiteral("\n\n[Referenced file: %1]\n%2\n").arg(relPath).arg(content);
        }
        enrichedMessage = fileContext + QStringLiteral("\n\n---\nUser message: ") + message;
    }

    sendMessage(enrichedMessage);
}

// ==================== 自动保存钩子 ====================
// Called from sendMessage and finalizeAiMessage to auto-save

