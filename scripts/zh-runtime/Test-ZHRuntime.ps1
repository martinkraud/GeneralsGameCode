#Requires -Version 7.0
[CmdletBinding()]
param([string]$Name = 'baseline', [switch]$CheckSource)
. "$PSScriptRoot/Common.ps1"
$manifest = Read-ZHStage $Name
if ($CheckSource) {
    $root = Get-ZHSource $manifest.SourcePath
    $inventoryPath = Join-Path $manifest.StagePath 'source-files.json'
    $expectedHash = $manifest.SourceInventorySHA256
} else {
    $root = $manifest.GamePath
    $inventoryPath = Join-Path $manifest.StagePath 'runtime-files.json'
    $expectedHash = $manifest.RuntimeInventorySHA256
}
if ((Get-FileHash -LiteralPath $inventoryPath).Hash -ne $expectedHash) { throw 'Inventory metadata changed.' }
Write-Host "Read-only full SHA256 comparison: $root"
$expected = @(Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json)
$actual = @(Get-ZHInventory $root)
$changes = @(Get-ZHInventoryDifferences $expected $actual)
if ($changes.Count) { $changes | ForEach-Object { Write-Host $_ }; throw 'Files were added, removed or changed relative to staging.' }
Write-Host "PASS: $($actual.Count) file paths, lengths and SHA256 values match the staging inventory."
Write-Host 'This comparison does not claim gameplay, performance or retail compatibility.'
