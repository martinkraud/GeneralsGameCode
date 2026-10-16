# X64.41 Path line-phase optimization + retail game-speed audit + R1 localisation tooling (CP15)

Three separate workstreams. The simulation stayed serial; no thread, pool or speculative execution was added; no gameplay timing or default speed changed.

* **A. Pathfinding line phase** - delivered: exact per-search passability memo; severe tail and path-search worst roughly halved again.
* **B. Retail TPS / Game Speed audit** - delivered as an audit (`RETAIL_GAME_SPEED_AUDIT.md`): retail Normal = 30 logic frames per second in both titles, proven from source and retail data; nothing implemented.
* **C. R1 replay determinism** - tooling delivered (per-frame CRC interval override, per-frame / per-object CRC trace, comparison and analysis scripts, staged diagnostic executable). **A short human recording is required**; the exact steps are in section 12.
* Tooling: the stale Stage4B.1 guard was redesigned (section 13).

Predecessors: `X64_38_CPU_SCALABILITY.md`, `X64_39_ZONECALC_OPTIMIZATION.md`, `X64_40_PATH_SEARCH_INVESTIGATION.md`.

## 1. Repository state at the start

`dev/modern-engine` == `origin/dev/modern-engine` == `bbcb68df1` (X64.40), clean tree, no upstream push.

## 2. Workstream A - what the line phase is

For **every expanded cell** `examineNeighboringCells` calls `iterateCellsAlongLine(cell -> goal)` (Bresenham) and runs `examineCellsCallback` on each cell of the straight line toward the goal until a cell blocks it
or the goal is reached. Per callback: `validMovementPosition`, the hierarchical `isPassable` test, `getPinched`, the human extent test, `checkForMovement`, the cliff test, `allocateInfo`, `setBlockedByAlly(false)`,
`costToGoal`, and - if the cell is not already on a list with a cost that is not worse - cost update, list removal and `putOnSortedOpenList`.

### 2.1 Instrumentation (`GENERALS_SIM_PATHS=1|2|3`, off by default)

Plain counters in the walk's stack struct, flushed once per walk into the path profile only when a sample is active: line walks, callbacks, why each walk stopped (invalid / zone / pinched / extent / movement /
fixed / cliff / alloc), callbacks that found the cell already on a list at a cost that is not worse ("kept"), cells updated / inserted, memo hits; `=3` additionally times the validity checks, `checkForMovement` and the
cost/list update (clock overhead included - attribution only, never used for the before/after timing). The runner reports distributions, the outcome split, the top-25 searches with their line-walk and callback counts.

### 2.2 Findings (heavy benchmark, traced run, all 24 132 internal A* searches)

| quantity | value |
|---|---|
| expansions / line walks | 12 407 639 / 12 383 565 (one walk per expansion) |
| callbacks | 80 194 590 = **6.48 per walk** (p50 3.4, p95 15.0, p99 33.3, max 94) |
| callbacks that were **kept** (cell already open/closed at a cost that is not worse; walk simply continues) | **64 785 756 = 80.8 %** |
| callbacks that updated / inserted a cell | 3 035 486 = 3.8 % |
| why walks end | zone not passable 9 388 334 (76 %), pinched 2 415 165, invalid terrain 424 192, `checkForMovement` 80 586, fixed unit 64 466, alloc failure 605, extent 0, cliff 0 |
| time split inside the callbacks (timed run, 25 ns clock reads included) | validity 1.75 s, `checkForMovement` 2.23 s, update 1.80 s |
| the big failing searches (>= 20 000 expansions) | 42.3 M callbacks for 8.34 M expansions (5.1 per expansion) but **59 callbacks per walk in the two worst searches** (6 116 512 callbacks for 103 910 walks) |

Forensics of the remaining worst searches (after X64.40): both are ~104 k-expansion exhausting failures; after the open-list fix ~90 % of their time was the line phase, and **78 % of their callbacks re-examine cells
the search has already examined**, performing the full chain of checks for a verdict that cannot change while one search runs.

### 2.3 Redundant work and the exactness argument

