# X64.39 ZoneCalc algorithmic optimization (CP13)

Status: **delivered, serial, bit-identical.** No threads, no worker pool, no gameplay change. The heavy benchmark digest is unchanged
(`74F2CB528BED42ED`, 2 400 checkpoints, 6 of 6 A/B runs plus the verify run). The serial legacy implementation is retained as the verification oracle.

Predecessor: `X64_38_CPU_SCALABILITY.md` (measurement foundation, found ZoneCalc to be ~30 % of the slowest 1 % of ticks and an O(zones)-per-merge scan).

## 1. Repository state at the start

`dev/modern-engine` == `origin/dev/modern-engine` == `4d1db422a` (X64.38), clean tree, `upstream` remote untouched. The X64.38 tooling (tick stats,
fingerprint, harness, heavy replay generator) was present and working.

## 2. The legacy algorithm, exactly (`Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp`)

`PathfindZoneManager::calculateZones` rebuilds all zones of the map whenever a cell type changed. One source for both games (Core); Generals and
Zero Hour differ only in `RTS_GENERALS && RETAIL_COMPATIBLE_PATHFINDING` branches that choose *which* tables a boundary pair is applied to - the merge
primitives are shared and are not touched by those branches.

1. **Label scan** (per 10x10 block, `j` outer / `i` inner): each cell takes a neighbour's label (`applyZone`) or a fresh label (`m_maxZone++`); two different labels met at a
   boundary are merged in the local array `zoneEquivalency[24000]`.
2. **Collapse + relabel**: ascending pass numbers the classes 1,2,3..., then every cell is rewritten through the numbering.
3. **Layers** (bridges) get a zone; **per-block tables** (`ZoneBlock::blockCalculateZones`): four tables (ground-cliff, ground-water, ground-rubble, crusher) per block, indexed
   relative to the block's first zone and storing absolute labels.
4. **Whole-map pair pass**: for every neighbouring cell pair with different zones the rule of each of the six tables (hierarchical, ground-cliff, ground-water, ground-rubble,
   terrain, crusher) decides whether to merge. Six tables of `zoneStorageType` (UnsignedShort).
5. **Flatten**: the hierarchical table, then five `flattenZones` calls (one pass through the hierarchical table, then a second union loop).

The merge primitives:

* `resolveBlockZones(src, target, E, n)`: `src`/`target` are class labels; the lower label wins; every entry equal to the other label is rewritten (scan of the block table).
* `resolveZones(src, target, E, n)`: both arguments are mapped through `E`, `finalZone = E[min]`, then **every** entry equal to either class is rewritten (scan of the
  whole table, up to 24 000 entries).
* `applyZone`: if the target cell has no zone yet it takes `E[source]`; if the classes differ it calls `resolveZones`.

## 3. The proven invariant

Start from the identity (`E[i] = i`). Claim: after any sequence of legacy merges, **`E[i]` is the smallest index of the equivalence class of `i`, and `E[label] = label`.**

Proof by induction on merges. Base: identity. Step: a merge of the classes with labels `a < b` rewrites every entry equal to `b` into `a`; by the hypothesis the labels are the class
minima, so the merged class has minimum `a = min(a, b)`, and `E[a] = a` still holds. `resolveBlockZones` is the same operation on block-relative storage with absolute labels;
`resolveZones` maps its arguments through `E` first, which by the idempotence `E[E[x]] = E[x]` yields the same two labels. Consequences: the final array is a function of the *partition* only,
independent of merge order, and equals the per-class minimum - exactly what a union-find that always keeps the smaller root computes (after flattening `E[i] = find(i)`).

Machine-checked, not only argued: `ZoneEquivalence.ProvenInvariantEveryEntryIsItsClassMinimum` recomputes the classes by brute force after every one of 400 random legacy merges and checks `E[i] <= i`,
`E[E[i]] = E[i]` and `E[i] = min(class)` for all entries.

**Where the invariant does not hold: `flattenZones`.** Its first loop rewrites entries *through a second table* (`arr[i] = hier[arr[hier[arr[i]]]]`), so its second loop calls
`resolveZones` on an array that is no longer idempotent. That function is therefore **kept legacy** (verbatim) in both modes. After the optimization it is 0.8 s of 72 000 ticks (2.1 ms per recalculation).

