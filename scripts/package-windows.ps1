param(
    [string]$Preset = "release",
    [string]$QtRoot = "",
    [string]$QtVersion = "6.8.3",
    [switch]$NoBootstrap
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildToolsRoot = Join-Path $repositoryRoot ".build-tools"
$portablePythonRoot = Join-Path $buildToolsRoot "python"
$portableQtRoot = Join-Path $buildToolsRoot "Qt"
$qtArchitecture = "msvc2022_64"

function Write-Step {
    param([string]$Message)

    Write-Host "`n==> $Message" -ForegroundColor Cyan
}

function Invoke-Checked {
    param(
        [string]$Program,
        [string[]]$Arguments
    )

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

function Resolve-Executable {
    param(
        [string]$Name,
        [string[]]$Candidates = @()
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $command) {
        return $command.Source
    }
    return $null
}

function Add-PathDirectory {
    param([string]$Directory)

    if (!$Directory) {
        return
    }
    $resolved = (Resolve-Path -LiteralPath $Directory).Path
    $entries = $env:PATH -split [IO.Path]::PathSeparator
    if ($entries -notcontains $resolved) {
        $env:PATH = "$resolved$([IO.Path]::PathSeparator)$env:PATH"
    }
}

function Find-Python {
    $candidates = @(
        @{ Program = "py.exe"; Prefix = @("-3") },
        @{ Program = "python.exe"; Prefix = @() },
        @{ Program = "python3.exe"; Prefix = @() }
    )
    foreach ($candidate in $candidates) {
        $program = Resolve-Executable -Name $candidate.Program
        if (!$program) {
            continue
        }
        & $program @($candidate.Prefix) -c "import sys; assert sys.version_info >= (3, 9)" 2>$null
        if ($LASTEXITCODE -eq 0) {
            return @{
                Program = $program
                Prefix = [string[]]$candidate.Prefix
            }
        }
    }
    return $null
}

function Install-PortableBuildTools {
    if ($NoBootstrap) {
        throw "CMake or Ninja was not found. Install it or rerun without -NoBootstrap."
    }

    $python = Find-Python
    if ($null -eq $python) {
        throw "Python 3.9+ is required once to bootstrap CMake, Ninja, and Qt. Install Python or provide the tools on PATH."
    }

    Write-Step "Installing portable CMake, Ninja, and the Qt downloader"
    New-Item -ItemType Directory -Force -Path $portablePythonRoot | Out-Null
    $arguments = @($python.Prefix) + @(
        "-m", "pip", "install",
        "--disable-pip-version-check",
        "--upgrade",
        "--target", $portablePythonRoot,
        "cmake", "ninja", "aqtinstall"
    )
    Invoke-Checked -Program $python.Program -Arguments $arguments
}

function Resolve-BuildTools {
    $cmakeCandidates = @(
        (Join-Path $portablePythonRoot "bin\cmake.exe"),
        (Join-Path $portablePythonRoot "cmake\data\bin\cmake.exe")
    )
    $ninjaCandidates = @(
        (Join-Path $portablePythonRoot "bin\ninja.exe"),
        (Join-Path $portablePythonRoot "ninja.exe")
    )

    $cmake = Resolve-Executable -Name "cmake.exe" -Candidates $cmakeCandidates
    $ninja = Resolve-Executable -Name "ninja.exe" -Candidates $ninjaCandidates
    if (!$cmake -or !$ninja) {
        Install-PortableBuildTools
        $cmake = Resolve-Executable -Name "cmake.exe" -Candidates $cmakeCandidates
        $ninja = Resolve-Executable -Name "ninja.exe" -Candidates $ninjaCandidates
    }
    if (!$cmake -or !$ninja) {
        throw "Portable build-tool installation completed but CMake or Ninja could not be located."
    }

    $cmakeDirectory = Split-Path -Parent $cmake
    $ctest = Join-Path $cmakeDirectory "ctest.exe"
    $cpack = Join-Path $cmakeDirectory "cpack.exe"
    if (!(Test-Path -LiteralPath $ctest -PathType Leaf) -or
        !(Test-Path -LiteralPath $cpack -PathType Leaf)) {
        throw "ctest.exe and cpack.exe must be installed beside cmake.exe."
    }

    Add-PathDirectory -Directory $cmakeDirectory
    Add-PathDirectory -Directory (Split-Path -Parent $ninja)
    return @{
        CMake = $cmake
        CPack = $cpack
        CTest = $ctest
        Ninja = $ninja
    }
}

function Enter-MsvcEnvironment {
    if (Resolve-Executable -Name "cl.exe") {
        return
    }

    $installationPath = $null
    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vsWhere -PathType Leaf) {
        $installationPath = (& $vsWhere -latest -products "*" -property installationPath |
            Select-Object -First 1)
    }
    if (!$installationPath -and $env:VSINSTALLDIR) {
        $installationPath = $env:VSINSTALLDIR.TrimEnd("\")
    }
    if (!$installationPath) {
        throw "Visual Studio 2022 with the Desktop development with C++ workload was not found."
    }

    $developerShell = Join-Path $installationPath "Common7\Tools\Launch-VsDevShell.ps1"
    if (!(Test-Path -LiteralPath $developerShell -PathType Leaf)) {
        throw "Visual Studio Developer PowerShell was not found at $developerShell."
    }

    Write-Step "Initializing the Visual Studio x64 build environment"
    & $developerShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
    if (!(Resolve-Executable -Name "cl.exe")) {
        throw "The Visual Studio environment was loaded, but the x64 C++ compiler is unavailable."
    }
}

function Get-CompilerBanner {
    # cl.exe writes its normal version banner to stderr and exits with code 2
    # when invoked without an input file. Windows PowerShell 5.1 converts that
    # stderr text into a NativeCommandError when ErrorActionPreference is Stop.
    # Run it through cmd.exe so stderr is merged before PowerShell receives it.
    $commandProcessor = Resolve-Executable -Name "cmd.exe" -Candidates @(
        $env:ComSpec)
    if (!$commandProcessor) {
        throw "cmd.exe is required to query the MSVC compiler version."
    }

    $output = & $commandProcessor /d /c "cl.exe 2>&1"
    $banner = $output |
        Where-Object { ![string]::IsNullOrWhiteSpace($_) } |
        Select-Object -First 1
    if (!$banner) {
        throw "The MSVC compiler was found, but its version could not be read."
    }
    return $banner.Trim()
}

function Test-QtRoot {
    param([string]$Candidate)

    if (!$Candidate) {
        return $false
    }
    return (Test-Path -LiteralPath (Join-Path $Candidate "lib\cmake\Qt6\Qt6Config.cmake") -PathType Leaf) `
        -and (Test-Path -LiteralPath (Join-Path $Candidate "lib\cmake\Qt6Core5Compat\Qt6Core5CompatConfig.cmake") -PathType Leaf) `
        -and (Test-Path -LiteralPath (Join-Path $Candidate "lib\cmake\Qt6LinguistTools\Qt6LinguistToolsConfig.cmake") -PathType Leaf) `
        -and (Test-Path -LiteralPath (Join-Path $Candidate "bin\windeployqt.exe") -PathType Leaf)
}

function Find-QtRoot {
    param([string]$ExplicitRoot)

    $candidates = [Collections.Generic.List[string]]::new()
    foreach ($candidate in @(
        $ExplicitRoot,
        $env:QTDIR,
        (Join-Path $portableQtRoot "$QtVersion\$qtArchitecture")
    )) {
        if ($candidate) {
            $candidates.Add($candidate)
        }
    }

    if ($env:CMAKE_PREFIX_PATH) {
        foreach ($candidate in $env:CMAKE_PREFIX_PATH -split ";") {
            if ($candidate) {
                $candidates.Add($candidate)
            }
        }
    }

    foreach ($base in @("C:\Qt", "D:\Qt")) {
        if (Test-Path -LiteralPath $base -PathType Container) {
            Get-ChildItem -LiteralPath $base -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                ForEach-Object {
                    $candidates.Add((Join-Path $_.FullName $qtArchitecture))
                }
        }
    }

    foreach ($candidate in $candidates) {
        if (Test-QtRoot -Candidate $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Install-PortableQt {
    if ($NoBootstrap) {
        throw "Qt 6.5+ with Core5Compat and LinguistTools was not found. Install it, pass -QtRoot, or rerun without -NoBootstrap."
    }

    $python = Find-Python
    if ($null -eq $python) {
        throw "Python 3.9+ is required once to bootstrap Qt. Install Python or pass -QtRoot."
    }
    if (!(Test-Path -LiteralPath (Join-Path $portablePythonRoot "aqt") -PathType Container)) {
        Install-PortableBuildTools
    }

    Write-Step "Installing Qt $QtVersion for MSVC 2022 x64"
    New-Item -ItemType Directory -Force -Path $portableQtRoot | Out-Null
    $previousPythonPath = $env:PYTHONPATH
    $env:PYTHONPATH = if ($previousPythonPath) {
        "$portablePythonRoot$([IO.Path]::PathSeparator)$previousPythonPath"
    } else {
        $portablePythonRoot
    }
    Push-Location $buildToolsRoot
    try {
        $arguments = @($python.Prefix) + @(
            "-m", "aqt", "install-qt",
            "windows", "desktop", $QtVersion, "win64_msvc2022_64",
            "-O", $portableQtRoot,
            "--timeout", "30",
            "-m", "qt5compat"
        )
        Invoke-Checked -Program $python.Program -Arguments $arguments
    } finally {
        Pop-Location
        $env:PYTHONPATH = $previousPythonPath
    }
}

function Resolve-Qt {
    $resolved = Find-QtRoot -ExplicitRoot $QtRoot
    if (!$resolved) {
        Install-PortableQt
        $resolved = Find-QtRoot -ExplicitRoot $QtRoot
    }
    if (!$resolved) {
        throw "Qt installation completed but a usable Qt root could not be located."
    }

    Add-PathDirectory -Directory (Join-Path $resolved "bin")
    $prefixEntries = @($resolved)
    if ($env:CMAKE_PREFIX_PATH) {
        $prefixEntries += $env:CMAKE_PREFIX_PATH -split ";"
    }
    $env:CMAKE_PREFIX_PATH = ($prefixEntries | Select-Object -Unique) -join ";"
    return $resolved
}

function Confirm-PackageChecksum {
    param([string]$BuildDirectory)

    $archive = Get-ChildItem -LiteralPath $BuildDirectory -Filter "*.zip" -File |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $archive) {
        throw "CPack completed but no ZIP archive was found in $BuildDirectory."
    }

    $checksumPath = "$($archive.FullName).sha256"
    if (!(Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
        throw "The checksum file was not generated: $checksumPath"
    }
    $recorded = ((Get-Content -LiteralPath $checksumPath -Raw).Trim() -split "\s+")[0]
    $actual = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash
    if ($recorded -ne $actual) {
        throw "SHA-256 verification failed for $($archive.Name)."
    }

    return @{
        Archive = $archive.FullName
        ChecksumFile = $checksumPath
        SHA256 = $actual.ToLowerInvariant()
    }
}

function New-PackageOutputDirectory {
    param([string]$BuildDirectory)

    $packagesDirectory = Join-Path $BuildDirectory "packages"
    New-Item -ItemType Directory -Force -Path $packagesDirectory | Out-Null

    for ($attempt = 0; $attempt -lt 10; $attempt++) {
        $timestamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
        $outputDirectory = Join-Path $packagesDirectory $timestamp
        if (!(Test-Path -LiteralPath $outputDirectory)) {
            New-Item -ItemType Directory -Path $outputDirectory | Out-Null
            return (Resolve-Path -LiteralPath $outputDirectory).Path
        }
        Start-Sleep -Milliseconds 10
    }

    throw "Could not create a unique package output directory in $packagesDirectory."
}

function Remove-PackageStagingDirectory {
    param([string]$PackageDirectory)

    $resolvedPackageDirectory = (Resolve-Path -LiteralPath $PackageDirectory).Path
    $stagingDirectory = [IO.Path]::GetFullPath(
        (Join-Path $resolvedPackageDirectory "_CPack_Packages"))
    $expectedPrefix = $resolvedPackageDirectory.TrimEnd("\") + "\"
    if (!$stagingDirectory.StartsWith(
            $expectedPrefix,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a CPack staging directory outside $resolvedPackageDirectory."
    }
    if (!(Test-Path -LiteralPath $stagingDirectory -PathType Container)) {
        return
    }

    try {
        Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
        Write-Host "Removed temporary CPack staging directory."
    } catch {
        Write-Warning "The package is valid, but its temporary staging directory could not be removed: $($_.Exception.Message)"
    }
}

Push-Location $repositoryRoot
try {
    if ($env:OS -ne "Windows_NT") {
        throw "This script supports Windows only."
    }

    $tools = Resolve-BuildTools
    Enter-MsvcEnvironment
    $resolvedQtRoot = Resolve-Qt

    Write-Step "Toolchain"
    Write-Host "CMake: $(& $tools.CMake --version | Select-Object -First 1)"
    Write-Host "Ninja: $(& $tools.Ninja --version)"
    Write-Host "Compiler: $(Get-CompilerBanner)"
    Write-Host "Qt: $(& (Join-Path $resolvedQtRoot "bin\qmake.exe") -query QT_VERSION)"

    Write-Step "Configuring $Preset"
    Invoke-Checked -Program $tools.CMake -Arguments @("--preset", $Preset)

    Write-Step "Building $Preset"
    Invoke-Checked -Program $tools.CMake -Arguments @("--build", "--preset", $Preset)

    Write-Step "Running the complete test suite"
    Invoke-Checked -Program $tools.CTest -Arguments @("--preset", $Preset)

    $buildDirectory = Join-Path $repositoryRoot "build\$Preset"
    $packageDirectory = New-PackageOutputDirectory -BuildDirectory $buildDirectory
    Write-Step "Creating the portable package"
    Invoke-Checked -Program $tools.CPack -Arguments @(
        "--config", (Join-Path $buildDirectory "CPackConfig.cmake"),
        "-C", "Release",
        "-B", $packageDirectory
    )

    $result = Confirm-PackageChecksum -BuildDirectory $packageDirectory
    Remove-PackageStagingDirectory -PackageDirectory $packageDirectory
    Write-Step "Package complete"
    Write-Host "Archive: $($result.Archive)"
    Write-Host "Checksum: $($result.ChecksumFile)"
    Write-Host "SHA-256: $($result.SHA256)"
} finally {
    Pop-Location
}
