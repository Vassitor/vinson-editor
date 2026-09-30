> 归档说明：本文保存项目最初的开发任务书，包含早期阶段计划和当时的验收清单。
> 当前实现与目录结构以 [开发指南](../DEVELOPMENT.zh-CN.md)、[架构说明](../ARCHITECTURE.zh-CN.md)和[更新日志](../../CHANGELOG.zh-CN.md)为准。

# 桌面透明高性能文本编辑器开发任务规范

你是一名资深 C++ / Qt 桌面应用工程师和文本编辑器内核工程师。

请从零开始设计并实现一款**轻量、高性能、可高度自定义外观的桌面纯文本编辑器**。

项目必须是一个真正可长期维护和扩展的桌面应用，而不是 Demo。

请自主完成工程初始化、架构设计、代码实现、测试、构建配置和文档编写。遇到一般性的技术选择时自行做合理决策，不要频繁询问我。

---

# 一、项目目标

开发一款具有以下核心特点的桌面文本编辑器：

1. 极低资源占用。
2. 快速启动。
3. 支持超大文本文件。
4. 编辑大文件时保持 UI 响应。
5. 支持背景颜色自定义。
6. 支持背景透明度调整。
7. 支持背景完全透明。
8. 支持字体、字号、字体颜色设置。
9. 支持系统边框和无边框模式切换。
10. 窗口最小可以缩小到仅显示一行文本。
11. 支持一个真正的“极简模式”。
12. Windows 优先，同时尽量保持 Linux/macOS 可移植性。
13. 后续可以扩展标签页、语法高亮、插件等能力，但第一阶段不要过度设计。

这个软件首先是：

> 一个高性能纯文本编辑器。

其次才是：

> 一个外观高度自由的桌面悬浮文本窗口。

任何 UI 功能都不能明显牺牲大文件性能。

---

# 二、固定技术栈

优先使用：

- C++20
- Qt 6 Widgets
- Scintilla 5
- Lexilla
- CMake
- Qt Test / CTest
- Git

要求使用当前稳定的 Qt 6、Scintilla 5 和 Lexilla。

不要使用：

- Electron
- Chromium
- WebView
- React
- Vue
- HTML/CSS 编辑器界面
- Monaco Editor
- CodeMirror
- QTextEdit 作为正式编辑内核

允许使用 Qt 自带组件实现菜单、设置、对话框等 UI。

文本编辑核心必须基于：

> Scintilla

优先直接集成官方 Scintilla/Lexilla 源码，而不是依赖 QScintilla，以减少额外依赖并获得完整底层控制能力。

---

# 三、总体设计原则

必须遵循以下原则。

## 1. 模块化

禁止把大量逻辑堆进：

`MainWindow.cpp`

必须至少拆分：

- Application
- MainWindow
- EditorWidget
- EditorDocument
- WindowController
- FileManager
- LargeFileController
- SettingsManager
- ThemeManager
- SearchController

窗口、编辑器、文件 IO、配置、大文件逻辑必须彼此解耦。

## 2. UI 线程不能承担重型 IO

禁止在 UI 主线程一次性进行：

- 超大文件读取
- 大文件编码扫描
- 全文统计
- 大文件搜索索引
- 大文件 hash
- 大文件语法分析

需要使用：

- QThread
- QtConcurrent
- worker object
- 分块处理
- queued connection

等合理方式处理。

Scintilla 控件本身的更新仍然要遵循 GUI 线程约束。

## 3. 不要过早优化

普通文件使用简单可靠的实现。

只有达到大文件阈值后才进入专门的 Large File Mode。

## 4. 优先保证稳定

不要为了所谓“现代架构”引入：

- 不必要的依赖
- RPC
- Web 技术
- 数据库
- 网络服务
- 复杂插件系统

第一版本必须保持工程简单。

---

# 四、建议工程结构

创建类似以下结构：

