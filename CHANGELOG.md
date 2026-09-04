# Changelog

[简体中文](CHANGELOG.zh-CN.md) | English

All notable changes to Vinson Editor are documented in this file. Versions use
the Semantic Versioning scheme.

## [Unreleased]

### Added

- Multi-document tabs in the default window mode, with `Ctrl+Alt+Left/Right`
  navigation, browser-style top-chrome presentation, drag reordering, and
  independent editor state for each open file.
- Single-instance launch forwarding: reopening Vinson Editor now activates the
  running window and opens the requested files as tabs.
- An optional session setting that restores file-backed tabs on the next
  startup.
- A persistent **Open Recent** menu with ordered, de-duplicated file history,
  missing-file cleanup, and a clear-list command.
- A dockable edit-history timeline backed by Scintilla's native undo stack,
  with current/saved markers and restoration to any logical edit state.
- Persistent font-size adjustment with `Ctrl+Mouse Wheel`.
- A user-selectable wrap-around option in the non-modal find/replace panel.
- Simplified Chinese editions of the README, changelog, architecture,
  development, performance, and release documentation.

### Changed

- Grouped the settings dialog into appearance and shortcut categories, kept
  color swatches stable on hover, and refined the edit-history panel with
  menu-like rows while retaining native dock controls. Standard save, settings,
  and color-dialog buttons now use the bundled Qt Chinese translations.
- Matched the find and replace panel background to the edit-history panel in
  the default window mode.
- Prevented a one-pixel black seam from appearing beside the vertical scroll
  bar after maximizing or resizing the window.
- Kept editor text and line numbers sharp when moving the window between
  monitors with different display scaling.
- Unified whole-buffer and streaming line-ending detection, including CRLF
  sequences split across input chunks.
- Made Qt Linguist Tools optional for core builds while retaining translated
  resources and localization tests when the tools are installed.
- Restored native window-manager menu shadows and made the top border a direct
  frameless-window drag target.

## [0.1.0] - 2026-08-28

### Added

- Native Qt 6 and Scintilla plain-text editing surface with file, edit, search,
  replace, navigation, and drag-and-drop workflows.
- Asynchronous bounded-memory loading and atomic saving for UTF-8, UTF-16, and
  common line-ending formats.
- Automatic Normal, Large, and Very Large modes, plus a reproducible benchmark
  covering real files through 1 GiB.
- Live font, color, alpha, and view settings with validated persistence.
- Frameless, always-on-top, and reversible one-line Minimal modes.
- Debug and Release presets, automated GUI/core tests, Windows version metadata,
  Qt Runtime deployment, and portable ZIP/TGZ packaging with SHA-256 checksums.
