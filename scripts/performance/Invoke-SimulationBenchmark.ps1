<#
X64.38 repeatable headless simulation benchmark (no window, no GPU, no human play, no proprietary content committed).

It generates a self-playing AI-versus-AI replay from the header of any replay you recorded with this build
(New-SimulationBenchmarkReplay.py), runs `generalsv.exe -headless -replay` N times with GENERALS_SIM_BENCH set, and compares the
determinism fingerprints of the runs (equal digests = identical authoritative state at every checkpoint).

  powershell -File Invoke-SimulationBenchmark.ps1 -GameDir <dir with the .big files and generalsv.exe> -Template <your.rep> `
      -Out <empty output dir> [-Map "Twilight Flame" -MapSize 402471 -Ai 7 -Level H -Minutes 40 -Seed 20260 -Runs 2]

The executable is copied next to the game files under a distinct name only when -Exe is given (the engine finds its data relative to the
executable); the installed game is never modified.
#>
param(
	[Parameter(Mandatory)][string]$GameDir,
	[Parameter(Mandatory)][string]$Template,
	[Parameter(Mandatory)][string]$Out,
	[string]$Exe = "",
	[string]$Map = "Twilight Flame",
	[int]$MapSize = 402471,
	[int]$Ai = 7,
	[ValidateSet("E", "M", "H")][string]$Level = "H",
	[double]$Minutes = 40,
	[int]$Seed = 20260,
	[int]$Runs = 2,
	[int]$FingerprintInterval = 30
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $Out | Out-Null
$gen = Join-Path $PSScriptRoot "New-SimulationBenchmarkReplay.py"
$rep = Join-Path $Out "benchmark.rep"
python -I $gen --template $Template --out $rep --map $Map --map-size $MapSize --ai $Ai --level $Level --minutes $Minutes --seed $Seed
if ($LASTEXITCODE -ne 0) { throw "replay generation failed" }

$exeName = "generalsv.exe"
if ($Exe) {
	$exeName = "generalsv_simbench.exe"
	Copy-Item $Exe (Join-Path $GameDir $exeName) -Force
}
$env:GENERALS_SIM_IGNORE_CRC = "1"
$env:GENERALS_SIM_FP_INTERVAL = "$FingerprintInterval"
$digests = @()
Push-Location $GameDir
try {
	for ($i = 1; $i -le $Runs; $i++) {
		$env:GENERALS_SIM_BENCH = Join-Path $Out "run$i"
		cmd /c "`"$(Join-Path $GameDir $exeName)`" -headless -replay `"$rep`" > `"$(Join-Path $Out "run$i.stdout.txt")`" 2>&1"
		$line = Select-String -Path (Join-Path $Out "run${i}_summary.txt") -Pattern "digest" | Select-Object -First 1
		$digests += ($line.Line -replace '.*digest ', '')
		Write-Host ("run {0}: digest {1}" -f $i, $digests[-1])
	}
} finally {
	Pop-Location
	Remove-Item Env:GENERALS_SIM_BENCH, Env:GENERALS_SIM_IGNORE_CRC, Env:GENERALS_SIM_FP_INTERVAL -ErrorAction SilentlyContinue
}
$unique = $digests | Sort-Object -Unique
if ($unique.Count -eq 1) { Write-Host "DETERMINISM: all $Runs runs produced the identical fingerprint $($unique[0])" }
else { Write-Host "DETERMINISM FAILURE: fingerprints differ: $($digests -join ', ')"; exit 2 }
Get-Content (Join-Path $Out "run1_summary.txt") | Select-Object -First 12
