# X64.43 - Exhaustive failing path searches: characterisation and decision

Serial, no threads, **no gameplay or pathfinding change**. Result of the decision gate: **C - no justified optimization.** What this milestone adds is an off-by-default diagnostic, an explanation backed
by measurements, and a documented "no" with the evidence for it.

Starting state: `dev/modern-engine` = `origin/dev/modern-engine` = `ecb646054`, clean tree. Authoritative heavy-benchmark fingerprint `74F2CB528BED42ED`: unchanged in every run of this milestone.

## 1. Question

After X64.39-X64.42 the remaining known tail was A* searches that **exhaust the open list and return no path** (`Pathfinder::internalFindPath`, "null after work"), ~40 % of the slowest 1 % of ticks in the
X64.42 profile. Why do they happen, who asks for them, and can they be made cheaper **without changing a single path, decision or simulation result**?

## 2. Diagnostic (new, off by default)

`GENERALS_SIM_PATHFAIL=1` (headless benchmark runner) enables `Core/GameEngine/Source/GameLogic/AI/PathFailureDiag.cpp` (+ `Include/GameLogic/Pathfinder/PathFailureDiag.h`):

* Hooks in `Pathfinder::internalFindPath` (begin / pop counter / success / failure), in `findPath` (did the hierarchical pre-search find a path or fall back?) and one counter in each of six `PathfindCell`
  mutators and in the zone-recalculation call. With the switch off each hook is one branch on a plain int; four default runs of the instrumented build match the X64.42 default runs within run-to-run
  noise (mean 0.936 vs 0.945 ms, p99 6.1 vs 5.9 ms, ticks > 33.3 ms 4.75 vs 4.5, derived missed deadlines 9.3 vs 8.0 - different sessions).
* For every search that reaches the main loop it records a request signature (start cell, goal cell, locomotor surfaces, crusher, radius, centre flag, human rules, layers, ignored obstacle, tunnelling).
  For every search that **fails after work** it additionally records, before the failure cleanup: the frame, requester (object, template, team prototype, player, AI state, command source, attack-path flag),
  hierarchical result, zones, goal-cell facts (type, unit flags, pinched, obstacle), the goal's 8 neighbours (reached by the search? terrain-valid?), the explored (closed) region (size, bounding box, nearest
  explored cell to the goal), the **frontier** of the explored region (every never-entered neighbour of an expanded cell, classified by reason: cliff / water / rubble / structure / off-map / ally in the footprint
  that is not idle / uncrushable enemy in the footprint) with the blocking units, a terrain-only breadth-first reachability check of start to goal (same cell-validity and diagonal rules, ground layer), and
  repetition data (earlier asks of the same signature, zone recalculations and cell-state mutations in between).
* **Read-only**: contract `scripts/performance/test_x64_43_pathfail_contract.py` (8 tests incl. 6 mutation tests) forbids any mutator, random-number use or write to a `Pathfinder` member in the diagnostic and
  requires every hook to be guarded; the Stage4B.1 function-level guard was extended to treat the hooks as instrumentation (2 new tests, one of which proves a behavioural edit next to a hook is still caught).
  Digests with the diagnostic on and off are identical on Dark Mountain (`803DDAB2FFEAB52D`), Rogue Agent (`222CEECE0F6577BC`) and Armored Fury (`E54CADD2FBB358B4`), and the fingerprint is unchanged on
  the heavy benchmark with it on.
* It only costs time for failing searches (a few hundred per 72 000 ticks) and a signature update per search; diagnostic runs are kept apart from timing runs. The failing-search time it reports is taken
  before its own analysis starts.

## 3. Workload

Heavy benchmark (Twilight Flame, 7 Brutal AI, 72 000 ticks, 30 TPS) for depth; the 16 representative maps (40 simulated minutes each, 7 AI where the map allows) for generality. Bridge Busters, Cliff and SwissMP have
idle AIs (0 searches), as noted since X64.40.

## 4. What the failing searches are (heavy benchmark)

