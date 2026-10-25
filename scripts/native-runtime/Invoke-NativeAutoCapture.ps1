#Requires -Version 7.0
# X64.45: unattended capture of a staged normal-launch candidate for terrain / world inspection (developer aid, not a validation gate).
# Starts the staged executable like a player would, drives the real shell screens (GENERALS_AUTOFLOW=skirmish|campaign|challenge: Skirmish setup -> Start, Single Player -> USA -> Easy, General's Challenge -> Easy -> general -> Play) or
# -devMode -quickGame, writes the composed frame of the shell and of the running match (GENERALS_AUTOSHOT) and quits the game by itself.
# A window is shown on the desktop for about a minute. The stage's generals64.log is copied next to the images; the stage itself is run in
# place, so use a disposable stage (never the candidate you intend to hand to a human): the strict stage check fails afterwards.
# The black-pixel figure is a crude regression indicator for the "black terrain strips" defect (no pure-black rows in a lit match view).
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [ValidateSet('Skirmish','Campaign','Challenge','SaveLoad','Slots','MatchOptions','MainMenu','QuickGame')][string]$Flow = 'Skirmish',
      [ValidateRange(0,11)][int]$General = 0,
      [switch]$LookAtCenter,              # developer aid: camera over the map centre before the match frame (GENERALS_AUTOSHOT_LOOKAT)
      [string]$UiScale = '',              # developer override 75..300 (GENERALS_FORCE_UI_SCALE): layout audits at every UI scale
      [string]$ClientSize = '',           # developer override WxH (GENERALS_FORCE_CLIENT_SIZE), e.g. 5120x1440 on a smaller screen
      [ValidateRange(120,20000)][int]$MatchFrames = 1500,
      [ValidateRange(60,600)][int]$TimeoutSeconds = 240,
      [string]$Disable = '',
      [switch]$Classic)                   # UI.2: start the retail frontend (-classic) to compare / to prove the Modern shell is absent
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if (@(Get-Process -Name generals,generalsv,generalszh -ErrorAction SilentlyContinue).Count) { throw 'Exit all existing games first' }
$stage = Get-NativeStage $Title $Name
$manifest = Get-Content -LiteralPath (Join-Path $stage 'manifest.json') -Raw | ConvertFrom-Json
$exe = Join-Path $manifest.GamePath ([IO.Path]::GetFileName($manifest.Build.Executable))
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Staged executable missing: $exe" }
$out = Join-Path $script:ZHRepository ('build/diag/autocapture-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + "-$Title-$Name")
New-Item -ItemType Directory -Force -Path $out | Out-Null
$env:GENERALS_AUTOSHOT = $out
$env:GENERALS_AUTOSHOT_FRAMES = "$MatchFrames"
if ($Flow -ne 'QuickGame') { $env:GENERALS_AUTOFLOW = $Flow.ToLowerInvariant() }
if ($Flow -in 'Challenge','SaveLoad') { $env:GENERALS_AUTOFLOW_GENERAL = "$General" }
if ($Disable) { $env:GENERALS_NATIVE_DISABLE = $Disable }
if ($ClientSize) { $env:GENERALS_FORCE_CLIENT_SIZE = $ClientSize }
if ($UiScale) { $env:GENERALS_FORCE_UI_SCALE = $UiScale }
if ($LookAtCenter) { $env:GENERALS_AUTOSHOT_LOOKAT = 'center' }
[string[]]$arguments = if ($Flow -eq 'QuickGame') { @('-devMode', '-quickGame', '-noaudio') } else { @('-noaudio') }
if ($Classic) { $arguments += '-classic' }
$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $manifest.GamePath -PassThru
try {
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Write-Warning 'Timed out; the game is terminated.'; $process.Kill() }
} finally {
    $env:GENERALS_FORCE_UI_SCALE = $null; $env:GENERALS_AUTOSHOT = $null; $env:GENERALS_AUTOSHOT_FRAMES = $null; $env:GENERALS_AUTOFLOW = $null; $env:GENERALS_AUTOFLOW_GENERAL = $null; $env:GENERALS_NATIVE_DISABLE = $null; $env:GENERALS_FORCE_CLIENT_SIZE = $null; $env:GENERALS_AUTOSHOT_LOOKAT = $null
}
$log = Join-Path $manifest.GamePath 'generals64.log'
if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination (Join-Path $out 'generals64.log') }
Add-Type -AssemblyName System.Drawing
foreach ($bmp in Get-ChildItem -LiteralPath $out -Filter *.bmp) {
    $image = [System.Drawing.Bitmap]::FromFile($bmp.FullName)
    try {
        $png = [IO.Path]::ChangeExtension($bmp.FullName, 'png')
        $image.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
        # pure-black pixels in the upper 65 % of the frame (the world view; the control bar owns the rest)
        $rect = [System.Drawing.Rectangle]::new(0, 0, $image.Width, [int]($image.Height * 0.65))
        $data = $image.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $bytes = New-Object byte[] ($data.Stride * $data.Height)
            [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
        } finally { $image.UnlockBits($data) }
        $black = 0; $pixels = $data.Width * $data.Height
        for ($y = 0; $y -lt $data.Height; ++$y) {
            $row = $y * $data.Stride
            for ($x = 0; $x -lt $data.Width; ++$x) {
                $i = $row + $x * 4
                if ($bytes[$i] -eq 0 -and $bytes[$i + 1] -eq 0 -and $bytes[$i + 2] -eq 0) { ++$black }
            }
        }
        Write-Host ('{0}: {1:N2} % pure-black pixels in the world view' -f $bmp.Name, (100.0 * $black / $pixels))
    } finally { $image.Dispose() }
}
Write-Host "Exit code $($process.ExitCode); images and log: $out"