```text
transparent-editor/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
├── docs/
│   ├── ARCHITECTURE.md
│   ├── PERFORMANCE.md
│   └── DEVELOPMENT.md
│
├── cmake/
│
├── third_party/
│   ├── scintilla/
│   └── lexilla/
│
├── src/
│   ├── main.cpp
│   │
│   ├── app/
│   │   ├── Application.h
│   │   └── Application.cpp
│   │
│   ├── window/
│   │   ├── MainWindow.h
│   │   ├── MainWindow.cpp
│   │   ├── WindowController.h
│   │   └── WindowController.cpp
│   │
│   ├── editor/
│   │   ├── EditorWidget.h
│   │   ├── EditorWidget.cpp
│   │   ├── EditorDocument.h
│   │   ├── EditorDocument.cpp
│   │   ├── EditorConfig.h
│   │   └── EditorConfig.cpp
│   │
│   ├── file/
│   │   ├── FileManager.h
│   │   ├── FileManager.cpp
│   │   ├── FileLoader.h
│   │   ├── FileLoader.cpp
│   │   ├── FileSaver.h
│   │   ├── FileSaver.cpp
│   │   ├── EncodingDetector.h
│   │   └── EncodingDetector.cpp
│   │
│   ├── largefile/
│   │   ├── LargeFileController.h
│   │   ├── LargeFileController.cpp
│   │   └── LargeFilePolicy.h
│   │
│   ├── search/
│   │   ├── SearchController.h
│   │   └── SearchController.cpp
│   │
│   ├── settings/
│   │   ├── SettingsManager.h
│   │   ├── SettingsManager.cpp
│   │   ├── ThemeManager.h
│   │   └── ThemeManager.cpp
│   │
│   └── ui/
│       ├── SettingsDialog.h
│       ├── SettingsDialog.cpp
│       ├── FindReplaceWidget.h
│       └── FindReplaceWidget.cpp
│
├── resources/
│
└── tests/
```

可以根据实际情况小幅调整，但必须保持职责清晰。

---

# 五、窗口系统

实现两种窗口模式。

## Normal Mode

普通编辑器窗口。

包含：

- 标题栏
- 菜单栏
- 编辑区域
- 状态栏

后续可以扩展标签页，但 MVP 不要求多标签。

## Frameless Mode

无系统边框。

要求：

- 没有操作系统标题栏
- 没有系统边框
- 编辑区域填满窗口
- 可以拖动窗口
- 可以调整窗口大小
- 必须保留重新进入设置界面的方式
- 必须有快捷键退出无边框模式

优先使用 Qt 的：

- `Qt::FramelessWindowHint`
- `QWindow::startSystemMove()`
- `QWindow::startSystemResize()`

尽量不要自己编写复杂的平台相关窗口拖动算法。

只有 Qt 无法满足需求时才增加 Windows Native Event 代码。

平台相关代码必须独立封装。

---

# 六、透明背景

应用启动时就应考虑透明窗口能力。

使用 Qt 顶层窗口透明能力设计。

Windows 下应考虑：

- `Qt::WA_TranslucentBackground`
- `Qt::FramelessWindowHint`

不要在运行过程中频繁增加/移除 `WA_TranslucentBackground`。

推荐策略：

应用创建窗口时即启用透明能力。

普通不透明背景使用：

```text
alpha = 255
```

完全透明使用：

```text
alpha = 0
```

这样透明度变化只改变背景绘制，而不需要反复重建 native window。

背景设置至少包含：

```text
red
green
blue
alpha
```

即：

```text
RGBA
```

透明度范围：

```text
0 - 255
```

或者 UI 显示：

```text
0% - 100%
```

其中：

- 100% = 完全不透明
- 0% = 完全透明

必须正确区分：

> 窗口背景透明

和：

> 整个窗口 opacity

不要简单调用整个窗口透明度导致文字也一起变透明。

用户调整背景透明度时：

**字体必须仍然可以保持完全不透明。**

例如：

```text
背景 alpha = 0
字体 alpha = 255
```

应该能够实现：

桌面上只看到文字。

---

# 七、字体和颜色

设置至少支持：

- 字体 family
- 字号
- 字体颜色
- 背景颜色
- 背景透明度
- 光标颜色
- 选中文字颜色

例如：

```text
Font:
JetBrains Mono

Font Size:
16

Text:
#FFFFFF

Background:
#000000

Background Alpha:
0
```

所有配置必须实时预览。

字体设置发生改变时，不应重新读取文档。

---

# 八、窗口最小尺寸

这是核心需求。

