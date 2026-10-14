#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Name,
      [Parameter(Mandatory)][string]$AssetSource,
      [switch]$ModernExtensions)
. (Join-Path $PSScriptRoot 'Common.ps1')
$stage=Get-NativeStage $Title $Name
if (Test-Path -LiteralPath $stage) { throw 'Fresh stage name required; existing evidence is never overwritten' }
$source=Get-ZHPath $AssetSource; Assert-ZHNoLinks $source
if (Test-ZHWithin $source (Join-Path $script:ZHRepository 'build')) { throw 'Use installed title assets, not another staged runtime' }
$required=if ($Title -eq 'Generals') { @('INI.big','W3D.big','Terrain.big','Textures.big','Window.big','English.big') }
          else { @('INIZH.big','W3DZH.big','TerrainZH.big','TexturesZH.big','WindowZH.big','EnglishZH.big',
              'ZH_Generals/INI.big','ZH_Generals/W3D.big','ZH_Generals/Terrain.big','ZH_Generals/English.big') }
foreach ($file in $required) {
    if (!(Test-Path -LiteralPath (Join-Path $source $file) -PathType Leaf)) { throw "Missing title asset $file" }
}
if (!(Test-Path -LiteralPath (Join-Path $source 'Data') -PathType Container)) { throw 'Missing Data assets' }
$build=Get-NativeBuild $Title
$null=Get-NativePE $build.Executable
$assets=@(Get-ZHTree $source | Where-Object {
    !$_.PSIsContainer -and ($_.DirectoryName -eq $source -and $_.Extension -ieq '.big' -or
        (Test-ZHWithin $_.FullName (Join-Path $source 'Data')) -or
        ($Title -eq 'ZeroHour' -and ($_.DirectoryName -eq (Join-Path $source 'ZH_Generals') -and $_.Extension -ieq '.big' -or
            (Test-ZHWithin $_.FullName (Join-Path $source 'ZH_Generals/Data'))))) -and $_.Extension -notin @('.exe','.dll','.pdb')
} | Sort-Object FullName | ForEach-Object {
    [pscustomobject]@{Path=[IO.Path]::GetRelativePath($source,$_.FullName);Length=$_.Length;SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
$game=Join-Path $stage 'game'
New-Item -ItemType Directory -Path $game -Force | Out-Null
foreach ($entry in $assets) {
    $target=Join-Path $game $entry.Path
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $source $entry.Path) -Destination $target
    if ((Get-FileHash -LiteralPath $target).Hash -ne $entry.SHA256) { throw "Asset copy mismatch: $($entry.Path)" }
}
if ($ModernExtensions) {
    # X64.32: Zero Hour style shortcut sidebar for Generals, generated from the installed retail archives as loose
    # override files (removing them restores retail). Classic staging simply omits this switch.
    if ($Title -ne 'Generals') { throw 'Modern extensions are Generals-only' }
    $python=Get-Command python -ErrorAction SilentlyContinue
    if (!$python) { throw 'python is required for -ModernExtensions' }
    & $python.Source (Join-Path $PSScriptRoot 'modern_extensions.py') --game $source --out $game
    if ($LASTEXITCODE -ne 0) { throw 'modern_extensions.py failed' }
}
foreach ($file in @($build.Executable,$build.Pdb)) { Copy-Item -LiteralPath $file -Destination $game }
$dependencies=@(Assert-NativeDependencies (Join-Path $game ([IO.Path]::GetFileName($build.Executable))) $game)
Write-ZHJson (Join-Path $stage 'manifest.json') ([pscustomobject]@{
    Schema=1;Title=$Title;StagePath=$stage;GamePath=$game;AssetSource=$source;Assets=$assets
    Build=$build;Dependencies=$dependencies;Inventory=@(Get-ZHInventory $game)
    Mode='headless init-only; no execute/replay/simulation'; CreatedUtc=[DateTime]::UtcNow.ToString('o')
})
Write-Host "Staged without launching: $stage"
