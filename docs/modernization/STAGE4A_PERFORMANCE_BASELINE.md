# Stage 4A: CPU performance baseline and instrumentation

Follow-up: real developer A/B captures are analyzed directly in
[STAGE4A1_PATHFINDING_DEEP_PROFILE.md](STAGE4A1_PATHFINDING_DEEP_PROFILE.md).
Pathfinding is a measured B-stall lead; existing aggregate data does not identify
the exact Stage 4B fix. Stage 4A.1 adds optional individual-search detail.
The remaining text records the original Stage 4A implementation/validation state.

Started clean on dev/modern-engine at 1de7e65d173104ad2f288ac0a379e102a5057ebc. Latest accepted Stage 3C.4 commit: 1de7e65d1; prior 289be9bf9, 8fdc9c939, 14aed7d51. Developer reports full 1v1 acceptance and same-binary OFF visibly stepped turning. Full high-refresh/ultrawide checks remain later. Large-map developer + 5 AI combat spikes already occurred ON and OFF; this is a baseline workload, not causal evidence against interpolation. No game launch or real bottleneck measurement is performed by this task.

## Pre-implementation call-tree audit

Both GameEngine::execute loops call update then FramePacer::update. The outer measurement includes the existing frame limiter sleep/spin in its own category. GameEngine::update runs radar/audio, GameClient, message propagation, optional Network, scheduler readiness, optional GameLogic and client step. No scheduler branch/order changes.

GameClient::update includes existing presentation capture, drawable/client/shroud work, TerrainVisual update, Display update, conditional particle-manager update, Display draw, UI/shell. W3DDisplay::update prepares visible Drawables; W3DDisplay::draw drives views/UI/movie/render frame; W3DView::draw submits scene/culling/filter work. WW3D::End_Render flushes renderer and may call DX8Wrapper Present; measure this combined boundary explicitly as flush/present, not GPU time or pure Present.

GameLogic::update may early-return during pause/freeze/loading. It processes recorder/CRC/commands, normal and sleepy update-module loops, AI global update, partition, destruction, weapon/locomotor stores, disabled-object maintenance, then increments m_frame only on a completed tick. Count that increment separately from invocations. Do not report rendering iterations as simulation ticks.

Object update loops invoke modules selected by existing sleep/disabled rules. AIUpdateInterface::update includes AI state-machine/turret/locomotion work; special subclass overrides may add work outside the base. AI::update processes Pathfinder queue and PlayerList; AIPlayer::update is a useful player-strategy boundary. Locomotor::locoUpdate_moveTowardsPosition is coarse movement work. Pathfinder public request functions, internal ground/hierarchical search, and path reconstruction are distinct nested intervals; queue counters already expose pathsFound and cumulative cells allocated. These are entry/queue-cell counts, not globally unique orders or universal expanded-node counts.

Weapon::privateFireWeapon is the common firing path (including special/detonation wrappers); WeaponStore::update covers delayed damage/store work, not all combat. MissileAIUpdate::update includes its base AI update. Other physics/projectile/damage/targeting work can remain in object/logic residuals. Do not mislabel selected combat boundaries as total combat.

Existing USE_PERF_TIMER/PerfGather is disabled by NO_PERF_TIMERS, uses manually enabled TSC/QPF configurations and old calibration/aggregation; existing Tracy/WWPROFILE infrastructure is compile-option dependent and is not enabled for this baseline. FrameRateLimit already uses Windows QPC/QPF. Reuse those API primitives without reading or modifying its scheduler anchors. New profiler is independent observer state, with integer QPC ticks in measured code and double conversions only in report output.

## Chosen measurement architecture

Always-compiled, runtime opt-in developer probes: -performanceProfile <absolute-output-directory>. Default OFF; disabled scopes read a null thread-local recorder pointer and perform no QPC, allocation, names or I/O. No compile-option matrix or external dependency is needed. The active pointer exists only on the main capture thread, leaving future threads unprofiled until separate per-thread buffers/merge semantics are designed. No worker threads are created.

