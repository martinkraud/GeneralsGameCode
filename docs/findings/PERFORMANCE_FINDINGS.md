# Performance findings (simulation CPU)

All measurements use the deterministic heavy benchmark described in [`docs/PERFORMANCE.md`](../PERFORMANCE.md): map Twilight Flame, one idle human + 7 Brutal AI, 72 000 authoritative ticks
(40 game minutes at 30 TPS), headless replay simulation, one development machine. Numbers are therefore evidence about *this workload on this machine*; the root causes are code-level and
visible without the benchmark. Every optimization below was built so that the original algorithm is still in the binary and selectable, and the authoritative fingerprint
`74F2CB528BED42ED` (FNV-1a over frame, state CRC and object count, 2 400 checkpoints) was identical in every mode.

Where a number is a "same-executable A/B", the two arms are the same binary with an environment switch, run alternately with nothing else running. Percentages for the "slowest 1 % of ticks"
are shares of the summed time of the 720 slowest ticks.

---

## P-1. Synchronous disk flush of a diagnostic log on the simulation thread (X64.42)

**Classification: HIGH upstream relevance for any build that has a flushed log written from gameplay code; the exact trigger (this log) is MKRaudstein-specific.**

### Problem / symptom

A 70-110 ms tick every 300 logic frames (every 10 s of game time) in large AI battles, and 55-84 ms spikes in the object-update phase. Both showed up as the dominant source of ticks above
the 33.333 ms Normal-speed deadline once the pathfinder had been optimised.

### Affected source

* `Core/GameEngine/Include/Common/StartupDiagnostics.h`, `StartupDiagnostics::checkpoint()`: appended a record with `WriteFile` and then called `FlushFileBuffers(s.file)` on **every** record,
  including in a normal launch, where the file is `generals64.log` next to the `generalsv*.exe` / `generalszh*.exe`.
* Gameplay callers (Generals; Zero Hour has none of these):
  `Generals/Code/GameEngine/Source/GameLogic/Object/SpecialPower/OCLSpecialPower.cpp:156` (`special_power/activate`),
  `Generals/Code/GameEngine/Source/GameLogic/AI/AIGroup.cpp:2676,2688` (`special_power/dispatch`, `missing_science`),
  `.../Object/Update/AIUpdate/DeliverPayloadAIUpdate.cpp:189,312` (`delivery/trace`, `delivery/start`),
  `.../Object/ObjectCreationList.cpp:498` (`delivery/no_ai`). All are guarded by `StartupDiagnostics::normalLaunch()` only.
* History: the per-record flush was introduced with the X64.12 startup gates (`b79dc70d5`), where a crash-safe record is the purpose; the gameplay checkpoints were added during the
  X64.31 Daisy Cutter / special-power investigation (`03eabecfa`) and stayed in the normal-launch build.

### Reproduction / workload

Heavy benchmark with `GENERALS_SIM_SCRIPTS=1` (per-script / per-condition / per-action attribution, new in X64.42). All 15 script updates above 20 ms were on frames divisible by 300
(worst 83 ms) and the cost was one action, `[Skirmish] Special power -- fire at enemy's highest cost area` (60 calls, 1 320 ms total, 22 ms mean, 78-86 ms max), fired by the AI
superweapon (`... Fire`) scripts; the 300-frame period is consistent with a script evaluation delay of 10 s (`executeScript` sets the next evaluation frame to `frame + getDelayEvalSeconds() * LOGICFRAMES_PER_SECOND`; the script data itself was not inspected).

### Root cause and why it stalls

Sub-timing the action split it into *computing the target* (6.5 ms in total over the run) and *firing the power* (1 359 ms in total). Firing reaches the checkpoint calls above, each of which
issues a synchronous flush of the file buffers to storage: 10-85 ms depending on the drive/OS state. It is on the authoritative logic thread, so the tick that fires a power, delivers a
payload (paradrop, airstrike, carpet bomb) or dispatches a group power stalls for the duration of the disk flush. The log contained exactly 60 `special_power` records for the 60 calls. Running the same replay under an
executable name that is not a game executable (no log is opened) removed the stalls: script update worst 83 ms -> 5.0 ms, none above 20 ms, the action's total 1 394 -> 37 ms.

