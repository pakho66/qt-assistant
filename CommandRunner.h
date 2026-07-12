#ifndef COMMANDRUNNER_H
#define COMMANDRUNNER_H

#include <QObject>
#include <QProcess>
#include <QVariantMap>

class CommandRunner : public QObject
{
    Q_OBJECT

public:
    explicit CommandRunner(QObject *parent = nullptr);

    QVariantMap execute(const QString &command, const QString &workingDir, int timeoutSec = 120);

    void setQtEnv(const QString &mingwBin, const QString &qtBin);

private:
    QProcessEnvironment m_env;
    bool m_envReady = false;

    QString m_mingwBin;
    QString m_qtBin;
};

#endif // COMMANDRUNNER_H
