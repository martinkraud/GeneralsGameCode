# Game Speed - design for a future explicit feature (DOCUMENT ONLY)

Status: **design, not implemented.** No Game Speed UI or runtime behaviour exists or is added by X64.42. This file turns the proposal in
`RETAIL_GAME_SPEED_AUDIT.md` section 4 into a contract plus the implication research a later, separately authorized milestone needs.
The facts it relies on are proven in the audit: retail **Normal = exactly 30 logic frames per second** in Generals and Zero Hour; the retail
Skirmish Game Speed slider was the FPS cap of the one-update-per-loop-iteration main loop; this fork decouples logic from render
(`FramePacer`, `m_enableLogicTimeScale = TRUE`, default `LOGICFRAMES_PER_SECOND`), so render FPS no longer changes game speed.

## 1. Contract

1. **Game Speed changes the simulation cadence only** (logic ticks per wall-clock second). It is a new setting, never derived from, and never
   changing: Render FPS Limit, Render Scale, VSync, Smooth presentation / interpolation, the Enhanced HUD, or audio volume.
2. **Normal = exactly 30 TPS** (33.333... ms) and equals original/retail behaviour. It is the default of every install, of the campaign, of every
   replay recorded without a speed, and the only basis for performance claims. Any other figure is labelled "at N TPS".
3. **Levels**: names are UI; the stored value is TPS. Retail bounds were 15..60 (+ uncapped). Proposal: Slower 15-25, **Normal 30**, Faster 36/45/60.
   "Uncapped" stays a benchmark/replay facility (`-headless`, replay fast-forward), not a gameplay level.
4. **Where**:
   * Options -> Gameplay: personal **default for newly created Skirmish matches** (shows the Normal marker). Never alters a running match or the campaign.
   * Skirmish Match Options: per-match value, initialised from the personal default; stored in `Skirmish.ini`, the replay header and saves.
   * Campaign / scripted missions: **Normal only** (control disabled with the reason) unless a future design changes it explicitly.
   * Multiplayer / LAN: **one authoritative cadence for the whole match**, chosen by the host (a ceiling) and folded into the existing minimum-rate
     negotiation; never a per-player setting.
5. Implementation seam: route the chosen value into `FramePacer::setLogicTimeScaleFps` (already separate from `setFramesPerSecondLimit`, which is the
   render cap). Never reuse the `MSG_NEW_GAME` `maxFPS` argument as a render cap for a speed-bearing match without the compatibility rule below.

## 2. Implications (researched from the source; open items are flagged)

| area | finding | rule for the implementation |
|---|---|---|
| **Replay compatibility** | The replay header stores `maxFPS` (retail: the FPS cap the match ran at). Today playback pins logic at 30 TPS and treats the value as a render cap. The Generals Skirmish header also needed `SC=` for starting cash (X64.41 R1); any new match setting must be serialised the same way or the re-simulation diverges. | Add the speed as an **explicit new header field** (written only when not Normal, like `SC=`). Old replays and replays with a legacy `maxFPS` of 30..240 play as Normal. Playback speed is a view-side facility; the simulation result is frame-indexed and identical at any pace. Add a replay test per non-Normal level. |
| **Saved games** | Saves are frame-indexed (logic frame, CRC); no wall-clock in logic state. The speed is session configuration, not game state. | Store the match speed in the save so loading reproduces the pace; a save from before the feature loads as Normal. |
| **LAN / network lockstep** | `ConnectionManager` computes the minimum FPS over players and clamps it to `[MIN_LOGIC_FRAMES=5, m_framesPerSecondLimit]`; `changeLogicTimeScale` is refused in network games; run-ahead is measured in frames, so latency tolerance in *time* shrinks as TPS rises. | One host-defined cadence in the lobby game options (protocol field with a default of Normal for old peers), clamped by the slowest peer. Needs its own validation at non-Normal rates (not part of any single-player milestone). |
| **Audio** | Miles is a real-time mixer; events are triggered from logic. Not scaled by cadence. | At Faster levels voices, music stingers and ambient loops keep real time (documented behaviour). Needs a listening pass; no pitch shifting. |
| **Client effects / particles** | Client update runs per render frame; the fork scales UI/camera by `getBaseOverUpdateFpsRatio()` (30 / logic fps) in `CommandXlat`, `W3DView`, `InGameUI`, `Drawable`, `StealthUpdate`, `W3DWater`, `W3DTreeBuffer`. | Keep using the ratio; verify each consumer at 15/45/60. Decoration that is purely client-side stays render-driven. |
| **Smooth presentation interpolation** | Interpolation uses the logic frame phase from `FramePacer` (`getLogicFramePhase`), i.e. the *actual* logic interval. | Must read the actual logic rate (not the constant 30). Re-run the interpolation tests with a variable logic rate. |
| **Timers, tooltips, UI displays** | Gameplay clocks are logic-frame based; UI conversions divide by `LOGICFRAMES_PER_SECOND` (`InGameUI`, `Drawable`, `GameClient`, `ScoreScreen`). | They display **game seconds** (30 frames = 1 game second), exactly as retail did at its Fast settings. State this in the option text. Real-time-based UI (double-click, key repeat) stays real time. |
| **Scripts / AI** | Script timers, AI delays, build/weapon/economy durations are logic-frame counts (audit section 3). | Gameplay ratios are preserved at any cadence. Campaign scripts/cinematics are tuned at 30, hence Normal-only. |
| **Performance** | The per-tick deadline is 1000/TPS ms; tick cost does not change with the speed. X64.42 removed the periodic 70-110 ms tick that would have cost two deadlines at 60 TPS; the remaining tail (rare 70 ms exhaustive failing path searches) is still above a 60 TPS deadline. | Do not offer Faster levels as hardware compensation. Re-measure the tail at each level with the heavy benchmark model before shipping them. |

## 3. Open items for the implementing milestone

* Header field name and parser tolerance in retail Generals/Zero Hour readers (old binaries must not crash on a replay they cannot play).
* Lobby protocol field and the negotiation with the minimum-FPS rule.
* A test matrix: replay record -> re-simulate at each level; save/load at each level; interpolation with a changing logic rate; UI text with game seconds.
* Whether Classic Experience keeps the retail 15..60 slider semantics (recommended) while Modern Experience shows named levels over the same stored TPS.
