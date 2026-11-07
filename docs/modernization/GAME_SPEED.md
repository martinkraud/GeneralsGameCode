# Game Speed (Skirmish) - archaeology, architecture, evidence

Status: **implemented for Skirmish (Modern Experience), both titles; automatically validated; native human review and multiplayer validation pending.** Supersedes the design-only
[GAME_SPEED_DESIGN.md](GAME_SPEED_DESIGN.md) (its contract and implication research stand; section 6 below records what was decided and measured). Retail background:
[RETAIL_GAME_SPEED_AUDIT.md](RETAIL_GAME_SPEED_AUDIT.md). Start point `aac354d5f`.

**Real two-PC multiplayer validation of `2071ad765` remains pending. Multiplayer Game Speed is NOT implemented and NOT validated.**

## 1. What the engine already had (archaeology)

| Mechanism | Where | Finding |
|---|---|---|
| Logic cadence constant | `LOGICFRAMES_PER_SECOND = 30` (`GameCommon.h`, `WWCommon.h`) | the retail Normal; "must never change". Every gameplay quantity (movement, weapons, build, income, AI, scripts, timers) is converted once at INI load to logic frames, so **gameplay is a function of logic-frame counts only** |
| Retail Skirmish "Game Speed" slider | `SkirmishGameOptionsMenu.wnd:SliderGameSpeed` (hidden since X64.52A) | was the **FPS cap** of the one-update-per-loop-iteration loop (15..60, uncapped above 60); sent as the 4th `MSG_NEW_GAME` argument and stored in the replay header `maxFPS`. In this fork that value is a **render** cap and never paces the simulation |
| Logic pacing | `FramePacer::m_logicTimeScaleFPS` + `GameEngine::canUpdateRegularGameLogic` accumulator | offline logic is decoupled from rendering and runs at 30 ticks/s whatever the render cap is (`m_enableLogicTimeScale = TRUE`). **This is the right seam**: Game Speed scales this rate and nothing else |
| Upstream developer speed keys | `MSG_META_INCREASE/DECREASE_LOGIC_TIME_SCALE` (Shift+Ctrl+Numpad +/-) | steps the raw logic fps in 5s, minimum 30 in release (so it cannot slow down), refused in network games, writes no match state. Kept unchanged outside a Modern Skirmish game |
| Replay fast-forward | `m_TiVOFastMode` | replays only; uncapped logic; unrelated |
| Script `setTimeMultiplier` / `isTimeFast` | scripts, cinematics | view-side; unrelated |
| Network logic rate | `TheNetwork->getFrameRate()`, minimum-FPS negotiation, capped by `m_framesPerSecondLimit`; `2071ad765` multiplayer timing fix | **owns the logic rate in a network game**; Game Speed does not enter it |
| Presentation | `PresentationClock` (period-agnostic), `getLogicFramePhase`, `getBaseOverUpdateFpsRatio` (30 / update fps) | interpolation uses the real tick period; camera / UI scale by render rate, not logic rate |
| Saves, replays, CRCs | frame-indexed | no wall clock in logic state; no `timeGetTime` in `GameLogic` (grep) |
| Pause | `GameLogic::setGamePaused`, `m_logicTimeScaleEnabledMemory` | pauses the logic scale (rate 0); unchanged |
| Audio (Miles), particles / FX | real time mixer; client update per render frame | not scaled; documented below |

Conclusion: **no new timing architecture was needed.** Game Speed is a ratio applied to the existing offline logic-rate seam, owned by one match setting.

## 2. Architecture

* **One authoritative value**: `GameInfo::m_gameSpeed`, **percent of Normal** (75 / 100 / 125 / 150; 100 = default), pure rules in `Common/GameSpeed.h` (+ `GameSpeedXfer.h`). Logic rate = 30 x percent / 100, kept **exact** (22.5 / 37.5 / 45 logic frames per second; `FramePacer::getActualLogicTimeScaleFpsExact`, the accumulator now reads a `Real`).
* **Pacing only**: `GameLogic::getGameSpeed()` returns the rule for a Skirmish or replay game and **Normal for network games, Campaign, Challenge and the shell**; `GameEngine::canUpdateGameLogic` hands it to `FramePacer::setGameSpeedPercent` every frame. At 100 the rate is `fps x 1.0f`: bit-identical to the pre-feature value. Nothing under `GameLogic/` reads the rule (contract test).
* **Independent of rendering**: the rule never touches `setFramesPerSecondLimit`, VSync, Render Scale or interpolation. Presentation keeps interpolating at any level (the clock reads the real tick period).
* **Persistence**:
  * options string / replay header: key `GS=<percent>`, written **only when not Normal** (default strings and replay headers stay byte-identical; an older executable refuses an unknown key instead of playing a different pace);
  * Skirmish.ini: `GameSpeed = Slow | Normal | Fast | VeryFast` (written only when the Modern selector is offered);
  * saves: `SkirmishGameInfo` xfer **version 7** (written when the rule is not Normal, or Enhanced AI, or a superweapon limit; older saves load as Normal);
  * Classic never offers the control and always starts at Normal.
