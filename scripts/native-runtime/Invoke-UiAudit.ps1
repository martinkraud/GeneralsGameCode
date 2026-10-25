#Requires -Version 7.0
# UI.2: generic frontend audit of a staged candidate (Wraps Invoke-NativeAutoCapture.ps1 -Flow <Flow>): geometry / overlap / text fit / mouse hit / focus checks at one or more client sizes and UI scales. Checkpoint lines of the flow are filtered with -Filter.
# or more client sizes (GENERALS_FORCE_CLIENT_SIZE). Wraps Invoke-NativeAutoCapture.ps1 -Flow MatchOptions.
# The flow only opens the Skirmish screen and its dropdowns and quits (it never leaves the screen through a button), so nothing is written to the user's data; as a guard the
# preference / settings files of the game's user-data folders are hashed before and after, and any difference is reported (and the file restored from the copy taken before).
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [string[]]$ClientSizes = @('1920x1080'),
      [string[]]$UiScales = @(''),                       # '' = the stage's own setting (Auto), else 75..300

      [string]$Flow = 'MainMenu',
      [string]$Filter = 'mainmenu',
      [string]$Disable = '',
      [switch]$Classic,
      [string]$Out = '')
$ErrorActionPreference = 'Stop'
$ClientSizes = @($ClientSizes | ForEach-Object { $_ -split ',' } | Where-Object { $_ }); $UiScales = @($UiScales | ForEach-Object { $_ -split ',' })
$docs = [Environment]::GetFolderPath('MyDocuments')
$dataDirs = @(Get-ChildItem -LiteralPath $docs -Directory -Filter 'Command and Conquer Generals*Data' -ErrorAction SilentlyContinue)
$backup = Join-Path ([IO.Path]::GetTempPath()) ("matchopts-userdata-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $backup | Out-Null
$before = @{}
foreach ($d in $dataDirs) {
    foreach ($f in Get-ChildItem -LiteralPath $d.FullName -File -Filter *.ini -ErrorAction SilentlyContinue) {
        $before[$f.FullName] = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
        Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $backup ([Guid]::NewGuid().ToString('N') + '.ini')) -Force
        Set-Content -LiteralPath (Join-Path $backup ($before[$f.FullName] + '.src')) -Value $f.FullName
    }
}
$results = @()
foreach ($scaleChoice in $UiScales) { foreach ($size in $ClientSizes) {
    $label = if ($scaleChoice) { "$size@$scaleChoice" } else { $size }
    Write-Host "== $Title $Name client $size ui scale $(if ($scaleChoice) { $scaleChoice } else { 'auto' })"
    $captureArgs = @{ Title = $Title; Name = $Name; Flow = $Flow; ClientSize = $size; TimeoutSeconds = 240 }
    if ($scaleChoice) { $captureArgs.UiScale = $scaleChoice }
    if ($Disable) { $captureArgs.Disable = $Disable }
    if ($Classic) { $captureArgs.Classic = $true }
    for ($w = 0; $w -lt 60 -and @(Get-Process -Name generals, generalsv, generalszh -ErrorAction SilentlyContinue).Count; ++$w) { Start-Sleep -Seconds 1 }   # a previous game must have exited
    $knownDirs = @(Get-ChildItem (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'build\diag') -Directory | Where-Object { $_.Name -like "autocapture-*-$Title-$Name" } | ForEach-Object { $_.Name })
    try { & (Join-Path $PSScriptRoot 'Invoke-NativeAutoCapture.ps1') @captureArgs | Out-Host } catch { Write-Warning "capture failed: $_" }
    $dir = Get-ChildItem (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'build\diag') -Directory | Where-Object { $_.Name -like "autocapture-*-$Title-$Name" -and $knownDirs -notcontains $_.Name } | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $dir) { Write-Warning "no new capture directory for $label"; $results += [pscustomobject]@{ Size = $label; Dir = ''; Lines = 0; Defects = -1 }; continue }
    $log = Join-Path $dir.FullName 'generals64.log'
    $lines = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | Where-Object { $_ -match $Filter } } else { @() }
    $defects = @($lines | Where-Object { $_ -match '\bdefect\b' })
    $results += [pscustomobject]@{ Size = $label; Dir = $dir.FullName; Lines = $lines.Count; Defects = $defects.Count }
    $lines | ForEach-Object { Write-Host "   $_" }
    if ($Out) {
        New-Item -ItemType Directory -Force -Path $Out | Out-Null
        foreach ($f in Get-ChildItem -LiteralPath $dir.FullName -Filter "autoshot-$Filter*.png") { Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $Out ("$Title-$label-" + $f.Name)) -Force }
        $lines | Set-Content -LiteralPath (Join-Path $Out "$Title-$label-$Filter.txt")
    }
} }
# user-data guard
$changed = 0
foreach ($path in $before.Keys) {
    $now = if (Test-Path -LiteralPath $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { '' }
    if ($now -ne $before[$path]) {
        ++$changed
        $copy = Get-ChildItem -LiteralPath $backup -Filter '*.ini' | Where-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq $before[$path] } | Select-Object -First 1
        Write-Warning "user data file changed during the audit: $path"
        if ($copy) { Copy-Item -LiteralPath $copy.FullName -Destination $path -Force; Write-Warning 'restored from the copy taken before' }
    }
}
Remove-Item -LiteralPath $backup -Recurse -Force
$results | Format-Table -AutoSize | Out-Host
Write-Host ("user data files checked: {0}, changed: {1}" -f $before.Count, $changed)
