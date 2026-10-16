# X64.40 Path-search deep profiling + serial optimization (CP14)

Status: **delivered, serial, exact.** The retail A* open list now finds its insertion point through an index instead of walking the list from the head. The
open-list evolution is identical to the original walk at every one of 23.5 million insertions of the heavy benchmark, the authoritative digest is unchanged
(`74F2CB528BED42ED`), and no thread, worker pool or speculation was added. The severe tail collapsed: worst tick 265 -> 160 ms, ticks > 33.3 ms 188 -> 45,
derived missed deadlines 389 -> 75, maximum backlog 2.9 s -> 127 ms. Also here: the R1 replay question answered (native client diverges too), the
stage4b1/stage4b3 guard investigation, and ZoneCalc/open-list verification on 16 base maps.

Predecessors: `X64_38_CPU_SCALABILITY.md`, `X64_39_ZONECALC_OPTIMIZATION.md`.

## 1. Repository state at the start

`dev/modern-engine` == `origin/dev/modern-engine` == `c51dc603d` (X64.39), clean tree.

## 2. Instrumentation (all env-gated, default off)

| switch | effect |
|---|---|
| `GENERALS_SIM_PATHS=1` | per-search trace in the headless runner: every `PathScope` sample of every path kind (request, internal A*, ground, hierarchical, closest, attack, safe, patch, move-away, queue, dispatch, reconstruction, cleanup, zones, ...) is kept in a compact record; the report prints, per kind, count / total / mean / p95 / p99 / p99.9 / max and the counts above 5 / 10 / 25 / 50 / 100 / 200 ms, the 25 slowest searches (frame, requesting ObjectID, outcome, head pops, open-list inserts, cell infos allocated / failed, forward / reverse insertion hops, cells cleaned, layer, human, crusher, from -> to), the work distribution of the A* kernel (p50 / p95 / p99 / p99.9 / max), correlations of search time with each work counter, a breakdown by number of cells expanded, the outcome split (path / closest / null before work / **null after work = the open list was exhausted** / zone rejected), requester class, and the hierarchical-fallback split; `<base>_paths.csv` has every search sample >= 1 ms |
| `GENERALS_SIM_PATHS=2` | additionally samples the neighbour and line phases of 1 in 64 A* iterations, with the share spent in open-list insertion |
| `GENERALS_PATH_LEGACY=1` | the original head-first insertion walk only (no index) - A/B on one binary |
| `GENERALS_PATH_VERIFY=1` | the original walk is applied; the index only PREDICTS; the first difference prints frame, cell, cost, both predecessors and the list size and `_exit(3)` |
| `GENERALS_PATH_TRACE_HASH=1` | FNV-1a over the complete open-list evolution (every insertion with its predecessor, every removal), printed as "open list evolution digest" - comparable between modes |
| `GENERALS_AUTOPLAY_REPLAY=<file>` | (diagnostic, in `GameLogic::update`) starts that replay once in the NORMAL native client a few seconds after the shell map runs, exactly as the Replay menu does, and prints `AUTOPLAY logic frame N state crc` every 300 frames with unbuffered stdout |

The trace adds clock reads to every search (the tail statistics are therefore taken from separate non-traced runs; trace runs are used for attribution).
Thresholds are reported, not logged; only samples >= 1 ms are written to disk.

## 3. Baseline (X64.39 code, original insertion walk; trace run of the same heavy benchmark: Twilight Flame, 7 brutal AI, 72 000 ticks)

| kind | count | total ms | mean | p95 | p99 | p99.9 | max | > 5 ms | > 10 | > 25 | > 50 | > 100 | > 200 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| request (findPath) | 24 142 | 18 924 | 0.78 | 1.08 | 26.7 | 113.8 | 232.5 | 562 | 442 | 279 | 46 | 30 | 2 |
| internal (A*) | 24 132 | 18 242 | 0.76 | 0.91 | 26.7 | 112.2 | 231.6 | 541 | 440 | 279 | 46 | 30 | 2 |
| hierarchical | 38 244 | 1 147 | 0.03 | 0.13 | 0.47 | 1.14 | 7.2 | 6 | 0 | 0 | 0 | 0 | 0 |
| closest | 2 948 | 2 154 | 0.73 | 1.16 | 5.9 | 88.0 | 93.0 | 39 | 18 | 15 | 15 | 0 | 0 |
| attack | 11 971 | 5 063 | 0.42 | 2.43 | 3.8 | 5.6 | 8.1 | 35 | 0 | 0 | 0 | 0 | 0 |

