# Replay and determinism findings

Evidence level: each section says what was observed, what was proven and what was not. Nothing here claims that Generals replays or lockstep are
"solved"; multiplayer and cross-architecture determinism remain **unvalidated** in this fork.

## R-1. A Generals Skirmish replay did not record the Starting Cash, so it re-simulated with a different economy

| | |
|---|---|
| Subsystem | replay recorder / game-info serialisation |
| Symptom | A live recording and its re-simulation diverge (`CRC Mismatch in Frame 1458`) although the engine is deterministic |
| Impact | Any replay recorded with a non-default Starting Cash is not reproducible in Generals; an economy-dependent command silently fails during playback |
| Fix complexity | small (one conditional string insertion) |
| Evidence | strong: first divergence localised to a frame, forced-cash re-simulation matches the live trace on all 6 640 common frames, fixed writer verified natively |
| Commit | `dff555c24` (fix), `94428fa89` (CRC tooling used to find it), `f55345938` (closure) |
| Upstream applicability | **Possible**, only for a build that lets the player choose a non-default Starting Cash in Generals (see "Why this is a fork finding first") |

### Problem / symptom

A Skirmish was started with Starting Cash 50 000 (`Skirmish.ini: StartingCash = 50000`), played for 3 min 41 s and saved as a replay. Re-simulating that file headless
(`generalsv.exe -headless -replay file.rep`) reported `CRC Mismatch in Frame 1458`. The first recording attempt of this series (a shorter, simpler match with default cash) had been
deterministic, which is why the problem looked like a rare nondeterminism and not like missing input.

### Affected source

* `Core/GameEngine/Source/GameNetwork/GameInfo.cpp`, `GameInfoToAsciiString()` (the options string that becomes the replay header): the `#if RTS_GENERALS` branch writes
  `M=...;MC=...;MS=...;SD=...;C=...;` only. The `#else` (Zero Hour) branch also writes `US=`, `SR=`, **`SC=<starting cash>`** and `O=`.
* `ParseAsciiStringToGameInfo()` (same file) already accepts `SC=` for both titles (`key.compare("SC")` -> `startingCash.deposit(...)`, then `game->setStartingCash`).
* `Generals/Code/GameEngine/Source/Common/RTS/Player.cpp` (`Player::init`, the `m_money.countMoney() == 0` branch): the starting money of every player comes from
  `TheGameInfo->getStartingCash()`.
* `Generals/Code/GameEngine/Source/Common/Recorder.cpp`, `RecorderClass::startRecording()`: the Skirmish branch writes `GameInfoToAsciiString(TheSkirmishGameInfo)`.

### Why this is a fork finding first

Retail Generals has no Starting Cash control in Skirmish; Zero Hour has one. This fork added a Starting Cash choice to the Generals Skirmish Match Options (X64.31 fix3 /
`082ae9e64`), reusing the Zero Hour plumbing. The Generals header branch had never needed the field. Any project that exposes a non-default Starting Cash in Generals (or any other
setup value that is not in the Generals options string) has the same hole: **a value that shapes the simulation but is not in the replay header is an input the replay cannot reproduce.**

### Reproduction

1. Start a Generals Skirmish with a non-default Starting Cash, build several structures and queue units.
2. Exit and replay the saved replay headless with a CRC interval of 1 (`GENERALS_REPLAY_CRC_INTERVAL=1` in this fork; retail builds use a larger interval, which only delays the report).
3. The re-simulation starts every player with the default (10 000).

### Root cause and why it diverged late

* Live: starting money 50 000. Re-simulation: 10 000 (the `GameInfo` default, because the header had no `SC=`).
* Until the first purchase the two simulations were identical in every checked component. Both AI and human spending was affordable on both sides for a while.
* At logic frame 1458 the human's `MSG_DOZER_CONSTRUCT` (a War Factory, 2 000) was accepted live but rejected in the re-simulation: a temporary trace of
  `GameLogic::onDozerConstruct` -> `BuildAssistant::buildObjectNow` -> `DozerAIUpdate::construct` showed `canMakeUnit == CANMAKE_NO_MONEY` with the human's money at **0** (the
  re-simulation had spent 10 000 on four queued units, power plant, barracks, supply centre, second power plant and eight more units; live had spent the same but had 40 000 more).
  The object `762` therefore existed only in the live game, which made the object count and the object CRC differ at frame 1459.
* **The state CRC does not contain money.** `Player::crc()` (Generals `Player.cpp`) transfers only the battle-plan bonuses, `m_skillPoints` and `m_sciencePurchasePoints`; `Money::crc()`
  (`Money.cpp`) is empty and `Player::crc` does not call it. This is unchanged retail behaviour (neither file was modified for it). In the per-frame diagnostic trace the `players=` component was
  identical for frames 1456-1459 although the two simulations held different amounts of money. So the first CRC-visible effect is the *consequence* (an object that exists on one side only), which can
  be arbitrarily later than the cause.

### How it was localised (reusable method)

`GENERALS_REPLAY_CRC_INTERVAL=1` (CRC every frame) + `GENERALS_CRC_TRACE=<file>` and `GENERALS_CRC_TRACE_OBJECTS=1`: one line per logic frame with the CRC split into
`objects / rng / partition / players / ai` and the object count, plus one line per object (ObjectID, template, CRC). `scripts/diagnostics/Compare-CrcTraces.py` reports the first differing frame,
which components differ and which ObjectIDs exist on one side only. Narrowing: global CRC -> component -> object set -> the single object that exists on one side (`762 AmericaWarFactory`) ->
the command that creates it (parsed from the replay: `1458 MSG_DOZER_CONSTRUCT`) -> the rejection reason in the build path.

