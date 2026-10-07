#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Name = 'baseline',
    [string[]]$GameArguments = @(),
    [switch]$BackupUserData,
    [switch]$AcknowledgeSharedUserData,
    [Alias('WhatIf')][switch]$ValidateOnly
)
. "$PSScriptRoot/Common.ps1"
Assert-ZHNoGameProcess
Assert-ZHGameArguments $GameArguments
$manifest = Read-ZHStage $Name
$data = Get-ZHUserData
$exe = Join-Path $manifest.GamePath 'generalszh.exe'
if (Test-ZHWithin $exe $script:ZHProtected) { throw 'Protected Steam launch refused.' }
$arguments = @('-setCwd', $manifest.GamePath) + $GameArguments
Write-Host "Shared user data: $($data.Path) ($($data.Resolution))"
Write-Host ('Command (JSON argv; array preserves quoting): ' + (@($exe) + $arguments | ConvertTo-Json -Compress))
Write-Host "OS working directory: $($manifest.GamePath)"
if ($ValidateOnly) {
    if ($BackupUserData) { & "$PSScriptRoot/Backup-ZHUserData.ps1" -ValidateOnly }
    Write-Host 'VALIDATION ONLY: no game launch, receipt, acknowledgement or user-data changes.'
    if (-not $BackupUserData -and -not $AcknowledgeSharedUserData) { Write-Host 'Launch would require -BackupUserData or explicit -AcknowledgeSharedUserData.' }
    return
}
if (-not $BackupUserData -and -not $AcknowledgeSharedUserData) {
    throw 'Launch blocked: choose -BackupUserData (verified fresh backup) or -AcknowledgeSharedUserData (explicit shared-data risk acknowledgement). Dedicated Windows test account is preferred.'
}
$backupReceipt = $null
if ($BackupUserData) {
    $backupReceipt = & "$PSScriptRoot/Backup-ZHUserData.ps1"
    if (-not $backupReceipt.Verified -or $backupReceipt.UserDataPath -ne $data.Path -or $backupReceipt.User -ne $data.User) {
        throw 'Backup receipt does not match the current account/user-data path.'
    }
} else { Write-Host 'EXPLICIT ACKNOWLEDGEMENT: this game can change existing shared preferences, saves, maps, replays and logs.' }
$record = [ordered]@{
    StartedUtc=[DateTime]::UtcNow.ToString('o'); User=$data.User; UserDataPath=$data.Path
    Executable=$exe; ExecutableSHA256=$manifest.Build.ExecutableSHA256; Arguments=$arguments
    AcknowledgedSharedData=[bool]$AcknowledgeSharedUserData; Backup=$backupReceipt
}
$recordPath = Join-Path $manifest.StagePath ('launch-' + [Guid]::NewGuid().ToString('N') + '.json')
Write-ZHJson $recordPath $record
# ArgumentList avoids PowerShell/shell command interpolation and Windows quoting errors.
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $exe
$start.WorkingDirectory = $manifest.GamePath
$start.UseShellExecute = $false
foreach ($argument in $arguments) { $start.ArgumentList.Add($argument) }
Assert-ZHNoLinks $manifest.GamePath
Write-Host "Launching staged development binary; launch record: $recordPath"
$process = [Diagnostics.Process]::Start($start)
$process.WaitForExit()
Write-Host "Game exit code: $($process.ExitCode)"
Write-ZHJson ($recordPath + '.exit.json') @{ExitedUtc=[DateTime]::UtcNow.ToString('o'); ExitCode=$process.ExitCode}
exit $process.ExitCode
