# 性能

简体中文 | [English](PERFORMANCE.md)

## 大文件设计

明确执行换行转换时使用 Scintilla 的同步可撤销操作，超大文档可能短暂阻塞界面。编码转换仍由已有保存工作线程处理，大文档继续按范围流式保存。换行状态基于文本修改通知增量维护各文档的 CR、LF 和 CRLF 数量，并处理修改边界，不复制或重新扫描整个文档。

除 `EditorWidget` 外，应用代码不会访问原始 Scintilla 消息；文件工作线程也绝不直接操作 GUI 控件。

文件按照统一的 64 MiB 和 512 MiB 阈值分类。开始加载前，大文件和超大文件会获得一个新建的 Scintilla 文档，并启用 `SC_DOCUMENTOPTION_TEXT_LARGE | SC_DOCUMENTOPTION_STYLES_NONE`。文档连接后释放创建者引用，选择空 lexer，并默认关闭自动换行。

源文件大小加 1 MiB 编辑预留会传给 `SCI_CREATEDOCUMENT`。已知容量可避免加载期间反复扩展完整缓冲区；额外预留则避免首次小修改触发 Scintilla 更大幅度的几何扩容。

加载器在工作线程中以 256 KiB 读取源文件。最多允许四个已解码分块等待处理，把排队数据限制在约 1 MiB。有效的 ASCII/UTF-8 数据会被增量校验并直接传递，无需经过 UTF-16 `QString` 往返；UTF-16 输入仍使用有状态的逐块转换。GUI 只有在分块追加到 Scintilla 后才进行确认，加载过程可以取消。

换行符检测与加载共用同一条流式数据路径，并能识别横跨两个分块的 CRLF，不需要额外的全文扫描。

大文件模式查找由单次定时器逐个调度相互重叠的 8 MiB Scintilla 目标范围。重叠区域可以保留跨分片边界的匹配，每次扫描后返回正常事件循环；不会复制完整文档或建立全文索引。搜索进度及取消入口保持可用，编辑、替换和标签切换会暂停到搜索结束。全部替换仍为同步操作，超大文件模式需要用户确认。基准工具继续使用同步分片辅助接口，下方历史测量结果未重新测量定时器调度开销。

普通保存会创建一个完整 UTF-8 快照。大文件模式则由工作线程每次请求一个 256 KiB Scintilla 范围，执行流式 UTF-8/UTF-16 转换，并通过 `QSaveFile` 提交。操作期间编辑器会被禁用，以保持偏移量稳定。取消或失败会丢弃临时输出，不会截断源文件。

## 复现基准测试

使用 Release 构建。生成器会创建精确大小的 ASCII/LF 文件，并在靠近末尾处放置唯一搜索标记：

```bash
cmake --preset release
cmake --build --preset release --target vinson-large-file-benchmark
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --generate /tmp/vinson-editor-phase8
```

每个大小都应在新进程中运行，避免前一个文档的分配器状态影响 RSS：

```bash
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --file /tmp/vinson-editor-phase8/benchmark-10MiB.txt
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --file /tmp/vinson-editor-phase8/benchmark-100MiB.txt
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --file /tmp/vinson-editor-phase8/benchmark-500MiB.txt
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --file /tmp/vinson-editor-phase8/benchmark-1024MiB.txt
```

工具输出一个 JSON 对象。它测量异步打开、Linux RSS 峰值、事件循环定时器间隔、搜索靠近文件末尾的标记、100 次等距行跳转、中部插入十字节并撤销，以及原子保存。删除临时副本前会校验已保存文件的大小。

## 测量结果 — 2026-08-28

环境：Release（`-O3`）、x86-64 Linux 7.0.0-29、Qt 6.10.2、GCC 15.2.0、Intel Core i7-10870H（8 核/16 线程）、30 GiB 内存。测试文件和保存目标位于 16 GiB `/tmp` tmpfs 上。因此保存数据描述的是编辑器流水线和内存支持的 I/O，不代表物理 SSD 或 Windows 文件系统速度。