It is not benchmark-specific: every real normal-launch game paid this flush whenever those paths ran. The 55-84 ms `object_update_loops` spikes were the same cause (payload-delivery records);
no separate update-module optimisation was needed.

### Fix used by MKRaudstein

`checkpoint()` keeps `WriteFile` and skips `FlushFileBuffers` unless (a) an evidence-harness mode is active (`enabled()`), (b) the log is not the normal-launch log, or (c) the environment variable
`GENERALS_NORMAL_LOG_FLUSH=1` is set. `WriteFile` hands the record to the OS, which preserves it if the *process* crashes; only an OS crash or power loss could lose the last records.

### Behavioural constraints

Nothing in the simulation reads the log; no record content, order or count changes. The harness modes (startup gates) keep their per-record durability.

### Correctness evidence

* Fingerprint `74F2CB528BED42ED` in all 8 A/B runs and in the final build.
* **16 maps** (the X64.40/41 representative set, 14 simulated minutes each, 7 AI where the map allows): the digest with the flush equals the digest without it on every map, and the
  *content* of `generals64.log` (stage / status / code / detail sequence) is identical on every map.
* `scripts/performance/test_x64_42_startup_log_contract.py`: structural contract on `checkpoint()` plus four mutation tests (unconditional flush, `flush = true`, harness modes unflushed, switch missing) - each must fail the contract.

### Benchmark (same-executable A/B, alternating x 4, `GENERALS_NORMAL_LOG_FLUSH=1` vs default)

| | flush on (X64.41 behaviour) | fixed |
|---|---|---|
| script update worst | 85.1 ms | 3.9 ms |
| script share of the slowest 1 % | 17.2 % | 1.9 % |
| `object_update_loops` worst / share | 55.5 ms / 17.3 % | 10.9 ms / 3.6 % |
| p99.9 tick | 17.8 ms | 9.1 ms |
| ticks > 33.3 / 50 / 66.7 ms | 34.0 / 22.5 / 8.8 | 4.8 / 2.8 / 2.0 |
| derived missed deadlines | 45.2 | 8.2 |
| max derived backlog | 55.0 ms | 49.7 ms |
| ticks > 20 ms (one paired run) | 61, of which 17 on frames divisible by 300 | 14, none periodic |

### Risks / trade-offs

Records written in the last moments before an operating-system crash or power loss can be lost. Setting `GENERALS_NORMAL_LOG_FLUSH=1` restores the old behaviour for a forensic session.
The 4 096-record cap of the normal log is unchanged.

### Tests / commits

`ScriptProfileTest` (4 tests), `test_x64_42_startup_log_contract.py`. Commit `1ebe641f9`; report `docs/modernization/X64_42_SIMULATION_TAIL_OPTIMIZATION.md`.

### Upstream applicability

Potentially useful to any project with a flushed (`FlushFileBuffers`, `fflush` + `_commit`, `fsync`) diagnostic written from per-tick or event-driven simulation code: check for
synchronous durability calls on the logic thread. Nothing in stock GeneralsGameCode was shown to have this; the log is part of this fork's normal-launch diagnostics.

---

## P-2. `ZoneCalc`: an O(zones) table rewrite per equivalence merge (X64.39)

**Classification: POSSIBLE upstream relevance (shared `Core` pathfinder code, behaviour-identical rewrite).**

### Problem / symptom

Recalculating the pathfinder zones (`PathfindZoneManager::calculateZones`, run when cell types change, 386 times in the benchmark) took 23.5 ms per call and was ~30 % of the slowest 1 % of
ticks (moderate tail, 16-33 ms).

### Affected source

`Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp` - `PathfindZoneManager::calculateZones`, `applyZone`, `resolveZones`, `resolveBlockZones`, `ZoneBlock::blockCalculateZones`, `flattenZones`.
Generals and Zero Hour share the merge primitives (differences are in `RTS_GENERALS && RETAIL_COMPATIBLE_PATHFINDING` branches that choose *which table* a boundary pair is applied to).