禁止设置传统桌面程序那种：

```text
minimumHeight = 200
```

窗口必须可以缩小到大约：

> 当前字体单行文本高度 + 必要 padding

最小高度必须根据：

`QFontMetrics`

动态计算。

例如：

```text
minimumHeight =
fontMetrics.height()
+ topPadding
+ bottomPadding
```

极简情况下，只显示：

```text
hello world_
```

不能因为：

- 菜单栏
- 工具栏
- 状态栏
- 滚动条

而阻止窗口进一步缩小。

---

# 九、Minimal Mode

实现专门的极简模式。

建议快捷键：

```text
Ctrl + Shift + M
```

进入 Minimal Mode 后：

- 隐藏菜单栏
- 隐藏标题栏
- 隐藏状态栏
- 隐藏行号
- 隐藏滚动条
- 隐藏额外工具栏
- 使用无边框窗口
- 允许背景完全透明
- 编辑区域填满窗口
- 允许窗口缩小到一行文本

桌面上的最终视觉效果可以只是：

```text
docker compose up -d
```

或者：

```text
TODO: deploy server tonight
```

用户必须能够通过快捷键再次退出 Minimal Mode。

建议：

```text
Ctrl + Shift + M
```

来回切换。

额外实现：

```text
Esc
```

不能直接导致未保存文本丢失。

---

# 十、窗口交互

Frameless / Minimal Mode 下至少支持：

### 移动窗口

实现可靠的拖动机制。

不要破坏正常文本选择。

优先方案：

按住：

```text
Alt + 鼠标左键
```

拖动窗口。

这样普通左键仍然负责文本选择。

### 修改字号

支持：

```text
Ctrl + Mouse Wheel
```

调整字号。

### 可选透明度快捷调整

可以支持：

```text
Alt + Mouse Wheel
```

调整背景透明度。

但必须能够关闭这个功能。

---

# 十一、文本编辑器核心

使用 Scintilla。

封装：

```text
EditorWidget
```

禁止 MainWindow 到处直接发送 Scintilla message。

应当由 EditorWidget 封装：

- setText
- appendText
- font
- fontSize
- textColor
- backgroundColor
- wrapMode
- lineNumber
- undo
- redo
- selectAll
- gotoLine
- currentLine
- currentColumn
- documentModified
- scroll
- search

等操作。

如果部分低级 Scintilla message 必须暴露，应保持接口集中。

---

# 十二、基础编辑功能

第一稳定版必须实现：

### File

- New
- Open
- Save
- Save As
- Reload
- Exit

### Edit

- Undo
- Redo
- Cut
- Copy
- Paste
- Select All

### Search

- Find
- Find Next
- Find Previous
- Replace
- Replace All
- Go To Line

### View

- Word Wrap
- Line Number
- Always On Top
- Frameless Mode
- Minimal Mode

### Settings

- Font
- Font Size
- Font Color
- Background Color
- Background Alpha
- Cursor Color

---

# 十三、文件拖放

支持把文件直接拖到窗口。

例如：

```text
xxx.log
xxx.txt
xxx.sql
xxx.json
```

拖入后直接打开。

如果当前文档存在未保存修改，需要先处理未保存状态。

---

# 十四、未保存状态

必须维护：

```text
modified
```

状态。

标题可以：

```text
example.txt
```

修改后：

```text
*example.txt
```

关闭应用、打开其他文件、Reload 时：

如果当前内容被修改：

必须提示：

```text
Save
Discard
Cancel
```

绝对不能静默丢失内容。

---

# 十五、安全保存

保存文件尽量避免：

> 程序崩溃导致原文件变成 0 字节。

普通文件优先研究使用：

`QSaveFile`

实现：

```text
写临时文件
↓
flush
↓
commit/atomic replace
```

对于超大文件，如果 QSaveFile 的复制/空间开销不合理，可以设计专门的大文件保存策略。

任何特殊策略都必须写入：

`docs/PERFORMANCE.md`

说明原因和风险。

---

# 十六、文件编码

第一版本至少可靠支持：

- UTF-8
- UTF-8 BOM
- ASCII

在能力允许时支持：

- UTF-16 LE
- UTF-16 BE

不要一开始开发复杂通用编码探测器。

