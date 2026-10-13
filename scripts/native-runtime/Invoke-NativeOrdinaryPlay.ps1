#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [switch]$RunReviewedOrdinaryPlay,
      [ValidateRange(300,7200)][int]$TimeoutSeconds=4200)
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
if (!$RunReviewedOrdinaryPlay) { throw 'Play launch requires explicit developer review and -RunReviewedOrdinaryPlay' }
if (@(Get-Process -Name generals,generalsv,generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
$manifest=Read-NativeOrdinaryPlayStage $Title $Name
$run=Join-Path $manifest.StagePath ('runs/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffffZ'))
Assert-ZHNoLinks $run
if ($run -notmatch '^[A-Za-z]:\\[\x20-\x7e]+$' -or $run.Length -ge 180) { throw 'Diagnostic directory must be ASCII, absolute and shorter than 180 characters' }
New-Item -ItemType Directory -Path (Join-Path $run 'userdata/CrashDumps') | Out-Null
$exe=Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))
$arguments=@(Get-NativeOrdinaryPlayArguments $run)
$receipt=[ordered]@{Title=$Title;Executable=$exe;Build=$manifest.Build;WorkingDirectory=$manifest.GamePath
    Arguments=$arguments;UserData=([IO.Path]::GetFullPath((Join-Path $run 'userdata'))+'\')
    DumpPath=[IO.Path]::GetFullPath((Join-Path $run 'userdata/CrashDumps'));LogPath=(Join-Path $run 'startup.log')
    ShutdownEvidencePath=(Join-Path $run 'shutdown-exception.bin')
    Dependencies=$manifest.Dependencies;StartedUtc=[DateTime]::UtcNow.ToString('o');TimeoutSeconds=$TimeoutSeconds}
Write-ZHJson (Join-Path $run 'launch.json') $receipt
$process=[Diagnostics.Process]::new()
$info=[Diagnostics.ProcessStartInfo]::new($exe)
$info.WorkingDirectory=$manifest.GamePath; $info.UseShellExecute=$false
$info.CreateNoWindow=$false; $info.WindowStyle=[Diagnostics.ProcessWindowStyle]::Normal
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
    if ($fields.Count -eq 9 -and $fields[5] -in @('module','userdata','dump_path','startup_parse')) { $pathReceipts[$fields[5]]=$fields[8] }
}
$pathsMatch=$pathReceipts['module'] -eq $exe -and $pathReceipts['userdata'] -eq $receipt.UserData -and
    $pathReceipts['dump_path'] -eq ($receipt.DumpPath+'\') -and $pathReceipts['startup_parse'] -eq $receipt.WorkingDirectory
$shutdownBytes=if (Test-Path -LiteralPath $receipt.ShutdownEvidencePath -PathType Leaf) {
    (Get-Item -LiteralPath $receipt.ShutdownEvidencePath).Length
} else { $null }
try { $evidence=Test-NativeOrdinaryPlayEvidence -Log $log -ProcessId $pidValue -Receipt $receipt }
catch { $evidence=[pscustomobject]@{Valid=$false;Errors=@($_.Exception.Message);Records=0;Summary='Classifier failed'} }
$dumps=@(if(Test-Path -LiteralPath $receipt.DumpPath -PathType Container){Get-ChildItem -LiteralPath $receipt.DumpPath -Recurse -Force -File})
$success=$started -and !$timeout -and !$errorText -and $exit -eq 0 -and
    $null -ne $shutdownBytes -and $shutdownBytes -eq 0 -and $pathsMatch -and $evidence.Valid -and $dumps.Count -eq 0
Write-ZHJson (Join-Path $run 'exit.json') ([pscustomobject]@{
    FinishedUtc=[DateTime]::UtcNow.ToString('o');ProcessId=$pidValue;Started=$started;TimedOut=$timeout
    ExitCode=$exit;Error=$errorText;Success=$success;HasStartupLog=($log.Length -gt 0)
    ActualPaths=$pathReceipts;PathsMatch=$pathsMatch;ShutdownEvidencePath=$receipt.ShutdownEvidencePath;ShutdownEvidenceBytes=$shutdownBytes
    OrdinaryPlayEvidence=$evidence;CrashDumpCount=$dumps.Count;ManualVisibilityRequired=$true
    Interpretation=if ($log.Length -eq 0) {'No in-process checkpoint: inspect loader/CRT/static initialization or rejected gate'} else {'Inspect last completed/begun checkpoint'}
})
Write-Host "Receipts and flushed log: $run"
if (!$success) { throw 'Ordinary-play validation failed; preserve evidence, do not rerun automatically' }
Write-Host 'PASS: ordinary-play machine gate and cleanup. Original MainMenu -> Skirmish -> map -> native D3D11 terrain and world objects under the original UI and Control Bar, with live input; what was exercised is in play_summary. Visual and interaction review is a separate developer judgement. No next trial is launched.'
