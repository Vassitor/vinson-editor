# Plugins

[简体中文](PLUGINS.zh-CN.md) | English

## Installing and managing plugins

Open **Plugins → Manage Plugins**, choose **Import Plugin**, and select a
`.vinson-plugin` package. Imported packages are enabled immediately and their
commands appear under **Plugins → Plugin name**. No restart or compiler is required.
Enable, disable, uninstall, or reload packages in the manager. Enabled state
persists across restarts. Uninstall removes the managed copy, preserving the
original source package and plugin preferences. Importing the same ID offers an
atomic replacement that preserves enabled state, settings and shortcut overrides.
Values rejected by the updated schema fall back to new defaults. Invalid packages
cannot overwrite the installed version.

**Install Example** installs the bundled Text Tools plugin, with uppercase
selection, JSON formatting, word count, timestamps, line sorting/deduplication,
trailing whitespace cleanup, wrapping, numbered lists, JSON string copying and
text reports (11 commands). Click Install Example again to update an older copy. Its source
is [text-tools.vinson-plugin](../examples/plugins/text-tools.vinson-plugin).
After editing an installed package, use **Reload** to refresh commands. Broken
packages show an error and can be uninstalled. Packages copied into the directory
manually are disabled until enabled through the manager.

**Open Plugin Folder** opens the per-user installation directory under Qt's
application data location: typically `%APPDATA%/VinsonEditor/Vinson Editor/plugins`
on Windows and `~/.local/share/VinsonEditor/Vinson Editor/plugins` on Linux.
`enabled.json` stores enabled IDs; `preferences.json` stores settings and command
shortcuts. Corrupt preferences fall back to defaults and must be repaired before
saving to avoid replacing old data. No plugin command runs during import or startup.

## Developer guide preview

The manager's **Developer Guide** tab previews this complete guide offline in
Simplified Chinese or English. Click the example source link to view its JSON
without installing or executing it.

## Security review

Importing and updating statically checks command scripts before writing the
package, without executing commands. Patterns include dynamic evaluation
(`eval`, `Function`, `constructor`), system/module, network and file API references,
encoded content, escaped identifiers, and potentially unlimited loops such as
`while (true)`, `while (1)` and `for (;;)`. Ordinary comments, string text and
common regex bodies are ignored; template expressions and string property access
are inspected.

Findings list the command title/ID, script line (starting at 1), code excerpt and
specific consequences. **Cancel** is the default; Escape and closing the window
also cancel. Only explicitly choosing **Install Anyway** proceeds. Cancelling an
update preserves the installed package, enabled state and preferences. Approval
is tied to the package's SHA-256; changed bytes require another review. Valid
packages without findings follow normal import, without a claim of certified safety.

The heuristic may miss risks or report harmless patterns. It cannot recognize
all indirect calls, obfuscation, recursion, complex regexes or memory exhaustion.
At most the first 64 distinct category/line findings per command are displayed.
Decoding, constructors and loops are not inherently harmful: review their use.
File, network and system APIs are absent in the current runtime; references
indicate potential intent, not permission to use these APIs. Scripts can still
read and change supplied text, and the in-process engine lacks strict memory
isolation. Install trusted plugins. This installation check does not monitor
manual copies or later edits in the plugin directory; re-import modified packages
to check their new code.

## Package format (API v1)

A package is one UTF-8 JSON file, at most 1 MiB, with the `.vinson-plugin` extension:

```json
{
  "apiVersion": 1,
  "id": "my.text-tools",
  "name": "My Text Tools",
  "version": "1.0.0",
  "description": "Uppercase the selected text",
  "commands": [
    {
      "id": "uppercase",
      "title": "Uppercase Selection",
      "input": "selection",
      "output": "replaceSelection",
      "script": "return context.text.toUpperCase();"
    }
  ]
}
```

IDs start with a lowercase letter and contain only lowercase letters, digits,
dots, and hyphens, up to 80 characters. Plugin IDs are globally unique; command
IDs are unique within a plugin. `name` and `title` are nonempty strings up to 120
characters; `version` is nonempty and at most 40 characters. Optional `description`
is at most 2000 characters. Each package contains 1–32 commands. `apiVersion` must
be the number `1`. Unsupported API versions and invalid syntax fail at import.

`script` is a strict-mode JavaScript function body taking `context` and returning
a string synchronously. Standard built-ins such as `JSON`, `Date`, and regular
expressions are available. Async commands, Node.js modules, DOM, network, Qt
objects, and file access are unavailable.

| `input` | `context.text` |
| --- | --- |
| `selection` | Current selection; requires selected text |
| `document` | Full text of the active tab |
| `none` | Empty string, useful for text generators |
| `line` | Current line, excluding its line ending |
| `selectionOrDocument` | Selected text, or the whole document when no text is selected |

