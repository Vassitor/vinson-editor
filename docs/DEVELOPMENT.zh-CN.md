# 开发指南

简体中文 | [English](DEVELOPMENT.md)

## 支持的工具链

主要发布目标是使用 MSVC 2022 或更新版本的 64 位 Windows。Linux 用于持续开发和可移植性检查。CMake 项目会拒绝 32 位配置。

最低开发要求为 CMake 3.25、Ninja、C++20 编译器，以及包含 Core、Gui、Widgets、Test 和 Core5Compat 的 Qt 6.5。Qt Linguist Tools 是可选依赖；安装后会编译内置翻译并启用本地化测试。第 0 阶段初始化环境为 Ubuntu 26.04 x86_64、GCC 15.2.0；Ubuntu 当前软件包提供 CMake 4.2.3 和 Qt 6.10.2。

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

`third_party/scintilla` 是 Scintilla 5.6.6 官方发布源码，`third_party/lexilla` 是 Lexilla 5.5.3 官方发布源码。请保持这些目录不含本地修改，使上游升级易于审查。项目专用构建逻辑应放在 `cmake/` 中。RGBA 背景适配会生成在 `build/<preset>/generated/scintilla` 下，绝不能直接修改供应商源码。

升级任一依赖时：

1. 从 `scintilla.org` 下载稳定版本。
2. 校验发布版本并保留上游许可证。
3. 用无本地修改的新源码替换对应目录。
4. 配置并构建 Debug 和 Release 预设。
5. 运行完整测试套件并更新架构及性能说明。

## 仓库布局

- `src/app`：应用生命周期和启动
- `src/window`：顶层窗口和窗口模式控制
- `src/editor`：面向 Scintilla 的编辑器抽象
- `src/file`：编码/换行符检测、有界异步加载和原子保存
- `src/largefile`：统一阈值和大文档行为
- `src/search`：搜索/替换编排
- `src/settings`：外观主题和经过校验的 `QSettings` 持久化
- `src/ui`：职责集中的对话框和可复用 UI 组件
- `tests`：Qt Test 和 CTest 目标
- `scripts`：可重复执行的发布打包入口

## 自动化测试

`file_core_tests` 覆盖 ASCII 和 Unicode 编码、BOM 保留、LF/CRLF/CR 检测、跨分块换行检测、混合换行符、空文件、Unicode 路径、无效 UTF-8、文件缺失、取消原子保存、文档元数据和多分块异步 `FileManager` 加载；还覆盖 64/512 MiB 策略边界，以及按范围流式跨线程 UTF-16 保存。

`editor_widget_tests` 使用 Qt offscreen 平台插件，覆盖编辑、视图选项、大文档创建、范围读取、跨搜索分片边界匹配，以及 `Ctrl+鼠标滚轮` 字号调整请求。

`search_controller_tests` 覆盖向前/向后遍历、循环查找开关、大小写和全字匹配、Unicode 文本、替换与撤销、转到行、空查询及非模态查找控件交互。

`appearance_tests` 验证实时字体/透明度预览、在不修改文档的情况下应用样式，以及透明度为零的背景像素与不透明前景像素能够同时正确渲染。

`window_controller_tests` 验证可组合窗口标志、几何保留、无边框移动/缩放光标，以及窗口事件过滤器启用时真实的 Scintilla 文本选择；还验证可逆的极简模式 UI 状态、从字体推导的单行尺寸、此前无边框状态恢复，以及在保留极简模式内编辑内容的同时安全进入和退出。

`tray_controller_tests` 和 `global_shortcut_tests` 覆盖托盘菜单、窗口显隐和老板键校验；原生全局快捷键注册仅在受支持的 Windows 环境中验证。

`settings_manager_tests` 覆盖设置文件缺失时的默认值、完整持久化往返，以及字体、字号、颜色、布尔值、窗口几何和目录数据损坏时的回退行为。安装 Qt Linguist Tools 后，`localization_tests` 还会验证简体中文资源和英文回退。

`vinson-large-file-benchmark` 是手动运行的 Release 性能工具。它默认参与构建，但不会注册为 CTest，因为完整测试文件集约占 1.6 GiB。复现命令和最新测量结果见 [PERFORMANCE.zh-CN.md](PERFORMANCE.zh-CN.md)。
