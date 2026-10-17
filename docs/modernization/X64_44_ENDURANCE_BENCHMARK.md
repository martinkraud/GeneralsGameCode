# X64.44 - Real-game scalability / chokepoint endurance benchmark (CP18)

Status: **CLOSED - instrumentation delivered, native run completed and analysed (section 9).** This is a measurement milestone. Nothing about the simulation, the pathfinder, the AI, timing, gameplay or replays was changed.
Optimization, multithreading, Enhanced AI and Game Speed were explicitly out of scope.

Starting state: `dev/modern-engine` = `origin/dev/modern-engine` = `274c32e0a` (X64.43), clean tree. Authoritative benchmark fingerprint `74F2CB528BED42ED`.

## 1. Purpose

X64.43 showed that retail AI armies can seal chokepoints and make single path searches explore ~22 000 cells, and that the serial CPU work reached diminishing returns **on the canonical automated benchmark**. That benchmark (idle human,
7 Brutal AIs, 40 minutes) has only 600-800 live objects; the historical real-game symptom - a large map where AI armies keep growing and the game slows over time - is a different regime. The question this milestone prepares to answer:

> In a long, naturally growing Skirmish, when does the simulation start to degrade, which subsystem scales badly first, and can authoritative Normal speed (30 ticks per second) be sustained?

## 2. Two complementary benchmarks

| | A) canonical automated benchmark | B) real-game endurance benchmark (this milestone) |
|---|---|---|
| workload | Twilight Flame, idle human, 7 Brutal AI, 72 000 ticks, headless replay | a human plays a real native Skirmish (human + 5 AI, large 6-player map), natural growth |
| determinism | fingerprint `74F2CB528BED42ED` | not deterministic (human input); the live replay is kept for re-simulation |
| purpose | exact before/after comparison of serial optimizations | find the real scalability knee |
| tooling | `GENERALS_SIM_BENCH`, `GENERALS_SIM_PATHS`, ... | `GENERALS_ENDURANCE` (this document) |

A is unchanged and is not replaced. The telemetry can also run on A (headless replay) to validate itself.

## 3. Instrumentation (all off by default, read-only)

`GENERALS_ENDURANCE=<output directory>` enables `Core/GameEngine/Source/Common/EnduranceTelemetry.cpp` (`EnduranceTelemetry.h`), hooked at exactly two places per title (`Generals` and `GeneralsMD` `GameEngine::update`):
`EnduranceTelemetry::engineFrame();` and a `LogicTick` scope around the single `TheGameLogic->UPDATE();`. With the variable unset each hook is one test of a plain `int`.

Reuses existing measurement: the `PerformanceProfile` phase categories (through a private `Recorder` around each tick; if another recorder is already active the phase attribution is skipped and only timing is taken), the
`SimulationTickStats` real-time model (summary), and the X64.43 `PathFailureDiag`, extended with a **lite mode** (below). No new profiler was written.

* **Per completed logic tick:** duration (the first update that loads the map, paused or frozen updates are not ticks), start offset, phase attribution (29 simulation categories: object updates, AI global/player/object, movement,
  path queue/request/search/build, weapons, missiles, ZoneCalc and its parts, scripts, terrain, CRC, recorder, commands, build assist, partition, destroy, stores, disabled loop) and counters (update modules invoked, queued paths,
  queue cells, objects visited, ZoneCalc calls).
* **Every 300 logic frames (10 s of game time at Normal):** one CSV row with the interval's tick distribution (mean, p50, p95, p99, max, counts above 16.67 / 25 / 33.333 / 50 / 66.67 / 100 ms, an 11-bucket histogram), achieved tick rate and
  wall-clock lag, render-frame and paused-frame counts, a **read-only walk over the object list** (total objects; units and structures per player slot 0-11; stationary non-idle units = mobile units whose AI is neither idle nor moving,
  the candidates for the X64.43 blockers; moving and idle units), the phase costs, and the path-search statistics.
* **PathFailureDiag lite mode (`PathFailureDiag::enableLite`, never combined with the full `GENERALS_SIM_PATHFAIL` mode):** counters for every search (started, succeeded, exhausted the open list, exhausted >= 5 ms, failing time, pops) plus an
  analysis of **at most one expensive failure per 300 logic frames** (frontier classified as cliff / structure / ally non-idle in the footprint / uncrushable enemy / off map, number of blocking units, requester state and team). The
  analysis time is measured and subtracted from the tick it fell into (and from the `path_search` phase), and reported in `analysis_ms_excluded`.
