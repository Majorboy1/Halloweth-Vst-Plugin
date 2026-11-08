# Copies the built VST3 into a user-writable VST3 folder.
#
# Copying to "C:\Program Files\Common Files\VST3" requires administrator rights,
# so by default this uses D:\VST3. Add that folder to FL Studio's plugin search
# paths once (Options > Manage plugins > Plugin search paths).
#
# Usage:
#   .\scripts\install-vst3.ps1
#   .\scripts\install-vst3.ps1 -Destination "C:\Program Files\Common Files\VST3"

[CmdletBinding()]
param(
    [string] $Destination = "D:\VST3",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string] $Config = "Release"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot ("build-" + $Config.ToLowerInvariant() +
                                  "\HALLOWETH_artefacts\$Config\VST3\HALLOWETH.vst3")

if (-not (Test-Path $source)) {
    throw "VST3 bundle not found: $source`nRun scripts\build.ps1 first."
}

New-Item -ItemType Directory -Force -Path $Destination | Out-Null

$target = Join-Path $Destination "HALLOWETH.vst3"

if (Test-Path $target) {
    Write-Host "Replacing existing installation at $target" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $target
}

Copy-Item -Recurse -Path $source -Destination $Destination

Write-Host "Installed HALLOWETH.vst3 -> $Destination" -ForegroundColor Green
Write-Host ""
Write-Host "NOTE: FL Studio only scans the shared VST3 folder" -ForegroundColor Yellow
Write-Host "      (C:\Program Files\Common Files\VST3). This copy is for hosts that"
Write-Host "      scan custom VST3 folders."
Write-Host ""
Write-Host "For FL Studio, run scripts\install-vst3-system.ps1 as administrator." -ForegroundColor Cyan
