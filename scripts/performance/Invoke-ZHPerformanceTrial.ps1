#Requires -Version 7.0
# Child process adapter: Launch-ZHRuntime's exit cannot terminate the trial loop.
param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Directory, [switch]$ValidateOnly)
$argv = @('-performanceScenario', 'pathfinding-heavy', '-groundInterpolation', '-performanceProfile', $Directory, '-pathProfile')
& "$PSScriptRoot/../zh-runtime/Launch-ZHRuntime.ps1" -Name $Name -BackupUserData -ValidateOnly:$ValidateOnly -GameArguments $argv
