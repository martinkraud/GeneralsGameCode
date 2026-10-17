# Safety tests for Cleanup-ZHStages.ps1 (runs entirely inside a temporary tree; never touches real stages).
$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot 'Cleanup-ZHStages.ps1'
$work = Join-Path ([IO.Path]::GetTempPath()) ('zh-cleanup-test-' + [Guid]::NewGuid().ToString('N'))
$zh = Join-Path $work 'dev-runtimes\zh'
$archive = Join-Path $work 'evidence-archive\zh-stages'
$failures = New-Object System.Collections.Generic.List[string]

function Expect([bool]$condition, [string]$message) {
    if (-not $condition) { $failures.Add($message); Write-Host "FAIL: $message" } else { Write-Host "ok:   $message" }
}
function New-Stage([string]$name, [int]$ageMinutes, [string]$Extra) {
    $dir = Join-Path $zh $name
    New-Item -ItemType Directory -Force -Path (Join-Path $dir 'game') | Out-Null
    Set-Content -LiteralPath (Join-Path $dir 'game\generalszh.exe') -Value ('exe ' + $name)
    Set-Content -LiteralPath (Join-Path $dir 'manifest.json') -Value ('{"name":"' + $name + '"}')
    Set-Content -LiteralPath (Join-Path $dir 'launch-0123456789abcdef.json') -Value '{"launch":1}'
    if ($Extra) { Set-Content -LiteralPath (Join-Path $dir $Extra) -Value 'unexpected' }
    (Get-Item -LiteralPath $dir).LastWriteTime = (Get-Date).AddMinutes(-$ageMinutes)
}
function Run([string[]]$arguments) {
    $output = & pwsh -NoProfile -File $script @arguments 2>&1 | Out-String
    return [pscustomobject]@{ Code = $LASTEXITCODE; Text = $output }
}
function Plan-Hash([string]$text) { if ($text -match 'PLAN_HASH: ([0-9A-F]{64})') { return $Matches[1] } return $null }
function Snapshot { return ((Get-ChildItem -LiteralPath $work -Recurse -Force | ForEach-Object { $_.FullName.Substring($work.Length) + '|' + $_.Length }) -join "`n") }

