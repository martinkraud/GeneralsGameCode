#Requires -Version 7.0
# X64.45: starts a staged Zero Hour native candidate exactly like a player would (Invoke-NativeNormalLaunch.ps1, no diagnostic
# option) after taking a small snapshot of the player's Zero Hour settings files. The candidate must have been staged and
# preflighted with Stage-NativeOrdinaryPlay.ps1 -Title ZeroHour. The Steam installation is only ever read (by the stager).
# The game process is never touched by this script; end the run in the game (Exit Game) or by closing the window.
[CmdletBinding()]
param([string]$Name = 'dist-f38ba6418-zh',
      [switch]$Endurance,
      [switch]$BackupUserData,            # verified full copy of the Zero Hour data folder first (about 525 MB); this is the default
      [switch]$AcknowledgeSharedUserData, # skip the full copy: the game may change shared preferences, saves, maps, replays and logs
      [switch]$Classic,
      [switch]$PreflightOnly,             # verify the stage, dependencies and user-data path, print the plan, write nothing, start nothing
      [ValidateRange(1,4)][int]$MaxHours = 4)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$repo = $script:ZHRepository

# Fail before anything is written when the stage is not an intact reviewed candidate (same check the launcher repeats).
$manifest = Read-NativeOrdinaryPlayStage 'ZeroHour' $Name $script:NativeNormalLaunchGenerated -AllowCrashDumps
Write-Host "Candidate  : $Name (Zero Hour, AMD64, normal launch$(if ($Classic) { ', -classic: Modern Experience off' }))"
Write-Host "Executable : $(Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable)))"

if ($PreflightOnly) {
    $dlls = @($manifest.Dependencies | ForEach-Object { $_.Name }) -join ', '
    Write-Host "Dependencies: $dlls"
    Write-Host "Zero Hour user data: $((Get-ZHUserData).Path)"
    Write-Host 'PREFLIGHT PASS: the stage is an intact reviewed candidate. Nothing was written and no game was started.'
    return
}

# The engine reads and writes the player's real Zero Hour data folder (Options.ini, Skirmish.ini, saves, replays). The three small
# settings files are copied first so a bad first run can be undone by hand; saves, maps and replays are not copied (525 MB).
$data = Get-ZHUserData
# Same rule as the Win32 Zero Hour launcher (Launch-ZHRuntime.ps1): a launch needs a verified fresh backup or an explicit acknowledgement.
if (-not $BackupUserData -and -not $AcknowledgeSharedUserData) { $BackupUserData = $true }
if ($BackupUserData) {
    $receipt = & (Join-Path $repo 'scripts\zh-runtime\Backup-ZHUserData.ps1')
    if (-not $receipt.Verified -or $receipt.UserDataPath -ne $data.Path) { throw 'Backup receipt does not match the current user-data path.' }
    Write-Host "Full user-data backup: $($receipt.BackupPath)"
} else { Write-Host 'EXPLICIT ACKNOWLEDGEMENT: this game can change existing shared preferences, saves, maps, replays and logs.' }
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$snapshot = Join-Path $repo "build\dev-runtimes\userdata-backups\zh-native-settings-$Name-$stamp"
if (Test-Path -LiteralPath $data.Path -PathType Container) {
    New-Item -ItemType Directory -Force -Path $snapshot | Out-Null
    foreach ($file in 'Options.ini', 'Skirmish.ini', 'SkirmishStats.ini') {
        $source = Join-Path $data.Path $file
        if (Test-Path -LiteralPath $source -PathType Leaf) { Copy-Item -LiteralPath $source -Destination (Join-Path $snapshot $file) }
    }
    Write-Host "Settings snapshot: $snapshot"
} else {
    Write-Host "User data folder absent ($($data.Path)); the game will create it."
}

if ($Endurance) {
    $out = Join-Path $repo "build\endurance-telemetry\$Name-$stamp"
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    $env:GENERALS_ENDURANCE = $out
    Write-Host "Endurance telemetry: $out"
}
try {
    & (Join-Path $PSScriptRoot 'Invoke-NativeNormalLaunch.ps1') -Title ZeroHour -Name $Name -RunNormalLaunch -Classic:$Classic -TimeoutSeconds ($MaxHours * 3600)
} finally {
    $env:GENERALS_ENDURANCE = $null
    Write-Host ''
    Write-Host "Run evidence : $($manifest.StagePath)\normal-launch (exit receipt *.exit.json, generals64.log copy, CrashDumps if any)"
    Write-Host "Live log     : $($manifest.GamePath)\generals64.log is moved to normal-launch after the run"
}
