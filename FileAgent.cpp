#include "FileAgent.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

FileAgent::FileAgent(QObject *parent)
    : QObject(parent)
{
}

QString FileAgent::resolvePath(const QString &relativePath) const
{
    if (m_projectDir.isEmpty()) return QString();
    QString cleanPath = relativePath;
    cleanPath.replace("\\", "/");
    cleanPath = cleanPath.replace(m_projectDir + "/", "");
    if (cleanPath.startsWith("./")) cleanPath = cleanPath.mid(2);
    if (cleanPath.startsWith("/")) cleanPath = cleanPath.mid(1);
    return QDir::cleanPath(m_projectDir + "/" + cleanPath);
}

bool FileAgent::isPathSafe(const QString &absolutePath) const
{
    QString cleanAbs = QDir::cleanPath(absolutePath);
    QString cleanProj = QDir::cleanPath(m_projectDir);
    return cleanAbs.startsWith(cleanProj, Qt::CaseInsensitive);
}

bool FileAgent::isSourceFile(const QString &fileName) const
{
    static const QStringList exts = {
        ".pro", ".pri", ".qrc", ".ui",
        ".h", ".hpp", ".cpp", ".c", ".cc",
        ".qml", ".js",
        ".cmake", ".txt",
        ".ini", ".json", ".xml"
    };
    for (const auto &ext : exts) {
        if (fileName.endsWith(ext, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QStringList FileAgent::scanDir(const QString &dir, int maxDepth) const
{
    QStringList results;
    QDir d(dir);
    if (!d.exists()) return results;

    QFileInfoList entries = d.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst);

    for (const auto &info : entries) {
        QString relPath = QDir(m_projectDir).relativeFilePath(info.absoluteFilePath());
        if (info.isDir()) {
            if (maxDepth > 0 && info.fileName() != "debug" &&
                info.fileName() != "release" &&
                info.fileName() != ".git" &&
                info.fileName() != "build") {
                results << relPath + "/";
                results << scanDir(info.absoluteFilePath(), maxDepth - 1);
            }
        } else {
            if (isSourceFile(info.fileName())) {
                results << relPath;
            }
        }
    }
    return results;
}

QVariantMap FileAgent::listFiles(const QString &path)
{
    QVariantMap result;
    QString targetDir = resolvePath(path.isEmpty() ? "." : path);

    if (!isPathSafe(targetDir)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Path outside project directory");
        return result;
    }

    QDir dir(targetDir);
    if (!dir.exists()) {
        result["success"] = false;
        result["error"] = QStringLiteral("Directory does not exist: ") + path;
        return result;
    }

    QStringList entries;
    QFileInfoList infos = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst);

    for (const auto &info : infos) {
        QString name = info.fileName();
        if (info.isDir()) name += "/";
        entries << name;
    }

    result["success"] = true;
    result["files"] = entries.join("\n");
    result["count"] = entries.size();
    return result;
}

QVariantMap FileAgent::listProjectFiles()
{
    QVariantMap result;

    if (m_projectDir.isEmpty()) {
        result["success"] = false;
        result["error"] = QStringLiteral("Project directory not set");
        return result;
    }

    QDir dir(m_projectDir);
    if (!dir.exists()) {
        result["success"] = false;
        result["error"] = QStringLiteral("Project directory does not exist: ") + m_projectDir;
        return result;
    }

    QStringList files = scanDir(m_projectDir, 3);

    result["success"] = true;
    result["files"] = files.join("\n");
    result["count"] = files.size();
    return result;
}

QVariantMap FileAgent::readFile(const QString &path)
{
    QVariantMap result;
    QString absPath = resolvePath(path);

    if (!isPathSafe(absPath)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Path outside project directory");
        return result;
    }

    QFile file(absPath);
    if (!file.exists()) {
        result["success"] = false;
        result["error"] = QStringLiteral("File not found: ") + path;
        return result;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Cannot open file: ") + path;
        return result;
    }

    QByteArray rawData = file.readAll();
    file.close();

    const qint64 maxBytes = 50 * 1024;      // 50 KB
    const int maxLines = 1500;              // ~1500 lines

    QString content = QString::fromUtf8(rawData);
    int totalLines = content.count("\n") + 1;
    int displayedLines = totalLines;
    bool truncated = false;

    if (rawData.size() > maxBytes || totalLines > maxLines) {
        // Truncate to the first maxLines lines, but not more than maxBytes.
        QStringList lines = content.split("\n");
        int keepLines = qMin(maxLines, lines.size());
        int totalBytes = 0;
        for (int i = 0; i < keepLines; ++i) {
            if (totalBytes + lines[i].toUtf8().size() + 1 > maxBytes) {
                keepLines = i;
                break;
            }
            totalBytes += lines[i].toUtf8().size() + 1;
        }
        content = lines.mid(0, keepLines).join("\n");
        content += QStringLiteral("\n\n[File truncated: original %1 lines (%2 KB), showing first %3 lines (%4 KB). "
                                   "Use read_file with start_line and end_line to read other ranges.]")
                       .arg(totalLines)
                       .arg(rawData.size() / 1024.0, 0, 'f', 1)
                       .arg(keepLines)
                       .arg(totalBytes / 1024.0, 0, 'f', 1);
        truncated = true;
        displayedLines = keepLines;
    }

    result["success"] = true;
    result["content"] = content;
    result["lines"] = displayedLines;
    result["totalLines"] = totalLines;
    result["truncated"] = truncated;
    result["bytes"] = rawData.size();
    return result;
}

QVariantMap FileAgent::readFile(const QString &path, int startLine, int endLine)
{
    QVariantMap result;
    QString absPath = resolvePath(path);

    if (!isPathSafe(absPath)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Path outside project directory");
        return result;
    }

    QFile file(absPath);
    if (!file.exists()) {
        result["success"] = false;
        result["error"] = QStringLiteral("File not found: ") + path;
        return result;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Cannot open file: ") + path;
        return result;
    }

    QByteArray rawData = file.readAll();
    file.close();

    QStringList allLines = QString::fromUtf8(rawData).split("\n");
    int totalLines = allLines.size();

    // 1-based line numbers
    int start = qMax(1, startLine) - 1;      // convert to 0-based
    int end   = qMin(totalLines, endLine);
    if (end <= start) {
        result["success"] = false;
        result["error"] = QStringLiteral("Invalid line range: start=%1 end=%2 (file has %3 lines)")
                              .arg(startLine).arg(endLine).arg(totalLines);
        return result;
    }

    QStringList selected = allLines.mid(start, end - start);
    QString content = selected.join("\n");

    const qint64 maxBytes = 50 * 1024;
    int totalChars = 0;
    int keepLines = 0;
    for (const auto &line : selected) {
        int sz = line.toUtf8().size() + 1;
        if (totalChars + sz > maxBytes) break;
        totalChars += sz;
        keepLines++;
    }
    if (keepLines < selected.size()) {
        content = selected.mid(0, keepLines).join("\n");
        content += QStringLiteral("\n\n[Chunk truncated: requested lines %1-%2, showing lines %1-%3 (%4 KB) of %5 lines total]")
                       .arg(startLine).arg(endLine).arg(startLine + keepLines - 1)
                       .arg(totalChars / 1024.0, 0, 'f', 1).arg(totalLines);
    }

    result["success"] = true;
    result["content"] = content;
    result["lines"] = keepLines < selected.size() ? keepLines : selected.size();
    result["startLine"] = startLine;
    result["endLine"] = endLine;
    result["totalLines"] = totalLines;
    return result;
}

QVariantMap FileAgent::createFile(const QString &path, const QString &content)
{
    QVariantMap result;
    QString absPath = resolvePath(path);

    if (!isPathSafe(absPath)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Path outside project directory");
        return result;
    }

    QFileInfo info(absPath);
    QDir().mkpath(info.absolutePath());

    bool existed = QFile::exists(absPath);

    QFile file(absPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Cannot create file: ") + path;
        return result;
    }

    QByteArray data = content.toUtf8();
    qint64 written = file.write(data);
    file.close();

    int lineCount = content.count("\n") + 1;

    result["success"] = (written == data.size());
    result["action"] = existed ? QStringLiteral("overwritten") : QStringLiteral("created");
    result["path"] = path;
    result["bytes"] = (int)written;
    result["lines"] = lineCount;

    qDebug() << "FileAgent:" << (existed ? "Overwrote" : "Created") << path
             << "(" << lineCount << "lines," << written << "bytes)";

    return result;
}

QVariantMap FileAgent::modifyFile(const QString &path, const QString &oldText, const QString &newText)
{
    QVariantMap result;
    QString absPath = resolvePath(path);

    if (!isPathSafe(absPath)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Path outside project directory");
        return result;
    }

    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Cannot read file: ") + path;
        return result;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if (!content.contains(oldText)) {
        result["success"] = false;
        result["error"] = QStringLiteral("old_text not found in file: ") + path;
        result["hint"] = QStringLiteral("Make sure old_text matches exactly (including whitespace)");
        return result;
    }

    int replacements = content.count(oldText);
    content.replace(oldText, newText);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Cannot write file: ") + path;
        return result;
    }

    file.write(content.toUtf8());
    file.close();

    result["success"] = true;
    result["path"] = path;
    result["replacements"] = replacements;
    result["action"] = QStringLiteral("modified");

    qDebug() << "FileAgent: Modified" << path
             << "(" << replacements << "replacements)";

    return result;
}

QVariantMap FileAgent::deleteFile(const QString &path)
{
    QVariantMap result;
    QString absPath = resolvePath(path);

    if (!isPathSafe(absPath)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Path outside project directory");
        return result;
    }

    QFile file(absPath);
    if (!file.exists()) {
        result["success"] = false;
        result["error"] = QStringLiteral("File not found: ") + path;
        return result;
    }

    if (file.remove()) {
        result["success"] = true;
        result["path"] = path;
        result["action"] = QStringLiteral("deleted");
    } else {
        result["success"] = false;
        result["error"] = QStringLiteral("Cannot delete file: ") + path;
    }

    return result;
}