* **Selector**: a fourth rule of the generated Match Rules zone (`kRules`: `GameSpeed` / `ComboBoxGameSpeed`), default Normal.

## 3. Supported speeds (decision, with measurements)

Heavy Zero Hour workload = the seeded Twilight Flame AI-vs-AI benchmark with the 4x unit multiplier (`GENERALS_SIM_CLONE=3@8000`), 14 400 logic frames. Budget = 1000 / TPS.

| Level | TPS | Budget | Decision | Evidence |
|---|---|---|---|---|
| 0.50x | 15 | 66.7 ms | **not exposed** | technically sustainable (0.03 % of ticks over budget) but no demonstrated need; slow motion at 15 TPS also halves the interpolation smoothness benefit and needs an audio / FX review. Deferred |
| **0.75x Slow** | 22.5 | 44.4 ms | **accepted** | ZH 1x: 0.04 % of ticks over budget; 4x: 0.1 %; real graphical pace 21.5 of 22.5 TPS (light run), fingerprints = headless |
| **1.00x Normal** | 30 | 33.3 ms | **default, unchanged** | digests unchanged (section 5); heavy graphical 29.85 TPS |
| **1.25x Fast** | 37.5 | 26.7 ms | **accepted** | ZH 1x: 0.94 % over budget; 4x: 3.4 %; heavy graphical **37.28 of 37.5 TPS (99.4 %)** |
| **1.50x Very Fast** | 45 | 22.2 ms | **accepted, with the caveat below** | ZH 1x: 2.6 % over budget; 4x: 5.0 %; heavy graphical **44.69 of 45 TPS (99.3 %)**: a 8-minute match finished in 322 s against an ideal 320 s |
| 2.00x | 60 | 16.7 ms | **rejected** | the heavy-Zero-Hour tick p99 is 18 - 26 ms, above the 16.7 ms budget: 5.0 % (1x) / 6.3 % (4x) of ticks overrun and 6 - 11 % finish late with up to 170 - 200 ms of lateness. It works on an empty map, not in realistic late-game play, and it would need the simulation-tail work this milestone must not do |

Caveat for Very Fast (and Fast on weak CPUs): the average is sustained, but individual heavy ticks (p99 about 18 - 26 ms in a late Zero Hour match) can exceed the 22 ms budget; they show as brief hitches, never as lasting slowdown. Generals is far below every budget (max 0.17 % over at Very Fast on the 4x stress).

The accumulator credits at most one tick interval per render frame, so a very long tick is not "made up" by a burst: a pathological stall costs real time, it does not desynchronise anything (frame-indexed state is untouched).

Render cap rule: the logic rate can never exceed the render rate (existing behaviour: one logic update per render update at most). Fast and Very Fast need a render FPS limit of 38 / 45 or more. The default presets (60 and up) satisfy that; the runtime control tells the player when the limit is lower than the speed needs. At 60 / 120 / 240 FPS the pace was identical (44.18 / 44.69 / 44.15 TPS at Very Fast, below).

## 4. Runtime Speed Up / Speed Down

**Implemented, Skirmish only.** The existing upstream key pair (Shift+Ctrl+Numpad +/-, no retail binding, modifier combination: no accidental activation) steps through the discrete levels with the on-screen message `Game Speed: Fast - 1.25x` (`... (limit)` at the ends; a note when the render FPS limit is too low). Everywhere else (Classic, campaign, challenge, replays, network) the keys keep their upstream developer meaning. The change is pacing only: it is written to the match's game info (so a save keeps it), **not** to the replay (a replay plays at the speed it was started with) and never to the render limit.

## 5. Determinism, replay, save / load (evidence)

