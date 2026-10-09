# Performance cycle closeout — 2026-10-09

Accepted starting commit: `0ff7f9c9bf8dfe188015af36c7db8d5388756eda`.
Branch: `dev/modern-engine`. Original index empty; 23 modified and 21 new files.

## Review and inventory

Every original modified/new file is classified below. A = paused Stage4A.3;
B = Developer Mode; C = Stage4B.1 optimization/proof; D = capture analysis;
E = Stage4B.2 investigation; F = Stage4B.3 oracle/benchmark; G = shared integration
and documentation. No H generated or I unexplained files occur in this Git-visible
inventory. The new closeout document itself is G.

| Original state | Class | File |
|---|---|---|
| modified | G | `Core/GameEngine/CMakeLists.txt` |
| modified | G | `Core/GameEngine/Include/Common/PerformanceProfile.h` |
| modified | F | `Core/GameEngine/Include/GameLogic/Pathfinder/PathfindCell.h` |
| modified | F | `Core/GameEngine/Include/GameLogic/Pathfinder/PathfindCellInfo.h` |
| modified | F | `Core/GameEngine/Include/GameLogic/Pathfinder/PathfindCellList.h` |
| modified | G | `Core/GameEngine/Source/Common/CommandLine.cpp` |
| modified | G | `Core/GameEngine/Source/Common/PerformanceProfile.cpp` |
| modified | G | `Core/GameEngine/Source/Common/PerformanceProfileRuntime.cpp` |
| modified | B | `Core/GameEngine/Source/GameClient/Input/Keyboard.cpp` |
| modified | C | `Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp` |
| modified | G | `Generals/Code/GameEngine/Include/GameClient/GameClient.h` |
| modified | G | `Generals/Code/GameEngine/Source/Common/GameEngine.cpp` |
| modified | G | `Generals/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp` |
| modified | B | `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp` |
| modified | G | `GeneralsMD/Code/GameEngine/Include/GameClient/GameClient.h` |
| modified | G | `GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp` |
| modified | G | `GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp` |
| modified | B | `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp` |
| modified | G | `Tests/Google/Core/CMakeLists.txt` |
| modified | G | `Tests/Google/Core/GameEngine/Common/PathProfileTest.cpp` |
| modified | G | `Tests/Google/Core/GameEngine/Common/PerformanceProfileBenchmark.cpp` |
| modified | G | `docs/modernization/AGENT_HANDOFF.md` |
| modified | G | `docs/modernization/ROADMAP.md` |
| new | G | `Core/GameEngine/Include/Common/DeveloperHarness.h` |
| new | G | `Core/GameEngine/Source/Common/DeveloperHarness.cpp` |
| new | B | `Core/GameEngine/Source/Common/DeveloperInteractive.cpp` |
| new | G | `Core/GameEngine/Source/Common/DeveloperToolsRuntime.cpp` |
| new | G | `Tests/Google/Core/GameEngine/Common/DeveloperHarnessTest.cpp` |
| new | F | `Tests/Google/Core/GameEngine/Common/RetailOpenListReference.inc` |
| new | F | `Tests/Google/Core/GameEngine/Common/RetailOpenListTest.cpp` |
| new | F | `Tests/Google/Core/GameEngine/Common/RetailOpenListUnrolledCandidate.inc` |
| new | B | `docs/modernization/DEVELOPER_MODE.md` |
| new | A | `docs/modernization/STAGE4A3_PERFORMANCE_HARNESS.md` |
| new | D | `docs/modernization/STAGE4B1_INTERACTIVE_CAPTURE.md` |
| new | C | `docs/modernization/STAGE4B1_PATH_POLICY_HOIST.md` |
| new | E | `docs/modernization/STAGE4B2_PATH_MOVEMENT_CACHE_AUDIT.md` |
| new | F | `docs/modernization/STAGE4B3_RETAIL_OPEN_LIST_INVESTIGATION.md` |
| new | B | `scripts/performance/Invoke-ZHDeveloperGame.ps1` |
| new | A | `scripts/performance/Invoke-ZHPerformanceScenario.ps1` |
| new | A | `scripts/performance/Invoke-ZHPerformanceTrial.ps1` |
| new | A | `scripts/performance/compare.py` |
| new | A | `scripts/performance/test_compare.py` |
| new | C | `scripts/performance/test_stage4b1_reference.py` |
| new | F | `scripts/performance/test_stage4b3_reference.py` |

