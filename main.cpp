#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSslSocket>
#include <QDebug>
#include <QSettings>
#include <QTextCodec>
#include "AiController.h"
#include "PromptManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qDebug() << "Qt Version:" << QT_VERSION_STR;
    qDebug() << "SSL supported:" << QSslSocket::supportsSsl();
    qDebug() << "SSL library version:" << QSslSocket::sslLibraryVersionString();

    AiController ai;
    PromptManager promptManager;

    // Load config
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings config(configPath, QSettings::IniFormat);
    config.setIniCodec(QTextCodec::codecForName("UTF-8"));
    QString savedKey = config.value("api_key", "").toString();
    QString savedModel = config.value("model", "deepseek-chat").toString();
    int savedRole = config.value("role", (int)PromptManager::QtDeveloper).toInt();

    if (!savedKey.isEmpty()) {
        ai.setApiKey(savedKey);
        qDebug() << "API Key loaded from config.ini";
    }
    ai.setModel(savedModel);
    ai.setApiUrl("https://api.deepseek.com/v1/chat/completions");

    // Agent config
    QString savedProjectDir = config.value("project_dir", "").toString();
    bool savedAgentMode = config.value("agent_mode", false).toBool();
    int savedPermLevel = config.value("permission_level", 0).toInt();

    if (!savedProjectDir.isEmpty()) {
        ai.setProjectDir(savedProjectDir);
    }
    if (savedAgentMode) {
        ai.setAgentMode(true);
    }
    ai.setPermissionLevel(savedPermLevel);

    ai.setSystemPrompt(promptManager.getSystemPrompt(savedRole));

    // Expose to QML
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("ai", &ai);
    engine.rootContext()->setContextProperty("promptManager", &promptManager);

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qDebug() << "ERROR: Failed to load QML!";
        return -1;
    }

    qDebug() << "AiChat started successfully!";
    return app.exec();
}
