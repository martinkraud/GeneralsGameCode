#Requires -Version 7.0
# X64.31: launches the staged AMD64 Generals exactly like a player would: no diagnostic token, no isolated
# user-data root, original frontend lifecycle. The only machine checks are the process exit code and the
# lightweight generals64.log written next to the executable.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [switch]$RunNormalLaunch,
      [switch]$Classic,   # -classic: the player-visible switch that turns every Modern Experience extension off (not a diagnostic token)
      [ValidateRange(60,14400)][int]$TimeoutSeconds=7200)
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
if (!$RunNormalLaunch) { throw 'Normal launch requires explicit developer review and -RunNormalLaunch' }
if (@(Get-Process -Name generals,generalsv,generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
# A log left by an earlier normal launch is the one tolerated difference; it is archived before the run.
$manifest=Read-NativeOrdinaryPlayStage $Title $Name $script:NativeNormalLaunchGenerated -AllowCrashDumps
$exe=Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Staged executable missing: $exe" }
$runs=Join-Path $manifest.StagePath 'normal-launch'
New-Item -ItemType Directory -Force -Path $runs | Out-Null
$stamp=[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffffZ')
$null=Restore-NativeRuntimeGeneratedFiles $manifest.GamePath $script:NativeNormalLaunchGenerated $runs "$stamp.previous"
$null=Restore-NativeRuntimeCrashDumps $manifest.GamePath $runs "$stamp.previous"

$info=[Diagnostics.ProcessStartInfo]::new($exe)
$info.WorkingDirectory=$manifest.GamePath; $info.UseShellExecute=$false
$info.CreateNoWindow=$false; $info.WindowStyle=[Diagnostics.ProcessWindowStyle]::Normal
if ($Classic) { $info.ArgumentList.Add('-classic') }
$process=[Diagnostics.Process]::new(); $process.StartInfo=$info
$started=$false; $timeout=$false; $exit=$null; $errorText=$null; $pidValue=$null
try {
    $started=$process.Start(); if (!$started) { throw 'Process.Start returned false' }
    $pidValue=$process.Id
    if (!$process.WaitForExit($TimeoutSeconds*1000)) {
        $timeout=$true; $process.Kill()
        if (!$process.WaitForExit(5000)) { throw 'Timed-out child did not exit after termination' }
    }
    $exit=$process.ExitCode
} catch { $errorText=$_.Exception.ToString() }
finally { $process.Dispose() }
$logPath=Join-Path $manifest.GamePath 'generals64.log'
$log=if (Test-Path -LiteralPath $logPath) { Get-Content -LiteralPath $logPath -Raw } else { '' }
# Preserve the runtime-generated log as evidence and restore the stage; then require the strict check to pass.
$kept=@(Restore-NativeRuntimeGeneratedFiles $manifest.GamePath $script:NativeNormalLaunchGenerated $runs $stamp)
$crashDumps=@(Restore-NativeRuntimeCrashDumps $manifest.GamePath $runs $stamp)
$cleanupError=$null
try { $null=Read-NativeOrdinaryPlayStage $Title $Name } catch { $cleanupError=$_.Exception.Message }
$result=[pscustomobject]@{Executable=$exe;ProcessId=$pidValue;Started=$started;TimedOut=$timeout;ExitCode=$exit
    Error=$errorText;HasLog=($log.Length -gt 0);PreservedLog=($kept -join ';');CrashDumps=($crashDumps -join ';');CrashDumpCount=$crashDumps.Count;StageRestored=($null -eq $cleanupError);CleanupError=$cleanupError;FinishedUtc=[DateTime]::UtcNow.ToString('o')}
Write-ZHJson (Join-Path $runs "$stamp.exit.json") $result
Write-Host "Normal launch finished: exit=$exit timedOut=$timeout log=$logPath"
if ($cleanupError) { throw "Stage was changed by something other than generals64.log: $cleanupError" }
if ($crashDumps.Count) { throw "Game crashed: $($crashDumps.Count) crash dump(s) preserved under $runs" }
if (!$started -or $timeout -or $errorText -or $exit -ne 0) { throw 'Normal launch did not exit cleanly; preserve generals64.log' }
Write-Host 'PASS: process exited with code 0. Frontend, audio and gameplay review is a separate developer judgement.'
