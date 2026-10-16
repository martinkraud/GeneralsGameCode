# X64.38 CPU scalability / multithreading foundation (CP12)

Status: **foundation delivered; no parallel code shipped.** This batch builds the measurement, the determinism oracle and the policy
pieces, audits the simulation from source, and stops with an evidence-based recommendation. The serial simulation is unchanged and remains
the reference; nothing here changes gameplay, command order, RNG, AI, pathfinding, saves, replays or multiplayer.

The goal is **not** "make Generals multithreaded". It is a stable authoritative 30 TPS (33.333 ms per tick), lower p95 / p99 / worst
simulation-tick times, fewer missed deadlines and good frame pacing from 4-core laptops to many-core desktops. Threads are only ever
an accelerator for work that is proven order-independent, and only where measurement says they pay.

## 1. What was built

| piece | where | notes |
|---|---|---|
| Tick distribution + real-time model | `Core/GameEngine/Include/Common/SimulationTickStats.h` (+ `SimulationTickStatsTest`) | mean / median / p90 / p95 / p99 / p99.9 / worst; counts above 16.67 / 25 / 33.333 / 50 / 66.67 ms; DERIVED model: missed deadlines, catch-up ticks, longest catch-up run, max backlog, sustained TPS |
| Determinism fingerprint | `Common/DeterminismFingerprint.h` (+ `DeterminismFingerprintTest`) | ordered checkpoints `{frame, GameLogic::getCRC(CRC_RECALC), object count}`, 64-bit FNV-1a digest, first-divergence locator |
| Worker-count / threshold policy | `Common/SimulationWorkerPlan.h` (+ `SimulationWorkerPlanTest`) | 0/1/2/3/4/6/8/Auto, SMT-aware, no hybrid assumption, `worthParallelising` overhead gate; **not wired to any thread** |
| Phase attribution | `PerformanceProfile` categories `Logic*` (scripts, terrain, crc, recorder, commands, build-assist, partition, destroy, stores, disabled-loop) and `Zone*` (label, relabel, blocks, hierarchy) | nested scopes inside `GameLogic::update` (both games) and `PathfindZoneManager::calculateZones`; exclusive time is reported so nothing double counts |
| Headless benchmark runner | `ReplaySimulation.cpp`, env-gated | `GENERALS_SIM_BENCH=<base>` writes `_summary.txt`, `_ticks.csv`, `_fingerprint.txt`; `GENERALS_SIM_FP_INTERVAL` (default 30 frames), `GENERALS_SIM_IGNORE_CRC`. Unset = the original loop, byte for byte |
| Heavy workload generator | `scripts/performance/New-SimulationBenchmarkReplay.py` | writes a self-playing AI-vs-AI replay from the header of a replay you recorded; **no game content is committed** (the map is referenced by name from the local install) |
| Harness | `scripts/performance/Invoke-SimulationBenchmark.ps1` | generates the replay, runs N times, compares digests, exits 2 on divergence |

## 2. Benchmark definition

A replay stores the match setup and the commands the HUMAN players issued; computer-controlled slots are simulated by the engine during
playback. A replay with one idle human and 7 brutal AIs is therefore a self-playing 8-player battle of any length: deterministic, repeatable,
headless (no window, GPU, audio or input), and free of proprietary content. Defaults: map `Twilight Flame` (Generals base, 402471 byte map),
7 brutal AI slots (`CH`, USA/China/GLA cycled, start positions = slot numbers, free-for-all), seed 20260, 40 game minutes = 72 000 ticks.
Every random choice is resolved in the setup string (an unresolved `-1` faction leaves an AI idle; 27 base maps were scanned and
`Twilight Flame`, `Whiteout`, `Fortress Avalanche`, `Armored Fury` showed the strongest object growth).

Reproduce (needs your own install and any replay recorded by this build as the header template):

