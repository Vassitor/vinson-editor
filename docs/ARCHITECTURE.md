# Architecture

[简体中文](ARCHITECTURE.zh-CN.md) | English

## Current structure

```text
main
  -> Application (process lifetime and application metadata)
      -> MainWindow (desktop window composition and actions)
          -> WindowController (window flags and native move/resize requests)
          -> FindReplaceWidget (non-modal search controls)
              -> SearchController (search/replace orchestration)
          -> EditHistoryWidget (undo-stack timeline and restoration)
          -> SettingsDialog (live appearance controls)
              -> ThemeManager (validated appearance state)
          -> EditorWidget (stable editor-facing API)
              -> ScintillaEdit (upstream Qt adapter)
                  -> Scintilla document engine
          -> EditorDocument (path, encoding, EOL, size, modified state)
          -> LargeFilePolicy (centralized thresholds and degradation rules)
          -> SettingsManager (validated persistent application state)
          -> FileManager (asynchronous operation coordinator)
              -> QThread + FileLoader
              -> QThread + FileSaver + QSaveFile
          -> FileChangeMonitor (external modification, removal, and recreation detection)
          -> RecoveryManager (background atomic recovery snapshots)
```

`MainWindow` owns presentation and action wiring, but does not send raw
Scintilla messages. `EditorWidget` is the boundary around the upstream widget;
it owns encoding mode, styles, view options, text access, and notification
translation into application-level Qt signals.

`PluginManager` validates and copies single-file JSON/JavaScript packages into
the user's application data directory and atomically persists enabled IDs.
Typed settings and shortcut overrides live in a separate atomic preferences file.
Package updates preserve this state; schema changes normalize incompatible values.
`PluginFieldForm` renders the shared settings/parameter schema, with persistent
configuration dialogs and transient command parameter dialogs. The host resolves
shortcut collisions, records the selected input range and supplies byte-based
editor metadata; outputs may also target the clipboard or a new unsaved tab.
`PluginDialog` handles import and lifecycle operations; `MainWindow` builds menu
commands and applies results through editor text APIs as one undo action.
Each command uses a fresh `QJSEngine` on a worker thread, with no QObject or I/O
APIs exposed. A GUI timer interrupts execution after three seconds; input and
output are bounded to 8 MiB. The host locks editing and records document identity
and revision to reject stale results. See [PLUGINS.md](PLUGINS.md) for API v1 and
the limits of in-process resource isolation.

`SearchController` translates user-level search state into the narrow search
API exposed by `EditorWidget`. Searches use Scintilla target ranges directly,
including forward/backward traversal and wrap-around, so neither the controller
nor the find/replace widget copies the complete document. Replace All is grouped
as one Scintilla undo action. `FindReplaceWidget` remains non-modal and returns
focus to the editor when closed.

In large modes, a single-shot `SearchController` timer schedules overlapping
8 MiB target ranges one at a time, returning to the normal event loop between
scans so cancellation buttons and `Esc` remain responsive. Only the query,
options, document identity, and offsets are retained. Cancellation stops the
timer and publishes a distinct result. `MainWindow` pauses editing, replacement,
history restoration, and tab switching, queues open requests, and defers external
file changes until the search ends. Text changes cancel pending searches; each
scan also checks document identity and length before using its offsets. During
search, `Esc` cancels first; afterwards its usual find-panel/minimal-mode behavior
resumes.

`EditHistoryWidget` reads logical edit groups from Scintilla's native undo
stack instead of retaining document snapshots. It shows initial, current, and
saved states, and restores a selected state by replaying undo or redo groups.
Opening or creating a document clears the underlying stack, so history always
belongs to the active document.

Line bookmarks use native Scintilla marker 0 through `EditorWidget`. Each
document retains its own marks and the engine adjusts them with line edits.
A separate symbol margin toggles a clicked line without changing the caret or
selection. Menu commands use the existing shortcut settings. Next/previous
navigation skips the current line and wraps at document boundaries. Mark changes
preserve text and undo history; file operations and active searches pause bookmark
commands. Hiding line numbers or entering minimal mode sets the symbol to Empty
and the margin width to zero, preventing Scintilla from tinting an entire line
when its symbol margin is hidden. Bookmarks last for the current open-document
session and are cleared on close/reload; they are not saved into text files or
persistent session settings.

`ThemeManager` owns the current in-memory appearance, clamps font sizes, keeps
caret and selection colors opaque, and applies independent background and
editor-text alpha through `EditorWidget` without touching the document. Window
chrome uses an opaque copy of the text color for readability. `SettingsDialog`
previews every control immediately; Cancel restores the pre-dialog appearance.

`TrayController` owns two independent `GlobalShortcut` registrations. The boss
key toggles visibility; the focus shortcut always shows, restores, raises, and
activates the window, then requests keyboard focus for `EditorWidget`.

`SettingsManager` is the only `QSettings` boundary. It validates font family,
point size, RGBA colors, booleans, geometry payload size, and the last directory
before returning application state. `MainWindow` restores window flags before
geometry, then verifies that a meaningful part of the frame intersects an
available screen. Invalid or off-screen geometry falls back to the primary
display. Settings never contain document text or file contents.

