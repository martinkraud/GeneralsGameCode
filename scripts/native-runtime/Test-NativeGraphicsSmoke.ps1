#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,[Parameter(Mandatory)][string]$Name,[switch]$UIImage,[switch]$DisplayLifecycle)
. (Join-Path $PSScriptRoot 'GraphicsCommon.ps1')
if($UIImage){$DisplayLifecycle=$true}
$manifest=Read-NativeGraphicsStage $Title $Name -DisplayLifecycle:$DisplayLifecycle -UIImage:$UIImage
Write-Host "PASS: AMD64 EXE/PDB, assets, hashes, D3D11/compiler/DXGI dependencies: $($manifest.GamePath)"
Write-Host 'No game process launched. A graphics run requires separate developer review.'