优先：

1. BOM
2. UTF-8 validity
3. fallback

保存时默认保留原始编码。

同时保留原始换行符：

- LF
- CRLF

状态栏显示：

```text
UTF-8 | CRLF
```

---

# 十七、大文件模式

这是整个项目最重要的技术要求之一。

不要等普通编辑器全部完成以后再补“大文件优化”。

从架构第一天开始设计：

```text
Normal File
Large File
Very Large File
```

建议默认阈值：

```text
< 64 MiB
Normal Mode

>= 64 MiB
Large File Mode

>= 512 MiB
Very Large File Mode
```

阈值必须集中定义，不允许散落 magic number。

以后允许用户修改。

---

# 十八、Scintilla Large Document

64 位程序必须研究并正确使用：

```text
SC_DOCUMENTOPTION_TEXT_LARGE
```

对于大文件，根据实际 Scintilla API 创建合适的 document。

例如应研究：

```text
SCI_CREATEDOCUMENT
SC_DOCUMENTOPTION_TEXT_LARGE
SC_DOCUMENTOPTION_STYLES_NONE
```

等能力。

不要在加载完普通 Scintilla document 后才发现需要 Large Text Mode。

Large Text Mode 必须在 document 生命周期设计阶段正确处理。

所有 Scintilla document：

- create
- attach
- addref
- release

必须严格遵守引用计数规则。

---

# 十九、Large File Mode 策略

进入 Large File Mode 后默认关闭可能产生高开销的功能。

至少考虑关闭：

- Word Wrap
- Syntax Highlight
- Lexer
- Semantic Analysis
- Spell Check
- Minimap
- 全文实时统计
- 实时全文搜索索引
- 自动格式化
- 实时文件 hash
- 大范围自动检测
- 复杂代码折叠

普通纯文本显示仍然必须可用。

Scintilla Large Document 可以考虑：

```text
SC_DOCUMENTOPTION_TEXT_LARGE
+
SC_DOCUMENTOPTION_STYLES_NONE
```

Large File Mode 下优先使用 null lexer。

---

# 二十、Very Large File Mode

对于：

```text
>= 512 MiB
```

进一步限制高成本操作。

可以考虑：

- 限制 Undo buffer
- 禁用自动备份
- 禁用 Word Wrap
- 禁止后台全文扫描
- 禁止自动全文 replace preview
- 延迟状态统计
- 按需计算行列
- 分块文件读取

但：

> 不允许直接禁止编辑，除非确实存在平台或 Scintilla 限制。

如果必须降级，应明确向用户显示当前 Large File Mode 状态。

例如状态栏：

```text
UTF-8 | LF | Large File Mode
```

---

# 二十一、超 2GB 文件

应用必须使用 64 位构建。

对于超过 2GB 的文件：

研究并使用 Scintilla：

```text
SC_DOCUMENTOPTION_TEXT_LARGE
```

不要声称能够保证任意大小文件完全无性能损失。

对于超大文件需要：

1. 检测文件大小。
2. 进入专门模式。
3. 禁用高成本功能。
4. 向用户显示当前模式。

如果发现 Scintilla 全量 document 模型对某些极端文件仍产生过高内存开销，将情况写入：

`docs/PERFORMANCE.md`

不要立即自己实现完整文本编辑器内核。

只有经过 benchmark 证明 Scintilla 成为瓶颈后，才考虑：

- memory mapped file
- piece table
- rope
- paged document
- viewport document

这些属于未来阶段。

MVP 不自行重新实现 Scintilla。

---

# 二十二、大文件读取

禁止类似：

```text
QFile::readAll()
```

读取巨大文件。

大文件必须：

```text
open
↓
分块读取
↓
分块送入编辑器
↓
允许事件循环继续执行
```

需要保证加载过程中：

- 窗口仍能移动
- 窗口仍能重绘
- 可以取消打开操作
- 不出现长时间“未响应”

可以设计：

```text
FileLoader worker
        ↓
chunk
        ↓
queued signal
        ↓
EditorWidget
```

注意：

不能从 worker thread 直接操作 QWidget / Scintilla GUI control。

---

# 二十三、加载状态

加载大文件期间应提供轻量提示。

例如状态栏：

