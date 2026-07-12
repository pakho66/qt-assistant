#include "ChatManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>

ChatManager::ChatManager(QObject *parent)
    : QObject(parent)
{
    m_chatsDir = QCoreApplication::applicationDirPath() + "/chats";
    QDir().mkpath(m_chatsDir);
    qDebug() << "ChatManager: chats dir =" << m_chatsDir;
}

QString ChatManager::generateId() const
{
    return QStringLiteral("chat_%1")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
}

QString ChatManager::chatFilePath(const QString &id) const
{
    return m_chatsDir + "/" + id + ".json";
}

QVariantList ChatManager::listChats() const
{
    QVariantList list;
    QDir dir(m_chatsDir);
    if (!dir.exists()) return list;

    QFileInfoList files = dir.entryInfoList(QStringList() << "chat_*.json", QDir::Files, QDir::Time);
    for (const auto &fi : files) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;

        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) continue;

        QJsonObject obj = doc.object();
        QVariantMap summary;
        summary["id"] = obj["id"].toString();
        summary["title"] = obj["title"].toString();
        summary["projectDir"] = obj["projectDir"].toString();
        summary["agentMode"] = obj["agentMode"].toBool();
        summary["createdAt"] = obj["createdAt"].toString();
        summary["updatedAt"] = obj["updatedAt"].toString();
        summary["messageCount"] = obj["displayMessages"].toArray().size();
        list.append(summary);
    }
    return list;
}

QString ChatManager::saveChat(const QString &id, const QString &title,
                               const QString &projectDir, bool agentMode,
                               const QVariantList &displayMessages,
                               const QJsonArray &history,
                               const QJsonArray &agentMessages,
                               const QVariantList &actionLog)
{
    QString chatId = id.isEmpty() ? generateId() : id;
    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Use the first user message as title if title is empty
    QString chatTitle = title;
    if (chatTitle.isEmpty()) {
        for (const auto &msg : displayMessages) {
            QVariantMap m = msg.toMap();
            if (m["role"].toString() == "user") {
                chatTitle = m["content"].toString().left(30);
                if (m["content"].toString().length() > 30)
                    chatTitle += "...";
                break;
            }
        }
        if (chatTitle.isEmpty()) chatTitle = QStringLiteral("New Chat");
    }

    QJsonObject obj;
    obj["id"] = chatId;
    obj["title"] = chatTitle;
    obj["projectDir"] = projectDir;
    obj["agentMode"] = agentMode;
    obj["createdAt"] = now;   // will be overwritten if loading existing
    obj["updatedAt"] = now;

    // If loading existing chat, preserve createdAt
    QString existingPath = chatFilePath(chatId);
    QFile existingFile(existingPath);
    if (existingFile.open(QIODevice::ReadOnly)) {
        QJsonDocument existingDoc = QJsonDocument::fromJson(existingFile.readAll());
        existingFile.close();
        if (existingDoc.isObject()) {
            QJsonObject existingObj = existingDoc.object();
            if (existingObj.contains("createdAt"))
                obj["createdAt"] = existingObj["createdAt"].toString();
        }
    }

    // Convert QVariantList to QJsonArray
    QJsonArray dispMsgs;
    for (const auto &msg : displayMessages) {
        dispMsgs.append(QJsonValue::fromVariant(msg));
    }
    obj["displayMessages"] = dispMsgs;
    obj["history"] = history;
    obj["agentMessages"] = agentMessages;

    QJsonArray logArray;
    for (const auto &entry : actionLog) {
        logArray.append(QJsonValue::fromVariant(entry));
    }
    obj["actionLog"] = logArray;

    QJsonDocument doc(obj);
    QFile file(existingPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "ChatManager: Cannot write file" << existingPath;
        return QString();
    }
    file.write(doc.toJson());
    file.close();

    qDebug() << "ChatManager: Saved chat" << chatId << "title:" << chatTitle
             << "messages:" << displayMessages.size();
    return chatId;
}

QVariantMap ChatManager::loadChat(const QString &id) const
{
    QVariantMap result;
    QString path = chatFilePath(id);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ChatManager: Cannot read file" << path;
        return result;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return result;

    QJsonObject obj = doc.object();
    result["id"] = obj["id"].toString();
    result["title"] = obj["title"].toString();
    result["projectDir"] = obj["projectDir"].toString();
    result["agentMode"] = obj["agentMode"].toBool();
    result["createdAt"] = obj["createdAt"].toString();
    result["updatedAt"] = obj["updatedAt"].toString();

    // Convert QJsonArray to QVariantList for QML
    QJsonArray dispMsgs = obj["displayMessages"].toArray();
    QVariantList dispList;
    for (const auto &v : dispMsgs) {
        dispList.append(v.toVariant());
    }
    result["displayMessages"] = dispList;

    result["history"] = obj["history"].toArray().toVariantList();
    result["agentMessages"] = obj["agentMessages"].toArray().toVariantList();

    QJsonArray logArray = obj["actionLog"].toArray();
    QVariantList logList;
    for (const auto &v : logArray) {
        logList.append(v.toVariant());
    }
    result["actionLog"] = logList;

    return result;
}

bool ChatManager::deleteChat(const QString &id)
{
    QString path = chatFilePath(id);
    QFile file(path);
    if (!file.exists()) return false;
    bool ok = file.remove();
    qDebug() << "ChatManager: Deleted chat" << id << "ok:" << ok;
    return ok;
}

bool ChatManager::projectDirExists(const QString &dir)
{
    if (dir.isEmpty()) return false;
    QString cleanDir = dir;
    cleanDir.replace("\\", "/");
    return QDir(cleanDir).exists();
}