`context.fileName` is the full current file path (empty for untitled documents),
`context.line` is the current 1-based line, and `context.lineEnding` is the insertion
line ending actually used by the editor (`\n`, `\r\n`, or `\r`).
Additional metadata includes the 1-based Scintilla `column`, `lineCount`, caret
`position`, selection byte boundaries `selectionStart`/`selectionEnd`, and UTF-8
`documentLength`. `inputScope` is the resolved input: `selection`, `document`,
`line`, or `none`. Byte offsets are not JavaScript string indices.

| `output` | Returned string usage |
| --- | --- |
| `replaceSelection` | Replace the original selection; requires selected text |
| `replaceDocument` | Replace all text of the active document |
| `insert` | Insert at the original caret without deleting existing selected text |
| `message` | Display plain text in a result dialog, truncated to 16000 UTF-16 units |
| `replaceInput` | Replace the original input range; useful for `line` and `selectionOrDocument`; incompatible with `none` |
| `clipboard` | Copy the result while preserving document content and modified state |
| `newDocument` | Put the result in a new untitled tab; nonempty results are unsaved and undoable; preserve the original tab |

Exceptions, non-string returns, cancellation, and timeout leave the document
unchanged. A text change is one undo operation and uses the existing modified,
save, and recovery mechanisms. Results are discarded if a programmatic edit or
document switch occurred while the command was running.

## Settings, shortcuts and command parameters

Select a plugin in the manager and choose **Plugin Settings**. Schema fields
generate checkboxes, spin boxes, text inputs and choice lists automatically.
The **Command Shortcuts** tab edits or clears individual bindings. OK saves and
applies; Cancel leaves preferences unchanged. Restore Defaults takes effect only
after confirmation with OK. Shortcuts also work with the menu bar hidden.
Use one key combination per command; Escape is reserved for cancellation.
Saving checks editor, global, style and other enabled plugin bindings. Conflicting
package defaults or later editor binding changes suppress only the plugin shortcut,
keeping the menu command available. The first command in plugin filename order
wins conflicts between plugins; reassign bindings in plugin settings.

The optional top-level `settings` and per-command `parameters` arrays share a
field schema, with at most 32 fields each. Field IDs follow plugin ID rules and
are unique within each array. Required keys are `id`, `label` (up to 120
characters), `type` and a correctly typed `default`:

```json
"settings": [
  {"id": "indent", "label": "Indentation", "type": "integer", "default": 2, "minimum": 1, "maximum": 8},
  {"id": "enabled", "label": "Case sensitive", "type": "boolean", "default": true},
  {"id": "prefix", "label": "Prefix", "type": "string", "default": ">", "maxLength": 100},
  {"id": "order", "label": "Order", "type": "choice", "default": "asc", "choices": ["asc", "desc"]}
]
```

Integer bounds default to -1000000/1000000 and must remain within that range.
String `maxLength` defaults to 4096 UTF-16 units and accepts 1–16384. Choice fields
require 1–64 unique nonempty strings, each at most 120 characters, and a default
from that list. Values are validated during import, saving and execution.

Read persisted settings through `context.settings`. Scripts receive a snapshot
and cannot persist changes to it. Commands declaring parameters display an input
dialog before running; read the values through `context.parameters`. Cancelling
the dialog does not execute code or modify the document. Parameters do not persist
between runs and omitted values use schema defaults. For example:

```json
{
  "id": "wrap",
  "title": "Wrap Selection",
  "shortcut": "Ctrl+Alt+Shift+W",
  "input": "selection",
  "output": "replaceSelection",
  "parameters": [
    {"id": "before", "label": "Before", "type": "string", "default": "**"},
    {"id": "after", "label": "After", "type": "string", "default": "**"}
  ],
  "script": "return context.parameters.before + context.text + context.parameters.after;"
}
```

`shortcut` is an optional Qt PortableText string. Missing or empty values define
no default binding. All additions are optional and existing API v1 packages remain
compatible.

## Execution limits

Each command runs on a worker thread in a fresh JavaScript engine. Editing and
tab switching are locked until completion. Use the status-bar **Cancel** button
or `Esc` to interrupt; the default timeout is 3 seconds. UTF-8 input and output
are limited to 8 MiB, as are whole-document replacements. Smaller selections
remain usable in large documents. Only the declared input and document metadata
are exposed, with no file, network, or editor object APIs.

This is an in-process script extension, not a process sandbox for hostile code.
Memory allocations and some built-in operations do not have strict resource
isolation. Install trusted plugins. API v1 supports text transformations and
generators; panels, language services, and native libraries would require a
future API.

## Building

The engine uses Qt Qml's `QJSEngine`. Install `qt6-declarative-dev` on Ubuntu or
include Qt6Qml in the Windows Qt SDK. Standard CMake deployment includes Qt6Qml
and its Qt6Network runtime dependency.
