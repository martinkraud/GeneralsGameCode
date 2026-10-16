<#
X64.41 R1: re-simulate the replay recorded with Run-R1-Diagnostic.cmd and name the first frame / component / object at which the live game and the
re-simulation differ. Agent / developer step; no interaction with the game.

  powershell -File Analyze-ReplayDiagnostic.ps1 -GameDir <folder with generalsv_r1diag.exe, r1_live.rep and r1_live.trace>

Output: r1_resim.trace (the re-simulation's trace) and the report of Compare-CrcTraces.py on the console.
#>
param([Parameter(Mandatory)][string]$GameDir)
$ErrorActionPreference = "Stop"
$exe = Join-Path $GameDir "generalsv_r1diag.exe"
$rep = Join-Path $GameDir "r1_live.rep"
$live = Join-Path $GameDir "r1_live.trace"
$resim = Join-Path $GameDir "r1_resim.trace"
foreach ($f in $exe, $rep, $live) { if (-not (Test-Path $f)) { throw "missing $f" } }
if (Test-Path $resim) { Remove-Item $resim }
$env:GENERALS_CRC_TRACE = $resim
$env:GENERALS_CRC_TRACE_OBJECTS = "1"
$env:GENERALS_SIM_IGNORE_CRC = "1"
$env:GENERALS_SIM_BENCH = (Join-Path $GameDir "r1_resim")
Push-Location $GameDir
try {
  cmd /c "`"$exe`" -headless -replay `"$rep`" > `"$(Join-Path $GameDir 'r1_resim_stdout.txt')`" 2>&1"
} finally { Pop-Location }
Get-Content (Join-Path $GameDir "r1_resim_stdout.txt") | Select-String "Mismatch" | Select-Object -First 3
python -I (Join-Path $PSScriptRoot "Compare-CrcTraces.py") $live $resim
