# 插件使用与开发

简体中文 | [English](PLUGINS.md)

## 安装与管理

打开 **插件 → 管理插件**，点击“导入插件”，选择 `.vinson-plugin` 文件。
导入成功后立即启用，命令出现在“插件 → 插件名称”下，无需重启或编译。
管理窗口支持启用、停用、卸载和重新加载，并保存启用状态供下次启动恢复。
卸载只删除编辑器管理的副本，保留原始导入文件和专属配置。
导入同一 ID 的插件时可确认替换旧版本；更新采用原子写入，并保留启用状态、设置和快捷键。
新版本不再接受的设置值会回退到其默认值，无效插件包不会覆盖旧版本。

点击“安装示例”可安装内置“文本工具”，包含选区转大写、格式化 JSON 文档、
统计字数、时间插入、行排序、行去重、清理行尾空格、包裹选区、编号列表、
复制 JSON 字符串和生成文本报告，共 11 个命令。
示例源码见 [text-tools.vinson-plugin](../examples/plugins/text-tools.vinson-plugin)。
已安装早期示例时，再次点击“安装示例”即可确认更新。
编辑已安装文件后点击“重新加载”即可刷新命令；无效包显示错误，并可卸载。
手工复制到插件目录的包默认停用，检查后可在管理窗口启用。

“打开插件文件夹”显示当前用户的安装目录。目录位于 Qt 的应用数据位置：
Windows 通常为 `%APPDATA%/VinsonEditor/Vinson Editor/plugins`；Linux 通常为
`~/.local/share/VinsonEditor/Vinson Editor/plugins`。目录中 `enabled.json` 保存启用列表，
`preferences.json` 保存各插件的设置和命令快捷键。配置文件损坏时使用默认值运行，保存前须修复文件，避免覆盖旧数据。
内置示例必须手动安装，启动和导入时不会运行插件命令。

## 开发文档预览

管理窗口的“开发文档”标签页可离线预览这份完整指南，支持简体中文和英文切换。
点击示例源码链接可直接查看内置插件的 JSON，不会安装或执行插件。

## 安装安全检测

导入和更新都会在写入前进行静态代码检查，不会执行命令。检测动态求值（`eval`、
`Function`、`constructor`）、系统或模块访问、网络和文件接口引用、编码内容、转义标识符，
以及 `while (true)`、`while (1)`、`for (;;)` 等可能无限运行的循环。
检查会忽略普通注释、字符串文本及常见正则正文，并检查模板插值和字符串形式的属性访问。

发现可疑模式时，确认窗口列出命令名称、ID、脚本行号（从 1 开始）、代码片段和具体风险。
默认选择“取消”，按 Esc 或关闭窗口均不安装；用户明确点击“仍然安装”才继续。
更新被取消时原有包、启用状态与配置保留。确认绑定包的 SHA-256；包发生变化必须重新检查。
没有匹配到可疑模式的有效包按通常流程导入，不显示安全认证之类的保证。

这是启发式检查，可能误报或漏报，不能识别所有间接调用、混淆、递归、复杂正则或内存耗尽。
每个命令最多显示前 64 条不同类别/行号的发现。编码、构造器和循环本身不一定有害，需结合上下文判断。
当前运行时没有文件、网络或系统接口，对这些接口的引用表示潜在访问意图，并不表示当前环境允许其执行。
脚本仍可读取和修改所提供的文本；进程内引擎不提供严格的内存隔离。只安装可信来源的插件。
此安装检查不监控安装目录的手工复制或后续编辑；修改包后应重新导入以检查新代码。

## 插件格式（API v1）


插件是 UTF-8 JSON 单文件，最大 1 MiB，扩展名为 `.vinson-plugin`。例如：

```json
{
  "apiVersion": 1,
  "id": "my.text-tools",
  "name": "我的文本工具",
  "version": "1.0.0",
  "description": "将选区转为大写",
  "commands": [
    {
      "id": "uppercase",
      "title": "选区转大写",
      "input": "selection",
      "output": "replaceSelection",
      "script": "return context.text.toUpperCase();"
    }
  ]
}
```

`id` 必须以小写英文字母开头，只能包含小写字母、数字、点和连字符，最长 80 字符。
插件 ID 全局唯一，命令 ID 在插件内唯一。`name` 和 `title` 为非空字符串，最长
120 字符；`version` 为非空字符串，最长 40 字符；可选 `description` 最长 2000 字符。
每个插件包含 1–32 个命令。API 版本必须为数字 `1`；不支持的版本和语法错误会在导入时拒绝。

`script` 为 JavaScript 函数体，使用严格模式，接收 `context` 参数，必须同步返回字符串。
普通 JavaScript 内置功能（如 `JSON`、`Date`、正则表达式）可用。
不支持异步命令、Node.js 模块、浏览器 DOM、网络请求、Qt 对象或文件访问。

| `input` | `context.text` |
| --- | --- |
| `selection` | 当前选区；无选区时提示先选择文本 |
| `document` | 当前标签页完整文本 |
| `none` | 空字符串，用于生成文本等操作 |
| `line` | 光标所在行的文本，不含换行符 |
| `selectionOrDocument` | 有选区时读取选区，否则读取整篇文档 |

