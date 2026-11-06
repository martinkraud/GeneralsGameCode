# X64.performance-final: congestion, HUNT failures, Present stalls and the final performance baseline

Starting point `954b1c554`, branch `dev/modern-engine`, 2026-11-06. This is the closing audit of the broad performance phase. It changes **no authoritative behaviour and no engine performance**: it adds opt-in
diagnostics, explains the last measured tail, records the reference baseline, and ends with the exit decision (section 8).

Real two-PC multiplayer validation of `2071ad765` remains pending; nothing here is multiplayer evidence.

Evidence: [`evidence/perf-final/`](evidence/perf-final/). Tools: `GENERALS_SIM_PATHFAIL=1` (extended), `GENERALS_PRESENT_STALL_LOG` (new), `scripts/performance/Analyze-PathFailures.py`.

## 1. The failing path searches are not congestion

Method: every search that exhausts its open list is analysed after the fact (read-only). New in this milestone: two breadth-first floods from the start cell with the A*'s own neighbour and diagonal
rules and an **unlimited** pool (one ignoring units, one applying the search's unit rules), per-(requester, goal) recurrence and recovery tracking, a counter of refused cell-info allocations, and the
share of each failure spent after the pool ran dry. Scenario: Zero Hour `twilight4H`, Enhanced AI, 50 000 cash, 23 400 ticks, `GENERALS_SIM_CLONE=<k>@8000` for 6x / 8x mobile units.

| | 1x | 6x | 8x |
|---|---|---|---|
| failing searches | 1 435 | 862 | 1 421 |
| time in failing searches | 17.1 s | 9.5 s | 14.7 s |
| mean cost of one failure | 11.9 ms | 11.0 ms | 10.3 ms |
| requested by the HUNT state | 99 % | 99 % | 98 % |
| goal reachable with an unlimited pool (class "budget") | 100 % | 99 % | 99 % |
| goal reachable only after ignoring units (class "temporary, blocked by units") | 0 % | 1 % | 1 % |
| statically unreachable (terrain or footprint) | 0 | 0 | 0 |
| failures during which a cell-info allocation was **refused** | 1 432 of 1 435 | 860 of 862 | 1 421 of 1 421 |

(The 6x / 8x rows of the previous audit, 1 108 / 3 034 failures, came from a different AI mode; in the default Skirmish configuration used here the counts are as above. In that other mode combat-movement
states contributed 34 %, and the mechanism below is the same.)

**Root cause.** The A* shares one pool of 30 000 `PathfindCellInfo` records with the cells that hold a unit or a goal. A search that has to flood a large basin before it can leave it (cliff ridges, the
goal behind a long detour) uses every free record and then stops: a failed search ends holding 14 400 - 21 200 cells (mean 16 100 at 8x, 19 500 at 1x; an earlier build of the probe that counted the free records before each search measured the same 14 600 - 21 400), so the remaining
9 000 - 15 000 records belong to unit-occupied cells. Only 2 % of a failing search's time is spent after the first refused allocation, so the cost is the flood itself, not a long tail. More units
leave fewer free records, so more searches that were barely successful become failures. This is **an existing engine budget, not collision with friendly units**: the goal is reachable, no static terrain
cause exists, and units block the route in about 1 % of the failures (the ally-blocking frontier counts are tiny). Zero Hour's HUNT state chooses far-away goals much more often than Generals (13
failing searches in the same scenario, 0.2 s).

Cost: at 1x the failing searches are about **a quarter of the Zero Hour simulation time** (17.1 s of ~64 s) and the whole >10 ms tail, but only 9-12 ticks per 23 400 exceed 33.3 ms.

## 2. Is any of it reusable? No (rejected)

* **Temporary vs permanent.** Class "temporary" is 1 %; after a failure the same (requester, goal) pair practically never succeeds again: of 549 failing pairs at 1x none ever succeeded later (1 of 539 at 8x,
  after 830 frames). The failure is effectively permanent for that hunter and goal, but it is permanent only *under the free-pool size at that moment*.
