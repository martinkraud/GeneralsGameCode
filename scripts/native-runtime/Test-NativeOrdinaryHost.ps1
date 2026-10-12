#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals')][string]$Title,[Parameter(Mandatory)][string]$Name)
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$manifest=Read-NativeOrdinaryHostStage $Title $Name
Write-Host "PASS: AMD64 EXE/PDB, assets, hashes, D3D11/compiler/DXGI dependencies, no D3D8/D3DX imports: $($manifest.GamePath)"
Write-Host 'No game process launched. An ordinary-host run requires separate developer review.'
