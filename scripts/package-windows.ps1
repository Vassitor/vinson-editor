param(
    [string]$Preset = "release",
    [string]$QtRoot = "",
    [string]$QtVersion = "6.8.3",
    [ValidateSet("ZIP", "NSIS")]
    [string]$Generator = "ZIP",
    [string]$NsisRoot = "",
    [switch]$NoBootstrap
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot "windows-tools.ps1")

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

function Find-Python {
    $candidates = @(
        @{ Program = (Join-Path $buildToolsRoot "Python313\python.exe"); Prefix = @() },
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
    $cmakeCandidates = @(Get-LocalToolCandidates -BuildToolsRoot $buildToolsRoot -Tool cmake)
    $ninjaCandidates = @(Get-LocalToolCandidates -BuildToolsRoot $buildToolsRoot -Tool ninja)

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

function Install-PortableQt {
    if ($NoBootstrap) {
        throw "Qt 6.5+ with Core5Compat, Qml and LinguistTools was not found. Install it, pass -QtRoot, or rerun without -NoBootstrap."
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
    $resolved = Find-QtRoot -ExplicitRoot $QtRoot -BuildDirectory (Join-Path $repositoryRoot "build\$Preset") `
        -BuildToolsRoot $buildToolsRoot -PreferredVersion $QtVersion -RequireLinguistTools
    if (!$resolved) {
        Install-PortableQt
        $resolved = Find-QtRoot -BuildToolsRoot $buildToolsRoot -PreferredVersion $QtVersion -RequireLinguistTools
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
    param([string]$BuildDirectory, [string]$Extension = "zip")

    $archive = Get-ChildItem -LiteralPath $BuildDirectory -Filter "*.$Extension" -File |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $archive) {
        throw "CPack completed but no .$Extension package was found in $BuildDirectory."
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

function Install-PortableNsis {
    if ($NoBootstrap) {
        throw "NSIS 3.03+ was not found. Pass -NsisRoot or rerun without -NoBootstrap to download a repository-local copy."
    }
    # Pinned official release and SHA-256 from the SourceForge download page:
    # https://sourceforge.net/projects/nsis/files/NSIS%203/3.12/nsis-3.12.zip/download
    $version = "3.12"
    $expectedHash = "56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f"
    $downloads = Join-Path $buildToolsRoot "downloads"
    $nsisDirectory = Join-Path $buildToolsRoot "nsis"
    New-Item -ItemType Directory -Force -Path $downloads, $nsisDirectory | Out-Null
    $archive = Join-Path $downloads "nsis-$version.zip"
    if (!(Test-Path -LiteralPath $archive -PathType Leaf) -or
        (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
        Write-Step "Downloading portable NSIS $version"
        $previousProtocol = [Net.ServicePointManager]::SecurityProtocol
        try {
            [Net.ServicePointManager]::SecurityProtocol = $previousProtocol -bor [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -UseBasicParsing -Uri "https://downloads.sourceforge.net/project/nsis/NSIS%203/$version/nsis-$version.zip" `
                -OutFile $archive
            # SourceForge can return an HTML download page with a timed redirect
            # instead of an HTTP redirect. Follow only its HTTPS download hosts.
            for ($redirect = 0; $redirect -lt 2; $redirect++) {
                if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -eq $expectedHash) { break }
                $page = Get-Content -LiteralPath $archive -Raw
                $match = [regex]::Match($page, 'http-equiv="refresh" content="[0-9]+; url=([^"]+)"')
                if (!$match.Success) { break }
                $url = [uri][Net.WebUtility]::HtmlDecode($match.Groups[1].Value)
                if ($url.Scheme -ne 'https' -or
                    !($url.Host -eq 'downloads.sourceforge.net' -or $url.Host.EndsWith('.dl.sourceforge.net'))) {
                    throw "The NSIS download page returned an unexpected redirect. Pass -NsisRoot to use an existing installation."
                }
                Invoke-WebRequest -UseBasicParsing -Uri $url.AbsoluteUri -OutFile $archive
            }
        } finally {
            [Net.ServicePointManager]::SecurityProtocol = $previousProtocol
        }
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
        throw "SHA-256 verification failed for the NSIS download: $archive"
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $nsisDirectory -Force
    $compiler = Join-Path $nsisDirectory "nsis-$version\makensis.exe"
    if (!(Test-Path -LiteralPath $compiler -PathType Leaf)) {
        throw "The NSIS archive did not contain makensis.exe: $compiler"
    }
    return $compiler
}

Push-Location $repositoryRoot
try {
    if ($env:OS -ne "Windows_NT") {
        throw "This script supports Windows only."
    }

    if ($Generator -eq "NSIS") {
        $nsisCandidates = @(
            (Join-Path $buildToolsRoot "nsis\makensis.exe"),
            (Join-Path $buildToolsRoot "nsis\nsis-3.12\makensis.exe"),
            (Join-Path ${env:ProgramFiles(x86)} "NSIS\makensis.exe"),
            (Join-Path $env:ProgramFiles "NSIS\makensis.exe")
        )
        if ($NsisRoot) {
            $nsisCandidates = @((Join-Path $NsisRoot "makensis.exe"))
            if (!(Test-Path -LiteralPath $nsisCandidates[0] -PathType Leaf)) {
                throw "makensis.exe was not found in -NsisRoot: $NsisRoot"
            }
        }
        $nsis = Resolve-Executable -Name "makensis.exe" -Candidates $nsisCandidates
        if (!$nsis) {
            $nsis = Install-PortableNsis
        }
        Add-PathDirectory -Directory (Split-Path -Parent $nsis)
        Write-Host "NSIS: $(& $nsis /VERSION)"
        if ($LASTEXITCODE -ne 0) { throw "Could not run the NSIS compiler." }
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
    Invoke-Checked -Program $tools.CMake -Arguments @("--preset", $Preset,
        "-DCMAKE_PREFIX_PATH=$resolvedQtRoot", "-DQt6_DIR=$resolvedQtRoot/lib/cmake/Qt6")

    Write-Step "Building $Preset"
    Invoke-Checked -Program $tools.CMake -Arguments @("--build", "--preset", $Preset)

    Write-Step "Running the complete test suite"
    Invoke-Checked -Program $tools.CTest -Arguments @("--preset", $Preset)

    $buildDirectory = Join-Path $repositoryRoot "build\$Preset"
    $packageDirectory = New-PackageOutputDirectory -BuildDirectory $buildDirectory
    Write-Step "Creating the $Generator package"
    $packageArguments = @(
        "--config", (Join-Path $buildDirectory "CPackConfig.cmake"),
        "-G", $Generator,
        "-C", "Release",
        "-B", $packageDirectory
    )
    if ($Generator -eq "NSIS") {
        $packageArguments += @("-D", "CPACK_NSIS_EXECUTABLE=$nsis")
    }
    Invoke-Checked -Program $tools.CPack -Arguments $packageArguments

    $extension = if ($Generator -eq "NSIS") { "exe" } else { "zip" }
    $result = Confirm-PackageChecksum -BuildDirectory $packageDirectory -Extension $extension
    Remove-PackageStagingDirectory -PackageDirectory $packageDirectory
    Write-Step "Package complete"
    Write-Host "Archive: $($result.Archive)"
    Write-Host "Checksum: $($result.ChecksumFile)"
    Write-Host "SHA-256: $($result.SHA256)"
} finally {
    Pop-Location
}
