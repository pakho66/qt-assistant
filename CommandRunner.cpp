#include "CommandRunner.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QCoreApplication>

CommandRunner::CommandRunner(QObject *parent)
    : QObject(parent)
{
    m_env = QProcessEnvironment::systemEnvironment();
}

void CommandRunner::setQtEnv(const QString &mingwBin, const QString &qtBin)
{
    m_mingwBin = mingwBin;
    m_qtBin = qtBin;

    // Ignore empty paths for portability
    QStringList paths;
    if (!mingwBin.isEmpty()) paths << mingwBin;
    if (!qtBin.isEmpty())    paths << qtBin;
    if (paths.isEmpty()) return;

    QString currentPath = m_env.value("PATH", "");
    m_env.insert("PATH", paths.join(";") + ";" + currentPath);
    m_envReady = true;

    qDebug() << "CommandRunner: Qt env set - MinGW:" << mingwBin << "Qt:" << qtBin;
}

QVariantMap CommandRunner::execute(const QString &command, const QString &workingDir, int timeoutSec)
{
    QVariantMap result;

    if (command.trimmed().isEmpty()) {
        result["success"] = false;
        result["error"] = QStringLiteral("Empty command");
        return result;
    }

    qDebug() << "CommandRunner: Executing:" << command
             << "in" << workingDir
             << "(timeout:" << timeoutSec << "s)";

    QProcess process;
    process.setProcessEnvironment(m_env);

    if (!workingDir.isEmpty()) {
        process.setWorkingDirectory(workingDir);
    }

    process.start("cmd.exe", QStringList() << "/c" << command);

    if (!process.waitForStarted(10000)) {
        result["success"] = false;
        result["error"] = QStringLiteral("Failed to start command: ") + process.errorString();
        result["exitCode"] = -1;
        return result;
    }

    QByteArray stdoutData;
    QByteArray stderrData;

    QElapsedTimer timer;
    timer.start();
    const int timeoutMs = timeoutSec * 1000;

    while (process.state() != QProcess::NotRunning) {
        // Poll every 50ms so we can read both stdout/stderr and keep UI responsive
        process.waitForReadyRead(50);

        stdoutData += process.readAllStandardOutput();
        stderrData += process.readAllStandardError();

        // Allow the UI to update (status, Stop button, log view)
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        if (timer.elapsed() > timeoutMs) {
            process.kill();
            process.waitForFinished(5000);
            result["success"] = false;
            result["error"] = QStringLiteral("Command timed out after %1 seconds").arg(timeoutSec);
            result["exitCode"] = -1;
            result["stdout"] = QString::fromUtf8(stdoutData);
            result["stderr"] = QString::fromUtf8(stderrData);
            return result;
        }
    }

    stdoutData += process.readAllStandardOutput();
    stderrData += process.readAllStandardError();

    int exitCode = process.exitCode();
    QString stdoutStr = QString::fromUtf8(stdoutData);
    QString stderrStr = QString::fromUtf8(stderrData);

    QString combined = stdoutStr;
    if (!stderrStr.isEmpty()) {
        if (!combined.isEmpty()) combined += "\n";
        combined += stderrStr;
    }

    if (combined.length() > 8000) {
        combined = combined.left(4000) + "\n\n... [output truncated] ...\n\n" + combined.right(4000);
    }

    result["success"] = (exitCode == 0);
    result["exitCode"] = exitCode;
    result["stdout"] = stdoutStr;
    result["stderr"] = stderrStr;
    result["output"] = combined;

    qDebug() << "CommandRunner: Finished, exit code:" << exitCode
             << ", output length:" << combined.length();

    return result;
}