* **Exact repeat.** 62 % of the failures repeat a (requester, goal) pair; in about 80 - 85 % of those the requester had not moved. The gap between consecutive failures of a pair is 61 frames at the median (2 hunt scans,
  p10 61, p90 170, maximum 12 357). But **zero** of the 886 repeats had zero pathfinder cell mutations in between (units move, flags change every frame), and the outcome depends on the free pool, the
  ally-moving and ally-blocking cost terms and the unit-occupied cells. A cached "this will fail again" would therefore not be provably identical to running the search. The previous milestone's
  exact-state negative cache failed for the same reason; no narrower congestion invariant exists because congestion is not the cause.
* **Early termination.** Only 2 % of a failure runs after the pool is exhausted, so cutting that tail saves nothing.
* **Changing the budget** (a larger pool, a cap on a search's cells) would change which requests succeed: forbidden by the brief and not an optimisation.
* **Changing HUNT** (skipping targets that failed before) changes behaviour: forbidden.

Result: **no exact optimisation exists** for the failing searches; documented and stopped. The per-cell cost of the flood was already reduced by X64.pf (about 0.6 us per cell).

## 3. HUNT target to failed path chain

Target selection (`AI::findClosestEnemy`, now the exact staged scan) -> `AIAttackApproachTarget` path request -> exhaustion -> the unit falls back to the hunt machine and the next scan 30 frames later
chooses the same or another far victim. Repeated pairs: 886 of 1 435 (10.3 s of the 17.1 s). World state between attempts always changed (zero mutation-free repeats). Reachability under unlimited pool
did not change; what changes is only how many records are free. No existing state records enough to avoid the repeat exactly. Enhanced AI strategy was not touched.

## 4. Present stalls (450-510 ms): not reproducible here, and probably not specific to Present

* ETW (`wpr` / `xperf` are installed) needs an elevated session or the Performance Log Users group; this session has neither and was not elevated. **No ETW trace was taken.**
* Instead the D3D11 backend gained an opt-in probe, `GENERALS_PRESENT_STALL_LOG=<file>` (`GENERALS_PRESENT_STALL_MS`, default 100): when a `Present` takes longer than the threshold it records the wait, the
  thread's CPU time during it, the result code, VSync/tearing, foreground/iconic/visible state, idle input time, pending GPU timestamp slots, DXGI frame statistics, DWM composition timing (late / dropped /
  missed frames, refresh rate), video-memory usage and budget, the previous four Present times, working set and page faults. Off, it costs one cached boolean per frame.
* Five seeded graphical runs with the probe (one with hitch-category profiling): **no Present longer than 60 ms in any of them** (0 of 5, against 3 of 8 in the previous milestone); the probe file stayed empty.
* The profiled run did show 22 hitches of 90 ms or more (up to 2 240 ms); **21 of them had a thread CPU time of 0-31 ms inside an interval of 100-2 240 ms**, i.e. the thread was not running, and they
  landed in the logic tick (13), predraw (about 6) and draw (2) with `Present` below 0.5 ms every time. A Generals run showed the same class (a 322 ms tick whose headless worst is 48 ms, a 543 ms draw).
  This is the pattern of the process being descheduled or paused (OS, security software, power management), landing wherever the thread happens to be; the earlier "inside Present" stalls fit it. Not proven.
* Classification: **external / environment investigation**; to be repeated on a native session with an elevated ETW trace (CPU scheduler + DXGI + DWM) if it appears for a player.

## 5. First-use hitch: attributed, not worked

Every run has one hitch of 100-170 ms ~2.5 s after start (`t=2.45 s`, interval 162 ms: predraw 34 ms, **draw 128 ms**, thread CPU 125 ms of 128 ms, `present` 0.2 ms, 104 textures and 146 vertex buffers created,
36 612 page faults, working set 330 -> 445 MB). It is CPU-bound creation and first touch of the world's textures and buffers in the first world draw (asset upload and memory commit), not shader
compilation (submit 17 ms), not Present, not disk or sound. Prewarming would move the same work into the loading screen (more load time, 100+ MB committed earlier); no prewarm was implemented. **Beta polish.**

## 6. Final performance baseline (reference for regression tests)

Machine: Intel Core Ultra 5 125U laptop. Scenario: `twilight4H`, 1 human slot + 4 Hard AI, Enhanced AI, 50 000 cash, 23 400 ticks (13 min). Median of three runs, headless (`Invoke-PathStress.ps1`), tick ms:

