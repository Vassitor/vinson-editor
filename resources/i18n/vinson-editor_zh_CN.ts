<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en">
<context>
    <name>vinson::PluginSafety</name>
    <message><source>Obfuscated code</source><translation>混淆代码</translation></message>
    <message><source>Dynamic code execution</source><translation>动态代码执行</translation></message>
    <message><source>System or module access</source><translation>系统或模块访问</translation></message>
    <message><source>Network access</source><translation>网络访问</translation></message>
    <message><source>File access or deletion</source><translation>文件访问或删除</translation></message>
    <message><source>Encoded content</source><translation>编码内容</translation></message>
    <message><source>Potential infinite loop</source><translation>可能的无限循环</translation></message>
    <message><source>Escaped identifiers can hide the APIs being called and make review harder.</source><translation>转义标识符可能隐藏所调用的接口，使代码更难审查。</translation></message>
    <message><source>Code can be generated or evaluated at runtime, hiding behavior from this static check. Generated code can still read or change the provided document text.</source><translation>代码可以在运行时生成或求值，使此静态检测无法识别其实际行为。生成的代码仍可读取或修改传入的文档文本。</translation></message>
    <message><source>This code refers to modules or system commands that could execute programs or access the computer in a host with those APIs. These host APIs are not provided by the current plugin runtime.</source><translation>代码引用了模块或系统命令，在提供这些接口的宿主中可能执行程序或访问计算机。当前插件运行时不提供这些宿主接口。</translation></message>
    <message><source>This code refers to network APIs that could send document text to a remote server in a host with those APIs. Network APIs are not provided by the current plugin runtime.</source><translation>代码引用了网络接口，在提供这些接口的宿主中可能将文档文本发送到远程服务器。当前插件运行时不提供网络接口。</translation></message>
    <message><source>This code refers to file APIs that could read, overwrite or delete files in a host with those APIs. File APIs are not provided by the current plugin runtime.</source><translation>代码引用了文件接口，在提供这些接口的宿主中可能读取、覆盖或删除文件。当前插件运行时不提供文件接口。</translation></message>
    <message><source>Decoded text may conceal executable code or destinations. Review how the decoded value is used; decoding alone is not necessarily harmful.</source><translation>解码后的文本可能隐藏可执行代码或目标地址。请检查解码结果的用途；解码操作本身不一定有害。</translation></message>
    <message><source>A loop without a limiting condition can consume CPU or memory. Cancellation and the time limit reduce CPU stalls but cannot guarantee recovery from memory exhaustion. Check for a reachable break or return.</source><translation>没有限制条件的循环可能耗尽 CPU 或内存。取消和超时可减少 CPU 长时间占用，但无法保证从内存耗尽中恢复。请检查是否存在可执行到的 break 或 return。</translation></message>
</context>
<context>
    <name>vinson::PluginConfigurationDialog</name>
    <message><source>Plugin Settings: %1</source><translation>插件设置：%1</translation></message>
    <message><source>This plugin has no settings.</source><translation>此插件没有专属设置。</translation></message>
    <message><source>Settings</source><translation>设置</translation></message>
    <message><source>Command Shortcuts</source><translation>命令快捷键</translation></message>
    <message><source>Plugin Settings</source><translation>插件设置</translation></message>
    <message><source>Use one key combination per command. Clear it to disable the shortcut. Shortcuts must not conflict with editor commands, styles or other enabled plugins.</source><translation>每个命令可设置一个组合键，清空即可取消快捷键。快捷键不能与编辑器命令、样式或其他已启用插件冲突。</translation></message>
</context>
<context>
    <name>vinson::PluginParametersDialog</name>
    <message><source>Set the options for this run.</source><translation>设置本次执行使用的参数。</translation></message>
</context>
<context>
    <name>vinson::PluginDialog</name>
    <message><source>Installed Plugins</source><translation>已安装插件</translation></message>
    <message><source>Developer Guide</source><translation>开发文档</translation></message>
    <message><source>Could not load the bundled developer guide.</source><translation>无法加载内置开发文档。</translation></message>
    <message><source>Example Plugin Source</source><translation>示例插件源码</translation></message>
    <message><source>Plugin Security Review</source><translation>插件安全检测</translation></message>
    <message><source>Potential risks were found in %1 (%2). Review them before installing.</source><translation>在 %1（%2）中发现潜在风险，请在安装前查看详情。</translation></message>
    <message><source>%1 — %2 (%3), script line %4