Fixed-depth scope stack and bounded preallocated per-outer-frame records. Integer inclusive/exclusive totals and invocation counts per category, plus cheap counters, frame IDs and FPS settings. Same-category recursion counts invocations but inclusive time only at its outermost category occurrence; exclusive time is self time over all occurrences. Never sum inclusive categories. Outer exclusive and parent self time are residual/unclassified work, not an invented subsystem.

No per-object timing log or string classification. Object loops get coarse aggregate scopes and cheap invocation counts; selected AI/movement/combat functions add scopes. File control polling and output happen outside the measured outer interval. QPC itself is an observation; no measured value controls gameplay, scheduling or interpolation.

Capture uses command.txt in the supplied output directory: unique start/stop commands, polled at most once per second while opt-in is enabled. Start allocates/clears buffers outside measured frames; stop writes reports outside measured frames. Auto-stop at 60 seconds or bounded frame capacity. No hotkey or game command/input/replay integration. Control/output stalls can perturb adjacent real frame pacing, so capture/report overhead and excluded gaps must be documented; do not reset FramePacer to hide them.

## Categories, counters and interpretation

The actual nesting is dynamic; a path request can originate from object AI or player strategy. The main hierarchy is:

```text
Outer frame
  Engine update
    Client update
      Drawable/shroud/ghost update block
      Terrain visuals; Display update; conditional particles
      Display draw -> Scene view draw; render-end flush/possible Present
    Message propagation; optional network update
    Optional GameLogic update
      Normal/sleepy object-update loops
        AI object base -> state machine/turrets/movement
        Missile update -> AI object base
        Weapon fire (where invoked)
      Global AI -> path queue; player strategy
      Weapon store; other logic work
  FramePacer wait/bookkeeping
```

All 26 category names are defined in PerformanceProfile::name(Category). The CSV includes inclusive, exclusive and invocation counts for every category. Exclusive time subtracts directly nested measured intervals; uninstrumented work stays in the nearest parent's self time. Instrumentation overhead also stays in measured durations. Same-category recursion adds calls but counts only the outer occurrence inclusively, avoiding double counting recursive path searches. These totals describe CPU wall time, including preemption and waits, rather than CPU execution cycles.

Six integer counters record completed m_frame increments, update-module invocations, visits in the existing disabled-object traversal, drawable updates, processed queued paths and cumulative queue cells allocated. Objects visited are not unique objects across the capture, nor an active-object gauge. Use visits per completed tick as a rough population indicator, retaining lifecycle caveats. AI/movement/request/search counts come from category invocation counts, not unique AI-controlled units or movement orders. No new object traversal or per-search-node counter is introduced.

Path cache/quick-existence checks are inside request boundaries where called; hierarchical/ground search and reconstruction have separate scopes. Zone maintenance in processPathfindQueue belongs to its residual. Aircraft/special requests and obstacle/terrain queries outside these selected entries remain in enclosing AI/movement/logic costs. WeaponTemplate::dealDamageInternal performs direct and area damage; firing and delayed-store callers cover some of it. Target acquisition, collision queries, non-missile physics, debris/decals/shadows and FX creation are not universally isolated. Their costs remain in enclosing object/client/render residuals. This first pass identifies a subsystem before adding finer probes.

## Samples and output

Each frame retains offset from capture start, logic frame before/after, requested/effective FPS cap, counters and category totals. A render-only iteration has zero completed ticks. GameLogic invocation statistics can include pause/freeze early returns. Category statistics use sums per outer frame in which that category was called; zeros on inactive frames are excluded. This is explicitly not a histogram of every individual invocation. Normally the execute loop performs at most one GameLogic update per outer iteration. If a future/reentrant path performs several, their costs are aggregated and the completed count remains explicit; there is no separate per-tick record.

