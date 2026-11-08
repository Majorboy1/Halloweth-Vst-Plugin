@echo off
REM Installs HALLOWETH into the shared VST3 folder that FL Studio scans.
REM Just double-click this file and click "Yes" on the Windows administrator prompt.

setlocal
set "INSTALLER=%~dp0install-vst3-system.ps1"

if not exist "%INSTALLER%" (
    echo.
    echo ERROR: installer not found:
    echo   %INSTALLER%
    echo.
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%INSTALLER%"

echo.
pause
