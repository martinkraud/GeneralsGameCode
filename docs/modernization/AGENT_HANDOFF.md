# Agent handoff

## X64.1 build foundation (2026-10-09)

X64.1 audit and parallel experimental Win64 presets are complete. Win32 Release
remains validated; Win64 configures and compiles independent native libraries,
but full engine compilation is blocked by DX8 dependencies and x86 diagnostics.
No native game linked or launched; x64 is not complete. Renderer boundary/native
backend dependency work must precede first x64 gameplay with this dependency set.

**Next proposed task: X64.2 - Native Windows diagnostics and stack-unwind foundation.**
Keep that scope bounded to contexts/unwinding/DbgHelp/dialog ABI and Win32
regression proof. Renderer dependency, GUI pointers, allocator/layout and
format/FP lockstep validation remain separate gates. No D3D11 or X64.2 work begun.
Pathfinding 2.0 remains backlog; Stage4A.3 remains PAUSED.

See [X64_1_BUILD_FOUNDATION.md](X64_1_BUILD_FOUNDATION.md) for actual compiler
counts, dependency matrix, renderer decision, validation and exact next scope.
The older X64.1 next-step notes below describe the prior closeout state.

## Performance cycle closeout (2026-10-09)

Stages 1-3 high-FPS timing/interpolation foundation and Stage 4A/A.1/A.2
profiling foundation are complete. Developer Mode / Quick Game / battle preset
are the accepted practical interactive development and stress workflow. Stage4A.3
automation remains **PAUSED**, with neutral-owner selection and warmup goal
relocation defects unresolved; it is not accepted representative automation.

Stage4B.1 surface-mask hoist is retained; no measurable speedup is claimed.
Stage4B.2 investigation is complete with no production movement cache. Stage4B.3
oracle/equivalence/microbenchmark work is complete with no selected production
insertion optimization; exact retail insertion behavior remains unchanged.

**Immediate next major phase: X64.1 - Architecture/dependency audit and Win64 build foundation.**
No x64 implementation is part of this closeout. Prefix/rank-hint feasibility and
other pathfinding work belong to the future **Pathfinding 2.0 backlog**, subject
to the existing equivalence and meaningful-benefit gates, not the immediate task.

See [PERFORMANCE_CYCLE_CLOSEOUT.md](PERFORMANCE_CYCLE_CLOSEOUT.md) for the reviewed
file inventory, commit grouping and final integrated validation. All earlier
next-step, uncommitted and no-push statements below describe their historical
task states and are superseded by this closeout. Preserve all local evidence.

## Stage4B.3 retail-list investigation: Outcome B (2026-10-09)

Actual production insertion oracle/microbenchmark and adversarial tests are added.
No production insertion optimization selected: cost snapshot and exact two-step
loop do not show a broad repeatable worthwhile gain. Three test-access friend
headers add no runtime state; actual insertion .cpp is unchanged. Exact identity,
links, tail,5000 cutoff, repair and noncanonical-owner behavior are covered.

Full x86 Release build PASS; CTest2/2; direct132/title; focused58/title; Python16;
PowerShell9. No stage4b3 runtime, launch, commit/push or staging. Stage4B.2 remains
rejected for implementation and Stage4A.3 remains PAUSED.

Future Pathfinding 2.0 backlog: bounded prefix/rank-hint feasibility against the
actual oracle, with complete mutation/repair validity proof and unchanged logical
hop cutoff. No production finger/index authorized; stop if broad invalidation is
needed. Larger Internal checking remains the alternative with its reference gate.

[STAGE4B3_RETAIL_OPEN_LIST_INVESTIGATION.md](STAGE4B3_RETAIL_OPEN_LIST_INVESTIGATION.md)
contains exact semantics, measured baseline/candidates, limits and reproduction.
Earlier next-step notes below are historical and superseded by this outcome.


## Stage4B.2 audit: production cache rejected pending equivalence (2026-10-09)

Stage4B.1 remains accepted for retention. Stage4B.2 added no gameplay code or
measurement observer. The audit establishes substantial successful line-query
repetition, but existing captures do not measure full query keys/bounded-cache hits,
and exact actual-helper/path reference equivalence has not been demonstrated.
Pool occupancy IDs share search storage; non-ground getCell fallback prevents an
unconditional30000-key bound. Across7 Internal>40ms, conservative successful-line
queries>=7851431 and repeats>=4701431 using15 layers; these are derived bounds,
not measured cache hit rates or saved milliseconds.