* **Every logic tick** goes to a second file (`frame,tick_ms,start_ms`, ~25 bytes per tick, about 8 MB for a three-hour match) so exact percentiles and the real-time backlog can be rebuilt offline.

### 3.1 Safety and determinism guarantees

* Read-only: contract `scripts/performance/test_x64_44_endurance_contract.py` (9 tests, 6 mutation tests) forbids any mutating call, random-number use, flush or unwrapped `UPDATE()` in the telemetry and its two hooks; the X64.43 contract still
  forbids mutation in `PathFailureDiag`; the Stage4B.1 function-level guard is unchanged and passes.
* **No synchronous flush** (X64.42): output uses `WriteFile` on in-memory buffers (tick file: 64 KB blocks; interval rows and failure samples: one small write per 10 s) and never `FlushFileBuffers`/`fflush`. The OS keeps the data across a process
  crash; only an operating-system crash or power loss can lose the last buffered rows.
* Verified on the canonical workload: fingerprint `74F2CB528BED42ED` with telemetry on (with the benchmark recorder and with the telemetry's own recorder), digests equal with telemetry on and off on Dark Mountain, Rogue Agent and Armored Fury, and
  four default runs of the instrumented build match four of the previous build (mean 0.938 vs 0.936 ms, p99 6.07 vs 6.11 ms, ticks > 33.3 ms 4.5 vs 4.75; within noise).
* Overhead: the whole 72 000-tick run spends 86-93 ms in world scans and failure analyses (measured, excluded from the tick figures); the per-scope phase recording costs what the existing benchmark recorder already costs (it is part of every
  X64.38-X64.43 number).

## 4. Output (written to the directory given by `GENERALS_ENDURANCE`; one set per game)

`endurance-<UTC stamp>-meta.txt` (key=value: title, git, build, CPU/RAM, map, players with side/type/difficulty, logic and render frame rate settings), `-intervals.csv` (the time series), `-ticks.csv`, `-pathfail.csv` (rate-limited
analysed failures), `-summary.txt` (written when the game ends or the process exits). A session starts at the first logic tick of a non-shell game and ends when the game is left.

### 4.1 Interval columns (`-intervals.csv`)

`session_s, frame, game_s, wall_s, interval_ticks, interval_wall_s, achieved_tps, lag_s, engine_frames, paused_frames, non_tick_updates` - timing of the interval; `achieved_tps = interval_ticks / interval_wall_s`; `lag_s = wall_s - game_s` (includes
pauses; the analysis uses a queue model instead). `tick_mean_ms ... tick_max_ms, gt16_67 ... gt100, bucket0..bucket10` - tick distribution (buckets: <=1, 2, 5, 10, 16.67, 25, 33.333, 50, 66.67, 100 ms, above).
`objects_total, units_total, structures_total, stationary_nonidle_units, moving_units, idle_units, p<N>_units, p<N>_structs, p<N>_stationary_nonidle (N = 0..11)` - world scale. `ms_<phase>` - exclusive phase time summed over the interval.
`update_modules, queued_paths, queue_cells, objects_visited, zone_calc_calls` - throughput counters. `path_searches, path_ok, path_fail, path_fail_ge5ms, path_fail_ms, path_fail_worst_ms, path_fail_pops, path_analyses, fr_cliff, fr_struct, fr_ally_busy,
fr_enemy_fixed, fr_offmap, blocker_units, zone_recalc_observed, map_mutations` - search statistics and the X64.43 dynamic-blocking frontier of the analysed failures. `analysis_ms_excluded, scan_ms` - diagnostic time.

## 5. Analysis tool

`python -I scripts/diagnostics/Analyze-EnduranceTelemetry.py <telemetry directory> [--window-minutes N] [--json out.json]` prints: session facts; exact whole-session tick statistics and the back-to-back deadline model; the real-time lag (queue
model over un-paused intervals); a timeline per window (objects, units, stationary non-idle units, achieved TPS, **render FPS**, p99, worst, ticks over 33.333 ms, per-tick cost of ai_object_base_update / path search / scripts, path failures,
ZoneCalc); heuristic markers with fixed definitions (first window below 29.5 TPS - and whether the render frame rate was also below 30.5, because the logic can advance at most one tick per render frame; first lag >= 1 s; first p99 > 16.67 ms;
first window with >= 5 ticks over 33.333 ms; first persistent model backlog); phase scaling first-vs-last window and correlations with object count and with stationary non-idle units; congestion statistics; the 15 slowest ticks with their
interval context; a decision aid listing the dominant phases. Tests: `scripts/diagnostics/test_analyze_endurance.py` (synthetic sessions, no game).

## 6. Native test procedure (human)

The map is **not guessed**: choose the large 6-player map you have in mind yourself.

1. Candidate: `build\dev-runtimes\native\Generals\x64-44-endurance-v1` (staged from the committed sources; exe/PDB hashes in its `manifest.json`).
2. Start with the launcher (double-click) `scripts\native-runtime\Run-EnduranceTest.cmd`, or in PowerShell from the repository root:
   `pwsh scripts\native-runtime\Run-EnduranceTest.ps1`. It sets `GENERALS_ENDURANCE=build\endurance-telemetry\x64-44-endurance-v1-<stamp>` and runs `Invoke-NativeNormalLaunch.ps1 -RunNormalLaunch` (4 h cap). No other setting is needed.
3. In the game: Solo Play -> Skirmish. Pick the 6-player map, add **5 AI opponents** (Brutal, as intended for the stress), Normal game speed (the retail slider is hidden in this fork; Normal = 30 TPS), default Starting Cash unless you
   deliberately want another value (the replay header records it), no cheats. Your render settings as you normally play (note them).
4. Play normally and let the AI armies grow. **Do not stop** because an occasional hitch appears; continue until the game is clearly and persistently slow, the match ends, it becomes impractical, or you have a long run. There is no required duration.
5. End the run: finish or leave the match, then **Exit Game** from the menu (a clean exit writes the summary file). If the game becomes unusable, close the window; the interval and tick files are still complete up to the last 10 s block.
6. Immediately afterwards copy `Documents\Command and Conquer Generals Data\Replays\00000000.rep` (it is overwritten by the next game).

### 6.1 What to preserve and return

* The whole folder `build\endurance-telemetry\x64-44-endurance-v1-<stamp>\` (5 small files).
* `build\dev-runtimes\native\Generals\x64-44-endurance-v1\normal-launch\` (generals64.log, exit receipt, crash dumps if any).
* The copied replay `00000000.rep` and your notes from the checklist below.

### 6.2 Observation checklist (write times as the in-game clock if you can)

1. When did slowness first become noticeable? Does the game *speed* slow (units/animations/clock run slowly) or only the picture become choppy?
2. Roughly how large are the biggest AI armies, and where (own base, cliffs, a chokepoint, near your base)?
3. Are AI armies standing still at cliffs/chokepoints, filling their own base, or repeatedly trying to move without arriving?
4. Do large forces fail to attack?
5. Does one faction or player seem pathological?
6. If you destroy a congested army (or an AI base), does the game speed suddenly recover?
7. Did you pause, minimise, change settings or alt-tab (the analysis ignores paused intervals, but note it)?
8. Machine state: other programs running, power mode, render FPS cap/VSync/Render Scale settings.

## 7. Post-run analysis plan (nothing is concluded here)

Build a timeline of: elapsed time -> live objects/units (per player) -> stationary non-idle units -> ai_object_base_update / object updates -> path requests -> failed and expensive searches and their blockers -> ZoneCalc, scripts and other phases -> tick distribution ->
missed deadlines and backlog -> achieved TPS and real-time lag, and attribute any slowdown to **render** (render FPS < 30.5 while ticks are cheap) or **simulation** (ticks consuming the budget). Identify the scalability knee if one exists with
statements scoped like "in this workload on this machine, sustained 30 TPS began degrading around ...". No unit-count limit is claimed.

### 7.1 Decision criteria (agreed in advance)

* **A** - the serial simulation stays healthy at very large natural workloads (lag ~0, TPS ~30, tail small): proceed to Zero Hour bring-up / completeness; no speculative multithreading.
* **B** - `ai_object_base_update` (or another throughput phase) becomes the dominant, growing limiter while the tail stays modest: investigate a deterministic multithreading / job-system candidate (read-only snapshot -> parallel computation ->
  deterministic ordered commit; document shared state, RNG, create/destroy, ordering, command/event dependencies first).
* **C** - path failures / congestion dominate again at real scale: characterise the real workload first, then choose between engine scalability work and a separate, optional Enhanced AI project (congestion awareness, stuck detection, staging, alternative
  routes); Classic behaviour stays the preservation path.
* **D** - another subsystem (including rendering) dominates: investigate that subsystem specifically.

## 8. Limitations

One human-played match is one sample on one machine; it is not deterministic. The tick figures come from the logic thread only; render cost shows up in `achieved_tps` / `engine_frames`, not in the tick time. `stationary_nonidle_units` is an
estimate of potential blockers from AI state, not the exact `checkForMovement` outcome (the sampled failure analyses give the exact frontier). The failure analysis is rate-limited (one per 10 s of game time), so its counts are samples. The
object scan counts live objects of the engine's list, including scenery and projectiles in `objects_total`. Intervals with paused frames are excluded from lag and TPS conclusions.

## 9. Native result (2026-10-17) - CLOSED

Evidence: `build/endurance-telemetry/x64-44-endurance-v1-20261017T075742Z/` (interval/tick/pathfail CSV, meta, summary, `analysis.json`, and `endurance-run-00000000.rep`, the preserved replay of the match; hash verified), candidate receipts in
`build/dev-runtimes/native/Generals/x64-44-endurance-v1/normal-launch/` (exit code 0, no crash dump). These directories are git-ignored build output and are not touched by the cleanup tools.

### 9.1 The run

Twilight Flame (an 8-slot map; not a 6-player map), one human plus 5 Brutal AI (China, America, 3x GLA), normal launch, Normal speed. **92 656 ticks = 51.5 min of game time, 51.3 min of wall time, no pauses.** The AI armies did **not** reach the
historical pathological size: no huge stationary armies at chokepoints were observed by the player, and the telemetry agrees (peak 153 stationary non-idle units).

| minute | objects | units | stationary non-idle | p99 tick ms | worst ms | ai_object_base_update ms/tick | path_search ms/tick |
|---|---|---|---|---|---|---|---|
| 5 | 711 | 109 | 25 | 7.1 | 17.0 | 0.72 | 0.06 |
| 10 | 911 | 267 | 92 | 10.4 | 21.4 | 2.18 | 0.13 |
| 15 | 1 006 | 343 | 94 | 11.1 | 41.5 | 2.64 | 0.14 |
| 20 | 1 022 | 351 | 96 | 11.7 | 28.3 | 2.38 | 0.13 |
| 25 | 1 024 | 362 | 99 | 12.8 | 41.9 | 2.36 | 0.16 |
| 30 | 1 068 | 427 | 134 | 13.0 | 40.2 | 2.54 | 0.18 |
| 35 | 1 047 | 423 | 118 | 10.9 | 32.1 | 2.18 | 0.13 |
| 40 | 1 038 | 420 | 69 | 12.8 | 39.1 | 2.74 | 0.20 |
| 45 | 953 | 357 | 43 | 8.2 | 15.3 | 1.41 | 0.03 |
| 50 | 841 | 274 | 42 | 9.8 | 12.2 | 1.13 | 0.04 |
| 51.5 | 727 | 213 | 26 | 5.6 | 11.0 | 0.90 | 0.01 |

(5-minute windows; each row is the end of its window.) Peaks: **1 227 live objects (28 min), 469 units (35 min), 222 structures (15.7 min), 153 stationary non-idle units (29 min)**; per-slot unit peaks: human 163, AIs 56-118. The world reached about 1 000 objects by ~10 min,
plateaued, and shrank after ~40 min as AIs were destroyed.

### 9.2 PROVEN FOR THIS RUN (this workload, this machine, one match)

* **Within budget:** tick mean 4.15 ms (12 % of 33.333 ms), median 4.01, p95 6.87, p99 10.97 (33 %), p99.9 17.2 (52 %), worst 41.9 ms (126 %). Ticks above 16.67 / 25 / 33.333 / 50 ms: 109 / 30 / 6 / 0 (0.118 % / 0.032 % / 0.0065 % / 0 %).
  Derived missed deadlines 6, longest catch-up run 1, maximum backlog 8.5 ms.
* **Cadence:** 308 un-paused intervals, achieved rate median 29.998 TPS (range 29.89-30.18); interval wall time 9.907-10.037 s (ideal 10.000); the queue-model real-time lag at the end was 0.35 s over 51 minutes. Render FPS median 95, minimum 51, never near
  the 30 boundary, so the logic was not render-bound. **No scalability knee and no sustained degradation occurred.**
* **CPU attribution (share of the summed tick time):** ai_object_base_update 48 %, object_update_loops 21 %, partition 7.6 %, scripts 5.4 %, path search 2.8 %, locomotor 2.7 %, path request entries 2.3 %, ai_player_strategy 1.6 %, ZoneCalc (all parts) 1.4 %
  (783 recalculations, 7.05 ms each). `ai_object_base_update` cost 4-8 microseconds per unit per tick, **linear** with the unit count (7.7 / 7.2 / 6.2 / 5.8 / 4.0 us per unit in successive 10-minute windows); no superlinear growth was observed.
* **Pathfinding:** 54 142 searches, 834 exhausted the open list (1.5 %), 128 of those took >= 5 ms, worst failing search 33 ms, total failing time 1.79 s = 0.47 % of tick time. All 44 sampled lite analyses had blocking units on the explored frontier (the X64.43
  mechanism, at small scale); 20 of the 44 had no hierarchical path (fallback), 41 of 44 were terrain-connected. Not a meaningful scalability cost in this run.
* **The tail events are deterministic simulation events.** The complete match (92 656 frames, human commands included) was re-simulated headless from the preserved replay: **zero CRC mismatches, 3 088 matching fingerprint checkpoints (digest `BBEBE2E01257A379`),
  926 replay CRC checks**, and 26 of the 30 slowest native ticks occurred on the same simulation frames (per-tick correlation 0.89). Most are path searches of 20-34 ms; one is a 38.7 ms script action (`AI player build near a supply source`, script
  `GLA Base Expand Start 26`, frame 37 450, 20.8 min); one is an ~18 ms `ai_player_strategy` tick (frame 18 702, 10.4 min). One event each: nothing is optimised on this basis.
* **First long real-game determinism validation:** a 51-minute human-played Skirmish recorded by the native build re-simulates with no divergence (the Starting Cash replay fix of X64.41 was present). This is evidence for this match and this build; it is
  not a proof of general replay or network determinism.
* **Real graphical game vs headless:** the same simulation costs 4.15 ms per tick in the native game and 2.12 ms headless (**about 1.95x**, uniform across ticks while the tail events cost about the same). The cause is **not known** (candidates: client-side
  modules attached to objects, cache pressure from rendering on the same thread, memory behaviour). It applies to this observed run, not to all workloads; the canonical headless numbers in `PERFORMANCE.md` are therefore a lower bound on real-game cost.

### 9.3 SUGGESTED (not demonstrated)

* Substantial measured headroom exists at this scale. At the observed 4-8 us per unit per tick a rough linear extrapolation reaches the budget at several times this unit count; this is an extrapolation, **not a supported limit**, and no unit-count maximum is claimed.
* The occasional over-budget ticks are rare single-thread bursts (path search, one script action, one AI strategy tick), which parallelising the per-object loop would not remove.

### 9.4 NOT TESTED / NOT ESTABLISHED

* The historical pathological workload (very large AI armies accumulating at cliffs and chokepoints, armies failing to attack, progressive slowdown) was **not reproduced**; this run says nothing about performance under it. That workload was remembered from Zero Hour and
  is to be measured there.
* Other maps, Zero Hour, multi-hour matches, larger or different AI mixes, multiplayer/lockstep.
* The player's report of occasional mouse clicks that needed repeating: **unconfirmed**. There are no click timestamps and the telemetry has no per-render-frame timing (only per-interval render frame counts and logic-tick times), so no correlation can be established or
  excluded. Do not treat it as simulation lag.

### 9.5 Telemetry fix made at closeout

The summary reported `last_frame=1`: the update that leaves the game resets the frame counter and had been recorded as the final "tick" (in this run: frame 92 656, 32.1 ms, the world teardown; the published count of 30 ticks above 25 ms includes it, 29 without).
`LogicTick` now treats an update whose frame counter went backwards as a non-tick (`scripts/performance/test_x64_44_endurance_contract.py`, 3 new tests). Telemetry-only change; fingerprint `74F2CB528BED42ED` unchanged with it on. Not yet confirmed natively.

### 9.6 Decision (precommitted rules A-D)

Outcome **A** for the observed workload: the serial simulation remained healthy at the largest natural scale reached. **Do not add multithreading before Zero Hour on this evidence; the serial CPU optimization phase stays closed.** Next major work: Zero Hour native
validation and completeness, then repeat the endurance telemetry there (reusing `GENERALS_ENDURANCE`). Open backlog items from this run: the 1.95x real-vs-headless per-object overhead (cause unknown), and an optional per-render-frame hitch log if the input report needs
chasing.