22 069 searches reached the A* main loop: 21 489 found a path, **580 exhausted the open list** (1.95-1.99 s in total, mean 3.4 ms; the whole run's search time is ~65 s of tick time).

| failing searches by cost | count | share of failing time |
|---|---|---|
| >= 1 ms | 335 | 99.7 % |
| >= 5 ms | 108-112 | ~50 % |
| >= 10 ms | 14-16 | ~22 % |
| >= 20 ms | 6-7 | ~15 % |
| >= 33.3 ms (a whole tick period) | 4 | ~12 % |
| >= 50 ms | 2 | ~8 % |

(Two diagnostic runs: counts near the thresholds differ by one or two because the threshold is a timing measurement.) 245 of the 580 are cheap (< 1 000 head pops, < 1 ms in total 6 ms). The other 335 explore
**about 22 000 cells** (median 22 238 cells, p90 22 988, maximum 25 084) with 10 000-104 000 head pops, and conclude "no path" only when the region is used up.

### 4.1 Which requests (all 112 expensive failures, heavy benchmark)

* **Entry point.** Every one is `Pathfinder::findPath` -> `internalFindPath`, reached only through `AIUpdateInterface::doPathfind` -> `computePath` (the pathfind queue). No other caller exists in the sources.
  After a failure, `computePath` falls back to `findClosestPath` (a second search) when the unit has no path.
* **Requester AI state:** GUARD 92 (869 ms), DOCK 10, HUNT 5, MOVE_TO 2, FOLLOW_PATH 2, GUARD_TUNNEL_NETWORK 1. Over all 580 failures: GUARD 385, FOLLOW_PATH 92, HUNT 42, DOCK 42.
* **Command source:** SCRIPT 100 (940 ms), AI 12: the AI skirmish scripts order a team to guard / move to a location (teams `China D5H Base Expanders4`, `China D2H Big Team - C4`, `China D2H Infantry Big Rush4`,
  `OverlordA`, `Tanks`; supply trucks of `teamSkirmishChina4` for DOCK).
* **Mover:** ground combat units of computer players (Redguards, Tank Hunters, Battlemasters, Gattling tanks, supply trucks, one dozer); 0 human requests; all 99 % "computer rules".
* **Hierarchical pre-search:** 98 of the 112 had found a hierarchical path (so the A* ran restricted to its zone corridor); 14 (428.8 ms, the five most expensive ones: 77, 73, 45, 37, 28 ms) had fallen back to
  "all zones passable". The fallback is not a reliable predictor of failure: 523 requests fall back in the run and 417 of them succeed (X64.40).
* **Goal:** a clear cell in 111 of 112; in 73 the goal cell carries another unit's goal flag. Goals concentrate: 112 expensive failures target only 29 areas (4x4 cells), e.g. around cell (21,272) / (22,288) (the
  west map edge) from many bases, and around (400,146) / (392,156) (the east side) from the China army at (371,155).

### 4.2 Why they fail

* **The zone test cannot reject them.** In all 112 the effective start and goal zones are equal (`zone1 == zone2`); the existing zone screen rejected only 3 requests in the whole run. Zones describe static terrain
  connectivity and say "connected".
* **Terrain connects them too:** the terrain-only breadth-first check (same cell-validity rule, structures treated as impassable) reaches the goal in 109 of the 112 expensive failures (3: BFS 0, where the check's
  simplifications - ground layer only, diagonal rule on static validity - may differ from the real search).
* **The goal's neighbours were never reached** (0 of 8 for 110 of 112; the search explored a region whose nearest cell is 3-160 cells from the goal) although they are terrain-valid.
* **The explored region's frontier is sealed by units:** all 112 have blocking units on the frontier. Summed over all 580 failures (distinct units per failure) the diagnostic saw 7 129 blocker sightings: relationship to the mover ally 6 900 / enemy 229, owner side China 6 910 / GLA 151 / America 68, mostly
  stationary Battlemasters, Tank Hunters, Gattlings, Dragons. The retail rule is in `Pathfinder::checkForMovement`: a footprint cell holding a *fixed* unit blocks the move when the unit is an ally that is **not idle**
  ("can't path through not-idle units", `RTS_GENERALS && RETAIL_COMPATIBLE_PATHFINDING`) or an enemy that cannot be crushed. The remaining frontier is cliffs (218 k cells), structures (159 k) and the map edge.
