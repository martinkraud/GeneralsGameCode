# Stage 3A — persistent render FPS options

2026-10-08, based on clean `dev/modern-engine` checkpoint `b68b2e82f`.
Changes remain unstaged; no commit/push. Stage 3A gameplay/UI/persistence acceptance
is **UNVERIFIED** until the developer manually tests the new candidate.

## Evidence carried forward

The developer manually verified Stage 2: raising render FPS with Ctrl+numpad +
no longer speeds up offline gameplay; 60 FPS was achieved with that control,
camera motion was visibly smoother, and infantry/vehicles/gameplay timing stayed
normal. No obvious smoke-test problem was observed. This supports approximately
30-TPS behavior; actual TPS was not instrumented. Startup `-fps 60` still appeared
to select 30. These are Stage 2 results, not Stage 3A acceptance.

## Architecture and command-line finding

`FramePacer` owns the active render cap. GlobalData's `m_framesPerSecondLimit`
is the configured/reset cap. Packaged GameData previously supplied 30 and graphics
LOD supplied the legacy `m_useFpsLimit` boolean. GameEngine copies GlobalData's
cap into FramePacer late in initialization; ScriptEngine::reset restores it after
temporary script changes. Render hotkeys change FramePacer and the limiter boolean,
without changing the configured cap or writing preferences.

Core `CommandLine.cpp::parseFPSLimit` does exist and assigns the numeric argument
to GlobalData. However, the `-fps` entry in the parser table is inside
`#if defined(RTS_DEBUG)`. The ordinary `win32` Release build has no such parser
entry. This explains the observed failure; it is not evidence of bad launcher
quoting. No CLI feature or parser change is included. Debug builds can still
override the startup cap through their existing parser.

Options callbacks are duplicated in Generals and GeneralsMD. Window layouts and
string tables come from installed BIG assets, not tracked source files. The main
menu loads an OptionPreferences object, populates controls, calls `saveOptions`
on main Accept, then writes preferences and closes. Cancel/Back discards that
object. Advanced Accept transfers advanced settings to the main dialog, rather
than committing the new FPS preference.

OptionPreferences derives from the existing map-based UserPreferences. It loads
`Options.ini` beneath the engine's resolved Documents user-data directory, or
`Options_InstanceNN.ini` for extra client instances. Existing write serializes
the map, retaining unknown keys/values but normalizing order/formatting and not
preserving comments. No new persistence subsystem, registry or GlobalData
architecture is introduced. The sole test seam is an optional constructor flag
that skips loading for in-memory validation tests; normal construction is unchanged.

## Implementation and policy

- `RenderFpsPreset::DefaultFpsValue` is the single authoritative default: **60**.
  Its finite Options list is **30, 60, 120, 144, 165, 240 FPS**.
- OptionPreferences reads/writes **`FrameRateLimit`**. Missing, malformed,
  overflowed and unsupported values fall back to 60 without mutating the loaded
  map. A setter validates as well. Existing Options.ini without this key works.
- Both GlobalData INI overlay paths apply the validated preference after packaged
  defaults. Both late GameEngine initialization paths enable the finite limiter.
  Applying graphics LOD no longer disables it through the legacy LOD flag.
- Shared `FrameRateOptions.cpp` creates native caption/combo gadgets beside the
  resized Detail controls. It reuses their font, image/color appearance and native
  callbacks; owning strings/user data are never shallow-copied. Combo resize
  uses the existing `GGM_RESIZED` child sizing implementation.
- Both packaged layouts were inspected read-only from the protected baseline:
  VideoParent width 236, Detail combo width 144/height 24 at relative (8,113).
  The split provides two 107-wide columns with a 6-wide gap at 800x600 creation
  resolution. The new caption uses the combo's Arial 10 font in both titles,
  including Generals whose original Detail caption uses larger Arial 14.
  There is no asset patch. A mod can provide named
  `OptionsMenu.wnd:ComboBoxFrameRateLimit`; incompatible layouts without room
  safely omit the generated control. Their visual compatibility remains untested.
- Caption uses `GUI:FrameRateLimit` through FETCH_OR_SUBSTITUTE, with English
  fallback "Frame Rate Limit". Numeric entries use `N FPS`. Translations require
  the corresponding external string-table entry; none is silently overwritten.
- Main Accept stores the selected cap, sets legacy **`FPSLimit = yes`**, updates
  GlobalData's reset cap and calls the existing FramePacer setter immediately.
  This runs after other graphics settings so LOD cannot defeat it. Existing
  menu persistence/error handling is reused; no new guarantee of atomic writes.
  Cancel does not apply or write this selection. Defaults selects 60, pending
  main Accept; Defaults followed by Cancel leaves this preference unchanged.
