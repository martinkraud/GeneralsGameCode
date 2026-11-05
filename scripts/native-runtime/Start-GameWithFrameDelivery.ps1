# Starts the game with the frame-delivery telemetry on (multiplayer smoothness audit). Changes nothing else: the game is started exactly as a double click would, with
# one environment variable for THIS process (GENERALS_FRAMEDELIVERY=<OutDir>). The game appends nothing to the Steam folder or user data; the CSV files go to OutDir.
#
#   powershell -ExecutionPolicy Bypass -File Start-GameWithFrameDelivery.ps1 -GameDir "<game folder>" -OutDir C:\lan-test\fd
#   (add -PerFrame for one row per rendered frame; windows summary = 5 s rows, always written)
#
# After the match: Collect-LanTestEvidence.ps1 ... -FrameDeliveryDir C:\lan-test\fd   and   python Summarize-FrameDelivery.py C:\lan-test\fd
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$GameDir,
      [string]$OutDir = 'C:\lan-test\fd',
      [switch]$PerFrame)
$ErrorActionPreference = 'Stop'
$gameDirFull = (Resolve-Path -LiteralPath $GameDir).Path
$exe = $null
foreach ($name in @('Generals.exe', 'generalszh.exe', 'generalsv.exe')) {
    $p = Join-Path $gameDirFull $name
    if (Test-Path -LiteralPath $p) { $exe = $p; break }
}
if (-not $exe) { throw "No Generals executable in $gameDirFull" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$env:GENERALS_FRAMEDELIVERY = (Resolve-Path -LiteralPath $OutDir).Path
if ($PerFrame) { $env:GENERALS_FRAMEDELIVERY_FRAMES = '1' }
Write-Host "Starting $exe with frame-delivery telemetry -> $env:GENERALS_FRAMEDELIVERY"
Start-Process -FilePath $exe -WorkingDirectory $gameDirFull