`WindowController` is the sole owner of frameless and always-on-top transitions.
It preserves geometry, window state, focus, and combined flags when Qt recreates
the native window. While frameless, its application event filter only consumes
`Alt+Left` presses accepted by `QWindow::startSystemMove()` or presses in a
six-pixel resize border accepted by `QWindow::startSystemResize()`. Ordinary
editor mouse events pass through unchanged, preserving text selection. The top
border is also a direct move target, while corners and other edges resize. F11
remains attached to the main window while its system frame is absent.

Frameless mode replaces the editor's padded minimum height with the actual
Scintilla line height, including configured line spacing. It refreshes nested
layout minima and preserves room for visible panels. Appearance changes update
the minimum height; leaving frameless mode restores the captured size limits.

Minimal mode is another reversible `WindowController` state. On entry the
controller snapshots the prior frame, menu/status/find-panel visibility, line
numbers, scrollbars, minimum sizes, and normal window geometry. It then hides chrome, enables
frameless mode, and calculates a one-line minimum height from the active editor
font. Exit restores the snapshot, including a pre-existing frameless state.
Font changes refresh the minimum height through the existing appearance signal.
Minimal mode itself is not restored at startup; persistence reads the captured
normal geometry and prior frameless preference while Minimal mode is active.

`EditorDocument` contains document metadata independently of the widget.
Its selected encoding and saved encoding are tracked separately, so text undo
cannot clear an encoding-only unsaved change. `EditorWidget` maintains newline
counts per native document from insertion/deletion notifications and their
adjacent bytes, including CRLF pairs formed or split by edits. Explicit EOL
conversion is one native undo action; insertion EOL mode is restored per tab.
`FileManager` owns one operation at a time and manages short-lived worker
threads. `FileLoader` opens and decodes files in 256 KiB chunks. A four-credit
semaphore bounds queued chunks to approximately 1 MiB; the GUI acknowledges a
chunk only after appending it to Scintilla. This prevents a fast disk from
queuing a complete large file in memory. ASCII and UTF-8 take a validated
byte-preserving path; only UTF-16 input creates bounded intermediate `QString`
chunks.

`FileChangeMonitor` watches both every open file and its parent directory. A
clean document reloads automatically after an external content change; local
edits and removed or renamed files are resolved when their tab becomes active.
The monitor is suspended around the editor's own atomic saves and re-baselined
after completion so those writes are not reported as external changes.

`RecoveryManager` writes independent atomic data and metadata files for dirty
normal documents on a dedicated worker thread. A snapshot is submitted after
editing settles for about 2.5 seconds and immediately when switching tabs;
saving, closing, and normal shutdown remove it. Leftover snapshots are offered
on the next startup. Recovery is capped at 8 MiB per document and is disabled
for Large and Very Large modes to avoid duplicating large buffers.

`LineEndingDetector` is shared by whole-buffer and streaming paths. It recognizes
CRLF sequences split across chunk boundaries without requiring a second pass.

`LargeFilePolicy` classifies source sizes as Normal below 64 MiB, Large at
64 MiB, and Very Large at 512 MiB. `MainWindow` applies the policy before the
first decoded chunk is appended. `EditorWidget` creates and attaches a new
Scintilla document with `TEXT_LARGE | STYLES_NONE`, balances the creator
reference after `SETDOCPOINTER`, and selects the null lexer. Word wrap defaults
off in both large modes, while Very Large Replace All requires confirmation.
The source size plus a small edit reserve preallocates document capacity. The
current mode is always included in the status bar. Large searches use
overlapping target-range slices so paint and timer events run during long scans.

`FileSaver` uses a stable UTF-8 snapshot for normal documents. Large documents
instead request 256 KiB ranges from `EditorWidget` one at a time; encoding and
`QSaveFile` I/O stay on the worker thread, and the editor is disabled so the
declared range cannot change mid-save. Cancellation or failure discards the
temporary file rather than truncating the source. ASCII documents are upgraded
to UTF-8 when required, avoiding data loss.

Scintilla is compiled as the private static target `Scintilla::Scintilla` from
the official 5.6.6 source tree. The upstream `ScintillaEdit` layer is used
because it provides a generated typed API on top of `ScintillaEditBase`.
The vendored tree remains unmodified: CMake creates a build-directory copy of
`Editor.cxx` with guarded RGBA style-foreground and style-background adapters.
This is necessary because the public style-color messages otherwise normalize
every color to opaque RGB before the Qt renderer sees it. Configuration fails
if the pinned source no longer matches either expected adapter point.
Qt 6 builds link Core5Compat because the current upstream Qt adapter uses
`QTextCodec` for legacy code pages. The build defines the upstream
`EXPORT_IMPORT_API` annotation as empty because a static library must not expose
Windows DLL import/export declarations.

Application targets use C++20. The unmodified Scintilla target is compiled as
C++17, matching its upstream requirement and keeping upstream C++17 constructs
from producing C++20 deprecation diagnostics.

Plain-text editing requires no lexer library. Syntax support can introduce a
lexer dependency when it has an application target and a defined large-file policy.

Dependencies point toward application-neutral models and services. Only
`EditorWidget` on the GUI thread may mutate the Scintilla control.

## Threading rule

Qt widgets and Scintilla remain exclusively on the GUI thread. File reading,
decoding, encoding, and atomic writing run in worker threads. Only queued Qt
signals cross the boundary; cancellation and chunk acknowledgement are
thread-safe atomic/semaphore operations because a busy worker cannot service
queued slots until its current operation returns.