Whether a cell passes `validMovementPosition` (the `fromCell` argument is unused), the hierarchical passability test, `getPinched`, the extent test, `checkForMovement` (cell flags, units, object relationships - no logic
runs inside a search) and the cliff test depends only on the **cell** and on the **search parameters** (object, surfaces, crusher, radius, centre-in-cell, human, ignored obstacle), which are constant for the whole of one search,
and on the layer of the cell the walk comes from (recorded with the verdict). The walk keeps passing over cells it has already accepted. The memo stores, per cell info, the **search generation** and layer in which the cell passed
all of those checks **and** obtained its info record. A later callback for the same cell, same generation and layer skips the checks and continues with the unchanged cost handling (`allocateInfo`, `setBlockedByAlly(false)`,
`costToGoal`, kept / update). Everything with side effects keeps its order. A *new* generation begins with every list reset (every search start; `PathfindCellList::reset`), so verdicts from earlier searches - including cells whose
info records persist because units occupy them - can never be reused. Info records are re-initialised (generation 0) when recycled.

### 2.4 The optimization

`ExamineCellsStruct` carries the generation and the mode flags; `PathfindCell::hasLineVerdict / setLineVerdict` (two new fields in `PathfindCellInfo`); the callback was restructured so that the six checks run only on a memo miss. The
order of the checks, the stop reasons and the cost logic are unchanged (the early returns became a `stop` code evaluated in the same order). `GENERALS_LINE_LEGACY=1` disables the memo (A/B on one binary).

Not done, deliberately: moving `costToGoal` after the kept test (exact and ~0.3 s, but the worst tick is now set by the script tick, section 8, so further path micro-work no longer moves the primary metrics);
a layout change of `m_map` (column-major access; no evidence that it dominates now).

## 3. Exact-result oracle

| check | result |
|---|---|
| `GENERALS_LINE_VERIFY=1`: every memo hit also runs the full original checks and must agree (first difference prints frame, cell, layer, stop reason and `_exit(3)`) | **72 072 137 memo hits re-verified in the heavy benchmark, 0 differences** |
| decision stream: `GENERALS_LINE_TRACE_HASH=1` hashes every callback's decision (stop reason / kept / updated / alloc failure) with the cell | original = memo = `1C551E6BD64E50C9` |
| open-list evolution digest (X64.40 oracle, every insertion and removal) | original = memo = `5DE477B0AFDAD8BB` (identical to the X64.40 value) |
| authoritative fingerprint (2 400 checkpoints) | `74F2CB528BED42ED` in every run of every mode |
| 16 base maps, three runs each (default / all-legacy / all-verify) | **0 mismatches**: 1 649 ZoneCalc recalculations, 33 918 373 open-list insertions, **50 635 424 line memo hits** re-verified; all 16 default-vs-legacy fingerprints identical; all exit codes 0 |

The three maps whose AIs barely act (Bridge Busters, Cliff, SwissMP) contribute almost nothing beyond the load-time calculation.

## 4. Heavy benchmark, before / after (X64.40 state -> X64.41; same executable, `GENERALS_LINE_LEGACY=1` vs default, 4 alternating rounds, nothing else running)

| whole tick | X64.40 (legacy line phase) | X64.41 (memo) |
|---|---|---|
| mean ms | 0.962 | 0.947 |
| median | 0.676 | 0.681 |
| p90 | 1.659 | 1.662 |
| p95 | 2.169 | 2.179 |
| p99 | 6.383 | 6.229 |
| p99.9 | 24.244 | **18.109** |
| worst (mean of 4 runs; per run) | 174.6 (179.3 / 176.4 / 163.8 / 178.9) | 124.2 (92.8 / 231.9 / 86.0 / 86.0) |
| ticks > 16.67 ms | 96 | 82 |
| ticks > 25 ms | 69 | 50 |
| **ticks > 33.333 ms** | 45 | **34** |
| ticks > 50 ms | 30 | **24** |
| **ticks > 66.67 ms** | 15 | **10** |
| ticks > 100 ms (per run) | 3.0 (3, 5, 2, 2) | 0.5 (0, 2, 0, 0) |
| derived missed deadlines | 79 | **48** |
| longest catch-up run | 12 | 4 |
| **max backlog** | 141 ms | **91 ms** |

**An honest caveat about the worst tick.** Three of the four memo runs have a worst tick of 86-93 ms and one has 231.9 ms; an earlier 3-round batch had 97.5 / 83.4 / **969 ms**. All three outliers are the **periodic script tick**
(`logic_scripts`, every 300th frame, 70-110 ms normally) hit by a one-off stall - at frame 65 400 the same tick is 72 ms in the other three memo runs and in the legacy run, and the 969 ms event was at frame 54 900 in only one run. Two such
events appeared in 7 memo runs and none in 7 original runs; with this sample that is not distinguishable from chance and I could not link it to the memo (the stall is inside the script phase, not the path phase), but it is reported rather than
averaged away. The median of the per-run worst ticks is 177.6 ms -> **89.4 ms**.

