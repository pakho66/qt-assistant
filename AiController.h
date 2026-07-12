#ifndef AICONTROLLER_H
#define AICONTROLLER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>

class FileAgent;
class CommandRunner;
class ChatManager;

class AiController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString  response    READ response    NOTIFY responseChanged)
    Q_PROPERTY(bool     loading     READ loading     NOTIFY loadingChanged)
    Q_PROPERTY(QString  statusText  READ statusText  NOTIFY statusTextChanged)
    Q_PROPERTY(QString  model       READ model       NOTIFY modelChanged)
    Q_PROPERTY(QString  apiUrl      READ apiUrl      NOTIFY apiUrlChanged)
    Q_PROPERTY(QVariantList quickPrompts READ quickPrompts NOTIFY quickPromptsChanged)
    Q_PROPERTY(QVariantList messageHistory READ messageHistory NOTIFY messageHistoryChanged)
    Q_PROPERTY(QString  apiKey      READ apiKey      NOTIFY apiKeyChanged)

    Q_PROPERTY(bool     agentMode   READ agentMode   NOTIFY agentModeChanged)
    Q_PROPERTY(QString  projectDir  READ projectDir  NOTIFY projectDirChanged)
    Q_PROPERTY(QVariantList actionLog READ actionLog NOTIFY actionLogChanged)
    Q_PROPERTY(QVariantMap pendingAction READ pendingAction NOTIFY pendingActionChanged)
    Q_PROPERTY(bool     hasPendingAction READ hasPendingAction NOTIFY hasPendingActionChanged)
    Q_PROPERTY(int      permissionLevel READ permissionLevel NOTIFY permissionLevelChanged)
    Q_PROPERTY(QString  currentChatId   READ currentChatId   NOTIFY currentChatIdChanged)
    Q_PROPERTY(QString  currentChatTitle READ currentChatTitle NOTIFY currentChatTitleChanged)

public:
    explicit AiController(QObject *parent = nullptr);
    ~AiController();

    QString response()   const { return m_response; }
    bool    loading()    const { return m_loading; }
    QString statusText() const { return m_statusText; }
    QString model()      const { return m_model; }
    QString apiUrl()     const { return m_apiUrl; }
    QString apiKey()     const { return m_apiKey; }
    QVariantList quickPrompts() const;
    QVariantList messageHistory() const { return m_displayMessages; }

    bool    agentMode()  const { return m_agentMode; }
    QString projectDir() const { return m_projectDir; }
    QVariantList actionLog() const { return m_actionLog; }
    QVariantMap pendingAction() const { return m_pendingAction; }
    bool hasPendingAction() const { return !m_pendingAction.isEmpty(); }
    int permissionLevel() const { return m_permissionLevel; }
    QString currentChatId() const { return m_currentChatId; }
    QString currentChatTitle() const { return m_currentChatTitle; }

    Q_INVOKABLE void sendMessage(const QString &message);
    Q_INVOKABLE void stopGeneration();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void setApiKey(const QString &key);
    Q_INVOKABLE void setModel(const QString &model);
    Q_INVOKABLE void setApiUrl(const QString &url);
    Q_INVOKABLE void setSystemPrompt(const QString &prompt);
    Q_INVOKABLE void loadConfig();
    Q_INVOKABLE void saveConfig(const QString &key, const QString &model, int role);
    Q_INVOKABLE QString getConfigValue(const QString &key) const;
    Q_INVOKABLE void copyToClipboard(const QString &text) const;

    Q_INVOKABLE void setAgentMode(bool enabled);
    Q_INVOKABLE void setProjectDir(const QString &dir);
    Q_INVOKABLE void approvePendingAction();
    Q_INVOKABLE void rejectPendingAction();
    Q_INVOKABLE void setPermissionLevel(int level);

    // ===== Chat history management =====
    Q_INVOKABLE QVariantList chatList() const;
    Q_INVOKABLE void loadChat(const QString &id);
    Q_INVOKABLE void deleteChat(const QString &id);
    Q_INVOKABLE void saveCurrentChat();
    Q_INVOKABLE void newChat();
    Q_INVOKABLE bool checkProjectDir(const QString &dir) const;
    Q_INVOKABLE void setProjectDirForLoadedChat(const QString &dir);
    Q_INVOKABLE void keepLoadedHistory(bool keep);

    // ===== @file mention =====
    Q_INVOKABLE QVariantList searchProjectFiles(const QString &prefix) const;
    Q_INVOKABLE void sendMessageWithFiles(const QString &message, const QStringList &filePaths);
    Q_INVOKABLE QString readFileForMention(const QString &relativePath) const;

