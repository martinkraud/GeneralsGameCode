# Engine Scalability & Massive Battle Stress Audit (X64.sc)

Starting point: `726d6b266` (X64.pf). Scope: measure how far the modernized Generals / Zero Hour engine scales beyond retail workloads and which subsystem limits it first. One accepted code change
(an upstream bug-fix port), the rest is measurement tooling and documentation. No gameplay, determinism or pathfinding semantics were changed. **Real two-PC multiplayer validation of `2071ad765`
remains pending**; nothing here is multiplayer evidence.

All numbers: Intel Core Ultra 5 125U laptop (14 logical processors, iGPU), Release x64, headless seeded AI-vs-AI replays unless marked "graphical".

## 1. What was built

| Tool | Purpose |
|---|---|
| `GENERALS_SIM_CLONE=<k>@<frame>` (`ScalabilityMultiplier`, called from `GameLogic::update`) | Diagnostic unit multiplier. At the given logic frame every mobile, non-aircraft, non-structure unit of a computer player gets `k` extra copies (same type, same team) on legal cells in a fixed spiral. Inert unless set; same setting + same replay = same simulation (digests reproduce, section 3). Works headless and in the graphical game. Never use in multiplayer. |
| `GENERALS_SIM_GCO=<file>` (`SpatialQueryDiag`) | Read-only characterisation of `PartitionManager::getClosestObjects`: calls, time, cells opened, objects inspected, distance/filter pass counts, repeats within a frame, by radius / world size / three-frame caller. |
| `scripts/performance/Invoke-ScalingLadder.ps1`, `Summarize-TickWindow.py` | AI-count / cash ladder and per-window tick statistics (only ticks after the multiplication are compared). |
| Bench report: `process memory at end` (working set, peak, private bytes, handles) | Memory/handle accounting for headless runs. |
| `SimSampler::describeAddress` | Symbolisation helper for the query characterisation. |

Workload definition used below: **1x** = the normal heavy battle of the `twilight4H` scenario (Twilight Flame, 4 brutal AI, Enhanced AI, 50 000 cash, 13 game minutes; ~1000 objects, ~240 mobile
computer units at frame 15 000). **k** = `k` extra copies of every mobile computer unit at frame 15 000 (units x(1+k)). Windows are frames 15 000-23 400 (8 401 ticks, 4.7 game minutes).

## 2. Why a ladder by AI count / cash does not work