* **Authoritative result is speed independent**: the same seeded replay at 75 / 100 / 125 / 150 gives the identical headless fingerprint at all 480 checkpoints in both titles (`evidence/game-speed/headless-digests.txt`); in the graphical client every run matches its headless run at every checkpoint (9 light runs + 3 heavy runs, first divergence index -1).
* **Render FPS does not change the result**: Very Fast at render caps 60 / 120 / 240: same pace (44.18 / 44.69 / 44.15 TPS), fingerprints equal to headless.
* **RNG**: no code path that reads the rule is in the simulation (contract test); logic is advanced one frame at a time exactly as before, so the number of render frames between ticks cannot add or reorder an RNG call (no RNG is drawn in the scheduler).
* **Replay**: a replay carries `GS=` in its setup string only when not Normal and plays at the recorded pace; a replay recorded without the key (every existing replay) plays at Normal. The result of a replay is frame-indexed and identical at every pace.
* **Recorded replays** (real Skirmish matches played through the frontend at 100 / 125 / 150 %, both titles): the replay header carries `GS=125` / `GS=150` and no key at Normal; each replay plays back headless **with CRC checking** without a mismatch; game time covered 1:39 / 2:04 / 2:29 for the same wall-clock recording (`evidence/game-speed/replay-record.txt`). Synthetic seeded replays with `GS=` play at the recorded pace in the graphical client (table in `evidence/game-speed/graphical-pace.md`).
* **Save / load** (`skirmishsaveload` autoflow: Skirmish -> save -> quit to menu -> load -> continue): the pace log shows 125 (or 150) at start, Normal while in the menu / loading, and 125 (150) again after the load in both titles. The saved-vs-loaded object comparison shows the same few differing moving workers at Normal as at 125 / 150 (Zero Hour 7 at Normal, Generals 0): that is the existing one-frame difference of the audit flow, not a Game Speed effect (`evidence/game-speed/saveload-pace.txt`).
* **Normal equivalence** (final committed tree): Zero Hour 1x `1E5982E5B3A1B86B`, 4x (`3@15000`) `CF9C9DB782B65644`, `city5H` `FB748B205FB71351`; Generals 1x `1014EE85E384C19C`, 4x `E649F6A6E27117B8`; X64.pf line decision `6AC45762C1656E3D` / `B414DB6B28633F5A`, open list `5B85CEF9CC94BCB0` / `30F676D2A410CAE1`; golden AI gates PASS for both titles.

## 6. Multiplayer readiness (not implemented)

Architecturally ready: the value is one `GameInfo` field that already travels in the options string (`GS=`, written only when not Normal; an unknown key is refused by older builds), so a host-authoritative lobby selection can later set it for everyone. **Deliberately not done**: the network path ignores the rule (`getGameSpeed()` and `getActualLogicTimeScaleFpsExact` return Normal / the negotiated network rate when `TheNetwork` exists), the LAN UI has no control, and nothing changes the lockstep negotiation or `2071ad765`. Open before it can be offered: the lobby control (Modern LAN milestone), a protocol compatibility rule for peers that do not know the key, lockstep validation at non-Normal rates (run-ahead is measured in frames, so latency tolerance in time shrinks as the rate rises), and real two-PC testing.

## 7. Known limits and remaining work

* Audio and client particle / FX timing are real time and are not scaled (documented behaviour of retail Fast settings too); a listening pass at Very Fast is pending.
* Campaign and Challenge never use the rule (Normal only); scripts, cinematics and mission timers are untouched.
* The legacy Match Options band (modern display without the Modern frontend) has no room for a fourth rule: Game Speed is a Modern Match Setup rule.
* Native human review of the selector, the messages and the feel at 1.25x / 1.5x.
* The command-UI start delay (known issue 5) was investigated in this milestone and is unrelated to the speed: see PROJECT_STATUS.

## 8. Commands and aids

`GENERALS_FORCE_GAME_SPEED=<75|100|125|150>` starts a new Skirmish at that level without touching `Skirmish.ini` (test aid); `New-SimulationBenchmarkReplay.py --game-speed N [--fps N]`; `Invoke-GraphicalBenchmark.ps1 -GameSpeed N -HeaderFps N`; the autoflow `GENERALS_AUTOFLOW=skirmishsaveload`; every pace change logs `gamespeed pace percent=... logic_fps=... render_cap=...`.
