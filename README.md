# Vinson Editor

[简体中文](README.zh-CN.md) | English

Vinson Editor is a lightweight, native plain-text editor built with C++20,
Qt 6 Widgets, and Scintilla. It supports asynchronous file loading, safe saving,
target-based search/replace, live appearance customization, reversible window
modes, and a specialized large-document path.

## Current features

- Native Qt 6 desktop window with menu and status bar
- Import and update JavaScript text plugins with settings, command shortcuts, parameter forms, line/selection/document processing, clipboard and new-tab results; cancellable background execution and undo, 11 bundled Text Tools commands and a [plugin guide](docs/PLUGINS.md)
- Scintilla 5.6.6 editing widget, compiled directly from upstream source
- UTF-8 text input, undo/redo, clipboard actions, wrapping, and line numbers
- Dockable per-document-session edit-history timeline with current/saved markers
  and state restoration
- New, Open, Save, Save As, Reload, Exit, and single-file drag-and-drop
- Multi-document tabs, session restore, recent files, and reopening the most recently closed file tab
- External modification, removal, and rename detection with automatic clean reloads and explicit conflict choices
- Save-time disk version checks, including content fingerprints up to 8 MiB, before overwriting an externally changed file
- Size-bounded background recovery snapshots for normal documents, offered after an abnormal exit
- Unsaved-change protection for New, Open, Reload, and Exit
- UTF-8, UTF-8 BOM, ASCII, UTF-16 LE, and UTF-16 BE round trips
- File > Save Encoding selects UTF-8, UTF-8 BOM, UTF-16 LE, or UTF-16 BE for the next save; changing it marks the document as unsaved independently of text undo.
- File > Convert Line Endings converts existing text to LF, CRLF, or CR as one undo action and sets the newline used for subsequent input in that tab.
- LF, CRLF, CR, and mixed-EOL detection; existing content stays intact until explicitly converted
- Worker-thread, 256 KiB chunked loading with a bounded GUI queue and cancellation
- Worker-thread atomic saving through `QSaveFile`
- Automatic Normal/Large/Very Large modes at centralized 64/512 MiB thresholds
- Scintilla large-text, style-free documents selected before large-file loading
- Large-file defaults disable word wrap and use the null lexer
- Range-streamed, atomic large-file saving without a whole-document snapshot
- Explicit status-bar mode feedback and a Very Large Replace All confirmation
- Reproducible 10 MiB–1 GiB load/search/edit/save benchmark tooling
- Validated `QSettings` persistence for geometry, appearance, view options,
  window flags, and the last file directory
- Recovery of saved windows that no longer intersect an available display
- Cursor line/column, encoding, EOL, file-size, and modified-state feedback
- Non-modal find/replace with next, previous, wrap-around, case, and whole-word options
- Scheduled large-file search slices with progress and cancellation through Cancel Search, the status-bar Cancel button, or `Esc`; cancellation preserves text, selection, and undo history
- Replace current, replace all as one undo action, and go to line
- Per-document line bookmarks with a clickable gutter, customizable shortcuts, clearing, and wrapping next/previous navigation; marks follow line edits and survive tab switches, and are cleared when closing or reloading a document
- Live font family/size and text, background, cursor, and selected-text colors
- Multiple named custom appearance styles with optional switch shortcuts and JSON import/export
- Independent background and text opacity: 0 is transparent, 255 is opaque.
  Background opacity applies in frameless and minimal modes.
- Configurable system-wide boss key plus a focus shortcut that shows, restores,
  and activates the window, then focuses the editor for immediate typing
- Frameless mode with top-border or `Alt+Left Drag` system movement, edge resizing, and a one-line minimum height that follows font size and line spacing
- Always-on-top mode, composable with frameless mode
- Minimal mode hides all chrome and allows a dynamically sized one-line window
- Minimal-mode exit through `Ctrl+Shift+M` or `Esc` without changing document data
- 64-bit-only CMake configuration with Debug and Release presets
- Headless Qt Test coverage and an application smoke-test mode
- CPack portable archives with Qt Runtime deployment and SHA-256 checksums

## Dependencies

- A 64-bit C++20 compiler (MSVC 2022+, GCC 12+, or Clang 15+)
- CMake 3.25+
- Ninja
- Qt 6.5+ with Widgets, Test, Core5Compat, and Qml
- Qt Linguist Tools (optional; required to compile the bundled translations)

