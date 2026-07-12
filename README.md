# AiChat - Qt AI 编程 Agent

> 项目位置：`G:\qtproject\AiChat`
> AI 模型：DeepSeek（deepseek-v4-pro，默认）
> 构建系统：qmake（附带 CMakeLists.txt）
> Qt 版本：5.14.2 MinGW 32-bit

---

## 功能特性

### 聊天模式（默认）

| 功能 | 说明 |
|------|------|
| AI 代码生成 | 让 DeepSeek 帮你编写 Qt/C++ 项目代码 |
| 流式响应 | 打字机效果，逐字显示 AI 回复 |
| 多轮对话 | 支持上下文连贯的多轮对话 |
| 消息历史 | 完整对话历史显示，用户/AI 消息气泡区分 |
| 6 种 AI 角色 | 通用助手、工业HMI、智能家居、调试专家、Qt开发、数据分析 |
| 20+ 快捷提示词 | 按分类组织，一键填充常用提示 |
| 停止生成 | 随时中止 AI 回复 |
| 复制消息 | 一键复制 AI 回复内容 |
| API Key 持久化 | 配置保存在 config.ini，下次启动自动加载 |

### Agent 模式（新增）

| 功能 | 说明 |
|------|------|
| 自动创建文件 | AI 在项目目录中直接创建源码文件 |
| 自动修改代码 | AI 读取现有文件并修改内容 |
| 自动删除文件 | AI 删除不需要的文件（需用户确认） |
| 项目结构扫描 | AI 自动扫描项目文件结构 |
| 读取文件内容 | AI 读取项目中的源文件进行分析 |
| 执行编译命令 | AI 运行 qmake/make 等构建命令（需用户确认） |
| 自动修复错误 | 编译失败后 AI 自动分析错误并修复 |
| 操作日志 | 所有 Agent 操作实时显示在日志面板 |
| 安全审批 | 危险操作（删除/命令）需用户确认后执行 |
| 路径安全 | 所有文件操作限制在项目目录内，防止越界 |

## 快速运行

双击运行：
```
G:\qtproject\AiChat\debug\AiChat.exe
```

所有依赖（Qt DLL、QML 模块、OpenSSL、MinGW 运行时、config.ini）已部署到位。

## 使用方式

### 聊天模式

1. 程序启动后，API Key 已从 config.ini 自动加载
2. 在底部输入框输入问题，按 **Ctrl+Enter** 发送
3. AI 回复以打字机效果逐字显示
4. 点击右上角**设置**可修改 API Key、模型、AI 角色

### Agent 模式

1. 点击顶部 **Agent 模式** 开关，切换到 Agent 模式
2. 点击 **选择目录** 按钮，选择你的 Qt 项目文件夹
3. 在输入框描述你想要的功能，例如：
   - "帮我创建一个带按钮和文本框的窗口程序"
   - "扫描项目，找出编译错误并修复"
   - "在项目里添加一个网络通信模块"
4. AI 会自动：
   - 扫描项目文件结构
   - 读取相关源文件
   - 创建/修改文件
   - 运行编译命令
5. 危险操作（删除文件、执行命令）会弹出审批栏：
   - 点击 **执行** 确认运行
   - 点击 **拒绝** 跳过该操作
6. 所有操作记录显示在左侧日志面板中

## 文件结构

```
AiChat/
├── AiChat.pro          ← qmake 构建配置
├── CMakeLists.txt      ← CMake 构建配置（备选）
├── qml.qrc             ← QML 资源文件
├── main.cpp            ← 程序入口
├── AiController.h      ← AI 控制器头文件（聊天+Agent）
├── AiController.cpp    ← AI 控制器实现
├── PromptManager.h     ← 提示词管理器头文件
├── PromptManager.cpp   ← 提示词管理器实现
├── FileAgent.h         ← 文件操作 Agent 头文件
├── FileAgent.cpp       ← 文件操作 Agent 实现
├── CommandRunner.h     ← 命令执行器头文件
├── CommandRunner.cpp   ← 命令执行器实现
├── main.qml            ← QML 界面（聊天+Agent）
└── debug/
    ├── AiChat.exe      ← 可执行文件
    ├── config.ini      ← 配置文件
    ├── libssl-1_1.dll  ← OpenSSL
    ├── libcrypto-1_1.dll
    └── Qt5*.dll        ← Qt 运行时
```

