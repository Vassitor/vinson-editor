# Changelog

[简体中文](CHANGELOG.zh-CN.md) | English

All notable changes to Vinson Editor are documented in this file. Versions use
the Semantic Versioning scheme.

## [Unreleased]

### Fixed

- Saving checks the disk file against the loaded version using size and
  modification time, plus a SHA-256 content fingerprint for files up to 8 MiB.
  Changed or unverifiable files require confirmation before overwrite.
- Extend the existing rounded menu shadow beyond the popup boundary, fixing clipped shadows and detached rectangular outlines without changing menu widgets, colors, or dimensions.
- Closing the last tab now refreshes its document identity and clears the saved
  session entry, keeping subsequent tab switching and reordering consistent.
- Opening Find skips copying oversized selections before checking the prefill limit.
- Restore Defaults now also resets application shortcuts and the startup option
  while preserving saved styles; selecting the current style again reapplies it.
- Invalid global shortcut fallbacks no longer collide with a customized boss key.
- Settings clarify opacity values and supported window modes, remove hard-coded
  shortcut hints, and improve text contrast on bright color swatches.

### Added

- Manual per-tab save encoding selection (UTF-8, UTF-8 BOM, UTF-16 LE/BE), with unsaved-change protection for encoding-only changes. LF/CRLF/CR conversion is one undo action; EOL status tracks edits and undo incrementally without rescanning the document.
- Per-document line bookmarks through Search > Bookmarks, the gutter beside line numbers, or customizable `Ctrl+F2`. `F2`/`Shift+F2` navigate with wrapping and `Ctrl+Shift+F2` clears the current document's marks. Bookmarks follow line edits, remain independent across tabs, and preserve text, modified state, and undo history. Minimal mode hides the symbols while retaining keyboard navigation.
- Large-file find now schedules one slice at a time with progress and cancellation from the find panel, status bar, or `Esc`. Editing, replacement, and tab switching pause until completion; cancellation preserves text, selection, and undo history.
- Expanded appearance controls for font style, selection, line numbers, current-line highlighting, caret width, and line spacing, with JSON import/export for the current appearance and saved styles.
- Added abnormal-exit recovery with background atomic snapshots after editing
  settles and immediate snapshots on tab switches. Recovery is limited to
  8 MiB per normal document and does not duplicate large documents.
- Added external file-change detection with automatic clean reloads, explicit
  conflict/removal choices, and suppression around the editor's own atomic saves.
- Added **Reopen Closed Tab** and extracted recent-file and closed-tab history
  into a dedicated model to prepare for richer session recovery.
- Named custom appearance styles with optional per-style shortcuts and
  `Ctrl+Alt+PageUp/PageDown` cycling.
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

- Made every application keyboard shortcut editable and persistent, including
  entering and exiting minimal mode, and changed the settings dialog background
  to white.
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
