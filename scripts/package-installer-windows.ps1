# Build and test the application, deploy Qt, then create an NSIS installer.
# Requires Visual Studio C++ tools and NSIS 3.03 or newer.
param(
    [ValidateSet("release")]
    [string]$Preset = "release",
    [string]$QtRoot = "",
    [string]$QtVersion = "6.8.3",
    [string]$NsisRoot = "",
    [switch]$NoBootstrap
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

& (Join-Path $PSScriptRoot "package-windows.ps1") `
    -Preset $Preset -QtRoot $QtRoot -QtVersion $QtVersion `
    -Generator NSIS -NsisRoot $NsisRoot -NoBootstrap:$NoBootstrap