## 4. Differential oracle (built before touching the engine)

`Core/GameEngine/Include/GameLogic/Pathfinder/ZoneEquivalence.h` holds BOTH implementations in one pure header, so the code the tests exercise is the code the engine runs:
`legacy*` (verbatim semantics) and `minRoot*` (union-find; path halving; smaller root survives; explicit flatten). `Tests/.../ZoneEquivalenceTest.cpp` (9 tests, ~190 ms) drives both with the
same sequences and compares the **resulting arrays bit for bit** (labels, not connectivity), the returned decisions, and the cell zones handed out by `applyZone`:

ascending, descending and alternating chains of 2 000; stars to the lowest and to the highest zone; repeated and already-equal merges; 29 disconnected groups; 36 random sparse / medium / dense
graphs (n = 2 500, up to 9 000 merges, checked every 400 operations); a simulated label scan over random 90x70 terrain grids including zero-zone targets; block tables for first-zone
offsets 0, 1, 7, 100, 4097, 20000 and sizes 2..400; the flatten stage fed from either implementation; and a near-maximum table (n = 24 000). A compiler internal error in the first
form of `legacyResolveZones` (MSVC optimizer, `z_googletest`) was worked around by simplifying the loop; the semantics are unchanged and the test passes.

## 5. In-engine verify mode (`GENERALS_ZONE_VERIFY=1`)

Runs the candidate first and captures everything the pathfinder consumes: `m_maxZone`, `m_zonesAllocated`, **all six tables**, **every cell zone** of the map, the layer zones, and per zone block
(`first zone, count, bridge flag, passable flag` and the four block tables over `count` entries). It then runs the **legacy oracle on the same inputs** and captures again. The legacy result is what stays in the
game state, so a wrong candidate can never alter the simulation. On the first difference it prints segment, index (and cell x/y), legacy value, candidate value and sizes, then `_exit(3)`s. With
verification off nothing extra runs: the mode is read once; the default path is the candidate only. `GENERALS_ZONE_LEGACY=1` selects the original primitives alone (A/B without rebuilding).

Results on the 40-minute heavy benchmark: **388 recalculations compared (386 in-game + the two at map load), 0 mismatches**, digest unchanged. Self-test of the verifier
(`GENERALS_ZONE_VERIFY_INJECT=1` flips one candidate value on the 4th comparison): detected immediately - `ZONE VERIFY MISMATCH at frame 2, comparison 4: segment 'table:hierarchical' index 5: legacy=1 candidate=0 (sizes 4096/4096)`.

## 6. The new algorithm

Min-root disjoint set over the same arrays: `find` with path halving (`E[x] = E[E[x]]`), `union` keeps the smaller root, and each table is flattened (`E[i] = find(i)`) at the three points where
legacy arrays are consumed as final labels: after the label scan, after the block scans, and after the pair pass. `applyZone` returns the neighbour's *root* where legacy returned `E[neighbour]` - the
same value by the invariant. No RNG, no threads, no hashing, no new allocation (the arrays already existed), iteration order unchanged.

Complexity: legacy O(Z) per merge, Z up to 24 000 (entries scanned per merge, six tables, plus the flatten stage); new amortized O(log Z) per merge (path halving without ranks; in practice near constant) and one
O(Z) flatten per table per recalculation.

## 7. Performance - the exact X64.38 heavy benchmark (Twilight Flame, 7 brutal AIs, 72 000 ticks, serial), same executable, alternating legacy/default, 3 rounds each

Absolute milliseconds; legacy vs default (mean of 3 runs; individual runs in `build`-side logs, deviation is a few percent).

| whole simulation tick | legacy | default | change |
|---|---|---|---|
| mean ms | 1.312 | 1.189 | -9.4 % |
| median ms | 0.696 | 0.684 | |
| p90 ms | 1.804 | 1.719 | |
| p95 ms | 2.503 | 2.338 | |
| **p99 ms** | **23.387** | **8.370** | **-64 %** |
| p99.9 ms | 42.596 | 46.885 | within run-to-run noise (41.4-43.2 vs 42.5-55.5) |
| worst ms | 238.0 | 239.1 | unchanged |
| ticks > 16.67 ms | 836 | 450 | -46 % |
| ticks > 25 ms | 580 | 350 | -40 % |
| ticks > 33.333 ms | 139 | 140 | unchanged |
| ticks > 50 ms | 60 | 67 | unchanged (noise) |
| ticks > 66.67 ms | 48 | 52 | unchanged (noise) |
| derived missed deadlines | 306.7 | 311.3 | unchanged |
| derived max backlog ms | 2 473 | 2 433 | unchanged |