Four files share a UTC timestamp/PID/serial stem:

| Suffix | Content |
| --- | --- |
| -summary.txt | Title, Git SHA/dirty state, MSVC/build timestamp, capture start/report UTC, label, stop reason, interpolation state, QPC frequency, elapsed seconds, frame/tick counts, initial caps, counters and category tail summaries |
| -frames.csv | Every bounded frame, offsets/IDs/caps/counters and all category inclusive/exclusive/calls |
| -categories.csv | 52 inclusive/exclusive rows: active-frame count, invocations, total/mean/min/max/p50/p95/p99 milliseconds |
| -slow.csv | Top 10 outer frames and top 10 completed-logic frames, ranked independently, with every category breakdown and offsets/IDs |

Nearest-rank percentiles and stable top-N ranking avoid an arbitrary spike threshold. For a visible stall, use its window label/time offset and frame IDs to examine slow.csv and frames.csv. The slow logic ranking requires at least one completed tick and ranks the frame's GameLogic aggregate. It must not be described as independent tick timings when multiple ticks occur. Summary elapsed time includes inter-frame gaps up to the last recorded frame end; summed outer measurements omit control/allocation/report gaps. A pending start is consumed once; start during an active capture is ignored. Stop when inactive is harmless. Use a fresh nonce for every command and a fresh start after an automatic stop.

Default OFF avoids QPC and I/O in all scopes. Opt-in but idle polls the file at most once per second and queries QPC at the outer boundary. Capturing uses a 64-deep fixed stack and reserves 16,384 frames once, stopping at capacity or 60 seconds. An individual blocked frame may exceed that time before control returns. No per-scope allocation or formatted output occurs. Report/statistics allocations and synchronous writes occur after measurement; they may visibly stall the game at capture end. Errors disable profiling or emit OutputDebugString diagnostics without controlling gameplay; failed output is not a successful capture. Abrupt termination/crash need not produce reports. Windows file-control/frontend operation remains a guarded manual acceptance item; automated report tests cover the data/report layer.

## Automated validation and observer overhead

Windows x86 Release, Ninja Multi-Config win32 preset, MSVC 14.51.36231 (/W3, dynamic Release CRT); separate legacy profiling/Tracy build options remain OFF. Configure succeeded. Full build succeeded for both games, WorldBuilders and Google tests. Final CTest passed 2/2; direct binaries passed 84 tests per title (168 total), including 10 deterministic profiler tests per title. The separately invoked disabled Release profiler benchmark passed for both titles. Unit tests cover disabled/no clock, nested RAII and recursion accounting, bounded storage including zero capacity, reset/start/stop, nearest-rank and empty statistics, top-N/completed filtering, counters/render-only frames, metadata/CSV/empty report output, stack overflow and regressed clocks. Correctness assertions use fake timestamps, not hardware thresholds.

Intermediate results are retained: the initial build succeeded, but its CTest run failed the report test in both titles with SEH 0xc0000005 (83/84 each). Metadata layout was edited while that build was running, leaving inconsistent objects. Rebuilding all affected consumers after edits resolved the failure; the final CTest and direct tests pass without changing the report algorithm to hide it. Auxiliary edit scripts also required corrections for the forced-PCH W3D files and matching an actual definition rather than a comment; the resulting production diff was reviewed. An automatic approval review timed out on a large workspace-writing shell command before it ran; smaller apply_patch edits completed the work. No safety rejection remains blocked.

Median of five repeats, 500,000 operations per repeat, on this development machine:

| Operation | Generals ns/op | Zero Hour ns/op |
| --- | ---: | ---: |
| Disabled scope | 1.682 | 1.671 |
| Enabled scope, two QPC reads + aggregation | 56.785 | 58.481 |
| QPC query | 24.198 | 24.494 |
| Synthetic push/pop, no QPC | 9.479 | 8.474 |
| Enabled integer counter | 1.862 | 1.866 |

