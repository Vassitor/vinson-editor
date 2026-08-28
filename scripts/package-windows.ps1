param(
    [string]$Preset = "release"
)

$ErrorActionPreference = "Stop"

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

Invoke-Checked -Program cmake -Arguments @("--preset", $Preset)
Invoke-Checked -Program cmake -Arguments @("--build", "--preset", $Preset)
Invoke-Checked -Program ctest -Arguments @("--preset", $Preset)
Invoke-Checked -Program cmake -Arguments @("--build", "--preset", "release-package")

Write-Host "Portable archive and SHA-256 checksum are in build/$Preset."