* Reading: a stationary army (often the AI's own, in a non-idle guard / attack state) stands in a chokepoint; every unit behind it that is told to guard or move beyond it makes A* explore the whole connected pocket
  (~22 000 cells, re-expanding cells ~5 times: 104 000 pops for 21 000 cells in the worst search) before concluding that no way through exists. This matches the observed behaviour of AIs stacking up large armies at
  chokepoints.

## 5. Repetition

* 10 072 distinct request signatures; 472 ever failed; **55 failed more than once (108 repeat failures)**; 455 failed every time they were asked; 17 alternated between success and failure.
* 120 failures repeat an earlier signature; median gap 110 frames (3.7 s), 22 within one second, 3 in the same frame, maximum 42 472 frames. Only 22 of the 112 expensive failures repeat an earlier signature (160 ms of 1 031 ms).
* **Topology** does change between repeats in most cases (zone recalculation in between for 40 of 120), and the **cell state** (units' positions, goals and flags in the pathfinder map) changes constantly:
  the mean number of cell-state mutations between two asks of the same signature is 95 945 and only **2 of 120** repeats saw none.
* Consequence: even the most favourable failure memo would save at most ~0.16 s per 72 000 ticks, and a state-version token (the only way to prove a memoised failure unchanged) would have hit twice.

## 6. Multi-map check (diagnostic, 40 simulated minutes each)

14 maps produced searches (183 191 searches in total, 1 085 exhausted the open list, 117 of them >= 5 ms, **3 >= 20 ms (all Dark Mountain), 1 >= 33.3 ms**). In every map with expensive failures the pattern is the same:
GUARD/SCRIPT requesters, terrain connects the points, units on the frontier (blocked by units in 117 of 117), hierarchical path found for ~95 %. Twilight Flame is the outlier in how many large failures it produces.
Per map (searches / failures / >= 5 ms / >= 20 ms / worst ms): Armored Fury 35 773 / 155 / 15 / 0 / 14.0; Dark Mountain 17 640 / 87 / 32 / 3 / 35.1; Fallen Empire 12 526 / 49 / 1 / 0 / 5.1; Fortress Avalanche 26 668 / 132 / 5 / 0 / 14.8;
Golden Oasis 10 703 / 29 / 3 / 0 / 6.7; Homeland Alliance 11 664 / 69 / 6 / 0 / 10.5; Lone Eagle 13 366 / 162 / 19 / 0 / 9.1; Rogue Agent 11 988 / 145 / 28 / 0 / 16.0; Tournament Island 12 961 / 75 / 5 / 0 / 6.2;
Whiteout 23 647 / 179 / 3 / 0 / 6.0; Alpine Assault 1 891 / 1 / 0; Scorched Earth 2 103 / 2 / 0; Silent River 2 261 / 0 (diagnostic times; the numbers near a threshold vary by a few per run).

## 7. Can the engine already know? Is an exact early rejection possible?

1. **Existing zone / topology knowledge:** equal zones in every case; static, therefore no help. The hierarchical pre-search does not prove failure (it ignores units; 80 % of its failures are followed by a successful A*).
2. **Information at the caller:** `computePath` knows the requested destination and runs cheap checks before `findPath` (destination valid, quick line passable); it has no knowledge of what units seal the region.
3. **What an exact rejection would have to prove:** that no enterable-cell path exists for this mover from this start in the *current* unit configuration. That is a property of (a) static terrain and structures (zone recalculations
   track them), (b) the unit flags in the footprints of ~2 000-6 000 frontier cells, (c) object state outside the map (a blocker's idleness, relationships between the mover's owner and each blocker, the mover's crush ability).
   A token covering (b) and (c) would need per-cell version stamps on every unit position / goal update, per-object state versions for idleness, and per-failure frontier bookkeeping, in the hottest pathfinder paths.
   The measured benefit ceiling is ~0.16-0.3 s per 72 000 ticks (repeats only) and the measured token hit rate is 2 of 120. Equivalence cannot be demonstrated with this machinery, and the gain does not justify it.
4. **Making the exhaustive search itself cheaper:** the work per search (expansions, line walks) is the retail algorithm; the open-list insertion (X64.40) and the line-walk checks (X64.41) were the exact parts. The remaining
   costs are the 104 000 re-expansions of ~21 000 cells (which determine the chosen path and tie-breaking) and ~6 M line-walk callbacks; no further exact reduction was found.
5. **Caller-side:** the requests come from AI scripts issuing guard/move orders into sealed regions. Avoiding them would change AI behaviour (retry cadence), which is gameplay timing and out of scope.

## 8. Decision gate

**C - no justified optimization.** The remaining failures are legitimate exhaustive searches whose outcome depends on the live positions and states of hundreds of units. No exact request-level rejection, duplicate
elimination or cache can be proven equivalent without a large new invariant-tracking subsystem, and the achievable gain is small. D also holds: after X64.42 about 6-7 failing searches per 72 000 ticks exceed 20 ms
(0.01 % of ticks, ~4 of the ~8 derived missed deadlines of the heavy benchmark). Nothing was changed in production behaviour. The diagnostic stays as tooling.

## 9. Rejected ideas (none implemented in production)

| idea | why rejected |
|---|---|
| failure memo keyed by request signature | repeats are 120 of 580 failures; cell state changes by ~96 000 mutations between repeats (token hit 2 of 120); stale-state risk; benefit <= ~0.16 s per run |
| failure memo keyed by goal / region ("goal unreachable for a mover class") | the sealing depends on per-unit state and mover relationships; 112 expensive failures hit 29 goal areas so the ceiling is higher (~0.7 s) but equivalence is unprovable |
| trust the hierarchical result to skip the full A* | hierarchical failure is followed by a successful A* in 80 % of cases |
| node / time budget per search | changes results (a valid long path would become a failure) - forbidden |
| earlier abort when the goal's neighbours are unreached | cannot be known before exhausting the region; a path may still exist around |
| treat idle/non-idle blockers differently in the search | changes pathfinding semantics - forbidden |

## 10. Validation

* Win64 and Win32 builds; ctest 16/16 and 23/23; gtests 420 / 419 (Win64) and 382 / 382 (Win32).
* Python: 24 contract and performance tests (incl. Stage4B.1 with 2 new tests, Stage4B.3, the X64.35 instrumentation contract, the new X64.43 contract); ZH and native cleanup workflow tests.
* Fingerprint `74F2CB528BED42ED` with the diagnostic on and off; digests equal with it on and off on three further maps.
* No production behaviour change, so no legacy switch and no A/B were needed. No existing test was weakened: one contract (`calculateZones(...); } return;`) constrained where the zone-recalculation counter may sit and the counter was
  placed accordingly.

## 11. Remaining bottlenecks

1. These failing searches: six to seven per 72 000 ticks above 20 ms on Twilight Flame, rarer elsewhere; dependent on large idle/guarding armies at chokepoints.
2. ZoneCalc recalculation coinciding with other work (~5.6 ms, 386 per run).
3. `ai_object_base_update` cumulative throughput (not a tail issue).
4. Frame-0 AI setup (one-time).

## 12. Is the serial CPU optimization phase at diminishing returns?

**Yes.** Reasons: the Normal budget is 33.333 ms and the heavy 8-player benchmark now misses it ~8 times in 72 000 ticks (median 0.7 ms, p99 5.9 ms); what remains is workload-specific (AI armies sealing
chokepoints), rare, and state-dependent in a way that has no cheap exact shortcut; `ai_object_base_update` is throughput, not tail; the next real gains would be architectural (parallelism) or require changing AI/pathing
behaviour, neither of which is a low-risk serial optimization. **Recommendation: pause CPU optimization here** and move to feature / completeness / Zero Hour validation work until a real-game scalability measurement
(below) shows the engine failing to sustain 30 TPS.

## 13. Future real-game scalability benchmark (design only; nothing implemented, no limits claimed)

The known real-game symptom - a 6-player map where AIs stack up very large armies at cliffs/chokepoints and the game slows over time - is not reproduced by the 40-minute AI-vs-AI benchmark at its current army sizes.
A future benchmark should measure, on that map with a human + 5 AI (recorded or scripted, deterministic): object count over time; per-tick percentiles, deadline misses and backlog (existing model); sustained TPS when the
simulation is driven at the real-time rate; path requests per second and failing searches (`GENERALS_SIM_PATHS`, `GENERALS_SIM_PATHFAIL`); `ai_object_base_update`, `object_update_loops`, ZoneCalc; and the blocker
count at chokepoints. Goal: answer "how large can a real Generals battle become before the engine no longer sustains Normal 30 TPS", with measured numbers only.

## 14. Recommendation for the next milestone

Not another CPU optimization. Candidates: (a) the real-game scalability benchmark above (measurement only), (b) Zero Hour native validation and completeness work, (c) Game Speed implementation when separately authorized.
