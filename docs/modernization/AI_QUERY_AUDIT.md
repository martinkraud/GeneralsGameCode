# AI query, object update and seeded graphical benchmark audit (X64.aq)

Starting point: `5ada0f901` (X64.sc). Scope: measure the dominant AI spatial queries and the per-object update, build a repeatable graphical benchmark, and remove redundant work **only where the result is provably identical**.
Real two-PC validation of `2071ad765` remains pending; nothing here is multiplayer evidence. Disk hygiene (done first, free disk 21.8 -> 70.3 GB) is documented in [DISK_RETENTION.md](DISK_RETENTION.md).

Hardware for every number: Intel Core Ultra 5 125U laptop, Release x64, seeded replays. "1x" = Zero Hour / Generals `twilight4H` (Twilight Flame, 4 brutal AI, Enhanced AI, 50 000 cash).

## 1. Seeded graphical benchmark (`scripts/performance/Invoke-GraphicalBenchmark.ps1`)

The normal (rendering) client plays a generated, seeded AI-vs-AI replay: `-loadreplay <file>` with the shell map off, no input, the replay's start camera, VSync off, and the opt-in `GENERALS_SIM_FINGERPRINT` writing the authoritative state CRC every 30 logic frames.
The same replay is then simulated headless with the same executable; the summary compares both fingerprints line by line and keeps only `summary.json`, `fingerprint.txt` and the headless summary (the raw per-tick / per-frame files are deleted).

**Repeatability evidence.** Two Zero Hour graphical runs have identical fingerprints for all 540 checkpoints (9 game minutes), and every run (6 of 6) equals the headless fingerprint on the whole common prefix (`first_divergence_index = -1`).
The simulation inside the graphical client is therefore exactly the headless simulation, so the headless digests and gates of earlier work apply to it.

Pitfalls found and fixed while building it (all in the tools, not the game): playing the replay through the opt-in autoplay hook starts it *inside the shell map* and leaves ~104 shell objects in the simulation (fingerprints differ from frame 30), so the benchmark uses `-loadreplay`;
with VSync on, an idle or locked display stalls `Present` to ~4 Hz (the first overnight run), so VSync is off; replay playback renders at the engine's 120 Hz cap whatever `-fps` says; the end-of-match screen idles at a very low frame rate, so the runner ends the process when the telemetry writes its match summary.

### Baseline ("before") and final ("after"), 9-minute replay, window = logic frames 8 000-16 200 (8 201 ticks)

| run | mean | p50 | p95 | p99 | p99.9 | worst | ticks > 33.3 ms | render fps | render interval p99 (median window) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| ZH before #1 | 5.235 | 3.855 | 17.289 | 22.017 | 31.344 | 57.9 | 5 | 114.7 | 19.3 |
| ZH before #2 | 5.423 | 3.872 | 18.035 | 23.738 | 32.923 | 69.4 | 8 | 113.7 | 21.0 |
| **ZH after #1** | **4.306** | **3.092** | **15.828** | **18.534** | 25.271 | 55.3 | 3 | 116.7 | 18.5 |
| **ZH after #2** | **4.339** | **3.067** | **15.931** | **18.879** | 31.197 | 74.6 | 8 | 116.7 | 18.5 |
| **ZH after #3** | 4.567 | 3.153 | 16.190 | 21.047 | 41.578 | 78.6 | 16 | 115.7 | |
| **ZH after #4** | 4.473 | 3.162 | 16.224 | 19.912 | 30.004 | 54.8 | 6 | 116.4 | |
| Generals before | 2.374 | 2.108 | 4.321 | 7.231 | 12.465 | 19.3 | 0 | 119.6 | 8.5 |
| **Generals after** | **1.664** | **1.527** | **2.671** | **5.609** | 8.595 | 13.0 | 0 | 119.9 | 8.3 |

* Zero Hour (2 runs before, 4 after): mean tick **-17 %** (5.33 -> 4.42 ms), p50 -19 %, p95 -9 %, **p99 -14 %** (22.9 -> 19.6 ms); run-to-run spread of the p99 is about 1.3 ms, so the two best and the two worst "after" runs bracket the change. Generals (1 run each): mean -30 %, p99 -22 %. The worst tick stays noisy (one ~60-75 ms tick per run is a failing path search) and is not an improvement claim.
* Whole replay (16 200 ticks): ZH mean 3.54 / 3.68 -> 2.99 / 2.95 ms, p99 20.2 / 22.0 -> 17.6 / 17.7 ms.
* Every run's fingerprint equals the headless one; the "after" fingerprints equal the "before" ones (same 544 checkpoints `C595319B827648D9`): the optimisation did not change the simulation.
* Raw summaries: `docs/modernization/evidence/graphical-benchmark/{before,after}/`.

