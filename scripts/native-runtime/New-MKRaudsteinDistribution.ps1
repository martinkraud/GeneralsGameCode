#Requires -Version 7.0
# DISTRIBUTION-AUDIT: the ONE normal distribution command. A single invocation builds, packages, manifests, hashes and validates BOTH titles
# (Generals AND Zero Hour), so a distribution can never again be produced for one title and forgotten for the other.
#
#   pwsh scripts/native-runtime/New-MKRaudsteinDistribution.ps1                  # stage both fresh candidates from the current win64 build, package, validate both
#   pwsh scripts/native-runtime/New-MKRaudsteinDistribution.ps1 -Build           # ... after (re)building the two player executables first
#   pwsh scripts/native-runtime/New-MKRaudsteinDistribution.ps1 -GeneralsCandidate X -ZeroHourCandidate Y   # package already staged candidates
#
# Per title this runs the existing single-title tools (kept for development): New-ZeroHourOverlayPackage.ps1 (-Title ...) then
# Test-ZeroHourOverlayPackage.ps1 (-Title ...), which proves  staged runtime == clean install + ZIP  file-for-file and by feature probe.
# It then writes one combined manifest + SHA256SUMS for both titles and fails (non-zero) if ANY step for EITHER title fails.
# The Steam installations are only read (by the stager and the validator's disposable copy). Nothing is installed and no real game directory is written.
[CmdletBinding()]
param([string]$GeneralsCandidate,
      [string]$ZeroHourCandidate,
      [switch]$Build,                 # cmake --build build/win64 --config Release --target g_generals z_generals first
      [switch]$SkipValidation,        # development only: package without running the validator (a distribution MUST be validated)
      [string]$OutputDirectory,
      [string]$Version,
      [switch]$AllowDirtyTree)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
$repo = $script:ZHRepository
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'build\dist' }
$steamZh = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'
$titles = @(
    @{ Title = 'Generals'; Candidate = $GeneralsCandidate; Assets = (Join-Path $steamZh 'ZH_Generals'); Suffix = 'gen' },
    @{ Title = 'ZeroHour'; Candidate = $ZeroHourCandidate; Assets = $steamZh; Suffix = 'zh' })

Push-Location $repo
try { $commit = (git rev-parse --short=9 HEAD).Trim() } finally { Pop-Location }
if (-not $Version) { $Version = $commit }

if ($Build) {
    $cmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path -LiteralPath $cmake)) { $cmake = 'cmake' }
    & $cmake --build (Join-Path $repo 'build\win64') --config Release --target g_generals z_generals
    if ($LASTEXITCODE) { throw "Build of the two player executables failed ($LASTEXITCODE)." }
}

$summary = [System.Collections.Generic.List[object]]::new()
foreach ($t in $titles) {
    $candidate = $t.Candidate
    if (-not $candidate) {
        $candidate = "dist-$commit-$($t.Suffix)"
        $stagePath = Join-Path $repo "build\dev-runtimes\native\$($t.Title)\$candidate"
        if (Test-Path -LiteralPath $stagePath) { Write-Host "Reusing the already staged candidate $candidate" }
        else {
            Write-Host "== staging $($t.Title) candidate $candidate from the current win64 build =="
            & (Join-Path $PSScriptRoot 'Stage-NativeOrdinaryPlay.ps1') -Title $t.Title -Name $candidate -AssetSource $t.Assets | Out-Null
        }
    }
    Write-Host "== packaging $($t.Title) from $candidate =="
    $pkgArgs = @{ Title = $t.Title; Candidate = $candidate; OutputDirectory = $OutputDirectory; Version = $Version }
    if ($AllowDirtyTree) { $pkgArgs.AllowDirtyTree = $true }
    & (Join-Path $PSScriptRoot 'New-ZeroHourOverlayPackage.ps1') @pkgArgs
    $zip = Join-Path $OutputDirectory "MKRaudstein-$($t.Title)-x64-Preview-$Version.zip"
    if (-not (Test-Path -LiteralPath $zip)) { throw "Expected package missing: $zip" }
    $validated = $false
    if (-not $SkipValidation) {
        Write-Host "== validating $($t.Title) =="
        & (Join-Path $PSScriptRoot 'Test-ZeroHourOverlayPackage.ps1') -Title $t.Title -Candidate $candidate -Zip $zip
        $validated = $true
    }
    $symbols = Join-Path $OutputDirectory "MKRaudstein-$($t.Title)-x64-Preview-$Version-Symbols.zip"
    $entries = [IO.Compression.ZipFile]::OpenRead($zip)
    try { $count = $entries.Entries.Count } finally { $entries.Dispose() }
    $summary.Add([ordered]@{ title = $t.Title; candidate = $candidate; zip = (Split-Path $zip -Leaf); bytes = (Get-Item $zip).Length; files = $count
                             sha256 = (Get-FileHash $zip -Algorithm SHA256).Hash; symbols = if (Test-Path $symbols) { Split-Path $symbols -Leaf } else { $null }
                             validated = $validated })
}

# One combined record for the distribution: both titles, one commit, one hash list.
$combined = [ordered]@{ distribution = 'MKRaudstein x64 Preview'; version = $Version; commit = $commit; titles = $summary }
$combinedPath = Join-Path $OutputDirectory "MKRaudstein-x64-Preview-$Version.distribution.json"
$combined | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $combinedPath -Encoding utf8
($summary | ForEach-Object { "$($_.sha256)  $($_.zip)" }) | Set-Content -LiteralPath (Join-Path $OutputDirectory "MKRaudstein-x64-Preview-$Version.SHA256SUMS") -Encoding ascii
Write-Host ''
foreach ($s in $summary) { Write-Host ("{0,-9} {1} files {2:N0} B  SHA-256 {3}  validated={4}" -f $s.title, $s.files, $s.bytes, $s.sha256, $s.validated) }
if (-not $SkipValidation -and @($summary | Where-Object { -not $_.validated }).Count) { throw 'A title was not validated.' }
Write-Host "DISTRIBUTION READY for BOTH titles: $combinedPath"