Disabled/enabled scope tests include a no-inline harness call to prevent folding away the disabled branch. These are approximate synthetic costs, not in-game overhead guarantees. At 10,000 scopes/tick, 62 ns each would be roughly 0.62 ms/tick; real calls/caches may differ. Inspect invocation counts in actual captures before adding finer probes. The retained Frame is 696 bytes, capacity 16,384, reserved payload 11,403,264 bytes (~10.88 MiB), plus small fixed stack/recorder overhead and temporary reporting allocations. No Object/Drawable fields grow.

Build logs: build/stage4a-configure.log, stage4a-build.log, stage4a-build-final.log; initial/final CTest logs; stage4a-g-direct.log, stage4a-z-direct.log; stage4a-g-benchmark.log, stage4a-z-benchmark.log. Changed-line warning audit found zero warnings on added/changed lines, with 140 existing warning occurrences across the two successful build logs (not 140 unique warnings). Tracked git diff --check and separate new-file whitespace checks were run; CRLF conversion advisories are Git line-ending notices rather than whitespace defects. Generated verification helpers/logs remain ignored under build.

Final allocator/lifetime audit additionally found retained path/command strings. GameMemory.cpp routes global new/delete through the engine allocator, so profiling now releases these strings/path as well as the recorder before engine teardown. GameEngine destruction also invokes idempotent shutdown, covering initialization/execute failure cleanup when the engine exists. No allocator architecture was changed. The subsequent full build (stage4a-build-shutdown-final.log, zero warnings), CTest 2/2 (stage4a-ctest-shutdown-final.log), 168 direct tests and both benchmarks pass. The table above uses this final rerun. Earlier benchmark measurements were ~1.6 ns disabled / 58–62 ns enabled and remain comparable. There is no in-game measurement yet.

An auxiliary whitespace command disabled Git CRLF normalization and consequently misclassified existing carriage returns as trailing whitespace, producing a noisy failed check without modifying files. The corrected tracked check uses repository settings; the separate new-file check explicitly accepts CR-at-EOL. Both pass with zero whitespace defects. No mass line-ending conversion was performed to satisfy the faulty check.

## Guarded runtime candidate and integrity

Fresh candidate: build/dev-runtimes/zh/stage4a-performance-baseline/game. Staged with the unchanged guarded Stage-ZHRuntime.ps1 workflow from the explicit Steam Zero Hour source. Complete copy retained existing wrappers/modifications; only the fresh candidate received the development EXE/PDB overlay. No previous runtime was overwritten. Manifest and full source/runtime inventories are in the candidate's parent directory.

The first Stage 4A staging was retained as stage4a-before-shutdown-audit; its game files were moved intact within the guarded runtime root, with original manifests retained and a new manifest recording the relocated paths. The helper initially refused overwriting an existing manifest and a 49-character name exceeding its 48-character bound; the preservation used fresh manifest filenames and a shorter valid name. Its full 369-file inventory and relocated EXE/PDB identity pass. The requested stage4a-performance-baseline was then freshly staged from the final validated shutdown build; no earlier acceptance candidate was moved or overwritten. Logs: stage4a-stage-final.log, stage4a-integrity-source-final.log, stage4a-integrity-candidate-final.log, stage4a-launch-validation-final.log and stage4a-integrity-intermediate.log under build.

| Identity | Value |
| --- | --- |
| Source revision | 1de7e65d173104ad2f288ac0a379e102a5057ebc, dirty Stage 4A worktree |
| EXE SHA-256 | B2FCAB752588BCC11306F1F2C8643DFCD8054778A6EDABA7EDCC102C5625E0D8 |
| PDB SHA-256 | 47AA4386DFC0225808A2BB0A73BFCB6FF994A46570F11789C5863D90A8222FF5 |
| Matching EXE/PDB GUID | 5706bc8d-51e9-49fc-9096-a636d0fd56be |
| Matching age | 15 |

