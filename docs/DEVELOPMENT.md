# Development

[简体中文](DEVELOPMENT.zh-CN.md) | English

## Supported toolchain

The primary release target is 64-bit Windows with MSVC 2022 or newer. Linux is
used for continuous development and portability checks. The CMake project
rejects 32-bit configurations.

Minimum development requirements are CMake 3.25, Ninja, a C++20 compiler, and
Qt 6.5 with Core, Gui, Widgets, Test, and Core5Compat. Qt Linguist Tools is
optional; when installed, it compiles bundled translations and enables the
localization test. Phase 0 was initialized on Ubuntu 26.04 x86_64 with GCC
15.2.0. Ubuntu's current packages provide CMake 4.2.3 and Qt 6.10.2.

Ubuntu setup:

```bash
sudo apt update
sudo apt install cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-5compat-dev
```

To compile translations, also install:

```bash
sudo apt install qt6-tools-dev
```

Then configure, build, and test:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Release archives are produced by the `release-package` build preset. On
Windows, `scripts/package-windows.ps1` runs the complete configure, build, test,
and package pipeline; see [RELEASE.md](RELEASE.md) for deployment details.

If Qt is installed outside the system prefix, set `CMAKE_PREFIX_PATH` to the
Qt installation prefix before configuring.

## Third-party source policy

`third_party/scintilla` is the official Scintilla 5.6.6 source release and
`third_party/lexilla` is the official Lexilla 5.5.3 source release. Keep their
trees unmodified so upstream upgrades remain reviewable. Project-specific build
logic belongs in `cmake/`. The RGBA foreground/background adaptation is generated under
`build/<preset>/generated/scintilla`; never apply it directly to the vendored
source.

When upgrading either dependency:

1. Download a stable release from `scintilla.org`.
2. Verify the release version and retain its upstream license.
3. Replace the corresponding source tree without local edits.
4. Configure and build Debug and Release presets.
5. Run the full test suite and update architecture/performance notes.

## Repository layout

- `src/app`: application lifetime and startup
- `src/window`: top-level windows and window-mode control
- `src/editor`: Scintilla-facing editor abstractions
- `src/file`: encoding/EOL detection, bounded asynchronous loading, and atomic saving
- `src/largefile`: centralized thresholds and large-document behavior
- `src/search`: search/replace orchestration (Phase 3)
- `src/settings`: appearance themes and validated `QSettings` persistence
- `src/ui`: focused dialogs and reusable UI pieces
- `tests`: Qt Test and CTest targets
- `scripts`: repeatable release packaging entry points

## Automated tests

`file_core_tests` covers ASCII and Unicode encodings, BOM retention, LF/CRLF/CR
detection across chunk boundaries, mixed line endings, empty files, Unicode paths, invalid UTF-8,
missing files, canceled atomic saves, document metadata, and a multi-chunk
asynchronous `FileManager` load. It also covers the 64/512 MiB policy boundaries
and range-streamed, cross-thread UTF-16 saving. `editor_widget_tests` runs with Qt's offscreen
platform plugin and covers control-wheel font-size requests. `search_controller_tests` covers forward/backward traversal,
the wrap-around toggle, case and whole-word matching, Unicode text, replacement and undo,
go-to-line behavior, empty queries, and non-modal find-widget interaction. It also
covers asynchronous large-file boundary matches, wrap-around, cancellation and
restart, invalidation after text/document changes, and the panel's cancel button.
Main-window tests exercise panel/status-bar cancellation, `Esc` in normal and
minimal modes, restored editing state, and queued external open requests.
Bookmark tests cover toggling/clearing, wrapping navigation, empty documents,
line edits, document isolation in normal and large modes, real gutter clicks,
and hiding symbols without decorating the text. Main-window tests cover default
and custom shortcuts, tab switching, clearing on reload, minimal-mode navigation,
and disabling bookmark commands during search.
`document_history_tests` covers normalization, de-duplication, capacity limits,
and restoration order for recent files and closed tabs. `file_change_monitor_tests`
uses real temporary files to verify modification, removal, recreation, and
monitoring suspension around application saves. Main-window tests additionally
cover clean automatic reloads and keeping local edits after a conflict.
`recovery_manager_tests` covers background atomic snapshot writes, reads,
removal, bulk cleanup, invalid identifier rejection, and the 8 MiB limit.
`editor_widget_tests` verifies that restored text is marked dirty without
changing its content.
`appearance_tests` verifies live font/alpha previews, style application without
document mutation, translucent editor-text pixels, and alpha-zero backgrounds.
`window_controller_tests` verifies composable window flags, geometry retention,
frameless resize cursors, and real Scintilla text selection with the window
event filter active. It also verifies reversible minimal-mode UI state,
font-derived one-line sizing, prior-frameless restoration, and safe keyboard
entry/exit while preserving edits made in minimal mode.
`settings_manager_tests` covers missing-file defaults, complete persistence
round trips, and fallback behavior for malformed fonts, sizes, colors, booleans,
geometry, and directories.

`tray_controller_tests` and `global_shortcut_tests` cover tray actions, window
visibility, focus restoration, shortcut validation, and simultaneous native
registration of the boss and focus shortcuts. Native registration is exercised
only on supported Windows environments. When Qt Linguist Tools is available,
`localization_tests` validates the Simplified Chinese resources and English
fallback.

`vinson-large-file-benchmark` is a manual Release performance harness. It is
built by default but is not registered as a CTest because its full fixture set
occupies about 1.6 GiB. Reproduction commands and the latest measurements are
in `docs/PERFORMANCE.md`.
