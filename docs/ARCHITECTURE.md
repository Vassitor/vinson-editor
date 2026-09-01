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
```

`MainWindow` owns presentation and action wiring, but does not send raw
Scintilla messages. `EditorWidget` is the boundary around the upstream widget;
it owns encoding mode, styles, view options, text access, and notification
translation into application-level Qt signals.

`SearchController` translates user-level search state into the narrow search
API exposed by `EditorWidget`. Searches use Scintilla target ranges directly,
including forward/backward traversal and wrap-around, so neither the controller
nor the find/replace widget copies the complete document. Replace All is grouped
as one Scintilla undo action. `FindReplaceWidget` remains non-modal and returns
focus to the editor when closed.

`EditHistoryWidget` reads logical edit groups from Scintilla's native undo
stack instead of retaining document snapshots. It shows initial, current, and
saved states, and restores a selected state by replaying undo or redo groups.
Opening or creating a document clears the underlying stack, so history always
belongs to the active document.

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

Minimal mode is another reversible `WindowController` state. On entry the
controller snapshots the prior frame, menu/status/find-panel visibility, line
numbers, scrollbars, minimum sizes, and normal window geometry. It then hides chrome, enables
frameless mode, and calculates a one-line minimum height from the active editor
font. Exit restores the snapshot, including a pre-existing frameless state.
Font changes refresh the minimum height through the existing appearance signal.
Minimal mode itself is not restored at startup; persistence reads the captured
normal geometry and prior frameless preference while Minimal mode is active.

`EditorDocument` contains document metadata independently of the widget.
`FileManager` owns one operation at a time and manages short-lived worker
threads. `FileLoader` opens and decodes files in 256 KiB chunks. A four-credit
semaphore bounds queued chunks to approximately 1 MiB; the GUI acknowledges a
chunk only after appending it to Scintilla. This prevents a fast disk from
queuing a complete large file in memory. ASCII and UTF-8 take a validated
byte-preserving path; only UTF-16 input creates bounded intermediate `QString`
chunks.

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

Lexilla 5.5.3 is pinned beside Scintilla but is not linked yet. Plain
text requires no lexer, and delaying the Lexilla target avoids enabling costly
styling before the large-file policy is in place.

Dependencies point toward application-neutral models and services. Only
`EditorWidget` on the GUI thread may mutate the Scintilla control.

## Threading rule

Qt widgets and Scintilla remain exclusively on the GUI thread. File reading,
decoding, encoding, and atomic writing run in worker threads. Only queued Qt
signals cross the boundary; cancellation and chunk acknowledgement are
thread-safe atomic/semaphore operations because a busy worker cannot service
queued slots until its current operation returns.