### Legacy algorithm

1. Label scan per 10x10 block: each cell takes a neighbour's label (`applyZone`) or a fresh one (`m_maxZone++`); two different labels meeting at a boundary are merged in `zoneEquivalency[24000]`.
2. Collapse (ascending numbering of the classes) and relabel every cell.
3. Layer (bridge) zones and four per-block tables.
4. A whole-map pair pass applies the rule of each of six tables (hierarchical, ground-cliff, ground-water, ground-rubble, terrain, crusher) to every neighbouring pair with different zones.
5. `flattenZones` for the hierarchical table, then five more.

`resolveZones(src, target, E, n)` maps both arguments through `E`, takes the smaller class, and then **scans the whole table (up to 24 000 entries) rewriting every entry equal to either class**.
That is O(Z) per merge; the benchmark's merges × 24 000 entries × six tables was `zone_hierarchy` = 7 082 ms of 9 081 ms.

### The invariant that makes a faster exact version possible

Starting from the identity, after any sequence of legacy merges **`E[i]` is the smallest index of the equivalence class of `i`, and `E[label] = label`.** Proof by induction on the merges: merging classes
with minima `a < b` rewrites every entry equal to `b` into `a`, so the new class has minimum `a`. `resolveBlockZones` is the same operation on block-relative storage with absolute labels; `resolveZones`
applies it after mapping its arguments through `E`, which is idempotent (`E[E[x]] = E[x]`). Consequence: the final arrays depend only on the *partition* of labels, not on merge order, and they equal
the per-class minimum - exactly what a union-find that always keeps the smaller root computes after flattening.

**Where the invariant does not hold:** `flattenZones`. Its first loop rewrites entries through a *second* table (`arr[i] = hier[arr[hier[arr[i]]]]`), so its second loop calls `resolveZones` on an
array that is no longer idempotent. That function stays verbatim legacy in both modes (0.8 s of the benchmark, 2.1 ms per recalculation after the change).

### Fix

`Core/GameEngine/Include/GameLogic/Pathfinder/ZoneEquivalence.h` holds both implementations in one pure header: `legacy*` (verbatim semantics) and `minRoot*` - `find` with path halving
(`E[x] = E[E[x]]`), `union` keeps the smaller root, and each table is flattened (`E[i] = find(i)`) at the three points where the arrays are consumed as final labels (after the label scan, after the block
scans, after the pair pass). `applyZone` returns the neighbour's root where legacy returned `E[neighbour]` - the same value by the invariant. No RNG, no threads, no new allocation, same iteration order.
Complexity: amortised near O(1) per merge plus one O(Z) flatten per table per recalculation.

### Correctness evidence

* `ZoneEquivalenceTest` (9 tests): both implementations are driven with identical sequences and the resulting arrays compared **bit for bit** (ascending/descending/alternating chains of 2 000, stars, repeated merges,
  29 disconnected groups, 36 random graphs of 2 500 nodes and up to 9 000 merges, simulated label scans over random 90x70 grids, block tables with first-zone offsets 0 ... 20 000, the flatten stage fed from either
  implementation, a table of 24 000). One test recomputes the classes by brute force after each of 400 random merges and checks `E[i] <= i`, idempotence and `E[i] = min(class)`.
* `GENERALS_ZONE_VERIFY=1`: runs the candidate, then the legacy oracle on the same inputs, compares all six tables, every cell zone, the layer zones and every block's tables; the legacy result stays in the game state, so a
  wrong candidate cannot alter the simulation; first difference prints segment/index/values and `_exit(3)`. **388 recalculations compared, 0 mismatches**, plus 1 649 on 16 further maps. `GENERALS_ZONE_VERIFY_INJECT=1`
  flips one candidate value and the verifier reports it immediately (mutation test of the oracle). `GENERALS_ZONE_LEGACY=1` selects the original primitives for an A/B on one binary.

