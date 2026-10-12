#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [switch]$RunReviewedInitOnly,
      [ValidateRange(10,180)][int]$TimeoutSeconds=120)
. (Join-Path $PSScriptRoot 'Common.ps1')
if (!$RunReviewedInitOnly) { throw 'Game launch requires explicit developer review and -RunReviewedInitOnly' }
if (@(Get-Process -Name generals,generalsv,generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
$manifest=Read-NativeStage $Title $Name
$run=Join-Path $manifest.StagePath ('runs/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffffZ'))
Assert-ZHNoLinks $run
if ($run -notmatch '^[A-Za-z]:\\[\x20-\x7e]+$' -or $run.Length -ge 180) { throw 'Diagnostic directory must be ASCII, absolute and shorter than 180 characters' }
New-Item -ItemType Directory -Path (Join-Path $run 'userdata/CrashDumps') | Out-Null
$exe=Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))
$arguments=@('-headless','-nativeInitOnly',$run)
$receipt=[ordered]@{Title=$Title;Executable=$exe;Build=$manifest.Build;WorkingDirectory=$manifest.GamePath
    Arguments=$arguments;UserData=([IO.Path]::GetFullPath((Join-Path $run 'userdata'))+'\')
    DumpPath=[IO.Path]::GetFullPath((Join-Path $run 'userdata/CrashDumps'));LogPath=(Join-Path $run 'startup.log')
    ShutdownEvidencePath=(Join-Path $run 'shutdown-exception.bin')
    BaseAssetsPath=if ($Title -eq 'ZeroHour') { (Join-Path $manifest.GamePath 'ZH_Generals')+'\' } else { $null }
    Dependencies=$manifest.Dependencies;StartedUtc=[DateTime]::UtcNow.ToString('o');TimeoutSeconds=$TimeoutSeconds}
Write-ZHJson (Join-Path $run 'launch.json') $receipt
$process=[Diagnostics.Process]::new()
$info=[Diagnostics.ProcessStartInfo]::new($exe)
$info.WorkingDirectory=$manifest.GamePath; $info.UseShellExecute=$false
$info.CreateNoWindow=$true; $info.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
foreach ($arg in $arguments) { $info.ArgumentList.Add($arg) }
$process.StartInfo=$info
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
$log=if (Test-Path -LiteralPath $receipt.LogPath) { Get-Content -LiteralPath $receipt.LogPath -Raw } else { '' }
$pathReceipts=@{}
foreach ($line in ($log -split "`n")) {
    $fields=$line.TrimEnd("`r") -split "`t",9
    if ($fields.Count -eq 9 -and $fields[5] -in @('module','userdata','dump_path','startup_parse','base_assets')) {
        $pathReceipts[$fields[5]]=$fields[8]
    }
}
$pathsMatch=$pathReceipts['module'] -eq $exe -and $pathReceipts['userdata'] -eq $receipt.UserData -and
    $pathReceipts['dump_path'] -eq ($receipt.DumpPath+'\') -and $pathReceipts['startup_parse'] -eq $receipt.WorkingDirectory
if ($Title -eq 'ZeroHour') { $pathsMatch=$pathsMatch -and $pathReceipts['base_assets'] -eq $receipt.BaseAssetsPath }
$shutdownBytes=if (Test-Path -LiteralPath $receipt.ShutdownEvidencePath -PathType Leaf) {
    (Get-Item -LiteralPath $receipt.ShutdownEvidencePath).Length
} else { $null }
$success=$started -and !$timeout -and !$errorText -and $exit -eq 0 -and
    $null -ne $shutdownBytes -and $shutdownBytes -eq 0 -and $log.Contains("native_shutdown`tarmed") -and
    !$log.Contains("native_shutdown`texception") -and
    $log.Contains("init_only`tcomplete") -and $log.Contains("engine_cleanup`tcomplete") -and
    $log.Contains("WinMain`tcomplete") -and $log.Contains("FP`tcomplete") -and $pathsMatch -and !$log.Contains("`tfail`t")
Write-ZHJson (Join-Path $run 'exit.json') ([pscustomobject]@{
    FinishedUtc=[DateTime]::UtcNow.ToString('o');ProcessId=$pidValue;Started=$started;TimedOut=$timeout
    ExitCode=$exit;Error=$errorText;Success=$success;HasStartupLog=($log.Length -gt 0)
    ActualPaths=$pathReceipts;PathsMatch=$pathsMatch;ShutdownEvidencePath=$receipt.ShutdownEvidencePath;ShutdownEvidenceBytes=$shutdownBytes
    Interpretation=if ($log.Length -eq 0) {'No in-process checkpoint: inspect loader/CRT/static initialization or rejected gate'} else {'Inspect last completed/begun checkpoint'}
})
Write-Host "Receipts and flushed log: $run"
if (!$success) { throw 'Native init-only validation failed; preserve evidence, do not rerun automatically' }
Write-Host 'PASS: initialized and cleaned up; execute/replay/simulation skipped. No next trial is launched.'
