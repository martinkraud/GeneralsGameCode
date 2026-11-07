@echo off
rem MKRaudstein Developer Preview - double-click to install. Needs only the Windows PowerShell that ships with Windows.
title MKRaudstein Developer Preview installer
rem a PowerShell 7 parent console would leak its module path into Windows PowerShell 5.1 and hide its built-in commands
set "PSModulePath="
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Install-MKRaudstein.ps1" -Pause %*
