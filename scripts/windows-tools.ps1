# Shared discovery for repository-local tools and existing Windows SDKs.
function Resolve-Executable {
    param([string]$Name, [string[]]$Candidates = @())
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $command) { return $command.Source }
    return $null
}

function Add-PathDirectory {
    param([string]$Directory)
    if (!$Directory) { return }
    $resolved = (Resolve-Path -LiteralPath $Directory).Path
    if (($env:PATH -split [IO.Path]::PathSeparator) -notcontains $resolved) {
        $env:PATH = "$resolved$([IO.Path]::PathSeparator)$env:PATH"
    }
}

function Get-LocalToolCandidates {
    param([string]$BuildToolsRoot, [ValidateSet('cmake', 'ninja')][string]$Tool)
    foreach ($directory in @('python', 'python-packages')) {
        $root = Join-Path $BuildToolsRoot $directory
        # Prefer real binaries over pip launchers tied to a relocated Python.
        if ($Tool -eq 'cmake') { Join-Path $root 'cmake\data\bin\cmake.exe' }
        Join-Path $root "bin\$Tool.exe"
        Join-Path $root "$Tool.exe"
    }
}

function Get-CachedQtRoot {
    param([string]$BuildDirectory)
    if (!$BuildDirectory) { return $null }
    $cache = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (!(Test-Path -LiteralPath $cache -PathType Leaf)) { return $null }
    foreach ($line in Get-Content -LiteralPath $cache) {
        if ($line -match '^Qt6_DIR:[^=]+=(.+)[/\\]lib[/\\]cmake[/\\]Qt6[/\\]?$') {
            return $Matches[1]
        }
    }
    return $null
}

function Test-QtRoot {
    param([string]$Candidate, [switch]$RequireLinguistTools)
    if (!$Candidate) { return $false }
    $files = @('lib\cmake\Qt6\Qt6Config.cmake',
        'lib\cmake\Qt6Core5Compat\Qt6Core5CompatConfig.cmake',
        'lib\cmake\Qt6Qml\Qt6QmlConfig.cmake',
        'bin\windeployqt.exe')
    if ($RequireLinguistTools) {
        $files += 'lib\cmake\Qt6LinguistTools\Qt6LinguistToolsConfig.cmake'
    }
    foreach ($file in $files) {
        if (!(Test-Path -LiteralPath (Join-Path $Candidate $file) -PathType Leaf)) {
            return $false
        }
    }
    return $true
}

function Find-QtRoot {
    param([string]$ExplicitRoot, [string]$BuildDirectory,
        [string]$BuildToolsRoot, [string]$PreferredVersion = '6.8.3',
        [switch]$RequireLinguistTools)
    if ($ExplicitRoot) {
        if (!(Test-QtRoot -Candidate $ExplicitRoot -RequireLinguistTools:$RequireLinguistTools)) {
            throw "The specified Qt root is missing required MSVC x64 Qt components: $ExplicitRoot"
        }
        return (Resolve-Path -LiteralPath $ExplicitRoot).Path
    }
    $candidates = @((Get-CachedQtRoot -BuildDirectory $BuildDirectory), $env:QTDIR)
    if ($env:CMAKE_PREFIX_PATH) { $candidates += $env:CMAKE_PREFIX_PATH -split ';' }
    $portableRoot = Join-Path $BuildToolsRoot 'Qt'
    $candidates += Join-Path $portableRoot "$PreferredVersion\msvc2022_64"
    foreach ($base in @($portableRoot, 'C:\Qt', 'D:\Qt')) {
        if (Test-Path -LiteralPath $base -PathType Container) {
            $versions = Get-ChildItem -LiteralPath $base -Directory |
                Where-Object { $_.Name -match '^6\.\d+\.\d+$' } |
                Sort-Object { [version]$_.Name } -Descending
            foreach ($version in $versions) {
                $candidates += Join-Path $version.FullName 'msvc2022_64'
            }
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-QtRoot -Candidate $candidate -RequireLinguistTools:$RequireLinguistTools) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Enter-MsvcEnvironment {
    if (Resolve-Executable -Name 'cl.exe') { return }
    $installationPath = $null
    $vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vsWhere -PathType Leaf) {
        $installationPath = & $vsWhere -latest -products '*' -version '[17.0,)' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
            Select-Object -First 1
    }
    if (!$installationPath -and $env:VSINSTALLDIR) { $installationPath = $env:VSINSTALLDIR }
    if (!$installationPath) {
        throw 'Visual Studio 2022 or newer with the Desktop development with C++ workload was not found.'
    }
    $developerShell = Join-Path $installationPath 'Common7\Tools\Launch-VsDevShell.ps1'
    if (!(Test-Path -LiteralPath $developerShell -PathType Leaf)) {
        throw "Visual Studio Developer PowerShell was not found at $developerShell."
    }
    & $developerShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
    if (!(Resolve-Executable -Name 'cl.exe')) {
        throw 'The Visual Studio environment was loaded, but the x64 C++ compiler is unavailable.'
    }
}
