# Incremental modernization roadmap

Stage 4A.2 final selection (2026-10-08): direct analysis of all reports in
`build/performance/stage4a2-B-final-2` resolves the first Stage 4B target as
Internal goal-directed line/movement checking via `examineNeighboringCells`,
the integer `iterateCellsAlongLine` and `examineCellsCallback`. In all 31 complete
Internal searches over 40 ms, combined checking exceeds insertion; sampled
shares are 51.19% Line self, 15.56% Neighbor self and 33.25% insertion.
Capacity ended the 43.9108-second capture with 15 dropped records; the entire
incomplete final frame is excluded. Closest retries favor insertion, a later
candidate. See [STAGE4A2_PATH_PHASE_SAMPLING.md](STAGE4A2_PATH_PHASE_SAMPLING.md)
for individual evidence, observer sensitivity, invariants and acceptance plan.
No additional manual five-AI selection capture or WPR is needed. Recommend the
reproducible guarded developer scenario/benchmark harness as the next
infrastructure task before optimization iterations, with an unchanged reference
and deterministic correctness comparisons. No harness or optimization is
implemented; stop for developer review. The earlier selection requests below
are historical and superseded by this result.

Stage 4A.2 (2026-10-08): the accepted/pushed Stage 4A/4A.1 baseline is 854b8294d.
[STAGE4A2_PATH_PHASE_SAMPLING.md](STAGE4A2_PATH_PHASE_SAMPLING.md) records bounded
every-64th per-search line/neighbor/insertion observations to resolve the remaining
Stage 4B gate. No pathfinding optimization or automated scenario is implemented.
Request one delayed-start, approximately 45-second already-busy ground-heavy
developer + 5 AI capture; no new ETL. Select exactly one first Stage 4B target from
repeated individual-search phase evidence, preserving authoritative behavior.
After target selection, prioritize a reproducible developer performance scenario
before a long optimization series: build -> isolated stage -> deterministic launch
-> warm-up -> representative heavy pathfinding/object/AI workload -> bounded
capture -> exit -> reference comparison. Automated runs should be the frequent
development benchmark; real user+AI matches remain milestone validation.
This future harness direction is documentation only. Older entries are historical.

Accepted Stage 4A/4A.1 tooling baseline (2026-10-08): developer authorized one
tooling commit. The synchronized WPR analysis and selection limits are recorded in
[STAGE4A1_PATHFINDING_DEEP_PROFILE.md](STAGE4A1_PATHFINDING_DEEP_PROFILE.md).
Real CPU pathfinding stalls repeat, but insertion versus line/movement checking
is not resolved by the truncated caller stacks. Stage 4B remains unselected;
the bounded sampled phase accumulator remains a review proposal, not implemented.
Pre-commit Release build, CTest 2/2, 184 direct tests, profiler benchmark smoke
checks and pathfinder token audit pass; EXE/PDB match the captured candidate.
No gameplay changes, game launch or push. Older entries are historical.

Stage 4A.1 update (2026-10-08):
[STAGE4A1_PATHFINDING_DEEP_PROFILE.md](STAGE4A1_PATHFINDING_DEEP_PROFILE.md) analyzes
all original A/B capture formats directly. B's worst completed logic frame is
172.436 ms, with 163.287 ms aggregated across five path_search calls; this does
not establish a single 163 ms search. Queue work/cells scale sharply, and the
5,000-cell queue budget is checked between requests. Pathfinding is a measured
lead, but the exact Stage 4B optimization remains undecided.
Default-OFF -pathProfile, alongside -performanceProfile <directory>, adds bounded
individual linked search/phase records, workload counters and paths.csv. No path,
AI, queue, sort, gameplay or threading optimization. Current candidate is
stage4a1-pathfinding-deep-final. Final Release build, CTest 2/2, 184 direct tests
and both aggregate/deep benchmarks per title pass. Source/new/previous Stage 4A
inventories pass (369 files each); launch validation passes without a launch.
Use the report's delayed-start capture procedure
to avoid deliberately recording focus transitions. Preserve original A/B files.
Stage 4A entries below are historical; real developer captures now exist.

