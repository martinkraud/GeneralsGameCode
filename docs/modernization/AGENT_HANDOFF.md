# Agent handoff

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

## Next action

Developer: follow STAGE2_TIMING.md to create/validate a fresh `stage2-timing`
runtime and manually compare 30 versus 60 with fresh backup protection. Do not
use Ctrl+Shift+numpad to repair speed during acceptance. Retain baseline unchanged.
Record actual rendered FPS and completed logic-frame change per elapsed second
where instrumentation permits; distinguish target values from measurements.
Only after 60 passes, characterize 120/144/240, pause/loading/speed transitions,
visual stepping, replay and multiplayer. Complete remaining Stage 1 coverage
and profiling separately. No automatic game launch, commit or push.