Path search (this run, absolute ms):

| | X64.40 | X64.41 |
|---|---|---|
| `path_search` total (clean runs, 4 round mean) | 5 748 ms | **4 535 ms** (-21 %) |
| `path_search` worst tick contribution | 167.3 ms | **74.8 ms** |
| `path_search` share of the slowest 1 % of ticks | 32.3 % | **25.0 %** |
| internal A* total (traced run) | 5 317 ms | **4 758 ms** |
| internal A* mean / p95 / p99 / p99.9 / worst | 0.220 / 0.61 / 5.1 / 15.3 / 153.7 ms | **0.197 / 0.60 / 5.8 / 10.0 / 91.2 ms** |
| A* searches > 25 / 50 / 100 ms | 17 / 6 / 2 | **6 / 2 / 0** |
| line phase, ns per callback (>= 20 000-expansion searches) | - | 60 ns (whole search time / callbacks) |
| top slowest searches (ms) | 153.7, 152.0, 94.2, 70.5, 64.0, 50.3, 46.2, 38.7, 37.2, 34.8 | **91.2, 86.4, 49.5, 41.7, 30.6, 27.4, 24.8, 20.4, 18.9, 18.8** |
| expansions per search / callbacks per walk | unchanged | unchanged (the search itself is identical) |

Cumulative X64.39 -> X64.41 on the same benchmark: tick p99 23.4 -> 6.2 ms; worst tick 239-265 -> ~86-93 ms (typical run); ticks > 33.3 ms 188 -> 34; > 66.7 ms 53 -> 10; > 100 ms 32 -> 0.5; derived missed
deadlines 389 -> 48; maximum backlog 2 910 -> 91 ms.

## 5. Attribution after the optimization (mean of 4 clean runs, exclusive time)

| phase | total ms | max ms | share of the slowest 1 % of ticks |
|---|---|---|---|
| ai_object_base_update | 25 005 | 4.7 | 6.4 % |
| object_update_loops | 12 758 | 56.4 | **17.4 %** |
| logic_scripts | 7 032 | (86-122; periodic) | **18.0 %** |
| path_search | 4 535 | 74.8 | 25.0 % |
| path_request_entries | 2 118 | 10.1 | 3.3 % |

Newly exposed: **the periodic script tick** - 15 ticks above 20 ms in a run, all on frames that are multiples of 300 (every 10 s of game time: 65 700 = 91 ms, 29 700 = 80 ms, 47 700 = 79 ms, 38 700 = 79 ms ...), 1.24 s
in total for ticks above 10 ms - and update-module spikes (max 56-84 ms). With the worst path search at 75 ms the primary metrics are now set by scripts and object updates, not by pathfinding.

## 6. Retail Game Speed audit (workstream B) - summary

Full evidence in `RETAIL_GAME_SPEED_AUDIT.md`. Proven from the original sources and the retail data of both titles: **Normal = 30 logic frames per second, 33.333 ms per frame, in Generals and in Zero Hour**
(`LOGICFRAMES_PER_SECOND = 30`; `GameData.ini`: `UseFPSLimit = Yes`, `FramesPerSecondLimit = 30` in `INI.big` and `INIZH.big`; the original loop runs one logic update per loop iteration, paced by the FPS cap). The retail "Game Speed"
slider in Skirmish (15..60, above 60 uncapped) *was that FPS cap*, sent as the fourth argument of `MSG_NEW_GAME` and stored in the replay header; campaign sends none. Multiplayer negotiates the minimum FPS of all players, clamped to the cap. This fork decouples
logic from render (`FramePacer` logic time scale, default on = 30 TPS). All gameplay timing (movement, weapons, builds, economy, AI, scripts, saves, replays, network) is a function of logic frames, so a speed change preserves gameplay ratios; audio and
client presentation do not scale automatically. The deadline for a level is 1000/TPS ms; the measured exposure of the heavy benchmark at 20-60 TPS is in the audit.

## 7. R1 - state after this milestone

Known: the user replay diverges at frame 1600 in headless **and** in the normal native client (X64.40); the divergence started in frames 1501-1600; recorded inputs in that window: a group selection at 1518 and a dozer
construct at 1564. New in X64.41 (all inert unless their variable is set):

* `GENERALS_REPLAY_CRC_INTERVAL=<n>`: release builds record and generate the replay CRC every n logic frames (the engine only had `-ReplayCRCInterval` in DEBUG_CRC builds). n = 1 makes the engine's own comparison name the first
  divergent frame, not the next multiple of 100.