%5
Code: %6</source><translation>%1 — %2（%3），脚本第 %4 行
%5
代码：%6</translation></message>
    <message><source>This static check does not execute commands and cannot detect every risk. Plugins can read or change the supplied text. The current runtime exposes no file, network or system APIs, but it runs inside the editor process and cannot guarantee protection from memory exhaustion. Only continue if you trust the author and accept these risks.</source><translation>此静态检测不会执行命令，也无法识别所有风险。插件可以读取或修改传入的文本。当前运行时不提供文件、网络或系统接口，但运行在编辑器进程内，无法保证防止内存耗尽。请仅在信任作者并接受这些风险时继续。</translation></message>
    <message><source>Install Anyway</source><translation>仍然安装</translation></message>
    <message><source>Up to 64 findings per command are shown.</source><translation>每个命令最多显示前 64 条检测结果。</translation></message>
    <message><source>Plugin Settings…</source><translation>插件设置…</translation></message>
    <message><source>Update Plugin</source><translation>更新插件</translation></message>
    <message><source>Replace %1 (%2) with version %3? Settings and enabled state will be kept.</source><translation>将 %1（%2）替换为版本 %3？将保留设置和启用状态。</translation></message>
    <message><source>Manage Plugins</source><translation>管理插件</translation></message>
    <message><source>Import a .vinson-plugin package to add commands to the Plugins menu. Only install plugins from authors you trust. Commands can read and change the current document.</source><translation>导入 .vinson-plugin 插件包，将命令添加到“插件”菜单。请只安装可信作者的插件；插件命令可以读取和修改当前文档。</translation></message>
    <message><source>Plugin</source><translation>插件</translation></message>
    <message><source>Version</source><translation>版本</translation></message>
    <message><source>Status</source><translation>状态</translation></message>
    <message><source>Import Plugin…</source><translation>导入插件…</translation></message>
    <message><source>Install Example</source><translation>安装示例</translation></message>
    <message><source>Enable</source><translation>启用</translation></message>
    <message><source>Disable</source><translation>停用</translation></message>
    <message><source>Uninstall</source><translation>卸载</translation></message>
    <message><source>Reload</source><translation>重新加载</translation></message>
    <message><source>Open Plugin Folder</source><translation>打开插件文件夹</translation></message>
    <message><source>Plugins</source><translation>插件</translation></message>
    <message><source>Could not open the plugin folder.</source><translation>无法打开插件文件夹。</translation></message>
    <message><source>Import Plugin</source><translation>导入插件</translation></message>
    <message><source>Vinson plugins (*.vinson-plugin)</source><translation>Vinson 插件 (*.vinson-plugin)</translation></message>
    <message><source>Uninstall Plugin</source><translation>卸载插件</translation></message>
    <message><source>Uninstall %1?</source><translation>卸载 %1？</translation></message>
    <message><source>Error</source><translation>错误</translation></message>
    <message><source>Enabled</source><translation>已启用</translation></message>
    <message><source>Disabled</source><translation>已停用</translation></message>
    <message><source>No plugins installed. Import a package or install the example to get started.</source><translation>尚未安装插件。可导入插件包或安装示例。</translation></message>
    <message><source>%1
ID: %2 · %3 command(s)
%4</source><translation>%1
ID：%2 · %3 个命令
%4</translation></message>
    <message><source>%1