- Existing Ctrl+numpad +/- and Ctrl+Shift+numpad +/- remain unchanged. FPS hotkeys
  are temporary, do not write Options.ini, and can be reset by new-game/script
  reset or main Options Accept. Opening Options shows the saved preference,
  not the temporary hotkey cap. Restart retains its existing current-cap behavior.
  Logic-speed hotkeys remain explicit developer overrides, not graphics options.

Other finite values (50,75,100,180) are representable by the numeric pacer; several
already occur in the developer preset list. A six-choice menu avoids unnecessary
size. The existing developer presets, including 480 and uncapped, are preserved.
165 is valid for the numeric pacer even though absent from that legacy hotkey list.

Unlimited is deliberately omitted. Disabling the limiter returns the existing
1,000,000 FPS sentinel and bypasses sleeping. Offline logic still uses measured
elapsed render time with Stage 2 scaling enabled, but rendering can consume all
available CPU/GPU. Fast-forward, scripts and network paths retain their existing
limiter overrides. No new unlimited semantics or performance claim is invented.

## Complete legacy Skirmish Game Speed audit

Repository-wide searches covered `SliderGameSpeed`, `StaticTextGameSpeed`,
`sliderGameSpeedID`, `staticTextGameSpeedID`, `setFPSTextBox`, the `FPS` preference
and every `MSG_NEW_GAME` producer/consumer. Both title implementations live in
`GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp`:

| Call site | Previous behavior | Stage 3A |
|---|---|---|
| Menu initialization | Loads SkirmishPreferences `FPS`, default GlobalData cap; clamps 15..61 and sets slider | Initialization retained, controls hidden |
| `setFPSTextBox` | Shows number; above 60 shows `--`; disables number when equal to configured cap | Retained for hidden legacy gadgets |
| `GSM_SLIDER_TRACK` | Calls only `setFPSTextBox` | Retained; hidden control takes no input |
| `SkirmishPreferences::write` | Reads slider and saves raw position to `Skirmish.ini:FPS` | Stops replacing that entry; existing loaded key is preserved |
| Back/Start callbacks | Call `prefs.write()` while saving other skirmish settings | Other map/faction/color/name/cash/superweapon/slot semantics unchanged |
| `reallyDoStart` | Reads slider; values >60 become 1000, values <15 become 15; appends cap as fourth new-game argument | Sends configured Options cap for both skirmish and campaign-map branches |
| Reset callback | Slider reset code is commented out | Still inactive |
| IDs/window lookup | Finds slider and numeric label | Retained to avoid asset/layout changes |

The packaged caption is surprisingly named `StaticTextBattleHonors1`; both BIG
layouts confirm its text is `GUI:GameSpeed`. It is hidden only if the named
window still has that localized text, preserving unrelated mod uses of that ID.
Slider, numeric label and verified caption are hidden, not removed from assets.

The fourth argument's only engine consumer is
`GameLogicDispatch.cpp::onNewGame`: validates 1..1000, sets the render cap and
enables the limiter, then starts the game using the first three arguments.
It never sets logic TPS, difficulty, rank points, RNG or player options from FPS.
Previously low slider caps could slow simulation because at most one logic tick
can run per render iteration; that is a consequence of frame pacing, not a
separate speed mechanism. There are no unrelated slider semantics to preserve.

Other fourth-argument senders were inspected: QuitMenu restart preserves its
current FramePacer cap; Recorder optionally supplies replay pacing; Zero Hour
ChallengeMenu forced LOGICFRAMES_PER_SECOND as a render cap. The shared consumer,
restart and replay paths are **unchanged**. ChallengeMenu now supplies the
configured render cap, preserving its difficulty/rank/RNG behavior. Other
producers (MainMenu, MapSelect, ScoreScreen, GameEngine startup, GameState mission
transition, Shell, LAN and Internet staging) do not supply a fourth cap argument.
Recorder's recording path also inspects this argument; its format is unchanged.
No message/wire/replay serialization layout was changed.

## Simulation and compatibility boundary

Stage 2's `m_enableLogicTimeScale = TRUE` and target 30 are intact. BaseFps,
WWSyncPerSecond, accumulator, command scheduling, RNG, save/replay data and tick
units are unchanged. FPS application calls no logic-speed setter. Network rate
queries still precede offline scaling, and network readiness remains authoritative.
Legacy network-start code can disable render limiting; this stage does not
redesign multiplayer pacing. Scripts/fast-forward/developer speed controls retain
their intentional overrides. Replays can retain recorded render pacing overrides.