Stage 4A update (2026-10-08):
[STAGE4A_PERFORMANCE_BASELINE.md](STAGE4A_PERFORMANCE_BASELINE.md) records the CPU instrumentation and exact capture procedures.
Started clean on dev/modern-engine at 1de7e65d173104ad2f288ac0a379e102a5057ebc.
Developer accepted/committed/pushed Stage 3C.4 after a complete 1v1; same-binary
OFF immediately restored stepped turning. Main-PC high-refresh/ultrawide remains pending.
Default-OFF -performanceProfile <absolute-directory> adds bounded QPC observations,
inclusive/exclusive category totals, frame/tick counts, tails and top-N breakdowns.
Release build and CTest 2/2 pass; 168 direct Google tests and both profiler benchmarks
pass. Fresh candidate stage4a-performance-baseline is for developer captures only;
full source/new/six earlier acceptance inventories pass (369 files each), and
launch validation passes without launching or backing up user data.
No game launch, optimization, scheduling/AI/path decision changes, Install, Steam or
user-data writes, commit or push. Large-map + 5 AI spikes predate this stage and
occurred ON/OFF. Collect the documented light/stress windows with accepted
interpolation ON and a stable finite cap; use evidence to choose Stage 4B later.
Older entries retain their historical acceptance states.

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

Stage 3C.1 update (2026-10-08): the developer reviewed/committed/pushed Stage 3A/3B.
The separate scheduler-observing presentation timing API is implemented; legacy
phase, tick policy, network readiness and formats are unchanged. No world or
controls implementation. Release build and 56 Google tests pass; all six caps
and non-integer/jitter/recovery behavior have deterministic synthetic coverage.
[STAGE3C1_PRESENTATION_CLOCK.md](STAGE3C1_PRESENTATION_CLOCK.md) documents explicit
invalid/snap timing for unsupported modes and the one-tick latency tradeoff.
Stage 3C.2 requires review before a narrow, gated ground-unit translation cache:
match completed generations/epochs/object lifetimes, handle discontinuities and
audit picking/bone/attachment queries. Network smoothing, generalized rotation,
aircraft/projectiles and WASD/rebinding remain separate. Runtime visual/CRC/LAN
acceptance is still open; synthetic cadence tests do not establish compatibility.

Stage 3B update (2026-10-08): Stage 3A is now developer manually accepted,
including default 60, normal gameplay, immediate 120-FPS Accept and persistence
through relaunch; TPS is still inferred. The analysis-only
[STAGE3B_INTERPOLATION.md](STAGE3B_INTERPOLATION.md) identifies missing generic
world-root interpolation alongside existing animation/decorative/particle
mechanisms, and a visual-phase/scheduler-remainder mismatch at non-integer ratios.
Proposed Stage 3C: establish and test a scheduler-aligned presentation phase
contract, then gate a client-only translation prototype for a narrow ground-unit
subset with lifecycle snaps and gameplay-query isolation. Preserve tick policy,
ordering, network pacing and serialized formats. No Stage 3C implementation is
authorized by this investigation; review the plan and runtime acceptance matrix.
Separately, plan opt-in user-rebindable camera actions retaining arrows and mouse
controls. All WASD letters have gameplay/contextual conflicts in the copied
English assets; resolving press/release ownership and focus gates precedes any
controls implementation. This is not bundled into the interpolation prototype.
Earlier updates below describe their historical acceptance state.

Stage 3A update (2026-10-08): Stage 2's 60-FPS offline smoke test is developer
verified: raising render FPS no longer accelerates gameplay; the camera is
smoother. TPS remains inferred rather than instrumented. The focused Stage 3A
candidate provides default 60, finite persistent Options render caps and retires
the audited conflicting Skirmish slider; 30-TPS scaling remains intact.
[STAGE3A_FPS_OPTIONS.md](STAGE3A_FPS_OPTIONS.md) records build/tests, guarded staging
and the required manual UI/persistence checks. Stage 3A gameplay acceptance remains
UNVERIFIED; Stage 3B interpolation, network/replay validation and broader
performance/ultrawide/renderer work remain separate. Earlier updates are historical.

Stage 2 update (2026-10-08): the developer's guarded baseline passes startup,
menus, ordinary skirmish and clean exit, with 369-file source/runtime hash
checks before/after. Raising the render cap manually accelerates offline play.
The first narrow correction enables the existing 30-TPS logic scale by default;
no scheduler rewrite is involved. [STAGE2_TIMING.md](STAGE2_TIMING.md) tracks the
candidate and required manual 30-versus-60 acceptance. Higher caps, replay/LAN,
full Stage 1 scenario coverage and performance characterization remain open.

Stage 1 update, 2026-10-08: guarded staging/launch/backup/integrity helpers and
synthetic infrastructure checks are implemented under `scripts/zh-runtime`.
[STAGE1_WORKFLOW.md](STAGE1_WORKFLOW.md) documents exact manual launch, baseline,
profiling and Stage 2 preparation. This historical update preceded the developer's
manual runtime session and Stage 2 candidate above. Stage 1 measurement work is
not complete; the broader scenario/profile/compatibility tests remain open.

