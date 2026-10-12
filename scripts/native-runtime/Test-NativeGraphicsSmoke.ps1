#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,[Parameter(Mandatory)][string]$Name)
. (Join-Path $PSScriptRoot 'GraphicsCommon.ps1')
$manifest=Read-NativeGraphicsStage $Title $Name
Write-Host "PASS: AMD64 EXE/PDB, assets, hashes, D3D11/compiler/DXGI dependencies: $($manifest.GamePath)"
Write-Host 'No game process launched. A graphics run requires separate developer review.'