Scintilla 5.6.6 is vendored under `third_party/` from its official source
release. The editor uses plain-text mode and does not require a lexer library.

On Ubuntu 26.04, install the development dependencies with:

```bash
sudo apt update
sudo apt install cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-5compat-dev qt6-declarative-dev
```

To compile the bundled translations, also install:

```bash
sudo apt install qt6-tools-dev
```

## Build and run

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/vinson-editor
```

Run the test suite with:

```bash
ctest --preset debug
```

For a non-interactive startup check (useful in CI):

```bash
QT_QPA_PLATFORM=offscreen ./build/debug/vinson-editor --smoke-test
```

Build a tested Release portable package on Windows with:

```powershell
.\scripts\package-windows.ps1
```

Or use the platform-neutral CMake package preset after running Release tests:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --build --preset release-package
```

Windows packages use `windeployqt` automatically and run without a Qt SDK.
See [docs/RELEASE.md](docs/RELEASE.md) for package contents and clean-machine
acceptance steps.

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for platform notes and the
dependency policy.

## Keyboard shortcuts

All application command shortcuts can be reassigned or cleared from **Settings
→ Appearance and Shortcuts → Shortcuts**. Custom style shortcuts are edited with
their styles on the Appearance tab. Mouse-wheel and drag gestures keep the
bindings shown below.

| Action | Shortcut |
| --- | --- |
| New | `Ctrl+N` |
| Open | `Ctrl+O` |
| Save | `Ctrl+S` |
| Save As | `Ctrl+Shift+S` |
| Reload | `Ctrl+Shift+R` |
| Quit | Platform standard (`Ctrl+Q` on Linux) |
| Undo | `Ctrl+Z` |
| Redo | Platform standard (`Ctrl+Shift+Z` on Linux) |
| Cut / Copy / Paste | `Ctrl+X` / `Ctrl+C` / `Ctrl+V` |
| Select all | `Ctrl+A` |
| Find / Replace | `Ctrl+F` / `Ctrl+H` |
| Find next / previous | `F3` / `Shift+F3` |
| Go to line | `Ctrl+G` |
| Toggle current-line bookmark | `Ctrl+F2` |
| Next / previous bookmark | `F2` / `Shift+F2` |
| Clear all bookmarks in the current document | `Ctrl+Shift+F2` |
| Edit history | `Ctrl+Shift+H` |
| Previous / next custom style | `Ctrl+Alt+PageUp` / `Ctrl+Alt+PageDown` |
| Adjust font size | `Ctrl+Mouse Wheel` |
| Always on top | `Ctrl+Shift+T` |
| Frameless mode | `F11` |
| Move a frameless window | Drag the top border or use `Alt+Left Drag` |
| Minimal mode | `Ctrl+Shift+M` |
| Exit minimal mode | `Esc` or `Ctrl+Shift+M` |

## Windows installer

To generate a Windows installer, install [NSIS 3.03+](https://nsis.sourceforge.io/Download)
and run `./scripts/package-installer-windows.ps1`. Use `-NsisRoot "C:\Tools\NSIS"`
for an extracted NSIS distribution. The script builds, runs the complete test suite,
deploys Qt, and writes an `.exe` installer with a `.sha256` checksum under
`build/release/packages/<timestamp>/`. Installation requires administrator rights
and includes Start Menu shortcuts, third-party notices, and an uninstall entry.
When upgrading, the installer reads the registered installation directory and
shows an in-place update notice without uninstalling the previous version. If
the editor is running, it asks whether to stop the process and continues only
after stopping it successfully. Uninstalling retains user settings
and documents. The existing `package-windows.ps1` continues to produce portable ZIPs.
The confirmation warns that stopping the running process can discard unsaved content.

## Documentation

See the [documentation index](docs/README.md) for development, architecture,
performance measurements, and release instructions. Application sources live
under `src/`, manual performance tools under `benchmarks/`, and dependency and
packaging integration under `cmake/`.

## License

Project code is licensed under the MIT License. Scintilla retains its upstream
license in `third_party/scintilla/License.txt`.
Third-party copyright notices, Qt SDK inventories, and license texts are listed
in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and included in release packages.
