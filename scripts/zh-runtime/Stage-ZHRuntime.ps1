#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourcePath,
    [string]$Name = 'baseline',
    [ValidateSet('win32', 'win32-profile')][string]$BuildPreset = 'win32',
    [switch]$Refresh,
    [Alias('WhatIf')][switch]$ValidateOnly
)
. "$PSScriptRoot/Common.ps1"
Assert-ZHNoGameProcess
$source = Get-ZHSource $SourcePath
$stage = Get-ZHStage $Name
if (Test-Path -LiteralPath $stage) {
    if (-not $Refresh) { throw 'Destination exists. Use a new -Name or -Refresh (creates a new sibling; retains old runtime).' }
    $Name = $Name.Substring(0, [Math]::Min(20, $Name.Length)) + '-' + [DateTime]::UtcNow.ToString('yyyyMMddHHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 6)
    $stage = Get-ZHStage $Name
}
if ((Test-ZHWithin $stage $source) -or (Test-ZHWithin $source $stage)) { throw 'Source and destination overlap.' }
if (Test-Path -LiteralPath $stage) { throw 'Fresh destination unexpectedly exists.' }
$tree = @(Get-ZHTree $source)
$build = Get-ZHBuild $BuildPreset
$bytes = ($tree | Where-Object { -not $_.PSIsContainer } | Measure-Object Length -Sum).Sum
Write-Host "Validated explicit Steam source (structural validation, not license authentication): $source"
Write-Host "Complete copy: $($tree.Count) entries, $bytes file bytes; includes existing wrappers/modifications."
Write-Host "New stage: $stage"
Write-Host "Build: $($build.Executable); EXE/PDB GUID $($build.DebugGuid), age $($build.DebugAge) match."
Write-Host 'Plan: hash source; copy all directories/files; verify each copy; overlay EXE/PDB; write provenance. Never install, delete or link.'
if ($ValidateOnly) { Write-Host 'VALIDATION ONLY: no files written and no game launched.'; return }

Write-Host 'Hashing complete source tree. This can take several minutes.'
$inventory = @(Get-ZHInventory $source)
Assert-ZHNoLinks $stage
New-Item -ItemType Directory -Path $stage | Out-Null
$game = Join-Path $stage 'game'
# No completed manifest is written if any copy/hash operation fails. Partial stages are retained.
Copy-ZHTree $source $game $inventory
foreach ($artifact in @($build.Executable, $build.Pdb)) {
    $target = Join-Path $game (Split-Path $artifact -Leaf)
    Write-Host "Overlay development artifact in new runtime only: $target"
    Copy-Item -LiteralPath $artifact -Destination $target -Force
}
if ((Get-FileHash -LiteralPath (Join-Path $game 'generalszh.exe')).Hash -ne $build.ExecutableSHA256 -or
    (Get-FileHash -LiteralPath (Join-Path $game 'generalszh.pdb')).Hash -ne $build.PdbSHA256) { throw 'Build artifacts changed during staging.' }
Assert-ZHAssets $game
Write-Host 'Rechecking complete source inventory for changes during staging.'
if (@(Get-ZHInventoryDifferences $inventory @(Get-ZHInventory $source)).Count) {
    throw 'Source changed during staging. Partial runtime retained without a completed manifest; retry with a new name.'
}
$revision = & git -C $script:ZHRepository rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot record source revision.' }
$status = @(& git -C $script:ZHRepository status --short --untracked-files=all)
if ($LASTEXITCODE -ne 0) { throw 'Cannot record worktree state.' }
Write-ZHJson (Join-Path $stage 'source-files.json') $inventory
Write-Host 'Hashing final runtime inventory.'
Write-ZHJson (Join-Path $stage 'runtime-files.json') @(Get-ZHInventory $game)
$scripts = @(Get-ZHInventory $PSScriptRoot)
$manifest = [ordered]@{
    Schema = 1; CreatedUtc = [DateTime]::UtcNow.ToString('o'); SourcePath = $source
    StagePath = $stage; GamePath = $game; Name = $Name; Revision = [string]$revision
    WorktreeStatus = $status; Build = $build; PowerShell = $PSVersionTable.PSVersion.ToString()
    Scripts = $scripts; SourceFileCount = $inventory.Count; SourceBytes = $bytes
    SourceInventorySHA256 = (Get-FileHash -LiteralPath (Join-Path $stage 'source-files.json')).Hash
    RuntimeInventorySHA256 = (Get-FileHash -LiteralPath (Join-Path $stage 'runtime-files.json')).Hash
    ImportantRuntimeFiles = @(Get-ZHInventory $game | Where-Object { $_.Path -match '(?i)(\.dll$|d3d8\.cfg$|generalszh\.(exe|pdb)$)' })
    RuntimeTests = 'NOT RUN'; RetailCompatibility = 'NOT ESTABLISHED; modern MSVC build'
}
Write-ZHJson (Join-Path $stage 'manifest.json') $manifest
Write-Host "Completed staging: $stage"
Write-Host "Next: ./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name '$Name' -ValidateOnly"
Write-Host 'Game was not launched. User-data protection is required before launch.'