No Stage4B.2 candidate or runtime command. Next proposed optimization investigation:
actual-retail-list equivalence tests/microbenchmark for forwardInsertionSortRetailCompatible,
whose repeated cost is measured and whose production primitive can be tested without
an object/terrain world. Preserve exact ties/5000-hop/repair/order; reject noise-sized
gains. Movement caching remains promising but requires the explicit reference gate.
No automatic heap/fixed-mode/policy change. Stage4A.3 remains PAUSED.

Full dependency/pool audit, bounds, rejection reason, alternatives and validation:
[STAGE4B2_PATH_MOVEMENT_CACHE_AUDIT.md](STAGE4B2_PATH_MOVEMENT_CACHE_AUDIT.md).
The prior next-step proposals below are historical and superseded by this audit.


## Developer runtime review and latest capture — PASS (2026-10-09)

Developer confirms TEST1 quick-game/overlay/money/reveal/spawn/capture/normal exit
PASS and TEST2 battle preset PASS (96 units per side, movement/fighting, interactive,
capture and normal exit). Release instant build remains documented/deferred and is
not a blocker. Matching launch receipts for both runs show the expected candidate
EXE hash and exit0.

Latest exact battle capture: build/performance/dev-20261009T082332Z-c286325d,
stem20261009T082505Z-29724-1;60.0094s clean automatic60-second stop,30375path records,
zero drops/stack-clock/phase errors,120requested/effective cap and interpolation1.
GameLogic mean/p95/p99/max3.3180/8.2369/27.8651/165.1720ms;15logic frames>30ms,
all pathfinding-dominated.770Internal searches,64>10ms/11>30ms/7>40ms; worst156.232ms.
Severe sampled Line/Neighbor/insertion61.87/13.22/24.90%; all7checking winners.

Retain Stage4B.1; optimized route is exercised, manual gameplay is healthy, no
regression evident. NO matched speedup claim; its inline getter hoist may be too
small to distinguish from run variation. Proposed next high-value task: bounded
single-Internal-search reuse of pure checkForMovement footprint/query results in
the line callback, subject to read/write invariance proof and exact reference
comparisons. No Stage4B.2 implemented or authorized by this analysis. Sorted insertion
is a later target; rendering/present is sustained wall cost but GPU/wait cause is
unresolved. Stage4A.3 debugging stays PAUSED; do not resume Fix6 or add another
selection capture/profiler.

Full measured analysis, risks and acceptance plan:
[STAGE4B1_INTERACTIVE_CAPTURE.md](STAGE4B1_INTERACTIVE_CAPTURE.md).
All earlier source/forensics/evidence preserved; documentation-only update, no
build/game launch/commit/push. The earlier never-launched runtime-review state below
is historical and superseded by the developer's successful manual review.


## Current priority — interactive Dev Mode / Stage4B.1 (2026-10-09)

Developer review explicitly PAUSES the Stage4A.3 relocation investigation and Fix6.
Do not rerun old scenarios or stage another diagnostic Fix6. All automation, forensic
snapshots, reference1–5, previous runtimes and working -skipIntro remain preserved.
The neutral-owner0 defect and goal_relocated_after_warmup remain unresolved harness
issues; neither is corrected or used as a prerequisite for current optimization.
Earlier "Stage4B locked" / "next diagnostic trial" instructions below are historical
and superseded by this priority decision.

Part A independent interactive Quick Game and actions are implemented and validated
without game launch. Explicit slot0 human / slot1 enemy ownership rejects neutral.
Part B implements one local surface-mask hoist in the accepted Internal line callback;
source-level exact-reference/order proof passes. No runtime speedup or gameplay success
is claimed. See [DEVELOPER_MODE.md](DEVELOPER_MODE.md) and
[STAGE4B1_PATH_POLICY_HOIST.md](STAGE4B1_PATH_POLICY_HOIST.md).

Next: developer runtime review of ONE fresh isolated stage4b1-devmode-pathfinding
candidate, first Quick Game/actions, then a bounded existing-profiler interactive
battle capture. No further five-AI selection capture required. No automation/Fix6
prerequisite. No launch, commit or push by agent. Future reproducible benchmark repair
remains a separate follow-up, after developer priority review.



Latest Stage4A.3 forensic state (2026-10-09): Fix5/reference-5 failed at tick91
with goal_relocated_after_warmup, unit293/index76, (600.5,3080.5)->(610.5,3080.5).
Center checkForAdjust failed; right-neighbor first spiral cell succeeded. Exact
terrain/occupancy/reservation/connectivity branch is unrecorded. Same validation
primitive runs at preparation/order time, but world state differs and prior orders
can synchronously write reservations. No speculative endpoint/tolerance fix.
Bounded getter-only index76 cell snapshots added to source for review; no corrected
Fix6 runtime staged because root cause remains unproven. All96 units were owner0
(neutral): independent ownership defect must be resolved before benchmark acceptance,
not assumed to explain this relocation. Developer confirms real -skipIntro success.
Preserve reference1..5/runtimes; no rerun, launch, commit/push or Stage4B. Current
forensic report atop STAGE4A3_PERFORMANCE_HARNESS.md supersedes older launch procedures.