%2</source><translation>%1
%2</translation></message>
</context>
<context>
    <name>vinson::PluginManager</name>
    <message><source>Review the security findings and confirm this exact package before installing. If the package changed, scan it again.</source><translation>请先查看安全检测详情并确认此插件包，再进行安装。插件包发生变化时必须重新检测。</translation></message>
    <message><source>Invalid plugin settings schema.</source><translation>插件设置格式无效。</translation></message>
    <message><source>Invalid command shortcut or parameter schema.</source><translation>命令快捷键或参数格式无效。</translation></message>
    <message><source>Invalid value for %1.</source><translation>%1 的值无效。</translation></message>
    <message><source>Unknown plugin setting or parameter.</source><translation>未知的插件设置或参数。</translation></message>
    <message><source>Cannot read plugin preferences. Repair preferences.json before saving settings.</source><translation>无法读取插件配置。保存设置前请修复 preferences.json。</translation></message>
    <message><source>Could not save plugin preferences: %1</source><translation>无法保存插件配置：%1</translation></message>
    <message><source>Use one key combination for %1; Escape is reserved for cancellation.</source><translation>%1 只能设置一个组合键；Esc 保留用于取消操作。</translation></message>
    <message><source>Shortcut %1 for %2 is already in use.</source><translation>%2 的快捷键 %1 已被占用。</translation></message>
    <message><source>Unknown plugin command shortcut.</source><translation>未知的插件命令快捷键。</translation></message>
    <message><source>This plugin ID is already installed. Confirm replacement to update it.</source><translation>此插件 ID 已安装。确认替换即可更新。</translation></message>
    <message><source>Cannot read plugin file.</source><translation>无法读取插件文件。</translation></message>
    <message><source>Plugin packages must be at most 1 MiB.</source><translation>插件包不能超过 1 MiB。</translation></message>
    <message><source>Invalid plugin JSON: %1</source><translation>插件 JSON 无效：%1</translation></message>
    <message><source>Unsupported plugin API version; expected 1.</source><translation>不支持此插件 API 版本；需要版本 1。</translation></message>
    <message><source>Invalid plugin ID, name, version or description.</source><translation>插件 ID、名称、版本或描述无效。</translation></message>
    <message><source>A plugin must contain a commands array.</source><translation>插件必须包含 commands 命令数组。</translation></message>
    <message><source>A plugin must contain between 1 and 32 commands.</source><translation>插件必须包含 1 至 32 个命令。</translation></message>
    <message><source>Invalid or duplicate plugin command.</source><translation>插件命令无效或重复。</translation></message>
    <message><source>Invalid script in %1: %2</source><translation>%1 中的脚本无效：%2</translation></message>
    <message><source>Could not save plugin settings: %1</source><translation>无法保存插件设置：%1</translation></message>
    <message><source>Duplicate plugin ID.</source><translation>插件 ID 重复。</translation></message>
    <message><source>Wait for the running plugin command to finish.</source><translation>请等待正在执行的插件命令完成。</translation></message>
    <message><source>This plugin ID is already installed. Uninstall it before importing another version.</source><translation>此插件 ID 已安装。导入其他版本前请先卸载。</translation></message>
    <message><source>The plugin destination already exists or cannot be created.</source><translation>插件目标文件已存在或无法创建。</translation></message>
    <message><source>Could not copy the plugin package.</source><translation>无法复制插件包。</translation></message>
    <message><source>The plugin package changed during import. Try again.</source><translation>插件包在导入期间发生更改。请重试。</translation></message>
    <message><source>Plugin is unavailable or a command is running.</source><translation>插件不可用或有命令正在执行。</translation></message>
    <message><source>Could not remove the plugin package.</source><translation>无法删除插件包。</translation></message>
    <message><source>Plugin input exceeds the 8 MiB limit.</source><translation>插件输入超过 8 MiB 限制。</translation></message>
    <message><source>Plugin commands must return a string.</source><translation>插件命令必须返回字符串。</translation></message>
    <message><source>Plugin output exceeds the 8 MiB limit.</source><translation>插件输出超过 8 MiB 限制。</translation></message>
    <message><source>Plugin command cancelled or timed out.</source><translation>插件命令已取消或超时。</translation></message>
    <message><source>Plugin command is unavailable or another command is running.</source><translation>插件命令不可用或有其他命令正在执行。</translation></message>