### Benchmark (same executable, alternating legacy/default, 3 rounds each)

ZoneCalc per recalculation: 23.5 -> 5.5 ms (-77 %), worst 28.5-32.1 -> 7.5-7.8 ms; `zone_hierarchy` 7 082 -> 465 ms (15x); `zone_label` 781 -> 376 ms (same merge primitive). Whole tick: p99 23.4 -> 8.4 ms
(-64 %), ticks > 16.67 ms 836 -> 450. **Unchanged** by design: ticks > 33.3 ms (139 -> 140), the worst tick, derived missed deadlines (307 -> 311) - those came from path search (P-3).

### Risks / trade-offs

The legacy flatten stage is kept; it is now the largest ZoneCalc piece (801 ms of the 2 140 ms total, 2.1 ms per recalculation). A path-halving union-find without ranks has no worst-case logarithmic bound; in practice it is near constant here.

### Commits

`c51dc603d`; report `docs/modernization/X64_39_ZONECALC_OPTIMIZATION.md`.

---

## P-3. A* open-list insertion walked the sorted list from the head (X64.40)

**Classification: POSSIBLE upstream relevance (shared `Core` pathfinder code; upstream already has a different, non-retail-compatible mitigation).**

### Problem / symptom

Rare searches took 100-230 ms. With the benchmark's 24 132 internal A* searches: 580 searches that exhaust the open list and return no path were 70 % of all A* time (12.7 s of 18.2 s); the 343 searches
that expand ≥ 20 000 cells (1.4 % of searches) were 72.7 %.

### Affected source

`Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp`: `PathfindCellList` (`putOnSortedOpenList`), `forwardInsertionSortRetailCompatible`, `removeFromOpenList`. The open list is a doubly linked list sorted
ascending by 16-bit total cost.

### Root cause

Retail insertion starts **at the head every time** and walks past every node with cost ≤ the new cost, capped at 5 000 nodes (the cap reuses the constant `PATHFIND_CELLS_PER_FRAME`; this is a coincidence of reuse, not a
CPU budget). That is O(open-list length) per insert. Measured: mean 97 hops per insertion (2.29e9 hops over 23.5 M insertions), 120 for the A* kernel; forward hops per search p99 3.1 M, p99.9 9.5 M, max 19.0 M; correlation of
search time with forward hops 0.944. The forensic worst search (frame 66 557, 29 835 expansions, 33 059 inserts, 19.0 M hops = 575 per insertion) spent 96 % of its 231.6 ms in the walk.

### Behavioural constraint (what had to stay exact)

Among equal costs the retail walk stops *after* the last node whose cost ≤ the new cost, so ties are broken by insertion order; the upstream "reverse insertion sort" (`RETAIL_COMPATIBLE_PATHFINDING=0`) picks a different tie
node and changes which path is found, so it is not retail-compatible and was not used. The 5 000-hop cap can leave the list out of order; that behaviour has to be reproduced too.

### Fix

`Core/GameEngine/Include/GameLogic/Pathfinder/OpenListIndex.h` (pure, header-only): while the list is sorted and has fewer than `cap` nodes the cap cannot bind, so the legacy walk ends at the *upper bound* of the new
cost. The index keeps, per 16-bit cost, "present" and the last node of that run (a two-level bit set finds the largest present cost ≤ c). The shared decision `openListInsertionPredecessor`: (1) sorted and < cap nodes ->
from the index (0 hops); (2) otherwise the original walk (with the original dangling-pointer repair); if that walk stops at the cap or repaired anything, the list may be out of order, the index is marked unsorted and not used
again until the list restarts. `PathfindCellList::reset` restarts it; a removed node leaves under the key it was registered with (`PathfindCellInfo::m_openKey`); the original linking code then runs verbatim. In the full run
the fallback was never needed (the open list never reached 5 000 nodes).

### Correctness evidence