Hitches (render intervals flagged by the frame-delivery telemetry, by dominant phase): `postdraw` = the logic tick: 9 and 14 rows before, **3 and 8 after** (ZH), 0 in Generals; plus in every run exactly **one `draw` hitch of 100-140 ms and one `predraw` hitch of 50-110 ms** (first-use work around match start, ~5 s in).
The extreme outliers (608 ms in the earlier unseeded 6x run) recurred in the seeded runs: `zha2` had a 453 ms frame and `zha4` two (480 ms at t = 2.8 s and 510 ms at t = 274.5 s); no other run exceeded 140 ms. Section 9 attributes them (the stall is inside `Present`).

## 2. AI spatial-query workload (`GENERALS_SIM_GCO`, `PartitionManager::getClosestObjects`)

ZH 1x, whole 13-minute replay (diagnostic run, timer overhead included; digest unchanged):

| | queries | time | cells opened | objects inspected | empty answers |
|---|---:|---:|---:|---:|---|
| before | 884 569 (38.7 / frame) | 8.78 s | 387 M | 85 M | 309 782 = 35 %, **5.33 s = 60.7 % of the time** |
| after (staged hunt scan) | 930 002 | 6.33 s | 322 M | 81 M | 347 830 = 37 %, 5.24 s |
| Generals before | 537 638 (24.3 / frame) | 4.16 s | 304 M | 50 M | 58.9 % of the time |

Callers, ZH before (three frames above the query):

| caller | calls | ms | us/call | cells | objects | empty | identical to the same asker's previous answer |
|---|---:|---:|---:|---:|---:|---:|---:|
| `AIGuardIdleState::update` -> `lookForInnerTarget` | 16 428 | 3 153 | 192 | 8 524 | 1 478 | 92 % | 95 % |
| `AIHuntState::update` -> `findClosestEnemy` (whole map, sorted) | 8 209 | 2 853 | 348 | 14 396 | 1 859 | 0 % | 21 % |
| `getNextMoodTarget` -> `findClosestEnemy` | 154 316 | 1 305 | 8.5 | 387 | 131 | 97.5 % | 97 % |
| `AIGuardReturnState::update` | 3 872 | 586 | 151 | 7 153 | 1 117 | 60 % | 88 % |
| `StealthDetectorUpdate` | 96 796 | 225 | 2.3 | 110 | 44 | 99 % | 99.9 % |

Generals has the same two leaders (guard idle 1.64 s, hunt 1.37 s of 4.16 s). Raw reports: `docs/modernization/evidence/ai-query/gco-*.txt`.

## 3. Idle guard scans: why they cannot be skipped exactly

The guard state scans every `m_guardEnemyScanRate` frames (each guard state instance schedules its own scan; the diagnostic identifies the asker by the guarded position, so units guarding the same point share one row), over the guard radius, with the filters of its owner; 92 % return nothing and 95 % of re-scans return exactly what the last scan returned.
So the avoidable work is real (about 3.7 s of a 66 s run, 5.6 % of the tick), but **no exact skip rule exists**:

* A repeat is valid only if nothing the query depends on changed. The query reads live object positions (distance test), cell membership, and per-object state read by the filters (relationship / team, alive, stealth, shroud, weapon status, line of sight). Between two scans of the same guard 64 % of re-scans see more than 100 object cell updates in the partition (22 % see none, those are the same-frame repeats), and none of the state changes above announces itself to the partition.
  Within one frame the partition cells are frozen (they are updated once at the end of the frame, `PartitionManager::update`), but positions are live, and other objects update in between, so even the 22 % same-frame repeats can legitimately differ.
* A cheaper exact check (a revision counter per partition cell) would need every producer of the state above to bump it (death, team change, stealth, fog, weapon state, movement inside a cell): dozens of sites in code that is not covered by a proof, so the first missed site silently changes the simulation.
* Rejected, with the measurements above as the evidence. The only exact levers left are the constants of the traversal (section 5).