</context>
<context>
    <name>vinson::MainWindow</name>
    <message><source>Plugin result copied to clipboard.</source><translation>插件结果已复制到剪贴板。</translation></message>
    <message><source>Plugin result opened in a new tab.</source><translation>插件结果已在新标签页中打开。</translation></message>
    <message><source>Shortcut %1 is already in use. Change it in plugin settings.</source><translation>快捷键 %1 已被占用，请在插件设置中修改。</translation></message>
    <message><source>&amp;Plugins</source><translation>插件(&amp;P)</translation></message>
    <message><source>Manage Plugins…</source><translation>管理插件…</translation></message>
    <message><source>No enabled plugins</source><translation>没有已启用的插件</translation></message>
    <message><source>Plugin Result</source><translation>插件结果</translation></message>
    <message><source>Plugin command completed.</source><translation>插件命令已完成。</translation></message>
    <message><source>Document changed; plugin result was discarded.</source><translation>文档已更改，已丢弃插件结果。</translation></message>
    <message><source>Select text before running this plugin command.</source><translation>执行此插件命令前请先选择文本。</translation></message>
    <message><source>Plugin text operations are limited to 8 MiB. Select a smaller range.</source><translation>插件文本操作限于 8 MiB。请选择较小的范围。</translation></message>
    <message><source>Running plugin: %1…</source><translation>正在执行插件：%1…</translation></message>
    <message><source>Save &amp;Encoding</source><translation>保存编码(&amp;E)</translation></message>
    <message><source>Convert &amp;Line Endings</source><translation>转换换行符(&amp;L)</translation></message>
    <message><source>&amp;Bookmarks</source><translation>书签(&amp;B)</translation></message>
    <message><source>Toggle &amp;Bookmark</source><translation>添加或移除书签(&amp;B)</translation></message>
    <message><source>&amp;Next Bookmark</source><translation>下一处书签(&amp;N)</translation></message>
    <message><source>&amp;Previous Bookmark</source><translation>上一处书签(&amp;P)</translation></message>
    <message><source>&amp;Clear All Bookmarks</source><translation>清除全部书签(&amp;C)</translation></message>
    <message><source>Bookmarks cleared.</source><translation>已清除当前文档的全部书签。</translation></message>
    <message><source>Bookmark added at line %1.</source><translation>已在第 %1 行添加书签。</translation></message>
    <message><source>Bookmark removed from line %1.</source><translation>已移除第 %1 行的书签。</translation></message>
    <message><source>Moved to bookmark at line %1.</source><translation>已转到第 %1 行的书签。</translation></message>
    <message><source>No bookmarks in this document.</source><translation>当前文档没有书签。</translation></message>
    <message><source>New Tab</source><translation>新建标签页</translation></message>
    <message><source>Save Tab</source><translation>保存标签页</translation></message>
    <message><source>Copy File Path</source><translation>复制文件路径</translation></message>
    <message><source>Close Tab</source><translation>关闭标签页</translation></message>
    <message><source>Close Other Tabs</source><translation>关闭其他标签页</translation></message>
    <message><source>Close Tabs to the Right</source><translation>关闭右侧标签页</translation></message>
    <message><source>Close All Tabs</source><translation>关闭所有标签页</translation></message>
    <message><source>Cancel</source><translation>取消</translation></message>
    <message><source>Ready</source><translation>就绪</translation></message>
    <message><source>Untitled</source><translation>未命名</translation></message>
    <message><source>Ln %1, Col %2</source><translation>第 %1 行，第 %2 列</translation></message>
    <message><source>Cancel or wait for the current file operation.</source><translation>请取消当前文件操作或等待操作完成。</translation></message>
    <message><source>&amp;File</source><translation>文件(&amp;F)</translation></message>
    <message><source>&amp;New</source><translation>新建(&amp;N)</translation></message>
    <message><source>&amp;Open…</source><translation>打开(&amp;O)…</translation></message>
    <message><source>Open &amp;Recent</source><translation>最近使用的文件(&amp;R)</translation></message>
    <message><source>Reopen Closed Tab</source><translation>重新打开已关闭的标签页</translation></message>
    <message><source>Reopen closed tab</source><translation>重新打开已关闭的标签页</translation></message>
    <message><source>The file no longer exists and cannot be reopened.\n%1</source><translation>该文件已不存在，无法重新打开。\n%1</translation></message>
    <message><source>File changed on disk</source><translation>磁盘上的文件已更改</translation></message>
    <message><source>%1 may have changed on disk since it was opened. Saving now could overwrite another program's changes.</source><translation>自打开以来，磁盘上的 %1 可能已发生变化。现在保存可能覆盖其他程序的修改。</translation></message>
    <message><source>Overwrite File</source><translation>覆盖文件</translation></message>
    <message><source>File removed from disk</source><translation>磁盘上的文件已移除</translation></message>
    <message><source>%1 was changed by another program. Reloading will discard your editor changes.</source><translation>%1 已被其他程序更改。重新加载将丢弃编辑器中的更改。</translation></message>
    <message><source>Reload from Disk</source><translation>从磁盘重新加载</translation></message>
    <message><source>Keep Editor Changes</source><translation>保留编辑器更改</translation></message>
    <message><source>Kept editor changes for %1</source><translation>已保留 %1 的编辑器更改</translation></message>
    <message><source>%1 was removed or renamed by another program.</source><translation>%1 已被其他程序移除或重命名。</translation></message>
    <message><source>Keep Open</source><translation>保持打开</translation></message>
    <message><source>Reloading externally changed file %1</source><translation>正在重新加载外部更改的文件 %1</translation></message>
    <message><source>Recover unsaved documents</source><translation>恢复未保存的文档</translation></message>
    <message><source>Vinson Editor found %1 document(s) with unsaved changes from a previous session.</source><translation>Vinson Editor 发现上次会话中有 %1 个包含未保存更改的文档。</translation></message>
    <message><source>Restore Documents</source><translation>恢复文档</translation></message>
    <message><source>Discard Recovery Data</source><translation>丢弃恢复数据</translation></message>
    <message><source>Restored %1 unsaved document(s)</source><translation>已恢复 %1 个未保存的文档</translation></message>
    <message><source>Crash recovery is limited to documents of %1 or smaller</source><translation>崩溃恢复仅支持不超过 %1 的文档</translation></message>
    <message><source>No Recent Files</source><translation>暂无最近使用的文件</translation></message>
    <message><source>&amp;Clear Recent Files</source><translation>清除最近使用的文件(&amp;C)</translation></message>
    <message><source>Open recent file</source><translation>打开最近使用的文件</translation></message>
    <message><source>The file no longer exists and was removed from the recent files list.\n%1</source><translation>该文件已不存在，已从最近使用的文件列表中移除。\n%1</translation></message>
    <message><source>&amp;Save</source><translation>保存(&amp;S)</translation></message>
    <message><source>Save &amp;As…</source><translation>另存为(&amp;A)…</translation></message>
    <message><source>&amp;Reload</source><translation>重新加载(&amp;R)</translation></message>
    <message><source>E&amp;xit</source><translation>退出(&amp;X)</translation></message>
    <message><source>&amp;Edit</source><translation>编辑(&amp;E)</translation></message>
    <message><source>&amp;Undo</source><translation>撤销(&amp;U)</translation></message>
    <message><source>&amp;Redo</source><translation>重做(&amp;R)</translation></message>
    <message><source>Cu&amp;t</source><translation>剪切(&amp;T)</translation></message>
    <message><source>&amp;Copy</source><translation>复制(&amp;C)</translation></message>
    <message><source>&amp;Paste</source><translation>粘贴(&amp;P)</translation></message>
    <message><source>Select &amp;All</source><translation>全选(&amp;A)</translation></message>
    <message><source>&amp;Search</source><translation>搜索(&amp;S)</translation></message>
    <message><source>&amp;Find…</source><translation>查找(&amp;F)…</translation></message>
    <message><source>&amp;Replace…</source><translation>替换(&amp;R)…</translation></message>
    <message><source>Find &amp;Next</source><translation>查找下一个(&amp;N)</translation></message>
    <message><source>Find &amp;Previous</source><translation>查找上一个(&amp;P)</translation></message>
    <message><source>&amp;Go To Line…</source><translation>转到行(&amp;G)…</translation></message>
    <message><source>&amp;View</source><translation>视图(&amp;V)</translation></message>
    <message><source>Word &amp;Wrap</source><translation>自动换行(&amp;W)</translation></message>
    <message><source>Word wrap may be slow in %1.</source><translation>在%1下自动换行可能较慢。</translation></message>
    <message><source>Line &amp;Numbers</source><translation>行号(&amp;N)</translation></message>
    <message><source>Edit History</source><translation>编辑历史</translation></message>
    <message><source>Edit &amp;History</source><translation>编辑历史(&amp;H)</translation></message>
    <message><source>Always on &amp;Top</source><translation>总在最前(&amp;T)</translation></message>
    <message><source>&amp;Frameless Mode</source><translation>无边框模式(&amp;F)</translation></message>
    <message><source>&amp;Minimal Mode</source><translation>极简模式(&amp;M)</translation></message>
    <message><source>Exit Minimal Mode</source><translation>退出极简模式</translation></message>
    <message><source>&amp;Settings</source><translation>设置(&amp;S)</translation></message>
    <message><source>Custom &amp;Styles</source><translation>自定义样式(&amp;S)</translation></message>
    <message><source>Previous Style</source><translation>上一个样式</translation></message>
    <message><source>Next Style</source><translation>下一个样式</translation></message>
    <message><source>Previous Tab</source><translation>上一个标签页</translation></message>
    <message><source>Next Tab</source><translation>下一个标签页</translation></message>
    <message><source>Style: %1</source><translation>样式：%1</translation></message>
    <message><source>&amp;Appearance and Shortcuts…</source><translation>外观和快捷键(&amp;A)…</translation></message>
    <message><source>Increase Background Opacity</source><translation>提高背景不透明度</translation></message>
    <message><source>Decrease Background Opacity</source><translation>降低背景不透明度</translation></message>
    <message><source>Background opacity: %1 / 255</source><translation>背景不透明度：%1 / 255</translation></message>
    <message><source>Font size: %1 pt</source><translation>字号：%1 磅</translation></message>
    <message><source>Replace All in a very large file</source><translation>在超大文件中全部替换</translation></message>
    <message><source>Replace All may take a long time and create a large undo record. Continue?</source><translation>全部替换可能耗时较长并产生很大的撤销记录。是否继续？</translation></message>
    <message><source>Go To Line</source><translation>转到行</translation></message>
    <message><source>Line number:</source><translation>行号：</translation></message>
    <message><source>Open file</source><translation>打开文件</translation></message>
    <message><source>Scintilla could not create a document for this file.</source><translation>Scintilla 无法为此文件创建文档。</translation></message>
    <message><source>Loading %1</source><translation>正在加载 %1</translation></message>
    <message><source>Loading %1 / %2</source><translation>正在加载 %1 / %2</translation></message>
    <message><source>Loaded %1</source><translation>已加载 %1</translation></message>
    <message><source>Saved %1</source><translation>已保存 %1</translation></message>
    <message><source>File operation failed</source><translation>文件操作失败</translation></message>
    <message><source>File operation canceled</source><translation>文件操作已取消</translation></message>
    <message><source>New document</source><translation>新建文档</translation></message>
    <message><source>Scintilla could not create a new document.</source><translation>Scintilla 无法创建新文档。</translation></message>
    <message><source>Open Text File</source><translation>打开文本文件</translation></message>
    <message><source>Text files (*);;All files (*)</source><translation>文本文件 (*);;所有文件 (*)</translation></message>
    <message><source>Another file operation is in progress.</source><translation>另一项文件操作正在进行。</translation></message>
    <message><source>Save file</source><translation>保存文件</translation></message>
    <message><source>Saving %1</source><translation>正在保存 %1</translation></message>
    <message><source>Unsaved changes</source><translation>未保存的更改</translation></message>
    <message><source>Save changes to %1?</source><translation>是否保存对 %1 的更改？</translation></message>
    <message><source>Save Text File</source><translation>保存文本文件</translation></message>
    <message><source>Text files (*.txt);;All files (*)</source><translation>文本文件 (*.txt);;所有文件 (*)</translation></message>