Current Stage4A.3 state (2026-10-09, supersedes historical entries below):
Fix4/reference-4 controlled failure at tick91 after96 spawns/90 warmup/76 orders;
no capture started. Exact reason workload_goal_changed_or_unreachable conflates
three checks, so no endpoint/pathfinder correction is justified yet. Fix5 adds
bounded first-check/unit/candidate diagnostics and safe opt-in -skipIntro (implicit
for scenario), preserving all readiness guards/checks/thresholds. Release/CTest,
119 direct and45 focused tests/title,11 Python and actual intro CLI tests pass.
Next: review never-launched stage4a3-pathfinding-workload-fix5-final; authorize ONE
fresh reference-5 diagnostic trial using the current Stage4A3 report procedure.
No game launched, no commit/push; all old evidence retained. Stage4B stays locked,
with accepted future target Internal goal-directed line/movement/neighbor checking.



Stage 4A.3 workload fix4 (2026-10-09): automation completion and coordinate
schema repair are accepted; v1/reference-3 remains invalid for comparison and
Closest dominated. Source/CSV/map inspection identifies pre-expansion destination
footprint rejection followed by normal Closest retry; crowded8-unit goals and
repeated redirects are the leading workload cause, with exact cell ownership
unrecorded. Prepared version2 candidate uses bounded legal/connected western
bank pairs, min40 spacing and one ordinary script move/unit at capture start.
No pathfinder or normal-game policy change. New qualification gate is structural:
96 Internal records,3 >=20k pops/500k info attempts with both checking samples,
and >=75% Internal pop share versus Closest; timing remains diagnostic only.
Accepted A.2 passes; v1 fails. Release/CTest2of2,42 focused/title and10 Python
tests pass. Next: developer review then ONE guarded trial of never-launched
stage4a3-pathfinding-workload-fix4 into fresh stage4a3-reference-4. Do not launch
fix3, start extra trials or implement Stage4B before reviewing this result.
No launch/commit/push. See current one-trial procedure atop the Stage4A3 report;
older entries below are historical. Target remains Internal goal-directed
line/movement/neighbor checking.


Stage 4A.3 fix2 real trial review (2026-10-09): reference-3/trial-1 completed
96 units/480 orders/90 warmup/300 capture ticks, finalized all reports and exited
intentionally with code0. No new crash. Post-validation found float-coordinate
parsing and lossy six-digit coordinate serialization; reporting-only repair
keeps strict fingerprints (original trial still mismatches and cannot be reused
as a validated reference). Preserve all three reference directories and fix2.
No startup/gameplay/pathfinding change. Severe workload is 93 Closest retries,
not Internal: Internal max18.6461ms, zero>40ms. Accepted Stage4B target remains
Internal line/checking; do not run two more unchanged scenario trials or treat
this capture as representative. Next: review schema repair and narrow scenario
reachability/failed-request workload investigation before a separately approved
scenario revision/capture. Full Release build, CTest2/2, focused40/title and
8 Python tests pass. No launch/commit/push. See Stage4A3 report's real-trial section;
earlier pending-runtime entries below are historical.

Stage 4A.3 fix2 (2026-10-09): fix1 crashed at tick 0, GameLogic.cpp:3757,
reading null TheGameInfo+8 in CRC generation. Matching PID3592 full/minidumps,
exact staged PDB age27 and captured Sizzle/movie/loading state prove the harness
requested Skirmish before Intro completed. Slots were ready but no scenario
map load/player construction, units, orders, warm-up or profiler recording
completed. Preserve reference-1/reference-2, both earlier runtimes and dumps.
Fix2 waits for the complete Intro lifecycle and no movie/load/clear transition
before requesting the unchanged scenario. Adds only read-only readiness
accessors, bounded scenario breadcrumbs and focused regressions. Release build,
CTest 2/2, 113 direct and 39 focused tests per title, six Python tests and static/
evidence audits pass. Next: review the updated Stage 4A.3 report and fresh guarded
`stage4a3-performance-harness-fix2`, then developer trials into fresh
`stage4a3-reference-3`. Runtime remains unproven. No game launch, Stage 4B,
commit or push. Earlier entries are historical.