**Honest reading:** ZoneCalc was responsible for the *moderate* tail (16-33 ms): p99 falls from 23 ms to 8 ms and the number of ticks above 16.67 ms nearly halves. It was **not** responsible for the *severe* tail
(> 33 ms, the worst tick, deadline misses, backlog): those are unchanged, because they come from path search (a single search reaches 228 ms). The deadline-miss model cannot improve until path search does.

| ZoneCalc (per recalculation, 386 calls) | legacy | default |
|---|---|---|
| total | 9 081 ms (9 029-9 181) | 2 140 ms (2 112-2 162) |
| mean per call | 23.5 ms | 5.5 ms (-77 %) |
| p50 / p95 / p99 | 23.4 / 25.7 / 26.9 ms | 5.5 / 6.4 / 7.0 ms |
| worst | 28.5-32.1 ms | 7.5-7.8 ms |

| ZoneCalc sub-phase (total ms over 386 calls, run 1) | legacy | default |
|---|---|---|
| zone_hierarchy (pair pass + table flatten-all) | 7 082 | **465** (15x) |
| zone_flatten (legacy `flattenZones`, kept) | 793 | 801 |
| zone_label | 781 | 376 |
| zone_relabel | 297 | 290 |
| zone_blocks | 223 | 228 |
| remainder (ZoneCalc exclusive) | 4.0 | 3.6 |

(`zone_label` also fell because its merge primitive is the same function.) After the change the largest ZoneCalc piece is the legacy flatten stage (0.9 % of simulation time).

## 8. Determinism

Digest `74F2CB528BED42ED` over 2 400 checkpoints in: 3 legacy runs, 3 default runs, 1 verify run (and the injection run stops at the injected difference as designed). No first-divergence incident occurred.

## 9. Cache / locality audit (deferred, measured separately later)

`m_map` is `PathfindCell **` where `m_map[x]` is a column (`m_map[i] = &m_blockOfMapCells[i*(height)]`), but every scan iterates `j` outer and `i` inner, so consecutive iterations jump by `height * sizeof(PathfindCell)`.
Swapping loop order is **not** semantics-free in the label scan (label numbers depend on scan order) nor in the block scan; in the pair pass it *is* safe now (final tables are order independent by section 3), but the pair pass is only 465 ms (0.5 %)
after this milestone, so there is nothing left to win there. Deferred; no layout change was made or combined with the algorithmic change.

## 10. Post-optimization subsystem attribution (default path, run 1, exclusive ms, 72 000 ticks)

| phase | total ms | share | p99 per tick | max | share of slowest 1 % of ticks |
|---|---|---|---|---|---|
| ai_object_base_update | 25 355 | 29.3 % | 1.38 | 6.3 | 2.4 % |
| **path_search** | 18 340 | 21.2 % | 3.04 | **228.1** | **73.3 %** |
| object_update_loops | 12 682 | 14.6 % | 0.51 | 54.9 | 5.5 % |
| logic_scripts | 6 895 | 8.0 % | 0.34 | 79.9 | 5.3 % |
| **path_request_entries** | 6 256 | 7.2 % | 2.04 | 88.8 | 9.5 % |
| logic_partition | 4 827 | 5.6 % | 0.21 | 0.7 | 0.6 % |
| ai_player_strategy | 2 862 | 3.3 % | 0.40 | 17.0 | 0.7 % |
| all ZoneCalc phases | 2 140 | 2.5 % | - | 7.8 | ~1.1 % |
| path_reconstruction | 930 | 1.1 % | 0.29 | 3.3 | 0.9 % |

The slowest 1 % of ticks is now defined by **path search (73 %) plus path-request entries (9.5 %)**. ZoneCalc fell from 30 % to ~1 %. No new hotspot appeared; scripts (max 80 ms) and the remaining update modules (max 55 ms) are the next, much smaller, tail contributors.

