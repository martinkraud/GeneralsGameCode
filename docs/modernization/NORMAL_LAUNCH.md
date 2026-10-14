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

## 9. First native run and workflow fix

The developer's first normal launch (evidence: `evidence/x64-31-first-normal-launch/`) exited 0 after about 35 s with a clean shutdown; the wrapper then failed on `Runtime changed: Added: generals64.log`. `Invoke-NativeNormalLaunch.ps1` now archives any stale log, preserves the new one into `<stage>/normal-launch/`, removes it from the stage and re-verifies the stage strictly. Only an added root-level `generals64.log` is tolerated; `Test-NativeNormalLaunchWorkflow.ps1` proves added/changed/removed other files, subdirectory copies and path-like names still fail. Fix candidate: `x64-31-fix1`.

## 10. First review crash and fixes (x64-31-fix1 -> x64-31-fix2)

Evidence: `evidence/x64-31-fix1-crash/` (log, receipt, CrashMG, symbolized analysis; the 780 MB CrashFG stays in the
stage). MG and FG are the same exception (the dumper fires inside `GameEngine::execute`'s `catch(...)`).

| # | Finding | Root cause | Fix / status |
|---|---|---|---|
| 1 | Crash after ~7 min of play | `std::runtime_error` from the D3D11 file-texture bridge: `W3DLaserDraw` (feedback laser from `AssistedTargetingUpdate`) loads its texture with the default full mip chain; the bridge accepted `MIP_LEVELS_1` only, and also threw for any absent/unsupported file | Bridge accepts any requested mip count (top level, same CPU content the legacy chain derived from); absent/invalid/unsupported files use the legacy translucent-magenta placeholder and are reported (`missing_texture` checkpoints, first 16); `TextureClass::Get_Surface_Level(0)` is backed by the CPU content so the laser's aspect-ratio read is real. Test: `NativeFileTextureTest` |
| 2 | Unit voices too slow / deep | Pooled 3D sample handles kept the previous clip's playback rate (Hz). The engine multiplies `AIL_3D_sample_playback_rate` by the event's pitch shift after loading the file, so the stale rate compounded across reuses. Miles resets the rate to the file's native rate whenever a file is set | `NativeMiles` resets the rate on every file set. Source files audited: 22050 Hz mono PCM (speech, 99.9%), 22050 IMA ADPCM (SFX), 44100 music; the mixer maths (`rate/44100`) was verified by duration tests. Tests: `PlaybackDurationFollowsSourceSampleRate`, `PitchShiftScalesTheFilesOwnRate`, `RecycledHandleDoesNotKeepThePreviousClipsRate`, `ImaAdpcmDurationUsesTheHeaderRate` |
| 3 | Skirmish: "Game Speed" label without a control | The retail Generals WND has `SliderGameSpeed`; this fork intentionally hides it (render rate lives in Options) and tried to hide its label `StaticTextBattleHonors1` by comparing text, which did not match | Label hidden by control name. The slider itself stays hidden by design |
| 4 | Skirmish: no Starting Cash | **Not an original base-Generals control.** `ZH_Generals/Window.big` `SkirmishGameOptionsMenu.wnd` has no cash control (72 named windows: map, per-slot player/faction/color/team/start-position, honors, game-speed slider); only Zero Hour's `WindowZH.big` has `ComboBoxStartingCash`. Starting cash for Generals Skirmish is `DefaultStartingCash` from GameData.ini. | Nothing to restore; no custom control added |
| 5 | Selection ring coarse / strong green | Not fixed (renderer fidelity backlog). The ring is a terrain-conforming decal (`EmitDecal`, 2.5-unit steps, capped at 33x33) of a high-resolution texture sampled without mip filtering; large footprints are minified without mips (aliasing, "angular" look) and the decal uses the per-object diffuse at full strength | Next step: give effect textures a generated mip chain + trilinear sampling, then compare blend/alpha against `W3DProjectedShadow`/`W3DShadow` decal semantics |

### Crash evidence handling in the workflow

`Invoke-NativeNormalLaunch.ps1` moves `CrashDumps\Crash(FG|MG)-<date>-<build>-pid<N>.dmp` out of the stage into
`<stage>/normal-launch/crash-<stamp>/`, records them in the exit receipt, still re-verifies the stage strictly,
and **fails the run** whenever any dump exists. Only that exact MiniDumper name pattern is moved; the global
`Read-NativeStage` check is unchanged (`-AllowCrashDumps` is opt-in for the launcher's pre-run read).
Covered by `Test-NativeNormalLaunchWorkflow.ps1` (22 checks).

Fix candidate `x64-31-fix2` (generalsv.exe SHA256 `001733DD04034A08E0B04C47730D2B9EA0D1ADF9A962CB4AE931AE79F2A0A8A8`).

## 11. fix3: native audio lifetime, Starting Cash extension (x64-31-fix3)

What the fix2 review proved: normal launch, Skirmish to victory, a second Skirmish in the same process, the USA
campaign intro/mission, D3D11 world rendering close to retail, no laser/mip crash, and (initially) correct
voice pitch. The remaining blocker was audio degrading over a long session.

### Confirmed root cause: 32 bit completion handles

`MilesAudioManager`'s end-of-sample callbacks (`setSampleCompleted`, `set3DSampleCompleted`, `setStreamCompleted`)
cast the Miles handle to `UnsignedInt` and `findPlayingAudioFrom` cast it back. On AMD64 the handle is a heap
pointer above 4 GB, so the lookup never matched: "Audio has completed playing, but we can't seem to find it"
(a debug-only assert) and the completion was dropped. A finished sound therefore stayed in the manager's
playing list, its pooled 2D/3D handle never returned to the free list, and its priority slot was never
released. Over a long match the pool filled with finished sounds: new voices/SFX were rejected or evicted by
priority (Tomahawk, promotion, Bulldozer, campaign explosions) while music (a dedicated stream with its own
slot) kept playing. The completion path is a pure engine bug, not a NativeMiles allocation leak.

### Further findings and fixes (NativeMiles)

| Finding | Evidence | Fix |
|---|---|---|
| Max distance silenced sounds | `Gains()` zeroed the voice beyond `maxDist`; Miles stops attenuating there and leaves culling to the app | Clamp the distance to the maximum; inverse-distance roll-off from the minimum is unchanged |
| Completion callbacks ran on the mixer thread | the engine's handler loads the next sound portion (file I/O) synchronously; it stalled the mixer and starved waveOut (millisecond crackle) | A dedicated callback thread; deviceless/test mode stays inline |
| WAV decode under the global lock | `AIL_set_sample_file` decoded while holding the mixer lock | Decode first, lock only to attach |
| No visibility | | `NativeMiles::Stats` (handles allocated/released, active/peak voices, completions queued/delivered, starved buffers) reported by `MilesAudioManager::update` as `audio_stats` checkpoints every 30 s in `generals64.log` together with free/playing 2D/3D counts |
| Mixer scheduling | | Mixer thread at `THREAD_PRIORITY_HIGHEST` |

Not changed on purpose: channel counts, buffer size, audible radius (the engine's own limits are the contract).

Tests (`native_miles_test`, 20): 3000 rounds x 6 overlapping plays against pools of 8 handles with the
engine's return-on-callback behavior (no leak, no rejection drift, every completion has the exact handle value),
64 bit handle round trip, looped samples complete once, stop/reuse without callback 200x, handle accounting,
min/max attenuation, listener movement and pan. Engine side is pinned by the contract (pointer-sized handle
types, no `(UnsignedInt)` casts).

### Starting Cash (intentional Generals extension)

Retail Generals Skirmish has no such control; Zero Hour does. The shared plumbing already existed
(`GameInfo::m_startingCash` -> `Player::init` for every player whose template money is 0, human and AI alike).
Added to the Generals Skirmish screen:

* control: a native combo box (same technique as the Options frame-rate selector, appearance cloned from the
  team combo) in the hidden Game Speed slider's slot, captioned by reusing that row's label
* values: Zero Hour's 5000 / 10000 / 20000 / 50000, always including the retail `DefaultStartingCash` (10000),
  marked "(Default)"
* default: every menu entry resets to `DefaultStartingCash` and applies a stored choice only if it is one of the
  offered values; the choice persists in `Skirmish.ini` (`StartingCash`) like Zero Hour; `GameData.ini` is never
  touched; Campaign never reads it (`TheGameInfo` is bound to the skirmish info only for `GAME_SKIRMISH`)
* note: the Generals `getStartingCash()` fallback used the multiplayer-settings default, which is 0 for retail
  Generals (no `MultiplayerStartingMoneyChoice`); it now falls back to `DefaultStartingCash`
* switch: `startingCashExtensionEnabled()` is the single gate for a future Classic/Modern experience selection

### Selection indicator (not changed)

The earlier hypothesis (missing mips) was wrong: effect textures are already built with a mip chain and sampled
trilinearly (`ObjectTexture`, `emitEffect`). The terrain-conforming decal is a 2.5-unit grid; the retail
projection and its blend are not identifiable without an in-game reference comparison. Left for the renderer
fidelity backlog with this correction.

Fix candidate `x64-31-fix3` (generalsv.exe SHA256 `E4E33FBBE67DFCFD60F27FBA1B2E38391CA5ACD49804A9A9E78536E5D2456067`).

## 12. fix4 (x64-31-fix4): positional stereo, Fuel Air Bomb diagnosis, Starting Cash layout

### Evidence from the fix3 run (`audio_stats`, 43 reports over ~21 minutes)

Pools behaved as designed: `free2d`/`free3d` fell to 0 only transiently during heavy combat (peak 30 voices,
25 simultaneous 3D samples) and recovered every time; `completions == delivered` (5989 each, lag 0);
`starved = 0` in every report; `underruns` reached 1 (a single music-decode wait). The 64 bit completion-handle
fix, the callback thread and decode-outside-lock changes are therefore confirmed by the run: no starvation and no
leak. No change to the audio output path was made for crackle; nothing in the evidence calls for one.

### Reversed left/right (root cause)

`MilesAudioManager::setDeviceListenerPosition` passes the camera forward vector with `up = (0, 0, -1)`. Miles is
left-handed (x right, y up, z forward), so its listener right vector is `up x forward`; the `-Z` up is the engine's
explicit compensation for Generals' right-handed Z-up world. NativeMiles computed `forward x up`, i.e. the opposite
vector, so every source was mirrored. Fix: right = `up x forward` (no output channel swap). Front/rear are
unaffected by pan and measure equal loudness. Test `PanFollowsTheEnginesListenerOrientationConvention` covers east/west/
north/south for a listener facing north, east, south and west with the engine's up vector.

### Fuel Air Bomb (Daisy Cutter): not proven offline

Retail Generals' "Fuel Air Bomb" is `SpecialPower SuperweaponDaisyCutter` -> `OCLSpecialPower` ->
`SUPERWEAPON_DaisyCutter` (DeliverPayload: `AmericaJetB52`, payload `DaisyCutterBomb`, delivery distance 140).
What was established statically:

* Whole-engine compile scan for pointer/size truncation (C4311/C4302/C4312) over the Generals and Core game-engine
  and game-engine-device sources (3,024 translation units). 194 distinct hits; every one in the activation ->
  OCL -> DeliverPayload -> locomotor path is a small integer stored in a `void*` user-data slot (module type, bit
  mask, player index) that round-trips; nothing truncates an object, module, handle or callback pointer there.
  The remaining hits are LAN/GameSpy/UI list-box item data.
* The path itself (`onDoSpecialPowerAtLocation` -> `AIGroup::groupDoSpecialPowerAtLocation` ->
  `OCLSpecialPower::doSpecialPowerAtLocation` -> `DeliverPayloadNugget::create` ->
  `DeliverPayloadAIUpdate` states) was read end to end; no 32/64 bit sensitive construct was found, and
  `B52Locomotor` (`Appearance = WINGS`, `Surfaces = AIR`) uses the generic move-towards path.
* Adjacent aircraft-delivered powers share the same nugget/AI/locomotor code (18 DeliverPayload OCLs: Daisy
  Cutter, Carpet Bomb, Paradrop 1-3, Crate Drop, A-10 strikes, Napalm, Artillery Barrage, GLA/China
  cargo planes). A defect in the shared path would affect all of them; one in the B-52/DaisyCutterBomb data only
  the two B-52 powers. The review only exercised one.

The first native review of the statement "the aircraft is created in GameLogic or not" cannot be answered without
running the game, so this build records it: in normal launch `generals64.log` receives bounded `special_power`
and `delivery` checkpoints (command dispatch with science/module/can-do result, OCL activation, aircraft start
data, and a once-a-second trace of position, speed, distance, AI/state-machine state, payload count, disabled/dead
flags, up to 160 records). The next review decides between: command never reaches the module (`dispatch`
missing / `canDo=0`), OCL has no transport/AI (`no_ai`), aircraft exists but does not move or is disabled
(`trace`), or it flies and completes (then the defect is presentation/payload effects, not logic). No gameplay
timers or logic were changed.

### Starting Cash layout (root cause of the "inserted in the Team column" look)

The fix3 combo was parented to the menu root with coordinates read from the slider, which lives in the inset
`SubParent` (origin 42,41): it landed ~41 px up and left of its slot, over the last Players rows. It is now created
under the slider's own parent, one setup-grid row tall (24), on the Players column's left edge and width (x 72,
w 144), vertically centred in the slider's slot under the reused "Game Speed" section header (relabelled "Starting
Cash"). The unnamed "Slower"/"Faster" captions of the hidden slider are hidden as well. Values, default marking,
`Skirmish.ini` persistence, human/AI propagation, Campaign isolation and the single extension gate are unchanged.

Fix candidate `x64-31-fix4` (generalsv.exe SHA256 `93DDC0B22FDD4453F3D58458C7CE57B98A63077B58904DD97ADB54E705D8833E`).