Stage 4A.3 fix1 (2026-10-09): the first developer trial failed in initial Setup:
menu-owned `TheSkirmishGameInfo` was null, and the automatic harness incorrectly
required it before initialization. No scenario map request, units, orders,
warm-up or capture occurred. Preserve reference-1/trial-1 and the original
runtime. Its old marker omitted the absolute logic tick; do not invent one.
Scenario-only lazy allocation now follows normal engine ownership; bounded
failure diagnostics include state/tick/progress and specific startup reasons.
Two focused regressions were added. Final Release build, CTest 2/2, 111 direct
and 37 focused tests per title, four synthetic profiler benchmarks per title,
six Python tests and static audits pass. Fresh guarded candidate is
`stage4a3-performance-harness-fix1`, staged but not launched. Next: developer
review, then three automatic trials into fresh `stage4a3-reference-2` using the
updated report commands. Runtime success/coverage/repeatability remain unproven.
No Stage 4B, game launch, commit or push. Earlier entries are historical.

Stage 4A.3 (2026-10-09): start at
[STAGE4A3_PERFORMANCE_HARNESS.md](STAGE4A3_PERFORMANCE_HARNESS.md).
Accepted baseline `0ff7f9c9b` is synchronized/pushed; current additions are
uncommitted, unstaged infrastructure only. Default-OFF scenario automatically
prepares fixed Twilight Flame offline allied China slots, spawns 96 AI-owned
ground units, schedules ordinary move orders, warms up 90 ticks, captures 300
ticks, writes bounded metadata/CRC/RNG/path fingerprints and exits. Existing
manual profiler commands remain available. Repeated-trial guarded scripts and
strict validity/configuration/correctness comparison are in scripts/performance.
Minimal gated Ctrl+Alt+Shift+F5â€“F11 tools provide overlay, money, reveal,
predefined spawning and capture. Instant build keeps its existing compile-time
gate and is unavailable in this Release build; scenario restart is a fresh process.
Full x86 Release build, CTest 2/2, 109 direct tests per title, 35 focused tests per
title, four profiler/overhead benchmarks per title, six Python tests and token
audits pass. Source/new/previous runtime inventories pass 369 files each; wrapper
two-trial validation-only checks create no output, backups, receipts or process.
Candidate `stage4a3-performance-harness` is staged and unlaunched (PDB age 25).
Next developer action: review, then execute three automatic unchanged-reference
trials from the report. Actual map/setup, severe Internal coverage and matching
fingerprints remain unvalidated. Keep Stage 4B unimplemented until that reference
is qualified; its selected target remains Internal line/movement checking.
No new manual five-AI selection battle, game launch, commit, push or generated-file
staging. Earlier entries are historical.

Stage 4A.2 final analysis (2026-10-08): selection gate resolved from all five
reports plus command.txt in `build/performance/stage4a2-B-final-2`.
Read [STAGE4A2_PATH_PHASE_SAMPLING.md](STAGE4A2_PATH_PHASE_SAMPLING.md) first.
Valid 43.9108-second capacity stop, phases enabled at stride 64, zero phase or
stack/clock errors, interpolation ON and requested/effective cap 120. Exclude
all final-frame detail because 15 records dropped; 32,718 complete rows remain.
The 31 Internal searches over 40 ms consistently favor checking: sampled
Line/Neighbor/insertion shares 51.19/15.56/33.25%, robust to the documented
clock-overhead sensitivity. First proposed Stage 4B route is Internal
goal-directed line/movement checking through `examineNeighboringCells` ->
integer `iterateCellsAlongLine` -> `examineCellsCallback`. Preserve all checks,
cell order, FP/cost, allocations, parents/reopens, insertion ties/5000-hop rule,
retry/hierarchy/queue policy and deterministic state. Closest insertion is a
later candidate; this decision does not generalize to all Ground/Closest work.
Next recommended infrastructure task: reproducible deterministic heavy developer
scenario/benchmark harness with unchanged reference and differential correctness
checks, before long optimization iterations. No further manual five-AI selection
capture or WPR requested. Analysis changed only the three modernization documents;
existing Stage 4A.2 source/test work is preserved. No behavior/harness implementation,
game launch, staging, commit or push in this analysis. Stop for developer review.
Earlier unresolved-gate and capture instructions below are historical.

