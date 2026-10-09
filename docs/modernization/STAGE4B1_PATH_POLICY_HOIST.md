# Stage4B.1 — bounded line callback movement-policy hoist

**Closeout status (2026-10-09): accepted for retention after successful manual
Developer Mode and battle-preset review. No measured speedup is claimed.**
Original staging/uncommitted statements below are historical.

Status: initial developer runtime review PASS (2026-10-09); uncommitted.
Part A interactive Developer Mode was built and passed CTest2/2 plus47 focused tests
per title before this pathfinder change was made. Stage4A.3 automation is paused.

## Accepted target and sole optimization group

Stage4A.2 final capture:43.9108seconds before bounded capacity stop; incomplete final
frame excluded,32718 complete records,31 Internal searches>40ms. Severe sampled split:
line self51.19%, neighbor self15.56%, insertion33.25%; checking wins all31, with line
alone exceeding insertion in each. No further five-AI selection capture is required.

Route: `internalFindPath -> examineNeighboringCells -> iterateCellsAlongLine ->
examineCellsCallback`. AIPathfind.cpp now stores one `LocomotorSurfaceTypeMask` value
in the stack-local ExamineCellsStruct instead of a LocomotorSet pointer. Setup takes
`locomotorSet.getValidSurfaces()` once immediately before the existing synchronous walk.
Both callback reads use the copied mask: validMovementPosition and TCheckMovementInfo
acceptableSurfaces. This is the ONLY authoritative pathfinding change, shared by both
builds. Other callers of this line-checking branch benefit too; neighbor checks themselves,
groundCellsCallback and other callback contexts are unchanged.

## Invariance proof and limits

Both title getters are a pure return of m_validLocomotorSurfaces. That integer is set
by construction/clear, addLocomotor and snapshot load; none is called during this
synchronous walk. iterateCellsAlongLine invokes only the supplied examineCellsCallback;
it changes integer traversal coordinates and reads cells. Callback changes path cell info,
parents/cost/open/closed state and observer counters, not locomotor configuration.
validMovementPosition reads cell/obstacle/surface classification. checkForMovement reads
object IDs/interfaces/relationships, ignored obstacles, cell occupancy and crush capability;
its mutable output is its TCheckMovementInfo, not locomotor state. canCrushOrSquish is a
capability query, not damage/movement execution. Pool/list operations do not update units
or dispatch game ticks. No concurrent authoritative execution or thread was introduced.
Thus every replaced read has the same integer value as the setup snapshot.

A regression test reverses exactly this local transformation and compares the ENTIRE
pathfinder token sequence against accepted baseline0ff7f9c9bf8dfe188015af36c7db8d5388756eda.
It also requires both title LocomotorSet headers/Locomotor.cpp implementations to match
that reference and checks setup-before-walk ordering. This establishes source-level
operation equivalence conditional on the audited invariant; it is not a synthetic search
result claim. Existing in-tree tests have no initialized terrain/world full-search fixture;
no full-map path/replay/multiplayer comparison was run without launching. Runtime
compatibility remains part of developer acceptance, not claimed as executed.

All branch/callback/cell expansion order, returned/null outcomes and path results follow
from the same inputs and unchanged operations. No path costs/FP expressions, parents,
reopen policy, tie sorting,5000-hop behavior, movement/unit/obstacle checks, hierarchy/retry,
pool capacity, queue order/budget, RNG, fixed pathfinding, serialization or30TPS changed.
No persistent cache, invalidation scheme or saved field was added. Context is stack-local.

Expected mechanism is removal of repeated pointer/member reads across a line walk.
The getter is inline; the compiler may already eliminate some/all redundant reads.
There is NO measured speedup claim or guarantee this tiny change improves runtime.
Do not infer reduction from phase percentages. Profile before deciding further changes.

Rejected for this group: skip/cache occupancy or checkForMovement results (mutable path
and unit state); skip cells or alter line traversal; cache across searches/ticks; change
hierarchy/retry/Closest/pool/queue policies; heap/sorted-list replacement; enable
s_useFixedPathfinding. Each would require additional correctness proof beyond this scope.

## Acceptance

Full x86 Release both games/tools; CTest/direct and focused developer/path/profiler tests;
Python exact-reference and report-analysis tests; no-dev integration token audit; formatting
and preservation hashes; one fresh isolated EXE/PDB/inventory verified runtime. Exact
executed counts/hashes are recorded in the final review report, not implied by this checklist.

Manual Dev Mode procedures are in DEVELOPER_MODE.md. First verify stable quick-game
readiness/ownership/actions and normal exit. Then capture a bounded heavy interactive
battle with existing performance/path/phase profiling. Report GameLogic mean/p95/p99/max,
Internal durations and non-overlapping root cost, pops/info/hops/work and phase shares.
Require no observer errors; account for capacity/drops/incomplete final frame as documented.
Accepted Stage4A captures identify the target but are not matched optimized/reference
benchmarks. Before claiming improvement, compare repeated matching setups/actions/caps
on reference versus optimized binaries, verify work/outcomes/order/fingerprints where
observable, and check replay/save/offline determinism separately. Never use timing in
simulation decisions. Any changed authoritative result/work order rejects the change.