```
python scripts/performance/New-SimulationBenchmarkReplay.py --template "Last Replay.rep" --out bench.rep --map "Twilight Flame" --map-size 402471 --ai 7 --level H --minutes 40 --seed 20260
set GENERALS_SIM_BENCH=out\run1 & set GENERALS_SIM_IGNORE_CRC=1 & generalsv.exe -headless -replay bench.rep   (cwd = the game directory)
powershell -File scripts/performance/Invoke-SimulationBenchmark.ps1 -GameDir <game dir> -Template "Last Replay.rep" -Out out -Exe <generalsv.exe> -Runs 3
```

The engine finds its data relative to the executable, so a copy of the build is run from the game directory under a distinct name
(`generalsv_sim38.exe`); the staged game files are not modified.

## 3. Baseline (serial reference, x64 Release; the development machine, which Windows reports as 14 logical processors; this machine only)

Three runs of the 72 000-tick heavy benchmark, build `93a461f3e` + X64.38 instrumentation:

| | run 1 | run 2 | run 3 |
|---|---|---|---|
| mean / median tick ms | 1.368 / 0.707 | 1.357 / 0.707 | 1.323 / 0.695 |
| p90 / p95 / p99 / p99.9 ms | 1.919 / 2.763 / 24.43 / 48.13 | 1.895 / 2.716 / 24.30 / 46.91 | 1.801 / 2.494 / 24.06 / 47.66 |
| worst ms | 239.5 | 242.0 | 239.0 |
| ticks > 16.67 / 25 / 33.3 / 50 / 66.7 ms | 848 / 677 / 166 / 70 / 50 | 844 / 662 / 160 / 70 / 50 | 840 / 660 / 146 / 70 / 50 |
| model missed deadlines / max backlog | 344 / 2.50 s | 332 / 2.55 s | 311 / 2.46 s |
| uncapped throughput | 731 ticks/s (24x real time) | 737 | 756 |
| **fingerprint digest (2400 checkpoints)** | `74F2CB528BED42ED` | `74F2CB528BED42ED` | `74F2CB528BED42ED` |

The picture: the average tick is tiny (1.4 ms, 4 % of the budget) but the **tail is not**: 1 % of ticks exceed 24 ms, 166 ticks exceed a full
33.3 ms period and the worst is 240 ms. A single tick at 100+ ms is a visible hitch in single player and a stall for every peer in multiplayer.
Averages hide exactly this, which is why the tail statistics are the success criteria. (The model's "sustained TPS" saturates at 30 because
the average is far below the period; it is reported but the discriminating numbers are the tail counts, missed deadlines and backlog.
The model is derived from measured durations run back to back, not a paced run.)