</context>
<context>
    <name>vinson::Application</name>
    <message><source>Vinson Editor</source><translation>Vinson Editor</translation></message>
    <message><source>Could not start the single-instance service: %1</source><translation>无法启动单例服务：%1</translation></message>
</context>
<context>
    <name>vinson::SingleInstance</name>
    <message><source>Could not create the single-instance request directory.</source><translation>无法创建单例请求目录。</translation></message>
    <message><source>Could not identify the running instance.</source><translation>无法识别正在运行的程序实例。</translation></message>
    <message><source>The running instance is not ready.</source><translation>正在运行的程序实例尚未就绪。</translation></message>
    <message><source>The running instance did not acknowledge the request.</source><translation>正在运行的程序实例未确认该请求。</translation></message>
</context>
<context>
    <name>vinson::EditHistoryWidget</name>
    <message><source>Inserted %1 bytes</source><translation>插入 %1 字节</translation></message>
    <message><source>Deleted %1 bytes</source><translation>删除 %1 字节</translation></message>
    <message><source>Replaced text</source><translation>替换文本</translation></message>
    <message><source>Edited document</source><translation>编辑文档</translation></message>
    <message><source>No edits in this document.</source><translation>此文档尚无编辑记录。</translation></message>
    <message><source>Restore Selected</source><translation>恢复所选状态</translation></message>
    <message><source>Current</source><translation>当前</translation></message>
    <message><source>Saved</source><translation>已保存</translation></message>
    <message><source>%1 (%2)</source><translation>%1（%2）</translation></message>
    <message><source>Initial state</source><translation>初始状态</translation></message>