## 核心类

### AiController
- 封装 DeepSeek API 调用
- **聊天模式**：SSE 流式响应，逐字显示
- **Agent 模式**：Function Calling，AI 自主调用工具
- 管理多轮对话历史、配置持久化
- Agent 循环：AI 决策 -> 工具执行 -> 结果反馈 -> AI 继续

### FileAgent
- 项目文件操作：创建、读取、修改、删除
- 路径安全验证：所有操作限制在项目目录内
- 项目结构扫描：递归扫描源文件（最大深度 3）
- 过滤无关目录：debug/release/.git/build

### CommandRunner
- 执行 shell 命令（qmake、mingw32-make 等）
- 预配置 Qt + MinGW 环境变量
- 超时处理（默认 120 秒）
- 输出截断（超长输出保留头部和尾部）

### PromptManager
- 6 种 AI 角色系统提示词
- 20+ 快捷提示词（6 大分类，含 12 个编程提示）
- Agent 模式专用系统提示词

## Agent 工具定义

| 工具名 | 功能 | 需要确认 |
|--------|------|---------|
| list_project_files | 扫描项目源文件结构 | 否 |
| list_files | 列出指定目录文件 | 否 |
| read_file | 读取文件内容 | 否 |
| create_file | 创建新文件并写入内容 | 否 |
| modify_file | 修改现有文件内容 | 否 |
| delete_file | 删除文件 | 是 |
| run_command | 执行 shell 命令 | 是 |

## AI 角色

| 角色 | 适用场景 |
|------|---------|
| 通用助手 | 通用问答，简洁直接 |
| 工业HMI | 工业人机界面，安全优先 |
| 智能家居 | 亲切口语，适合语音 |
| 调试专家 | 嵌入式 Linux 调试诊断 |
| **Qt开发** | Qt/C++ 代码生成（默认） |
| 数据分析 | 传感器数据解读 |

## 模型说明

| 模型 | 说明 |
|------|------|
| deepseek-v4-pro | 旗舰模型，代码生成能力最强（默认） |
| deepseek-chat | DeepSeek-V3，通用对话，快速响应 |
| deepseek-reasoner | DeepSeek-R1，深度推理模型 |

## 配置文件

`debug/config.ini`:
```ini
[General]
api_key=sk-xxxxxxxxxxxxxxxx
model=deepseek-v4-pro
role=4
project_dir=          ← Agent 模式项目目录
agent_mode=false      ← Agent 模式开关
```

## 在 Qt Creator 中打开

1. 打开 Qt Creator
2. File -> Open File or Project
3. 选择 `G:\qtproject\AiChat\AiChat.pro`
4. 选择 "Desktop Qt 5.14.2 MinGW 32-bit" 套件
5. 点击 Run（Ctrl+R）

## 获取 API Key

1. 访问 https://platform.deepseek.com
2. 注册/登录
3. 左侧菜单 -> API Keys
4. 创建 API Key
5. 复制 `sk-xxxxxxxxxxxxxxxx`
6. 在程序中点击设置 -> 粘贴 Key -> 保存

## 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| SSL supported: false | 缺少 OpenSSL | 将 libssl-1_1.dll 和 libcrypto-1_1.dll 放到 exe 目录 |
| HTTP 401 | API Key 无效 | 检查 Key 是否正确 |
| HTTP 403 | 余额不足 | 检查 DeepSeek 平台账户余额 |
| HTTP 404 | API URL 错误 | 确认是 /v1/chat/completions |
| QML 加载失败 | QML 模块缺失 | 运行 windeployqt --qmldir 重新部署 |
| Agent 不工作 | 未选择项目目录 | 切换 Agent 模式后先选择项目文件夹 |
| 编译命令失败 | 环境变量未设置 | CommandRunner 已预配置 Qt/MinGW 路径 |