Stage 4A.2 (2026-10-08): started clean/synchronized at accepted/pushed 854b8294d.
[STAGE4A2_PATH_PHASE_SAMPLING.md](STAGE4A2_PATH_PHASE_SAMPLING.md) is the current
observer/capture handoff. Only bounded per-search every-64th phase measurement is
added: line, remaining neighbor/layer and their nested insertion; no extra rows,
sort/policy/budget/state changes or game launch. Final x86 Release build, CTest 2/2,
198 direct tests, focused tests, sequential benchmark trials and token/static audit
pass. New guarded candidate is stage4a2-path-phase-sampling. Use the report's
single delayed 45-second busy ground-heavy developer + 5 AI capture, interpolation
ON and cap 120. No further WPR is requested. Select one Stage 4B target only after
analyzing repeated expensive individual phase records. No commit/push.
After target selection, prioritize a reproducible deterministic developer workload
harness before a long optimization series (build, isolated stage, launch, warm-up,
heavy pathfinding/object/AI load, bounded capture, exit, reference comparison).
No such harness is implemented now. Real user+AI matches remain milestone checks;
automated scenarios should become frequent development benchmarks. Older entries
below retain historical states.

Accepted Stage 4A/4A.1 tooling baseline (2026-10-08): developer authorized one
tooling commit, including the synchronized WPR analysis in
[STAGE4A1_PATHFINDING_DEEP_PROFILE.md](STAGE4A1_PATHFINDING_DEEP_PROFILE.md).
Pre-commit win32 Release build, CTest 2/2, 184 direct tests, both profiler benchmark
smoke suites and the pathfinder token audit pass. Release EXE/PDB hashes still
match the captured candidate exactly. Stage 4B selection remains unresolved:
truncated ETW caller chains and comparable insertion/checking leaf costs require
review of the minimal sampled phase observer proposal. That observer is not
implemented. No gameplay change, game launch or push; stop after the tooling
commit for developer review. Entries below retain historical states.

Stage 4A.1 update (2026-10-08):
[STAGE4A1_PATHFINDING_DEEP_PROFILE.md](STAGE4A1_PATHFINDING_DEEP_PROFILE.md) is current.
HEAD remains 1de7e65d173104ad2f288ac0a379e102a5057ebc; preceding Stage 4A changes
were already dirty and are preserved. Actual A/B summaries/categories/frames/slow
files were read and cross-checked. Pathfinding dominates several B logic tails,
but 163.287 ms is an aggregate of five search calls, not one measured search.
Do not choose an exact Stage 4B algorithm change yet. New -pathProfile plus
-performanceProfile <directory> records individual linked calls/work/maintenance;
without the extra flag the same candidate retains shallow aggregate profiling.
Final Release build/CTest 2/2, 184 direct tests and all profiler benchmarks pass;
source/new/previous Stage 4A full inventories pass (369 files each), and launch
validation passes without a launch or backup. Read the report for capacities,
inclusive counters, outcomes and delayed-start
manual commands. Fresh candidate stage4a1-pathfinding-deep-final is unlaunched.
No optimization, game launch, commit or push. Preserve original capture bytes
and all previous runtime candidates. Older entries describe historical states.

Stage 4A update (2026-10-08):
[STAGE4A_PERFORMANCE_BASELINE.md](STAGE4A_PERFORMANCE_BASELINE.md) is the current handoff.
Started clean on dev/modern-engine at 1de7e65d173104ad2f288ac0a379e102a5057ebc;
Stage 3C.4 is developer accepted/committed/pushed after full 1v1 and OFF comparison.
Stage 4A adds default-OFF developer CPU profiling with an explicit existing output
directory, file start/stop controls, 60-second/16,384-frame bound, QPC scope accounting,
CSV tails/top-N/counters and synthetic tests/overhead measurements. No optimization.
Final win32 Release build, CTest 2/2, 168 direct Google tests and both profiler
benchmarks pass. Initial report-test failure from metadata edits during build is
retained/documented; a consistent rebuild passes. Candidate stage4a-performance-baseline
is unlaunched; full source/new/six previous acceptance inventories pass (369 files
each), with matching EXE/PDB identity and hashes recorded in the report.
Use exact A (1 AI) and B (5 AI) capture instructions in the report;
primary accepted interpolation ON, stable finite cap, preserve metadata/settings.
Runtime profiler control/output and in-game overhead still need manual acceptance.
No real match bottleneck measured; do not select Stage 4B yet or assume pathfinding,
interpolation or threading is the answer. No commit/push/Install/Steam/user-data writes.
Earlier entries below are historical; later main-PC high-refresh/ultrawide still pending.