其他上下文字段：`fileName` 为当前文档完整路径（未命名文档为空），`line` 为从 1 开始的
当前行号，`lineEnding` 为编辑器当前实际插入换行符（`\n`、`\r\n` 或 `\r`）。
`column` 为从 1 开始的 Scintilla 列号，`lineCount` 为文档行数，`position` 为光标字节位置，
`selectionStart`、`selectionEnd` 为选区的字节边界，`documentLength` 为 UTF-8 文档字节数。
`inputScope` 为实际读取范围：`selection`、`document`、`line` 或 `none`。
字节位置不能直接作为 JavaScript 字符串索引使用。

| `output` | 返回字符串的用途 |
| --- | --- |
| `replaceSelection` | 替换执行前选区，必须有选区 |
| `replaceDocument` | 替换当前文档内容 |
| `insert` | 在执行前光标位置插入，不删除已有选区内容 |
| `message` | 以纯文本结果窗口展示，最多显示前 16000 个 UTF-16 单元 |
| `replaceInput` | 替换刚刚读取的范围，适合 `line` 或 `selectionOrDocument`；不能与 `none` 搭配 |
| `clipboard` | 将结果写入剪贴板，保留当前文档内容和修改状态 |
| `newDocument` | 将结果放入新的未命名标签页，原标签保持不变；非空结果标记为未保存，可撤销或保存 |

脚本抛出异常、返回非字符串、取消或超时时，文档保持原状。
每次文本修改作为一个撤销操作，沿用现有修改标记、保存和恢复机制。
若执行期间文档发生程序性变更或切换，丢弃结果，避免覆盖新的编辑。

## 插件设置、快捷键和执行参数

在管理窗口选择插件，点击 **插件设置**。设置页根据插件声明自动生成复选框、
整数输入、文本输入或下拉框；命令快捷键页可修改或清空每个命令的组合键。
点击“确定”保存并立即生效，取消不改动配置，“恢复默认值”在确认后恢复声明的默认值。
快捷键在菜单隐藏时仍可使用；每个命令只接受一个组合键，`Esc` 保留用于取消。
保存时检查编辑器命令、全局快捷键、样式和其他已启用插件的冲突。
导入包默认快捷键或后续编辑器绑定发生冲突时，保留菜单命令并暂停冲突快捷键；
可在插件设置中重新分配。多个插件冲突时，按插件文件名排序的首个命令优先。

插件顶层可声明可选 `settings` 数组，每个命令可声明可选 `parameters` 数组。
二者使用相同的字段格式，最多各 32 个字段；字段 ID 使用插件 ID 的相同命名规则，
在各自数组内唯一。每个字段必须包含 `id`、`label`、`type` 和类型正确的 `default`。

```json
"settings": [
  {"id": "indent", "label": "JSON 缩进", "type": "integer", "default": 2, "minimum": 1, "maximum": 8},
  {"id": "enabled", "label": "区分大小写", "type": "boolean", "default": true},
  {"id": "prefix", "label": "前缀", "type": "string", "default": ">", "maxLength": 100},
  {"id": "order", "label": "排序方向", "type": "choice", "default": "asc", "choices": ["asc", "desc"]}
]
```

`integer` 的可选 `minimum`／`maximum` 默认为 -1000000／1000000，均须在此范围内。
`string` 的可选 `maxLength` 默认为 4096，可设置为 1–16384；长度按 UTF-16 单元计算。
`choice` 必须声明 1–64 个唯一、非空的字符串选项，每个选项最长 120 字符，默认值须属于选项。
字段标签最长 120 字符，设置值会在导入、保存和执行前校验。

脚本通过 `context.settings.indent` 等读取插件设置。插件无法直接修改已保存配置；
每次运行接收独立快照。声明了 `parameters` 的命令会在执行前显示输入框，
脚本通过 `context.parameters` 读取本次输入，取消输入不会执行脚本或修改文档。
参数不跨运行保存；省略参数值时使用声明的默认值。例如包裹选区：

```json
{
  "id": "wrap",
  "title": "包裹选区",
  "shortcut": "Ctrl+Alt+Shift+W",
  "input": "selection",
  "output": "replaceSelection",
  "parameters": [
    {"id": "before", "label": "前缀", "type": "string", "default": "**"},
    {"id": "after", "label": "后缀", "type": "string", "default": "**"}
  ],
  "script": "return context.parameters.before + context.text + context.parameters.after;"
}
```

`shortcut` 是可选的 Qt PortableText 字符串，未声明或为空时不设置默认快捷键。
原有 API v1 插件无需添加这些字段，仍可直接使用。

## 执行边界

命令在后台线程的独立 JavaScript 引擎中运行，执行期间暂时锁定文档编辑和标签切换。
点击状态栏“取消”或按 `Esc` 可中断，默认 3 秒超时。输入和输出 UTF-8 文本均限 8 MiB；
整文档替换同样限制为 8 MiB。大文件可对较小的选区执行命令。
引擎不提供文件、网络或编辑器对象接口，插件只接收明确选择的输入和文档元数据。

这是进程内脚本扩展，不是对不可信代码的进程级沙箱；内存分配和部分内置操作不能获得
严格的资源隔离。因此应只安装可信来源插件，未来的面板、语言服务、原生 DLL 等扩展
需要新增 API 版本。目前 API v1 专用于文本处理和文本生成命令。

## 构建依赖

插件引擎使用 Qt Qml 中的 `QJSEngine`。构建需要 Qt Qml 开发组件，Ubuntu 可安装
`qt6-declarative-dev`。Windows Qt SDK 需要 `Qt6Qml`；发布部署需要 `Qt6Qml` 和
其 `Qt6Network` 运行库，正常 CMake 部署会自动携带。
