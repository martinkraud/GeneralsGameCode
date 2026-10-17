# X64.42 - Simulation tail reduction (periodic script tick + update-module spikes)

Serial, no threads. Starting point: `dev/modern-engine` = `origin/dev/modern-engine` = `f55345938` (X64.41 closed, R1 closed), tree clean except untracked staging output under `build/dev-runtimes`.
Authoritative heavy-benchmark fingerprint (Twilight Flame, 7 brutal AI, 72 000 ticks): **`74F2CB528BED42ED`** - unchanged in every run of this milestone.

## 1. Baseline (X64.41, same heavy benchmark)

Ticks > 33.3 ms: 34; > 50: 24; > 66.7: 10; missed deadlines (derived, 30 TPS): 48; p99.9 18.1 ms; `logic_scripts` periodic spikes of 70-110 ms on frames divisible by 300; `object_update_loops` max 56-84 ms.

## 2. Instrumentation (off by default)

`Common/ScriptProfile.h/.cpp` (new, Core) + hooks in both `ScriptEngine.cpp` (Generals, Zero Hour): per-script, per-condition-type and per-action-type time, the worst 24 script updates with their top scripts / conditions / actions, a
`frame % 300` split, free-form counters. Enabled only by `GENERALS_SIM_SCRIPTS=1` in the headless benchmark runner; otherwise each hook is a single `bool` test (RAII probes). Output is a bounded text report (`<base>_scripts.txt`).
Unit tests: `ScriptProfileTest` (inert when off, attribution and ranking, nested actions, bounded list).

## 3. Root cause of the 300-frame script spike

Attribution of the 15 script ticks > 20 ms (all on frames divisible by 300; 83 ms worst): one action, **`[Skirmish] Special power -- fire at enemy's highest cost area`** (60 calls, 1320 ms total, 22 ms mean, 78-86 ms max),
fired by the AI superweapon scripts (`... Fire` scripts with a 10 s evaluation delay = 300 frames at 30 TPS). Sub-timing split it: **computing the target 6.5 ms in total (0.1 ms per call, legacy; 2 ms with a prototype snapshot) vs
firing the power 1359 ms in total**. A first hypothesis (the 221 `getPlayerSuperweaponValue` evaluations x `calcCostToBuild`) was therefore measured, implemented as an exact snapshot (verified 10 191 evaluations, 0 differences, mutation caught)
and **rejected and reverted**: it saved 4 ms per run.

The firing path writes launch-log records (`OCLSpecialPower::doSpecialPowerAtLocation`, `DeliverPayloadAIUpdate` start/trace, `AIGroup` dispatch -> `StartupDiagnostics::checkpoint`) and `checkpoint` called **`FlushFileBuffers` per record**
in the normal-launch log (`generals64.log`, `generalsv*.exe` / `generalszh*.exe`): a synchronous disk flush on the simulation thread, 10-85 ms each. Evidence:

* the log has exactly 60 `special_power` records for the 60 calls;
* the same replay run from an executable whose name is not a game executable (no log opened): script worst 83 ms -> 5.0 ms, 0 script ticks > 20 ms, action total 1394 -> 37 ms;
* A/B with the same executable and `GENERALS_NORMAL_LOG_FLUSH=1` (flush) versus default (section 5).

This is not benchmark-only: every real game in a normal launch paid a disk flush on the logic thread whenever a superweapon, a paradrop / airstrike delivery or a group special-power dispatch happened. The 56-84 ms "update-module spikes" of
`object_update_loops` were the same cause (payload-delivery checkpoints), so no separate object-update optimization is needed.

## 4. The change and why behaviour is preserved

`StartupDiagnostics::checkpoint` (`Core/GameEngine/Include/Common/StartupDiagnostics.h`): in a normal launch the record is still written with `WriteFile` (the OS keeps it across a process crash, so the forensic value stays; only an OS
crash / power loss could lose the last records) but is no longer flushed per record. The evidence-harness modes (`enabled()`) still flush every record; `GENERALS_NORMAL_LOG_FLUSH=1` restores the old behaviour in a normal launch.
Nothing in the simulation reads the log. Evidence of exactness:

* fingerprint `74F2CB528BED42ED` in all 8 A/B runs, in the verify/legacy runs, and in the final build;
* **16 maps** (the X64.40/41 representative set, 14 simulated minutes, 7 AI where the map allows): digest with flush = digest without, on every map; **launch-log content (stage / status / detail sequence) identical** on every map;
* contract `scripts/performance/test_x64_42_startup_log_contract.py` (5 tests, 4 mutation tests: unconditional flush, flush always true, harness modes unflushed, missing switch - each detected).

## 5. A/B results (same executable, alternating A,B x 4, nothing else running)

A = `GENERALS_NORMAL_LOG_FLUSH=1` (X64.41 behaviour), B = default.

| whole tick | A (flush) | B (fixed) |
|---|---|---|
| mean / median ms | 0.952 / 0.681 | 0.882 / 0.663 |
| p90 / p95 / p99 ms | 1.665 / 2.192 / 6.205 | 1.582 / 2.048 / 5.733 |
| **p99.9 ms** | **17.80** | **9.13** |
| worst ms | 88.4 | 83.0 (path search, section 7) |
| ticks > 16.67 / 25 ms | 80.0 / 48.8 | 28.8 / 6.2 |
| **ticks > 33.3 / 50 / 66.7 ms** | **34.0 / 22.5 / 8.8** | **4.8 / 2.8 / 2.0** |
| derived missed deadlines | 45.2 | 8.2 |
| longest catch-up run | 3.8 | 3.2 |
| max backlog ms | 55.0 | 49.7 |