signals:
    void responseChanged();
    void loadingChanged();
    void statusTextChanged();
    void modelChanged();
    void apiUrlChanged();
    void apiKeyChanged();
    void quickPromptsChanged();
    void messageHistoryChanged();
    void errorOccurred(const QString &error);
    void streamChunkReceived(const QString &chunk);
    void configLoaded(const QString &apiKey, const QString &model, int role);

    void agentModeChanged();
    void projectDirChanged();
    void actionLogChanged();
    void pendingActionChanged();
    void hasPendingActionChanged();
    void permissionLevelChanged();
    void currentChatIdChanged();
    void currentChatTitleChanged();

    // Chat history signals
    void projectDirNotFound(const QString &oldPath);
    void askKeepHistory(int messageCount, const QString &title);
    void chatLoaded(const QString &title, const QString &projectDir, int messageCount);
    void chatSaved(const QString &id, const QString &title);

private slots:
    void onReadyRead();
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_manager;
    QNetworkReply         *m_currentReply = nullptr;

    QString m_response;
    bool    m_loading     = false;
    QString m_statusText;
    QString m_model       = "deepseek-chat";
    QString m_apiUrl      = "https://api.deepseek.com/v1/chat/completions";
    QString m_apiKey;
    QString m_systemPrompt;

    QJsonArray m_history;
    QVariantList m_displayMessages;
    QString m_streamBuffer;

    double  m_temperature = 0.7;
    int     m_maxTokens   = 4096;
    int     m_timeoutSec  = 120;

    bool    m_stoppedByUser = false;

    FileAgent    *m_fileAgent;
    CommandRunner *m_cmdRunner;
    ChatManager  *m_chatManager;

    bool    m_agentMode  = false;
    QString m_projectDir;
    QVariantList m_actionLog;
    QVariantMap m_pendingAction;
    int     m_permissionLevel = 0;  // 0=low(all ask), 1=medium(delete/cmd ask), 2=high(no ask)

    QJsonArray m_agentMessages;
    QList<QJsonObject> m_pendingToolCalls;
    QList<QPair<QString, QString>> m_toolResults;
    int     m_agentLoopStartIndex = -1;  // index of assistant msg that triggered current tool_calls

    // Chat history state
    QString m_currentChatId;
    QString m_currentChatTitle;
    QVariantMap m_pendingLoadData;  // chat data loaded but waiting for path/history decision

    QJsonObject buildRequestBody(const QString &userMessage) const;
    void setStatus(const QString &status);
    void addUserMessage(const QString &content);
    void finalizeAiMessage();

    void addLogEntry(const QString &type, const QString &desc,
                     const QString &status, const QString &detail);
    void updateLogEntry(int index, const QString &status, const QString &detail);

    void sendAgentRequest();
    void onAgentReplyFinished(QNetworkReply *reply);
    QJsonArray buildToolDefinitions() const;
    QString buildAgentSystemPrompt() const;
    void executeToolCalls(const QJsonArray &toolCalls);
    void processNextPendingTool();
    QVariantMap executeTool(const QString &name, const QJsonObject &args);
    void continueAgentLoop();

    QString describeTool(const QString &name, const QJsonObject &args) const;
    QString formatToolResult(const QString &name, const QVariantMap &result) const;
};

#endif // AICONTROLLER_H