Below-30 achieved rendering/stalls can still slow logic; offering no cap below
30 does not repair insufficient hardware headroom. No interpolation, world/unit
transform smoothing, renderer, ultrawide/FOV or x64 work is included.

## Automated verification

Run in x86 Visual Studio Developer PowerShell:

```powershell
cmake --preset win32
cmake --build --preset win32
ctest --preset win32 --output-on-failure
git diff --check
```

Release configure/build passed for both titles and tools (MSVC 19.51.36260, X86).
Both CTest executables passed: **11 Google tests each, 22 total**, including four
OptionPreferences tests each for default/missing key, all six accepted values,
malformed/unsupported/overflow fallback, setter validation and unrelated-key
preservation. The existing four FramePacer tests now also check default 60,
logic target 30 and 165/30 pacing ratios. Tests use deterministic timing and
in-memory preferences, not GlobalData construction, registry or Options.ini writes.
Actual UI application and disk persistence remain guarded manual acceptance.

A verified real user-data backup was made before running the existing test suites
(their crash handler can write Documents). Post-test user data still matched it.
Build/test/log/private-backup artifacts remain under ignored `build`.

## Candidate and guarded manual acceptance

The candidate is `build/dev-runtimes/zh/stage3a-fps-options/game` with provenance,
matching EXE/PDB identity and complete source/runtime inventories beside `game`.
Staging uses the explicit Steam source and copies assets/wrappers; it never
installs into Steam. Preserve `baseline` and `stage2-timing` unchanged.
Full source and candidate integrity checks compare all 369 file paths, lengths
and SHA256s. Both preserved runtime inventories also pass all 369-file checks.
Earlier review copies were retained under `stage3a-fps-options-before-*`, with
their original metadata; they are not the final candidate and must not be launched.
No game process is launched by the agent. Validate from PowerShell 7 at repo root:

```powershell
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage3a-fps-options -CheckSource
./scripts/zh-runtime/Test-ZHRuntime.ps1 -Name stage3a-fps-options
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3a-fps-options -BackupUserData -ValidateOnly
```

For the developer's manual launch, use normal persistent configuration, no `-fps`:

```powershell
./scripts/zh-runtime/Launch-ZHRuntime.ps1 -Name stage3a-fps-options -BackupUserData
```

1. With an existing Options.ini lacking FrameRateLimit, confirm 60 selected and
   normal offline speed without hotkeys. Do not delete/edit real user preferences
   to manufacture a fresh configuration. Record map/settings/wrapper/mode and
   measured or inferred FPS/TPS separately.
2. Inspect graphics Detail and Frame Rate Limit captions, dropdown, text, click,
   keyboard focus, Defaults and repeated dialog opening at normal resolutions.
   Check all six choices. Select 144 then Cancel: prior cap/file must remain.
   Defaults then Cancel must likewise leave the saved FPS preference unchanged.
3. Select 120 and Accept. Confirm immediate cap, normal speed, and read-only
   inspection of `Options.ini` showing `FrameRateLimit = 120`, `FPSLimit = yes`
   with unrelated values preserved. The game's normal Options write is expected;
   no script should edit that file. Exit and relaunch with a new backup; expect
   120 restored. Test 30,60,144,165,240 as headroom allows.
4. Start new skirmishes, campaign/Challenge where available and mission restart:
   confirm the preference is not replaced by the old slider/Challenge default.
   Check hidden Game Speed caption/slider/numeric label. Verify unchanged map,
   faction, difficulty, starting cash, superweapon and player settings.
5. Check pause/resume, selection/camera, minimize/restore and clean exit. Compare
   a timed movement/build event at 30 and 60. At 60, use Ctrl+numpad + temporarily,
   verify normal speed and no preference-file rewrite; relaunch restores the
   selected cap. Do not use logic-speed hotkeys to repair normal gameplay.
6. Follow STAGE2_TIMING's Tracy frame-mark/LogicFrame delta procedure for actual
   FPS/TPS. Cap labels indicate requested policy, not measured throughput.
   Report TPS UNKNOWN if no counter capture exists. Keep save/replay/LAN/retail
   compatibility, high-cap load, interpolation and ultrawide results separate.
7. Re-run source/runtime validation for candidate and preserved stages after
   exit; explain any runtime-log drift. Store manual results in ignored build
   results. Never overwrite personal saves or restore backups automatically.

Stage 3B interpolation remains separate. No Stage 3A gameplay or UI success is
claimed from compilation/tests/staging alone.