</context>
<context>
    <name>vinson::TrayController</name>
    <message><source>E&amp;xit</source><translation>退出(&amp;X)</translation></message>
    <message><source>&amp;Hide Window</source><translation>隐藏窗口(&amp;H)</translation></message>
    <message><source>&amp;Show Window</source><translation>显示窗口(&amp;S)</translation></message>
    <message><source>&amp;Settings…</source><translation>设置(&amp;S)…</translation></message>
    <message><source>Boss key: Disabled</source><translation>老板键：已禁用</translation></message>
    <message><source>Boss key: %1</source><translation>老板键：%1</translation></message>
    <message><source>Focus shortcut: Disabled</source><translation>聚焦快捷键：已禁用</translation></message>
    <message><source>Focus shortcut: %1</source><translation>聚焦快捷键：%1</translation></message>
</context>
<context>
    <name>vinson::GlobalShortcut</name>
    <message><source>Use one shortcut containing at least one modifier key.</source><translation>请使用一组至少包含一个修饰键的快捷键。</translation></message>
    <message><source>The shortcut is already used by another application.</source><translation>该快捷键已被其他应用程序占用。</translation></message>
    <message><source>Global shortcuts are supported on Windows only.</source><translation>全局快捷键仅支持 Windows。</translation></message>
</context>
<context>
    <name>vinson::FindReplaceWidget</name>
    <message><source>Cancel Search</source><translation>取消搜索</translation></message>
    <message><source>Match case</source><translation>区分大小写</translation></message>
    <message><source>Whole word</source><translation>全字匹配</translation></message>
    <message><source>Wrap around</source><translation>循环查找</translation></message>
    <message><source>Next</source><translation>下一个</translation></message>
    <message><source>Previous</source><translation>上一个</translation></message>
    <message><source>Close</source><translation>关闭</translation></message>
    <message><source>Replace</source><translation>替换</translation></message>
    <message><source>Replace All</source><translation>全部替换</translation></message>
    <message><source>Find:</source><translation>查找：</translation></message>
    <message><source>Replace:</source><translation>替换：</translation></message>