Shared files contain opt-in startup/engine adapters, capture lifecycle APIs,
report-only coordinate precision, regression tests or current-state documentation.
Source review and exact-reference checks identify the surface-mask snapshot as
the only production pathfinding transformation. No enabled movement cache or
production insertion candidate exists. Three access-only friends add no fields.
Test-only unrolling and frozen reference remain for reproducibility of the rejected
experiment; they are not linked into gameplay. Paused bounded scenario diagnostics
are intentionally retained, only behind the explicit scenario gate.

Developer mutation controls require the offline dev context and reject network,
replay, campaign/loading and neutral ownership. No default game behavior change
is intended. Historical local paths and artifact hashes in reports document actual
runs; scripts derive workspace/output paths and use existing guarded launchers.
No credentials, executable outputs, unrelated edits or mass formatting were found.
Cached review caught one extra EOF blank line in new DeveloperInteractive.cpp;
only that whitespace was removed, followed by another integrated build/test pass.

## Dependency-safe commit grouping

1. Developer/performance workflow and paused Stage4A.3 tooling: shared lifecycle,
   startup/input/UI hooks, DeveloperHarness and independent interactive adapter,
   report precision/test changes, guarded scripts/analyzer and their documentation.
2. Stage4B.1: sole surface-mask hoist, exact inverse reference audit and runtime/
   capture reports. This depends on the developer/profiling workflow above.
3. Stage4B.2/4B.3: completed investigations, access-only friends, actual primitive
   fixture/frozen oracle/test-only candidate and audits, plus current-state closeout.
   Retail test registration follows the shared test registration from commit 1.

Generated artifacts are deliberately excluded: all build/performance captures and
ETL files, runtime copies/receipts, build/test logs, temporary audit/analysis scripts
and JSON under build. Existing evidence and accepted runtime are preserved locally.

## Final independently executed validation

Independently run on the exact integrated source before staging/commits:

- Existing Ninja Multi-Config Win32 configure (`cmake -S . -B build/win32`)
  and full default Release build under VS x86 environment: PASS, both games,
  tools and tests. No Install target; existing compiler/platform preserved.
- CTest Release: **2/2 PASS**.
- Direct Google suites: **132/132 PASS per title**, six disabled tests per title.
- Focused RetailOpenList/profiler/path-phase/DeveloperHarness: **58/58 per title**.
- Explicit actual-retail oracle/reference suite: **11/11 per title** (included
  in the focused/full suites, separately rerun too).
- Opt-in actual-retail microbenchmark: **1/1 per title**; all nine complete
  production identity orders equal the retained Stage4B.3 outputs. This is a
  repeatability check, not a new speedup claim; raw timing logs stay ignored.
- Isolated command-line test processes: **12/12 PASS** across both titles
  (dev/quick both orders, quick alone, skip, dev+skip, scenario gate). These
  execute Google tests only and do not start gameplay.
- Python/reference suite: **16/16 PASS** (report/schema plus B.1/B.3 exact audits).
- PowerShell AST: **9/9 PASS**; developer helper default and battle ValidateOnly
  both PASS with existing candidate. No output directory, backup or game launch.
- Provided preservation/token audit: PASS, **391** earlier runtime/evidence
  files plus accepted A.2 report hashes; removing gated adapters reproduces
  baseline engine/logic/input/UI tokens in order. Five latest interactive battle
  reports retain exact bytes/hashes. Every non-document file is unchanged from
  closeout task entry except removal of the extra EOF blank line noted above;
  its code tokens are identical. All generated evidence remains ignored locally.
- `git diff --check`: PASS. Index empty during preservation audit.

Logs from this independent run are ignored `build/performance-cycle-*` files.
No full-map replay/save/network equivalence or new runtime review is claimed.
Manual Developer Mode/battle acceptance belongs to the prior developer runs.

## Accepted state and next task

Stages 1-3 timing/interpolation and Stage4A/A.1/A.2 profiling are complete.
Stage4A.3 is PAUSED and not accepted representative automation. Developer Mode is
retained with successful manual quick-game and 96-vs-96 battle review. Stage4B.1
is retained without a measurable speedup claim. Stage4B.2 has no production cache.
Stage4B.3 retains oracle/benchmark/equivalence tooling only; retail insertion is
unchanged, including ties, 5000-hop cutoff, repair and ownership/tail semantics.

Prefix/rank-hint and further checking/list optimizations are future Pathfinding 2.0
backlog, requiring exact equivalence and meaningful measured benefit.

**Next: X64.1 — Architecture/dependency audit and Win64 build foundation.**
This closeout does not begin that task, launch a game, stage a new runtime, run
CMake Install or modify Steam.