Search shape (A* kernel, 24 132 searches): median search expands **2** cells, p95 908, p99 22 495, max 107 143 (cell-info pool is 30 000, allocated p99 22 144, max 25 015);
inserts p50 8 / p99 22 518 / max 116 166; **forward insertion hops p99 3.1 million, p99.9 9.5 million, max 19.0 million**. Correlation of search time with: forward hops **0.944**,
open-list inserts 0.861, head pops 0.857, cell infos 0.772. By cells expanded: the 343 searches that expand >= 20 000 cells are 1.4 % of searches and **72.7 %** of A* time. By outcome:
**580 searches that exhaust the open list and return no path = 12 694 ms = 70 % of all A* time**, 21 489 successful searches = 5 547 ms. 523 requests (2 132 ms) fall back from the
hierarchical search; 106 of those return no path. Requester: 24 131 computer players, 1 human (the benchmark is AI against AI). The mean walk is 97 hops per insertion
(2.29e9 hops over 23.5 M insertions in the full run), 120 hops per insertion for the A* kernel.

## 4. The worst search, forensically (frame 66 557, ObjectID 15 349, ground locomotor, crusher, computer player)

From (345,3944) to (3995,1645) across the map, outcome **null_after_work** (the goal is unreachable; the search exhausts the reachable region):

| quantity | value |
|---|---|
| inclusive time | **231.6 ms** (an outlier search of 234 ms in the phase-sampled run) |
| cells expanded (head pops) | 29 835 |
| open-list inserts | 33 059 (final list length about 33 059 - 29 835 = 3 224 nodes) |
| cell infos allocated / allocation failures | 24 374 / 1 041 (the 30 000-entry pool ran dry: the search itself ended through failures and an empty open list, not through any budget) |
| **forward insertion hops** | **19 023 070** = **575 per insertion** |
| time by phase (sampled, scaled) | neighbour phases 236 ms, of which **open-list insertion 226 ms (96 %)**; line checks 9 ms |

So 96 % of this 231 ms search was the sorted-linked-list insertion walk: a list that is re-walked from the head on every insert (a regression over all searches puts a hop at roughly 10-12 ns, pointer
chasing through the shared info pool). Nothing else about the search is slow: with the index the same search is no longer among the 25 slowest (all of those are above 15 ms).

Other very slow searches (100k expansions): 103 920 pops / 4.2 M hops = 200 ms, and 107 143 pops = 188 ms: here insertion is only ~25 % and the **line phase (iterateCellsAlongLine ->
examineCellsCallback, the Stage4B.1 hoist target) is ~75 %** - at roughly 1.5 us per expanded cell.

## 5. What `PATHFIND_CELLS_PER_FRAME` (5 000) actually means

Verified in `processPathfindQueue`, `internalFindPath`, `cleanOpenAndClosedLists`:

* It is incremented in exactly one place: `cleanOpenAndClosedLists()` adds the number of cells it releases **after** a search has finished (`m_cumulativeCellsAllocated += count`).
* It is read in exactly one place: the `while` of `processPathfindQueue` between two requests (`while (m_cumulativeCellsAllocated < 5000 && queue not empty)`), reset to 0 at the start of each queue run.
* **It is cumulative across requests within one tick and charged after the fact.** It decides how many *additional* requests start that tick, never how long one search may run.
* **There is no bound inside a search**: `internalFindPath` and the other searches loop `while (!m_openList.empty())` until the goal is popped or the list is empty. One request may
  expand the whole map; the observed maximum is 107 143 expansions and 25 084 cleaned cells charged for a single request (5 x the budget).
