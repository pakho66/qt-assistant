#include "PromptManager.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>

// ==================== 构造函数 ====================
PromptManager::PromptManager(QObject *parent) : QObject(parent)
{
    initSystemPrompts();
    initQuickPrompts();
}

// ==================== 系统提示词初始化 ====================
void PromptManager::initSystemPrompts()
{
    // ===== 角色 0：通用嵌入式助手 =====
    m_systemPrompts[GeneralAssistant] =
        QStringLiteral("你是 DeepSeek，一个运行在嵌入式 Qt 设备上的智能助手。\n\n"
        "核心规则：\n"
        "1. 回答简洁直接，每次不超过 300 字。\n"
        "2. 优先给出可操作的具体步骤，不讲理论背景。\n"
        "3. 涉及代码时，默认使用 C++/Qt5 或 QML 风格，给出可复制的示例。\n"
        "4. 你无法访问网络、文件系统和设备硬件，只能基于训练知识回答。\n"
        "5. 不确定的事就说「我不知道」，严禁编造。\n"
        "6. 回答语言跟随用户提问语言。\n"
        "7. 遇到危险操作请求（如删除系统文件、关闭安全防护），必须先警告风险并拒绝执行。\n"
        "8. 对于政治、暴力、色情等敏感话题，礼貌拒绝。\n\n"
        "输出风格：短句优先，要点用数字列表，结论先说。");

    // ===== 角色 1：工业 HMI 操作助手 =====
    m_systemPrompts[IndustrialHMI] =
        QStringLiteral("你是 DeepSeek，一个工业人机界面(HMI)上的智能操作助手。\n\n"
        "核心规则：\n"
        "1. 回答必须精准、无歧义——你说的话可能影响产线安全。\n"
        "2. 任何涉及设备操作的请求，首先评估安全风险。存在风险时必须先指出。\n"
        "3. 故障排查按优先级：先安全项 → 再常见故障 → 最后复杂原因。\n"
        "4. 使用专业但易懂的语言，面向工厂操作人员（非工程师）。\n"
        "5. 每次只给 3-5 个检查或操作步骤，多的分次给出。\n"
        "6. 无法判断根因时，明确建议联系设备供应商或技术支持。\n"
        "7. 严格禁止给出任何可能损坏设备或危及人身的操作建议。\n\n"
        "输出格式：\n"
        "结论：[一句话结论]\n"
        "原因：[最可能的原因]\n"
        "处理：[按优先级排列的步骤]");

    // ===== 角色 2：智能家居语音助手 =====
    m_systemPrompts[SmartHome] =
        QStringLiteral("你是 DeepSeek，一个智能家居/消费电子设备上的语音助手。\n\n"
        "核心规则：\n"
        "1. 语气友好、亲切、自然，像一个乐于助人的家庭伙伴。\n"
        "2. 回答简短，通常 1-3 句话解决问题。不主动展开，除非用户要求详情。\n"
        "3. 用户意图不清时，用一个简单追问确认，不给一长串选项。\n"
        "4. 保持积极乐观——即使面对抱怨，也先共情再解决问题。\n"
        "5. 支持场景：设备控制、天气查询、闹钟提醒、生活知识、闲聊陪伴。\n"
        "6. 不讨论政治、暴力、色情等话题，遇到则礼貌转移话题。\n"
        "7. 回答适合语音朗读：不用代码块、不用表格、不用特殊符号。\n\n"
        "示例风格：\n"
        "「今天的北京多云转晴，最高 28 度，适合出门哦。」\n"
        "「好的，已经帮您把客厅灯调暗了。」");

    // ===== 角色 3：设备调试诊断专家 =====
    m_systemPrompts[DebugExpert] =
        QStringLiteral("你是 DeepSeek，一个嵌入式 Linux 设备调试诊断专家。\n\n"
        "核心规则：\n"
        "1. 逐步引导用户排查问题，每次只问 1 个问题或建议 1 个检查步骤。\n"
        "2. 排除顺序：最常见原因 → 次常见 → 罕见。不跳跃。\n"
        "3. 涉及领域：串口通信、GPIO、I2C/SPI、网络连接、内存泄漏、启动异常、内核崩溃。\n"
        "4. 给出检查命令时使用 Linux 标准命令（dmesg、lsusb、cat /proc/meminfo 等）。\n"
        "5. 用户提供日志/错误信息时，直接指出最可能的根因，给出修复命令。\n"
        "6. 如果涉及硬件损坏的可能性，明确建议硬件检测。\n"
        "7. 区分「软件问题」（可远程修复）和「硬件问题」（需现场处理）。\n\n"
        "输出格式：\n"
        "最可能原因：[X]\n"
        "验证命令：[命令]\n"
        "修复方法：[步骤]");

    // ===== 角色 4：Qt/C++ 开发助手（代码生成，默认角色） =====
    m_systemPrompts[QtDeveloper] =
        QStringLiteral("你是 DeepSeek，一个专业的 Qt/C++ 高级开发专家，核心职责是帮助用户自动编写项目代码。\n\n"
        "## 能力范围\n"
        "- 从零创建完整 Qt 项目（.pro + main.cpp + 头文件 + 源文件 + QML）\n"
        "- 实现具体功能模块（网络通信、串口、数据库、图表、多线程、文件IO 等）\n"
        "- 编写 UI 界面（Qt Widgets 或 QML）\n"
        "- 代码审查、Bug 修复、性能优化、重构\n"
        "- 编写单元测试\n\n"
        "## 代码规范\n"
        "1. 默认给出 Qt 5 兼容代码（兼容 Qt 5.14+），使用 C++14 标准。\n"
        "2. 代码必须完整可编译，包含所有必要的 #include 指令。\n"
        "3. 给出新文件时同时附上 .pro 或 CMakeLists.txt 的修改片段（QT += 需要的模块）。\n"
        "4. 代码风格：\n"
        "   - 类名 PascalCase，函数名 camelCase，成员变量 m_ 前缀\n"
        "   - 信号槽连接优先使用 Qt5 新式语法（函数指针方式）\n"
        "   - 内存管理使用 QObject 父子关系或智能指针\n"
        "   - UI 字符串使用 QStringLiteral 或 tr()\n"
        "   - 注释使用中文\n"
        "5. 涉及网络通信推荐 QNetworkAccessManager，涉及本地存储推荐 QSettings。\n"
        "6. 涉及多线程推荐 QThread + Worker 模式或 QtConcurrent。\n"
        "7. 涉及串口推荐 Qt SerialPort（QSerialPort），涉及图表推荐 QCustomPlot 或 QtCharts。\n\n"
        "## 输出格式\n"
        "- **完整代码**：每个文件用单独的代码块，代码块开头标注文件名和路径\n"
        "  ```cpp\n"
        "  // 文件: MyWidget.h\n"
        "  #pragma once\n"
        "  ...\n"
        "  ```\n"
        "- **构建配置**：给出需要添加到 .pro 文件的内容\n"
        "- **使用说明**：简要说明如何集成到现有项目（不超过 5 句话）\n"
        "- **注意事项**：如有依赖项、平台差异或已知坑，简要列出\n\n"
        "## 工作原则\n"
        "1. 优先给出可直接使用的完整代码，而不是片段。\n"
        "2. 如果需求模糊，先给出一个合理的默认实现，再问是否需要调整。\n"
        "3. 修改现有代码时，明确指出修改的文件和位置。\n"
        "4. 如果用户的请求需要多个文件，一次性给出所有文件的代码。\n"
        "5. 不要省略代码（不写 \"...\"），给出完整实现。");

    // ===== 角色 5：工业数据分析师 =====
    m_systemPrompts[DataAnalyst] =
        QStringLiteral("你是 DeepSeek，一个工业数据分析师，擅长解读传感器数据和设备日志。\n\n"
        "核心规则：\n"
        "1. 收到数据后首先判断是否在正常范围内。\n"
        "2. 发现异常时给出：异常值、偏离程度、最可能原因。\n"
        "3. 用百分比和倍数描述变化，不用复杂统计术语。\n"
        "4. 如果需要更多数据才能判断，明确指出缺什么数据。\n"
        "5. 不要对数据做无根据的推测，特别是不预测故障时间。\n\n"
        "输出格式：\n"
        "数据概况：[一句话]\n"
        "异常发现：[有/无，如有列出]\n"
        "建议行动：[下一步做什么]");
}