Stage 3C.4 update (2026-10-08):
[STAGE3C4_GROUND_ORIENTATION.md](STAGE3C4_GROUND_ORIENTATION.md) records root-heading interpolation.
Started clean at 289be9bf9 after developer Stage 3C.3 acceptance/commit/push.
Existing clock and completed XYZ pair now share two heading samples, +8 bytes
per Drawable (history 48, Drawable 392 in x86). Local shortest-yaw basis rotation
precedes existing instance/physics/model decoration; canonical getters, aiming,
turret/barrel states and simulation remain unchanged. Separate default-false
orientation capabilities exclude articulated trucks, terrain-aligned roots and
infantry combat while retaining translation. The same process-local flag remains
OFF by default. Release build/CTest 2/2 and 148 direct Google tests pass; both
Release synthetic benchmarks pass. New candidate stage3c4-ground-orientation;
full inventories PASS for it, Steam source and all five previous acceptance
candidates (369 files each). Hashes and manual 1v1 procedure are in the report.
Stage 3C.4 runtime acceptance remains pending. Developer + 5 AI combat spikes
already occurred ON/OFF before this stage; retain for later stress profiling.
Ordinary acceptance uses developer vs 1 AI. Recommend PERFORMANCE BASELINE /
PROFILING next after 1v1 review; no speculative spike fix or deeper presentation
stage implemented. No launch/Install/user-data/Steam changes/commit/push.
Earlier entries below describe their historical acceptance states.

Stage 3C.3 update (2026-10-08):
[STAGE3C3_GROUND_HARDENING.md](STAGE3C3_GROUND_HARDENING.md) records the hardening.
Developer Stage 3C.2 same-binary laptop A/B confirmed smoother eligible movement
with ON and no obvious speed/basic-selection regression. Starting clean at
8fdc9c939, preserve the clock/history/render architecture; default-OFF capability
policy replaces stock-name gating, admits standard truck roots and ordinary
combat/guard/formation movement, and excludes special AI/script/containment states.
Local box selection follows presentation XYZ; commands/getters stay canonical.
Explicit semantic/locomotor/train displacement history resets add no simulation
mutation. Release build/CTest 2/2 and 106 Google tests pass; actual x86 history
impact remains 40 bytes/Drawable, with synthetic cost measurements in the report.
Final fresh candidate: build/dev-runtimes/zh/stage3c3-ground-hardening.
Full SHA-256 inventories pass for it, the Steam source and all four older
candidates (369 files each); EXE/PDB identities/hashes are in the report.
Stage 3C.3 runtime acceptance is pending. Recommend more ground hardening/manual
acceptance before orientation. No launch/Install/user-data/Steam changes/commit/
push; previous entries are historical.


Stage 3C.2 update (2026-10-08): read
[STAGE3C2_GROUND_INTERPOLATION.md](STAGE3C2_GROUND_INTERPOLATION.md).
Default-off Release `-groundInterpolation` prototype uses the Stage 3C.1 pair
for XYZ only on six exact stock ground templates in idle/move-to states.
Drawable-owned nonserialized history captures final completed generations before
client updates/views; local matrix replacement precedes instance/physics
composition. Explicit lifecycle/containment/load/large-move snaps; unknown short
semantic relocations and rendered picking versus canonical selection remain
gated limitations. Clock, legacy phase, simulation ordering and formats stay
unchanged. Release build and 90 Google tests pass (CTest 2/2); fresh candidate
`stage3c2-ground-interpolation` and Steam source pass 369-file SHA256 checks each.
Baseline/Stage 2/Stage 3A remain intact. Runtime acceptance is entirely pending;
no game launch/Install/commit/push. Stage 3C.3 starts with candidate review and picking/teleport/FX acceptance,
not broader controls or interpolation. Earlier entries below are historical.

**Latest, Stage 3C.1 (2026-10-08):** read
[STAGE3C1_PRESENTATION_CLOCK.md](STAGE3C1_PRESENTATION_CLOCK.md) first. Started clean
on `dev/modern-engine` at `bc87b4954`; developer had committed/pushed Stage 3A/3B.
FramePacer owns a separate PresentationClock. Both engine schedulers copy actual
post-subtraction remainder/T; after its existing wait FramePacer publishes the
completed frame pair, epoch, alpha and validity for the next client pass. Normal
offline alpha is clamp((remainder + newly measured delta)/T). Existing
getLogicFramePhase and particle/physics consumers are unchanged.

Unsupported network/immediate/fast modes, pause/freeze/halt, >=T deltas and invalid
observations snap to canonical current state; valid timing needs two fresh
consecutive completions. Both title GameLogic reset/loadPostProcess and FramePacer
reset invalidate presentation history, never the existing engine accumulator.
No source changes outside this timing boundary, no world cache/interpolation,
controls, RNG or format/pacing changes. Release configure/build passes; CTest 2/2,
28 Google tests each (56 total). Existing warnings on unchanged lines are recorded
in the stage report. Runtime FPS/TPS, visuals and compatibility remain unverified.
No new runtime candidate was needed for this unused API; existing stages are
preserved. No launch, Install, commit or push. Stop for developer review.
Final read-only baseline/Stage 2/Stage 3A integrity checks pass all 369 files each.
Earlier updates below describe their historical state and authorization.

