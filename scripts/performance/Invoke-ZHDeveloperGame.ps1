#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Name = 'stage4b1-devmode-pathfinding',
    [ValidateSet('none','battle')][string]$Preset = 'none',
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
# Fresh output per invocation; validation only never creates output or launches.
$output = Join-Path $repo ('build/performance/dev-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$arguments = @('-devMode','-quickGame','-groundInterpolation','-performanceProfile',$output,'-pathProfile')
if ($Preset -ne 'none') { $arguments += @('-devPreset',$Preset) }
# Validate guards BEFORE creating the output directory or performing a backup.
& "$repo/scripts/zh-runtime/Launch-ZHRuntime.ps1" -Name $Name -GameArguments $arguments -BackupUserData -ValidateOnly
Write-Host "Developer capture output: $output"
if ($ValidateOnly) { return }
$null = New-Item -ItemType Directory -Path $output
& "$repo/scripts/zh-runtime/Launch-ZHRuntime.ps1" -Name $Name -GameArguments $arguments -BackupUserData