Script subsystem (`logic_scripts` phase): total 7083 -> 5632 ms; max 85.1 -> 3.9 ms; share of the slowest 1 % of ticks 17.2 % -> 1.9 %; script ticks > 20 ms: 15 -> 0 (profile run, 5.0 ms worst).
`object_update_loops`: max 55.5 -> 10.9 ms, share of the slowest 1 % 17.3 % -> 3.6 %. Per-run values: `flush_A1..4` / `flush_B1..4` (scratch, not committed). Ticks > 20 ms: 61 (17 on frames divisible by 300) -> 14 (0 periodic).
Top slow frames before: frames 65400 / 65700 / 56700 ... 69-80 ms, `logic_scripts` 99 % of each; after: 15867 (78.8 ms) and 10468 (76.9 ms), both `path_search`.

## 6. Reprofile (post-fix, run B1; mean of 4 where noted)

1. Largest contributor to the slowest 1 % (720 ticks, 5637 ms): **path_search 39.6 %**, zone_flatten 12.1 %, ai_object_base_update 7.5 %, zone_hierarchy 6.7 %, zone_label 5.7 %, path_request_entries 5.1 %, zone_relabel 4.5 %, object_update_loops 3.5 % (was 17.8 %), logic_scripts < 2 % (was 17.9 %).
2. Largest repeatable single-tick spike: exhaustive failing path searches ("null_after_work", ~104 000 pops, ~21 000 distinct cells, 6.1 M line-walk callbacks) - frames 15867 / 10468 (~77 ms), then 10295 (47), 9470 (36), 10133 (31), 12607 (27).
3. Largest cumulative CPU: ai_object_base_update 37.7 % (throughput, low individual max 4.9 ms), object_update_loops 17.6 %, logic_scripts 8.9 %, logic_partition 7.1 %, path_search 6.9 %.
4. Remaining path-search tail: 14 ticks > 20 ms, all in a few clusters (frames ~9 000-16 000 and 28 516); those are the retail A* exhausting a region, already shortened by X64.40/41; no exact algorithmic shortcut is known without changing results.
5. ZoneCalc: ~5.6 ms per recalculation, 386 recalculations; the six `zone_*` phases are 12.1 % + 6.7 % + 5.7 % + 4.5 % of the slowest 1 % because recalculations coincide with other work in the same tick.
6. Scripts: mean 0.14 ms per tick; the worst remaining tick is ~4-5 ms. Other maps: frame 0 (map-load initialization, "AI player build near a supply source" 2 x 60 ms on Alpine Assault, 15 ms on Rogue Agent) is a one-time load cost, not a gameplay tick; a few one-off 30-60 ms values in the 16-map table did not reproduce on re-run (OS noise, reported rather than hidden).
7. Update modules / object updates: no spike above 11 ms after the fix.

## 7. Second optimization: not performed

The reprofile identifies path search as the next bottleneck, but every available lever is either already taken (open-list index, line memo: X64.40/41) or would change search results (early termination of exhaustive failing searches, bounding pops) or needs a deeper
restructuring of the retail A* line-walk callbacks. No safe exact optimization with a strong local oracle is in hand, and 14 ticks in 72 000 are left. Per the milestone rules this is documented for X64.43 instead of attempted.

## 8. Rejected alternatives

* Snapshot / memo for `computeSuperweaponTarget` (exact, verified, but 1.5 % of the action; reverted).
* Moving the superweapon scripts to another frame, changing the 300-frame cadence, skipping the log: rejected (semantics).
* Dropping the log records or the cap: the records are useful forensic evidence; only the per-record disk flush was wrong.
* Asynchronous log writer thread: unnecessary; `WriteFile` alone is microseconds.

## 9. Validation

Win64 and Win32 builds; ctest 16/16 and 23/23; gtests 420 (g) / 419 (z) on Win64 and 382 / 382 on Win32 (new: 4 `ScriptProfile` tests); 22 Python contract / performance tests including Stage4B.1, Stage4B.3 and the new startup-log contract with
4 mutation tests; 10 PowerShell workflow tests; fingerprint `74F2CB528BED42ED`; 16-map digest and launch-log equality. The earlier verify-mode oracle for the rejected snapshot (10 191 evaluations, 0 mismatches, injected difference caught with exit 3) is recorded for the next time such an oracle is needed.

## 10. Remaining bottlenecks and X64.43 recommendation

1. Exhaustive failing path searches (section 6): characterise which requests produce them (destination in an unreachable / blocked region) and whether the *request path* (not the result) can be made cheaper exactly, e.g. a cheaper line-walk callback, or prove a request class always fails and its result is reproducible without the search.
2. ZoneCalc recalculation coinciding with other work (5.6 ms each).
3. `ai_object_base_update` throughput (25 s of 63 s): not a tail issue; investigate only for throughput.
4. Frame-0 initialization cost of AI supply-source building (one-time).
Not recommended yet: threads (still serial by decision); Game Speed implementation (design only: `GAME_SPEED_DESIGN.md`).

## 11. Tooling added

`GENERALS_SIM_SCRIPTS=1`, `GENERALS_NORMAL_LOG_FLUSH=1` (A/B / forensic). Harness scripts used for the A/B and the 16 maps live in the session scratchpad; the method is the X64.38-41 one (`Invoke-SimulationBenchmark.ps1`, `New-SimulationBenchmarkReplay.py`).
