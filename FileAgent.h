#ifndef FILEAGENT_H
#define FILEAGENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class FileAgent : public QObject
{
    Q_OBJECT

public:
    explicit FileAgent(QObject *parent = nullptr);

    void setProjectDir(const QString &dir) { m_projectDir = dir; }
    QString projectDir() const { return m_projectDir; }

    QVariantMap listFiles(const QString &path = QString());
    QVariantMap listProjectFiles();
    QVariantMap readFile(const QString &path);
    QVariantMap readFile(const QString &path, int startLine, int endLine);
    QVariantMap createFile(const QString &path, const QString &content);
    QVariantMap modifyFile(const QString &path, const QString &oldText, const QString &newText);
    QVariantMap deleteFile(const QString &path);

    QString resolvePath(const QString &relativePath) const;
    bool isPathSafe(const QString &absolutePath) const;

private:
    QString m_projectDir;

    QStringList scanDir(const QString &dir, int maxDepth = 3) const;
    bool isSourceFile(const QString &fileName) const;
};

#endif // FILEAGENT_H
