param(
    [string]$BuildDirectory = "",
    [string]$QtRoot = "",
    [int]$BuildJobs = 4,
    [switch]$SkipBuild,
    [switch]$DeployOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildToolsRoot = Join-Path $repositoryRoot ".build-tools"
if (!$BuildDirectory) {
    $BuildDirectory = Join-Path $repositoryRoot "build\release"
}
if (!$QtRoot) {
    $QtRoot = Join-Path $repositoryRoot ".build-tools\Qt\6.8.3\msvc2022_64"
}

if (!(Test-Path -LiteralPath $BuildDirectory -PathType Container)) {
    throw "Build directory was not found: $BuildDirectory"
}
if (!(Test-Path -LiteralPath $QtRoot -PathType Container)) {
    throw "Qt directory was not found: $QtRoot"
}

$resolvedBuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$resolvedQtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$application = Join-Path $resolvedBuildDirectory "vinson-editor.exe"
$deployTool = Join-Path $resolvedQtRoot "bin\windeployqt.exe"

if (!(Test-Path -LiteralPath $deployTool -PathType Leaf)) {
    throw "windeployqt was not found: $deployTool"
}

$vsDevCmdCandidates = @()
if ($env:VSINSTALLDIR) {
    $vsDevCmdCandidates += Join-Path $env:VSINSTALLDIR `
        "Common7\Tools\VsDevCmd.bat"
}
foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
    $vsDevCmdCandidates += Join-Path ${env:ProgramFiles} `
        "Microsoft Visual Studio\2022\$edition\Common7\Tools\VsDevCmd.bat"
}
$vsDevCmd = $vsDevCmdCandidates |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (!$vsDevCmd) {
    throw "Visual Studio 2022 build tools were not found."
}

if (!$SkipBuild) {
    $cmakeCandidates = @(
        (Join-Path $buildToolsRoot "python\cmake\data\bin\cmake.exe"),
        (Join-Path $buildToolsRoot "python\bin\cmake.exe")
    )
    $cmake = $cmakeCandidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
    if (!$cmake) {
        $cmakeCommand = Get-Command cmake.exe -CommandType Application `
            -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $cmakeCommand) {
            $cmake = $cmakeCommand.Source
        }
    }
    if (!$cmake) {
        throw "CMake was not found. Install it or restore the repository build tools."
    }

    Write-Host "Building the latest Release executable..." -ForegroundColor Cyan
    $buildCommand = 'call "{0}" -arch=x64 -host_arch=x64 >nul && "{1}" --build "{2}" --target vinson-editor -j {3}' -f `
        $vsDevCmd, $cmake, $resolvedBuildDirectory, $BuildJobs
    & $env:ComSpec /d /s /c $buildCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Release build failed with exit code $LASTEXITCODE. If Vinson Editor is running, exit it from the system tray and retry."
    }
}

if (!(Test-Path -LiteralPath $application -PathType Leaf)) {
    throw "The executable was not found after the build: $application"
}

Write-Host "Deploying Qt runtime dependencies..." -ForegroundColor Cyan
$deployCommand = 'call "{0}" -arch=x64 -host_arch=x64 >nul && "{1}" --release --compiler-runtime "{2}"' -f `
    $vsDevCmd, $deployTool, $application
& $env:ComSpec /d /s /c $deployCommand
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

if ($DeployOnly) {
    Write-Host "Deployment completed: $application" -ForegroundColor Green
    exit 0
}

Write-Host "Starting Vinson Editor..." -ForegroundColor Cyan
Start-Process -FilePath $application -WorkingDirectory $resolvedBuildDirectory