## 11. Path search - scratch-state audit (audit only, nothing implemented)

What one search touches (from `Pathfinder::internalFindPath`, `processPathfindQueue`, `PathfindCell`, `AIUpdateInterface::doPathfind`):

| state | kind | what a per-search scratch design needs |
|---|---|---|
| `m_openList`, `m_closedList` (members of `Pathfinder`) | shared mutable; `reset()` per search | one pair per search (private lists) |
| `PathfindCellInfo` pool: `s_infoArray[30000]`, `s_firstFree` free-list; `PathfindCell::allocateInfo/releaseInfo` | global mutable; **the per-cell search data lives in the shared map cell** (`cell->m_info`: parent, costs, open/closed flags) | per-search overlay keyed by cell index instead of attaching info to `PathfindCell`; bounded pool per search; same exhaustion semantics (a search that cannot allocate fails) |
| `m_cumulativeCellsAllocated` vs `PATHFIND_CELLS_PER_FRAME` (5 000), added to at `cleanOpenAndClosedLists` | shared counter | the budget couples request N to the cell counts of requests 1..N-1; a parallel scheme must charge costs in queue order and stop where the serial loop would stop |
| `m_ignoreObstacleID`, `m_isTunneling`, `s_useFixedPathfinding`, `m_isMapReady`, `m_logicalExtent` | shared flags set per call | pass in as per-search parameters / snapshot |
| `m_zoneManager` (`getEffectiveZone` ...) | read-only during a search *except* on the frame ZoneCalc runs (the queue returns before servicing) | immutable snapshot is trivially valid within a tick |
| queue `m_queuedPathfindRequests[512]` ring, `m_queuePRHead/Tail` | ordering state | the order defines which requests are served before the budget runs out; keep it serial |
| result commit: `ai->doPathfind(this)` -> `computePath`/`computeAttackPath`, **then immediately `TheAI->pathfinder()->updateGoal(...)`** | **writes goal-unit / position flags into the shared cell map** | **the decisive dependency**: request N+1 reads cell flags that request N just wrote (blocked/occupied checks, `getPosUnit`/`getGoalUnit`) |
| object reads inside a search (`findObjectByID(cell->getPosUnit())`, `getCrushableLevel`, `getControllingPlayer`) | reads of live objects | read-only; valid on a snapshot if no commit intervenes |
| object lifetime / cancellation | requests hold `ObjectID`s resolved at dispatch; `doPathfind` re-checks `m_waitingForPath` | a speculative result must be discarded if the object died or was re-targeted before its turn |
| failure paths | `findPath` returns null; fallbacks (`findClosestPath` ...), `TheGameLogic->incrementOverallFailedPathfinds()` (global counter), `forceCleanCells()` crash recovery (also posts UI + audio) | the failure counter and the recovery are serial side effects; commit order must be preserved |

Conclusion: the kernel *can conceptually* become `immutable snapshot -> private scratch (open/closed lists + cell-info overlay) -> result buffer -> serial commit in queue order`, **but the commit is not independent of the next search**, because committing request N writes the cell flags that search N+1 reads. A correct scheme therefore needs speculation with validation: each search records the set of cells it examined; when it is committed in order, if any earlier commit wrote a flag on one of those cells (or consumed budget differently than assumed), the speculative result is discarded and the search re-run serially. Equivalent results are then guaranteed by construction, and the speculation hit-rate (how often validation fails) is an empirical question that must be measured before building anything. Prerequisites in order: (1) move per-cell search data out of the shared `PathfindCell` (largest change), (2) an in-engine verify mode for path results exactly like the one built here (every path, node by node, against the serial result), (3) the pool. Not started.

## 12. Bounded CRC / replay-mismatch audit

Observation (X64.38, re-confirmed): the user's replay, recorded by this build, reports `CRC Mismatch in Frame 1600` on headless playback. **Independent of this milestone**: the same frame with the new ZoneCalc, with `GENERALS_ZONE_LEGACY=1`, and with the previously staged `x64-37-modern-settings-v4` executable (which contains neither X64.38 nor X64.39).

