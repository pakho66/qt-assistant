#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonArray>

class ChatManager : public QObject
{
    Q_OBJECT
public:
    explicit ChatManager(QObject *parent = nullptr);

    // List all saved chat sessions (summary only: id, title, projectDir, updatedAt, messageCount, agentMode)
    QVariantList listChats() const;

    // Save a chat session to disk. Returns the chat id (generated if empty).
    QString saveChat(const QString &id, const QString &title,
                     const QString &projectDir, bool agentMode,
                     const QVariantList &displayMessages,
                     const QJsonArray &history,
                     const QJsonArray &agentMessages,
                     const QVariantList &actionLog);

    // Load a chat session from disk. Returns full data map.
    QVariantMap loadChat(const QString &id) const;

    // Delete a chat session file
    bool deleteChat(const QString &id);

    // Check if a directory exists on disk
    static bool projectDirExists(const QString &dir);

    // Get the chats directory path
    QString chatsDir() const { return m_chatsDir; }

private:
    QString m_chatsDir;
    QString generateId() const;
    QString chatFilePath(const QString &id) const;
};

#endif // CHATMANAGER_H
