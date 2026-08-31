# Vinson Editor

简体中文 | [English](README.md)

Vinson Editor 是一款使用 C++20、Qt 6 Widgets 和 Scintilla 构建的轻量级原生纯文本编辑器。项目现已完成第 10 阶段的发布工程：原生编辑界面具备异步文件加载、安全保存、基于目标范围的查找与替换、实时外观定制、可逆窗口模式，以及专门的大文档处理路径。

## 当前功能

- 带菜单栏和状态栏的原生 Qt 6 桌面窗口
- 直接从上游源码编译的 Scintilla 5.6.6 编辑控件
- UTF-8 文本输入、撤销/重做、剪贴板操作、自动换行和行号
- 新建、打开、保存、另存为、重新加载、退出及单文件拖放
- 在新建、打开、重新加载和退出前保护未保存的更改
- 支持 UTF-8、带 BOM 的 UTF-8、ASCII、UTF-16 LE 和 UTF-16 BE 往返读写
- 检测 LF、CRLF、CR 和混合换行符，且不会规范化已有内容
- 工作线程以 256 KiB 分块加载，GUI 队列有界并支持取消
- 工作线程通过 `QSaveFile` 原子保存
- 在统一的 64/512 MiB 阈值下自动切换普通、大文件和超大文件模式
- 加载大文件前选择 Scintilla 大文本、无样式文档
- 大文件默认关闭自动换行并使用空 lexer
- 大文件按范围流式原子保存，无需创建完整文档快照
- 状态栏明确显示当前模式，并在超大文件中执行全部替换前确认
- 可复现的 10 MiB–1 GiB 加载、搜索、编辑和保存基准工具
- 通过校验后的 `QSettings` 持久化窗口几何、外观、视图选项、窗口标志和最近目录
- 当已保存窗口位置不再与任何可用显示器相交时自动恢复
- 状态栏反馈光标行列、编码、换行符、文件大小和修改状态
- 非模态查找/替换，支持上一个、下一个、可选循环查找、区分大小写和全字匹配
- 替换当前项、以单个撤销操作执行全部替换，以及转到指定行
- 实时定制字体、字号、文字、背景、光标和选中文字颜色
- 背景与编辑区字体透明度可分别在 0–255 范围内调整
- 可配置系统级老板键，以及用于显示、还原并激活窗口后聚焦编辑区以便立即输入的聚焦快捷键
- 无边框模式支持拖动顶部边缘或使用 `Alt+鼠标左键拖动` 进行系统移动，并支持边缘缩放
- 可与无边框模式组合使用的窗口置顶模式
- 极简模式隐藏全部窗口装饰，并允许动态缩小为单行窗口
- 使用 `Ctrl+Shift+M` 或 `Esc` 退出极简模式，且不会改变文档内容
- 仅支持 64 位的 CMake 配置，提供 Debug 和 Release 预设
- 支持无界面的 Qt Test 测试及应用启动冒烟测试
- 使用 CPack 生成包含 Qt Runtime 和 SHA-256 校验和的便携归档包

## 截图

> 截图占位：首个带标签的二进制版本发布时将补充 Windows 便携版截图。

## 依赖

- 64 位 C++20 编译器（MSVC 2022+、GCC 12+ 或 Clang 15+）
- CMake 3.25+
- Ninja
- Qt 6.5+，包含 Widgets、Test 和 Core5Compat
- Qt Linguist Tools（可选；编译内置翻译时需要）

Scintilla 5.6.6 和 Lexilla 5.5.3 的官方发布源码已包含在 `third_party/` 中。Lexilla 当前仅固定版本，待引入 lexer 支持时再链接；目前编辑器有意保持纯文本模式。

在 Ubuntu 26.04 上安装开发依赖：

```bash
sudo apt update
sudo apt install cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-5compat-dev
```

如需编译内置翻译，再安装：

```bash
sudo apt install qt6-tools-dev
```

## 构建和运行

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/vinson-editor
```

运行测试套件：

```bash
ctest --preset debug
```

执行非交互式启动检查（适用于 CI）：

```bash
QT_QPA_PLATFORM=offscreen ./build/debug/vinson-editor --smoke-test
```

在 Windows 上构建经过测试的 Release 便携包：

```powershell
.\scripts\package-windows.ps1
```

也可以在运行 Release 测试后使用跨平台 CMake 打包预设：

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --build --preset release-package
```

Windows 包会自动运行 `windeployqt`，无需 Qt SDK 即可启动。包内容和纯净环境验收步骤参见 [docs/RELEASE.zh-CN.md](docs/RELEASE.zh-CN.md)。

平台说明和依赖策略参见 [docs/DEVELOPMENT.zh-CN.md](docs/DEVELOPMENT.zh-CN.md)。

## 键盘快捷键

| 操作 | 快捷键 |
| --- | --- |
| 新建 | `Ctrl+N` |
| 打开 | `Ctrl+O` |
| 保存 | `Ctrl+S` |
| 另存为 | `Ctrl+Shift+S` |
| 重新加载 | `Ctrl+Shift+R` |
| 退出 | 平台标准快捷键（Linux 上为 `Ctrl+Q`） |
| 撤销 | `Ctrl+Z` |
| 重做 | 平台标准快捷键（Linux 上为 `Ctrl+Shift+Z`） |
| 剪切 / 复制 / 粘贴 | `Ctrl+X` / `Ctrl+C` / `Ctrl+V` |
| 全选 | `Ctrl+A` |
| 查找 / 替换 | `Ctrl+F` / `Ctrl+H` |
| 查找下一个 / 上一个 | `F3` / `Shift+F3` |
| 转到行 | `Ctrl+G` |
| 调整字号 | `Ctrl+鼠标滚轮` |
| 窗口置顶 | `Ctrl+Shift+T` |
| 无边框模式 | `F11` |
| 移动无边框窗口 | 拖动顶部边缘或使用 `Alt+鼠标左键拖动` |
| 极简模式 | `Ctrl+Shift+M` |
| 退出极简模式 | `Esc` 或 `Ctrl+Shift+M` |

## 许可证

项目代码使用 MIT 许可证。Scintilla 和 Lexilla 分别保留其 `third_party` 目录中的上游许可证。