**Latest, Stage 3B (2026-10-08):** read
[STAGE3B_INTERPOLATION.md](STAGE3B_INTERPOLATION.md) first. Investigation started
clean on `dev/modern-engine` at `967dfb739 Add persistent render FPS options`.
Stage 3A is developer manually accepted: default 60, normal gameplay, Options
dropdown, immediate Accept at 120, persistence through exit/relaunch and integrity
checks. Approximately 30 TPS remains inferred, not measured. This stage changes
documentation only: no source/build/data/runtime change, launch, commit or push.
Baseline, Stage 2 and Stage 3A copied-runtime checks each pass all 369 paths,
lengths and SHA256 hashes. No build/tests rerun for this documentation change.

Generic Object -> Drawable -> W3D root translation does not interpolate.
Existing skeletal interpolation, decorative physics and particle integration
must be retained. FramePacer visual phase is separate from the scheduler's
remainder; resolve its contract before a gated Stage 3C ground-unit translation
prototype. Keep endpoint state client-owned; never mutate canonical Drawable
transforms temporarily: logic bone/launch queries and rendered picking can cross
the boundary. Snap spawn/load/reset/teleport/containment discontinuities.
Arrow panning uses elapsed client time; follow-camera smoothing includes a fixed
per-update factor. WASD conflicts include Stop in both titles, aircraft selection
in ZH and contextual button shortcuts. Plan opt-in rebindable actions separately,
preserving arrows, focus gates and explicit key/conflict ownership. Do not
implement controls or interpolation until the developer reviews follow-up scope.
The Stage 3B cap/visual/replay/network matrix is still NOT RUN.
Earlier updates below describe their historical acceptance state.

**Latest, Stage 3A (2026-10-08):** read
[STAGE3A_FPS_OPTIONS.md](STAGE3A_FPS_OPTIONS.md) first. Starting checkpoint was clean
`b68b2e82f` on `dev/modern-engine`. The developer has verified Stage 2's 60-FPS
offline smoke test: hotkeys no longer speed up gameplay, with smoother camera
motion and normal vehicles/timing; approximately 30 TPS is inferred, not measured.
Release `-fps` is RTS_DEBUG-table guarded, so use normal Options persistence.
Stage 3A adds default 60 and Options 30/60/120/144/165/240 via FrameRateLimit in
Options.ini, preserving Stage 2 scaling and temporary developer controls. The
legacy Skirmish slider has a complete behavior/call-site audit before retirement;
restart/replay message semantics remain intact. No broad persistence refactor.
Release build and 22 Google tests pass. The separate `stage3a-fps-options` runtime
is prepared for guarded manual UI/persistence acceptance, **not automatically
launched**. Preserve baseline and Stage 2. No commit/push; changes remain unstaged.
Stage 3A runtime success is still UNVERIFIED. Earlier updates below are historical.

Read this first, then the linked documents. Investigation: 2026-10-07,
revision `adac468d732503ba50f1de21bbfc5e83982c5430`.

**Stage 2 update (2026-10-08):** read [STAGE2_TIMING.md](STAGE2_TIMING.md) next.
Developer manually verified baseline startup/menus/skirmish/exit 0, normal
default-speed gameplay, no obvious audio/render/asset failures, and full 369-file
hash checks before/after. Agent rechecks also pass. **Raising the render/frame-rate
cap using Ctrl + Numpad + caused offline gameplay/simulation to speed up.**
The candidate enables FramePacer's existing 30-tick scale by default, shared by
both titles. The scheduler, constants and network path are unchanged. Explicit
developer speed/legacy controls remain. Preserve the existing `baseline` runtime;
use a new `stage2-timing` stage. Never launch automatically. 60-FPS manual
acceptance, higher rates, interpolation, replays and multiplayer remain UNVERIFIED.

**Stage 1 update (2026-10-08):** `scripts/zh-runtime` now provides guarded
staging, backup, launch and full SHA256 integrity helpers, with non-destructive
synthetic checks. Read [STAGE1_WORKFLOW.md](STAGE1_WORKFLOW.md) for usage and the
manual scenario/profiling checklist. PowerShell 7 required. Actual Steam source,
build EXE/PDB pairing and user-data resolution passed validation only. No real
runtime copy/user-data backup, game launch, profile build or gameplay test was
performed. Do not launch the synthetic workflow-check runtimes. No engine change,
commit or push; existing uncommitted investigation documents are retained.

