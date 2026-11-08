# Installs HALLOWETH with the plugin data on D: while FL Studio still finds it.
#
# How it works:
#   - The real bundle lives at  D:\VST3\HALLOWETH.vst3
#   - C:\Program Files\Common Files\VST3\HALLOWETH.vst3 becomes a directory
#     JUNCTION pointing at it, so FL Studio's scanner sees the plugin in the
#     shared folder while the files stay on D:
#
# Needs administrator rights, so the script relaunches itself (click Yes on UAC).
# Close FL Studio before running.

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string] $Config = "Release"
)

$ErrorActionPreference = "Stop"

$identity  = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)

if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Requesting administrator rights - click Yes on the Windows prompt..." -ForegroundColor Yellow
    Start-Process -FilePath 'powershell.exe' -Verb RunAs -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-NoExit', '-File', $PSCommandPath
    )
    return
}

$flStudio = Get-Process -Name 'FL64', 'FL', 'FLStudio' -ErrorAction SilentlyContinue

if ($flStudio) {
    Write-Host "FL Studio is running. Close it completely, then run this again." -ForegroundColor Red
    exit 1
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot ("build-" + $Config.ToLowerInvariant() +
                                  "\HALLOWETH_artefacts\$Config\VST3\HALLOWETH.vst3")

if (-not (Test-Path $source)) {
    Write-Host "VST3 bundle not found:" -ForegroundColor Red
    Write-Host "  $source"
    Write-Host "Run scripts\build.ps1 first."
    exit 1
}

$dataFolder = "D:\VST3"
$dataPath   = Join-Path $dataFolder "HALLOWETH.vst3"
$sharedPath = "C:\Program Files\Common Files\VST3\HALLOWETH.vst3"

# ---- 1. Put the real bundle on D: -------------------------------------------
if (-not (Test-Path $dataFolder)) {
    New-Item -ItemType Directory -Path $dataFolder | Out-Null
}

if (Test-Path $dataPath) {
    $existing = Get-Item $dataPath -Force

    if ($existing.Attributes.HasFlag([IO.FileAttributes]::ReparsePoint)) {
        [IO.Directory]::Delete($dataPath, $false)     # removes the link only
    }
    else {
        Remove-Item -Recurse -Force $dataPath
    }
}

Copy-Item -Recurse -Path $source -Destination $dataFolder
Write-Host "Plugin data installed to $dataPath" -ForegroundColor Green

# ---- 2. Replace the shared-folder copy with a junction ----------------------
if (Test-Path $sharedPath) {
    $existing = Get-Item $sharedPath -Force

    if ($existing.Attributes.HasFlag([IO.FileAttributes]::ReparsePoint)) {
        [IO.Directory]::Delete($sharedPath, $false)
    }
    else {
        Remove-Item -Recurse -Force $sharedPath
    }
}

New-Item -ItemType Junction -Path $sharedPath -Target $dataPath | Out-Null

# ---- 3. Verify --------------------------------------------------------------
$binary = Join-Path $sharedPath "Contents\x86_64-win\HALLOWETH.vst3"

if (-not (Test-Path $binary)) {
    Write-Host "Something went wrong: the junction does not resolve to the plugin." -ForegroundColor Red
    exit 1
}

$link = Get-Item $sharedPath -Force
$sizeKb = [math]::Round((Get-Item $binary).Length / 1KB, 0)

Write-Host ""
Write-Host "Installed successfully" -ForegroundColor Green
Write-Host "  data:   $dataPath"
Write-Host "  link:   $sharedPath  ->  $($link.Target)"
Write-Host "  binary: $sizeKb KB"
Write-Host ""
Write-Host "FL Studio:" -ForegroundColor Cyan
Write-Host "  1. Start FL Studio"
Write-Host "  2. Options > Manage plugins > Find installed plugins"
Write-Host "  3. HALLOWETH stays available; updates only replace files on D:"
