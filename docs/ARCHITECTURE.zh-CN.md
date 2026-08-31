# 架构

简体中文 | [English](ARCHITECTURE.md)

## 当前结构

```text
main
  -> Application（进程生命周期和应用元数据）
      -> MainWindow（桌面窗口组合和操作入口）
          -> WindowController（窗口标志及原生移动/缩放请求）
          -> FindReplaceWidget（非模态搜索控件）
              -> SearchController（搜索/替换编排）
          -> SettingsDialog（实时外观控件）
              -> ThemeManager（经过校验的外观状态）
          -> EditorWidget（稳定的编辑器接口）
              -> ScintillaEdit（上游 Qt 适配层）
                  -> Scintilla 文档引擎
          -> EditorDocument（路径、编码、换行符、大小和修改状态）
          -> LargeFilePolicy（统一阈值和功能降级规则）
          -> SettingsManager（经过校验的持久化应用状态）
          -> FileManager（异步操作协调器）
              -> QThread + FileLoader
              -> QThread + FileSaver + QSaveFile
```

`MainWindow` 负责界面呈现和操作连接，但不直接发送原始 Scintilla 消息。`EditorWidget` 是上游控件的边界，负责编码模式、样式、视图选项、文本访问，以及将通知转换为应用级 Qt 信号。

`SearchController` 将用户级搜索状态转换为 `EditorWidget` 暴露的窄接口。搜索直接使用 Scintilla 目标范围，包括向前/向后遍历和可选循环查找，因此控制器和查找替换控件都不需要复制完整文档。全部替换会合并为一个 Scintilla 撤销操作。`FindReplaceWidget` 保持非模态，并在关闭时将焦点归还编辑器。

`ThemeManager` 持有当前内存中的外观状态，限制字号范围，保持光标和选中文字颜色不透明，并通过 `EditorWidget` 分别应用背景与编辑区文字的 alpha，而不触碰文档。窗口控件使用文字颜色的不透明副本以保持清晰。`SettingsDialog` 会立即预览每项设置；取消时恢复打开对话框之前的外观。

`TrayController` 持有两个相互独立的 `GlobalShortcut` 注册。老板键切换窗口显隐；聚焦快捷键始终显示、还原、置前并激活窗口，然后请求将键盘焦点交给 `EditorWidget`。

`SettingsManager` 是唯一的 `QSettings` 边界。返回应用状态前，它会校验字体系列、字号、RGBA 颜色、布尔值、窗口几何数据大小和最近目录。`MainWindow` 先恢复窗口标志，再恢复几何，并确认窗口有足够区域与可用屏幕相交。无效或位于屏幕外的几何会回退到主显示器。设置中绝不保存文档文本或文件内容。

`WindowController` 独占无边框和窗口置顶状态转换。Qt 重新创建原生窗口时，它会保留几何、窗口状态、焦点和组合标志。在无边框模式下，应用事件过滤器只消费被 `QWindow::startSystemMove()` 接受的 `Alt+鼠标左键` 操作，或被 `QWindow::startSystemResize()` 接受的六像素边缘操作。顶部边缘可以直接拖动窗口，角落和其他边缘用于缩放。普通编辑器鼠标事件会原样通过，因此不会破坏文本选择。系统边框消失后，F11 仍绑定在主窗口上。

极简模式是 `WindowController` 的另一种可逆状态。进入时，控制器保存此前的边框、菜单栏/状态栏/查找面板可见性、行号、滚动条、最小尺寸和普通窗口几何；随后隐藏窗口装饰、启用无边框模式，并按当前编辑器字体计算单行最小高度。退出时恢复快照，包括进入前已启用的无边框状态。字体改变会通过已有外观信号刷新最小高度。启动时不会自动恢复极简模式；在极简模式中持久化时，保存的是捕获到的普通窗口几何和此前的无边框偏好。

`EditorDocument` 独立于控件保存文档元数据。`FileManager` 同一时间只持有一个操作，并管理短生命周期工作线程。`FileLoader` 以 256 KiB 分块打开和解码文件。四个许可量的信号量将排队数据限制在约 1 MiB；GUI 只有在把分块追加到 Scintilla 后才确认消费，从而避免高速磁盘把整个大文件排进内存。ASCII 和 UTF-8 使用经过校验的字节保留路径；只有 UTF-16 输入会创建有界的中间 `QString` 分块。

`LineEndingDetector` 统一普通文本和流式加载的换行符统计，可正确识别跨分块边界的 CRLF，而无需扫描第二遍全文。

`LargeFilePolicy` 将小于 64 MiB 的源文件归为普通模式、64 MiB 及以上归为大文件模式、512 MiB 及以上归为超大文件模式。`MainWindow` 在追加首个解码分块前应用策略。`EditorWidget` 创建并连接带有 `TEXT_LARGE | STYLES_NONE` 的新 Scintilla 文档，在 `SETDOCPOINTER` 后平衡创建者引用，并选择空 lexer。两种大文件模式都默认关闭自动换行；超大文件执行全部替换前需要确认。源文件大小加上少量编辑预留用于预分配文档容量。状态栏始终显示当前模式。大文件搜索使用相互重叠的目标范围分片，让绘制和定时器事件在长时间扫描过程中继续运行。

`FileSaver` 对普通文档使用稳定的 UTF-8 快照。大文档则每次从 `EditorWidget` 请求一个 256 KiB 范围；编码转换和 `QSaveFile` I/O 保持在工作线程中，同时禁用编辑器，确保声明的范围不会在保存中途改变。取消或失败会丢弃临时文件，而不是截断源文件。ASCII 文档在需要时无损升级为 UTF-8。

Scintilla 官方 5.6.6 源码会编译为私有静态目标 `Scintilla::Scintilla`。项目使用上游 `ScintillaEdit` 层，因为它在 `ScintillaEditBase` 之上提供生成的类型化 API。供应商源码树保持不变：CMake 在构建目录中复制 `Editor.cxx`，并加入受保护的 RGBA 样式前景和背景适配器。这是必要的，因为公开的样式颜色消息会先把颜色规范化为不透明 RGB，Qt 渲染器因而无法获得透明度。如果固定版本的源码不再匹配预期适配点，配置会直接失败。

Qt 6 构建链接 Core5Compat，因为当前上游 Qt 适配层仍使用 `QTextCodec` 处理旧代码页。构建把上游 `EXPORT_IMPORT_API` 标注定义为空，因为静态库不应暴露 Windows DLL 导入/导出声明。

应用目标使用 C++20。未修改的 Scintilla 目标使用 C++17 编译，与其上游要求一致，也避免其中的 C++17 构造产生 C++20 弃用诊断。

Lexilla 5.5.3 与 Scintilla 一起固定在仓库中，但尚未链接。纯文本无需 lexer；延后引入 Lexilla 可以避免在大文件策略完善前启用高成本样式处理。

依赖方向指向与应用界面无关的模型和服务。只有 GUI 线程中的 `EditorWidget` 可以修改 Scintilla 控件。

## 线程规则

Qt 控件和 Scintilla 始终只在 GUI 线程中运行。文件读取、解码、编码和原子写入在工作线程中执行。跨线程边界只使用排队的 Qt 信号；取消和分块确认使用线程安全的原子量/信号量，因为工作对象忙于执行当前操作时无法处理排队槽。