## 4. HUNT scans: the scan -> unreachable target -> failing path chain, and the exact staged scan

**The chain exists and is quantified** (ZH 1x, `GENERALS_SIM_PATHFAIL`, 1 435 expensive failing searches, 21.9 s): 99 % of the failing time is requested from the HUNT state (1 426 failures, 21.7 s); every one is a scripted team (`cmd = SCRIPT`, "Rush" teams = 91 %).
Hunters re-scan every `ENEMY_SCAN_RATE` = 30 frames over the whole map (`range 9999.9`), pick the same enclosed target again, and fail the same search again: **886 of 1 426 failures (62 %, 13.1 s) repeat a (hunter, goal) pair that already failed**; the most failed goal cell failed 179 times; 28 % of the failures are cut short by the shared info pool (>= 20 000 cells).
This is AI behaviour, not redundant computation inside the engine (each re-request is a new request in a world that may have changed), and the strategy was not touched. The earlier pathfinding milestone showed why the failures themselves cannot be cached (their cost and budget charge depend on the exact pool state).

**What can be removed exactly: the scan.** `AI::findClosestEnemy` with attack info collects *every* enemy on the map, sorts near to far, then picks the best by `priority - floor(distance / modifier)` (ties: first in sorted order). The modifier is 100 world units per priority step, the table maximum is 30 (some scripts use 150 / 5000).
A candidate at distance >= R can reach at most `max(1, maxPriority - floor(R / modifier))`; it can replace the current best only with a modified priority >= the current one. So:

1. scan only the candidates inside R (3.5, 7.5, then 14.5 modifier steps; `R + margin` is queried, margin = asker radius + 3 cells so the ring cutoff of `getClosestObjects` cannot hide a candidate), run **the same selection loop** (now a shared function) on the sorted prefix (the smaller query's list is a prefix of the full list: same cells in the same order, stable merge sort that keeps ties in insertion order);
2. if the best modified priority is larger than that bound, no farther candidate can change the answer: return it; otherwise try the next R, finally the original full scan.

R sits half a step above a step boundary, so rounding can never matter. Every filter used here is a `const` query (relationship, line of sight, fog, can-attack, stealth), so evaluating them twice has no side effect.

**Evidence.** `GENERALS_HUNT_VERIFY=1` runs the staged and the full scan for every call and exits with code 3 on the first difference: **24 713 staged answers verified, 0 differences** (ZH 1x 8 062, ZH 4x units 9 290, Generals 7 361); simulation digests unchanged (`1E5982E5B3A1B86B`, `CF9C9DB782B65644`, Generals `1014EE85E384C19C`).
98 % of the wide hunt scans end in a bounded stage (stage 0: 15 %, stage 1: 37 %, stage 2: 47 %; 190 scans fall back to the full scan). Hunt-scan query time 2.85 s -> 0.57 s; mean tick (headless, alternating x 4) **2.533 -> 2.455 ms (-3.1 %)**, p50 1.54 -> 1.46 ms; p95 / p99 unchanged (the tail is path search).
`GENERALS_HUNT_STAGED=0` restores the single scan for A/B.

## 5. Partition traversal

The traversal is memory-latency bound (the sampler puts half of `getClosestObjects` on the walk of the object-in-cell linked lists; 57 % of the records visited are skips of objects already seen because large objects sit in many cells). A prefetch of the next record was measured in the previous milestone (no gain) and was not repeated.
The one structural change that would help (contiguous per-cell arrays) must reproduce the list order (it decides ties) and touches the central data structure of every query; it was not attempted and is the first candidate if query cost becomes the limit again.

## 6. Where the per-object update time goes (`GENERALS_SIM_UPDPROF`, ZH 1x, 31.7 s inclusive over 21.8 M update calls)

| module | share | calls | us/call |
|---|---:|---:|---:|
| `AIUpdateInterface` (all AI) | 47.9 % | 1.68 M | 9.0 |
| `WorkerAIUpdate` / `JetAIUpdate` / `SupplyTruckAIUpdate` / `DozerAIUpdate` | 6.6 / 5.8 / 3.7 / 2.4 % | | 5.2 / 9.5 / 6.1 / 2.7 |
| `ObjectWeaponStatusHelper` | 4.9 % | 3.93 M | 0.40 |
| `PhysicsBehavior` | 4.8 % | 2.04 M | 0.74 |
| `ProductionUpdate` | 4.0 % | 2.89 M | 0.44 |
| `StealthUpdate` | 3.6 % | 4.05 M | 0.28 |
| `StealthDetectorUpdate` | 3.0 % | 152 k | 6.3 |

Inside `AIUpdateInterface::update` (20.3 s): state machine 16.1 s, locomotor 2.2 s, turret AI 1.7 s. State machine by AI state: **GUARD 4.68 s, HUNT 4.11 s, IDLE 2.71 s**, DOCK 1.19 s, ATTACK_MOVE_TO 1.16 s, ATTACKFOLLOW_TEAM 0.79 s, ATTACK_OBJECT 0.56 s, MOVE_TO 0.53 s.
So guard + hunt = 55 % of the state-machine time and most of it is the two queries of sections 3 and 4. Generals: guard 3.24 s, hunt 2.13 s of 7.1 s.

Redundant-work candidates examined and **not** taken: `ProductionUpdate` and `ObjectWeaponStatusHelper` run every frame for objects that are idle (the source even carries "use SLEEPY_UPDATE here"), but letting them sleep changes the shape of the sleepy-update heap, whose tie order between equal-key modules decides the order in which objects update;
that order feeds the logic RNG, so any change there is a simulation change unless proven otherwise. `Drawable::updateDrawableClipStatus` (per weapon slot per frame) is client-side state, but skipping it when the ammo is unchanged is not safe across model swaps. The critical sections around the pool allocator (3.5 % of samples) are shared with other threads.

## 7. Why Zero Hour costs about 3.6x Generals (same scenario, 23 400 ticks)

Tick time 59.3 s vs 16.4 s; the phase gap (42.9 s) decomposes as:

| phase | ZH ms | Generals ms | share of the gap |
|---|---:|---:|---:|
| `path_search` | 21 207 | 1 094 | **46.9 %** (82 860 vs 25 158 searches; 0.256 vs 0.044 ms per search) |
| `ai_object_base_update` | 13 447 | 6 210 | 16.9 % (3.15 M vs 1.75 M AI updates) |
| `object_update_loops` | 8 586 | 2 746 | 13.6 % (21.8 M vs 10.3 M module calls) |
| `path_request_entries` + `path_reconstruction` | 5 115 | 1 205 | 9.1 % |
| `logic_partition` | 3 053 | 1 083 | 4.6 % |
| `ai_player_strategy` + `ai_global` | 2 767 | 877 | 4.4 % |
| scripts, weapons, locomotion, destroy | | | 4.4 % |

So 56 % of the gap is pathfinding (the HUNT-failure chain of section 4: 1 426 vs 13 failing searches), 17 % is AI update, 14 % is object update. The rest of the gap is volume: Zero Hour has ~1.5x the objects and ~2.1x the update-module calls (stealth, weapon-status, production, physics and supply modules on more object types),
and ~1.65x the spatial queries (884 569 vs 537 638); the per-unit scaling shape is the same (X64.sc).

## 8. Why a graphical logic tick costs more than the headless tick for the same simulation

The seeded benchmark makes the comparison exact (same simulation, same frames 8 000+, before the optimisation): ZH mean 5.24 vs 3.05 ms (+72 %), p99 22.0 vs 16.9 ms; Generals 2.37 vs 0.88 ms (+170 %). By phase (ms per tick):

| phase | ZH graphical | ZH headless | ratio | Generals ratio |
|---|---:|---:|---:|---:|
| `ai_object_base_update` | 1.632 | 0.718 | 2.27 | 2.98 |
| `object_update_loops` | 0.952 | 0.449 | 2.12 | 2.63 |
| `logic_scripts` | 0.281 | 0.100 | 2.81 | 3.78 |
| `logic_partition` | 0.233 | 0.122 | 1.91 | 2.28 |
| `locomotor_move` | 0.113 | 0.050 | 2.27 | 2.85 |
| `path_search` | 1.224 | 1.085 | **1.13** | |

The inflation sits in the per-object phases (2-4x) and almost not at all in compute-bound path search (1.13x), and the fingerprints prove no extra simulation work. Two mechanisms fit this and were not separated further: (1) in the graphical client the logic calls into real client objects (drawables, draw-module and model-condition updates, particle / effect / audio requests, shroud) that are
no-ops in headless; (2) rendering the ~120 frames per second on the same thread evicts the simulation's working set (object data, partition cells) between ticks, which hurts pointer-chasing phases most and compute loops least. The optimisation's larger graphical gain (-19 % vs -3 % headless) is consistent with (2): the removed scan touched ~1 860 objects across the map, which in the graphical process were cold.
Moving the simulation to another thread is out of scope and not suggested by this data; the practical lever is the number of objects the tick touches.

## 9. Extreme render outliers

**Two classes, now separated by the retained `top_render_hitches` record** (time, phase split, `present_ms`, texture / buffer creations, draws, page faults, working set; in every `summary.json`):

* **First-use creation (~100-130 ms, every run, 2-4 s after start):** a `draw` / `predraw` frame with `tex_created` 104, `buf_created` 146, ~36 000 page faults and the working set growing from ~330 to ~500 MB. Expected one-time cost; tracked, not worked.
* **`Present` stalls (450-510 ms, 3 of 8 seeded runs):** the whole interval is `present_ms` (`zha4`: interval 509.6 ms, `present_ms` 493.5; interval 479.6 ms, `present_ms` 479.3), with no texture / buffer creation, 54-132 page faults and a normal tick (0-14 ms). The CPU side of the frame is ordinary; the swap-chain `Present` blocked for about half a second, once at match start (t = 2.8 s) and once mid-match (t = 274.5 s). That points at the presentation path (display / compositor / GPU queue) rather than at the game's draw code or asset loading, and it occurs with VSync off; the earlier 608 ms frame is the same class. The cause below `Present` (DWM, driver, display power state) was not determined and needs an ETW trace on a native session; if it matters, capture one with the next `top_render_hitches` that shows `present_ms` > 400.

## 10. Congestion pathfinding (6x vs 8x mobile units, `GENERALS_SIM_PATHFAIL`)

| | 6x units | 8x units |
|---|---:|---:|
| expensive failing searches | 1 108 | 3 034 (x2.7) |
| failing time | 13.1 s | 26.3 s |
| requested from HUNT | 94 % | **66 %** (ATTACKFOLLOW team paths 17 %, ATTACK_MOVE_TO 9 %, ATTACK_OBJECT 8 %) |
| distinct (requester, goal) pairs | 686 | 2 087 (x3.0) |
| repeats of an earlier failing signature | 28 % | 21 % |
| failures with blocking units on the frontier | 47 % | **61 %** (15.0 s) |
| failures at the info-pool limit (>= 20 000 cells) | 398 | 398 (13 %) |

The jump is **more distinct units failing, from combat-movement states, against crowds of their own units** (blockers on the frontier), not repeated requests and not more pool exhaustion. Each failure is a different request, so no new exact redundant-work opportunity exists; the pathfinder was not changed.

## 11. Upstream awareness (`upstream/main`, 183 commits since the divergence)

Commits touching the files in scope: `bfc752661` (Zero Hour object lookup moves from the direct-index vector to a hash map, the Generals way) - **rejected**, it replaces the faster structure; `2119cc9b6` (do not exit a temporary AI state already reset during its own update) - **deferred**, a behaviour fix that can change replays;
`b9fb894bf` (route engine math through WWMath) - **rejected** for determinism (different floating-point sequences); `8ed77a761` crate pickup, `2886271c5` / `090934812` tunnel network, `407248bfd` `IABS` consolidation - not performance relevant. Nothing upstream changes the partition query, the guard / hunt scans or the update loop.

## 12. Multithreading reassessment

No change to the recommendation. The only large read-only workloads found are the guard / hunt queries (about 10 % of the tick before this work, 6 % after), but their results are consumed immediately inside the order-dependent state machine; running them ahead or in parallel would change *when* each unit sees the world.
Path search remains the one candidate with an independent-read structure (shared info pool, ordered publication needed). The graphical inflation (section 8) is a memory / client-coupling effect that threading the simulation would not remove.

## 13. Remaining bottlenecks (new measurements)

1. Path search under congestion and the HUNT failure chain: 47 % of the Zero Hour - Generals gap, the entire > 33 ms tail.
2. Idle guard scans: 3.7 s of 66 s (5.6 %), 92 % empty, no exact skip.
3. The per-object update of the AI state machines (guard / idle / hunt) and the sleepy-update heap (`rebalanceChildSleepyUpdate` 2.4 % of samples).
4. Graphical-mode inflation of the per-object phases (2-4x): client coupling and cache pressure.
5. Allocator critical sections (3.5 % of samples); first-use render hitches (~100-130 ms once per run); `Present` stalls of 450-510 ms (3 of 8 seeded runs, inside the swap-chain call, cause not determined).

## 14. Headless before / after (Zero Hour 1x, whole replay, 23 400 ticks; `GENERALS_HUNT_STAGED=0` vs default, alternating x 2, digest `1E5982E5B3A1B86B` in every run)

| | mean | p50 | p95 | p99 | p99.9 | worst | > 33.3 ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| before (staged scan off) | 2.529 / 2.538 | 1.543 / 1.530 | 13.59 / 13.83 | 16.66 / 16.70 | 23.33 / 23.47 | 54.9 / 62.0 | 6 / 6 |
| after (default) | **2.453 / 2.456** | **1.461 / 1.464** | 13.82 / 13.80 | 16.70 / 16.59 | 24.60 / 23.56 | 57.2 / 67.3 | 5 / 6 |

Per phase (the optimisation touches `ai_object_base_update`): the gain is a lower median and mean; the tail (p95 and above) is path search and does not move. The first version of the staged scan, which computed the table maximum on every scan, gained only 1.5 % (that call alone was 0.9 % of the samples); computing it lazily gave 3.1 %.
Generals: 7 361 hunt scans were verified against the full scan with zero differences; no separate headless A/B was run for Generals (its measured effect is the graphical one in section 1: mean tick -30 %).

## 15. Scalability: did the knee move? (Zero Hour, frames 15 000+, headless, units multiplied by `GENERALS_SIM_CLONE`)

| mobile-unit factor | objects | before: mean / p99 / ticks > 33.3 ms | after: mean / p99 / ticks > 33.3 ms |
|---|---:|---|---|
| 6x | 2 219 | 8.76 / 23.7 / 11 | 8.66 / 23.9 / 12 |
| 8x | 2 705 | 15.49 / 32.2 / 61 (paired run with the scan off: 15.81 / 33.2 / 80) | **14.82 / 31.6 / 46** |
| 10x | 3 191 | 18.55 / 44.0 / 354 | **16.81 / 34.0 / 106** |

Up to ~2 200 objects nothing changes (the load is not hunt-bound); beyond it the hunt scans are a growing share and the knee moves up: 8x units now sits at p99 31.6 ms (was 32.2 / 33.2), and 10x units drops from "cannot sustain 30 TPS" (4 % of ticks over budget) to "little headroom" (p99 34 ms, 1.3 % over).
The sustainable ceiling is therefore about **2 700-2 900 objects** instead of 2 200-2 700 on this CPU. The simulation digests of all rungs are unchanged (`0771FEC67A54AD51`, `2DAA9B5CCD2C13E2`, `2FE2E19AE1BDD237`).

## 16. Determinism and regression (final build)

* Zero Hour 1x digest `1E5982E5B3A1B86B` unchanged in every run with the optimisation on and off; 4x units `CF9C9DB782B65644`; Generals `1014EE85E384C19C`.
* X64.pf guard digests unchanged: `twilight4H` line decision `6AC45762C1656E3D`, open list `5B85CEF9CC94BCB0`; `city5H` fingerprint `FB748B205FB71351`, line `B414DB6B28633F5A`, open list `30F676D2A410CAE1`.
* `GENERALS_HUNT_VERIFY`: 24 713 staged answers compared with the full scan, 0 differences.
* Golden AI gates (`Invoke-AiBehaviorGate.ps1 -Quick`): Zero Hour PASS, Generals PASS (Original digests equal the X64.51 baseline executables, every run repeatable).
* Google Tests: Generals 557, Zero Hour 556 passed.
* Graphical: 8 seeded runs, every fingerprint equal to the headless one over the whole replay; the "after" fingerprints equal the "before" ones.
* Python suites: see the commit; new tests `test_asset_store.py`, `test_diag_cleanup.py`, `test_graphical_benchmark.py`.
* Not run: real two-PC multiplayer (`2071ad765` validation remains pending). The staged scan changes no network-visible behaviour (it is deterministic and verified), but this is not multiplayer evidence.