Revision `adac468d7`, 2026-10-07. Research order follows current source evidence:
existing render/logic separation must be evaluated before a new decoupling
prototype. Stage descriptions below are the plan; current status is recorded above.

## Stage 0 — Freeze the evidence baseline

- **Objective:** preserve source revision, Git safety, compiler/cache settings and
  known successful x86 build; distinguish developer reports from runtime tests.
- **Benefit:** future results can be reproduced and correctly attributed.
- **Risk:** mistaking a build for retail compatibility or a badge for x64 support.
- **Compatibility:** no behavior change; preserve retail guards.
- **Tests:** verify branch/remotes, clean initial tree, binaries/compiler metadata;
  establish optimized VC6 reference separately when retail checks are needed.
- **Rollback:** retain this revision and original artifacts; no source changes yet.

## Stage 1 — Disposable runtime and reproducible profiling baseline

- **Objective:** implement an opt-in staging/launch workflow using a full asset
  copy outside Steam, explicit CWD and a manifest. Isolate user data with a test
  account or first design a reversible path override in a separate change.
- **Benefit:** safe debugging and meaningful measurements without modifying Steam.
- **Risk:** archive/DLL provenance, legacy registry key, shared user data and
  existing app-local D3D wrapper can contaminate results.
- **Compatibility:** no engine/data-format change for staging; keep baseline
  assets/settings and explicitly record registry dependencies.
- **Tests:** destination guard/no links to Steam; file hashes; DLL import inventory;
  launch, skirmish/campaign, audio/video, save/load, mod load; modern same-build
  replay checks and separate retail VC6 replay checks. Capture Tracy Release
  profile plus uninstrumented reference on identical scenarios, including frame
  distributions and ticks/second. Include user-data backup/restore verification.
- **Rollback:** discard only the verified disposable runtime; restore backed-up
  test user data. Keep Steam and existing build cache untouched.

**Implemented infrastructure:** a small PowerShell staging/launch
helper with a destination guard, file manifest and explicit CWD, plus a repeatable
scenario checklist. It must not invoke the cached install target, edit registry,
copy over Steam or change timing. Decide user-data isolation before launching.

## Stage 2 — Characterize existing FPS and display behavior

- **Objective:** correct the verified offline speed defect by enabling the
  existing 30-TPS scale by default, then exercise FramePacer, network
  pacing, interpolation, presentation interval, resolutions and window modes.
- **Benefit:** identifies actual gaps and avoids duplicating upstream work.
- **Risk:** raising FPS alone speeds offline simulation; profiler/wrapper effects
  and Present waits can obscure findings.
- **Compatibility:** no tick-unit/order/network-path changes; offline wall-clock
  pacing changes above 30. Same-build replay/live input need separate validation.
- **Tests:** 30/60/120/144/240 rendering with explicit 30-tick policy; non-integer
  ratios; pause, load, fast-forward, scripted time freeze, minimized/device-loss
  cases; fixed camera/replays; CRCs and tick counts; Alt-Tab and DPI tests.
- **Rollback:** return test settings/runtime to baseline; disable diagnostics.

## Stage 3 — Close measured scheduling/interpolation gaps

- **Objective:** define explicit normal-speed policy and fix one demonstrated
  visual/timing gap per change using existing FramePacer/phase infrastructure.
- **Benefit:** smoother high-refresh presentation with stable gameplay speed.
- **Risk:** command-to-frame assignment, update ordering, logic callbacks,
  spawn/teleport transitions and catch-up policy can affect simulation.
- **Compatibility:** preserve fixed logic units and default/legacy behavior;
  any policy change requires documented replay/multiplayer analysis first.
- **Tests:** frame-indexed CRC equivalence across caps; live input tests;
  transforms/animation/particles/attachments/camera; speed/pause/load/stall cases;
  long LAN sessions with mixed caps and existing replay corpus.
- **Rollback:** opt-in feature flag and focused revert; retain legacy scheduler
  and unmodified serialized state. No render threading in the first prototype.

## Stage 4 — High-refresh presentation and frame pacing

- **Objective:** tune only demonstrated limiter/Present issues and complete
  render-rate-independent visual updates for 120/144/165/240 Hz scenarios.
- **Benefit:** lower jitter/latency and consistent animation/input feel.
- **Risk:** sleep/spin tradeoffs, GPU/CPU saturation, clock handling and wrapper
  behavior; average FPS can hide poor tail latency.