Read-only full inventory checks PASS, 369 paths/lengths/SHA-256 values each, for the Steam source, new candidate, baseline, stage2-timing, stage3a-fps-options, stage3c2-ground-interpolation, stage3c3-ground-hardening and stage3c4-ground-orientation. Logs: build/stage4a-integrity-source.log, stage4a-integrity-candidate.log and stage4a-integrity-<previous-name>.log. Launch-ZHRuntime -ValidateOnly with the profiling arguments and BackupUserData also passes; no backup/receipt was actually written. This verifies guarded staging/argument construction, not runtime flag parsing or gameplay/performance. No game launch, Install, Steam/user-data modification, commit, push or execution-policy change occurred. Developer review and actual A/B workload captures are pending.

## Manual Scenario A: light baseline

These commands are for the developer after review, from the repository root in PowerShell 7. They have not been used to launch the game by the agent. Keep the generated reports under ignored build/performance. A dedicated Windows test account is preferable; the existing launch helper below creates a verified user-data backup when actually launched. ValidateOnly does not create that backup or launch.

```powershell
$profileDir = 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a-A'
New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
Set-Content -LiteralPath (Join-Path $profileDir 'command.txt') -Value ('idle ' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a-performance-baseline -CheckSource
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage4a-performance-baseline
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage4a-performance-baseline -BackupUserData -GameArguments @('-groundInterpolation', '-performanceProfile', $profileDir) -ValidateOnly
# Developer launch after reviewing validation:
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage4a-performance-baseline -BackupUserData -GameArguments @('-groundInterpolation', '-performanceProfile', $profileDir)
```

Set and Accept a persistent finite 120 FPS render cap in Options if stable; otherwise choose a sustainable finite cap and use it unchanged in all comparisons. Do not use unlimited, legacy speed/debug FPS overrides, or a changed Game Speed setting. Retain accepted ground interpolation ON. Choose one small/medium stock map for developer vs 1 AI; record the exact map, factions, AI difficulty, resolution/detail/VSync/wrapper settings, power mode and approximate army sizes in notes beside the reports. Keep these fixed for repeats. Record any pause, focus loss, save/load, camera change or visible stall in each window.

In a second PowerShell terminal, reuse the same absolute $profileDir value. First capture idle/base building, then ordinary movement, then modest combat as three separate windows. For each, change the label below (A-idle, A-movement, A-combat), issue start after setup and play 30–60 seconds. Poll delay is up to one second. Either stop after about 30 seconds using the second command or let the 60-second bound stop it. Wait for all four report files before the next start. Do not include report-writing stalls as gameplay spikes.

```powershell
Set-Content -LiteralPath (Join-Path $profileDir 'command.txt') -Value ('start A-idle-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
# After the desired window, or omit this and allow automatic stop:
Set-Content -LiteralPath (Join-Path $profileDir 'command.txt') -Value ('stop A-idle-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
Get-ChildItem -LiteralPath $profileDir
```

Check summary title/dirty Git identity/build, interpolation=1, initial caps, elapsed duration, outer count versus completed ticks, and stack_or_clock_errors=0. At a stable 120 cap, render-only frames should be present and completed ticks should approach 30 per unpaused second; this is an observation to verify, not a hard benchmark assertion. Confirm each CSV opens and that top rows map to retained frame IDs. Repeat a representative window without -performanceProfile in the same binary to check noticeable observer interaction; this is not another interpolation ON/OFF investigation.

## Manual Scenario B: stress baseline

Exit the previous process. Repeat the Scenario A integrity and launch commands with $profileDir set to 'C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\performance\stage4a-B'. Initialize its command.txt to a fresh idle command before launch. Retain the same finite cap, interpolation ON, resolution and detail settings. Choose and record one large stock map with developer + 5 AI, factions/difficulties/start positions and relevant game rules. Use the same map/settings for later engine comparisons; do not assume perfectly identical gameplay.

