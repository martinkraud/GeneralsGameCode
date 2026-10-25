#Requires -Version 7.0
# UI.3: turns the screenshots kept by Invoke-FrontendMatrix.ps1 (PNG, build/ui3-evidence) into the small JPEG evidence set under docs/modernization/evidence/ui-3: images wider than 2560 px are
# downscaled to 2560 px, quality 76; the matrix records (JSON) and the filtered audit logs of the kept runs are copied next to them. Only files that match -Include are exported.
[CmdletBinding()]
param([string]$Source = 'build/ui3-evidence',
      [string]$Destination = 'docs/modernization/evidence/ui-3',
      [string[]]$Include = @('*-1920x1080-100-autoshot-mainmenu-root*', '*-1920x1080-100-autoshot-mainmenu-soloplay*', '*-1920x1080-100-autoshot-options-0*', '*-1920x1080-100-autoshot-options-2*',
                             '*-1920x1080-100-autoshot-options-5*', '*-1920x1080-100-autoshot-options-dialog*', '*-1920x1080-100-autoshot-matchsetup*', '*-1920x1080-100-autoshot-browser.png',
                             '*-1920x1080-100-autoshot-browser-dialog*', '*-1920x1080-100-autoshot-credits*', '*-3440x1440-100-autoshot-mainmenu-root*', '*-3440x1440-100-autoshot-matchsetup*',
                             '*-3440x1440-100-autoshot-options-0*', '*-5120x1440-100-autoshot-mainmenu-root*', '*-5120x1440-100-autoshot-options-0*', '*-5120x1440-100-autoshot-matchsetup*',
                             '*-3840x2160-200-autoshot-mainmenu-root*', '*-3840x2160-200-autoshot-options-0*', '*-1920x1080-300-autoshot-mainmenu-root*', '*-1920x1080-300-autoshot-options-0*',
                             '*-1920x1080-300-autoshot-matchsetup*', '*classic*'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$src = Join-Path $repo $Source
$dst = Join-Path $repo $Destination
$screens = Join-Path $dst 'screens'
New-Item -ItemType Directory -Force -Path $screens | Out-Null
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$params = New-Object System.Drawing.Imaging.EncoderParameters(1)
$params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, [long]76)
$count = 0
foreach ($file in Get-ChildItem -LiteralPath $src -Recurse -Filter *.png) {
    $keep = $false
    foreach ($pattern in $Include) { if ($file.Name -like $pattern -or $file.FullName -like $pattern) { $keep = $true } }
    if (-not $keep) { continue }
    $tag = if ($file.FullName -match 'classic') { 'classic-' } else { '' }
    $name = $tag + ($file.Name -replace '-autoshot', '' -replace '\.png$', '.jpg')
    $image = [System.Drawing.Image]::FromFile($file.FullName)
    try {
        $target = $image
        if ($image.Width -gt 2560) {
            $h = [int]($image.Height * 2560 / $image.Width)
            $bmp = New-Object System.Drawing.Bitmap(2560, $h)
            $g = [System.Drawing.Graphics]::FromImage($bmp); $g.InterpolationMode = 'HighQualityBicubic'; $g.DrawImage($image, 0, 0, 2560, $h); $g.Dispose()
            $target = $bmp
        }
        $target.Save((Join-Path $screens $name), $codec, $params)
        if ($target -ne $image) { $target.Dispose() }
    } finally { $image.Dispose() }
    ++$count
}
foreach ($file in Get-ChildItem -LiteralPath $src -Recurse -Include *-matrix.json, *.log.txt) {
    $rel = $file.FullName.Substring($src.Length).TrimStart('\', '/') -replace '[\\/]', '-'
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $dst ('logs-' + $rel)) -Force
}
Write-Host "exported $count screenshots to $Destination"
