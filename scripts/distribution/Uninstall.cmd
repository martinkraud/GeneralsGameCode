@echo off
rem MKRaudstein Developer Preview - double-click to uninstall (restores the original files). Works from the package folder and from <game folder>\MKRaudstein.
rem The scripts run from a temporary copy so the MKRaudstein folder can be deleted while they run.
title MKRaudstein Developer Preview uninstaller
rem a PowerShell 7 parent console would leak its module path into Windows PowerShell 5.1 and hide its built-in commands
set "PSModulePath="
set "MKR_TMP=%TEMP%\mkraudstein-uninstall-%RANDOM%"
mkdir "%MKR_TMP%" >nul 2>&1
copy /y "%~dp0tools\*.ps1" "%MKR_TMP%\" >nul
if exist "%~dp0package-manifest.json" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%MKR_TMP%\Uninstall-MKRaudstein.ps1" -PackageRoot "%~dp0." -Pause %* & exit /b
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%MKR_TMP%\Uninstall-MKRaudstein.ps1" -GameFolder "%~dp0.." -Pause %* & exit /b
)
