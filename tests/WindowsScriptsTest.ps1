# Run with Windows PowerShell 5.1 or PowerShell 7; no SDK or downloads required.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $repositoryRoot 'scripts/windows-tools.ps1')

function Assert-Equal {
    param($Actual, $Expected, [string]$Message)
    if ($Actual -ne $Expected) { throw "$Message (expected '$Expected', got '$Actual')" }
}

function New-QtFixture {
    param([string]$Root, [switch]$LinguistTools)
    $files = @('lib/cmake/Qt6/Qt6Config.cmake',
        'lib/cmake/Qt6Core5Compat/Qt6Core5CompatConfig.cmake',
        'lib/cmake/Qt6Qml/Qt6QmlConfig.cmake', 'bin/windeployqt.exe')
    if ($LinguistTools) { $files += 'lib/cmake/Qt6LinguistTools/Qt6LinguistToolsConfig.cmake' }
    foreach ($file in $files) {
        $path = Join-Path $Root $file
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
        [IO.File]::WriteAllText($path, '')
    }
}

$testRoot = Join-Path $repositoryRoot ('build/script-tests/' + [guid]::NewGuid().ToString())
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
$previousQtDir = $env:QTDIR
$previousPrefixPath = $env:CMAKE_PREFIX_PATH
try {
    $env:QTDIR = $null
    $env:CMAKE_PREFIX_PATH = $null
    $toolsRoot = Join-Path $testRoot 'tools with spaces'
    $olderQt = Join-Path $toolsRoot 'Qt/6.9.3/msvc2022_64'
    $newerQt = Join-Path $toolsRoot 'Qt/6.10.2/msvc2022_64'
    New-QtFixture -Root $olderQt -LinguistTools
    New-QtFixture -Root $newerQt -LinguistTools
    $olderQt = (Resolve-Path -LiteralPath $olderQt).Path
    $newerQt = (Resolve-Path -LiteralPath $newerQt).Path
    Assert-Equal (Find-QtRoot -BuildToolsRoot $toolsRoot) $newerQt 'Use installed Qt when the default version is absent; sort versions numerically'

    $buildDirectory = Join-Path $testRoot 'build with spaces'
    New-Item -ItemType Directory -Path $buildDirectory | Out-Null
    $cache = Join-Path $buildDirectory 'CMakeCache.txt'
    [IO.File]::WriteAllText($cache, "Qt6_DIR:PATH=$($olderQt.Replace('\', '/'))/lib/cmake/Qt6`n")
    $env:QTDIR = $newerQt
    Assert-Equal (Find-QtRoot -BuildDirectory $buildDirectory -BuildToolsRoot $toolsRoot) $olderQt 'Use the same SDK as the configured build'
    Assert-Equal (Find-QtRoot -ExplicitRoot $newerQt -BuildDirectory $buildDirectory -BuildToolsRoot $toolsRoot) $newerQt 'Explicit Qt root takes priority'
    $invalidRootRejected = $false
    try { Find-QtRoot -ExplicitRoot (Join-Path $testRoot 'missing') -BuildToolsRoot $toolsRoot | Out-Null }
    catch { $invalidRootRejected = $_.Exception.Message -like '*specified Qt root*' }
    Assert-Equal $invalidRootRejected $true 'Invalid explicit roots must fail instead of silently selecting another SDK'

    $minimalQt = Join-Path $testRoot 'qt without translations'
    New-QtFixture -Root $minimalQt
    Assert-Equal (Test-QtRoot -Candidate $minimalQt) $true 'Deployment does not require translation build tools'
    Assert-Equal (Test-QtRoot -Candidate $minimalQt -RequireLinguistTools) $false 'Packaging requires translation build tools'

    $legacyCmake = Join-Path $toolsRoot 'python-packages/cmake/data/bin/cmake.exe'
    $legacyNinja = Join-Path $toolsRoot 'python-packages/bin/ninja.exe'
    foreach ($path in @($legacyCmake, $legacyNinja)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
        [IO.File]::WriteAllText($path, '')
    }
    Assert-Equal (Resolve-Executable -Name cmake.exe -Candidates @(Get-LocalToolCandidates -BuildToolsRoot $toolsRoot -Tool cmake)) `
        (Resolve-Path -LiteralPath $legacyCmake).Path 'Reuse the previous python-packages tool layout'
    Assert-Equal (Resolve-Executable -Name ninja.exe -Candidates @(Get-LocalToolCandidates -BuildToolsRoot $toolsRoot -Tool ninja)) `
        (Resolve-Path -LiteralPath $legacyNinja).Path 'Reuse the previous Ninja layout'

    foreach ($script in Get-ChildItem (Join-Path $repositoryRoot 'scripts') -Filter '*.ps1') {
        $tokens = $null
        $parseErrors = $null
        [void][Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$tokens, [ref]$parseErrors)
        if ($parseErrors.Count) { throw "$($script.Name): $($parseErrors.Message -join '; ')" }
    }
    Write-Host 'Windows script discovery checks passed.'
} finally {
    $env:QTDIR = $previousQtDir
    $env:CMAKE_PREFIX_PATH = $previousPrefixPath
    $resolvedTestRoot = (Resolve-Path -LiteralPath $testRoot).Path
    $expectedParent = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'build/script-tests')).TrimEnd('\') + '\'
    if (!$resolvedTestRoot.StartsWith($expectedParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean fixtures outside $expectedParent"
    }
    Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
}
