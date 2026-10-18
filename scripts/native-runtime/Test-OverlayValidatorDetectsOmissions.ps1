#Requires -Version 7.0
# X64.48B: proves that the overlay validator FAILS when the distribution rule is broken. Takes a good ZIP, makes tampered copies and expects the package
# audit (Test-ZeroHourOverlayPackage.ps1 -AuditOnly: no staging, no launch) to reject each one:
#   1. a project runtime file of the staged candidate removed from the ZIP (and from its manifest, so only the stage comparison can notice);
#   2. a legitimate, unchanged game file added to the ZIP (EA data must never be redistributed);
#   3. a project runtime file replaced by different bytes.
# The good ZIP must pass the same audit. Needs the canonical staged candidate the ZIP was built from.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Title,
      [Parameter(Mandatory)][string]$Candidate,
      [Parameter(Mandatory)][string]$Zip)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'OrdinaryHostCommon.ps1')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$validator = Join-Path $PSScriptRoot 'Test-ZeroHourOverlayPackage.ps1'
$work = Join-Path ([IO.Path]::GetTempPath()) ('overlay-negative-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $work | Out-Null
$canonical = Read-NativeOrdinaryPlayStage $Title $Candidate $script:NativeNormalLaunchGenerated -AllowCrashDumps
$legitFile = ($canonical.Assets | Where-Object { $_.Path -match '\.ini$' -or $_.Path -match '\.cur$|\.ani$' } | Select-Object -First 1)
if (-not $legitFile) { $legitFile = $canonical.Assets | Select-Object -First 1 }
function New-Tampered([string]$Name, [scriptblock]$Edit) {
    $target = Join-Path $work "$Name\$(Split-Path $Zip -Leaf)"
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    Copy-Item -LiteralPath $Zip -Destination $target
    $archive = [IO.Compression.ZipFile]::Open($target, 'Update')
    try { & $Edit $archive } finally { $archive.Dispose() }
    return $target
}
function Audit([string]$Path) {
    $null = & pwsh -NoProfile -File $validator -Title $Title -Candidate $Candidate -Zip $Path -AuditOnly 2>&1
    return $LASTEXITCODE
}
$failures = @()
if ((Audit $Zip) -ne 0) { $failures += 'the GOOD package did not pass the audit' }
# a project file of the stage other than the executable (Generals: the INI/WND overrides; Zero Hour has only the executable -> the executable under its Steam name)
$gameRoot = [IO.Path]::GetFullPath($canonical.GamePath).TrimEnd('\') + '\'
$victimFile = Get-ChildItem -LiteralPath $canonical.GamePath -Recurse -File | Where-Object { (($_.FullName.Substring($gameRoot.Length)) -replace '\\', '/') -match '^(Data/INI|Window)/' } | Select-Object -First 1
$victimEntry = if ($victimFile) { ($victimFile.FullName.Substring($gameRoot.Length)) -replace '\\', '/' } else { 'Generals.exe' }
$t1 = New-Tampered 'omitted' { param($a)
    $a.GetEntry($victimEntry).Delete()
    $m = $a.GetEntry('MKRaudstein/manifest.json'); $rs = $m.Open(); $reader = New-Object IO.StreamReader($rs); $text = $reader.ReadToEnd(); $reader.Dispose(); $rs.Dispose(); $m.Delete()
    $json = $text | ConvertFrom-Json; $json.files = @($json.files | Where-Object { ($_.file -replace '\\', '/') -ne $victimEntry })
    $e = $a.CreateEntry('MKRaudstein/manifest.json'); $w = New-Object IO.StreamWriter($e.Open()); $w.Write(($json | ConvertTo-Json -Depth 6)); $w.Dispose() }
if ((Audit $t1) -eq 0) { $failures += "a missing project file ($victimEntry) was NOT detected" }
$t2 = New-Tampered 'legitimate' { param($a)
    $source = Join-Path $canonical.GamePath ($legitFile.Path -replace '/', '\')
    $e = $a.CreateEntry(($legitFile.Path -replace '\\', '/')); $s = $e.Open(); $b = [IO.File]::ReadAllBytes($source); $s.Write($b, 0, $b.Length); $s.Dispose() }
if ((Audit $t2) -eq 0) { $failures += "a redistributed legitimate game file ($($legitFile.Path)) was NOT detected" }
$t3 = New-Tampered 'modified' { param($a)
    $e = $a.GetEntry($victimEntry); $e.Delete(); $n = $a.CreateEntry($victimEntry); $s = $n.Open(); $b = [Text.Encoding]::ASCII.GetBytes('tampered'); $s.Write($b, 0, $b.Length); $s.Dispose() }
if ((Audit $t3) -eq 0) { $failures += "a modified project file ($victimEntry) was NOT detected" }
Remove-Item -LiteralPath $work -Recurse -Force
if ($failures.Count) { throw "Validator negative test FAILED:`n$($failures -join "`n")" }
Write-Host "PASS: the validator accepts the good package and rejects an omitted project file ($victimEntry), a redistributed legitimate file ($($legitFile.Path)) and a modified project file."