```text
Loading 382 MB / 1.4 GB
```

提供：

```text
Cancel
```

不要弹一个会完全阻塞窗口的 modal progress dialog。

加载完成后取消状态显示。

---

# 二十四、搜索性能

普通文件：

直接使用 Scintilla 搜索能力。

大文件：

搜索必须避免：

```text
复制整个 document
↓
std::string
↓
再全文搜索
```

尽量直接使用 Scintilla target/search API。

查找：

```text
Find Next
Find Previous
```

必须能够处理大文件。

对于：

```text
Replace All
```

在超大文件上执行前应提示用户，因为可能耗时很长。

---

# 二十五、自动换行

Word Wrap 是大文件常见性能问题。

因此：

Normal Mode：

```text
允许
```

Large File Mode：

```text
默认关闭
```

用户仍然可以手动开启，但可以显示性能提示。

---

# 二十六、行号

支持显示：

```text
1
2
3
...
```

Normal Mode 默认开启。

Minimal Mode 默认关闭。

超大文件情况下需要测试动态行号宽度是否造成明显性能问题。

---

# 二十七、Always On Top

支持：

```text
Always On Top
```

用于把编辑器作为桌面便签或悬浮文本窗口。

建议快捷键：

```text
Ctrl + Shift + T
```

使用 Qt：

```text
Qt::WindowStaysOnTopHint
```

处理窗口 flag 改变时，要确保：

- 窗口位置不丢失
- 大小不丢失
- 当前文档不丢失
- 焦点恢复正常

---

# 二十八、设置持久化

使用：

`QSettings`

保存：

```text
window geometry
window position
font family
font size
font color
background color
background alpha
cursor color
word wrap
line number
always on top
frameless mode
last directory
```

Minimal Mode 是否在重启后自动恢复可以设计为可配置。

不要保存：

- 当前文本全文
- 巨大文件内容

到 QSettings。

---

# 二十九、配置恢复

如果窗口上次关闭时位于已经不存在的显示器：

例如：

```text
Display 2
```

被拔掉，

应用启动后必须检测窗口 geometry 是否仍然位于可用屏幕范围内。

否则恢复到主显示器合理位置。

不要让窗口启动到屏幕外。

---

# 三十、快捷键

第一版建议：

```text
Ctrl + N
New

Ctrl + O
Open

Ctrl + S
Save

Ctrl + Shift + S
Save As

Ctrl + F
Find

Ctrl + H
Replace

Ctrl + G
Go To Line

Ctrl + Z
Undo

Ctrl + Y
Redo

Ctrl + A
Select All

Ctrl + MouseWheel
Font Size

Ctrl + Shift + M
Minimal Mode

Ctrl + Shift + T
Always On Top

F11
Frameless / Focus Mode
```

如果快捷键和操作系统常用快捷键冲突，请合理调整，并记录在 README。

---

# 三十一、右键菜单

EditorWidget 提供基本 context menu：

```text
Undo
Redo
--------
Cut
Copy
Paste
--------
Select All
--------
Find
```

Minimal Mode 下仍然应该可用。

---

# 三十二、状态栏

Normal Mode 显示：

```text
Ln 32, Col 18
UTF-8
CRLF
12.4 MB
```

大文件增加：

```text
Large File Mode
```

例如：

```text
Ln 32, Col 18 | UTF-8 | LF | 825 MB | Large File Mode
```

状态计算不能频繁扫描全文。

---

# 三十三、异常处理

必须正确处理：

- 文件不存在
- 没有读取权限
- 没有写入权限
- 文件正在被其他程序占用
- 磁盘空间不足
- 文件读取过程中被删除
- 文件保存过程中失败
- 非法编码
- 超大文件
- 内存不足
- 文件路径包含中文
- 文件路径包含空格
- 文件路径包含 Unicode

用户看到的是清晰错误提示。

程序不能直接崩溃。

---

# 三十四、日志

实现轻量日志。

至少区分：

```text
INFO
WARNING
ERROR
```

Debug 构建可以输出更多信息。

不要每次输入字符都写磁盘日志。

---

# 三十五、性能原则

必须避免：