Light reference (the user's own 1:54 skirmish replay, 3 437 ticks): mean 0.31 ms, p99 12.1 ms, worst (excluding the map-load update) small;
this is what the existing X64.35 scenarios looked like and is **not** a stress workload. The map-load update (the first `GameLogic::update`,
which starts the game) costs ~270-470 ms and is excluded from tick statistics and reported separately.

Note on the user's recorded replay: it was recorded by this x64 build and reports "CRC Mismatch in Frame 1600" when played back headless.
See section 8 (R1); the headless benchmark workload does not depend on recorded commands and has no mismatch.

## 4. Subsystem attribution (exclusive ms, heavy benchmark run 1, 72 000 ticks)

| phase | total ms | share | p99 ms | max ms | share of the slowest 1 % of ticks | calls |
|---|---|---|---|---|---|---|
| ai_object_base_update | 26 878 | 27.3 % | 1.71 | 8.5 | 1.8 % | 6 030 564 |
| **path_search** | 18 654 | 18.9 % | 3.23 | **229.5** | **50.6 %** | 62 402 |
| object_update_loops (remaining update modules) | 13 620 | 13.8 % | 0.63 | 60.4 | 4.9 % | 72 000 |
| **zone_calc (all)** | 9 494 | 9.6 % | - | 36.4 | **30.3 %** | 386 |
| logic_scripts | 7 619 | 7.7 % | 0.38 | 140.2 | 4.6 % | 72 000 |
| path_request_entries | 6 418 | 6.5 % | 2.09 | 93.7 | 5.7 % | 39 115 |
| logic_partition | 5 035 | 5.1 % | 0.26 | 2.6 | 0.5 % | 72 000 |
| ai_player_strategy | 3 029 | 3.1 % | 0.42 | 19.0 | 0.6 % | 576 000 |
| locomotor_move / weapon_fire / ai_global / path_reconstruction | 1 817 / 1 734 / 1 101 / 993 | 1.0-1.8 % each | | | < 0.3 % | |
| logic_destroy / disabled_loop / crc / stores / path_queue / missile / build_assist / terrain / commands / recorder / weapon_store | each < 0.7 % | | | | < 0.2 % | |

Two different problems:
* **Throughput** (where the average goes): AI object updates, the remaining update modules, scripts and partition. Spread over six million
  tiny calls; each is a small order-sensitive write to shared state (section 5). Nothing here is a good thread target.
* **Tail** (what causes missed deadlines): path search (51 % of the time inside the worst 1 % of ticks) and ZoneCalc (30 %), then path-request
  entries (6 %). Together ~87 % of the tail. Per-tick budget: 5 000 cells per frame (`PATHFIND_CELLS_PER_FRAME`) bounds a search's *queue*, yet
  one search reaches 229 ms.

### ZoneCalc (re-evaluated under the heavy benchmark)

386 recalculations in 40 game minutes (a whole-map recompute whenever any cell type changes; rate-limited by `ZONE_UPDATE_FREQUENCY` = 300
frames), ~24.6 ms each on average, 36 ms worst. A frame that recalculates zones services **no** path requests (`processPathfindQueue` returns
after `calculateZones`). Split of `calculateZones` (separate run, same digest):

| sub-phase | ms (386 calls) | share of ZoneCalc |
|---|---|---|
| `zone_hierarchy` (whole-map pair pass + flatten of 6 tables) | 6 888 | **~73 %** |
| `zone_label` (block-wise labelling scan) | 791 | 8 % |
| `zone_relabel` (collapse + map back) | 300 | 3 % |
| `zone_blocks` (per-block hierarchical data) | 232 | 2 % |
| remainder (exclusive) | ~3.5 | ~0 |

**Root cause of the cost is algorithmic, not a lack of threads.** `applyZone` -> `resolveZones` / `resolveBlockZones` merge two zone classes by
scanning the **entire** zone table (`for (i = 0; i < sizeOfZE; i++)`), once per boundary pair, for each of six tables, and `flattenZones` ends
with another union loop built on the same primitive: O(boundaries x zones) with zones up to 24 000. In addition the scans index `map[i][j]` with
`i` innermost over a column-major `PathfindCell **` (`m_map[x]` is a column), a strided, cache-hostile walk.

## 5. Thread-ownership audit (classes A-E)

A safe read-only parallel | B parallel compute, serial commit | C possible after a bounded refactor | D must stay serial / order-sensitive | E unknown

| subsystem | class | basis (from the source) |
|---|---|---|
| Script engine update (`TheScriptEngine->UPDATE`) | **D** | scripts mutate arbitrary world state, spawn objects, use the logic RNG |
| TerrainLogic update | D | bridge state read by scripts and the pathfinder right after |
| Command processing (`processCommandList`) | **D** | ordered by the command list; defines the authoritative order |
| Recorder / replay CRC bookkeeping | D | writes the replay stream and the CRC cache |
| Update-module loop (`m_sleepyUpdates` priority heap, `m_normalUpdates`) | **D** | one heap pop order = determinism; every `update()` creates/damages/moves objects and draws logic RNG; `m_curUpdateModule` is global |
| AI object update (state machines, targeting) | D | writes the unit and its victims; 192 `GameLogicRandomValue` call sites in `GameLogic/` |
| AI player strategy (`AIPlayer`, skirmish build/attack) | D | issues orders, reads/writes player economy and teams |
| Weapons / projectiles / damage | D | object creation (IDs), RNG, victim mutation, event order |
| Locomotor / movement | D | writes position, partition dirties, pathfinder goal/pos flags |
| Partition manager update | D (commit) / B (cell-cover math) | dirty list drained in list order; per-cell object lists are order-sensitive and feed target selection order; collision contacts then run `onCollide` in order |
| Partition queries (`iterateObjectsInRange`) used for targeting | A in isolation | read-only, but invoked from inside D code whose results then draw RNG; cannot be hoisted without batching the callers |
| Path search (`internalFindPath`) | **C** | pure function of terrain + request, BUT mutates shared state while searching: `m_openList`/`m_closedList`, per-cell `PathfindCellInfo` from one global free-list pool (`s_infoArray`, `s_firstFree`, 30 000 entries), a cumulative cell budget (`m_cumulativeCellsAllocated` vs `PATHFIND_CELLS_PER_FRAME`) that couples request N to the cost of requests 1..N-1, and results consumed in queue order (`ai->doPathfind`) |
| Path queue dispatch | D | FIFO ring of 512 ids; defines which request gets the frame's budget |
| **ZoneCalc** (`calculateZones`) | **C -> B** | deterministic function of the cell-type grid and layer data; writes cell zones, six tables, zone blocks; no RNG, no objects, no events. Stages are sequential in label allocation (`m_maxZone`) but the six equivalence tables are independent after the labelling stage |
| CRC fingerprint (`getCRC`) | D | running CRC over objects in list order, then RNG seed, partition, players, AI; every checkpoint (0.2 %) |
| Object creation / destruction, `allocateObjectID` | **D** | `m_nextObjID++` and the intrusive `m_objList`; creation order = ID order = CRC order |
| Logic RNG (`theGameLogicSeed[6]`) | D | one global stream; draw order is state |
| Renderer, audio (Miles thread), file system, texture loader | separate | already have their own threads / critical sections (`GameMemory`, `AsciiString`, `textureloader`); not simulation state; out of scope |
| Save / load, network / lockstep, CRC exchange | D | defined against the serial order |
| Shroud update | E | not exercised by this benchmark; needs its own measurement |

Pools and allocators (`GameMemory.cpp`, `AsciiString`, `UnicodeString`) take critical sections, so allocating from a worker is *safe* but
contends; a worker should not allocate game objects at all.

## 6. Dependency graph (one `GameLogic::update`)

```
ScriptEngine -> TerrainLogic -> [CRC every N frames] -> Recorder -> CommandList processing
     -> update modules (heap order; includes AI object updates, movement, weapons)      <-- RNG, object IDs, events
     -> TheAI (player strategy) -> BuildAssistant -> PartitionManager (cells, contacts)
     -> processDestroyList -> stores/victory -> disabled-status loop -> frame++
Pathfinder queue: runs from AI update modules' requests (queueForPath), serviced inside the AI update; ZoneCalc preempts a frame's servicing.
```

Reads/writes and must-precede: scripts precede terrain precede commands precede object updates (each reads state the previous wrote); partition
update must follow all movement of the tick and precede the next tick's targeting; destroy must follow everything that can kill; the CRC must
read a state no worker is mutating. **Order-, RNG- and lifetime-sensitive: everything in the main chain.** The only *data-parallel island* found
is ZoneCalc (pure input -> output) and, with a refactor, the search kernel of path requests.

## 7. Determinism audit

* **RNG**: one global logic stream; 192 draw sites in `GameLogic/`. Any code that draws from it must run in the serial order. Parallel code must
  not draw from it.
* **Client RNG inside logic**: 11 `GameClientRandomValue` call sites live under `GameLogic/` (`ActiveBody`, `EMPUpdate`,
  `ParticleUplinkCannonUpdate`, `StealthUpdate`, `StructureCollapseUpdate`, `StructureToppleUpdate`). `StructureToppleUpdate` feeds
  `m_nextBurstFrame` (logic state) from the client stream; the client stream is consumed by rendering/particles, so logic then depends on what
  the renderer drew. This is a pre-existing hazard and a plausible contributor to R1; it is *not* changed here.
* **Iteration order**: the update heap, the intrusive object list, the partition dirty list and the path queue all define behaviour by order.
  A parallel stage must emit results into an index-addressed buffer and commit them in the serial order, never in worker completion order.
* **Object IDs / creation / destruction**: allocation is a single counter; only the serial thread may create or destroy.
* **Floating point**: `setFPMode()` is applied by `GameLogic::update` and `getCRC`; workers would have to set the same FPU mode per thread, and
  reductions (sums) must use the serial order.
* **Containers**: `std::list` / intrusive lists / heaps are order-defined; hash iteration is not used for logic in the stages audited, but is
  unproven for the rest (E).
* **Verification mechanism**: the fingerprint. Two runs of the same replay must produce the same digest (below); a parallel mode must match the
  serial digest bit for bit before it may ship.

### Serial A vs serial B (measured)
The heavy benchmark was run six times in total across three builds (before the call counters, with them, and with the zone sub-phase scopes) and all produced
`74F2CB528BED42ED` over 2 400 checkpoints; the light replay produced `40148F96CB0EB2B0` twice. The oracle is stable and the instrumentation does
not perturb the simulation.

## 8. Risks recorded

* **R1** The user's own last replay (recorded by this build) reports a CRC mismatch at frame 1600 on headless playback. Record and playback
  diverge on this build while headless playback is self-consistent. Not investigated here; candidates: client-RNG use in logic (section 7), or
  state that depends on the live client. Needs a dedicated investigation before any multiplayer claim. The benchmark is unaffected.
* **R2** The model's sustained-TPS metric saturates at 30; use the tail counts and backlog.
* **R3** Results are from one machine (14 logical processors). Nothing here is evidence about 4-core laptops; the plan below treats them as the
  baseline where Auto = serial.

## 9. Serial reference and hashing strategy

* Serial is the default and the reference; `workers = 0` is always valid and is what 4-core machines get under Auto.
* A parallel stage must (a) compute into index-addressed buffers from an immutable snapshot, (b) commit serially in the serial order,
  (c) be skipped when `worthParallelising` says the work is small, (d) be bit-compared against the serial result in a verify mode, and
  (e) leave the fingerprint digest equal to the serial digest.
* The fingerprint is the engine's own state CRC sampled every 30 frames (configurable) plus the object count, hashed in order with FNV-1a.

## 10. Worker architecture and count strategy (design, not built)

* A bounded pool created once at game start (size from `SimulationWorkerPlan`), parked on an event when idle; jobs are index ranges with a join
  barrier; no work stealing, no dynamic task graph, no thread creation per tick. Workers never touch game objects, the RNG, the allocator pools
  for game state, or any `TheXxx` singleton.
* Counts: 0, 1, 2, 3, 4, 6, 8 and Auto. Auto counts **physical** cores from the OS (hybrid CPUs: a P and an E core are one each; no assumption
  about which is fast), reserves two cores (renderer, audio/driver/OS), caps at 6, and is **serial at 4 or fewer physical cores**. Explicit
  settings are honoured up to logical processors - 1. Pinned by `SimulationWorkerPlanTest`.
* Overhead rule: hand work over only when `serialCost * (1 - 1/(workers+1)) > 3 x handoffCost`; a job that fails the test runs on the caller, so
  small workloads cannot show negative scaling by construction. The hand-off cost must be *measured* on the target (not assumed) before the pool
  ships.
* Frame pacing: workers join inside the tick, so they never extend a tick beyond the serial maximum and never touch the presentation clock;
  Render Scale and the renderer are untouched.

## 11. First candidate, by evidence

**ZoneCalc `zone_hierarchy`.** It is 7.1 % of all simulation time and 20 % of the slowest 1 % of ticks, it is a pure function, and its inputs and
outputs are enumerable (cell zones + six tables + zone blocks), so equality with the serial result is directly checkable.

**But the evidence says threads are the wrong first tool.** The cost is the O(zones) scan inside every union. Replacing it with a union-find
that keeps the **minimum** representative produces *identical* arrays: the existing primitive relabels every entry equal to either class with
`finalZone = zoneEquivalency[min(...)]`, and by induction each label is the minimum index of its class. That is a pure algorithmic change, needs no
thread, no pool, and works on a 4-core laptop. A differential unit test (random union sequences against the existing primitive) proves it
independently of the engine; in-engine proof is the fingerprint (`74F2CB528BED42ED`) plus a verify mode that runs both and compares all tables.
Threads (one per table: the six tables are independent after the labelling stage) are a *later* option only if a profile of the optimised code
still shows ZoneCalc in the tail.

Not implemented in this batch: wiring any change into `calculateZones` was judged outside "one small, clearly safe prototype" because the
flatten stage has its own union loop and the in-engine verify mode does not exist yet. Recommendation for X64.39 below.

## 12. Rejected / deferred candidates

| candidate | decision | reason |
|---|---|---|
| Parallel object / AI update loop | rejected | order- and RNG-sensitive, shared writes (D) |
| Parallel script engine, commands, destroy, CRC | rejected | D |
| Parallel path search, general | deferred (C) | shared open/closed lists and cell-info pool, cumulative per-frame budget couples requests, results consumed in queue order; needs per-search scratch state and a budget-faithful speculative scheme; high value (51 % of the tail) but the largest refactor |
| Parallel partition update | deferred | 5 % of time, 0.5 % of the tail, commit is order-sensitive |
| Parallel per-table flatten of ZoneCalc | deferred | each flatten is O(zones) and tiny; hand-off would cost more than the work (fails `worthParallelising`) |
| Parallel CRC | rejected | 0.2 % of time; CRC is a running value over a defined order |
| Any renderer / Render Scale change | out of scope | |

## 13. Broad-hardware strategy

* 4 physical cores or fewer (typical laptops): serial. Under Auto nothing changes; algorithmic fixes (ZoneCalc, then path search) are the
  only benefit and are the safest.
* 6-8 cores: 3-4 workers at most, only for stages that pass the overhead gate.
* 12+ cores / hybrid: capped at 6; no core-type assumption, because the OS scheduler, not the engine, decides placement.
* Every parallel stage is optional, switchable to serial at runtime, and benchmarked against the serial baseline on the same replay on each tier
  before it is enabled by default. Negative scaling is a release blocker.

## 14. Validation

See the final report of the commit. Automated: `SimulationTickStats.*`, `DeterminismFingerprint.*`, `SimulationWorkerPlan.*`, the existing
PerformanceProfile and instrumentation contracts, the full win64 and win32 builds and test suites. Measurement: the three-run heavy baseline
above. **No native gameplay was run and none is needed for this batch** (all measurements are headless).

## 15. Next batch (X64.39 proposal)

1. In-engine ZoneCalc verify mode (`GENERALS_ZONE_VERIFY`): compute with the serial primitive and a candidate, compare all tables and cell zones.
2. Min-root union-find for `resolveZones` / `resolveBlockZones` / the flatten loop, with the differential unit test; keep the old path behind the
   verify switch for one release.
3. Re-run the heavy benchmark: success = p99/worst drop, `zone_hierarchy` share falls, digest `74F2CB528BED42ED` unchanged.
4. Then re-profile path search (51 % of the tail) and decide whether a per-search scratch refactor is justified; the worker pool is built only if a
   stage that passes the overhead gate still exists after the algorithmic fixes.

## Follow-up

X64.39 (`X64_39_ZONECALC_OPTIMIZATION.md`) implemented the serial ZoneCalc fix recommended in section 11 without threads and with a verify mode; section 15 of that document is the current next-step list. Note: the X64.38 hypothesis that `StructureToppleUpdate` explains the replay mismatch is not supported by the X64.39 audit.
