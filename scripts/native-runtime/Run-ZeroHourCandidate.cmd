@echo off
rem X64.45 Zero Hour native candidate launcher (double-click). Snapshots the Zero Hour settings files, then starts the staged
rem candidate exactly like a player would. Pass -Name <candidate> or -Endurance (telemetry) as needed.
setlocal
cd /d "%~dp0..\.."
pwsh -NoProfile -ExecutionPolicy Bypass -File "scripts\native-runtime\Run-ZeroHourCandidate.ps1" %*
echo.
pause
