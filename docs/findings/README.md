# Engineering findings index

This is a map of the reusable discoveries made while modernising GeneralsGameCode in the MKRaudstein fork. Each entry points to a write-up detailed enough to
reproduce the problem, understand the root cause, re-implement a fix independently and check it - **without trusting the patches in this repository.** The write-ups state
what was measured, what was proven and what was not.

How to read the tables:

* **Upstream relevance** - HIGH (shared engine code, deterministic defect), POSSIBLE (shared code, would need the reader to judge), FORK (specific to this fork's architecture or additions),
  HISTORICAL (already addressed or superseded), INSUFFICIENT (not enough evidence to recommend). This is a statement about the code *in this repository*; the findings were **not** compared
  against the current state of upstream or other forks.
* **Evidence** - strong (root cause isolated and the fix verified against an oracle or a native run), good (measured, mechanism understood, limited verification), limited.
* Commits are on `dev/modern-engine`; `git show <sha>` shows the exact change.

## Performance (simulation CPU) - [`PERFORMANCE_FINDINGS.md`](PERFORMANCE_FINDINGS.md)

| ID | Finding | Subsystem | Symptom | Impact (heavy benchmark) | Fix complexity | Evidence | Commit | Upstream relevance |
|---|---|---|---|---|---|---|---|---|
| P-1 | Synchronous `FlushFileBuffers` of a diagnostic log on the logic thread | `StartupDiagnostics::checkpoint`; special power / payload delivery / group dispatch | 70-110 ms tick every 300 frames; 55-84 ms update spikes | script worst 85 -> 3.9 ms; ticks > 33.3 ms 34 -> 4.8; missed deadlines 45 -> 8 | small (skip the flush) | strong (A/B, 16-map digest + log equality, contract + 4 mutation tests) | `1ebe641f9` | HIGH as a class of bug; the trigger is FORK |
| P-2 | `ZoneCalc` rewrote a 24 000-entry table per equivalence merge | pathfinder zones (`AIPathfind.cpp`, `ZoneEquivalence.h`) | 23.5 ms per zone recalculation | 5.5 ms (-77 %); `zone_hierarchy` 15x | medium (min-root union-find + oracle) | strong (bit-for-bit differential tests, in-engine verify on 388 + 1 649 recalcs, injection test) | `c51dc603d` | POSSIBLE |
| P-3 | A* open-list insertion walked from the head of a sorted list | pathfinder (`forwardInsertionSortRetailCompatible`, `OpenListIndex.h`) | 100-230 ms searches; 96 % of the worst search in the walk | A* total 18.2 -> 5.3 s; ticks > 33.3 ms 188 -> 45; backlog 2.9 s -> 127 ms | medium (exact cost-run index with fallback) | strong (23.5 M + 33.9 M insertions verified, evolution digest) | `bbcb68df1` | POSSIBLE |
| P-4 | Repeated accepted-cell checks in the A* line walk | pathfinder (`examineCellsCallback`) | 80.8 % of callbacks re-examined settled cells | `path_search` -21 %, worst 167 -> 75 ms | small-medium (per-search generation memo) | strong (72 M hits re-verified, decision digest) | `94428fa89` | POSSIBLE |
| P-5 | Negative results: superweapon-target snapshot, shadow-volume reuse | AI / renderer | - | ~4 ms per run; 0 ms | - | good | `4caebb9e2`, `f0c347b34` (+ X64.42 doc) | INSUFFICIENT value - not recommended |
| P-6 | `PATHFIND_CELLS_PER_FRAME` is not a CPU bound | pathfinder queue | one request may expand the whole map | documentation of semantics | - | good (source reading + observed 107 143 expansions) | `bbcb68df1` | POSSIBLE (documentation) |
| P-7 | Remaining exhausted A* searches are sealed pockets (stationary non-idle allied / uncrushable enemy units); no exact shortcut | pathfinder (`checkForMovement`, `internalFindPath`) | 6-7 searches above 20 ms per heavy run | explanation + decision not to optimise (ceiling 0.16-0.7 s per run) | - | strong (diagnostic on 14 maps, repeat and state-mutation measurement) | (diagnostic only; see the X64.43 document) | POSSIBLE (explanation only) |

## AMD64 porting - [`AMD64_PORTING_FINDINGS.md`](AMD64_PORTING_FINDINGS.md)

| ID | Finding | File / function | Failure | Evidence | Commit | Upstream relevance |
|---|---|---|---|---|---|---|
| A-1 | `ObjectPoolClass` block payload offset assumed a 4-byte link pointer | `WWLib/mempool.h` | crash at process exit (corrupt next-block link) | strong (PDB-resolved fault, byte-level test on both architectures) | `53f08fbdc` | HIGH |
| A-2 | `WindowMsgData` was 32 bits but carries pointers | `GameClient/GameWindow.h` | access violation loading `MainMenu.wnd` | strong (fault address == truncated pointer; dual-architecture test) | `f6e8c1aee` | HIGH |
| A-3 | Audio completion handles cast to `UnsignedInt` | `MilesAudioManager.cpp`, `GameAudio.h` | completions dropped; SFX/voices vanish over a long session | strong (20-test mixer suite incl. 64-bit handle round trip) | `082ae9e64` | POSSIBLE (needs a 64-bit audio library) |
| A-4 | Diagnostics / stack walking used the x86 `CONTEXT` | `Except.cpp`, `debug_*.cpp`, `StackDump.cpp` | 92 compile-error occurrences | good (build + platform test) | `923f730fe` | POSSIBLE |
| A-5 | x87 precision control is ignored on AMD64 | `setFPMode` (`GameLogic.cpp`) | none observed; equivalence unproven | good (probe test; characterisation only) | `4a36a0cd2` | POSSIBLE (cross-architecture determinism) |
| A-6 | Pointer-truncation compiler scan (negative evidence) | whole engine | 194 hits, special-power path clean | limited (path audited, rest not) | - | INSUFFICIENT |
| B-1 | D3DX/DX8 helper binaries are 32-bit only | `WW3D2` helpers | link failure (17 helper names per title) | good (characterisation tests, 3 -> 0 external helpers) | `91c1e3e12` ... `09b2983af` | FORK |
| B-2 | Miles Sound System is 32-bit | audio device layer | no AMD64 mixer | good | `bb98b8c01` | FORK |

## Replay and determinism - [`REPLAY_DETERMINISM_FINDINGS.md`](REPLAY_DETERMINISM_FINDINGS.md)

| ID | Finding | Subsystem | Symptom | Evidence | Commit | Upstream relevance |
|---|---|---|---|---|---|---|
| R-1 | Generals replay header omitted the Skirmish Starting Cash | `GameInfoToAsciiString`, `RecorderClass::startRecording`, `Player::init` | re-simulation diverges at the first purchase the smaller bank cannot afford (frame 1458 in the proven case); the state CRC does not contain money | strong (forced-cash trace matches 6 640 frames, native test of the fix) | `dff555c24` | POSSIBLE (any build exposing non-default cash in Generals) |
| R-2 | Deterministic benchmark + fingerprint + CRC-trace methodology | `ReplaySimulation.cpp`, `DeterminismFingerprint.h`, `scripts/diagnostics/` | - | strong (used to verify every optimization) | `4d1db422a`, `94428fa89` | POSSIBLE (tooling) |
| R-3 | AMD64 floating-point policy characterised, cross-architecture equivalence **not** proven | `setFPMode` (`GameLogic.cpp`) | - | good | `4a36a0cd2` | POSSIBLE |

## Related documents

* Performance methodology and results: [`../PERFORMANCE.md`](../PERFORMANCE.md)
* Retail Game Speed facts and the (unimplemented) Game Speed design: [`../modernization/RETAIL_GAME_SPEED_AUDIT.md`](../modernization/RETAIL_GAME_SPEED_AUDIT.md), [`../modernization/GAME_SPEED_DESIGN.md`](../modernization/GAME_SPEED_DESIGN.md)
* Per-milestone engineering records: [`../modernization/`](../modernization/) (`X64_38` ... `X64_42` for the CPU series)
* Roadmap and handoff: [`../modernization/FIRST_PLAYABLE_ROADMAP.md`](../modernization/FIRST_PLAYABLE_ROADMAP.md), [`../modernization/AGENT_HANDOFF.md`](../modernization/AGENT_HANDOFF.md)

## What is deliberately not here

No claim about overall game speed on other machines, no claim that all Generals determinism problems are solved, and no recommendation to merge any patch: the aim of these notes is that another
developer can verify the problem and implement their own fix.
