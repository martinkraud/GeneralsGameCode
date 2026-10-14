#Requires -Version 7.0
# X64.31: launches the staged AMD64 Generals exactly like a player would: no diagnostic token, no isolated
# user-data root, original frontend lifecycle. The only machine checks are the process exit code and the
# lightweight generals64.log written next to the executable.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [switch]$RunNormalLaunch,
      [ValidateRange(60,14400)][int]$TimeoutSeconds=7200)
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
if (!$RunNormalLaunch) { throw 'Normal launch requires explicit developer review and -RunNormalLaunch' }
if (@(Get-Process -Name generals,generalsv,generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
$manifest=Read-NativeOrdinaryPlayStage $Title $Name
$exe=Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Staged executable missing: $exe" }
$runs=Join-Path $manifest.StagePath 'normal-launch'
New-Item -ItemType Directory -Force -Path $runs | Out-Null
$stamp=[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffffZ')
$info=[Diagnostics.ProcessStartInfo]::new($exe)
$info.WorkingDirectory=$manifest.GamePath; $info.UseShellExecute=$false
$info.CreateNoWindow=$false; $info.WindowStyle=[Diagnostics.ProcessWindowStyle]::Normal
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
if ($log.Length) { Copy-Item -LiteralPath $logPath -Destination (Join-Path $runs "$stamp.generals64.log") }
$result=[pscustomobject]@{Executable=$exe;ProcessId=$pidValue;Started=$started;TimedOut=$timeout;ExitCode=$exit
    Error=$errorText;HasLog=($log.Length -gt 0);Log=$logPath;FinishedUtc=[DateTime]::UtcNow.ToString('o')}
Write-ZHJson (Join-Path $runs "$stamp.exit.json") $result
Write-Host "Normal launch finished: exit=$exit timedOut=$timeout log=$logPath"
if (!$started -or $timeout -or $errorText -or $exit -ne 0) { throw 'Normal launch did not exit cleanly; preserve generals64.log' }
Write-Host 'PASS: process exited with code 0. Frontend, audio and gameplay review is a separate developer judgement.'
