#Requires -Version 7.0
# X64.31 regression: generals64.log is an expected normal-launch artifact; every other runtime mutation still
# fails the integrity gate, and cleanup returns the stage to its manifest. Synthetic directories only; never
# launches a title.
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$checks=0
function Check([bool]$Condition,[string]$Message) {
    if (!$Condition) { throw "FAILED: $Message" }
    $script:checks++
}
$root=Join-Path ([IO.Path]::GetTempPath()) ('native-normal-launch-'+[Guid]::NewGuid().ToString('N'))
$game=Join-Path $root 'game'
$evidence=Join-Path $root 'normal-launch'
New-Item -ItemType Directory -Force -Path (Join-Path $game 'Data') | Out-Null
try {
    Set-Content -LiteralPath (Join-Path $game 'generalsv.exe') -Value 'exe' -NoNewline
    Set-Content -LiteralPath (Join-Path $game 'INI.big') -Value 'ini' -NoNewline
    Set-Content -LiteralPath (Join-Path $game 'Data/a.txt') -Value 'a' -NoNewline
    $inventory=@(Get-ZHInventory $game)
    $allow=$script:NativeNormalLaunchGenerated
    function RuntimeDiff { @(Get-NativeRuntimeDifferences $inventory @(Get-ZHInventory $game) $allow) }

    Check ($allow.Count -eq 1 -and $allow[0] -eq 'generals64.log') 'allowlist is exactly generals64.log'
    Check (@(RuntimeDiff).Count -eq 0) 'pristine stage has no differences'

    # 1. generals64.log is permitted, preserved, and (with the allowlist) invisible to the gate.
    Set-Content -LiteralPath (Join-Path $game 'generals64.log') -Value "1`tlog" -NoNewline
    Check (@(RuntimeDiff).Count -eq 0) 'generals64.log is permitted for normal launch'
    Check (@(Get-NativeRuntimeDifferences $inventory @(Get-ZHInventory $game) @()).Count -eq 1) 'without the allowance the same file is still a mutation'
    $kept=@(Restore-NativeRuntimeGeneratedFiles $game $allow $evidence 'stamp')
    Check ($kept.Count -eq 1 -and (Test-Path -LiteralPath $kept[0])) 'log preserved into evidence'
    Check ((Get-Content -LiteralPath $kept[0] -Raw) -eq "1`tlog") 'preserved log is byte-identical'

    # 3. cleanup leaves the stage exactly as staged (strict comparison, no allowance).
    Check (!(Test-Path -LiteralPath (Join-Path $game 'generals64.log'))) 'log removed from stage'
    Check (@(Get-NativeRuntimeDifferences $inventory @(Get-ZHInventory $game) @()).Count -eq 0) 'stage equals manifest after cleanup'
    Check (@(Restore-NativeRuntimeGeneratedFiles $game $allow $evidence 'again').Count -eq 0) 'cleanup with nothing to preserve is a no-op'

    # 2. Unrelated mutations still fail, even when the allowed log is also present.
    Set-Content -LiteralPath (Join-Path $game 'generals64.log') -Value 'x' -NoNewline
    Set-Content -LiteralPath (Join-Path $game 'extra.dat') -Value 'x' -NoNewline
    $d=@(RuntimeDiff)
    Check ($d.Count -eq 1 -and $d[0] -eq 'Added: extra.dat') 'unrelated added file fails'
    Remove-Item -LiteralPath (Join-Path $game 'extra.dat')
    Set-Content -LiteralPath (Join-Path $game 'Data/generals64.log') -Value 'x' -NoNewline
    $d=@(RuntimeDiff)
    Check ($d.Count -eq 1 -and $d[0] -eq 'Added: Data\generals64.log') 'same name in a subdirectory fails'
    Remove-Item -LiteralPath (Join-Path $game 'Data/generals64.log')
    Set-Content -LiteralPath (Join-Path $game 'INI.big') -Value 'tampered' -NoNewline
    $d=@(RuntimeDiff)
    Check ($d.Count -eq 1 -and $d[0] -eq 'Changed: INI.big') 'modified runtime file fails'
    Set-Content -LiteralPath (Join-Path $game 'INI.big') -Value 'ini' -NoNewline
    Remove-Item -LiteralPath (Join-Path $game 'Data/a.txt')
    $d=@(RuntimeDiff)
    Check ($d.Count -eq 1 -and $d[0] -eq 'Removed: Data\a.txt') 'deleted runtime file fails'
    # A modified log is not an *added* file only when it was part of the manifest: it never is, so a stage whose
    # manifest contained a log would report Changed and fail.
    Set-Content -LiteralPath (Join-Path $game 'Data/a.txt') -Value 'a' -NoNewline
    $withLog=@(Get-ZHInventory $game)
    Set-Content -LiteralPath (Join-Path $game 'generals64.log') -Value 'y' -NoNewline
    Check (@(Get-NativeRuntimeDifferences $withLog @(Get-ZHInventory $game) $allow).Count -eq 1) 'a changed log that was staged is still a mutation'

    # Crash dumps: preserved as evidence (moved out of the stage), reported, and never silently tolerated.
    Remove-Item -LiteralPath (Join-Path $game 'generals64.log') -Force
    $base=@(Get-ZHInventory $game)
    $dumpDir=Join-Path $game 'CrashDumps'
    New-Item -ItemType Directory -Force -Path $dumpDir | Out-Null
    foreach ($n in 'CrashFG-20261014-091403-bb98b8c01-pid24800.dmp','CrashMG-20261014-091403-bb98b8c01-pid24800.dmp') {
        Set-Content -LiteralPath (Join-Path $dumpDir $n) -Value 'dump' -NoNewline
    }
    Check (@(Get-NativeRuntimeDifferences $base @(Get-ZHInventory $game) @()).Count -eq 2) 'crash dumps are mutations by default'
    Check (@(Get-NativeRuntimeDifferences $base @(Get-ZHInventory $game) @() -AllowCrashDumps).Count -eq 0) 'crash dumps tolerated only with the explicit switch'
    Set-Content -LiteralPath (Join-Path $dumpDir 'other.dat') -Value 'x' -NoNewline
    Set-Content -LiteralPath (Join-Path $dumpDir 'CrashFG-evil.dmp') -Value 'x' -NoNewline
    Check (@(Get-NativeRuntimeDifferences $base @(Get-ZHInventory $game) @() -AllowCrashDumps).Count -eq 2) 'unrelated/odd files in CrashDumps still fail'
    Remove-Item -LiteralPath (Join-Path $dumpDir 'other.dat'),(Join-Path $dumpDir 'CrashFG-evil.dmp')
    $moved=@(Restore-NativeRuntimeCrashDumps $game $evidence 'crash')
    Check ($moved.Count -eq 2 -and (Test-Path -LiteralPath $moved[0]) -and (Test-Path -LiteralPath $moved[1])) 'dumps preserved into evidence'
    Check (@(Get-ChildItem -LiteralPath $dumpDir -File).Count -eq 0) 'dumps removed from the stage'
    Check (@(Get-NativeRuntimeDifferences $base @(Get-ZHInventory $game) @()).Count -eq 0) 'stage equals manifest after crash cleanup'
    Check (@(Restore-NativeRuntimeCrashDumps $game $evidence 'none').Count -eq 0) 'no dumps: nothing moved (crash flag false)'

    # Name validation: the allowlist can never smuggle a path.
    $threw=$false
    try { Restore-NativeRuntimeGeneratedFiles $game @('..\x') $evidence 's' | Out-Null } catch { $threw=$true }
    Check $threw 'path-like generated names are rejected'
}
finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host "PASS: $checks normal-launch generated-file integrity checks. No game launched."
