# Release packaging

[简体中文](RELEASE.zh-CN.md) | English

## Windows portable package

The supported release target is 64-bit Windows with Visual Studio 2022 or newer
and its **Desktop development with C++** workload. From any PowerShell in the
repository root, run:

```powershell
.\scripts\package-windows.ps1
```

The script initializes the Visual Studio x64 environment, locates CMake, Ninja,
and a compatible Qt installation, configures and builds Release, runs all
tests, invokes CPack, and verifies the generated SHA-256 checksum. If CMake,
Ninja, or Qt is missing, Python 3.9+ is used to install a repository-local copy
under `.build-tools/`; no system-wide installation is changed. Pass `-QtRoot`
to use a specific Qt directory, or `-NoBootstrap` for an offline,
already-provisioned environment.

Tool discovery supports both `.build-tools/python` and the older
`.build-tools/python-packages` layout. Qt discovery prefers `-QtRoot`, then the
SDK recorded in the build's CMake cache, environment paths, and installed MSVC
x64 SDKs under `.build-tools/Qt`, `C:\Qt`, or `D:\Qt`. It does not require the
default bootstrap version to be installed.

To build, deploy, and start the local Release executable, run
`.\scripts\deploy-and-run-windows.ps1`. Add `-DeployOnly` to deploy without
starting the app, or `-SkipBuild` to reuse an existing executable.

To create an installer, run `.\scripts\package-installer-windows.ps1`. If NSIS
is missing, the script downloads the pinned portable NSIS 3.12 release into
`.build-tools/nsis` and verifies its SHA-256 before extraction. Use `-NsisRoot`
to select an existing installation; `-NoBootstrap` disables all downloads.
Both entry points work from the repository root or the `scripts` directory.
Run `powershell -NoProfile -File tests/WindowsScriptsTest.ps1` from the root
to check tool discovery without downloading or building anything.

During CPack installation, Qt's CMake deployment API runs `windeployqt` and
places the required Qt DLLs, compiler runtime, `qt.conf`, and platform plugins
beside the application. The resulting ZIP and `.sha256` file are written to
an independent timestamped directory under `build/release/packages/`. Separate
output directories allow a new package to be built even while an earlier ZIP
is open in Explorer or another application. The final artifact paths and hash
are printed when the script completes.

Scintilla is compiled statically into `vinson-editor.exe` and needs no separate
runtime DLL. This plain-text release has no lexer-library dependency.

Release archives include the English and Simplified Chinese README, changelog,
and `docs/` documentation. Qt Linguist Tools must be installed in the release
environment so the in-application Chinese translation and its localization
test remain part of the release gate.

## Linux portability package

Run the equivalent Release pipeline with:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --build --preset release-package
```

The TGZ contains the executable, a root-level `vinson-editor` launcher, Qt
libraries, discovered non-system runtime dependencies, platform plugins,
documentation, and a SHA-256 checksum. Start the archive through the launcher;
it supplies the relative library path needed by dynamically loaded plugins.
The target machine must provide a compatible glibc and graphics/session
libraries. An AppImage remains a later distribution target.

## Clean-machine acceptance

Validate the Windows ZIP on a 64-bit Windows machine or VM that does not have a
Qt SDK on `PATH`:

1. Verify the SHA-256 file, then extract the ZIP to a new directory.
2. Start `vinson-editor.exe` without installing anything.
3. Create, save, reopen, reload, and drag/drop a Unicode-path text file.
4. Exercise Find/Replace and its wrap toggle, control-wheel font sizing,
   appearance controls, Always On Top, Frameless, and Minimal modes.
5. Close and reopen the application to confirm settings and geometry persist.
6. Confirm `platforms/qwindows.dll` and the required Qt DLLs remain in the
   extracted package and that no Qt SDK directory is added to `PATH`.

Windows clean-machine execution is the release gate. Linux package smoke tests
are a portability check and do not replace that gate.