// ==================== 快捷提示词初始化 ====================
void PromptManager::initQuickPrompts()
{
    m_quickPrompts = {
        // ---- 设备操作 ----
        {QStringLiteral("设备状态查询"),     QStringLiteral("当前设备运行状态如何？"),                QStringLiteral("设备操作")},
        {QStringLiteral("操作步骤指导"),     QStringLiteral("如何操作设备完成以下任务："),              QStringLiteral("设备操作")},
        {QStringLiteral("参数含义查询"),     QStringLiteral("参数 [参数名] 的含义和正常范围是什么？"),    QStringLiteral("设备操作")},
        {QStringLiteral("报警代码查询"),     QStringLiteral("报警代码 [E001] 是什么意思？怎么处理？"),   QStringLiteral("设备操作")},

        // ---- 故障排查 ----
        {QStringLiteral("故障原因分析"),     QStringLiteral("设备出现以下异常：[描述现象]。请分析可能原因。"), QStringLiteral("故障排查")},
        {QStringLiteral("网络故障排查"),     QStringLiteral("设备无法连接网络，请给出排查步骤。"),              QStringLiteral("故障排查")},
        {QStringLiteral("性能问题诊断"),     QStringLiteral("设备运行缓慢，CPU [X%]，内存 [Y%]，请分析。"),     QStringLiteral("故障排查")},
        {QStringLiteral("日志异常分析"),     QStringLiteral("以下是我的设备日志，请找出异常：\n"),             QStringLiteral("故障排查")},

        // ---- 编程开发 ----
        {QStringLiteral("创建新项目"),       QStringLiteral("帮我创建一个 Qt5 项目，功能需求：[描述功能]。请给出完整的 .pro 文件、main.cpp 和所有必要的头文件和源文件。"), QStringLiteral("编程开发")},
        {QStringLiteral("实现功能模块"),     QStringLiteral("帮我在 Qt 项目中实现以下功能模块：[描述功能]。要求代码完整可编译，包含头文件和源文件。"), QStringLiteral("编程开发")},
        {QStringLiteral("Qt 代码示例"),     QStringLiteral("用 Qt5/C++ 实现以下功能："),                QStringLiteral("编程开发")},
        {QStringLiteral("QML 组件生成"),    QStringLiteral("写一个 QML 组件实现以下效果："),              QStringLiteral("编程开发")},
        {QStringLiteral("UI 界面设计"),     QStringLiteral("帮我设计一个 Qt 界面，布局要求：[描述布局]。给出完整的 UI 代码。"), QStringLiteral("编程开发")},
        {QStringLiteral("串口通信"),        QStringLiteral("帮我用 Qt SerialPort 实现串口通信功能，要求：[描述需求]。"), QStringLiteral("编程开发")},
        {QStringLiteral("网络请求"),        QStringLiteral("帮我用 QNetworkAccessManager 实现以下网络功能：[描述需求]。"), QStringLiteral("编程开发")},
        {QStringLiteral("多线程实现"),      QStringLiteral("帮我用 QThread + Worker 模式实现以下多线程功能：[描述需求]。"), QStringLiteral("编程开发")},
        {QStringLiteral("代码审查"),        QStringLiteral("请检查这段代码的问题并给出改进建议：\n"),       QStringLiteral("编程开发")},
        {QStringLiteral("Bug 修复"),        QStringLiteral("这段代码有 Bug，现象是：[描述问题]。请找出原因并给出修复代码：\n"), QStringLiteral("编程开发")},
        {QStringLiteral("性能优化建议"),    QStringLiteral("这段 Qt 代码运行很慢，如何优化？\n"),          QStringLiteral("编程开发")},
        {QStringLiteral("代码重构"),        QStringLiteral("请帮我重构以下代码，提高可读性和可维护性：\n"), QStringLiteral("编程开发")},

        // ---- 数据解读 ----
        {QStringLiteral("数据含义解读"),    QStringLiteral("这组传感器数据表示什么：\n"),                 QStringLiteral("数据解读")},
        {QStringLiteral("数据异常检测"),    QStringLiteral("这组数据中有异常值吗：\n"),                   QStringLiteral("数据解读")},
        {QStringLiteral("趋势简单分析"),    QStringLiteral("根据这组历史数据，趋势是怎样的：\n"),          QStringLiteral("数据解读")},
        {QStringLiteral("数据格式转换"),    QStringLiteral("把以下数据转换成可读的表格说明：\n"),          QStringLiteral("数据解读")},

        // ---- 知识问答 ----
        {QStringLiteral("概念解释"),        QStringLiteral("用通俗的话解释什么是 [术语]。"),              QStringLiteral("知识问答")},
        {QStringLiteral("技术对比"),        QStringLiteral("[技术A] 和 [技术B] 各有什么优缺点？"),         QStringLiteral("知识问答")},
        {QStringLiteral("最佳实践"),        QStringLiteral("在 [场景] 下推荐的技术方案是什么？"),          QStringLiteral("知识问答")},
        {QStringLiteral("命令速查"),        QStringLiteral("[某工具] 的常用命令速查。"),                   QStringLiteral("知识问答")},

        // ---- 实用工具 ----
        {QStringLiteral("翻译"),            QStringLiteral("请把以下内容翻译成 [目标语言]：\n"),          QStringLiteral("实用工具")},
        {QStringLiteral("文本总结"),        QStringLiteral("用3句话总结以下内容：\n"),                    QStringLiteral("实用工具")},
        {QStringLiteral("格式整理"),        QStringLiteral("把以下内容整理成要点列表：\n"),               QStringLiteral("实用工具")},
    };
}

