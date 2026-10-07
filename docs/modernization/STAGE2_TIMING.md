# Stage 2: normal offline speed at higher render caps

2026-10-08, Europe/Oslo. Branch `dev/modern-engine`, parent revision
`adac468d732503ba50f1de21bbfc5e83982c5430`, uncommitted candidate.

## Evidence and root cause

**Developer manually verified:** the Stage 1 `baseline/game` starts normally,
menus work, ordinary skirmish gameplay appears normal at the original cap, no
obvious rendering/audio/asset failures occur, and exit returns 0. All 369 source
and runtime file paths/lengths/SHA256s match before and after play. These checks
also pass in the agent's read-only rechecks. Preserve this baseline unchanged.

**Developer manually verified defect:** "Raising the render/frame-rate cap using
Ctrl + Numpad + caused offline gameplay/simulation to speed up."

**Source verified:**

1. `Core/GameEngine/Source/Common/FramePacer.cpp::FramePacer` initializes the
   stored logic rate to `LOGICFRAMES_PER_SECOND` (30) but originally disables it.
2. `getActualLogicTimeScaleFps` returns the uncapped sentinel (1,000,000) when
   disabled and offline. The stored 30 alone does not schedule 30 ticks.
3. Zero Hour `GameEngine.cpp::canUpdateRegularGameLogic` returns true immediately
   when logic rate >= actual render cap, including uncapped equality. Thus one
   logic update occurs per outer/render iteration, making raised caps speed play.
4. With finite logic rate below the render cap, the same function already
   accumulates elapsed wall time, clamped per iteration to one logic period,
   consumes one period at readiness, and allows intervening render-only frames.
5. `update` keeps client/render/message work before optional logic/client step;
   `execute` calls FramePacer's wait/update afterward. No ordering changes needed.
6. `BaseFps = 30` and `WWSyncPerSecond = 30` define original time units. Neither
   changes. No gameplay constants are compensated.

## Candidate implementation

`FramePacer::FramePacer` now initializes `m_enableLogicTimeScale = TRUE` instead
of FALSE. The API comment in `FramePacer.h` documents the new default. This
activates the existing scheduler and matching client/visual timing queries from
startup, including caps selected by CLI. No new scheduling algorithm, sleep,
catch-up loop, data field or serialized state is added. Shared Core applies
the default to both Zero Hour and Generals; Generals gameplay remains untested.

| Render cap | Normal offline scheduling, absent explicit speed overrides |
|---:|---|
| 30 | Existing immediate branch, one tick per iteration; unchanged scheduler path |
| 60 | Existing accumulator, nominally one tick per two render iterations |
| 120 | Nominally one tick per four render iterations |
| 144 | Alternating intervals at non-integer 4.8 render/logic ratio |
| 240+ | Same 30-TPS target, more render iterations between ticks |

These are source expectations, **not achieved-rate measurements**. This retains
the old limit of at most one tick per iteration. Below 30 achieved FPS, simulation
can slow down. Long stalls are clamped rather than caught up. At cap 30 the
existing immediate path remains tied to achieved cadence. No guarantee of exact
wall-clock 30 TPS under load/stalls is introduced.

`CommandXlat::changeMaxRenderFps` and `MetaMap::generateMetaMap` are unchanged.
Ctrl+numpad +/- still selects caps; presets from 30 are 50, 56, then 60.
Ctrl+Shift+numpad +/- still intentionally changes logic rate by 5 and can disable
scaling on reaching the cap. Such an explicit developer override can restore
render-bound simulation. It is not normal-speed acceptance; restart the process
to restore the candidate default. No automatic repair of explicit overrides is
added. Frozen/halted queries and ignore flags are unchanged. Frame-target pause
still remembers/disables scaling and restores it on resume. TiVO replay fast
mode still bypasses the scheduler. These transitions require runtime testing.

Script `SET_FPS_LIMIT` still selects the render cap: above 30 it no longer
implicitly speeds normal logic. Script fast-time/tactical multipliers use the
pre-existing enabled-scale behavior; scenarios relying on previous implicit
render-bound speed may differ. Campaign/mod cinematic timing and fast modes
must be characterized before broad compatibility claims. This narrow candidate
does not redesign game-speed semantics or interpolate missing object motion.

## Network and replay boundaries

`canUpdateGameLogic` still chooses network readiness whenever `TheNetwork` is
present. `canUpdateNetworkGameLogic`, Network frame deadlines, command frames,
RNG, deterministic update order, arithmetic, save/wire/replay formats and client
CRC boundaries are untouched. FramePacer still queries the network frame rate
before the offline enabled-state check. No multiplayer pacing change is intended;
that source analysis is not a multiplayer test.

Offline replay wall-clock pacing changes with the default. TiVO fast-forward
remains explicit. Commands still execute by recorded logic frame, but additional
client updates/live input sampling can expose render-to-logic side effects.
Compare same-build replay CRCs by logic-frame index and test live input separately.
Modern MSVC remains distinct from optimized VC6 retail compatibility. Neither
replay nor multiplayer compatibility is established by compilation/unit tests.

## Build and automated verification

Using the installed Visual Studio **x86 Developer PowerShell**:

```powershell
cmake --preset win32 -DRTS_BUILD_OPTION_TESTS=ON
cmake --build --preset win32
ctest --preset win32 --output-on-failure
```

The pinned Google Test dependency required network access for its initial fetch.
Tests are now enabled in the local ignored win32 cache; no preset changes were
needed. Full win32 Release build passes. CTest passes both z_googletest and
g_googletest: seven tests each, fourteen total, including four new FramePacer
tests per title. No compiler warning is reported for the changed source/tests;
unrelated legacy warnings remain. Logs: ignored `build/stage2-build.log`,
`build/stage2-ctest.log`, and `build/win32/Testing/Temporary/LastTest.log`.