Capture B-precombat once armies are established before major fighting, then B-combat during the known busy-battle/stutter workload. Use the Scenario A start/stop commands with those labels and the B directory, 30–60 seconds each; repeat B-combat if needed to catch a visible stall. Record approximate match time, army composition/size, camera position, observed stall offset and any focus/pause transitions. Keep camera behavior comparable between repeats; rendering load depends on what is visible. Preserve all four reports for every window plus the runtime manifest and notes. If saving/reloading to set up repeats, do so only through the developer's guarded test session and record it; no such data was created here.

Compare outer/logic/client distributions and their p95/p99/max; separate limiter wait from engine work. Inspect category exclusive time and counters in the slow frames. A high render-end value can be driver/VSync/GPU waiting and does not prove a CPU rendering algorithm is expensive. Large residuals justify a focused additional probe before choosing an optimization.

## Determinism and follow-up decision

Final instrumentation only adds scope lifetimes, read-only frame/cap metadata and profiler-local integer counters. Existing gameplay calls remain in their original order with original arguments; no gameplay expression is evaluated through a profiler macro. No RNG, scheduler anchors, command generation/frame assignment, network payload, serialization, CRC, save/replay state, Object transforms, AI/path decisions, floating-point flags or simulation frequency changes. Profiling can affect wall-clock pacing through observer cost and file I/O; source review and synthetic tests do not establish network/replay runtime equivalence. Network timing is its existing update boundary; recorder/CRC/replay work inside GameLogic remains in its residual. No multiplayer profiling acceptance is claimed.

Source-level candidates only, without optimization: queue zone maintenance/search/reconstruction; sleepy update selection/heap maintenance and large module counts; player strategy/target acquisition; weapon delayed/area damage and other object residuals; per-player shroud/ghost drawable work; particles, shadows/treads/scene submission and renderer flush/waits. No candidate has been measured in a real match by this task. The known large-map spikes happened with interpolation ON and OFF before this stage.

Stage 4B is intentionally undecided. First collect A and B windows, confirm overhead is acceptable and the reports trustworthy, repeat the stress tail, and identify a category/residual with reproducible cost and correlating workload counts. Isolate waits from useful CPU work and inclusive overlap from exclusive cost. If attribution is too coarse, add a small targeted observer before changing behavior. Then propose one bounded, evidence-backed single-thread optimization with repeatable before/after captures and determinism/behavior acceptance criteria. Multithreading, renderer replacement, x64 and balance changes are not automatic next steps. Later main-PC high-refresh/5120x1440 results remain necessary; do not tune solely for this laptop.

## Final worktree record

Exact `git status --short --untracked-files=all` (no index changes):

```text
 M Core/GameEngine/CMakeLists.txt
 M Core/GameEngine/Source/Common/CommandLine.cpp
 M Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp
 M Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp
 M Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp
 M Generals/Code/GameEngine/Source/Common/GameEngine.cpp
 M Generals/Code/GameEngine/Source/GameClient/GameClient.cpp
 M Generals/Code/GameEngine/Source/GameLogic/AI/AI.cpp
 M Generals/Code/GameEngine/Source/GameLogic/AI/AIPlayer.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Locomotor.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/MissileAIUpdate.cpp
 M Generals/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp
 M Generals/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp
 M GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp
 M GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AI.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AIPlayer.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Locomotor.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/MissileAIUpdate.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp
 M GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp
 M Tests/Google/Core/CMakeLists.txt
 M docs/modernization/AGENT_HANDOFF.md
 M docs/modernization/ROADMAP.md
?? Core/GameEngine/Include/Common/PerformanceProfile.h
?? Core/GameEngine/Source/Common/PerformanceProfile.cpp
?? Core/GameEngine/Source/Common/PerformanceProfileRuntime.cpp
?? Tests/Google/Core/GameEngine/Common/PerformanceProfileBenchmark.cpp
?? Tests/Google/Core/GameEngine/Common/PerformanceProfileTest.cpp
?? docs/modernization/STAGE4A_PERFORMANCE_BASELINE.md
```

