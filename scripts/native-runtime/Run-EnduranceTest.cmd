@echo off
rem X64.44 real-game endurance test launcher (double-click). Starts the staged candidate with the endurance telemetry on.
setlocal
cd /d "%~dp0..\.."
pwsh -NoProfile -ExecutionPolicy Bypass -File "scripts\native-runtime\Run-EnduranceTest.ps1" %*
echo.
pause