- 每次输入都全文扫描
- 每次滚动都重新构造整个文档
- 每次光标移动都计算全文字符数
- 打开大文件时 readAll
- 把大文件转换成多个完整 QString 副本
- 无意义 UTF-8 → UTF-16 → UTF-8 往返
- 保存时建立多个完整文件副本
- 大文件实时 lexer
- 大文件实时 Word Wrap

特别注意 QString 使用可能带来的 UTF-16 内存开销。

大文件数据路径应尽量减少：

```text
file bytes
→ QString
→ std::string
→ Scintilla
```

这样的重复复制。

---

# 三十六、性能基准

创建：

```text
docs/PERFORMANCE.md
```

并提供 benchmark 测试方式。

准备测试文件：

```text
10 MB
100 MB
500 MB
1 GB
```

如果磁盘空间允许，再测试：

```text
2 GB+
```

记录：

- 打开耗时
- UI 是否无响应
- 内存增长
- 搜索耗时
- 滚动流畅度
- 保存耗时

不要写：

> 大文件不卡顿

这种无法验证的描述。

应记录真实测试结果。

性能验收的核心不是某个绝对秒数，而是：

> 大文件操作过程中 UI 事件循环必须保持可响应，不应该长时间出现系统“未响应”。

---

# 三十七、内存测试

特别监控：

```text
500 MB file
1 GB file
```

打开后的 Resident Memory。

如果出现：

```text
1GB 文件
→ 5GB / 10GB 内存
```

必须分析数据复制位置。

重点排查：

- QByteArray copy
- QString copy
- std::string copy
- Scintilla document
- Undo buffer
- style buffer

并在 PERFORMANCE.md 记录。

---

# 三十八、测试

至少建立：

```text
tests/
```

测试：

### Settings

- 保存
- 读取
- 默认值
- 非法值

### Encoding

- UTF-8
- UTF-8 BOM
- CRLF
- LF

### File

- Open
- Save
- Save As
- Empty File
- Unicode Path

### Large File Policy

输入：

```text
1MB
63MB
64MB
511MB
512MB
1GB
```

验证模式选择。

核心业务逻辑尽量可以脱离 GUI 测试。

---

# 三十九、构建

使用：

```text
CMakePresets.json
```

建议支持：

```text
Debug
Release
```

Windows：

优先支持：

```text
MSVC 2022+
```

同时尽量兼容：

```text
MinGW
```

最终 Release 必须是：

```text
x86_64 / 64-bit
```

不要提供 32 位构建作为主要发行版本。

---

# 四十、Windows 部署

提供：

```text
scripts/package-windows.*
```

或 CMake packaging。

正确处理：

```text
Qt DLL
platform plugins
Scintilla
Lexilla
```

保证打包后能在未安装 Qt SDK 的普通 Windows 环境运行。

可以使用：

```text
windeployqt
```

处理 Qt Runtime。

后续可以增加：

```text
Inno Setup
```

生成 installer。

MVP 先保证 portable package 可运行。

---

# 四十一、Linux

代码设计不能把所有窗口逻辑写死成 Win32 API。

Linux 尽可能使用 Qt abstraction。

后续目标：

```text
AppImage
```

但 Windows MVP 完成之前，不需要把大量时间用于 Linux 打包。

---

# 四十二、编码规范

使用现代 C++。

允许：

- RAII
- smart pointer
- enum class
- std::optional
- std::filesystem
- constexpr
- scoped connection
- strong types

禁止：

- 大量裸 new/delete
- 全局可变状态
- 无意义 singleton
- 巨型 MainWindow
- 巨型 God Class
- 到处 static variable
- catch(...)
- 空 catch
- 忽略文件错误

使用：

```text
nullptr
```

而不是：

```text
NULL
```

---

# 四十三、注释规范

注释解释：

> 为什么这样做。

而不是：

```cpp
// Set font
setFont(font);
```

对于：

- Scintilla 生命周期
- Large Text Mode
- Window transparency
- Native window behaviour
- Threading

必须留下必要说明。

---

# 四十四、文档

必须维护：

## README.md

包含：

- 项目介绍
- Features
- Screenshot placeholder
- Build
- Run
- Keyboard Shortcuts

## docs/ARCHITECTURE.md

解释：

```text
Application
↓
MainWindow
↓
EditorWidget
↓
Scintilla
```

