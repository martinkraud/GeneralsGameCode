# Native candidate retention policy and cleanup workflow

Why: every staged native candidate (`build/dev-runtimes/native/<Title>/<name>`) carries a full copy of the retail
`.big` assets (~1.5 GB Generals, ~2.7 GB Zero Hour). Without a policy ~65 candidates grew the repository to ~200 GB and
filled the development disk (cleaned up once: 199.7 GB -> 90.6 GB). This document prevents recurrence; it is a
conservative policy, not a space-maximizing one.

## 1. What is kept, what is regenerable
| class | examples | rule |
|---|---|---|
| Source, Git, docs, tests, scripts, build trees | everything outside `build/dev-runtimes` | **never touched** by cleanup |
| Active candidate | the one under native review right now | KEEP |
| Latest accepted candidate | comparison/rollback target | KEEP |
| Protected/reference candidates | listed in `scripts/native-runtime/native-retention.json` or `-Protect` | KEEP |
| Newest N per title (default 2) | recent work | KEEP |
| Run evidence | `runs\`, `normal-launch\` (crash dumps, replays, logs, screenshots) and the small stage metadata `manifest.json`, `ordinaryplay.json`, `graphics.json` | **archived and verified before** a candidate is removed (`build/evidence-archive/native-candidates/<Title>/<name>/`) |
| Candidate payload | staged retail `.big` assets, `generalsv.exe/.pdb`, INI/WND overrides | regenerable from Git + the legitimate installation with `Stage-NativeOrdinaryPlay.ps1`; not retained merely because a candidate once existed |
| Performance captures | `C:\perf\...`, `build/performance` | evidence: not managed by this tool |
| `build/dev-runtimes/zh` (Win32 stage runtimes), `userdata-backups` | | outside the tool's scope; not automatically deletable |
| Steam installation | | never touched; roots that look like Steam are refused |

## 2. Tool
`scripts/native-runtime/Cleanup-NativeCandidates.ps1`

```
# 1. dry run (default): prints KEEP / ARCHIVE / REMOVE / AMBIGUOUS per candidate, estimated reclaim and PLAN_HASH
pwsh scripts\native-runtime\Cleanup-NativeCandidates.ps1 -Active <current-candidate> -LatestAccepted <accepted-candidate>

# 2. after a human reviewed the plan, execute exactly that plan
pwsh scripts\native-runtime\Cleanup-NativeCandidates.ps1 -Active ... -LatestAccepted ... -Execute -ApprovedPlan <PLAN_HASH>
```
Parameters: `-Protect <names>` (name or `Title\name`), `-KeepNewest <n>`, `-PolicyFile`, `-AllowedEntry <names>` (extra
known top-level entries), `-NativeRoot`, `-ArchiveRoot`, `-WhatIf` (supported on top of the dry-run default).

Safety properties (all covered by `Test-NativeCleanupWorkflow.ps1`):
* Dry run is the default; nothing is copied or deleted without `-Execute`.
* `-Execute` also requires `-ApprovedPlan` equal to the hash of the plan being executed, so only a reviewed plan runs;
  any change to the tree after the dry run changes the hash and is refused.
* Fails closed: a candidate with an unexpected top-level entry, a reparse point (symlink/junction) anywhere inside, an
  unexpected name, or a Steam-looking path is classified AMBIGUOUS and kept.
* The native root must be `...\dev-runtimes\native`; Steam-looking roots and an archive root inside the native root
  are refused. Only `<NativeRoot>\<Title>\<candidate>` directories can be removed; the removal path is re-checked
  against the root.
* Evidence is copied and verified by file count and total bytes before the candidate is removed; a verification
  failure keeps the candidate and exits with code 2.
* No `git clean`, no recursion outside candidate directories, no build or source tree is ever a target.

## 3. Workflow after each native review
1. Accept/reject the candidate; update `-LatestAccepted` and, if needed, `native-retention.json`.
2. Run the dry run and read the plan. A pre-approved scoped rule may cover routine cases; anything unusual
   (AMBIGUOUS, a large reclaim, unknown entries) goes to the human first.
3. Execute with the plan hash. Record the reclaimed size in the milestone document.
4. Periodically review `build/dev-runtimes/zh`, `userdata-backups` and old crash dumps in
   `build/evidence-archive/` by hand; they are intentionally outside the automatic policy.

## 4. Future (not implemented)
Staging could share one read-only asset directory (hard links or a junction to an immutable copy) so a candidate costs
~50 MB instead of 1.5 GB. That must keep candidate isolation and must never point into or write to the Steam folder; it
is a staging redesign, deliberately not part of this change.

## 5. Current state (2026-10-16)
Dry run against the real tree with `-Active x64-35-attribution -LatestAccepted x64-34-smooth-presentation`: 5 candidates,
all KEEP, nothing removable, 0.00 GB reclaim (the previous cleanup already removed the superseded set).