### Fix

`RecorderClass::startRecording` (Generals) inserts `;SC=<cash>` before the slot list (`;S=`) **only when the Skirmish starting cash differs from `TheGlobalData->m_defaultStartingCash`**.
The parser needed no change. LAN and GameSpy option strings are untouched.

* Default-cash replays are byte-identical to before (header compatibility with replays and tools that predate the field).
* Old recordings made with non-default cash cannot be fixed automatically (the cash is not in the file); inserting `;SC=<n>` after `C=<n>` in the header repairs them by hand.

### Verification

* With the starting cash forced to 50 000 in the re-simulation, the per-frame trace matched the live trace on **all 6 640 common frames** (`NO DIVERGENCE`).
* Real CRC check (no override): the original replay stops at frame 1458; the same file with `;SC=50000` inserted runs to the end (exit code 0).
* Native test of the fixed writer: a new Skirmish with 50 000 cash recorded by the fixed build contained `...;C=1;SC=50000;S=...` and replayed for its full 1:56 without a mismatch.
* The full build/test matrix was re-run after the change; the benchmark fingerprint did not change (the benchmark's replay is generated with default cash).

### Limitations

* Only the starting cash was proven missing. Other setup values of the Generals options string (map contents mask, map CRC/size, seed, CRC interval, slots) were not audited for the same hole.
* An earlier recording that reported a mismatch at frame 1600 was never separately diagnosed; it is **not** claimed to have the same cause.
* Retail interval: a CRC every N frames (100 in a normal build) reports the divergence only at the next check; use interval 1 when localising.

## R-2. Strategy used to make determinism checkable at all

These are tooling findings, not engine bugs, but they are what made R-1 findable.

| Tool | What it gives | Where |
|---|---|---|
| Headless replay simulation | `generalsv.exe -headless -replay x.rep` simulates without a window, GPU, audio or input; a normal-launch executable still simulates headlessly (`m_headless` is set by the normal-launch profile) | `Core/GameEngine/Source/Common/ReplaySimulation.cpp` |
| Determinism fingerprint | FNV-1a over `{logic frame, GameLogic::getCRC(), object count}` taken every 30 frames (2 400 checkpoints in the 72 000-tick benchmark); the same digest `74F2CB528BED42ED` was produced by every build and every legacy/optimized switch of X64.38-X64.42 | `Core/GameEngine/Include/Common/DeterminismFingerprint.h` |
| Self-playing benchmark replay | one idle human + N computer players: AI slots are simulated by the engine at playback, so a "replay" with only a setup string is a deterministic, repeatable battle of any length, free of proprietary content | `scripts/performance/New-SimulationBenchmarkReplay.py` |
| `GENERALS_SIM_IGNORE_CRC=1` | keep simulating past a mismatch (the engine pauses on one) so the rest of a run can still be measured/traced | `ReplaySimulation.cpp` |
| Verify / legacy modes | each optimization keeps the original algorithm and a mode that runs both and compares (see `PERFORMANCE_FINDINGS.md`) | `AIPathfind.cpp` |

A matching fingerprint is necessary but not sufficient evidence of exactness: it covers the state CRC, which, as R-1 shows, does not include everything (money). The optimizations therefore also carry
local oracles that compare the actual intermediate data.

## R-3. Floating-point policy on AMD64 (a determinism risk that was characterised, not resolved)

`setFPMode()` (`GameLogic.cpp`) (both titles, identical) requests `_PC_24` precision and nearest rounding through `_controlfp`. On the AMD64 UCRT used here the precision request is ignored (control word
`0x0008001f` instead of Win32's `0x000a001f`; `MXCSR 0x1f80` on both); no invalid-parameter callback fires. SSE2 single operations round to binary32 naturally, but double expressions, library calls and
instruction order are not governed by the x87 precision setting. **No startup register setting proves that a 32-bit and a 64-bit build simulate identically**, and cross-architecture replay/lockstep
compatibility is **not validated** in this fork. Details: `docs/modernization/X64_11_NATIVE_STARTUP_READINESS.md` ("AMD64 floating-point decision"), test `NativeStartupFPTest.cpp`.

---

## R-4. A complete 51-minute human-played Skirmish re-simulates without divergence (X64.44, observation)

The native build recorded a 92 656-frame match (human + 5 Brutal AI, Twilight Flame, Starting Cash fix present). Headless re-simulation of the preserved replay produced **zero CRC mismatches over 926 replay CRC checks and 3 088 fingerprint checkpoints**, and reproduced 26 of the 30 slowest native ticks on the same
frames. This is the first long real-game determinism validation of this fork (one match, one build, one machine). It is **not** a proof of replay determinism in general, of cross-architecture determinism, or of network lockstep (R-3 is unchanged).

---

## R-5. The available Zero Hour replay does not re-simulate on any Zero Hour build (X64.45, observation)

`Replays 00000.rep` (74 s, recorded 2026-10-09) reports `CRC Mismatch in Frame 100` under `generalszh.exe -headless -replay` with the current Win32 build **and** with the Win32 builds of the runtime stages
`baseline` (2026-10-07, before every optimization), `stage3c3-ground-hardening` and `stage4a3-performance-harness-fix1`. The mismatch therefore does not come from the fork's serial optimizations or from X64.45. Cause unknown (the
recording environment, options or content may differ from the headless re-simulation; the Generals R-1 lesson - a replay header that omits a setting - is a candidate but was not examined). Zero Hour replay determinism is **not established**;
the first native Zero Hour candidate run produces a fresh replay for a proper check. Evidence: `docs/modernization/evidence/x64-45-zh-replay-check/`.