The 11 `GameClientRandomValue` call sites under `GameLogic/`, classified by whether they reach the state that `GameLogic::getCRC` hashes (each module's `crc()` only calls `UpdateModule::crc`; their `xfer()` - used for save games - does include some of the values):

| site | purpose | state it feeds | in the CRC? |
|---|---|---|---|
| `ActiveBody.cpp:773` | pick a bone for a damage particle system | particle placement | no |
| `EMPUpdate.cpp:255, 272` | spark offset and initial delay of a particle system | particle system | no |
| `ParticleUplinkCannonUpdate.cpp:627` | scorch-mark type ("just client fluff") | client scorch list | no |
| `StealthUpdate.cpp:105` | initial `m_pulsePhase` | drawable opacity pulse; saved (`xfer`), not CRC'd | no |
| `StructureCollapseUpdate.cpp:188, 216` | shudder offset | drawable instance matrix | no |
| `StructureToppleUpdate.cpp:173, 223, 271` | `m_nextBurstFrame`: delay of the next visual/audio burst | saved (`xfer`), not CRC'd; gates `doToppleDelayBurstFX` only (the topple itself uses `m_toppleFrame`) | no |

Result: **none of the 11 reaches the CRC or the authoritative simulation**; the client RNG state itself is excluded from the CRC (`GetGameLogicRandomSeedCRC` is the logic stream only). The X64.38 suspicion that `StructureToppleUpdate` explains the mismatch is therefore **not supported**. It remains a hygiene issue (logic code drawing the client stream; saves contain client-RNG-derived values).

Most likely next investigation (not performed here): the recording was made by the live native client, playback is headless. Candidates are live-only inputs read by logic, e.g. state derived from drawables / bones (headless has none) or from the modern presentation path (ground-interpolation, Render Scale, frame limit), and timing-dependent logic. The decisive first experiment is to play the same `.rep` **non-headless** in the native client: if it also mismatches at frame 1600 the recording itself differs from a re-simulation (live-client dependence); if it does not, the divergence is headless-specific. A DEBUG_CRC build can then localise the first differing object. This remains a **tracked blocker** before simulation multithreading is enabled by default or any multiplayer-determinism claim.

## 13. `scripts/performance/test_compare.py`

Root cause: the interpreter on this machine runs with **safe-path enabled** (`sys.flags.safe_path == True`), so the script's own directory is not put on `sys.path` and `import compare` (a sibling file) fails with `No module named 'compare'`
- from any working directory, and with `python -I`. Fix: the test inserts its own directory (`Path(__file__).resolve().parent`) into `sys.path` before importing. Nothing was weakened or skipped: all 11 tests run and pass from the repo root, from `scripts/performance`, with `python -I`, and under `python -m unittest discover`.

## 14. Remaining risks

* The in-engine verify mode proves equality on the benchmark maps and the two map-load calculations; it does not cover every shipped map. Run `GENERALS_ZONE_VERIFY=1` on other maps before treating the optimization as universal (the benchmark generator takes any base map).
* ZoneCalc recalculations are driven by structure changes. A real match with many more destroyed buildings will recalculate more often; the saving scales with recalculations, not with ticks.
* The severe tail (path search) is untouched, so the deadline-miss model is unchanged.
* R1 (record vs playback mismatch) is still open.

## 15. Next recommendation (X64.40)

1. Path search is now 73 % of the slowest 1 % of ticks and holds every tick above 66 ms. Start with a **serial** investigation of why single searches reach 228 ms (`path_search` p99 3 ms, max 228 ms): a per-search cell/iteration trace for the slowest searches from the existing `PathScope` data, looking for algorithmic waste (like ZoneCalc had) before any scratch-state refactor.
2. Only if the profile then still justifies it, build the verify harness for path results (section 11, item 2) and the per-search scratch refactor.
3. In parallel, spend one session on R1 (play the replay non-headless; DEBUG_CRC localisation).
4. Defer: the legacy flatten stage (0.8 s), the locality swap, any worker pool.

## Follow-up

X64.40 (`X64_40_PATH_SEARCH_INVESTIGATION.md`) took up the path-search recommendation: the serial fix was the retail open-list insertion walk, not a scratch-state refactor. Its R1 section answers the native-vs-headless question left open in section 12 above (the native client diverges at frame 1600 too).
