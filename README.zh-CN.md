# Vinson Editor

简体中文 | [English](README.md)

Vinson Editor 是一款使用 C++20、Qt 6 Widgets 和 Scintilla 构建的轻量级原生纯文本编辑器，支持异步文件加载、安全保存、查找与替换、实时外观定制、窗口模式切换，以及专门的大文档处理。

## 当前功能

- 带菜单栏和状态栏的原生 Qt 6 桌面窗口
- 自助导入和更新 JavaScript 文本插件，支持专属设置、命令快捷键、执行参数、当前行／选区／文档处理、复制结果及生成新标签页；后台执行可取消和撤销，内置 11 个文本工具命令，见[插件指南](docs/PLUGINS.zh-CN.md)
- 直接从上游源码编译的 Scintilla 5.6.6 编辑控件
- UTF-8 文本输入、撤销/重做、剪贴板操作、自动换行和行号
- 当前文档会话的可停靠编辑历史时间线，标记当前/已保存状态并支持恢复
- 新建、打开、保存、另存为、重新加载、退出及单文件拖放
- 多文档标签页、会话恢复、最近文件，以及重新打开最近关闭的文件标签页
- 监测磁盘上的文件修改、删除和重命名；无本地修改时自动重新加载，有冲突时由用户决定
- 保存前校验磁盘文件版本，并对不超过 8 MiB 的文件检查内容指纹，避免未经确认覆盖其他程序的修改
- 为普通文档后台保存受容量限制的原子恢复快照，并在异常退出后的下次启动提供恢复
- 在新建、打开、重新加载和退出前保护未保存的更改
- 支持 UTF-8、带 BOM 的 UTF-8、ASCII、UTF-16 LE 和 UTF-16 BE 往返读写
- “文件 → 保存编码”可选择 UTF-8、UTF-8 BOM、UTF-16 LE 或 UTF-16 BE；编码变更独立于文本撤销，触发未保存提示。
- “文件 → 转换换行符”可统一为 LF、CRLF 或 CR，转换可一次撤销，并为当前标签页设置后续输入的换行符。
- 检测 LF、CRLF、CR 和混合换行符，仅在明确选择转换时规范化已有内容
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
- 大文件查找按分片调度，显示搜索进度，可通过“取消搜索”、状态栏取消按钮或 `Esc` 取消；取消后保留文本、选区及撤销记录
- 替换当前项、以单个撤销操作执行全部替换，以及转到指定行
- 当前文档的行书签：点击行号旁的书签栏或使用快捷键添加、移除和清除，上一处／下一处跳转支持循环；书签随行编辑调整并在标签切换时保留，关闭或重新加载文档时清除
- 实时定制字体、字号、文字、背景、光标和选中文字颜色
- 保存多套命名自定义样式，并可为每套样式设置切换快捷键或通过 JSON 导入、导出
- 背景与文字不透明度可分别调整：0 为完全透明，255 为不透明；背景不透明度仅在无边框和极简模式下生效
- 可配置系统级老板键，以及用于显示、还原并激活窗口后聚焦编辑区以便立即输入的聚焦快捷键
- 无边框模式支持拖动顶部边缘或使用 `Alt+鼠标左键拖动` 进行系统移动，支持边缘缩放，最小高度随字号和行距调整，可缩小到一行
- 可与无边框模式组合使用的窗口置顶模式
- 极简模式隐藏全部窗口装饰，并允许动态缩小为单行窗口
- 使用 `Ctrl+Shift+M` 或 `Esc` 退出极简模式，且不会改变文档内容
- 仅支持 64 位的 CMake 配置，提供 Debug 和 Release 预设
- 支持无界面的 Qt Test 测试及应用启动冒烟测试
- 使用 CPack 生成包含 Qt Runtime 和 SHA-256 校验和的便携归档包

## 依赖

- 64 位 C++20 编译器（MSVC 2022+、GCC 12+ 或 Clang 15+）
- CMake 3.25+
- Ninja
- Qt 6.5+，包含 Widgets、Test、Core5Compat 和 Qml
- Qt Linguist Tools（可选；编译内置翻译时需要）

Scintilla 5.6.6 的官方发布源码已包含在 `third_party/` 中。当前编辑器使用纯文本模式，无需额外的词法分析器库。

在 Ubuntu 26.04 上安装开发依赖：

```bash
sudo apt update
sudo apt install cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-5compat-dev qt6-declarative-dev
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

生成 Windows 安装版（另需安装 [NSIS 3.03+](https://nsis.sourceforge.io/Download)）：

```powershell
.\scripts\package-installer-windows.ps1
# 使用解压的 NSIS 工具目录：
.\scripts\package-installer-windows.ps1 -NsisRoot "C:\Tools\NSIS"
```

脚本依次构建、运行完整测试、部署 Qt 并生成 `.exe` 和 `.sha256`，输出位于
`build/release/packages/<时间戳>/`。安装版提供开始菜单快捷方式、第三方许可证入口、
Windows 应用列表中的卸载入口。重复安装时自动读取现有安装目录并显示覆盖更新提醒；如果编辑器正在运行，
安装程序会询问是否停止，停止成功后继续覆盖安装。安装需管理员权限。
确认停止会强制结束正在运行的进程，弹窗会提醒未保存内容可能丢失。
卸载保留用户设置和文档。脚本不自动安装 NSIS；`-NoBootstrap` 可禁止自动准备其他构建依赖。

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

所有应用命令的键盘快捷键均可在**设置 → 外观和快捷键 → 快捷键**中重新绑定或清除；
自定义样式的切换快捷键可在“外观”页中随样式一起编辑。鼠标滚轮和拖动手势保留
下表所示的绑定。

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
| 添加 / 移除当前行书签 | `Ctrl+F2` |
| 下一处 / 上一处书签 | `F2` / `Shift+F2` |
| 清除当前文档全部书签 | `Ctrl+Shift+F2` |
| 编辑历史 | `Ctrl+Shift+H` |
| 上一个 / 下一个自定义样式 | `Ctrl+Alt+PageUp` / `Ctrl+Alt+PageDown` |
| 调整字号 | `Ctrl+鼠标滚轮` |
| 窗口置顶 | `Ctrl+Shift+T` |
| 无边框模式 | `F11` |
| 移动无边框窗口 | 拖动顶部边缘或使用 `Alt+鼠标左键拖动` |
| 极简模式 | `Ctrl+Shift+M` |
| 退出极简模式 | `Esc` 或 `Ctrl+Shift+M` |

## 文档导航

[文档索引](docs/README.md)汇总开发指南、架构说明、性能记录和发布步骤。
应用源码位于 `src/`，手动性能基准位于 `benchmarks/`，依赖集成和发布配置位于 `cmake/`。

## 许可证

项目代码使用 MIT 许可证。Scintilla 的上游许可证保留在 `third_party/scintilla/License.txt` 中。
第三方版权声明、Qt SDK 组件清单及许可证全文见
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，发布包会一并包含这些文件。
