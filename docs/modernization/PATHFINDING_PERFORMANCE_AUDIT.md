# Pathfinding Performance & Hitch Elimination (X64.pf)

Starting point: `22eb20383`. Scope: serial, exact optimisation of the A* pathfinder plus the measurement tooling. No multithreading, no gameplay change.
Everything below was measured headless on the dev laptop (Intel Core Ultra 5 125U) with seeded AI-vs-AI replays; none of it is multiplayer validation
(real two-PC validation of `2071ad765` is still pending).

## 1. Root cause: why failing searches are expensive

* Zero Hour: 37 644 path searches in the 13-minute `twilight4H` scenario, 1 435 of them failing, and those failing searches cost 21.9 s of the total. Generals, same scenario shape: 9 089 searches, 13 failing, 208 ms.
* A failing search cannot stop early: A* must exhaust everything reachable from the start. Because the line phase (a Bresenham walk from each expanded cell toward the goal) re-visits cells on the lists, one expansion costs far more than its 8 neighbours.
* Most failing searches do not even finish: they end when the shared info pool (`CELL_INFOS_TO_ALLOCATE`) / the per-frame budget (`PATHFIND_CELLS_PER_FRAME = 5000`, `m_cumulativeCellsAllocated`, saved and CRC'd) is exhausted. The result *and the budget charged to later searches in the same frame* depend on the exact free-pool count, so they are part of the authoritative state.
* In Zero Hour a scripted HUNT-state team repeatedly requests paths to targets inside enclosed bases (4x the request count of Generals), so the same unreachable goal is searched again and again.
* Zone recalculation is independent: about 15 ms per recalc, 56 recalcs in the Zero Hour scenario against 149 in Generals. It is not the dominant tail.

## 2. Negative-result cache: evaluated, not adopted

An exact cache was prototyped (kept as `docs/modernization/evidence/pathfinding/negative-cache-experiment.patch`). It recorded 1 434 failures and produced **0 replayable hits**: replaying a failure exactly needs the same free-pool count and the same persistent-info/unit state, which never repeated. Skipping the search, or returning a cached "no path" without reproducing the pool/budget effects, would change simulation results, so it was reverted. Documented in `docs/findings/PERFORMANCE_FINDINGS.md` P-5.

## 3. Accepted changes (all digest-exact)

| Change | Why it preserves semantics |
| --- | --- |
| `iterateCellsAlongLineInline<kProc>`: statement-for-statement copy of `iterateCellsAlongLine` with the callback resolved at compile time | same cells, same order, same callbacks, same stop rule |
| Callback: `allocateInfo`/verdict store only on memo miss; `costToGoal` only on the update path | `allocateInfo` is a no-op on a cell that already has info (memo hit implies info); `costToGoal` is pure |
| Integer cost step (`static_assert` that `0.5*COST_ORTHOGONAL` is an integer) | float sum was exact (16-bit costs) |
| `examineNeighboringCells`: terrain height read lazily; zone-passable test moved to the cost line | both are pure lookups, evaluated only where the result was used |
| `PathfindCellInfo` hot-fields-first layout | layout only; contract tables updated |
| `SimSampler` (`GENERALS_SIM_SAMPLE=<file>`) and `Invoke-PathStress.ps1` | opt-in tools, not used by the game |

**Rejected during validation:** a ground-layer fast path that read `&m_map[x][y]` instead of `getCell()`. It changed the line-decision digest on `twilight4H` (25 extra callbacks, all "movement" stops) although the fingerprint and open-list digests were identical. The cause was not found by reading the code, so it was removed rather than argued; with it gone the digest equals the baseline. A cell-prefetch variant measured slower and was dropped too.

## 4. Determinism results (final executables)

* `twilight4H` (ZH, Enhanced, 50 000 cash): fingerprint `1E5982E5B3A1B86B`, line-decision digest `6AC45762C1656E3D`, open-list evolution digest `5B85CEF9CC94BCB0` - all identical to the pre-change executable.
* `city5H`: line digest `B414DB6B28633F5A` identical (fingerprint `FB748B205FB71351`).
* `GENERALS_LINE_VERIFY` (earlier build of the same memo logic): 1 029 245 684 memo hits re-verified, 0 mismatches.
* Golden AI gates (`Invoke-AiBehaviorGate.ps1 -Quick`, Original digests vs the X64.51 baseline exes): Zero Hour PASS, Generals PASS, every run repeatable.
* Google Tests: Generals 557 passed, Zero Hour 556 passed.
* Python suites: performance 49, native-runtime 329 (7 skipped), rendering 30, diagnostics 5 - all OK.

## 5. Before / after (`twilight4H`, ZH, Enhanced, no diagnostics, two runs each; ms per simulation tick)

| | mean | p50 | p95 | p99 | p99.9 | worst |
| --- | --- | --- | --- | --- | --- | --- |
| before | 2.74 / 2.67 | 1.57 / 1.50 | 15.9 / 15.6 | 19.9 / 19.1 | 27.5 / 27.1 | 58.6 / 60.5 |
| after | 2.61 / 2.57 | 1.51 / 1.50 | 14.4 / 14.4 | 18.1 / 17.8 | 27.9 / 26.2 | 82.0 / 69.5 |

* Path-search time (profiler total): about 26.1 s -> 21.6 s (about 17 %).
* The headless p99 is already below the 25 ms target (stretch 20 ms) in this scenario. The "worst" column is noisy between runs (headless, shared laptop) and is not an improvement claim.
* The headline hitch numbers from the graphical Zero Hour run (p99 about 36 ms, worst about 195 ms) have **not** been re-measured in this milestone; that needs a 7+ minute graphical Skirmish endurance run.

## 6. Generals vs Zero Hour

Zero Hour requests about 4x the searches, has about 110x the failing searches, and spends 21.9 s vs 0.2 s in failures on the same scenario family. Cause: HUNT-state scripted teams in Zero Hour, not map size or unit counts. Zone recalcs are more frequent in Generals (149 vs 56) but cheap enough not to matter.

## 7. Multithreading feasibility (design only, nothing implemented)

Not justified. Searches share the info pool, the open/closed lists live inside the cells, and `m_cumulativeCellsAllocated` charges a per-frame budget in request order; results must be consumed in queue order and interact with replay and save/load. Making that parallel requires partitioned pools and a replayed budget, which changes results. With serial p99 already under target, there is no case for it.

## 8. Not done / open

* Upstream (TheSuperHackers) pathfinding commit audit: not performed in this milestone. Candidates to classify with `scripts/diagnostics/Classify-UpstreamCommits.py`: `1abf73d5d`, `21323414f`, `28bb4554f`, `8ed77a761`, `a4ac54b83`.
* Incremental zone recalculation: not built (correctness/invalidation model not understood well enough; recalcs are not the dominant tail).
* Graphical re-measurement of the Zero Hour tick tail (see section 5).
* Next non-pathfinding candidate: `PartitionManager::getClosestObjects` is about 9 % of sampled time (AI target queries).
* Secondary hitches (terrain full update about 31-33 ms on structure height changes, first-use assets about 30-40 ms) are tracked only.
* Path-result golden tests at unit level were not added; coverage is the replay digests above.

## 9. Tools

* `scripts/performance/Invoke-PathStress.ps1`: scenarios `twilight4M/4H/7H`, `desert3H`, `wasteland5M`, `city5H`, fixed seeds. Use absolute paths (the script changes directory into the game stage), and set `GENERALS_*` environment variables in the calling shell.
* `GENERALS_SIM_PATHFAIL=1`, `GENERALS_SIM_PATHS=1|2|3`, `GENERALS_LINE_TRACE_HASH`, `GENERALS_PATH_TRACE_HASH`, `GENERALS_LINE_VERIFY` (existing) and `GENERALS_SIM_SAMPLE=<file>` (new sampler).