`Tests/Google/Core/GameEngine/Common/FramePacerTest.cpp` covers:

- Finite default 30 rate and matching visual deltas at 30/60/120/144/240/480.
- Independent frozen/halted ignore flags and restoration.
- Explicit logic speed, disable-to-legacy behavior, and re-enable.
- Slow-render visual ratio clamping without extrapolation.

Tests inject deterministic elapsed time into FramePacer; they do not run a game,
sleep for frame timing, exercise full GameEngine scheduling or count actual
completed simulation ticks. Engine construction/destruction requires initialized
subsystems, so that is reserved for manual acceptance. A fresh verified real
user-data backup was made before running the existing unit suites, whose crash
handler can write there. No game process was started or user data restored.

## Guarded candidate runtime and manual acceptance

Use PowerShell 7 at the repository root. Never run CMake Install. Do not overwrite
the existing `baseline` stage. To reproduce staging on a machine where
`stage2-timing` does **not** already exist:

```powershell
$source = 'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'
./scripts/zh-runtime/Stage-ZHRuntime.ps1 -SourcePath $source -Name stage2-timing -ValidateOnly
./scripts/zh-runtime/Stage-ZHRuntime.ps1 -SourcePath $source -Name stage2-timing
```

If already staged, validate rather than overwrite; rebuilding later requires a
new stage name or the helper's non-destructive `-Refresh` sibling operation.

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage2-timing -CheckSource
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage2-timing
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage2-timing -BackupUserData -ValidateOnly
```

Candidate game path: `build/dev-runtimes/zh/stage2-timing/game`.
The manifest beside `game` records EXE/PDB identity/hashes, source/runtime
inventories, source revision/dirty worktree and build cache. No launch is automatic.

**Agent verified:** this full candidate stage is now created. Source and candidate
runtime each match all 369 inventory files; launch validation with backup planning
passes without launching or writing another backup. The original `baseline`
runtime still matches all 369 files after the build/stage operation. Build log,
staging log, private backup and runtime/manifests remain ignored by Git. No Steam
writes, registry edits, CMake Install, game launch, commit or push were performed.

**Developer manual sequence (all candidate gameplay results start NOT RUN):**

1. Record baseline/candidate executable hashes, map/faction/AI/settings,
   resolution/mode, wrapper/config and a repeatable camera position. Keep other
   settings identical and use a quiet map with enough performance headroom.
2. Manually launch the candidate at 30 with fresh user-data protection:

   ```powershell
   ./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage2-timing -BackupUserData -GameArguments @('-fps','30')
   ```

   Start the same offline skirmish. Confirm normal movement, construction,
   weapons, resources, AI and timer progression. Time one reproducible build or
   movement event over wall time. Check pause/resume and clean exit. Record
   Pass/Fail/Blocked with evidence, not just exit code.
3. Fresh process, same scenario and camera, requested 60 cap:

   ```powershell
   ./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage2-timing -BackupUserData -GameArguments @('-fps','60')
   ```

   New-game/script settings can select another cap. Confirm the active cap after
   entering gameplay; if it resets to 30, use Ctrl+numpad + to reach the displayed
   60 (30 -> 50 -> 56 -> 60). **Do not press Ctrl+Shift+numpad to repair speed.**
   Repeat the timed event. Expect comparable gameplay speed with more render
   updates and approximately 30 completed logic ticks/s. Record stepping/judder
   separately from speed; do not raise TPS to hide missing interpolation.
4. In the running 30-cap candidate, also raise the cap to 60 with the same keys
   while observing a timed event. Verify the original defect no longer occurs
   during a live transition. Pause/resume, move camera/select units, inspect
   particles/projectiles, minimize/restore, save/load with a unique development
   save, and exit cleanly. Do not overwrite personal saves.
5. Recheck candidate and Steam source with Test-ZHRuntime, and check `baseline`
   again. Runtime logs may introduce explainable drift; Steam must still match
   the staging source inventory. Store results under ignored
   `build/dev-runtimes/results/stage2-timing-<timestamp>.md`.

Only after 60 passes, characterize 120, 144 and 240 with the same procedure,
especially non-integer ratios, sustained load, input, pause/loading/script
speed/fast-forward transitions, replay and device recovery. 5120x1440 is a
separate characterization; no ultrawide UI/FOV change is included.

## Measuring actual render FPS versus logic TPS

Cap messages and logic-scale messages show **requested policy**, not measured
throughput. The existing profile build already has a GameClient frame mark and
Zero Hour GameLogic `PROFILER_PLOT("LogicFrame", now)`. No diagnostic code or UI
is added. Build/stage `win32-profile` separately following STAGE1_WORKFLOW.md,
with a fresh name such as `stage2-timing-profile`; never replace either baseline.

In Tracy 0.13.1, during a stable unpaused match segment with no loading/reset,
calculate render frame marks / elapsed seconds and
`(last LogicFrame value - first LogicFrame value) / elapsed seconds`. Use plot
**value change**, not number of plot samples or all GameLogic scope entries:
frozen script updates can repeat a frame value. The plot samples the frame
before simulation completion, so allow a one-tick boundary uncertainty. Record
120-second segments after warmup and repeat three times. Separate instrumentation
overhead from the ordinary Release reference. Expected example: Render about
60 FPS, Logic about 30 TPS. No such capture has been made yet.

If actual counters are unavailable, record TPS as UNKNOWN; a stopwatch comparison
supports gameplay-speed acceptance but does not prove the tick rate. Remaining
unknowns include every candidate gameplay result, general interpolation quality,
campaign/mod script speed behavior, replay/retail compatibility, multiplayer,
high-cap pacing and low-FPS/stall behavior under real load.