Paused Stage4A.3 relocation/neutral-owner defects are not resolved by this adapter. Their
forensic diagnostics, captures/reference1–5 and staged candidates remain preserved.

## Executed final validation / isolated candidate (2026-10-09)

- Full x86 Release default build passed for both titles and tools. Part A was built,
  CTest2/2 and47 focused tests/title passed BEFORE AIPathfind.cpp was changed.
- Final CTest2/2; direct Google121/title; focused DeveloperHarness/PerformanceProfiler/
  PathProfiler/PathPhaseProfiler47/title. Five pre-existing tests/title remain disabled.
- Python13 tests passed, including11 existing report tests and2 exact-reference/order
  tests. No runtime full-search/replay/lockstep test claimed.
- Ten actual command-line test invocations passed: both titles with each quick/dev
  order, quick alone, skipIntro alone and dev+skipIntro. Three performance scripts
  passed PowerShell parser checks; both developer helper modes passed ValidateOnly.
- Normal engine/logic/input/UI tokens reproduce baseline after removal of gated
  adapters. Preservation hashes match391 prior evidence/runtime files and accepted
  Stage4A.2 final capture. git diff --check passes; index empty.
- Candidate `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\dev-runtimes\zh\stage4b1-devmode-pathfinding\game`
  created fresh, never launched. Full runtime369-file path/size/SHA256 inventory and
  source369-file inventory both pass. No old runtime was overwritten and Steam
  source inventory is unchanged. EXE/PDB identity5706bc8d-51e9-49fc-9096-a636d0fd56be,
  age39, verified by staging.
- SHA256 EXE: `BDAA237D20078B50018C3862ED3849255A0A1B135118F52375714D186B49B5E4`
- SHA256 PDB: `B8F3D1EF6F7D657E1B38866E21800EB9F31844BCAFB5729FB9CE24857D762525`
- Worktree:20 tracked modified +14 untracked files, including preserved Stage4A.3;
  none staged. No commit/push/game launch. Captures/runtimes/logs/helpers remain ignored.

Implementation paths: Core/GameEngine/{Include/Common/DeveloperHarness.h,
Source/Common/DeveloperHarness.cpp,Source/Common/DeveloperInteractive.cpp,
Source/Common/DeveloperToolsRuntime.cpp,Source/Common/CommandLine.cpp,
Include/Common/PerformanceProfile.h,Source/Common/PerformanceProfileRuntime.cpp,
Source/GameLogic/AI/AIPathfind.cpp,CMakeLists.txt}; focused tests in
Tests/Google/Core/GameEngine/Common/DeveloperHarnessTest.cpp and
scripts/performance/test_stage4b1_reference.py; guarded manual helper
scripts/performance/Invoke-ZHDeveloperGame.ps1. Earlier uncommitted hooks/automation
remain. ROADMAP, AGENT_HANDOFF and Stage4A3 report now explicitly mark the pause.

Review limitations: runtime startup/spawning/orders still need developer verification;
partial terrain-limited spawning is possible; repeated spawns share deployment areas;
AI may issue normal orders to its own units. Standard Release instant-build/upgrades
are deferred. Overlay recorded counters can be stale. Reshroud is not a fog snapshot.
Do not treat arbitrary saved/reloaded interactive sessions as reproducible benchmarks.
No speedup measured; compiler may already remove these inline getter loads. Paused
Stage4A.3 neutral ownership and warmup relocation defects remain unresolved. All
reference evidence and bounded forensic code are retained; no Fix6 prepared.

## Subsequent developer runtime validation — retain Stage4B.1

Developer manually launched the prepared candidate: TEST1 Dev Mode and TEST2 battle
preset both PASS, with normal exits. Stage4B.1 remains source-equivalent under the
audited invariant and its line route is exercised substantially in the latest capture.
No regression is evident; runtime exact-path/replay/multiplayer equivalence is not
proved by manual success. No matched speedup is claimed, and the inline getter hoist
may be too small to distinguish from run variation. Retain it without spending more
trials isolating this tiny change.

See [STAGE4B1_INTERACTIVE_CAPTURE.md](STAGE4B1_INTERACTIVE_CAPTURE.md) for all five
reports, clean bounded60.0094s capture,15path-dominated severe logic frames, seven
severe Internal phase splits, alternative bottlenecks and the proposed substantive
next optimization. No Stage4B.2/source change made during this documentation update.
The earlier "never launched" statements record the original agent staging validation;
the developer's later successful manual runs supersede that state. Stage4A.3 stays
paused and its diagnostics/evidence are preserved.