| 文件 | 模式 | 打开 | 加载最大事件间隔 | 近末尾搜索 | 搜索最大事件间隔 | 100 次行跳转总计 / 最大值 | 中部编辑 | 保存 | 保存最大事件间隔 |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 MiB | 普通 | 0.197 s | 65 ms | 0.086 s | 86 ms | 1.332 s / 16 ms | 2 ms | 0.039 s | 28 ms |
| 100 MiB | 大文件 | 1.348 s | 158 ms | 0.615 s | 64 ms | 1.103 s / 12 ms | 11 ms | 0.273 s | 12 ms |
| 500 MiB | 大文件 | 6.942 s | 747 ms | 2.994 s | 61 ms | 1.086 s / 12 ms | 59 ms | 1.416 s | 14 ms |
| 1 GiB | 超大文件 | 14.087 s | 1,517 ms | 6.009 s | 69 ms | 1.086 s / 11 ms | 126 ms | 2.703 s | 15 ms |

在四次加载期间，10 ms 响应性定时器分别触发 6、65、331 和 680 次。因此事件循环持续取得进展，而不是在整个操作期间完全停顿。观测到的最大暂停是加载 1 GiB 文件时的 1.517 秒，主要发生在 Scintilla 分配初始连续间隙缓冲区时。1 GiB 搜索仍需数秒，但分片把最长事件间隔降到 69 ms，同时继续使用 Scintilla 直接搜索。

Offscreen 行跳转测量是可重复的导航/重绘代理，并不代表显示器帧率。实际滚动流畅度仍取决于窗口系统、GPU、字体、透明度和显示器刷新率。

### 常驻内存

| 文件 | 基线 RSS | 加载时 RSS 峰值 | 峰值增长 | 编辑/撤销后的 RSS | 保存时峰值增长 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 10 MiB | 30.9 MiB | 59.7 MiB | 28.8 MiB | 59.7 MiB | 10.0 MiB |
| 100 MiB | 31.1 MiB | 149.9 MiB | 118.8 MiB | 149.5 MiB | < 0.1 MiB |
| 500 MiB | 31.1 MiB | 601.8 MiB | 570.8 MiB | 601.5 MiB | < 0.1 MiB |
| 1 GiB | 31.1 MiB | 1,190.5 MiB | 1,159.4 MiB | 1,190.1 MiB | < 0.1 MiB |

加载 1 GiB 文件时，RSS 增长约为文件大小的 1.13 倍，而非 5–10 倍。其余开销主要来自 Scintilla 文本容量和行位置元数据；不存在样式字符存储。1 MiB 预留消除了此前首次编辑时的扩容（500 MiB 文件约增加 128 MiB，1 GiB 文件约增加 256 MiB），编辑后的 RSS 没有可测增长。大文件流式保存同样没有明显增加 RSS。普通路径按设计会在保存 10 MiB 测试文件时额外创建一个 10 MiB 快照。

## 已知限制

- 行书签按需启用 Scintilla 原生标记结构；首次添加标记时，内核会为文档中的每行分配一个标记槽，因此行数非常多的文档会增加行元数据内存。从未添加过书签的文档不分配此结构；上方历史基准数据未包含书签开销。
- 在此机器上，创建 Scintilla 初始 1 GiB 间隙缓冲区仍会产生约 1.5 秒停顿。要把文档创建完全移出 GUI 线程，需要采用 Scintilla 后台加载器生命周期；在更多平台测量证明复杂度合理之前暂不实施。
- 搜索取消在两个分片之间生效，正在执行的单个 Scintilla 范围扫描无法中断；取消延迟受单个分片的扫描时间影响。全部替换仍为同步操作，尚不支持取消。
- 中部插入耗时仍与 Scintilla 间隙缓冲区需要移动的文本量成正比；预留编辑容量后，测得 1 GiB 文件插入耗时为 126 ms。
- UTF-16 加载会执行有界的 UTF-16 到 UTF-8 转换，其内存和时间特征可能与此处测量的 ASCII 文件不同。
- 透明背景使用直接绘制，成本可能高于不透明背景。Offscreen 基准采用默认不透明外观。
- 项目不宣称支持无限文件大小或 1 GiB 操作零延迟。Scintilla 仍是完整文档编辑器；按照当前 MVP 测得的内存比例，尚无充分理由引入分页或内存映射方案。
