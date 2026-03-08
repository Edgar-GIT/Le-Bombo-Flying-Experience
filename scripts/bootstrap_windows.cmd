@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "POWERSHELL_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

if exist "%POWERSHELL_EXE%" (
    "%POWERSHELL_EXE%" -ExecutionPolicy Bypass -File "%SCRIPT_DIR%bootstrap_windows.ps1"
    exit /b %ERRORLEVEL%
)

where pwsh >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    pwsh -ExecutionPolicy Bypass -File "%SCRIPT_DIR%bootstrap_windows.ps1"
    exit /b %ERRORLEVEL%
)

echo [bootstrap] PowerShell not found. Install Windows PowerShell or PowerShell 7.
exit /b 1
