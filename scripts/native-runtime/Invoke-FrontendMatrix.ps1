#Requires -Version 7.0
# UI.3: the Modern Frontend validation matrix. Runs the audit flows of the migrated screens (Invoke-UiAudit.ps1 -> Invoke-NativeAutoCapture.ps1 -Flow ...) for every client size x UI scale on a staged
# candidate and reduces every run to one record: defects, whether the flow completed, the effective scale the layout settled on, the frontend cost line. Capture directories are deleted after parsing
# (a run is ~60 MB of bitmaps); screenshots are kept only for the runs that match -KeepShots ("<size>@<scale>" patterns), as PNG, under -Out.
#
#   pwsh -File scripts/native-runtime/Invoke-FrontendMatrix.ps1 -Title ZeroHour -Name x64-ui3-zh -Out build/ui3-evidence/zh
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals', 'ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [string[]]$Flows = @('MainMenu', 'Options', 'MatchSetup', 'Replay'),
      [string[]]$ClientSizes = @('1920x1080', '2560x1440', '3440x1440', '3840x2160', '5120x1440'),
      [string[]]$UiScales = @('75', '100', '150', '200', '300'),
      [string[]]$KeepShots = @('1920x1080@100', '3440x1440@100', '5120x1440@100', '3840x2160@200', '1920x1080@300'),
      [string]$Out = '',
      [switch]$Classic)
$ErrorActionPreference = 'Stop'
# `pwsh -File` passes a comma list as one string
$Flows = @($Flows | ForEach-Object { $_ -split ',' } | Where-Object { $_ }); $ClientSizes = @($ClientSizes | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$UiScales = @($UiScales | ForEach-Object { $_ -split ',' } | Where-Object { $_ }); $KeepShots = @($KeepShots | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$diag = Join-Path $repo 'build\diag'
$filterOf = @{ MainMenu = 'mainmenu|cost'; Options = 'options|cost'; MatchSetup = 'matchsetup|matchopts|cost'; Replay = 'browser|cost'; LoadGame = 'browser|cost'; Credits = 'credits|cost' }
$shotFilter = @{ MainMenu = 'mainmenu'; Options = 'options'; MatchSetup = 'matchsetup'; Replay = 'browser'; LoadGame = 'browser'; Credits = 'credits' }
$records = @()
if ($Out) { New-Item -ItemType Directory -Force -Path $Out | Out-Null }
foreach ($flow in $Flows) {
    foreach ($size in $ClientSizes) {
        foreach ($scale in $UiScales) {
            $label = "$size@$scale"
            for ($w = 0; $w -lt 60 -and @(Get-Process -Name generals, generalsv, generalszh -ErrorAction SilentlyContinue).Count; ++$w) { Start-Sleep -Seconds 1 }
            $known = @(Get-ChildItem $diag -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "autocapture-*-$Title-$Name" } | ForEach-Object { $_.Name })
            $arguments = @{ Title = $Title; Name = $Name; Flow = $flow; Filter = ($shotFilter[$flow]); ClientSizes = @($size); UiScales = @($scale) }
            if ($Classic) { $arguments.Classic = $true }
            try { & (Join-Path $PSScriptRoot 'Invoke-UiAudit.ps1') @arguments 6>&1 *> $null } catch { Write-Warning "run failed: $flow $label : $_" }
            $dir = Get-ChildItem $diag -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "autocapture-*-$Title-$Name" -and $known -notcontains $_.Name } | Sort-Object LastWriteTime | Select-Object -Last 1
            $record = [ordered]@{ Flow = $flow; Title = $Title; Size = $size; Scale = $scale; Classic = [bool]$Classic; Completed = $false; Defects = -1; EffectiveScale = ''; Cost = ''; Dir = '' }
            if ($dir) {
                $log = Join-Path $dir.FullName 'generals64.log'
                $lines = if (Test-Path -LiteralPath $log) { @(Get-Content -LiteralPath $log) } else { @() }
                $interesting = @($lines | Where-Object { $_ -match $filterOf[$flow] })
                $record.Defects = @($interesting | Where-Object { $_ -match '\bdefect\b' }).Count
                $record.Completed = [bool]($lines | Where-Object { $_ -match 'autoflow\s+observed' } | Select-Object -Last 1) -and [bool]($lines | Where-Object { $_ -match 'WinMain\s+complete' })
                $geometry = $lines | Where-Object { $_ -match 'effective_scale=(\d+)' } | Select-Object -First 1
                if ($geometry -and $geometry -match 'effective_scale=(\d+)') { $record.EffectiveScale = $Matches[1] }
                $cost = $interesting | Where-Object { $_ -match '\bcost\b.*frontend|idle frames=' } | Select-Object -Last 1
                if ($cost) { $record.Cost = (($cost -split "`t")[-1]).Trim() }
                $keep = $false
                foreach ($pattern in $KeepShots) { if ($label -like $pattern) { $keep = $true } }
                if ($Out -and $keep) {
                    foreach ($f in Get-ChildItem -LiteralPath $dir.FullName -Filter "autoshot-$($shotFilter[$flow])*.png") { Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $Out ("$Title-$flow-$size-$scale-" + $f.Name)) -Force }
                    $interesting | Set-Content -LiteralPath (Join-Path $Out "$Title-$flow-$size-$scale.log.txt")
                }
                Remove-Item -LiteralPath $dir.FullName -Recurse -Force -ErrorAction SilentlyContinue
            }
            $records += [pscustomobject]$record
            Write-Host ("{0,-10} {1,-9} {2,-10} {3,4}%  completed={4} defects={5} effective={6}" -f $flow, $Title, $size, $scale, $record.Completed, $record.Defects, $record.EffectiveScale)
        }
    }
}
$bad = @($records | Where-Object { -not $_.Completed -or $_.Defects -ne 0 })
Write-Host ("matrix runs: {0}, failed or with defects: {1}" -f $records.Count, $bad.Count)
if ($Out) { $records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Out "$Title-matrix.json") }
$bad | Format-Table Flow, Size, Scale, Completed, Defects, EffectiveScale -AutoSize | Out-Host
if ($bad.Count) { exit 1 }
