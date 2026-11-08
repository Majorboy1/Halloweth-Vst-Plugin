# Builds the HALLOWETH VST3 plugin and standalone app with MSVC.
#
# Usage:
#   .\scripts\build.ps1                 # Release build
#   .\scripts\build.ps1 -Config Debug   # Debug build
#   .\scripts\build.ps1 -Clean          # wipe build folder first
#
# Requires: Visual Studio 2022 Build Tools (C++ workload) and CMake 3.22+.

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string] $Config = "Release",
    [switch] $Clean
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot ("build-" + $Config.ToLowerInvariant())
$sfizzDir = Join-Path $projectRoot "ThirdParty\sfizz"

$git = Get-Command git -ErrorAction SilentlyContinue

if (-not $git) {
    throw "Git is required to initialize the pinned sfizz submodule."
}

if (-not (Test-Path (Join-Path $sfizzDir "CMakeLists.txt"))) {
    & $git.Source -C $projectRoot submodule update --init -- ThirdParty/sfizz
    if ($LASTEXITCODE -ne 0) { throw "Could not initialize ThirdParty/sfizz." }
}

$sfizzDependencies = @(
    "external/abseil-cpp",
    "external/filesystem",
    "external/invoke.hpp",
    "external/simde",
    "external/st_audiofile/thirdparty/dr_libs",
    "external/st_audiofile/thirdparty/libaiff",
    "external/st_audiofile/thirdparty/wavpack"
)

& $git.Source -C $sfizzDir submodule update --init -- $sfizzDependencies
if ($LASTEXITCODE -ne 0) { throw "Could not initialize sfizz build dependencies." }

function Resolve-CMake {
    $candidates = @(
        "D:\devtools\halloweth\cmake\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return $candidate }
    }

    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    throw "CMake not found. Install CMake 3.22+ or place it in D:\devtools\halloweth\cmake."
}

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing $buildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

$cmake = Resolve-CMake
Write-Host "CMake:    $cmake"
Write-Host "Build:    $buildDir ($Config)" -ForegroundColor Cyan

& $cmake -S $projectRoot -B $buildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

& $cmake --build $buildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$artefacts = Join-Path $buildDir "HALLOWETH_artefacts\$Config"
Write-Host ""
Write-Host "Build succeeded." -ForegroundColor Green
Write-Host "  VST3:       $artefacts\VST3\HALLOWETH.vst3"
Write-Host "  Standalone: $artefacts\Standalone\HALLOWETH.exe"