以及：

```text
FileManager
LargeFileController
SettingsManager
```

之间关系。

## docs/PERFORMANCE.md

记录：

- Large File Mode
- Scintilla document options
- benchmark
- 已知限制

## docs/DEVELOPMENT.md

说明开发环境和依赖。

---

# 四十五、不要过早实现以下功能

第一阶段不要实现：

- AI
- Markdown Preview
- Git
- LSP
- Cloud Sync
- Collaboration
- SSH
- FTP
- Plugin Marketplace
- Terminal
- Browser
- Database
- Monaco
- WebView
- Workspace
- 项目管理
- IDE 功能

这些全部不属于 MVP。

首先把：

> 文本编辑 + 透明窗口 + 极简模式 + 大文件性能

做到稳定。

---

# 四十六、开发阶段

不要一次写完所有代码。

按照以下阶段执行。

---

## Phase 0：环境检查

检查：

```text
compiler
cmake
Qt
git
```

确认平台。

如果缺少依赖：

明确输出安装方法。

不要擅自破坏系统现有开发环境。

---

## Phase 1：工程骨架

实现：

- CMake
- QApplication
- MainWindow
- EditorWidget
- Scintilla integration

验收：

```text
程序可以启动
编辑器可以输入文本
可以调整窗口大小
```

完成后提交：

```text
docs/ARCHITECTURE.md
```

---

## Phase 2：文件系统

实现：

- New
- Open
- Save
- Save As
- modified state
- Safe Save
- Drag & Drop

完成相关测试。

---

## Phase 3：编辑功能

实现：

- Undo
- Redo
- Cut
- Copy
- Paste
- Select All
- Find
- Replace
- Go To Line

---

## Phase 4：Appearance

实现：

- Font
- Font Size
- Text Color
- Background Color
- Background Alpha
- Cursor Color

验证：

```text
background alpha = 0
```

时：

> 背景完全透明，但字体仍然正常显示。

---

## Phase 5：Frameless Mode

实现：

- Frameless
- Window Move
- Window Resize
- Always On Top

确保：

> 文本选择不会被窗口拖动逻辑破坏。

---

## Phase 6：Minimal Mode

实现：

```text
Ctrl + Shift + M
```

进入：

- Frameless
- No Menu
- No Status
- No Scrollbar
- No Line Number

窗口允许缩到：

```text
单行高度
```

这是核心验收项。

---

## Phase 7：Large File Mode

实现：

```text
LargeFilePolicy
```

以及：

```text
SC_DOCUMENTOPTION_TEXT_LARGE
SC_DOCUMENTOPTION_STYLES_NONE
```

根据文件大小自动选择模式。

实现分块读取。

禁止大型文件：

```text
QFile::readAll()
```

---

## Phase 8：性能优化

生成：

```text
10MB
100MB
500MB
1GB
```

测试文件。

测试：

- Open
- Search
- Scroll
- Edit
- Save

分析内存复制。

修复明显性能问题。

记录到：

```text
docs/PERFORMANCE.md
```

---

## Phase 9：配置持久化

实现：

```text
QSettings
```

恢复：

- Window geometry
- Font
- Colors
- Alpha
- Always On Top
- View options

---

## Phase 10：Release

完成：

- Release build
- windeployqt
- portable package
- README
- CHANGELOG

确保干净机器可以运行。

---

# 四十七、每阶段工作方式

每开始一个阶段前：

1. 阅读当前代码。
2. 阅读 ARCHITECTURE.md。
3. 检查已有实现。
4. 不要重复实现已有能力。
5. 给出该阶段简短实施计划。
6. 开始编码。
7. 编译。
8. 修复编译错误。
9. 运行测试。
10. 修复测试失败。
11. 更新相关文档。

不要只生成代码而不验证。

---

# 四十八、Codex 行为规范

你拥有较高自主权。

对于一般工程选择：

直接做合理决定。

不要因为以下问题停下来询问：

```text
变量叫什么
文件放哪个目录
某个按钮放哪里
是否使用 enum
具体 class 名
```

自己按照最佳实践决定。

只有发生真正影响产品方向的问题时才询问。

---

# 四十九、禁止行为

禁止：

### 1