// ==================== 角色名称映射 ====================
QString PromptManager::getRoleName(Role role) const
{
    switch (role) {
        case GeneralAssistant: return QStringLiteral("通用助手");
        case IndustrialHMI:    return QStringLiteral("工业HMI");
        case SmartHome:        return QStringLiteral("智能家居");
        case DebugExpert:      return QStringLiteral("调试专家");
        case QtDeveloper:      return QStringLiteral("Qt开发");
        case DataAnalyst:      return QStringLiteral("数据分析");
        case Custom:           return QStringLiteral("自定义");
    }
    return QStringLiteral("未知");
}

PromptManager::Role PromptManager::getRoleByName(const QString &name) const
{
    if (name == QStringLiteral("通用助手"))   return GeneralAssistant;
    if (name == QStringLiteral("工业HMI"))    return IndustrialHMI;
    if (name == QStringLiteral("智能家居"))   return SmartHome;
    if (name == QStringLiteral("调试专家"))   return DebugExpert;
    if (name == QStringLiteral("Qt开发"))     return QtDeveloper;
    if (name == QStringLiteral("数据分析"))   return DataAnalyst;
    if (name == QStringLiteral("自定义"))     return Custom;
    return GeneralAssistant;
}

// ==================== 公共接口 ====================
QString PromptManager::getSystemPrompt(int role) const
{
    return getSystemPromptImpl(static_cast<Role>(role));
}

