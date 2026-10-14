#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals')][string]$Title,
      [Parameter(Mandatory)][string]$Name,[Parameter(Mandatory)][string]$AssetSource,
      [switch]$Classic)
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
# Reuse exact asset isolation/copy/inventory rules; never overwrite an old stage.
$build=Get-NativeBuild $Title
$null=Assert-NativeGraphicsDependencies $build.Executable (Split-Path $build.Executable -Parent)
# Normal-launch candidates carry the modern extension overlay unless -Classic is given.
& (Join-Path $PSScriptRoot 'Stage-NativeInitOnly.ps1') -Title $Title -Name $Name -AssetSource $AssetSource -ModernExtensions:(!$Classic)
$manifest=Read-NativeStage $Title $Name
$host_=[pscustomobject]@{
    Mode=(Get-NativeOrdinaryPlayMode);StagePath=$manifest.StagePath
    ManifestSHA256=(Get-FileHash -LiteralPath (Join-Path $manifest.StagePath 'manifest.json')).Hash
    Arguments=@(Get-NativeOrdinaryPlayArguments '<run-directory>')
    Dependencies=@(Assert-NativeGraphicsDependencies (Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))) $manifest.GamePath)
}
Write-ZHJson (Join-Path $manifest.StagePath 'ordinaryplay.json') $host_
$null=Read-NativeOrdinaryPlayStage $Title $Name
Write-Host "Ordinary-play stage/preflight PASS; no game launched: $($manifest.StagePath)"
