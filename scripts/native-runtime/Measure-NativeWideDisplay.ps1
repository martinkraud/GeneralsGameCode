#Requires -Version 7.0
# X64.48A: objective measurements for the ultrawide / fog of war fixes. For a title and several client sizes (a developer override presents sizes the
# monitor cannot show) it captures the running Skirmish match twice, with and without the native shroud, and reports
#   * pure-black pixels in the world view (black bands = terrain not drawn / map edge),
#   * mean luminance of the world view with the fog of war on and off (equal = the overlay is missing, lower = fog is drawn).
# Needs a disposable stage (the game is run in place); a window is shown for about half a minute per run.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [string[]]$Sizes = @('1920x1080', '2560x1440', '3440x1440', '5120x1440'),
      [switch]$CenterView)
$ErrorActionPreference = 'Stop'
$capture = Join-Path $PSScriptRoot 'Invoke-NativeAutoCapture.ps1'
$repo = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
Add-Type -AssemblyName System.Drawing
function Measure-Frame([string]$Path) {
    $image = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $black = 0; $sum = 0.0; $n = 0; $limit = [int]($image.Height * 0.6)
        for ($y = 0; $y -lt $limit; $y += 4) { for ($x = 0; $x -lt $image.Width; $x += 4) {
            $c = $image.GetPixel($x, $y); $sum += 0.2126 * $c.R + 0.7152 * $c.G + 0.0722 * $c.B; if ($c.R -eq 0 -and $c.G -eq 0 -and $c.B -eq 0) { ++$black }; ++$n } }
        [pscustomobject]@{ BlackPercent = [math]::Round(100.0 * $black / $n, 2); Luminance = [math]::Round($sum / $n, 1) }
    } finally { $image.Dispose() }
}
$rows = foreach ($size in $Sizes) {
    $result = @{}
    foreach ($shroud in $true, $false) {
        $arguments = @('-Title', $Title, '-Name', $Name, '-Flow', 'Skirmish', '-MatchFrames', '900', '-TimeoutSeconds', '200', '-ClientSize', $size)
        if (-not $shroud) { $arguments += @('-Disable', 'shroud') }
        if ($CenterView) { $arguments += '-LookAtCenter' }
        & pwsh -NoProfile -File $capture @arguments | Out-Null
        $dir = Get-ChildItem (Join-Path $repo 'build\diag') -Directory | Sort-Object LastWriteTime | Select-Object -Last 1
        $result[$shroud] = Measure-Frame (Join-Path $dir.FullName 'autoshot-game.png')
    }
    [pscustomobject]@{ Title = $Title; Size = $size; BlackPercent = $result[$true].BlackPercent; LuminanceFogOn = $result[$true].Luminance; LuminanceFogOff = $result[$false].Luminance
                       FogVisible = $result[$true].Luminance -lt $result[$false].Luminance - 3 }
}
$rows | Format-Table -AutoSize | Out-String | Write-Host
$rows | ConvertTo-Json | Write-Output