* Not charged: head pops, neighbour evaluations, line-of-sight checks, open-list insertion hops, path reconstruction, the hierarchical pre-search, zone calculation. A frame that recalculates zones returns before servicing any request.
* A failure or fallback does not reset or bypass it; it simply charges the cells of the failed search (a failing exhausted search charges 20-25 k cells and ends that tick's queue).
* The only hard bound on a single search is the shared 30 000-entry cell-info pool (`CELL_INFOS_TO_ALLOCATE`): when it is exhausted allocation fails and the search winds down.
* The same constant is *reused* as the cap of the retail insertion walk (5 000 hops per insertion). That is a coincidence of reuse, not a CPU-work budget.

The budget is therefore **not** a CPU-work bound (documented; the number was not changed).

## 6. Complexity / data-structure audit

* Open list: doubly linked, ascending by 16-bit total cost; the retail insertion starts at the head every time and walks past every node with cost <= the new cost, capped at 5 000 nodes
  (`forwardInsertionSortRetailCompatible`). **O(open-list length) per insert**; 76-96 % of A* time in the expensive searches (phase-sampled estimate 76 % of all search time, 81 % in the searches > 20 ms).
* The upstream "reverse insertion sort" (`reverseInsertionSort`, `RETAIL_COMPATIBLE_PATHFINDING=0`) addresses the same cost but picks a different node among equal-cost ties and is not
  retail-compatible; it was not used.
* Stage4B.3 (`STAGE4B3_RETAIL_OPEN_LIST_INVESTIGATION.md`) found no exact constant-factor win and listed prefix/rank hints as future work. This milestone is that work: an index with an exactness proof.
* Cell info allocation (pool free list), cleanup (one pass over open + closed) and reconstruction are linear and small (cleanup 362 ms, reconstruction 1 078 ms over the whole benchmark).
* Cache-hostile access: line checks and neighbour evaluation walk `m_map[x][y]` columns; not changed (see next steps).
* Repeated identical work: 335 exhausting failures >= 1 ms target only 71 distinct destination cells, issued by 108 objects (one object 16 times): the AI re-requests unreachable
  destinations as units arrive. **That is AI retry cadence = gameplay timing, deliberately not touched.** A result cache cannot be exact because the cell map's unit flags change every tick.

## 7. The optimization: `OpenListIndex` (exact by construction)

`Core/GameEngine/Include/GameLogic/Pathfinder/OpenListIndex.h` (pure, header-only, unit-testable). While the list is sorted and has fewer than 5 000 nodes the 5 000-hop cap cannot bind, so the
legacy walk ends at the **upper bound** of the new cost: directly after the last node whose cost is <= the new cost. The index keeps, per 16-bit cost, "present" and the last node of that run
(a two-level bit set finds the largest present cost <= c in a few instructions), and `noteInserted` / `noteRemoved` keep the run tails correct (a node removed after its cost was improved leaves under the key
it was registered with: `PathfindCellInfo::m_openKey`). The shared decision `openListInsertionPredecessor`:

1. index sorted and fewer than `cap` nodes -> answer from the index (0 hops);
2. otherwise the original walk (with the original dangling-pointer repair); if that walk stops at the cap, or repaired anything, the list may be out of order and **the index is marked unsorted and not used again until the list restarts**.

Wiring (`AIPathfind.cpp`): `PathfindCellList::reset` restarts the index with the list (registering the start node); `forwardInsertionSortRetailCompatible` takes the predecessor from the shared decision and then runs
the **original linking code verbatim**; `removeFromOpenList` maintains the tails. The legacy fixed-pathfinding switch (`s_useFixedPathfinding`) disables the index permanently. Win32 and Zero Hour share the code.
In the full 72 000-tick run **no insertion ever needed the walk fallback** (the open list never reached 5 000 nodes).

## 8. Exact-result verification

* **Differential oracle** (`OpenListIndexTest`, 8 tests, 2.1 s): a verbatim model of the retail walk is driven side by side with the index-assisted insertion on identical random operation sequences
  (inserts, head pops, arbitrary removals, the engine's "improve the cost while linked, then unlink and re-insert" pattern; clustered, uniform, tie-heavy and full 16-bit cost ranges; caps of 1, 2, 3, 7, 25, 60 and the real 5 000;
  lists growing past 5 000 nodes). The complete list order is compared after every operation.
* **In-engine verify mode**: **23 474 867 insertions** in the heavy benchmark compared against the original walk, 0 differences; plus **33.9 million** more on 13 maps below.
* **Open-list evolution digest** (every insertion with its predecessor and every removal): original walk = index = `5DE477B0AFDAD8BB`.
* **Authoritative fingerprint** `74F2CB528BED42ED` (2 400 checkpoints) in every run of every mode (legacy, index, verify, trace, ZoneCalc legacy / verify combinations).
* Not claimed: the dangling-pointer repair of a corrupted info chain is only reproduced when the walk runs; in the index path it is not checked (that state is a corruption which the existing `releaseOpenList`
  detector already turns into the permanent fixed-pathfinding mode, which also disables the index).

## 9. Benchmark: before / after (the exact heavy benchmark; same executable, `GENERALS_PATH_LEGACY=1` vs default, 3 alternating rounds, no tracing)

| whole tick | original walk | index | change |
|---|---|---|---|
| mean ms | 1.228 | **0.963** | -22 % |
| median | 0.689 | 0.688 | |
| p90 | 1.747 | 1.641 | |
| p95 | 2.393 | 2.116 | |
| **p99** | 8.626 | **6.303** | -27 % |
| **p99.9** | 51.604 | **24.167** | -53 % |
| **worst** | 265.2 (263-267) | **160.4** (156-164) | -40 % |
| ticks > 16.67 ms | 462 | 90 | -81 % |
| ticks > 25 ms | 375 | 68 | -82 % |
| **ticks > 33.333 ms** | 188 | **45** | -76 % |
| **ticks > 50 ms** | 76 | **34** | -55 % |
| **ticks > 66.67 ms** | 53 | **16** | -70 % |
| **ticks > 100 ms** | 32 | **2** (2-3) | -94 % |
| **derived missed deadlines** | 389 | **75** | -81 % |
| longest catch-up run | 123 | **10** | |
| **max backlog** | 2 910 ms | **127 ms** | -96 % |

(The model is derived from measured durations run back to back; it is not a paced run. The same-session original-walk numbers are a little slower than the X64.39 default row
(mean 1.189, worst 239, > 33 ms 140, missed 311, backlog 2 433 ms) - machine state differs between sessions; the comparison above is within one session.)

Path search specifically (traced runs, absolute ms, all searches of the 40-minute run):

| | before | after |
|---|---|---|
| internal A* total | 18 242 ms | **5 317 ms** (-71 %) |
| `findPath` requests total | 18 924 ms | 5 900 ms |
| internal A* mean / p95 / p99 / p99.9 | 0.756 / 0.91 / 26.7 / 112.2 ms | **0.220 / 0.61 / 5.1 / 15.3 ms** |
| internal A* worst | 231.6 ms | **153.7 ms** |
| A* searches > 25 / 50 / 100 ms | 279 / 46 / 30 | **17 / 6 / 2** |
| exhausting failures (580) | 12 694 ms | **2 528 ms** |
| searches (count, unchanged) | 24 132 | 24 132 |
| forward hops | 1.68e9 (A* kernel) | **0** |

Top slowest searches (ms) before -> after: the previous top 25 (112-232 ms, nearly all dominated by 4-19 M hops) are now below ~100 ms; the new top is **153.7, 152.0, 94.2, 70.5, 64.0, 50.3, 46.2, 38.7, 37.2, 34.8** - the first
two are the 100 k-expansion searches whose cost is the line phase, not insertion.

## 10. Post-optimization attribution (default path, mean of 3 runs, exclusive time)

| phase | total ms | max ms | share of the slowest 1 % of ticks (before -> after) |
|---|---|---|---|
| **path_search** | 19 636 -> **5 491** | 255.6 -> **153.5** | 72.1 % -> **29.9 %** |
| path_request_entries | 6 716 -> 2 225 | 103.6 -> 14.9 | 9.9 % -> 3.7 % |
| object_update_loops | 12 816 -> 12 863 | 61.8 -> 83.8 | 6.3 % -> **18.1 %** |
| logic_scripts | 7 049 -> 6 995 | 125.9 -> 88.4 | 5.9 % -> **15.8 %** |
| ai_object_base_update | 25 358 -> 25 066 | 5.3 -> 4.7 | 2.2 % -> 5.8 % |
| ZoneCalc (all) | ~2 150 | 7.8 | ~1 % |

Path search no longer owns the tail by itself; **the next contributors are the remaining update modules (max 84 ms) and scripts (max 88 ms)**, then the two 100 k-expansion searches. The remaining slow
searches are line-check-bound: 90 % of their time is the line phase.

## 11. Additional-map verification (ZoneCalc `GENERALS_ZONE_VERIFY` + open list `GENERALS_PATH_VERIFY`, 14 game minutes, 7 brutal AI unless noted)

16 base maps, each in three runs (default / both legacy / both verify): **0 ZoneCalc mismatches over 1 649 recalculations, 0 open-list mismatches over 33 918 373 insertions, all 16 default-vs-legacy fingerprints identical, all exit codes 0.**
Maps: Alpine Assault, Armored Fury, Bridge Busters, Cliff, Dark Mountain, Fallen Empire, Fortress Avalanche, Golden Oasis, Homeland Alliance, Lone Eagle (3 AI), Rogue Agent, Silent River, SwissMP, Tournament Island (3 AI), Whiteout, Scorched Earth.
Caveat, stated plainly: on Bridge Busters, Cliff and SwissMP the AIs barely act (2 zone recalculations, 0 open-list insertions) - they prove nothing beyond the load-time calculation; this is not a proof for all content.

## 12. R1: the replay CRC mismatch at frame 1600

* **Decisive experiment performed** (authorized, bounded): the same `.rep` played in the NORMAL native client (via the opt-in `GENERALS_AUTOPLAY_REPLAY` hook, no interaction), progress printed:
  logic frames 300 ... 1500 pass, then `CRC Mismatch in Frame 1600` - **the native client diverges at exactly the same checkpoint as headless playback.** Outcome **A**: a record-versus-resimulation divergence, **not** a headless-versus-client difference.
  (A first attempt printed nothing because the engine writes the mismatch with `printf` into a buffered redirected stdout and then pauses the game; stdout is now unbuffered in the hook.
  Also note: `generalsv.exe -replay x.rep` WITHOUT `-headless` still simulates headlessly in the normal-launch profile, because that profile sets `m_headless`; native playback needs the menu or the new hook.)
* Replay content (parsed): 29 193 recorded messages: 29 128 `MSG_SET_REPLAY_CAMERA`, 35 `MSG_LOGIC_CRC`, and only **25 gameplay commands** from the one human: 12 `CREATE_SELECTED_GROUP`, 5 `DESTROY_SELECTED_GROUP`, 7 `DOZER_CONSTRUCT` (frames 204, 1294, 1347, 1495, 1564, 1709, 2028), 5 `QUEUE_UNIT_CREATE`, 1 `SET_RALLY_POINT`.
* Experiments on edited copies (a command replaced by a discarded no-op): removing the 1495 construct changes the first mismatch to 1500 (so the state at checkpoint 1500 includes it and matched); removing all selection-group
  commands breaks the match already at 300 (selection groups are real inputs: `DOZER_CONSTRUCT` acts on the selected group); removing the 1518 group or the 1564 construct leaves it at 1600.
  The divergence therefore started in frames 1501-1600, with the 1518 group and the 1564 construct as the only recorded inputs in that window.
* Presentation: playback in the full native client (D3D11 renderer, HUD, interpolation) and the headless simulation agree with each other, so nothing in the presentation path feeds the simulation **during playback**. A
  presentation dependence at **record** time cannot be excluded from this data. The earlier client-RNG audit (X64.39) stands.
* **Not localised further**: `-ReplayCRCInterval` only exists in `DEBUG_CRC` builds (the header's CRC interval is 100 in release), so a per-frame reference needs either a DEBUG_CRC build or a small release-side interval override.
  Next investigation (one concise human step, when wanted): record a short live session that does only "build one structure with the dozer, wait 40 s" with a CRC interval of 1, play it back with `GENERALS_SIM_BENCH`
  (the runner prints the first differing frame), then add per-subsystem CRC lines for that frame. The mismatch remains a **tracked blocker** before multithreading by default or any multiplayer-determinism claim.

## 13. The stage4b1 / stage4b3 / ordinary-menu guards

* `scripts/performance/test_stage4b1_reference.py` (whole-file token equality of `AIPathfind.cpp` against Stage4B.1's base): **stale by construction, not by accident.** It requires the entire pathfinder file to equal a 2026-10-09 snapshot after
  undoing one change; it first failed at `10b4503e9` (X64.35 profiling scopes, tokens 68 340 vs 68 333) and every later authorized change (X64.38 zone scopes, X64.39, X64.40) widens the difference. It is **not** edited here and not silently baselined:
  it stays red and is reported. Recommended: replace the whole-file comparison with per-function allow-listed comparison (each function equal to the base unless it carries a documented later change) - a separate, explicitly authorized change.
* `scripts/performance/test_stage4b3_reference.py` guards the retail insertion body and the pathfinder representation. This milestone changes both on purpose, so it was **re-targeted, not relaxed**: the frozen reference still equals the accepted baseline token for
  token; the production function must still contain the baseline's linking block and dangling-pointer repair verbatim; the fallback walk in `OpenListIndex.h` must still be the baseline loop condition with the 5 000 cap; the differential tests must exist;
  and the representation may differ from the baseline **only** by the exact listed X64.40 additions. Mutation-checked: altering the loop condition, adding any other field, or touching the linking block makes it fail.
* `scripts/native-runtime/test_ordinary_menu_contract.py` lists the reviewed additions to `Shell.cpp`. The first form of the autoplay diagnostic added lines to `Shell::update` and failed it; the diagnostic was moved to `GameLogic::update`
  and `Shell.cpp` is byte-identical to the previous commit again.

## 14. Rejected / deferred

| item | decision |
|---|---|
| lowering `PATHFIND_CELLS_PER_FRAME` / bounding a search | rejected: changes which requests are served and when (gameplay timing) |
| caching failed searches / skipping unreachable destinations | rejected: not exact (unit flags change every tick); AI retry cadence is gameplay |
| spreading a long search over several ticks | rejected (section 15 of the brief): changes command latency |
| reverse insertion sort (`RETAIL_COMPATIBLE_PATHFINDING=0`) | rejected: different tie order = different paths |
| generation counters instead of clearing | not needed: clearing is per-cell release, 362 ms total |
| line-callback / neighbour cost, `m_map` column-major locality | deferred (next step A) |
| worker pool, parallel or speculative path search | **not justified yet** (section 15) |

## 15. Is parallel pathfinding still justified?

Not now. Path search fell from 21 % to 6.5 % of simulation time and from 72 % to 30 % of the slowest ticks without a thread; the worst search is 154 ms and comes from two ~100 k-expansion
exhausting searches whose cost is serial per-cell work (about 1.5 us per expansion). The earlier dependency (a committed request writes cell flags a later request reads) still makes parallel
search a speculation-with-validation project; the prerequisites (per-search scratch for the open/closed lists and cell infos, a path-result verify mode) are large, and the remaining tail
(object updates 84 ms, scripts 88 ms) is not parallelisable at all. Serial work remains the better lever.

## 16. Remaining risks

* The index assumes the open list is only modified through `putOnSortedOpenList` / `removeFromOpenList` / `reset` (verified by grep of every list manipulation in the file); a future edit that links cells another way must call the index or disable it.
* Verified on 14 maps with active AI (13 base maps plus Twilight Flame); the 3 maps where the AI stays idle add no pathfinding coverage.
* R1 remains open; no multithreading or multiplayer-determinism claim is made.
* `test_stage4b1_reference.py` is red (pre-existing, explained above).

## 17. Recommendation for X64.41

**A. another serial pathfinding optimization**, aimed at the line phase: examine `iterateCellsAlongLine` / `examineCellsCallback` / `validMovementPosition` / `checkForMovement` for repeated object lookups and cache-hostile cell access in
the 100 k-expansion exhausting searches (about 90 % of their remaining time), with the same oracle approach (hash of every pop and every path) - then re-measure. In parallel, one R1 session (DEBUG_CRC or interval override + a short recorded build).