AI-vs-AI armies saturate: 1x = 4 AI 50k cash (peak 1 149 objects); 2x = 7 AI 50k (1 555); 3x = 7 AI 150k (1 593); 4x = 7 AI 400k (1 958). Tick mean 2.5 -> 3.9 -> 4.2 -> 4.2 ms, p99 16.8 -> 15.7 -> 17.3 -> 16.3 ms.
Money does not become units (production time and the AI's own caps limit the army), so honest 2x-4x unit loads need the multiplier above. (`Invoke-ScalingLadder.ps1`, `build/diag/ladder-zh`.)

## 3. Scalability ladder (headless, simulation tick only)

Zero Hour, `twilight4H`, window = frames 15 000-23 400:

| Rung | mobile-unit factor | objects after injection | mean ms | p50 | p95 | **p99** | worst | ticks > 33.3 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1x | 1 | ~1 004 | 3.87 | 2.41 | 14.5 | **18.8** | 56.8 | 4 |
| k1 | 2 | 1 247 | 4.62 | 3.38 | 14.7 | **17.3** | 41.5 | 3 |
| k2 | 3 | 1 490 | 5.45 | 4.35 | 15.4 | **19.4** | 51.6 | 5 |
| k3 | 4 | 1 733 | 7.57 | 6.21 | 17.4 | **22.2** | 67.7 | 6 |
| k5 | 6 | 2 219 | 8.76 | 7.63 | 17.9 | **23.7** | 80.1 | 11 |
| k7 | 8 | 2 705 | 15.49 | 14.26 | 27.9 | **32.2** | 92.2 | 61 |
| k9 | 10 | 3 191 | 18.55 | 17.82 | 32.5 | **44.0** | 115.1 | 354 |

Generals, same scenario family, same window:

| Rung | mobile-unit factor | objects | mean ms | p50 | p95 | **p99** | worst | > 33.3 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1x | 1 | ~730 | 1.00 | 0.79 | 1.9 | **4.9** | 37.5 | 3 |
| k1 | 2 | 838 | 1.67 | 1.36 | 3.9 | **6.8** | 10.4 | 0 |
| k3 | 4 | 1 052 | 2.99 | 2.39 | 7.5 | **13.1** | 23.0 | 0 |
| k5 | 6 | 1 266 | 3.59 | 3.04 | 7.9 | **10.6** | 23.6 | 0 |
| k9 | 10 | 1 694 | 6.47 | 5.60 | 13.0 | **15.8** | 43.2 | 2 |

Determinism of the ladder: `k3` reproduces digest `CF9C9DB782B65644` in every run, before and after moving the multiplier into the game loop (so the move is exact); every rung is repeatable.

**Reading with the requested interpretation** (< 10 ms excellent, 10-20 healthy, 20-30 acceptable, 30-33.3 little headroom, > 33.3 cannot sustain):

* Zero Hour: healthy to 4x mobile units (p99 22 ms = acceptable), acceptable at 6x (p99 23.7 ms, ~2.2x the objects of the heavy battle), **little headroom at 8x (p99 32 ms)** and **cannot sustain 30 TPS at 10x (p99 44 ms, 4 % of ticks over budget)**.
  The knee is between 2 200 and 2 700 objects (about 1 500-1 900 mobile units) on this laptop CPU.
* Generals: p99 stays under 16 ms even at 10x units (1 700 objects); the knee is not reached in the ladder.
* Retail workload (the normal heavy battle, ~1 000 objects): 1x row; plausible heavy custom battle (2-3x objects): k3-k5 rows; extreme diagnostic: k7-k9.

## 4. Where the time goes, and how it scales (Zero Hour, ms per tick, same window)

| Phase | 1x | k3 | k5 | k7 | k9 | growth 1x -> k9 | behaviour |
|---|---:|---:|---:|---:|---:|---:|---|
| ai_object_base_update | 0.96 | 2.13 | 2.77 | 4.48 | 5.91 | x6.2 | ~O(units) (includes the target queries) |
| object_update_loops | 0.61 | 1.52 | 2.13 | 3.14 | 4.09 | x6.7 | ~O(units) |
| logic_partition | 0.25 | 0.78 | 1.08 | 1.64 | 2.30 | x9.1 | ~O(moving units) |
| path_search | 1.19 | 1.42 | 0.83 | 2.79 | 2.11 | noisy | driven by congestion / failures, not unit count (k5 -> k7 jump) |
| path_request_entries | 0.24 | 0.51 | 0.49 | 1.23 | 1.26 | x5.2 | ~O(path requests) |
| logic_destroy | 0.02 | 0.11 | 0.19 | 0.37 | 0.59 | x24 | the only super-linear phase, but only 3 % of the tick |
| weapon_fire / locomotor_move | 0.07 / 0.06 | 0.19 / 0.16 | 0.20 / 0.20 | 0.28 / 0.33 | 0.37 / 0.41 | x5-7 | ~O(units) |
| ai_player_strategy | 0.12 | 0.14 | 0.17 | 0.19 | 0.19 | x1.5 | O(1) in units (per AI player) |
| TOTAL | 3.87 | 7.57 | 8.76 | 15.49 | 18.55 | x4.8 | |

Nothing is quadratic in the unit count. The collapse is a linear sum of per-unit costs plus
the pathfinding tail that appears when units block each other. The first bottleneck is therefore **the per-object update cost of the AI and object update loops (35 % and 22 % of the tick at k9)**, then
partition updates (12 %), then path searching under congestion (11-39 % depending on the window).

Memory (headless, end of run): k3 553 MB working set / 525 MB private / 506 open handles; k9 675 MB / 640 MB / 510 handles (handles do not grow with the army; the 1x run was not instrumented for memory).

## 5. `PartitionManager::getClosestObjects` (not optimised)

`GENERALS_SIM_GCO` on the 1x scenario (the instrumented run is about 4 % slower; digest unchanged):

* 884 569 calls (38.7 per simulation frame), 7.65 s of a ~66 s run (about 12 % of the tick time including timer overhead; the sampler saw 8.6 %). At k3: 1 172 651 calls (51 per frame), 12.6 s.
* **68 % of the query time is two callers**: `AIGuardMachine::lookForInnerTarget` (idle guards scanning for an enemy: 16 428 calls, 167 us each, **8 500 cells opened and ~1 480 objects inspected per call to find nothing**: filter pass rate 0.08 per call) and
  `AI::findClosestEnemy` from `AIHuntState::update` (8 209 calls, 303 us each, 14 400 cells, 1 860 objects, 193 pass the filters; the priority rule needs *all* of them, sorted).
* A third caller, `AIUpdateInterface::getNextMoodTarget` (154 316 calls at 7 us) is cheap per call but frequent (15 % of the query time).
* 76 % of the time is in the 28 988 queries with a search radius between 800 and 1 600 world units: 201 us per call versus 0.3-10 us for everything smaller.
* Repeated identical queries in the same frame (same asker / radius / filter types; an upper bound because the world moves during the frame): 22 % for the guard scan, 14.5 % overall for wide queries, 2 % for the whole.
* Scaling: per-call cost of wide queries grows ~1.5x when the unit count quadruples (objects inspected per query 1 480 -> 2 570 with local density); calls grow ~1.3x; total query time x1.65 for x2.2 objects. Roughly linear in the world size, not quadratic.
* The cost is **memory latency**: the profiler puts ~50 % of the function on the loop that walks the linked list of object-in-cell records (`getNextCoi`) and the pointer chases behind it; 57 % of the records visited are skips of objects already seen
  (large objects are registered in many cells). A prefetch of the next record was tried as an exact experiment and measured **no gain** (windowed means 7.75 / 7.44 ms before, 7.38 / 7.42 ms after; digest identical), and was reverted.

**Decision: no optimisation.** The only large wins avoid the query rather than speed it up (skip a guard scan when nothing near it changed; reorder filters ahead of the distance test), and each needs either a proven invalidation model
(object moved / died / changed team / stealth / fog) or proof that every filter is pure, which this milestone did not establish. The candidate list is in section 11. Candidate ordering, tie-breaking and the selected target are untouched.

## 6. Graphical Zero Hour endurance (Skirmish autoflow on the disposable `mp-audit-zh` stage, 60 Hz, `726d6b266` descendant)

| | baseline run | 6x units (`GENERALS_SIM_CLONE=5@8000`) |
|---|---|---|
| ticks / game time | 22 168 / 12.3 min | 17 999 / 10.0 min |
| objects / mobile units (last window) | 720 / 340 | 1 432 / 1 082 |
| tick mean / median | 3.52 / 2.60 ms | 7.90 / 7.81 ms |
| **tick p95 / p99 / p99.9 / worst** | 9.8 / **25.5** / 48.5 / **89.7** ms | 22.5 / **41.6** / 52.7 / 87.3 ms |
| ticks > 33.3 / 50 / 66.7 / 100 ms | 129 (0.58 %) / 21 / 7 / 0 | 416 (2.3 %) / 38 / 2 / 0 |
| model missed deadlines / max backlog / lag at end | 151 / 72 ms / 5.1 s | 534 / 323 ms / 10.3 s |
| render interval median p50 / p95 / p99 (per-window medians) | 16.2 / 20.5 / 25.0 ms | 15.9 / 26.8 / 31.5 ms |
| render fps mean (min window) | 59.9 | 58.9 (28.7) |
| hitch rows dominated by the logic tick (`postdraw`) | 219 of 229 | 536 of 549 |
| working set / private (start -> end) | 665 -> 800 MB / 630 -> 793 MB | 668 -> 779 MB / 635 -> 744 MB |
| handles | 775 -> 775 | 747 -> 776 |

* **Comparison with the pre-X64.pf run** (804 s: p99 36.4, p99.9 62.2, worst 194.7 ms, 1.25 % over 33.3 ms) is **not an A/B**: the autoflow match is not seeded (different AI generals, a different match length, 12.3 vs 13.4 min).
  The baseline run is the same class of match and shows p99 25.5 ms, worst 89.7 ms, 0.58 % of ticks over budget. That is consistent with, but does not prove, a better tail; the 17 % path-search reduction explains at most a part of it
  (path search is 39 % of the last window), the worst tick of the earlier run (195 ms) did not recur. A paired measurement would need a seeded graphical match, which the project does not have.
* Slowest ticks of the baseline run (up to 89.7 ms) coincide with 13-34 failing searches in the same tick, as in the earlier audit.
* **Rendering is not the limiter**: in both runs the render interval holds 60 fps, hitches are tick-dominated, draw time in the hitch frames of the multiplied run has a median of 8.3 ms with ~430 draws (median draw count of the baseline hitch rows: 525). The graphical tick is also heavier than the headless one
  for the same object count (k5 headless p99 23.7 ms at 2 200 objects, graphical p99 41.6 ms at 1 430 objects and 1 082 units), because the logic runs on the render thread, interleaved with drawing, with cold caches.
  The largest single render outlier was one 608 ms draw frame in the multiplied run (cause not investigated) - tracked, not worked.

## 7. Secondary hitches (tracked only)

First-use asset hitches (predraw 8 of 229 hitch rows in the baseline, 7 of 549 in the multiplied run; the largest 122-156 ms) and the terrain full update remain, and are an order of magnitude rarer than tick-dominated hitches. They matter less
than the tick tail at every load measured here.

## 8. Multithreading decision

Question: *would this engine materially benefit from multithreading for realistic large battles?* **No.** At the heavy retail-class workload the headless p99 is 18.8 ms (Zero Hour) / 4.9 ms (Generals); at 2-3x the objects it is 22-24 / 10-13 ms.
The costs that grow with the army (AI update 35 %, object update 22 %, partition 12 % at k9) are state-mutating and order-dependent (class C). The one workload with an independent-read structure, path search, is 11-39 % of the tick and is
dominated by congestion failures, not by army size.

| Candidate | measured share (k9 / graphical tail) | scaling | benefit if parallel | determinism risk | complexity | class | recommendation |
|---|---|---|---|---|---|---|---|
| Object / AI update loops | 55 % at k9 | O(units) | large in theory | total (RNG, object creation, command side effects, ordered state) | prohibitive | **C** | keep serial |
| Partition update | 12 % | O(moving units) | moderate | high (shared cell lists, registration order) | high | **C** | keep serial |
| Spatial queries (`getClosestObjects`) | ~12 % of the tick | O(world) per call | moderate | queries run interleaved with state changes in update order; batching them changes what each sees | high | **C/B** | do not thread; reduce calls first (section 11) |
| A\* path search | 11-39 % | congestion | the only tail workload | high: shared info pool, per-frame budget, request order, replay/save state | high | **B** | only after serial work is exhausted; design in `X64_38` section 10 and `PATHFINDING_PERFORMANCE_AUDIT.md` |
| First-use asset decode | tail only, 2-3 % of hitches | none | removes rare 30-150 ms hitches | none for the simulation, asset managers not thread-safe | medium-high | **A** | candidate, but low priority by the numbers |
| Telemetry / logging | negligible | -- | -- | -- | -- | A | already off the hot path |

**Which subsystem first, if ever?** Path search (class B), with ordered publication in request order and per-worker scratch state, and only if graphical tick tails driven by congestion remain unacceptable. **At what workload would it become justified?**
Beyond ~2 500 objects / ~1 500 mobile units on this CPU class the sum of the class-C phases, not pathfinding, is what breaks 30 TPS, and threading path search would not cure that. Threading would therefore only be justified together with
a redesign of the object update (data-oriented, order-independent phases), which is not an incremental change. A bounded job system design (bounded workers, index-addressed results, serial commit, serial fallback, cancellation at shutdown, queue-depth and
per-job profiling) is documented in `X64_38`; no implementation is warranted by these measurements.

## 9. Generals vs Zero Hour

* Same engine, different cost: at 1x Zero Hour's tick is 3.9x Generals' (3.87 vs 1.00 ms) and its p99 3.8x; at the same objects (~1 700) Zero Hour's p99 is 22 ms and Generals' 16 ms at 10x units.
* The path-search difference of X64.pf persists: Zero Hour issues ~4x the searches and ~110x the failing searches (HUNT-state scripted teams), and the congestion jump between k5 and k7 appears only there.
* The per-unit costs (AI update, object update, partition) are of similar *shape* in both titles; Zero Hour's constants are higher (more modules per unit, more AI states, more scripted behaviour).
* Graphical measurements exist for Zero Hour only in this milestone.

## 10. Upstream pathfinding audit (TheSuperHackers `upstream/main`)

| Commit | Intent | Bug in our branch? | Determinism | Decision |
|---|---|---|---|---|
| `28bb4554f` bugfix(aiupdate): restore pending path requests after save load (#3491) | The pathfind queue is not saved; units with `m_waitingForPath && m_queueForPathFrame == 0` never get their path after a load | **Yes** (no `queueForPath` in `loadPostProcess`) | affects only the load path, not replays or multiplayer | **Ported** (both titles, `git apply`, provenance in the commit message) |
| `1abf73d5d` bugfix(pathfinder): stop following a destroyed path after moving allies (#3332) | Skip ordering a unit whose own `moveAllies` is still walking its path | Not applicable: the change is entirely inside `#if !RETAIL_COMPATIBLE_PATHFINDING`, which is 1 in every build of this branch (default in `GameDefines.h`); it also adds a member to `Pathfinder` (header guard) | would change pathfinding results outside retail mode | **Deferred** (reconsider if a non-retail-compatible mode is ever shipped) |
| `21323414f` bugfix(pathfinder): restore retail compatibility after PATHFIND_CELL_SIZE_F became a variable (#3519) | Keeps `PATHFIND_CELL_SIZE_F` a macro under `RETAIL_COMPATIBLE_CRC` | **No**: this branch still defines it as a macro in `AIPathfind.h` (the refactor to `constexpr` is not here) | none | **Rejected** (nothing to fix) |
| `8ed77a761` bugfix(aiupdate): `checkForCrateToPickup()` never returned a crate (#3481) | Order of clearing `m_crateCreated` | The bug exists, but upstream keeps it under `RETAIL_COMPATIBLE_CRC` (=1 here), so the port would be inert; making it effective changes AI gameplay (units start picking up crates) | gameplay change | **Deferred** (not pathfinding; would need an owner decision) |
| `a4ac54b83` bugfix(object): avoid a dangling contain module when the Demolition Bike dies landing on a Reinforcement Pad (#3542) | Validity check of `m_containedBy` in `Object::onDestroy` under retail compatibility | Cannot be applied: the patch edits the Generals-only tunnel workaround of an earlier upstream commit that this branch does not have | none at the intended site | **Deferred** (needs the prerequisite commit; crash-class fix, port both together) |

## 11. Remaining bottlenecks, ranked by measured importance

1. Per-unit AI and object update (`ai_object_base_update`, `object_update_loops`): 55 % of the tick at k9; scales linearly; the limit of the engine at ~2 500 objects. Inside it, `getClosestObjects` (~12 % of the tick) is the largest identifiable item.
2. Path searching under congestion (Zero Hour only): 11-39 % of the tick and the main source of the > 33 ms outliers in the graphical run.
3. Partition update (12 % at k9).
4. Graphical-run logic cost on the render thread (tick mean 2.7x the headless one at the same load); render itself is not a limiter.
5. First-use asset and terrain hitches (rare).

Candidate work for a later milestone, not started: (a) avoid guard / hunt scans with a partition change-generation invalidation model (exact only if proven), (b) a filter-order / pure-filter audit so cheap rejections precede distance and LOS work,
(c) a seeded graphical benchmark so graphical A/B comparisons become possible.

## 12. Determinism and regression (final build, all new code present)

* Zero Hour `twilight4H` 1x digest `1E5982E5B3A1B86B` unchanged (new diagnostics and the multiplier are inert without their environment variables); `k3` digest `CF9C9DB782B65644` reproduced in four runs.
* Golden AI gates (`Invoke-AiBehaviorGate.ps1 -Quick`, Original digests against the X64.51 baseline executables): **Zero Hour PASS**, **Generals PASS**, every run repeatable; Enhanced-mode digests equal the previous gate run.
* Google Tests: Generals 557 passed, Zero Hour 556 passed.
* Python suites: performance 49, native-runtime 329 (7 skipped), rendering 30, diagnostics 5 - all OK.
* Not run: real two-PC multiplayer (`2071ad765` validation remains pending); the unit multiplier must never be used in multiplayer.

## 13. Guard digests of X64.pf (for the next agent)

`twilight4H` (Zero Hour, Enhanced AI, 50 000 cash): fingerprint `1E5982E5B3A1B86B`, line-decision digest `6AC45762C1656E3D` (`GENERALS_LINE_TRACE_HASH`), open-list evolution digest `5B85CEF9CC94BCB0` (`GENERALS_PATH_TRACE_HASH`).
`city5H`: fingerprint `FB748B205FB71351`, line digest `B414DB6B28633F5A`. A ground-layer `getCell` bypass that changed the line digest (25 extra "movement" stops) was rejected; negative-result caches were proven infeasible
(0 replayable hits: replaying a failed search exactly needs the same free-pool count and unit state). Parallelising the current pathfinder is not justified (shared info pool, open/closed state in cells, per-frame allocation budget, request order, save/replay state).