* `GENERALS_CRC_TRACE=<file>` (+ `GENERALS_CRC_TRACE_OBJECTS=1`): one line per logic frame with the state CRC **split into objects / RNG seed / partition manager / players / AI** and the object count, and one line per object (ObjectID,
  template, snapshot CRC). It is taken at the exact point where the replay CRC is generated, so a live recording and its re-simulation write comparable files; it reads the same snapshots `GameLogic::getCRC` reads. Verified: the
  re-simulation trace is byte-identical between two runs, and enabling it does not change the authoritative fingerprint (`40148F96CB0EB2B0` on the user replay, same as without).
* `scripts/diagnostics/Compare-CrcTraces.py`: first differing frame, the differing components, and the ObjectIDs / templates whose CRC differs or that exist on one side only (tested on identical and on a mutated trace).
* `scripts/diagnostics/Analyze-ReplayDiagnostic.ps1`: re-simulates the recorded replay with the trace on and runs the comparison (tested end to end on a known-identical pair and on a mutated one).
* `scripts/diagnostics/Run-R1-Diagnostic.cmd` + the staged `generalsv_r1diag.exe`: the one human step.

The first divergent frame could **not** be obtained automatically: it needs a live reference, and the existing replay was recorded without one.

## 8. Is a human recording required? Yes - one short one (section 12 gives the steps)

## 9. Remaining determinism risk

R1 stays open and remains a blocker for multithreading by default and for any multiplayer or replay-determinism claim. The serial optimizations of X64.39-X64.41 are not affected: they are verified against the original code at every decision
and preserve the benchmark fingerprint.

## 10. Rejected / deferred in this milestone

| item | decision |
|---|---|
| any parallel / speculative path search, worker pool | not started (as instructed) and not justified: the worst path search is 75 ms and the script tick is larger |
| moving `costToGoal` after the kept test | exact, ~0.3 s; skipped (no effect on the primary metrics now) |
| `m_map` layout / loop order | no evidence it dominates the remaining 60 ns per callback; deferred |
| caching failed searches across ticks, bounding a search, changing the retry cadence | rejected (gameplay timing) |
| the periodic script tick | next target (section 14) |

## 11. Validation

Win64 and Win32 builds, ctest (16/16, 23/23), gtests (416 / 415 Win64, 378 / 378 Win32), every native-runtime and performance Python contract (including the redesigned `test_stage4b1_reference.py` and the updated
`test_stage4b3_reference.py`), the offline PowerShell workflow tests (10/10), the heavy benchmark fingerprint in every mode, the 16-map three-mode verification.

## 12. The one human step for R1 (about two minutes)

1. In `build\dev-runtimes\native\Generals\x64-37-modern-settings-v4\game\` double-click **`Run-R1-Diagnostic.cmd`** (it starts `generalsv_r1diag.exe` with a CRC every logic frame and the trace on).
2. Start any Skirmish against one easy AI, select the **dozer**, build **one** structure, wait about **40 seconds** in the match, then Exit Game and quit the game. Nothing else.
3. Tell me "done". The files `r1_live.rep` and `r1_live.trace` appear next to the executable; I run `Analyze-ReplayDiagnostic.ps1` and report the first divergent frame, component and ObjectID.

## 13. The Stage4B.1 guard

`scripts/performance/test_stage4b1_reference.py` compared the whole of `AIPathfind.cpp` token for token with the 2026-10-09 snapshot after undoing one change. That is only true until the first later change; it first failed at `10b4503e9`
(X64.35: profiler scopes, 68 340 vs 68 333 tokens) and could neither distinguish authorized from unauthorized edits nor keep protecting the rest of the file. It was **redesigned, not weakened**:

* `pathfinder_function_guard.py` splits the file into top-level definitions (brace counting that ignores `#else` branches), removes measurement code (`PERFORMANCE_PROFILE_SCOPE`, `PerformanceProfile::...`, `pathProfile` / `dispatch` statements,
  `if (pathProfile.enabled()...)`) and braces, and compares every definition with the snapshot;
* the **authorization tables** in the test list every behaviour-bearing change since the snapshot (11 changed, 23 added-name and 3 removed-name entries, covering 10 changed, 33 added and 4 removed definitions) with milestone, reason and the oracle that protects it; anything not listed fails, and a listed entry
  that no longer differs is reported as stale so the table cannot silently grow;
