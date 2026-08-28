# Vinson Editor

Vinson Editor is a lightweight, native plain-text editor built with C++20,
Qt 6 Widgets, and Scintilla. Phase 10 release engineering is in place: the
native editing surface is backed by asynchronous file loading, safe saving,
target-based search/replace, live appearance customization, reversible window
modes, and a specialized large-document path.

## Current features

- Native Qt 6 desktop window with menu and status bar
- Scintilla 5.6.6 editing widget, compiled directly from upstream source
- UTF-8 text input, undo/redo, clipboard actions, wrapping, and line numbers
- New, Open, Save, Save As, Reload, Exit, and single-file drag-and-drop
- Unsaved-change protection for New, Open, Reload, and Exit
- UTF-8, UTF-8 BOM, ASCII, UTF-16 LE, and UTF-16 BE round trips
- LF, CRLF, CR, and mixed-EOL detection without normalizing existing content
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
- Replace current, replace all as one undo action, and go to line
- Live font family/size and text, background, cursor, and selected-text colors
- Background alpha from 0–255, including a fully transparent background with opaque text
- Frameless mode with `Alt+Left Drag` system movement and edge system resizing
- Always-on-top mode, composable with frameless mode
- Minimal mode hides all chrome and allows a dynamically sized one-line window
- Minimal-mode exit through `Ctrl+Shift+M` or `Esc` without changing document data
- 64-bit-only CMake configuration with Debug and Release presets
- Headless Qt Test coverage and an application smoke-test mode
- CPack portable archives with Qt Runtime deployment and SHA-256 checksums

## Screenshot

> Screenshot placeholder: a Windows portable-build capture will be added with
> the first tagged binary release.

## Dependencies

- A 64-bit C++20 compiler (MSVC 2022+, GCC 12+, or Clang 15+)
- CMake 3.25+
- Ninja
- Qt 6.5+ with Widgets, Test, and Core5Compat

Scintilla 5.6.6 and Lexilla 5.5.3 are vendored under `third_party/` from their
official source releases. Lexilla is pinned now and will be linked when lexer
support is introduced; the current editor deliberately uses plain-text mode.

On Ubuntu 26.04, install the development dependencies with:

```bash
sudo apt update
sudo apt install cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-5compat-dev
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
| Always on top | `Ctrl+Shift+T` |
| Frameless mode | `F11` |
| Move a frameless window | `Alt+Left Drag` |
| Minimal mode | `Ctrl+Shift+M` |
| Exit minimal mode | `Esc` or `Ctrl+Shift+M` |

## License

Project code is licensed under the MIT License. Scintilla and Lexilla retain
their upstream licenses in their respective `third_party` directories.
