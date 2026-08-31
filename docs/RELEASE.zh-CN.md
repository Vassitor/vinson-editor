# 发布打包

简体中文 | [English](RELEASE.md)

## Windows 便携包

受支持的发布目标是 64 位 Windows、MSVC 2022 或更新版本，以及与之匹配的 64 位 Qt 6.5+。在能够通过 `CMAKE_PREFIX_PATH` 找到 Qt 的 Developer PowerShell 中运行：

```powershell
.\scripts\package-windows.ps1
```

脚本会配置并构建 Release、运行全部测试并调用 CPack。在 CPack 安装阶段，Qt CMake 部署 API 会运行 `windeployqt`，把所需 Qt DLL、编译器运行库、`qt.conf` 和平台插件放到应用旁边。生成的 ZIP 和 `.sha256` 文件位于 `build/release/`。

Scintilla 会静态编译进 `vinson-editor.exe`。Lexilla 已为未来语法支持固定版本，但当前纯文本版本不会链接它，因此两者都不需要单独的运行时 DLL。

发布归档同时包含英文和简体中文的 README、变更日志及 `docs/` 文档。只有安装 Qt Linguist Tools 时才会编译应用内置翻译；正式发布环境应安装该组件，以确保简体中文界面和本地化测试包含在发布门禁中。

## Linux 可移植包

使用以下等价 Release 流水线：

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --build --preset release-package
```

TGZ 包含可执行文件、根目录下的 `vinson-editor` 启动器、Qt 库、检测到的非系统运行时依赖、平台插件、中英文文档和 SHA-256 校验和。请通过启动器运行归档中的程序；启动器会提供动态加载插件所需的相对库路径。目标机器必须具备兼容的 glibc 及图形/会话库。AppImage 仍是后续发布目标。

## 纯净环境验收

请在 `PATH` 中没有 Qt SDK 的 64 位 Windows 机器或虚拟机上验证 ZIP：

1. 校验 SHA-256 文件，再把 ZIP 解压到新目录。
2. 无需安装任何内容，直接启动 `vinson-editor.exe`。
3. 使用包含 Unicode 字符的路径创建、保存、重新打开、重新加载并拖放文本文件。
4. 验证查找/替换、循环查找开关、`Ctrl+鼠标滚轮` 字号调整、外观设置、窗口置顶、无边框和极简模式。
5. 关闭并重新打开应用，确认设置和窗口几何能够持久化。
6. 确认 `platforms/qwindows.dll` 和所需 Qt DLL 仍在解压目录中，且 `PATH` 未加入任何 Qt SDK 目录。

Windows 纯净环境执行结果是正式发布门禁。Linux 包的冒烟测试只用于可移植性检查，不能替代这一门禁。
