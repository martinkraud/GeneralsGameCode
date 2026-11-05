# Disk retention policy and the 2026-11-05 cleanup (X64.dh)

The development machine fell to 21.8 GB free. The source tree is ~0.2 GB; **everything else is generated under `build/`**. This document records where the space was, what was done, and the rules that keep it bounded.
(Earlier policy and the 2026-10-17 cleanup: [NATIVE_CANDIDATE_RETENTION.md](NATIVE_CANDIDATE_RETENTION.md).)

## 1. Where the space was (before)

Free disk 21.8 GB; `GeneralsGameCode` 75.1 GB of which `build/` 74.8 GB (apparent size).

| Directory | GB | What it is |
|---|---:|---|
| `build/dev-runtimes/native` | 31.0 | 15 staged native candidates, 1.5-3.3 GB each (a private copy of the retail `.big` / `.bik` assets per stage) |
| `build/dev-runtimes/zh` | 14.3 | five protected Win32 Zero Hour stages, 2.9 GB each |
| `build/dev-runtimes/userdata-backups` | 5.8 | snapshots of the owner's own game data |
| `build/win64`, `build/win32` | 5.0, 4.4 | CMake / Ninja build trees (regenerable) |
| `build/diag` | 3.9 | diagnostic output: 2.3 GB of raw screenshots (22 `autocapture-*` runs, `zh-autoshot`) |
| `build/evidence-archive` | 3.2 | canonical evidence incl. 7 crash dumps (2.9 GB) |
| `build/performance` | 2.5 | one 2.4 GB ETL capture (Stage 4A.1) |
| `build/x64-52`, `build/x64-51` | 2.5, 1.1 | AI-gate scratch outputs and the replay templates |
| `build/dist` | 0.15 | three distributions' ZIPs and symbol ZIPs |
| other (`audit-tmp`, stage evidence, baselines) | < 0.5 | |
| outside the repo: `GeneralsGameCode-evidence-backup` | 7.0 | the 2026-10-17 verified evidence backup (same disk) |

Root causes: (1) every staged runtime duplicated ~32 GB of identical immutable assets (951 `.big` / `.bik` files, 982 duplicate copies); (2) the 2026-10-17 backup duplicated the evidence that stayed in `build/`;
(3) stages created by newer workflows (`dist-<hash>-gen|zh`, overlay-test, audit stages) were outside the name pattern the older retention tools manage; (4) raw screenshots / captures had no retention.

## 2. What was done and why it was safe

| Step | Tool | Result | Why safe |
|---|---|---:|---|
| Superseded distribution `4f5e092c1` (ZIPs, symbols, records, its two stages) and the two disposable `x64-48b-overlay-test` stages | `Remove-ObsoleteDistributions.ps1 -CurrentVersion 2071ad765 -RetainVersion a885d798f` (new `-RetainVersion`), dry run first | **8.5 GB** | exact generated names only; current (`2071ad765`) and previous validated (`a885d798f`) kept; regenerable from Git + installation |
| Identical `.big` / `.bik` assets shared across all stages by hard links to one content-addressed store | `Optimize-StageStorage.ps1` (new) | **31.65 GB**, 878 files linked, 0 failed | no content deleted; SHA-256 verified before every replacement; afterwards `Test-StageAssetIntegrity.ps1`: 11 stages, **2 022 assets, 0 mismatches** |
| In-repo evidence that is byte-identical to the verified backup replaced by hard links to the backup files | `Link-EvidenceBackup.ps1` (new) | **6.38 GB**, 2 807 files linked | SHA-256 equal; every documented path still resolves; both copies still protect against a mistaken deletion; 10 changed files and 24 over-long symbol-cache paths stay separate copies |
| Raw screenshots of finished UI experiments (22 `autocapture-*`, `zh-autoshot`, `desktop.png`) | `Clean-DiagnosticOutput.ps1` (new) | **2.30 GB** | not referenced by any document or script; summaries live in `docs/`; no crash dumps; age rule |

**Free disk: 21.8 GB -> 70.3 GB (+48.5 GB).** Apparent size of the project 75.1 GB -> ~64 GB; physical size about 27 GB (hard-linked files are counted once by the file system, not by directory listings).

## 3. Retained on purpose

