# Agent handoff

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
