# X64.46 - Zero Hour game-mode completeness (Campaign, General's Challenge, Save/Load)

Status: **OPEN - automated native validation done, human native validation pending.** Do not close until the human has run the three modes on the staged candidate (section 8).
Start point: HEAD `78f96405d` = `origin/dev/modern-engine`, working tree clean, X64.45 closed (candidate `x64-45-zh-bringup-v3` human validated).

## 0. What is validated how (read this first)
| Level | Meaning | Where it applies in this document |
| --- | --- | --- |
| **Audited** | Code read, compared with Generals / Core / retail behaviour; not executed | section 2 |
| **Automated native validated** | The real native `generalszh.exe` (Win64, normal launch, real window, real shell screens driven by `GENERALS_AUTOFLOW`) reached the state; evidence is the log / screenshot. **Not** human validation | sections 3-5 |
| **Human validated** | A person played it | none yet (section 8) |
| **Unavailable because movies are absent** | The mode works, the movie is skipped (no Bink decoder on AMD64) | section 6 |
| **Deferred** | Not done, with the reason | section 9 |

## 1. Method
`GENERALS_AUTOFLOW` (developer aid in `NativeAutoShot`, `W3DDisplayOrdinaryHost.cpp`; off unless the variable is set, only in normal launch) was extended from `skirmish` to `campaign`, `challenge` and `saveload`. Every step does what the player does: sends the same `GBM_SELECTED` a click sends, to the real button, **to the owner window the gadget would send to**, only when the button is visible and enabled all the way up the window tree; pushes the real screen; opens the quit menu like Esc; selects a list row; waits for input control (the quit menu disables Save/Load during a mission cinematic, exactly as for a player). The main menu stays hidden until the player moves the mouse or presses a key (`MainMenuInput`), so a key is sent like the player's first input. Nothing about the world is constructed, edited or fast-forwarded (a contract test forbids `MSG_NEW_GAME` / `startNewGame` in the aid).
`Invoke-NativeAutoCapture.ps1 -Flow Campaign|Challenge [-General n]|SaveLoad` runs it on a **disposable** stage (`x64-46-scratch`, deleted-by-nobody: it is a scratch stage, the candidate is separate). The aid also records `state` lines (frame, side, money, rank, objects, structures, the engine's own logic CRC; per component and per object around a save/load) in `generals64.log`.

## 2. Audit matrix (Phase A)
Legend: G = Generals native today, ZH = GeneralsMD before this milestone, R = result.
| Row | Shared or title specific | G | ZH before | R |
| --- | --- | --- | --- | --- |
| Campaign: selection / front-end flow (`MainMenu` Single Player -> USA/GLA/China -> difficulty) | `MainMenu.cpp` is ZH-specific (ZH has the Challenge button, a different drop-down set); logic shared | worked | ported in X64.45, never driven | **works** (3.1) |
| Campaign: mission selection / start (`setupGameStart`, `MSG_NEW_GAME`) | shared (`CampaignManager`) | worked | untested | works |
| Campaign: intro / movie request (`MD_USA01`, `Intro`, `Display::playMovie`) | Core shared | movie skipped when open fails | same code | works (skip logged) |
| Campaign: movie skip without Bink | `Display`/`InGameUI`/`WindowVideoManager`/`ScoreScreen`/`Intro` all null-check `open()` | consistent | same | **works** except the one defect below |
| Campaign: load screen (`SinglePlayerLoadScreen`) | Core `LoadScreen.cpp`, `#if RTS_GENERALS` differences | null stream -> hides percent, returns early (default background) | same code, ZH branch | works (movie skipped, default background; briefing text absent, 6) |
| Campaign: world creation, first playable frame | shared | worked | untested | works: GLA bikers cutscene frame reached (3.1) |
| Campaign: mission scripting, victory/defeat, next mission, return to frontend | shared scripts + ZH `ScoreScreen` (final-victory movie via `PlayMovieAndBlock`, null-checked) | not audited natively either | audited only | **not driven to a mission end** (9) |
| Challenge: selection screen (`ChallengeMenu`, `TheChallengeGenerals`) | **ZH only** | n/a | never run natively | **works** (3.2) |
| Challenge: general/opponent selection and state (`setGeneralCampaign`, `TheChallengeGameInfo`) | ZH only | n/a | untested | works (generals 0-4 started) |
| Challenge: intro / movie request and skip | ZH only (`ChallengeLoadScreen`) | n/a | **defect**: null stream dereferenced | **fixed** (4.1) |
| Challenge: load screen, world creation, first playable frame | ZH only | n/a | crashed at the load screen | works after the fix (3.2) |
| Challenge: scripting / UI dependencies | shared + ZH persona data | n/a | untested | first 755 logic frames run, player has control |
| Challenge: victory/defeat/return | ZH `ScoreScreen` -> `ChallengeMenu` | n/a | untested | exit via quit menu -> score screen -> Challenge screen -> Back -> main menu works (3.3); a real victory/defeat not driven (9) |
| Challenge: progression / state persistence (`TheChallengeGenerals` saved in `GameState`) | ZH only | n/a | audited | only through the save (3.3); unlocking by a win not driven (9) |
| Save: during a native ZH game, metadata, "Save/Load" popup | shared `GameState`, `PopupSaveLoad.cpp` identical | experimental | untested | works: file written, listed with description/time (3.3) |
| Return to frontend | shared | | | works (3.3) |
| Load through the normal UI, reconstruct world | shared `GameState::loadGame` | | | works (3.3) |
| Preserve faction, money, units, structures, upgrades, powers, scripts | xfer | | | side, money, rank, 470 objects (every one's full snapshot CRC identical), players CRC identical (3.3); scripts/powers only inside those CRCs |
| Continue after load, save again | | | | works (3.3) |
| Title-specific code in the three modes (checked, not assumed from Generals) | `ChallengeMenu.cpp`, `ChallengeLoadScreen`, `TheChallengeGenerals`, ZH `MainMenu`, ZH `ScoreScreen` | | | the only defect was in the Challenge load screen |

## 3. Automated native evidence (Win64, `x64-46-scratch`, real window, normal launch; this is **not** human validation)
### 3.1 Campaign
`-Flow Campaign`: wake the menu, Single Player, USA, Easy -> `movie skipped MD_USA01` -> terrain/roads/bridges load -> the first playable frames (an in-engine cutscene: the GLA bike convoy; black area = the unexplored shroud edge in the cutscene camera) -> clean exit code 0. The first mission was **not** played through.
### 3.2 General's Challenge
`-Flow Challenge -General 0..4`: Single Player, General's Challenge, Easy, general n, Play -> `movie skipped GeneralsChallengeBackground`, `VSSmall` -> world -> game frame captured (general 0: Air Force General, player base and units, terrain correct) -> exit code 0 for generals 0, 1, 2, 3, 4.
### 3.3 Save / Load (`-Flow SaveLoad`, Challenge general 0, one complete cycle in one process)
Steps recorded in the log: start Challenge; wait 300 logic frames and for input control; Esc; Save/Load; list row 0; Save; description confirm; **state `saved`**; Exit; Yes; score screen OK; Challenge screen Back; main menu; Load/Replay; Load Game; list row 0; Load; **state `after_load`**; 150 logic frames; **state `after_load_150_frames`**; Esc; Save/Load; Save; confirm; **state `saved_again`**; quit.
| State | frame | side | money | objects | structures | logic CRC |
| --- | --- | --- | --- | --- | --- | --- |
| saved | 770 | FactionAmericaAirForceGeneral | 16000 | 470 | 122 | 11301E3B |
| after_load (first running frame) | **770** | same | 16000 | 470 | 122 | 7C420246 |
| after_load_150_frames | 920 | same | 16000 | 470 | 122 | E68EA51F |
| saved_again | 935 | same | 16000 | 470 | 122 | E462E951 |
Per object: **470 of 470 objects have an identical full snapshot CRC before the save and after the load, none missing, none new**; the players component CRC is identical. The aggregate logic CRC differs because three components are not equal after a load: the **partition manager** (cell lists are rebuilt), the **AI** component (a counter-like difference, a few low bits) and the **logic RNG seed**. The RNG seed is not part of the save in this codebase (`GameState` is unchanged from upstream `adac468d7`), so a loaded game continues with a fresh random state: this is how the original save format behaves, not something introduced by the port; I have **not** verified it on the Win32 build (no equivalent unattended driver there), so treat it as "same code, expected the same". The save format was not touched. The simulation continues after the load (770 -> 920, 150 frames, object count stable) and the loaded game can be saved again.
Side effect: the runs wrote 14 test saves into the real Zero Hour user-data `Save` folder; they were all created by these runs (no earlier saves existed) and were **moved** (not deleted) to `build/evidence-archive/zh-x64-46-test-saves/` (git-ignored), so the candidate starts with an empty save list.

## 4. Blockers found, root cause, fix
### 4.1 B-1: General's Challenge crashed on every start (access violation, Win64)
Reproduced unattended (`-Flow Challenge`, exit code `-1073741819` = `0xC0000005`, log `evidence/x64-46-zh-modes/challenge-prefix-crash-generals64.log` ends right after `movie skipped GeneralsChallengeBackground`).
Root cause: `ChallengeLoadScreen::init` (`Core/GameEngine/Source/GameClient/GUI/LoadScreen.cpp`, compiled only for Zero Hour's use) did `m_videoBuffer->allocate(m_videoStream->width(), m_videoStream->height())` right after `TheVideoPlayer->open()`. With no decoder, `open()` returns null, and `W3DDisplay::createVideoBuffer()` returns a valid buffer object, so the `||` did not short-circuit and the null stream was dereferenced. The Generals-side `SinglePlayerLoadScreen`, `Display::playMovie`, `InGameUI`, `WindowVideoManager` and `ScoreScreen` already null-check; the Challenge load screen is Zero Hour only, which is why Generals never showed it.
Fix (narrowest, `LoadScreen.cpp`): if the stream is null, skip the movie, do not create the buffer, run the existing min-spec presentation (`activatePiecesMinSpec`: portraits, biographies, VS overlay) once, and continue loading. No change when a movie opens (Win32 / Bink path unchanged). Regression test: `test_unavailable_movies_are_skipped_not_dereferenced` (17 tests in `test_zero_hour_native_contract.py`). Re-run of the exact flow: passes (3.2).
No other blocker was found in the three modes. The later stalls while building the driver were driver defects (main menu hidden until input; clicks sent to the top window instead of the owner; Load/Replay submenu; quit confirmation box named `QuitMessageBox`; score screen and Challenge screen between a mission and the main menu), not game defects.

## 5. Movie-skip policy (Phase C)
The policy already existed in the shared code and is now consistent and observable:
> movie requested -> `TheVideoPlayer->open()` returns null (no decoder) -> the caller skips the movie and continues the correct transition.
Audit of every caller: `Display::playMovie`, `InGameUI::playMovie` / `playCameoMovie`, `WindowVideoManager::playMovie`, `SinglePlayerLoadScreen`, `ScoreScreen::PlayMovieAndBlock`, `Intro` (null-checked, or tolerant). The one non-conforming caller was `ChallengeLoadScreen` (4.1).
New: `BinkVideoPlayer::open` records every movie it cannot provide as `movie / skipped / <title>` in `generals64.log` (bounded, never fatal). No playback is faked, no mode is blocked because a movie is missing, no completion callback was needed (`notifyOfCompletedVideo` is already disabled upstream; scripted "movie finished" waits are not triggered by a skipped movie and the missions that were run did not wait on one).

## 6. What remains absent to the player (because of the missing movies)
Campaign mission briefing movie (and therefore its briefing text/audio, objective typing, cameo windows, the mission-specific load background), the General's Challenge load-screen movie and the animated portraits / "VS" movie (the static portraits, names and biographies are shown instead), cutscene movies, the final victory movies, the EA logo and sizzle intro. The Generals-style "min-spec" static presentation is what is shown for the Challenge.

## 7. Validation (2026-10-17, on the committed sources; automated only)
| Check | Result |
| --- | --- |
| Win64 build (`z_generals`, `g_generals`, test targets) | OK |
| Win64 ctest | 16 of 16 passed |
| Win64 gtests | `g_googletest` 420 passed, `z_googletest` 419 passed |
| Win32 full build | OK |
| Win32 ctest | 23 of 23 passed |
| Win32 gtests | `g_googletest` 382 passed, `z_googletest` 382 passed |
| Python native-runtime suites (`scripts/native-runtime/test_*.py`) | 226 tests OK (7 skipped, as before); `test_zero_hour_native_contract.py` has 17 tests |
| PowerShell workflow tests (synthetic temp directories, no game launched) | NormalLaunch 22, OrdinaryPlay 45, OrdinaryGame 31, OrdinaryHost 31, OrdinaryMenu 32, OrdinaryTerrain 59 checks, all PASS |
| Stage4B / Generals fingerprint | not re-run: no simulation, renderer or Generals gameplay source was touched (the changes are the Challenge load screen, a log line in the Bink wrapper and the developer capture aid, off by default); Generals still builds and its tests pass |
Not run: `scripts/rendering` (8 known failures unrelated, unchanged), replay re-simulation (not this milestone; R-6 stays open, no attempt to make Win32 match Win64).

## 8. Candidate and human checklist
**Candidate `x64-46-zh-modes-v1`** (AMD64 Zero Hour, normal launch), staged and preflighted (PREFLIGHT PASS, nothing run):
- Path: `build\dev-runtimes\native\ZeroHour\x64-46-zh-modes-v1\game\generalszh.exe`
- SHA-256 of `generalszh.exe`: `5862716d964aa1ff1513ebe9160b1d95ed9d340d2a2a0787d18f5592c2b902f1`
- Launcher (default is this candidate): `scripts\native-runtime\Run-ZeroHourCandidate.cmd` (equivalent: `pwsh -File scripts\native-runtime\Run-ZeroHourCandidate.ps1 -Name x64-46-zh-modes-v1`; `-PreflightOnly` checks without starting). It takes a verified backup of your Zero Hour user data first.
- The candidate carries the developer capture aid but it is inert unless `GENERALS_AUTOSHOT` / `GENERALS_AUTOFLOW` are set.
- The previous candidate `x64-45-zh-bringup-v3` is untouched (use `-Name x64-45-zh-bringup-v3`).
- Your Zero Hour `Save` folder is empty of test saves (they were moved to `build/evidence-archive/zh-x64-46-test-saves/`).

**Checklist (please note what you see; there are no movies, that is expected):**
1. *Campaign*: main menu (move the mouse), Single Player, USA (or GLA / China), Easy. Expect: no briefing movie, a load screen with the default background, then the mission (a cutscene first). Play a minute, give an order, win or lose or quit (Esc, Exit Mission): the score screen appears and you return to the menu. Note anything that hangs, crashes or is black.
2. *General's Challenge*: Single Player, General's Challenge, Easy, pick a general, Play. Expect: no video, a static VS screen with the two portraits, names and biographies, then the mission with control. Esc, Exit Mission, Yes: score screen, back at the Challenge screen. Try a second general.
3. *Save/Load*: in a Challenge (or campaign, after any cutscene) press Esc, Save/Load, choose Save (new slot), name it, confirm. Exit Mission, return to the main menu, Load/Replay, Load Game, pick the save, Load. Expect your base and units as saved, your money, the same general; the game continues; save again. If you win a mission, check that the next mission/general unlocks.
4. Report: any crash (note what you clicked), any black screen or missing UI, anything that cannot be left (stuck screen), and whether the loaded game looked identical.

## 9. Deferred / not done

- A mission played to victory or defeat (campaign and challenge), the next-mission continuation, challenge progression by winning, and the Zero Hour final-victory flow: the driver reaches the world but does not play the game; needs a human (or a scripted win, which would not be the real flow).
- Save/Load of a **campaign** mission (the first campaign mission starts with a cinematic during which the quit menu disables Save/Load by design) and save/load across different generals.
- Win32 comparison of the save/load invariants; R-6 stays open and untouched.
- Movie decoding (out of scope), Enhanced HUD, Options, animated UV, performance, endurance (out of scope).