* `OpenListIndexTest` (8 tests): a verbatim model of the retail walk is driven side by side with the index on identical random operation sequences (inserts, head pops, arbitrary removals, the
  "improve cost while linked, unlink and re-insert" pattern; clustered, uniform, tie-heavy and full-range costs; caps 1, 2, 3, 7, 25, 60 and 5 000; lists growing past 5 000), comparing the complete list order after every operation.
* `GENERALS_PATH_VERIFY=1`: the original walk is applied, the index only *predicts*; first difference prints frame, cell, cost, both predecessors and list size and `_exit(3)`. **23 474 867 insertions** in the benchmark
  and 33.9 M more on 16 base maps (33 918 373 insertions): 0 differences. (The X64.40 document says "13 maps" in one sentence and "16 base maps" in its results section; the 16-map script and the X64.41 document agree on 16.)
* `GENERALS_PATH_TRACE_HASH=1`: FNV-1a over the whole open-list evolution (every insertion with its predecessor, every removal): original walk = index = `5DE477B0AFDAD8BB`.
* `GENERALS_PATH_LEGACY=1` selects the original walk for an A/B on one binary.
* Not claimed: the repair of a *corrupted* info chain is only reproduced when the walk runs.

### Benchmark (same executable, alternating, 3 rounds each)

Tick: mean 1.228 -> 0.963 ms, p99 8.63 -> 6.30, p99.9 51.6 -> 24.2, worst 265.2 -> 160.4 ms, ticks > 33.3 ms 188 -> 45, > 66.7 ms 53 -> 16, > 100 ms 32 -> 2, derived missed deadlines 389 -> 75, longest
catch-up run 123 -> 10, max backlog 2 910 -> 127 ms. Traced runs (absolute, all searches): internal A* 18 242 -> 5 317 ms (-71 %), worst 231.6 -> 153.7 ms, searches > 25 ms 279 -> 17, forward hops 1.68e9 -> 0.

### Related audit

`PATHFIND_CELLS_PER_FRAME` (5 000) is **not** a CPU bound: it is charged after a search (`cleanOpenAndClosedLists`) and read only between requests in `processPathfindQueue`; one request may expand the whole map
(observed 107 143 expansions, 25 084 cells cleaned for one request). Repeated identical failing requests (335 exhausting failures target 71 distinct destinations from 108 objects) are AI retry cadence = gameplay timing and were
deliberately not touched; a result cache cannot be exact because the cell map's unit flags change every tick.

### Commits

`bbcb68df1`; report `docs/modernization/X64_40_PATH_SEARCH_INVESTIGATION.md`.

---

## P-4. Repeated accepted-cell checks in the A* line walk (X64.41)

**Classification: POSSIBLE upstream relevance (shared pathfinder code; behaviour-identical).**

### Problem / symptom

After P-3 the remaining worst searches were ~104 000-expansion exhausting failures whose time was ~90 % the *line phase*.

### Affected source

`Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp`: for **every expanded cell** `examineNeighboringCells` calls `iterateCellsAlongLine(cell -> goal)` (Bresenham) and runs `examineCellsCallback` on each cell of the straight line
until a cell blocks it or the goal is reached. Per callback: `validMovementPosition`, the hierarchical `isPassable`, `getPinched`, the human extent test, `checkForMovement`, the cliff test, `allocateInfo`,
`setBlockedByAlly(false)`, `costToGoal` and the cost/list update.

### Findings (24 132 searches)

12.4 M expansions -> 12.4 M line walks, 80.2 M callbacks (6.48 per walk; 59 per walk in the two worst searches); **80.8 % of the callbacks were "kept"** (the cell was already on a list at a cost that is not worse) and
78 % of the callbacks in the worst searches re-examined cells already examined in the same search.

### Root cause and the exactness argument

Whether a cell passes the six checks depends only on the *cell* and on the *search parameters* (object, surfaces, crusher, radius, centre-in-cell, human, ignored obstacle), which are constant for one search, plus the
layer of the cell the walk came from. No logic runs inside a search that could change those answers. So the verdict for (cell, search, layer) cannot change while the search runs.

### Fix

