#Requires -Version 7.0
[CmdletBinding()]
param([Alias('WhatIf')][switch]$ValidateOnly)
. "$PSScriptRoot/Common.ps1"
Assert-ZHNoGameProcess
$data = Get-ZHUserData
Invoke-ZHUserDataBackup $data -ValidateOnly:$ValidateOnly