- **Compatibility:** presentation-only changes must not change logic state/rate.
- **Tests:** p95/p99 pacing, CPU power/utilization, input latency proxy, VSync modes,
  low-FPS fallback, different refresh displays, CRC tests from Stage 3.
- **Rollback:** independent cap/pacing options with known baseline defaults.

## Stage 5 — Ultrawide, borderless and UI scaling

- **Objective:** extend current arbitrary-aspect support with tested projection,
  layout, scaling and desktop-window behavior; preserve mod UI compatibility.
- **Benefit:** usable 21:9/32:9/5120x1440 and high-DPI displays.
- **Risk:** picking/cursor/clipping, FOV/map bounds, stretched assets, device resets.
- **Compatibility:** no asset-format/INI semantic changes; optional layout policy.
- **Tests:** 4:3/16:9/21:9/32:9, multiple DPI scales, menu/HUD/tooltips/text,
  mod WND layouts, selection/placement, Alt-Tab/multi-monitor/minimize/recovery.
- **Rollback:** switch to legacy window/layout/FOV settings; separate commits.

## Stage 6 — Measured CPU improvements

- **Objective:** optimize only top measured costs, one subsystem at a time;
  consider safe client-side concurrency only after ownership analysis.
- **Benefit:** better CPU frame time and practical scene/unit capacity.
- **Risk:** ordering/precision changes, synchronization and memory ownership.
- **Compatibility:** deterministic output/order preserved; simulation threading
  requires a separate design and stronger tests.
- **Tests:** repeated identical traces/microbenchmarks, full replay CRCs, long
  matches, allocation/lifetime checks and non-profiled throughput confirmation.
- **Rollback:** focused revert or feature switch with reference implementation.

## Stage 7 — Memory and asset limits; separate x64 feasibility

- **Objective:** measure address-space/allocator/asset constraints before raising
  limits; separately scope native x64 graphics/audio/video and ABI work.
- **Benefit:** stability and larger practical content where evidence supports it.
- **Risk:** overflow, file/wire widths, CRC ordering, platform dependencies.
- **Compatibility:** changing architecture does not grant format compatibility;
  preserve field sizes or explicitly version changes before implementation.
- **Tests:** representative large mods/maps, memory pressure/leaks, save/load,
  old assets, x86 reference comparisons; x64 CI/runtime only if a viable design
  is implemented later.
- **Rollback:** preserve x86 runtime and original limits; independent branch and
  versioned feature choices. Do not start an x64 port to explain the badge.

## Stage 8 — AI/pathfinding scalability if profiling justifies it

- **Objective:** reduce demonstrated AI/path/spatial cost while preserving
  decisions, ordering, budgets and movement behavior.
- **Benefit:** larger practical battles/maps without simulation slowdown.
- **Risk:** very high determinism/gameplay sensitivity; even allocation-order
  changes can matter (existing retail guards acknowledge this).
- **Compatibility:** prioritize equivalent algorithms; document any deliberate
  behavioral change as a separate compatibility mode.
- **Tests:** adversarial terrain/crowds, path outcomes and frame-indexed CRCs,
  seeds/mods, long multiplayer, failure cases and benchmark confidence intervals.
- **Rollback:** retain baseline algorithm behind independent selection or revert.

## Stage 9 — Renderer modernization feasibility

- **Objective:** inventory IRenderBackend coverage and remaining DX8/D3DX types,
  fixed-function assumptions, effects/assets and tools before selecting a path.
- **Benefit:** informed options for graphics/API portability and future features.
- **Risk:** enormous visual/tool/content surface and performance regressions.
- **Compatibility:** no replacement until asset/mod fidelity and fallbacks have
  explicit acceptance criteria.
- **Tests:** image comparisons, shaders/materials/water/shadows/particles,
  mods/tools, device recovery and performance on representative hardware.
- **Rollback:** retain DX8 backend and old runtime; isolate experimental backend.

## Stage 10 — Multiplayer infrastructure research

- **Objective:** map lobby/services/transport and their current availability,
  then isolate service work from deterministic command protocol changes.
- **Benefit:** maintainable connectivity and modern service operations.
- **Risk:** security, latency, peer compatibility, replay semantics and external
  service dependencies; present service availability remains UNKNOWN.
- **Compatibility:** keep existing simulation/protocol where feasible; version
  any new peer protocol and explicitly separate incompatible sessions.
- **Tests:** interoperability matrix, packet loss/latency/reordering, reconnect,
  command/seed/order validation, long matches and replay reproducibility.
- **Rollback:** original transport/protocol/service configuration retained.

Tools may use other languages later, but the engine remains C++. No C# conversion,
speculative optimization or broad rewrite is part of this roadmap's first work.
