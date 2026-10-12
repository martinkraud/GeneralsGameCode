#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,[Parameter(Mandatory)][string]$AssetSource,[switch]$UIMenu,[switch]$UIPanel,[switch]$UISentence,[switch]$UIImage,[switch]$DisplayLifecycle)
. (Join-Path $PSScriptRoot 'GraphicsCommon.ps1')
if($UIMenu -and $Title -ne 'Generals'){throw 'UIMenu currently validates the original Generals reference only'}
if($UIMenu -and ($UIPanel -or $UISentence -or $UIImage)){throw 'UIMenu is a distinct static composition mode'}
if($UIMenu){$DisplayLifecycle=$true}
if($UIPanel){$UISentence=$true}
if($UISentence){$UIImage=$true}
if($UIImage){$DisplayLifecycle=$true}
# Reuse exact asset isolation/copy/inventory rules; never overwrite an old stage.
$build=Get-NativeBuild $Title
$null=Assert-NativeGraphicsDependencies $build.Executable (Split-Path $build.Executable -Parent)
& (Join-Path $PSScriptRoot 'Stage-NativeInitOnly.ps1') -Title $Title -Name $Name -AssetSource $AssetSource
$manifest=Read-NativeStage $Title $Name
$graphics=[pscustomobject]@{
    Mode=(Get-NativeGraphicsMode -DisplayLifecycle:$DisplayLifecycle -UIImage:$UIImage -UISentence:$UISentence -UIPanel:$UIPanel -UIMenu:$UIMenu);StagePath=$manifest.StagePath
    ManifestSHA256=(Get-FileHash -LiteralPath (Join-Path $manifest.StagePath 'manifest.json')).Hash
    Dependencies=@(Assert-NativeGraphicsDependencies (Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))) $manifest.GamePath)
}
Write-ZHJson (Join-Path $manifest.StagePath 'graphics.json') $graphics
$null=Read-NativeGraphicsStage $Title $Name -DisplayLifecycle:$DisplayLifecycle -UIImage:$UIImage -UISentence:$UISentence -UIPanel:$UIPanel -UIMenu:$UIMenu
Write-Host "Graphics stage/preflight PASS; no game launched: $($manifest.StagePath)"
