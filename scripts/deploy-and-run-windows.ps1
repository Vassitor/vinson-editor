param(
    [string]$BuildDirectory = "",
    [string]$QtRoot = "",
    [ValidateRange(1, 1024)]
    [int]$BuildJobs = 4,
    [switch]$SkipBuild,
    [switch]$DeployOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot "windows-tools.ps1")

if ($env:OS -ne "Windows_NT") { throw "This script supports Windows only." }

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildToolsRoot = Join-Path $repositoryRoot ".build-tools"
if (!$BuildDirectory) {
    $BuildDirectory = Join-Path $repositoryRoot "build\release"
}

if (!(Test-Path -LiteralPath $BuildDirectory -PathType Container)) {
    throw "Build directory was not found: $BuildDirectory"
}
$resolvedBuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$resolvedQtRoot = Find-QtRoot -ExplicitRoot $QtRoot -BuildDirectory $resolvedBuildDirectory `
    -BuildToolsRoot $buildToolsRoot
if (!$resolvedQtRoot) {
    throw "Qt with Core5Compat, Qml and windeployqt was not found. Pass -QtRoot or run scripts/package-windows.ps1 to prepare the tools."
}
Write-Host "Qt: $resolvedQtRoot"
Add-PathDirectory -Directory (Join-Path $resolvedQtRoot "bin")
$application = Join-Path $resolvedBuildDirectory "vinson-editor.exe"
$deployTool = Join-Path $resolvedQtRoot "bin\windeployqt.exe"

if (!(Test-Path -LiteralPath $deployTool -PathType Leaf)) {
    throw "windeployqt was not found: $deployTool"
}

Enter-MsvcEnvironment

if (!$SkipBuild) {
    $cmake = Resolve-Executable -Name "cmake.exe" -Candidates @(
        Get-LocalToolCandidates -BuildToolsRoot $buildToolsRoot -Tool cmake)
    if (!$cmake) {
        throw "CMake was not found. Install it or restore the repository build tools."
    }

    Write-Host "Building the latest Release executable..." -ForegroundColor Cyan
    $ninja = Resolve-Executable -Name "ninja.exe" -Candidates @(
        Get-LocalToolCandidates -BuildToolsRoot $buildToolsRoot -Tool ninja)
    if ($ninja) { Add-PathDirectory -Directory (Split-Path -Parent $ninja) }
    & $cmake --build $resolvedBuildDirectory --target vinson-editor -j $BuildJobs
    if ($LASTEXITCODE -ne 0) {
        throw "Release build failed with exit code $LASTEXITCODE. If Vinson Editor is running, exit it from the system tray and retry."
    }
}

if (!(Test-Path -LiteralPath $application -PathType Leaf)) {
    throw "The executable was not found after the build: $application"
}

Write-Host "Deploying Qt runtime dependencies..." -ForegroundColor Cyan
& $deployTool --release --compiler-runtime $application
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

if ($DeployOnly) {
    Write-Host "Deployment completed: $application" -ForegroundColor Green
    return
}

Write-Host "Starting Vinson Editor..." -ForegroundColor Cyan
Start-Process -FilePath $application -WorkingDirectory $resolvedBuildDirectory