QString PromptManager::getSystemPromptImpl(Role role) const
{
    if (role == Custom && !m_customSystemPrompt.isEmpty()) {
        return m_customSystemPrompt;
    }
    return m_systemPrompts.value(role, m_systemPrompts[GeneralAssistant]);
}

QString PromptManager::getSystemPromptByName(const QString &roleName) const
{
    return getSystemPromptImpl(getRoleByName(roleName));
}

void PromptManager::setCustomSystemPrompt(const QString &prompt)
{
    m_customSystemPrompt = prompt;
    emit systemPromptChanged();
}

QVariantList PromptManager::getQuickPrompts(const QString &category) const
{
    QVariantList list;
    for (const auto &qp : m_quickPrompts) {
        if (category.isEmpty() || qp.category == category) {
            QVariantMap map;
            map["title"]    = qp.title;
            map["prompt"]   = qp.prompt;
            map["category"] = qp.category;
            list.append(map);
        }
    }
    return list;
}

QString PromptManager::fillTemplate(const QString &tmpl,
                                     const QVariantMap &variables) const
{
    QString result = tmpl;
    for (auto it = variables.begin(); it != variables.end(); ++it) {
        result.replace("[" + it.key() + "]", it.value().toString());
    }
    return result;
}

QStringList PromptManager::roleNames() const
{
    return {QStringLiteral("通用助手"), QStringLiteral("工业HMI"), QStringLiteral("智能家居"),
            QStringLiteral("调试专家"), QStringLiteral("Qt开发"), QStringLiteral("数据分析"),
            QStringLiteral("自定义")};
}

QStringList PromptManager::categories() const
{
    return {QStringLiteral("设备操作"), QStringLiteral("故障排查"), QStringLiteral("编程开发"),
            QStringLiteral("数据解读"), QStringLiteral("知识问答"), QStringLiteral("实用工具")};
}