try {
    New-Stage 'stage2-old' 900
    New-Stage 'stage3-protected' 800
    New-Stage 'stage4-ambiguous' 700 'mystery.bin'
    New-Stage 'stage5-mid' 400
    New-Stage 'stage6-newer' 300
    New-Stage 'stage7-active' 10
    New-Stage 'workflow-check-0123456789ab' 100
    New-Stage 'random-folder' 600
    New-Stage 'stage1-junction' 950
    $outside = Join-Path $work 'outside'
    New-Item -ItemType Directory -Force -Path $outside | Out-Null
    Set-Content -LiteralPath (Join-Path $outside 'precious.txt') -Value 'do not touch'
    New-Item -ItemType Junction -Path (Join-Path $zh 'stage1-junction\game\link') -Target $outside | Out-Null

    $common = @('-ZHRoot', $zh, '-ArchiveRoot', $archive, '-Active', 'stage7-active', '-Protect', 'stage3-protected', '-KeepNewest', '2',
                '-PolicyFile', (Join-Path $work 'no-policy.json'))

    # 1. dry run changes nothing and prints a plan hash and the expected decisions
    $before = Snapshot
    $dry = Run $common
    Expect ($dry.Code -eq 0) 'dry run exits 0'
    $hash = Plan-Hash $dry.Text
    Expect ($null -ne $hash) 'dry run prints PLAN_HASH'
    Expect ($dry.Text -match 'DRY RUN') 'dry run says nothing will be changed'
    Expect ((Snapshot) -eq $before) 'dry run leaves the tree byte-for-byte unchanged'
    Expect ($dry.Text -match 'REMOVE\s+stage2-old') 'superseded stage is planned for removal'
    Expect ($dry.Text -match 'REMOVE\s+stage5-mid') 'older non-protected stage is planned for removal'
    Expect ($dry.Text -match 'REMOVE\s+workflow-check-0123456789ab') 'workflow-check leftover is planned for removal'
    Expect ($dry.Text -match 'KEEP\s+stage3-protected') 'protected stage is kept'
    Expect ($dry.Text -match 'KEEP\s+stage7-active') 'active stage is kept'
    Expect ($dry.Text -match 'KEEP\s+stage6-newer') 'newest stages (KeepNewest=2 includes the active one) kept'
    Expect ($dry.Text -match 'AMBIGUOUS\s+stage4-ambiguous') 'unexpected entry makes a stage AMBIGUOUS'
    Expect ($dry.Text -match 'AMBIGUOUS\s+random-folder') 'unexpected directory name is AMBIGUOUS'
    Expect ($dry.Text -match 'AMBIGUOUS\s+stage1-junction') 'a junction inside a stage makes it AMBIGUOUS'

    # 2. execute needs the approved plan hash
    $noHash = Run ($common + '-Execute')
    Expect ($noHash.Code -ne 0) 'execute without -ApprovedPlan is refused'
    $wrong = Run ($common + @('-Execute', '-ApprovedPlan', ('0' * 64)))
    Expect ($wrong.Code -ne 0) 'execute with a wrong plan hash is refused'
    Expect ((Snapshot) -eq $before) 'refused executions change nothing'

    # 3. a change after the dry run invalidates the plan
    New-Item -ItemType Directory -Force -Path (Join-Path $zh 'stage2-old\game\extra') | Out-Null
    Set-Content -LiteralPath (Join-Path $zh 'stage2-old\game\extra\more.bin') -Value ('x' * 4096)
    $stale = Run ($common + @('-Execute', '-ApprovedPlan', $hash))
    Expect ($stale.Code -ne 0) 'a plan hash from before a tree change is refused'
    $dry2 = Run $common
    $hash2 = Plan-Hash $dry2.Text
    Expect ($hash2 -ne $hash) 'the plan hash changes when the tree changes'

    # 4. root and archive guards
    $badRoot = Run @('-ZHRoot', $outside)
    Expect ($badRoot.Code -ne 0) 'a root that is not dev-runtimes\zh is refused'
    $steam = Join-Path $work 'Steam\dev-runtimes\zh'
    New-Item -ItemType Directory -Force -Path $steam | Out-Null
    $steamRun = Run @('-ZHRoot', $steam)
    Expect ($steamRun.Code -ne 0) 'a Steam-looking root is refused'
    $insideArchive = Run @('-ZHRoot', $zh, '-ArchiveRoot', (Join-Path $zh 'archive'))
    Expect ($insideArchive.Code -ne 0) 'an archive root inside the zh root is refused'

    # 5. execute the reviewed plan
    $exec = Run ($common + @('-Execute', '-ApprovedPlan', $hash2))
    Expect ($exec.Code -eq 0) 'execute with the matching plan hash succeeds'
    Expect (-not (Test-Path (Join-Path $zh 'stage2-old'))) 'superseded stage removed'
    Expect (-not (Test-Path (Join-Path $zh 'stage5-mid'))) 'older stage removed'
    Expect (-not (Test-Path (Join-Path $zh 'workflow-check-0123456789ab'))) 'workflow-check leftover removed'
    foreach ($kept in 'stage3-protected', 'stage7-active', 'stage6-newer', 'stage4-ambiguous', 'random-folder', 'stage1-junction') {
        Expect (Test-Path (Join-Path $zh $kept)) "kept: $kept"
    }
    Expect (Test-Path (Join-Path $outside 'precious.txt')) 'the junction target outside the tree is untouched'
    Expect (Test-Path (Join-Path $archive 'stage2-old\manifest.json')) 'removed stage metadata archived (manifest)'
    Expect (Test-Path (Join-Path $archive 'stage2-old\launch-0123456789abcdef.json')) 'removed stage metadata archived (launch receipt)'
    Expect (-not (Test-Path (Join-Path $archive 'workflow-check-0123456789ab'))) 'nothing is archived for workflow-check leftovers'
}
finally {
    if (Test-Path -LiteralPath $work) {
        $j = Join-Path $zh 'stage1-junction\game\link'
        if (Test-Path -LiteralPath $j) { (Get-Item -LiteralPath $j -Force).Delete() }
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}
if ($failures.Count -gt 0) { Write-Host ("{0} FAILURE(S)" -f $failures.Count); exit 1 }
Write-Host 'All ZH cleanup safety tests passed.'
exit 0
