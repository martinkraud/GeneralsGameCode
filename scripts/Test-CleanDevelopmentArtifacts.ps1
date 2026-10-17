# Safety tests for Clean-DevelopmentArtifacts.ps1. Dry run and refusal paths only: the real tree is read, never modified.
$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot 'Clean-DevelopmentArtifacts.ps1'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$failures = New-Object System.Collections.Generic.List[string]
function Expect([bool]$condition, [string]$message) {
    if (-not $condition) { $failures.Add($message); Write-Host "FAIL: $message" } else { Write-Host "ok:   $message" }
}
function Run([string[]]$arguments) {
    $output = & pwsh -NoProfile -File $script @arguments 2>&1 | Out-String
    return [pscustomobject]@{ Code = $LASTEXITCODE; Text = $output }
}
function Listing {
    $roots = @('build\dev-runtimes\native', 'build\dev-runtimes\zh')
    ($roots | ForEach-Object { $p = Join-Path $repoRoot $_; if (Test-Path $p) { Get-ChildItem -LiteralPath $p -Recurse -Depth 2 -Force -Directory | ForEach-Object { $_.FullName } } }) -join "`n"
}

$before = Listing
$dry = Run @()
Expect ($dry.Code -eq 0) 'dry run exits 0'
Expect ($dry.Text -match 'DRY RUN') 'dry run says nothing will be changed'
Expect ($dry.Text -match 'PLAN_HASH Native: [0-9A-F]{64}') 'dry run prints the native plan hash'
Expect ($dry.Text -match 'PLAN_HASH ZH: [0-9A-F]{64}') 'dry run prints the Zero Hour plan hash'
Expect ($dry.Text -match 'canonical evidence - refused') 'evidence directories are reported as refused'
Expect ($dry.Text -match 'build\\win64') 'build trees are reported, not cleaned'
Expect ((Listing) -eq $before) 'dry run leaves the runtime trees unchanged'

$noHash = Run @('-Execute')
Expect ($noHash.Code -ne 0) 'execute without any approved plan hash is refused'
$wrong = Run @('-Execute', '-ApprovedNativePlan', ('0' * 64), '-ApprovedZHPlan', ('0' * 64))
Expect ($wrong.Code -ne 0) 'execute with wrong plan hashes is refused'
$partial = Run @('-Only', 'ZH', '-Execute', '-ApprovedZHPlan', ('F' * 64))
Expect ($partial.Code -ne 0) 'execute with a wrong Zero Hour hash is refused'
Expect ((Listing) -eq $before) 'refused executions change nothing'

if ($failures.Count -gt 0) { Write-Host ("{0} FAILURE(S)" -f $failures.Count); exit 1 }
Write-Host 'All Clean-DevelopmentArtifacts safety tests passed.'
exit 0
