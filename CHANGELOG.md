# Changelog

[简体中文](CHANGELOG.zh-CN.md) | English

All notable changes to Vinson Editor are documented in this file. Versions use
the Semantic Versioning scheme.

## [Unreleased]

### Added

- Persistent font-size adjustment with `Ctrl+Mouse Wheel`.
- A user-selectable wrap-around option in the non-modal find/replace panel.
- Simplified Chinese editions of the README, changelog, architecture,
  development, performance, and release documentation.

### Changed

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
