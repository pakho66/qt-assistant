#ifndef PROMPTMANAGER_H
#define PROMPTMANAGER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QList>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>

class PromptManager : public QObject
{
    Q_OBJECT

public:
    // ===== 角色枚举 =====
    enum Role {
        GeneralAssistant,   // 通用嵌入式助手
        IndustrialHMI,      // 工业 HMI 操作助手
        SmartHome,          // 智能家居语音助手
        DebugExpert,        // 设备调试诊断专家
        QtDeveloper,        // Qt/C++ 开发助手（代码生成）
        DataAnalyst,        // 工业数据分析师
        Custom              // 自定义
    };
    Q_ENUM(Role)

    // ===== 快捷提示词结构 =====
    struct QuickPrompt {
        QString title;       // 按钮标题
        QString prompt;      // 填充到输入框的内容
        QString category;    // 分类标签
    };

    explicit PromptManager(QObject *parent = nullptr);

    // ===== 系统提示词 =====
    Q_INVOKABLE QString getSystemPrompt(int role) const;
    Q_INVOKABLE QString getSystemPromptByName(const QString &roleName) const;
    Q_INVOKABLE void setCustomSystemPrompt(const QString &prompt);

    // ===== 快捷提示词 =====
    Q_INVOKABLE QVariantList getQuickPrompts(
        const QString &category = "") const;

    // ===== 变量填充 =====
    Q_INVOKABLE QString fillTemplate(
        const QString &tmpl,
        const QVariantMap &variables) const;

    // ===== 角色名称列表（给 QML ComboBox 用） =====
    Q_INVOKABLE QStringList roleNames() const;

    // ===== 分类列表 =====
    Q_INVOKABLE QStringList categories() const;

signals:
    void systemPromptChanged();
    void quickPromptsChanged();

private:
    QString getSystemPromptImpl(Role role) const;
    void initSystemPrompts();
    void initQuickPrompts();
    QString getRoleName(Role role) const;
    Role getRoleByName(const QString &name) const;

    QHash<Role, QString> m_systemPrompts;
    QList<QuickPrompt>   m_quickPrompts;
    QString              m_customSystemPrompt;
};

#endif // PROMPTMANAGER_H
