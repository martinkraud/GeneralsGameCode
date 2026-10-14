# X64.31 — Original Game Experience / Normal Launch

Status: offline batch complete, candidate staged and preflighted, **awaiting human run**. No native execution was
performed by the agent. Evidence classes: *Verified offline* (unit/contract/build), *Traced* (source read, runtime
dependency followed), *Unexercised natively* (needs the developer's run).

## 1. What "normal launch" means

`generalsv.exe` started with **no diagnostic token** (`StartupDiagnostics::normalLaunch()`, AMD64 only).

| Aspect | Reviewed profiles (X64.22–X64.30) | Normal launch (X64.31) |
|---|---|---|
| argv | strict `-nativeOrdinary*` grammar | none required; any ordinary game arguments accepted |
| user data / dumps | isolated run directory | the player's normal `Command and Conquer Generals Data` |
| main loop | profile host loop (`runOrdinaryMenuHost`, bounded) | `TheGameEngine->execute()` — the original loop |
| evidence | 2048 record startup log, classifier | lightweight `generals64.log` next to the exe (cap 4096, failure never affects the game) |
| audio | off (`-noaudio`, dummy manager) | **real** `MilesAudioManager` on the native AIL implementation |
| `m_headless` | TRUE with per-subsystem overrides | TRUE with every override on; the frontend sites that gated on `m_headless` (load screen, score screen, shell return, movies, busy state) key on `!(m_headless && !normalLaunch())` |
| shell map | off in reviewed menu profile | on (`GENERALS_NATIVE_DISABLE=shellmap` is the fallback) |
| presentation | 1200x900 review window over 800x600 logical | same (scale `GENERALS_NATIVE_REVIEW_SCALE`, default 150%) |

Reviewed profiles are unchanged and still pass their contracts: the `-nativeOrdinary*` tokens remain the
repeatable automated path.

## 2. Startup / frontend path audit

| Stage | Original mechanism | Status on AMD64 normal launch |
|---|---|---|
| WinMain, registry, cmdline | `WinMain.cpp`, `CommandLine.cpp` | present; normal launch enters ordinary-host window creation, 800x600 windowed coerced (resolution architecture deferred) |
| Subsystem init order | `GameEngine::init` | present; audio now real; video player created (see below) |
| Splash / logo | `Intro` state machine (EA logo, Sage logo, trailer, then shell) | present; stages that rely on Bink movies skip when the stream is null (Intro tolerates), Traced |
| Intro / opening video | `Display::playMovie` -> `BinkVideoPlayer` | **not available**: `binkw32.dll` exists only 32 bit, no FFmpeg on this machine. `BinkOpen` stub returns null, `open()` returns null, callers skip. No fake video. |
| Frontend background | shell map (`Maps/ShellMap1`) + `MainMenu.wnd` | enabled; shell map rides the accepted native world path (`shellGameActive()`), Unexercised natively |
| Main menu | `MainMenu.cpp` + wnd | proven in X64.23/X64.27; real window manager + mouse |
| Menu music / UI sounds | `MilesAudioManager` via `AudioEventRTS` | audio manager now real: Traced. Music is MP3 in `Music.big` (Media Foundation decode), UI/unit/weapon samples WAV/IMA ADPCM (own decoder) |
| Options | `OptionsMenu.cpp` | UI present. Display-mode list is the single native mode 800x600x32; resolution/AA/texture/gamma settings do not change the renderer (explicitly deferred). Audio sliders go to the real manager. Unexercised natively |
| Single Player / Campaign | `SinglePlayerMenu`, `CampaignMenu`, `GameLogic::startNewGame` | start path reuses the proven Skirmish `MSG_NEW_GAME` path; briefing movie missing (no Bink) so the briefing is audio-less text, mission start unexercised natively |
| Skirmish setup | `SkirmishGameOptionsMenu.cpp` | proven through X64.24–X64.30 (map, faction, team, color, AI difficulty, slots, starting cash are original GUI controls feeding `SkirmishGameInfo`); Unexercised: every combination natively |
| Loading screens | `GameLogic::startNewGame` -> `LoadScreen::init/update` | no longer skipped by headless; draws through window manager. The load screen video (mission/short movie variants) is a Bink video -> absent -> still-image loader. Unexercised natively |
| In-game audio | `MilesAudioManager` | present on native AIL (music, speech, 3D units/weapons/explosions, ambient, EVA) |
| Pause / in-game menu | `QuitMenu`, `PauseKey`, `GameLogic::setGamePaused` | original code, no headless gate; Traced |
| Return to frontend | `GameLogic::exitGame` -> `startNewGame(GAME_SHELL)` -> `Shell::push(MainMenu.wnd)` | previously skipped by headless; now enabled; score screen (`ScoreScreen.wnd`) now shown. Native world object / track / bib / waypoint buffers rely on remove-notification from the scene; second-match resource reuse is the main residual risk |
| Save / Load | `GameState`, `SaveLoad.wnd` | original code, file-system only, no DX8/32 bit pointer serialization in the xfer path audited; engine-save of native-only state not required. Unexercised natively; treat as experimental |
| Replay | `Recorder`, `ReplayMenu` | replay write on game end is original; playback via the same loop. Unexercised natively. CRC identity with Win32 is the existing X64 sim determinism work, not changed here |
| LAN / multiplayer | `LanLobbyMenu`, `GameSpy` | UI code present; networking not modernized (explicitly out of scope). Treat as unsupported |
| Credits / exit | `CreditsMenu`, `MSG_META_DEMO_INSTANT_QUIT` | original code; clean shutdown path exercised in all reviewed profiles |
| Normal shutdown | `GameEngine::~GameEngine`, `NativeShutdownDiagnostics` | present; native audio shuts down the mixer thread and Media Foundation decoders in `AIL_shutdown` |

## 3. DX8 / 32-bit dependencies still in the graph

* **Bink** (`binkw32.dll`): movies unavailable. Needs an AMD64 decoder; explicit follow-up (FFmpeg/vcpkg or a Bink x64 SDK).
* **mss32.dll**: no longer required on AMD64 — replaced by `NativeMiles` behind `MilesLoader`. The 32-bit build is untouched.
* **Miles 3D provider catalogue / EAX / speaker types**: reported as a single "Miles Fast 2D Positional Audio" provider; speaker type accepted and ignored. Options audio-provider list therefore has one entry.
* **GameSpy / WOL**: 32-bit-only and unmodified; not reachable without network menus.
* **Display modes**: native list is one entry; `setDisplayMode` accepts only 800x600 (resolution architecture later).
* **Window/IME/Web browser**: the DX8 browser surface (`W3DWebBrowser`) is unused by Generals main flow.

## 4. Native audio backend (NativeMiles)

Original `MilesAudioManager` logic (event limits, priorities, fades, music transitions, 3D attenuation,
completion callbacks) runs unchanged. `MilesLoader` binds every forwarded `AIL_*` (except the unused `waveOut`
pair) to `NativeMiles` on AMD64:

* mixer thread, 44.1 kHz stereo 16-bit, `waveOut` device; if no device opens, a real-time paced mixer still runs so
  completion callbacks (which game flow waits on) keep firing
* PCM 8/16 bit and IMA ADPCM decoders; `AIL_decompress_ADPCM`, `AIL_WAV_info`
* MP3 music via Media Foundation, decoded progressively on a worker thread (underruns counted)
* 2D samples, 3D samples (min/max distance roll-off, occlusion, listener-relative pan), streams (length available
  without playing — `AIL_stream_ms_position`), quick API, file callbacks into the BIG file system
* offline tests: `native_miles_test` (8 cases: WAV/IMA decode, mixing, loops, pan, stop/resume, 3D gain,
  stream length, ADPCM decompression) — deterministic via deviceless `Render()`

Known differences vs. Miles: no EAX/reverb, no Doppler, linear resampling, no `AIL_set_stream_loop_block`, one
output device (default).

## 5. Input: edge scrolling

Root cause of the X64.30 gap: the original default mode is `EnabledInFullscreenApp` only; the AMD64 runtime always
presents in a window, so edge scrolling was never enabled. Fix (`LookAtXlat.h`): on `_WIN64` the default also
enables windowed. `Options.ini` `ScreenEdgeScrollEnabledInWindowedApp = no` still wins. Mouse positions are mapped
client -> logical 800x600 (`NativeOrdinaryHost::clientToLogical`, unit tests at 100/125/150/200%) before
`LookAtTranslator` evaluates the edge zone, so the 1200x900 review window scrolls at its own edges. Keyboard and
radar navigation use unchanged code. Verified offline (contract + mapping test); feel needs the human check.

## 6. Project identification (not implemented, audited)

* `MainMenu.cpp::initLabelVersion()` already fills an optional `MainMenu.wnd:LabelVersion` with the product
  version/hash string. This is the unobtrusive location: append a second line/suffix `64-bit • DirectX 11`
  (and "Modern Engine" in an About surface) there, behind a native-only branch.
* An Engine Information surface does not exist in the frontend; adding one means a new `.wnd` or Options
  sub-panel — a UI design task, not part of X64.31.
* Do not touch the EA logo/intro text; never imply an official remaster.

## 7. Automated coverage added

* `native_miles_test` (8 tests) — audio backend
* `scripts/native-runtime/test_normal_launch_contract.py` (7 tests) — normal-launch predicates, command line,
  edge-scroll default, audio binding, display hooks
* updated pins in the ordinary menu/play contracts for the shell-game guard helper
* stage/preflight: `Stage-NativeOrdinaryPlay.ps1` (unchanged) + new `Invoke-NativeNormalLaunch.ps1`

## 8. Residual risks for the human run

1. Return-to-frontend after a match: native object/track/bib/shroud resets.
2. Shell map: robustness and frame cost on the main menu (fallback `GENERALS_NATIVE_DISABLE=shellmap`).
3. Music MP3 decode through Media Foundation on the developer's machine; if a stream fails to open the engine
   plays that track silently and the game continues (the failure is not logged by the audio layer yet).
4. Load screen / Campaign briefing without video.
5. Save/Load/Replay never run natively.