* Distributions: `2071ad765` (current; two-PC validation pending) and `a885d798f` (previous), both titles: ZIP, symbols ZIP, manifests, stage-parity and overlay-test records, SHA-256 files, and their stages `dist-2071ad765-*`, `dist-a885d798f-*`.
* Measurement stages `mp-audit-zh` / `mp-audit-gen` (needed for the next benchmarks; each costs ~0.1 GB now) and the policy-protected stages (`native-retention.json`, `zh-retention.json`): they cost only their executables and small files after the sharing.
* `build/baseline-x64-50`, `build/baseline-x64-51` (golden gate executables), `build/x64-51/templates` (replay templates), guard digests in `docs/`.
* Crash dumps in `build/evidence-archive` (7 dumps, 2.9 GB): **never removed automatically**; they are the oldest unresolved evidence. They are now also deduplicated against the backup; a human may delete them once the investigation is closed.
* `userdata-backups` (5.8 GB): the owner's data snapshots; removal stays a by-hand decision after a per-file hash comparison with the live data (potential 3-4 GB: several snapshots share identical files).
* `build/win64`, `build/win32` build trees (regenerable, kept for the next build), `evidence-archive`, `performance` (canonical evidence; the 2.4 GB ETL now costs no extra space).
* `GeneralsGameCode-evidence-backup` outside the repo: kept (a safety copy by decision); with the hard links it costs no extra space for the files that are identical to the repo. Copy it to external storage if disk-failure protection is wanted.

## 4. Rules that keep it bounded

1. **Stages share assets.** `Stage-NativeInitOnly.ps1` (used by every `Stage-NativeOrdinary*.ps1` and the distribution pipeline) hard-links `.big` / `.bik` files >= 1 MB from `build/dev-runtimes/asset-store/<SHA-256>.asset`
   (created from the verified source on first use). A new full Zero Hour stage measured **0.09 GB instead of ~2.7 GB**. Executables / PDBs are never linked (they are rewritten in place by `-Refresh`).
   If linking is impossible the assets are copied as before.
2. **Free-space guard.** `Assert-FreeDiskSpace` (`AssetStore.ps1`) runs first in staging (size of what is not yet in the store + 2 GB), distribution building (6 GB), frame-delivery runs, the AI gate, the path-stress runner and the scaling ladder (2 GB each): they fail early with the cleanup hint.
3. **Removing stages returns the space.** `Remove-ObsoleteDistributions.ps1` (run by the distribution pipeline) and `Clean-DevelopmentArtifacts.ps1 -Execute` finish with `Optimize-StageStorage.ps1 -PruneStore`, which removes store entries no stage links to.
4. **Distribution retention** stays "current + previous validated"; `-RetainVersion` names a predecessor that has no record. Older ZIPs, symbol ZIPs and stages are removed by exact generated names.
5. **Diagnostic output is temporary.** `build/diag` and `build/audit-tmp` entries older than 7 days (raw captures: `autocapture-*`, `zh-autoshot*`, `*.bmp`, `*.png`) or 30 days (everything else) are removed by `Clean-DiagnosticOutput.ps1 -Execute`
   (dry run by default; `scripts/diag-retention.json`; a `KEEP` file or a protected name exempts; directories with crash dumps are never removed). Put conclusions in `docs/` (tables, digests, summaries) first.
6. **Never automatic:** crash dumps, `userdata-backups`, evidence, Steam, source, Git.
7. After a milestone: `pwsh scripts\Clean-DevelopmentArtifacts.ps1` (dry run), `pwsh scripts\Clean-DiagnosticOutput.ps1`, `pwsh scripts\native-runtime\Optimize-StageStorage.ps1`, then execute what was reviewed.

## 5. Safety properties and tests

All new tools are dry-run by default, refuse non-`dev-runtimes` / non-`build` roots, drive roots and Steam-looking paths, skip reparse points, verify hashes before replacing, use a temporary name plus move so an interrupted run loses nothing, and never recurse outside their allow-listed roots.
Tests (temporary trees only): `scripts/native-runtime/test_asset_store.py` (7), `scripts/native-runtime/test_diag_cleanup.py` (3), the existing `test_distribution_cleanup.py`, `Test-NativeCleanupWorkflow.ps1`, `Test-CleanDevelopmentArtifacts.ps1`.
`Test-StageAssetIntegrity.ps1` re-hashes every asset of every stage against its manifest.