* the Stage4B.1 hoist itself is asserted directly (one `acceptableSurfaces` member, one setup assignment from `getValidSurfaces()`, two callback reads, no `theLoco`); the locomotor getter / mutator files remain frozen (unchanged check);
* **mutation tests run with every execution**: a behavioural edit in `checkForMovement`, a constant change in `costToGoal`, an unlisted new function and an unlisted removal are each reported; an instrumentation-only edit is not.

`test_stage4b3_reference.py`'s allow-list was extended by the exact X64.41 additions (two `PathfindCellInfo` fields, two inline `PathfindCell` methods, the list generation counter); its mutation checks from X64.40 still hold.

## 14. Recommendation for X64.42

Evidence now points away from pathfinding: the worst tick is the **periodic script tick** (70-110 ms every 300 frames, plus rare stalls inside it) and the **update-module spikes** (max 56-84 ms). X64.42: attribute the script
tick (which script action, which object scans - the `logic_scripts` exclusive time has no sub-attribution yet), then apply the same method (counters, exactness oracle, serial fix). In parallel: close R1 with the diagnostic recording. Parallel
pathfinding remains unjustified (the remaining 75 ms search is smaller than the script tick).

## 15. R1 resolved and CLOSED: the Generals replay header did not record the Skirmish starting cash

The two staged recordings were analysed with the new tooling. First divergent frame 1459; the command at logic frame 1458 is the human's `MSG_DOZER_CONSTRUCT` (War Factory). A temporary env-gated trace of `onDozerConstruct` / `BuildAssistant::buildObjectNow` / `DozerAIUpdate::construct` (removed again, not committed) showed the re-simulation rejecting it with `canMakeUnit == CANMAKE_NO_MONEY`: the human had **0** money at frame 1458, the live game had at least 2000.

Cause: the live Skirmish was started with Match Options starting cash 50000 (`Skirmish.ini StartingCash = 50000`, X64.31 extension). `GameInfoToAsciiString` writes the starting cash (`SC=`) only for Zero Hour; the `RTS_GENERALS` branch has no such field, so a Generals replay re-simulates with the default 10000. The player CRC does not include money, so the difference stayed invisible until the first purchase the smaller bank could not afford (AI players were not cash-limited that early, so objects matched until then).

Proof: re-running the same replay with starting cash forced to 50000 gives `NO DIVERGENCE on the 6640 common frames` against the live trace; and with the real CRC check (no `GENERALS_SIM_IGNORE_CRC`) the replay with `SC=50000` inserted in its header simulates to the end (exit 0), the original stops at `CRC Mismatch in Frame 1458`.

Fix (replay format only, no gameplay change): `RecorderClass::startRecording` (Generals) now inserts `;SC=<cash>` into the Skirmish options string **only when the starting cash differs from the default**, so default-cash replays are byte-identical to before; LAN / GameSpy strings are untouched; `ParseAsciiStringToGameInfo` already parsed `SC=` in both titles. Replays recorded before this fix with non-default cash cannot be replayed exactly (cash unknown; patching `;SC=<n>` into the header repairs them).

Consequences: the R1 divergence was **not** a simulation nondeterminism; the earlier "simple scenario deterministic" result is consistent (default cash). The remaining determinism risks of section 9 are unchanged. Not yet verified: a fresh live recording with the fix (the writer path is covered by the equivalence above, not by a native run); the next human native test can confirm by recording a non-default-cash Skirmish and replaying it.

### 15.1 Closure evidence (human native test of `dff555c24`)

- The human recorded a new Skirmish (Lone Eagle, 50000 Starting Cash) with the staged build (`generalsv_fixcash.exe`, `Run-StartingCash-Test.cmd`) and replayed it: no CRC mismatch, no replay problem (1:56 of game time, CRC every frame).
- The generated header was inspected: `M=03maps/lone eagle;MC=4E5678CC;MS=237764;SD=239185390;C=1;SC=50000;S=...` - `SC=50000` is present, right after `C=1`.
- Full validation after `dff555c24`: Win64 and Win32 builds; ctest 16/16 (Win64) and 23/23 (Win32); gtests 416 (g) / 415 (z) on Win64 and 378 / 378 on Win32; 21 Python contract / performance tests (including `test_stage4b1_reference.py`, `test_stage4b3_reference.py`, `test_skirmish_match_options_contract.py`, `test_ordinary_menu_contract.py`); 10 PowerShell workflow tests; heavy benchmark fingerprint `74F2CB528BED42ED` (2400 checkpoints) unchanged with the final executable.
- R1 is closed. It was a replay-format omission, not a simulation nondeterminism. Open (unrelated) determinism risks are those of section 9.