为了快速完成而把全部实现写进：

```text
main.cpp
```

### 2

为了快速完成而用：

```text
QTextEdit
```

代替 Scintilla。

### 3

打开大文件直接：

```text
readAll()
```

### 4

大文件先全部转换成 QString 再传给 Scintilla。

### 5

在 UI Thread 做超大文件全文分析。

### 6

声称：

```text
支持无限大小文件
```

### 7

没有 benchmark 就声称：

```text
1GB 文件零延迟
```

### 8

为未来功能预先引入巨大 framework。

### 9

修改一个功能时顺便重构无关模块。

### 10

通过删除测试来解决测试失败。

---

# 五十、核心验收标准

最终 MVP 必须达到：

## UI

- [ ] Windows 可以正常启动
- [ ] 支持修改字体
- [ ] 支持修改字号
- [ ] 支持修改字体颜色
- [ ] 支持修改背景颜色
- [ ] 支持调整背景透明度
- [ ] 支持背景完全透明
- [ ] 背景透明不会让字体一起透明
- [ ] 支持无边框
- [ ] 支持 Always On Top
- [ ] 支持 Minimal Mode

## Minimal Mode

- [ ] 无标题栏
- [ ] 无菜单栏
- [ ] 无状态栏
- [ ] 无滚动条
- [ ] 可以继续输入
- [ ] 可以选择文本
- [ ] 可以移动窗口
- [ ] 可以退出 Minimal Mode
- [ ] 窗口可以缩小到一行文字高度

## Editor

- [ ] New
- [ ] Open
- [ ] Save
- [ ] Save As
- [ ] Undo
- [ ] Redo
- [ ] Find
- [ ] Replace
- [ ] Go To Line
- [ ] Drag & Drop

## Safety

- [ ] 未保存退出提示
- [ ] 保存失败不会破坏源文件
- [ ] Unicode path 正常
- [ ] 空文件正常
- [ ] 无权限文件有清晰错误

## Large File

- [ ] 使用 64-bit build
- [ ] 有 Large File Mode
- [ ] 有 Very Large File Mode
- [ ] 大文件关闭高成本特性
- [ ] 不使用 readAll 加载巨大文件
- [ ] UI 加载过程中仍能响应
- [ ] Scintilla Large Text Mode 正确使用
- [ ] 500MB 文件完成真实测试
- [ ] 1GB 文件完成真实测试
- [ ] benchmark 记录到 PERFORMANCE.md

---

# 五十一、最终产品方向

始终记住这个软件最终应该同时具备两种使用方式。

普通状态：

```text
┌──────────────────────────────────────────┐
│ example.txt                         ─ □ × │
├──────────────────────────────────────────┤
│ Hello                                    │
│                                          │
│ This is a text editor.                   │
│                                          │
├──────────────────────────────────────────┤
│ Ln 3, Col 23 | UTF-8 | LF                │
└──────────────────────────────────────────┘
```

极简状态：

```text
TODO: restart production server
```

甚至：

```text
背景：完全透明
边框：无
状态栏：无
菜单：无
```

桌面上视觉上只存在文字。

但无论处于哪种模式，它本质上仍然必须是：

> 一个可靠、快速、高性能的文本编辑器。

---

# 五十二、现在开始

现在执行以下任务：

1. 检查当前仓库状态。
2. 如果仓库为空，创建完整工程结构。
3. 检查当前操作系统和 C++/Qt/CMake 开发环境。
4. 确定 Scintilla 5 + Lexilla 的直接集成方式。
5. 创建 CMake 工程。
6. 完成 Phase 1。
7. 实际执行 configure/build。
8. 修复所有编译错误。
9. 启动程序验证基础编辑窗口能够工作。
10. 创建或更新 README.md 和 docs/ARCHITECTURE.md。
11. 汇报本阶段：
   - 创建了哪些文件
   - 架构如何设计
   - 编译结果
   - 测试结果
   - 下一阶段工作
12. 不要直接跨越所有阶段一次性生成几千行未经验证的代码。

如果发现当前设计中的某项要求与 Scintilla / Qt 实际 API 不符，以官方文档和实际编译结果为准进行合理调整，并在文档中解释原因。

开始执行 Phase 0 和 Phase 1。