- **Project:** developer fork of TheSuperHackers GeneralsGameCode; preserve
  Generals/Zero Hour gameplay/content/mod/save/replay/multiplayer identity.
  Engine stays C++; prioritize Zero Hour and mirror applicable Generals changes.
- **Workspace:** `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode`.
- **Git:** `dev/modern-engine`; origin is martinkraud fork, upstream fetch is
  TheSuperHackers, upstream push DISABLED. Initial tree was clean. Only these
  eight investigation docs were created in the first session; Stage 1 adds scripts
  and workflow documentation. No commit/push/branch/config change in either session.
  Keep main for synchronization. Future focused branches only when needed.
- **Build:** x86 Visual Studio Developer PowerShell:
  `cmake --preset win32`, `cmake --build --preset win32`.
  Developer reported success 4620/4620. Observed MSVC 19.51.36260.0 / X86 /
  pointer size 4, Ninja Multi-Config, executables and PDBs. Stage 2 rebuild/test
  evidence and exact commands are in STAGE2_TIMING.md.
- **Outputs:** `build/win32/Generals/Release/generalsv.exe`;
  `build/win32/GeneralsMD/Release/generalszh.exe` and tools.
- **Protect Steam:** never modify
  `C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour`.
  Current CMake install destination points there: **do not run Install**.
  Use a full copied runtime under ignored build output. No hard links/junctions.
  `-setCwd <path>` and `-useCwd` exist; default is executable directory.
  Runtime copy does not isolate user data. Missing legacy Generals registry key
  may trigger debug assertion; do not silently edit registry.
- **x64:** badge = 3 closed milestone items, one abandoned, not a certified
  native x64 build. No supported Windows x64 Zero Hour method in this checkout;
  DX8 setup is 32-bit gated; Bink/Miles loading is 32-bit only; MinGW rejects x64.
  Community x64 Generals guide is for another fork. Do not begin a port.
- **Timing:** existing FramePacer, offline accumulator, network-ready gating and
  partial visual interpolation already exist. Offline logic scaling originally
  defaulted disabled; the Stage 2 candidate defaults it enabled at 30. Never change BaseFps or
  WWSyncPerSecond (30). At most one logic tick per render iteration currently.
- **Compatibility:** modern MSVC build is not retail CRC compatible according to
  CMake/TESTING; optimized VC6 SP6 is the retail replay reference. Preserve retail
  guards, update order, logic RNG, command frames and snapshot formats.
- **Profiling:** win32-profile enables Tracy 0.13.1 + legacy profiling; Google
  Test/Benchmark optional. Existing Tracy LogicFrame plot can measure frame-index
  change per elapsed time. No profiling measurements have been performed.
- **Safety:** no force pushes/history rewrite/shared rebases/mass formatting;
  no generated outputs committed; no invasive engine work in investigation.
  Read CONTRIBUTING.md and AI_POLICY.md before upstream contribution; human
  accountability/disclosure and submission restrictions apply.

## Documentation

[BASELINE](BASELINE.md): runtime steps, build/Git state, evidence/unknowns.
[ARCHITECTURE](ARCHITECTURE.md): source ownership, systems, profiling.
[X64](X64.md): milestone and current build evidence.
[TIMING](TIMING.md): exact scheduler/visual timing and high-FPS constraints.
[RENDERER](RENDERER.md): W3D/DX8, displays/UI and ultrawide.
[COMPATIBILITY](COMPATIBILITY.md): lockstep, replay/save/RNG risks and tests.
[ROADMAP](ROADMAP.md): staged objectives, tests, compatibility and rollback.
[STAGE3C1_PRESENTATION_CLOCK](STAGE3C1_PRESENTATION_CLOCK.md): implemented timing
contract, deterministic evidence, conservative exclusions and Stage 3C.2 gates.

## Next action

Developer: review STAGE3C1_PRESENTATION_CLOCK.md, especially alpha/sample IDs,
epoch handling, unsupported-mode policy and Stage 3C.2 prerequisites. Authorize
the narrow ground-unit translation cache separately before implementation.
Synthetic clock tests pass; the Stage 3B manual 30/60/120/144/165/240 matrix still
needs measured FPS/TPS, visual comparisons, pause/load/containment/stall cases,
same-build replay CRCs and mixed-cap LAN checks. Controls/rebinding remains
separate. Keep existing runtimes intact and protect shared user data for any
developer manual session. No automatic game launch, commit or push.