Two fields in `PathfindCellInfo` (`m_lineGeneration`, `m_lineLayer`) record the **search generation** and layer in which the cell passed all checks *and* obtained its info record
(`PathfindCell::hasLineVerdict / setLineVerdict`). The generation is `PathfindCellList::searchGeneration()`, advanced by every `PathfindCellList::reset` (every search start), so a verdict from an earlier search -
including cells whose info persists because units occupy them - can never be reused; recycled info is re-initialised to generation 0. On a hit the callback skips the checks and continues with the unchanged
cost handling. Everything with side effects keeps its order; the callback was restructured into a `stop` code evaluated in the original order.

### Correctness evidence

* `GENERALS_LINE_VERIFY=1`: every memo hit also runs the full original checks and must agree (first difference: frame, cell, layer, stop reason, `_exit(3)`). **72 072 137 hits re-verified, 0 differences**; 50 635 424 more on 16 maps.
* `GENERALS_LINE_TRACE_HASH=1`: FNV-1a over every callback decision (stop reason / kept / updated / alloc failure, with the cell): original = memo = `1C551E6BD64E50C9`.
* Open-list evolution digest equal to the P-3 value `5DE477B0AFDAD8BB`; fingerprint unchanged on all 16 default-vs-legacy map pairs. `GENERALS_LINE_LEGACY=1` selects the original callback.

### Benchmark (same executable, alternating x 4)

`path_search` total 5 748 -> 4 535 ms (-21 %), worst contribution 167.3 -> 74.8 ms; internal A* worst 153.7 -> 91.2 ms (traced run), searches > 25 ms 17 -> 6; tick p99.9 24.2 -> 18.1 ms, ticks > 33.3 ms 45 -> 34,
derived missed deadlines 79 -> 48, max backlog 141 -> 91 ms. (One run in four had a single 231.9 ms outlier that was a periodic-script-tick stall, explained later by P-1.)

### Commits

`94428fa89`; report `docs/modernization/X64_41_PATH_LINE_PHASE_AND_R1.md`.

---

## P-5. Negative findings (do not repeat)

| Idea | Result | Evidence |
|---|---|---|
| Snapshot of the enemy's objects in `AIPlayer::computeSuperweaponTarget` (221 evaluations of `getPlayerSuperweaponValue`, each re-walking all team members and recomputing `ThingTemplate::calcCostToBuild`) | Verified **exact** (10 191 evaluations compared, 0 differences, an injected difference caught) but it saved only ~4 ms per 72 000-tick run: computing the target was 6.5 ms of the 1 359 ms the action cost; the real cost was P-1. **Reverted, not recommended.** | `X64_42_SIMULATION_TAIL_OPTIMIZATION.md` section 3 |
| Reusing planar shadow-volume geometry between frames (native renderer) | 98 % cache hits but the volume path was 0.14 of 1.02 ms: no benefit, reverted. (Decal-shadow *grid* reuse, in contrast, saved 0.59 ms/frame and was kept after visual acceptance.) | commits `4caebb9e2`, `f0c347b34`, `268506b49`; `X64_35_PERFORMANCE_OPTIMIZATION.md` |
| Making `PATHFIND_CELLS_PER_FRAME` a CPU bound or caching failing path requests | Not done: the budget is cumulative and after-the-fact (P-3), and the repeats are AI timing | P-3 "Related audit" |
| Threading / worker pool for the simulation | Not built. X64.38 produced the thread-ownership audit, a worker-count policy and a serial reference; the serial algorithmic wins above removed the reason first. | `X64_38_CPU_SCALABILITY.md` |

## P-6. Insufficient evidence (intentionally not claimed)

* "Exhaustive failing path searches" (the remaining ~70-80 ms tail): characterised (≈104 000 pops over ≈21 000 distinct cells and ≈6 M line-walk callbacks) but no exact optimisation is known.
* One-off 30-60 ms script values seen in a 16-map table did not reproduce on re-run and are noise, not an engine regression. Frame 0 (map load) AI supply-source setup of 15-120 ms is a one-time load cost, not a gameplay tick.