</context>
<context>
    <name>vinson::SettingsDialog</name>
    <message><source>Appearance</source><translation>外观</translation></message>
    <message><source>Settings</source><translation>设置</translation></message>
    <message><source>Typography</source><translation>字体</translation></message>
    <message><source>Colors</source><translation>颜色</translation></message>
    <message><source>Transparency</source><translation>透明度</translation></message>
    <message><source>Custom Styles</source><translation>自定义样式</translation></message>
    <message><source>Style:</source><translation>样式：</translation></message>
    <message><source>Switch shortcut:</source><translation>切换快捷键：</translation></message>
    <message><source>Save As…</source><translation>另存为…</translation></message>
    <message><source>Update</source><translation>更新</translation></message>
    <message><source>Rename…</source><translation>重命名…</translation></message>
    <message><source>Delete</source><translation>删除</translation></message>
    <message><source>A style stores the font, colors, and opacity. Use Settings &gt; Custom Styles to switch between saved styles.</source><translation>样式保存字体、颜色和不透明度，可通过“设置 → 自定义样式”切换。</translation></message>
    <message><source>No saved styles</source><translation>暂无已保存的样式</translation></message>
    <message><source>Select a saved style</source><translation>选择已保存的样式</translation></message>
    <message><source>Reset appearance, application and global shortcuts, and the startup option. Saved styles are kept.</source><translation>恢复外观、应用及全局快捷键和启动选项的默认值，保留已保存的样式。</translation></message>
    <message><source>Style Name</source><translation>样式名称</translation></message>
    <message><source>Name:</source><translation>名称：</translation></message>
    <message><source>A style with this name already exists.</source><translation>同名样式已存在。</translation></message>
    <message><source>You can save up to %1 styles.</source><translation>最多可保存 %1 个样式。</translation></message>
    <message><source>Style Shortcut</source><translation>样式快捷键</translation></message>
    <message><source>The shortcut for “%1” must contain a modifier key.</source><translation>“%1”的快捷键必须包含修饰键。</translation></message>
    <message><source>The shortcut for “%1” is already in use.</source><translation>“%1”的快捷键已被占用。</translation></message>
    <message><source>Shortcuts</source><translation>快捷键</translation></message>
    <message><source>Session</source><translation>会话</translation></message>
    <message><source>Restore open tabs on startup</source><translation>启动时恢复上次打开的标签页</translation></message>
    <message><source>Reopen previously saved files. Changes that were not saved to disk cannot be restored.</source><translation>重新打开上次已保存的文件，未保存到磁盘的更改无法恢复。</translation></message>
    <message><source>Global Shortcuts</source><translation>全局快捷键</translation></message>
    <message><source>Application Shortcuts</source><translation>应用快捷键</translation></message>
    <message><source>Command</source><translation>命令</translation></message>
    <message><source>Shortcut</source><translation>快捷键</translation></message>
    <message><source>Application Shortcut</source><translation>应用快捷键</translation></message>
    <message><source>The shortcut for “%1” is not valid.</source><translation>“%1”的快捷键无效。</translation></message>
    <message><source> pt</source><translation> 磅</translation></message>
    <message><source>Font:</source><translation>字体：</translation></message>
    <message><source>Font size:</source><translation>字号：</translation></message>
    <message><source>Font style:</source><translation>字体样式：</translation></message>
    <message><source>Bold</source><translation>粗体</translation></message>
    <message><source>Italic</source><translation>斜体</translation></message>
    <message><source>Cursor width:</source><translation>光标宽度：</translation></message>
    <message><source>Line spacing:</source><translation>行间距：</translation></message>
    <message><source> px</source><translation> 像素</translation></message>
    <message><source>Text color:</source><translation>文字颜色：</translation></message>
    <message><source>Text opacity:</source><translation>文字不透明度：</translation></message>
    <message><source>Background color:</source><translation>背景颜色：</translation></message>
    <message><source>Background opacity:</source><translation>背景不透明度：</translation></message>
    <message><source>Cursor color:</source><translation>光标颜色：</translation></message>
    <message><source>Selected text color:</source><translation>选中文字颜色：</translation></message>
    <message><source>Selection color:</source><translation>选区背景色：</translation></message>
    <message><source>Selection opacity:</source><translation>选区不透明度：</translation></message>
    <message><source>Line number color:</source><translation>行号颜色：</translation></message>
    <message><source>Current line color:</source><translation>当前行颜色：</translation></message>
    <message><source>Current line opacity:</source><translation>当前行不透明度：</translation></message>
    <message><source>Boss key:</source><translation>老板键：</translation></message>
    <message><source>Focus shortcut:</source><translation>聚焦快捷键：</translation></message>
    <message><source>The boss key works system-wide. Include Ctrl, Alt, Shift, or the Windows key.</source><translation>老板键在系统范围内生效，请包含 Ctrl、Alt、Shift 或 Windows 键。</translation></message>
    <message><source>Boss key</source><translation>老板键</translation></message>
    <message><source>Use one shortcut containing at least one modifier key.</source><translation>请使用一组至少包含一个修饰键的快捷键。</translation></message>
    <message><source>0 = transparent; 255 = opaque. Background opacity applies in frameless and minimal modes. Text opacity affects only editor text.</source><translation>0 为完全透明，255 为不透明。背景不透明度仅在无边框和极简模式下生效；文字不透明度仅影响编辑区文字。</translation></message>
    <message><source>The focus shortcut shows, restores, and activates the window, then focuses the editor for immediate typing.</source><translation>聚焦快捷键会显示、还原并激活窗口，然后聚焦编辑区以便立即输入。</translation></message>
    <message><source>Focus shortcut</source><translation>聚焦快捷键</translation></message>
    <message><source>Global shortcuts</source><translation>全局快捷键</translation></message>
    <message><source>The boss key and focus shortcut must be different.</source><translation>老板键与聚焦快捷键不能相同。</translation></message>
    <message><source>Text Color</source><translation>文字颜色</translation></message>
    <message><source>Background Color</source><translation>背景颜色</translation></message>
    <message><source>Cursor Color</source><translation>光标颜色</translation></message>
    <message><source>Selected Text Color</source><translation>选中文字颜色</translation></message>
    <message><source>Selection Color</source><translation>选区背景色</translation></message>
    <message><source>Line Number Color</source><translation>行号颜色</translation></message>
    <message><source>Current Line Color</source><translation>当前行颜色</translation></message>
    <message><source>Import…</source><translation>导入…</translation></message>
    <message><source>Export…</source><translation>导出…</translation></message>
    <message><source>Styles store all appearance options. Import and export use a JSON file containing the current appearance and saved styles.</source><translation>样式保存全部外观选项。导入和导出使用包含当前外观与已保存样式的 JSON 文件。</translation></message>
    <message><source>Import Appearance</source><translation>导入外观</translation></message>
    <message><source>Export Appearance</source><translation>导出外观</translation></message>
    <message><source>Vinson appearance files (*.json);;All files (*)</source><translation>Vinson 外观文件 (*.json);;所有文件 (*)</translation></message>
    <message><source>Could not read the selected file: %1</source><translation>无法读取所选文件：%1</translation></message>
    <message><source>The appearance file is not valid: %1</source><translation>外观文件无效：%1</translation></message>
    <message><source>Appearance and saved styles were imported.</source><translation>已导入外观和保存的样式。</translation></message>
    <message><source>Could not write the appearance file: %1</source><translation>无法写入外观文件：%1</translation></message>
    <message><source>Appearance and saved styles were exported.</source><translation>已导出外观和保存的样式。</translation></message>
