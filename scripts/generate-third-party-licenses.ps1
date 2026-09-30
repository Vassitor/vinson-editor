param(
    [string]$QtRoot = "",
    [string]$QtVersion = "6.8.3"
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
if (!$QtRoot) {
    $QtRoot = Join-Path $repository ".build-tools/Qt/$QtVersion/msvc2022_64"
}
$destination = Join-Path $repository "licenses"
$encoding = New-Object System.Text.UTF8Encoding($false)
New-Item -ItemType Directory -Force -Path "$destination/qt", "$destination/texts" | Out-Null

# The SDK SBOM preserves upstream copyright statements and custom license text.
# Include whole module inventories as a conservative superset, not as a claim
# that every source component is linked into the application.
$modules = @('qtbase', 'qt5compat', 'qtsvg', 'qttranslations')
$ids = New-Object 'System.Collections.Generic.SortedSet[string]'
$report = New-Object System.Text.StringBuilder
[void]$report.AppendLine("# Qt $QtVersion component notices")
[void]$report.AppendLine("")
[void]$report.AppendLine('Generated from the official Qt SDK SPDX documents. This is a module-level superset, including platform-specific and build-only components. NOASSERTION means the upstream inventory makes no assertion; it is not a license grant. License alternatives are retained verbatim. Commercial Qt terms are not included or granted by this project.')

foreach ($module in $modules) {
    $source = Join-Path $QtRoot "sbom/$module-$QtVersion.spdx.json"
    $sbom = Get-Content -LiteralPath $source -Raw | ConvertFrom-Json
    Copy-Item -LiteralPath $source -Destination "$destination/qt/$module-$QtVersion.spdx.json"
    foreach ($license in $sbom.hasExtractedLicensingInfos) {
        if ($license.licenseId -ne 'LicenseRef-Qt-Commercial') {
            [IO.File]::WriteAllText("$destination/texts/$($license.licenseId).txt", $license.extractedText, $encoding)
        }
    }
    [void]$report.AppendLine("`n## $module`n")
    foreach ($package in $sbom.packages) {
        [void]$report.AppendLine("### $($package.name) ($($package.versionInfo))`n")
        [void]$report.AppendLine("Declared license: $($package.licenseDeclared)`n")
        [void]$report.AppendLine("Concluded license: $($package.licenseConcluded)`n")
        [void]$report.AppendLine("Source: $($package.downloadLocation)`n")
        [void]$report.AppendLine("Copyright:`n`n$($package.copyrightText)`n")
        [void]$report.AppendLine($package.comment)
        foreach ($expression in @($package.licenseDeclared, $package.licenseConcluded)) {
            foreach ($token in ($expression -split '[\s()]+')) {
                if ($token -and $token -notin @('AND', 'OR', 'WITH', 'NONE', 'NOASSERTION')) {
                    [void]$ids.Add($token)
                }
            }
        }
    }
}

# Pin the license-text source so regeneration does not silently change versions.
$manifest = New-Object System.Text.StringBuilder
[void]$manifest.AppendLine("# License text provenance`n")
foreach ($id in $ids) {
    if ($id -eq 'LicenseRef-Qt-Commercial') { continue }
    $path = "$destination/texts/$id.txt"
    if ($id.StartsWith('LicenseRef-')) {
        if (!(Test-Path -LiteralPath $path)) { throw "Missing custom license: $id" }
        $origin = "Qt $QtVersion SDK SPDX extractedText"
    } else {
        $origin = "https://raw.githubusercontent.com/spdx/license-list-data/v3.27.0/text/$id.txt"
        if (!(Test-Path -LiteralPath $path)) {
            Invoke-WebRequest -Uri $origin -UseBasicParsing -OutFile $path
        }
    }
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    [void]$manifest.AppendLine("- ${id}: $origin (SHA-256: $hash)")
}
[IO.File]::WriteAllText("$destination/qt/NOTICES.md", $report.ToString(), $encoding)
[IO.File]::WriteAllText("$destination/SOURCES.md", $manifest.ToString(), $encoding)
Copy-Item -LiteralPath "$repository/third_party/scintilla/License.txt" -Destination "$destination/Scintilla.txt"
Copy-Item -LiteralPath "$repository/third_party/lexilla/License.txt" -Destination "$destination/Lexilla.txt"
Write-Host "Generated third-party notices and $($ids.Count - 1) license texts in $destination"
