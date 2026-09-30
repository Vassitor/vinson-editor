# 开发指南

简体中文 | [English](DEVELOPMENT.md)

## 支持的工具链

主要发布目标是使用 MSVC 2022 或更新版本的 64 位 Windows。Linux 用于持续开发和可移植性检查。CMake 项目会拒绝 32 位配置。

最低开发要求为 CMake 3.25、Ninja、C++20 编译器，以及包含 Core、Gui、Widgets、Test 和 Core5Compat 的 Qt 6.5。Qt Linguist Tools 是可选依赖；安装后会编译内置翻译并启用本地化测试。已记录的性能测试环境和测量结果见 [PERFORMANCE.zh-CN.md](PERFORMANCE.zh-CN.md)。

Ubuntu 环境配置：

```bash
sudo apt update
sudo apt install cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-5compat-dev
```

如需编译翻译：

```bash
sudo apt install qt6-tools-dev
```

随后配置、构建并测试：

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`release-package` 构建预设用于生成发布归档。在 Windows 上，`scripts/package-windows.ps1` 会执行完整的配置、构建、测试和打包流水线；部署细节参见 [RELEASE.zh-CN.md](RELEASE.zh-CN.md)。

如果 Qt 安装在非系统前缀中，请在配置前把 `CMAKE_PREFIX_PATH` 指向 Qt 安装前缀。

## 第三方源码策略

`third_party/scintilla` 是 Scintilla 5.6.6 官方发布源码。请保持该目录不含本地源码修改，使上游升级易于审查。项目专用构建逻辑应放在 `cmake/` 中。RGBA 前景/背景适配会生成在 `build/<preset>/generated/scintilla` 下，绝不能直接修改供应商源码。

升级 Scintilla 时：

1. 从 `scintilla.org` 下载稳定版本。
2. 校验发布版本并保留上游许可证。
3. 用无本地修改的新源码替换对应目录。
4. 配置并构建 Debug 和 Release 预设。
5. 运行完整测试套件并更新架构及性能说明。

## 仓库布局

- `CMakeLists.txt`：工具链要求、构建选项和子目录组织
- `src/CMakeLists.txt`：应用库、可执行文件、资源和翻译
- `src/app`：程序入口、应用生命周期和单实例启动
- `src/window`：顶层窗口和窗口模式控制
- `src/editor`：面向 Scintilla 的编辑器抽象
- `src/file`：编码/换行符检测、有界异步加载和原子保存
- `src/largefile`：统一阈值和大文档行为
- `src/search`：搜索/替换编排
- `src/session`：最近文件、关闭标签历史及恢复快照
- `src/settings`：外观主题和经过校验的 `QSettings` 持久化
- `src/ui`：职责集中的对话框和可复用 UI 组件
- `tests`：Qt Test 源码、统一 CTest 注册和测试运行时部署
- `benchmarks`：手动大文件性能测试及其 CMake 目标
- `cmake`：Scintilla 集成、版本/启动器模板和发布打包配置
- `resources`：翻译、图标及 Windows 应用/安装程序资源
- `scripts`：可重复执行的打包、部署和许可证生成入口
- `docs`：当前文档；`docs/design` 保存归档的原始任务书
- `third_party`：固定版本的上游源码
- `licenses`：上游声明、SPDX 清单和许可证全文

`build/` 和 `.build-tools/` 是本地生成目录，不提交到 Git。测试报告、预览程序和其他临时输出统一放在 `build/` 下；整个仓库忽略 Python 字节码缓存。
配置时使用 `-DVINSON_BUILD_BENCHMARKS=OFF` 可省略手动性能基准目标，使用 `-DVINSON_BUILD_TESTS=OFF` 可省略自动化测试。

## 自动化测试

`file_core_tests` 覆盖 ASCII 和 Unicode 编码、BOM 保留、LF/CRLF/CR 检测、跨分块换行检测、混合换行符、空文件、Unicode 路径、无效 UTF-8、文件缺失、取消原子保存、文档元数据和多分块异步 `FileManager` 加载；还覆盖 64/512 MiB 策略边界，以及按范围流式跨线程 UTF-16 保存。

`document_history_tests` 覆盖最近文件和关闭标签历史的路径规范化、去重、容量限制及恢复顺序。`file_change_monitor_tests` 使用真实临时文件验证外部修改、删除、重建以及应用保存期间暂停监视的行为；主窗口测试进一步覆盖无冲突自动重载和保留本地编辑的冲突处理。

`recovery_manager_tests` 覆盖后台原子快照的写入、读取、删除、批量清理、无效标识拒绝和 8 MiB 容量限制。`editor_widget_tests` 验证恢复后的文本在内容不变的情况下正确标记为未保存。

`editor_widget_tests` 使用 Qt offscreen 平台插件，覆盖编辑、视图选项、大文档创建、范围读取、跨搜索分片边界匹配，以及 `Ctrl+鼠标滚轮` 字号调整请求。

书签测试覆盖添加／移除／清除、边界循环跳转、空文档、随行编辑调整、普通及大文件模式中的文档隔离、真实书签栏点击，以及隐藏标记时保持正文无额外装饰。主窗口测试验证默认和自定义快捷键、标签切换、重新加载清理、极简模式导航和搜索期间暂停书签操作。

`search_controller_tests` 覆盖向前/向后遍历、循环查找开关、大小写和全字匹配、Unicode 文本、替换与撤销、转到行、空查询及非模态查找控件交互；还覆盖大文件异步跨分片匹配、循环查找、取消与重新搜索、正文或文档切换后的搜索失效，以及查找面板取消按钮。主窗口测试验证面板和状态栏按钮、普通及极简模式下的 `Esc` 取消、编辑状态恢复，以及搜索期间外部打开请求的排队。

`appearance_tests` 验证实时字体/透明度预览、在不修改文档的情况下应用样式、半透明编辑区文字像素和透明度为零的背景。

`window_controller_tests` 验证可组合窗口标志、几何保留、无边框单行最小高度、移动/缩放光标，以及窗口事件过滤器启用时真实的 Scintilla 文本选择；还验证可逆的极简模式 UI 状态、从字体推导的单行尺寸、此前无边框状态恢复，以及在保留极简模式内编辑内容的同时安全进入和退出。

`tray_controller_tests` 和 `global_shortcut_tests` 覆盖托盘菜单、窗口显隐、聚焦恢复、快捷键校验，以及老板键和聚焦快捷键的同时原生注册；原生全局快捷键注册仅在受支持的 Windows 环境中验证。

`settings_manager_tests` 覆盖设置文件缺失时的默认值、完整持久化往返，以及字体、字号、颜色、布尔值、窗口几何和目录数据损坏时的回退行为。安装 Qt Linguist Tools 后，`localization_tests` 还会验证简体中文资源和英文回退。

`vinson-large-file-benchmark` 是手动运行的 Release 性能工具。它默认参与构建，但不会注册为 CTest，因为完整测试文件集约占 1.6 GiB。复现命令和最新测量结果见 [PERFORMANCE.zh-CN.md](PERFORMANCE.zh-CN.md)。