Exact tracked `git diff --stat` (new untracked files are listed above and included separately in build/stage4a-review.patch):

```text
 Core/GameEngine/CMakeLists.txt                        |  3 +++
 Core/GameEngine/Source/Common/CommandLine.cpp         | 12 ++++++++++++
 Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp    | 13 +++++++++++++
 .../Source/W3DDevice/GameClient/W3DDisplay.cpp        |  9 ++++++---
 .../Source/W3DDevice/GameClient/W3DView.cpp           |  2 ++
 Generals/Code/GameEngine/Source/Common/GameEngine.cpp | 14 +++++++++++---
 .../Code/GameEngine/Source/GameClient/GameClient.cpp  |  8 ++++++--
 Generals/Code/GameEngine/Source/GameLogic/AI/AI.cpp   |  2 ++
 .../Code/GameEngine/Source/GameLogic/AI/AIPlayer.cpp  |  2 ++
 .../GameEngine/Source/GameLogic/Object/Locomotor.cpp  |  2 ++
 .../Source/GameLogic/Object/Update/AIUpdate.cpp       |  2 ++
 .../Object/Update/AIUpdate/MissileAIUpdate.cpp        |  2 ++
 .../GameEngine/Source/GameLogic/Object/Weapon.cpp     |  3 +++
 .../GameEngine/Source/GameLogic/System/GameLogic.cpp  |  8 ++++++++
 .../Code/GameEngine/Source/Common/GameEngine.cpp      | 14 +++++++++++---
 .../Code/GameEngine/Source/GameClient/GameClient.cpp  |  8 ++++++--
 GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AI.cpp |  2 ++
 .../Code/GameEngine/Source/GameLogic/AI/AIPlayer.cpp  |  2 ++
 .../GameEngine/Source/GameLogic/Object/Locomotor.cpp  |  2 ++
 .../Source/GameLogic/Object/Update/AIUpdate.cpp       |  2 ++
 .../Object/Update/AIUpdate/MissileAIUpdate.cpp        |  2 ++
 .../GameEngine/Source/GameLogic/Object/Weapon.cpp     |  3 +++
 .../GameEngine/Source/GameLogic/System/GameLogic.cpp  |  8 ++++++++
 Tests/Google/Core/CMakeLists.txt                      |  2 ++
 docs/modernization/AGENT_HANDOFF.md                   | 19 +++++++++++++++++++
 docs/modernization/ROADMAP.md                         | 17 +++++++++++++++++
 26 files changed, 150 insertions(+), 13 deletions(-)
```

Exact tracked `git diff --numstat`:

```text
3	0	Core/GameEngine/CMakeLists.txt
12	0	Core/GameEngine/Source/Common/CommandLine.cpp
13	0	Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp
6	3	Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp
2	0	Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp
11	3	Generals/Code/GameEngine/Source/Common/GameEngine.cpp
6	2	Generals/Code/GameEngine/Source/GameClient/GameClient.cpp
2	0	Generals/Code/GameEngine/Source/GameLogic/AI/AI.cpp
2	0	Generals/Code/GameEngine/Source/GameLogic/AI/AIPlayer.cpp
2	0	Generals/Code/GameEngine/Source/GameLogic/Object/Locomotor.cpp
2	0	Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp
2	0	Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/MissileAIUpdate.cpp
3	0	Generals/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp
8	0	Generals/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp
11	3	GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp
6	2	GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp
2	0	GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AI.cpp
2	0	GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AIPlayer.cpp
2	0	GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Locomotor.cpp
2	0	GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp
2	0	GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/MissileAIUpdate.cpp
3	0	GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp
8	0	GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp
2	0	Tests/Google/Core/CMakeLists.txt
19	0	docs/modernization/AGENT_HANDOFF.md
17	0	docs/modernization/ROADMAP.md
```
