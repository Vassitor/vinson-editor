# 第三方软件声明 / Third-party notices

Vinson Editor 的项目代码采用 [MIT License](LICENSE)。第三方组件保留各自的版权及许可条件，本项目的 MIT 许可证不替代这些条件。

## 直接依赖

| 组件 | 版本 | 用途 | 许可证及声明 |
| --- | --- | --- | --- |
| Scintilla | 5.6.6 | 静态编译的文本编辑控件 | [上游许可证全文](licenses/Scintilla.txt) |
| Lexilla | 5.5.3 | 仓库附带的词法分析器源码；当前 CMake 未链接 | [上游许可证全文](licenses/Lexilla.txt) |
| Qt | 本次清单为 6.8.3 | 动态链接 Core、Gui、Widgets、Core5Compat；部署工具还可能携带 Network、Svg 及平台、样式、图像插件、翻译 | [组件版权清单](licenses/qt/NOTICES.md)、[LGPL v3](licenses/texts/LGPL-3.0-only.txt)、[GPL v3](licenses/texts/GPL-3.0-only.txt) |

Scintilla 和 Lexilla 的来源版本及归档校验值见 [third_party/README.md](third_party/README.md)（源码仓库中）。

Qt 的不同文件与内含组件可能采用不同许可条件。完整的上游许可表达式、版权声明及组件来源保存在 `licenses/qt/` 的 SPDX JSON 中；许可证全文在 `licenses/texts/` 中。清单保留上游的许可备选项，不表示项目已获得 Qt 商业许可。

## Qt 清单的范围

本次从 Qt 6.8.3 Windows MSVC x64 SDK 的 `qtbase`、`qt5compat`、`qtsvg`、`qttranslations` SBOM 生成声明。为保留模块内的上游声明，包含了这些模块的完整清单，因此也包含未随本应用发布的平台代码、构建工具和测试组件。该清单不是应用二进制的精确依赖图。

Qt SDK 的原始 SPDX 文档和自定义许可证全文均保留原文。标准许可证文本来自固定版本的 SPDX License List；来源及 SHA-256 见 [licenses/SOURCES.md](licenses/SOURCES.md)。`NOASSERTION` 表示上游未作断言，不代表无版权或无许可要求。

Qt 官方资料：[许可说明](https://doc.qt.io/qt-6.8/licensing.html)、[Qt 内含第三方代码](https://doc.qt.io/qt-6.8/licenses-used-in-qt.html)、[对应版本源码归档](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/)。

## 其他 Windows 运行组件

部署工具可能额外复制 `d3dcompiler_47.dll`、`dxcompiler.dll`、`dxil.dll`、`opengl32sw.dll` 或 Microsoft Visual C++ Runtime。这些组件不应被视为适用 Qt 的 LGPL 或本项目的 MIT 许可证。

当前构建目录中的这些独立运行库没有随附完整的来源及许可资料，Qt 模块 SBOM 也不足以确定其所有许可条件。因此本清单的已核验范围是上述源码依赖和 Qt SDK 模块；发布其他运行库时仍需依据实际供应包补充相应声明。更换 Qt 版本、平台或部署组件时需重新生成并核对清单。

## 重新生成

```powershell
./scripts/generate-third-party-licenses.ps1 -QtRoot "path/to/Qt/6.8.3/msvc2022_64" -QtVersion "6.8.3"
```

脚本读取本地 SDK 的原始 SBOM，并在缺少标准许可证文本时从 SPDX 官方仓库下载。生成结果提交到仓库后，普通构建及打包不需要联网。安装和 CPack 发布包包含本文件以及整个 `licenses/` 目录。

---

Vinson Editor is MIT licensed. Third-party software retains its own copyrights and license terms. The bundled notices cover Scintilla 5.6.6, vendored but currently unlinked Lexilla 5.5.3, and the listed Qt 6.8.3 SDK module inventories. These inventories are a conservative module-level superset, not a binary dependency audit. Separate Windows graphics and compiler runtimes require notices from their actual distribution packages. Original copyright notices, SPDX metadata, license texts and provenance are provided in `licenses/`.