| workload | mean | p50 | p95 | p99 | p99.9 | worst | ticks > 33.3 ms | digest |
|---|---|---|---|---|---|---|---|---|
| Zero Hour 1x (realistic heavy) | 2.75 | 1.64 | 15.0 | 18.6 | 26.4 | 75.5 | 9 | `1E5982E5B3A1B86B` |
| Zero Hour 4x mobile units (3@15000, stress) | 4.31 | 1.98 | 16.3 | 21.7 | 32.9 | 80.2 | 22 | `CF9C9DB782B65644` |
| Generals 1x | 0.78 | 0.64 | 1.81 | 5.01 | 9.85 | 45.1 | 6 | `1014EE85E384C19C` |
| Generals 4x mobile units (3@15000, stress) | 1.40 | 0.77 | 4.48 | 10.1 | 17.1 | 27.0 | 0 | `E649F6A6E27117B8` |

Seeded graphical benchmark (`Invoke-GraphicalBenchmark.ps1`, 9 min, VSync off, fingerprint equal to the headless run at all 540 checkpoints in every run):

| | mean | p50 | p95 | p99 | p99.9 | worst | > 33.3 ms | render interval p50 / p99 (median window) / max |
|---|---|---|---|---|---|---|---|---|
| Zero Hour (4 runs) | 3.08 - 3.34 | 1.92 - 2.11 | 15.0 - 15.8 | 18.8 - 22.3 | 25.6 - 36.5 | 66 - 87 | 7 - 26 | 8.33 / 19.5 - 20.7 ms / 98 - 170 ms |
| Generals (1 run) | 1.47 | 1.28 | 2.68 | 5.99 | 24.8 | 322 (probable stall) | 14 | 8.33 / 8.4 ms / 569 ms (a stall) |

Sustainable object range (unchanged by this milestone, section 15 of [AI_QUERY_AUDIT.md](AI_QUERY_AUDIT.md)): Zero Hour keeps p99 under the 33.3 ms budget up to about **2 700 - 2 900 objects**
(a normal 1x match peaks near 1 500).

Major remaining bottlenecks: (1) the exhaustive failing searches of the Zero Hour HUNT state, a quarter of its 1x simulation time and its whole tail; (2) idle-guard scans (92 % empty, not exactly skippable);
(3) AI state machines and update modules; (4) the graphical client's 2-4x per-object phase inflation. Known hitch classes: first-use creation (~100-170 ms once), off-CPU process stalls (environment), logic
ticks > 33 ms from failing searches (0.04 % of ticks at 1x).

Determinism and gates for this build: Zero Hour 1x `1E5982E5B3A1B86B`, `city5H` `FB748B205FB71351`, line-decision `6AC45762C1656E3D` / `B414DB6B28633F5A`, open list `5B85CEF9CC94BCB0` / `30F676D2A410CAE1`
(all unchanged), golden AI gates PASS (Zero Hour, Generals), Google Tests 557 / 556, Python suites native-runtime 339, performance 54, diagnostics 5, rendering 30.

## 7. Multithreading

Unchanged: no broad simulation threading. The failing searches are independent read-only floods whose result is consumed by an order-dependent state machine, and they cost 25 % of one Zero Hour title only;
a future option would be speculative parallel search with in-order commit, which needs an exactness proof for the pool sharing described in section 1. Not started.

## 8. Performance-phase exit decision

**Stop the broad performance phase.** Realistic gameplay is inside the simulation budget on this machine (Zero Hour 1x p99 19 ms, 0.04 % of ticks above 33.3 ms; Generals p99 5 ms), the knee is at
~2 700 - 2 900 objects, and no exact low-risk optimisation remains that addresses a measured bottleneck.

| finding | class |
|---|---|
| Zero Hour HUNT failing searches (11 ms each, ~1 400 per match) | Beta target (needs a behaviour decision, not an exact optimisation) |
| ticks > 33.3 ms from those searches (9-26 per 23 400) | Beta target |
| first-use render hitch 100-170 ms once per match | Beta target (loading-screen prewarm) |
| off-CPU stalls of 100-2 240 ms (incl. earlier "Present" stalls) | external / environment investigation |
| idle-guard scans, AI/update module cost, graphical per-object inflation | future optimisation |
| behaviour above ~2 900 objects | extreme-stress-only |
| anything blocking a Developer Preview | **none** |

Only real two-PC multiplayer validation of `2071ad765` (still pending) stands between the current state and a Developer Preview on the performance side.
