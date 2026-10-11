#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name)
. (Join-Path $PSScriptRoot 'Common.ps1')
$manifest=Read-NativeStage $Title $Name
Write-Host "PASS: AMD64 EXE/PDB, assets, hashes and startup DLL preflight: $($manifest.GamePath)"
Write-Host 'No game process launched. User data and dumps will be isolated under a fresh stage/runs directory.'