</context>
<context>
    <name>vinson::EditorWidget</name>
    <message><source>Undo</source><translation>撤销</translation></message>
    <message><source>Redo</source><translation>重做</translation></message>
    <message><source>Cut</source><translation>剪切</translation></message>
    <message><source>Copy</source><translation>复制</translation></message>
    <message><source>Paste</source><translation>粘贴</translation></message>
    <message><source>Select All</source><translation>全选</translation></message>
    <message><source>Find…</source><translation>查找…</translation></message>
</context>
<context>
    <name>vinson::SearchController</name>
    <message><source>Searching… Press Esc to cancel.</source><translation>正在搜索… 按 Esc 取消。</translation></message>
    <message><source>Search cancelled.</source><translation>搜索已取消。</translation></message>
    <message><source>Enter text to find.</source><translation>请输入要查找的文字。</translation></message>
    <message numerus="yes"><source>Replaced %n occurrence(s).</source><translation><numerusform>已替换 %n 处。</numerusform></translation></message>
    <message><source>Moved to line %1.</source><translation>已转到第 %1 行。</translation></message>
    <message><source>Line %1 is outside the document.</source><translation>第 %1 行超出文档范围。</translation></message>
    <message><source>No matches for “%1”.</source><translation>未找到“%1”。</translation></message>
    <message><source>Search wrapped at the document boundary.</source><translation>已从文档边界继续搜索。</translation></message>
    <message><source>Match found.</source><translation>已找到匹配项。</translation></message>
</context>
<context>
    <name>LargeFilePolicy</name>
    <message><source>Large File Mode</source><translation>大文件模式</translation></message>
    <message><source>Very Large File Mode</source><translation>超大文件模式</translation></message>
</context>
<context>
    <name>vinson::FileLoader</name>
    <message><source>Cannot open %1: %2</source><translation>无法打开 %1：%2</translation></message>
    <message><source>Cannot read %1: %2</source><translation>无法读取 %1：%2</translation></message>
    <message><source>%1 contains invalid %2 data.</source><translation>%1 包含无效的 %2 数据。</translation></message>
    <message><source>%1 ends with an incomplete %2 sequence.</source><translation>%1 以不完整的 %2 序列结尾。</translation></message>
    <message><source>%1 contains invalid UTF-8 data.</source><translation>%1 包含无效的 UTF-8 数据。</translation></message>
    <message><source>%1 ends with an incomplete UTF-8 sequence.</source><translation>%1 以不完整的 UTF-8 序列结尾。</translation></message>
</context>
<context>
    <name>vinson::FileSaver</name>
    <message><source>Cannot save %1: %2</source><translation>无法保存 %1：%2</translation></message>
    <message><source>Cannot commit %1 safely: %2</source><translation>无法安全提交 %1：%2</translation></message>
    <message><source>The editor contains invalid UTF-8 data.</source><translation>编辑器包含无效的 UTF-8 数据。</translation></message>
    <message><source>The editor could not provide the next save range.</source><translation>编辑器无法提供下一段保存范围。</translation></message>
    <message><source>The document changed while it was being saved.</source><translation>文档在保存过程中发生了更改。</translation></message>
</context>
</TS>
