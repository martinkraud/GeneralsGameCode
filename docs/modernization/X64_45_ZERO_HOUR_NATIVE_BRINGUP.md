# X64.45 - Zero Hour native audit and bring-up (CP19)

Status: **CLOSED 2026-10-17 - Zero Hour is playable natively; the terrain-strip defect is fixed and HUMAN NATIVE VALIDATED on candidate `x64-45-zh-bringup-v3` (sections 11 and 12).** The human performs the native runs. Nothing in this
document is a claim that Zero Hour "works" natively: every rendering, audio, UI and gameplay row below is *implemented* or *unverified* until that
run. Method labels used throughout: **Static** (source read / diff against Generals), **Built** (compiled and unit/contract tested),
**Ran headless** (executed by the agent without a window), **Not run natively** (needs the human).

Starting state: `dev/modern-engine` = `origin/dev/modern-engine` = `05617f2b6` (X64.44 closeout), clean tree. Authoritative benchmark fingerprint
`74F2CB528BED42ED` (Generals, unchanged by this milestone: no simulation code was touched).

## 1. Summary

* **Finding (Static): before this milestone `generalszh.exe` could not start as a normal launch.** The AMD64 "normal launch" (a game executable started
  without a diagnostic option) is title independent in Core: `CommandLine.cpp` selects the headless world and `StartupDiagnostics::normalLaunch()` is
  true for `generalsv*` *and* `generalszh*`. Only Generals carried the hooks that turn that selection into the real application. In Zero Hour no
  application window was created (`WinMain` skips `initializeAppWindows` when `m_headless`), the mouse and window manager were dummies, the shell
  push and load screens were gated on `m_headless`, and the shared display initialisation fails closed on a missing `ApplicationHWnd`
  (`W3DDisplayOrdinaryHost.cpp`, "ApplicationHWnd client is not the expected size"). The earlier Zero Hour milestones (X64.12-X64.20) only ever ran the
  *diagnostic* init-only and graphics-smoke modes. `GAMEPLAY_RENDERING_COMPLETENESS.md` section 12 had already recommended a Zero Hour
  parameterization batch; it had not been done.
* **Done (Built):** the existing, reviewed Generals native hooks were ported to Zero Hour mechanically (section 4). The shared native terrain code no
  longer gates roads, bridges, waypoints and bibs on the title. The Win64 and Win32 builds and all test suites pass (section 5). Zero Hour's headless
  init-only gate passes on the current build with the real Steam assets (**Ran headless**, evidence `evidence/x64-45-zh-init-only/`).
* **Prepared:** candidate `x64-45-zh-bringup-v3` (v2 was the first one, run once by the human; section 11), staged outside the Steam installation, integrity-preflighted (section 8). **Not launched.**
* **Not established (important):** any native Zero Hour behaviour, Zero Hour replay determinism (the one available Zero Hour replay does not
  re-simulate on *any* build, including the 2026-10-07 baseline; section 7), performance, the Zero Hour-only features (section 3).

## 2. What already works for Zero Hour through shared code

Everything below is one implementation compiled for both titles. "Proven" refers to the Generals natively accepted milestones (see `FIRST_PLAYABLE_ROADMAP.md`),
not to Zero Hour.

| Shared layer | Source | Evidence that applies to Zero Hour |
| --- | --- | --- |
| D3D11 backend, texture bridge, Render2D / UI, fonts | `Core/Libraries/Source/WWVegas/WW3D2/Backend` | Built for both; native-accepted on Generals (X64.16-X64.28); Zero Hour graphics smoke accepted historically (X64.16/X64.20) |
| Native audio (`NativeMiles`, Media Foundation MP3, WAV/IMA decoders) | `Core/GameEngineDevice/Source/MilesAudioDevice` | Built for both; Generals accepted (X64.31); Zero Hour audio never run natively |
| Native terrain, water, shroud, particles, shadows, scorches, ghosts, decals, draw modules incl. the Zero Hour-only ones (Laser, Rope, ProjectileStream, Overlord*, PoliceCar, Dependency, Science) | `Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldObjects.cpp`, `NativeWorldEffects.cpp`, `.../Drawable/Draw` | Built for both; Generals accepted (X64.29); Zero Hour never rendered natively |
| Modern Display (native resolution, borderless, monitor, UI scale, Hor+ camera, VSync, Render Scale, FPS cap), Modern settings storage, understated footer | `Core/GameEngine/Include/Common/DisplayRuntime.h`, `DisplaySettings.h`, `Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp`, `OptionsTabs.cpp` | Built for both; Generals accepted (X64.33/X64.37). Zero Hour had no Options-screen or HUD hooks until this milestone |
| Smooth presentation / ground interpolation | `Drawable.cpp`, `GroundTranslation` | Hooks already in both titles (X64.34) |
| Simulation work (ZoneCalc, open-list index, line-walk memo, tail reduction, telemetry) | `Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp` | Shared and serial; exactness was measured on Generals (fingerprint `74F2CB528BED42ED`) - **not** on Zero Hour (section 7) |
| Persisted-settings engine, `-classic` switch, `GENERALS_NATIVE_DISABLE` crash-isolation list, normal-launch log `generals64.log` | `StartupDiagnostics.h`, `ModernExperience.h` | Title independent |

## 3. Audit matrix

Legend: **IMPL** implemented, **PART** partial, **UNVER** implemented/ported but never executed natively, **BROKEN** known not to work, **DEFER** consciously deferred.

| Zero Hour area | State | Evidence / note |
| --- | --- | --- |
| Win64 build of `generalszh` | IMPL | `build/win64/GeneralsMD/Release/generalszh.exe` builds clean of new warnings; pointer-truncation warnings only in 5 harmless spots (small integers stored in `void*` item data in `OptionsMenu.cpp`, `WindowVideoManager.h`) |
| Win32 legacy build of `generalszh` | IMPL | builds; ctest 23/23, gtest 382 pass; no Win32 behaviour changed (every new branch is `normalLaunch()`/`_WIN64` gated) |
| Headless init-only (`-nativeInitOnly`) on real assets | IMPL (Ran headless) | PASS on the current build: INI/archives/subsystems, base assets resolved from the staged `ZH_Generals`, clean shutdown, exit 0 |
| Normal launch window / display / shell / mouse / window manager | BROKEN before, PART now | ported (section 4); **UNVER**: never executed |
| Native terrain, objects, water, shroud, particles, shadows | UNVER | shared code; first Zero Hour frame has never been drawn |
| Roads, bridges, bibs, waypoints, ghost objects, hint objects | UNVER | ported from Generals; Zero Hour's road buffer needed its own traversal (per-segment visibility) |
| Animated UV mappers (Rotate, SineLinearOffset, StepLinearOffset, ZigZagLinearOffset) | PART / DEFER | Zero Hour's mapper classes have different internals (`RadiansPerMilliSec`, `ClampFix`, `HalfPeriod`/`Remainder`); `Native_Evaluate` returns false for them, so those textures are static natively. Cosmetic; needs a derivation from Zero Hour's own `Update_Mapper` |
| Factions, generals (12 sides), subfactions, general's powers, command sets, units, buildings, special powers | UNVER | data driven (`INIZH.big`, loaded by the init-only gate); presentation shared |
| Zero Hour shortcut sidebar | IMPL (retail) | Zero Hour ships the taller bar; the Generals data overlay (`modern_extensions.py`) is never applied to it |
| Skirmish setup, Starting Cash, AI difficulty, slots | UNVER | Zero Hour has the native Starting Cash control (the Generals extension is not used); Game Speed slider already hidden in the earlier Zero Hour work; Skirmish screen has no Modern layout work |
| General's Challenge, Campaigns, briefing/cutscene movies | UNVER / DEFER | no Bink decoder natively (as for Generals): movies are skipped, text/still images only |
| Frontend (Main Menu, Options, Load/Save, Replay menu, Score screen) | PART | footer + Modern Options ported; **3 Zero Hour-only Options controls are not placed on any Modern Options page and are hidden there**: `CheckDoubleClickAttackMove`, `Retaliation`, `CheckHeatEffects`. Their saved values (`UseDoubleClickAttackMove`, `Retaliation`, ...) still apply; `-classic` shows the retail page |
| Enhanced HUD (USA/China/GLA) | DEFER | reviewed against the Generals control bar only; Zero Hour sides are not "supported" (single gate in `ControlBarScheme.cpp`), so the Interface option has no effect there |
| HUD placement under Modern Display (superweapon timers, named timers, subtitles, popup messages, player-info list) | UNVER | ported (`DisplayRuntime::hudX/hudY/referenceScale`) |
| Widescreen / ultrawide / borderless / monitor choice / Render Scale / VSync / FPS cap | UNVER | shared Core; Render2D viewport now follows the UI screen rectangle in Zero Hour too |
| Audio (music, speech, SFX, EVA) | UNVER | shared `NativeMiles`; Zero Hour audio archives (`AudioZH.big`, `SpeechZH.big`, `MusicZH.big`) are staged |
| Radar / minimap | UNVER | shared `W3DRadar` path |
| Save / Load | UNVER | original code; experimental as for Generals |
| Replays: recording / playback | UNVER | see section 7; native process reads the 64-bit registry view, so the registry `Version` is the default 65536 instead of the Steam value 65540 (affects replay/network version stamps only) |
| Determinism (Zero Hour) | NOT ESTABLISHED | section 7 |
| Performance, endurance, pathological huge-army workload | NOT TESTED | the telemetry (`GENERALS_ENDURANCE`) works for both titles; run it on Zero Hour after the first candidate is accepted |
| `Player.cpp` shortcut-source fix (cargo planes ignored as special-power sources) | DEFER | a Generals fix with no evidence yet that Zero Hour has the same defect; do not port blindly |
| LAN / GameSpy / multiplayer | DEFER | not modernized for either title |
| Game Speed, multithreading, Enhanced AI, Ultra effects | DEFER | out of scope (X64.44 decision A) |

## 4. The port (Built; same discipline as Generals)

No new behaviour: every hook exists in the Generals tree, was reviewed there and is `#if defined(_WIN64)` or `normalLaunch()` gated.

| Area | Zero Hour file(s) | Change |
| --- | --- | --- |
| Window | `Main/WinMain.cpp` | checked ordinary-host window + release (byte-identical to the Generals block), `ModernExperience`, begin checkpoint "GeneralsMD AMD64 normal launch" |
| Engine | `GameEngine.cpp` | plain caption in Modern mode; native particle manager selection |
| Client | `GameClient.cpp` | keyboard / mouse / window manager real instead of dummy under the profile |
| Logic | `GameLogic.cpp` | ghost manager, busy display, load screens, shell push no longer skipped by `m_headless` in a normal launch |
| UI | `InGameUI.cpp`, `W3DInGameUI.{h,cpp}` | movie gates; `DisplayRuntime` HUD / subtitle / popup / timer placement; hint render objects through `NativeWorldObjects` |
| Frontend | `OptionsMenu.cpp`, `MainMenu.cpp` | Modern Options (display / graphics / audio / controls / gameplay pages), confirmation dialog, footer branding |
| Render2D | `WW3D2/render2d.cpp` | viewport = UI screen rectangle (UI scale / anchors) |
| Device | `W3DBibBuffer`, `W3DBridgeBuffer`, `W3DRoadBuffer`, `W3dWaypointBuffer`, `W3DGhostObject` | native (CPU-backed) accessors and headless-world allocation |
| Core | `HeightMap.cpp`, `BaseHeightMap.{h,cpp}`, `NativeWorldEffects.cpp` | the `RTS_GENERALS` gates on roads / bridges / waypoints / bibs removed |
| Core | `ControlBarScheme.cpp` | Enhanced HUD not offered to Zero Hour sides |

Method: the Generals changes since the upstream merge-base (`adac468d7`) were compared line by line with the Zero Hour twins; hunks already present were
skipped, absent ones applied with a whitespace-insensitive locator, and the rest edited by hand. Files that stay intentionally different (allow-list in
`scripts/native-runtime/test_zero_hour_native_contract.py`): `GameLogic.cpp` (Generals-only setFPMode diagnostic receipts), `GameClient.cpp`
(`startMenuOnlyShell`, a Generals-only diagnostic host entry), `W3DRoadBuffer.cpp` (Zero Hour's own traversal), `WinMain.cpp` (begin text).

**Tests.** New `test_zero_hour_native_contract.py` (12 tests): every Generals hook line exists in Zero Hour except the allow-list; the parity check is shown to bite
(`Player.cpp`); the ordinary-host window block is byte-identical; `GameMain` never dispatches the diagnostic hosts and `WinMain` does not know the menu / game /
terrain profiles; the diagnostic grammar and hosts still refuse Zero Hour; the Zero Hour road traversal mirrors its own `drawRoads`; staging / launching accept Zero Hour;
Enhanced HUD gate; classic switch. **Three existing assertions that said "Zero Hour is untouched" were narrowed, not removed:** host / play contracts now check Zero Hour's
`GameMain` (the diagnostic dispatch stays untouched) and the play contract counts the two `ordinaryPlay` mentions inside the byte-identical window block; the terrain contract now
requires that the shared terrain source is *not* title gated; the normal-launch staging assertion matches the Generals-only overlay switch.

## 5. Build and test results (this milestone, commit `fb4381382` and scripts `9d5dab6f3`)

| Check | Result |
| --- | --- |
| Win64 `z_generals g_generals z_googletest g_googletest` + the `x64-38-win64` target set | builds; ctest 16/16; `z_googletest` 419 pass, `g_googletest` 420 pass |
| Win32 full Release (`x64-38-win32`) | builds; ctest 23/23; gtest 382 pass |
| Python: `scripts/native-runtime` 219 tests (7 skipped), `scripts/performance` 49, `scripts/diagnostics` 5 | all pass |
| `scripts/rendering` reference tests | **8 failures, identical on the unmodified X64.44 tree (verified with `git stash`)**: pre-existing, unrelated to this milestone, not investigated |
| PowerShell workflow tests (native normal-launch, ordinary play / host / menu / game / terrain, graphics, cleanup, ZH cleanup, development-artifact cleanup) | all pass; `Test-ZHWorkflow.ps1` deliberately not run |
| Headless determinism fingerprint (Generals) | not re-measured: no simulation or telemetry code changed in this milestone (X64.44 closeout measured `74F2CB528BED42ED`) |

## 6. Staging policy and what was not touched

* The Steam installation is only read (the stagers copy `*.big`, `Data`, `ZH_Generals` out of it). No CMake `Install`, no executable copied into it, no DLL replaced.
* Candidate runtimes live under `build/dev-runtimes/native/ZeroHour/` (git-ignored). The protected stages `ZeroHour\x64-20-fix1` and `zh\{baseline, stage3c2, stage3c3 (+pre-surface-audit), stage4a3}` were
  neither run nor modified (only a read-only copy of three of their executables was used for section 7).
* Temporary stages created and removed by this milestone: `x64-45-zh-bringup-v1` (superseded: its build changed; the launcher guard rejects it), `x64-45-zh-scratch` (init-only run, evidence kept), `zh\x64-45-w32-replay`
  (Win32 headless replay check, evidence kept). No retention tool was executed (the native tool's plan would also have archived an unrelated Generals candidate).

## 7. Offline / headless validation performed

| Item | Result |
| --- | --- |
| Zero Hour init-only on the current Win64 build with the real Steam assets (scratch stage, isolated user data) | **PASS**, exit 0, `base_assets` resolved to the staged `ZH_Generals`, `init_only complete`. Receipts: `evidence/x64-45-zh-init-only/` |
| Architecture, dependencies | candidate imports system DLLs only (`d3d11`, `dxgi`, `D3DCOMPILER_47`, `DINPUT8`, `WINMM`, `WS2_32`, CRT, ...); no `mss32.dll` / `binkw32.dll` (audio is native, movies are absent) |
| Assets | 278 files copied and hash-verified against the source inventory (`INIZH`, `W3DZH`, `TexturesZH`, `TerrainZH`, `WindowZH`, `EnglishZH`, `AudioZH`, `SpeechZH`, `MusicZH`, `MapsZH`, `ShadersZH`, patch archives, `Data`, `ZH_Generals` base archives) |
| Base-game assets in a native normal launch | found by the recursive `*.big` scan of the working directory (the staged `ZH_Generals`); the Generals registry key `InstallPath` does not exist on this machine in either registry view and is not needed |
| Registry (64-bit view) | the Zero Hour key exists only in `WOW6432Node`; a 64-bit process therefore falls back to its defaults: language `english` (correct here), user-data leaf `Command and Conquer Generals Zero Hour Data` (the same folder), `Version` 65536 (Steam: 65540) |
| User data | the native process uses the player's real `Documents\Command and Conquer Generals Zero Hour Data` (525 MB: Maps, Replays, Save, `Options.ini`, `Skirmish.ini`). The launcher takes a verified full backup first |
| Pointer-width hazards in Zero Hour-only code | Win64 compile shows pointer-truncation warnings only in `OptionsMenu.cpp` (4, small integers carried in `void*` combo item data) and `WindowVideoManager.h` (video windows, unused without Bink) |
| `m_headless` uses | the set of `m_headless` call sites in the Zero Hour tree equals the Generals tree (the only difference is the Generals-only diagnostic `GameMain` branch), so no un-gated headless skip remains |
| Zero Hour replay re-simulation (Win32 `generalszh.exe -headless -replay`, the only available replay `00000000.rep`, 74 s, recorded 2026-10-09) | **CRC mismatch at frame 100 with the current build and also with the Win32 builds of stages `baseline` (2026-10-07), `stage3c3-ground-hardening` and `stage4a3-performance-harness-fix1`** (`evidence/x64-45-zh-replay-check/`). So it is not caused by the optimizations or by this milestone; cause unknown (candidates: replay recorded under different options / content, start-cash or map identity). **Zero Hour determinism is NOT ESTABLISHED.** The first native candidate run records a fresh Zero Hour replay for a proper check |

## 8. The current candidate (v3; v2 was run once, see section 11)

* **Runtime:** `C:\Users\buskr\Documents\ProjectsX\GeneralsGameCode\build\dev-runtimes\native\ZeroHour\x64-45-zh-bringup-v3\game\generalszh.exe`
  (AMD64 PE, SHA-256 `05BCFA054EA33D230497A08786FBBB3C7C6D507785B58AB79E353C4D7467593B`, built from `a1211da11`; the stage manifest pins it and the launcher re-verifies the whole stage).
* **Launcher** (repository root; nothing else is needed): double-click `scripts\native-runtime\Run-ZeroHourCandidate.cmd`, or

```powershell
pwsh -NoProfile -File scripts\native-runtime\Run-ZeroHourCandidate.ps1 -Name x64-45-zh-bringup-v3
```

  Options: `-PreflightOnly` (verify, print the plan, write and start nothing - already run, PASS), `-Classic` (starts with the player-visible `-classic` switch: every Modern Experience extension off),
  `-Endurance` (telemetry to `build\endurance-telemetry\...`), `-AcknowledgeSharedUserData` (skip the 525 MB backup). By default the launcher first runs the verified `Backup-ZHUserData.ps1`
  (same rule as `Launch-ZHRuntime.ps1`), snapshots `Options.ini` / `Skirmish.ini` / `SkirmishStats.ini`, then starts the game exactly like a player would (no diagnostic token, working directory = the stage).
* **Expected startup (from the Generals experience; Zero Hour is unverified):** no EA logo or videos (Bink is absent), a window appears (Modern Display resolves the stored settings; first run uses automatic defaults),
  the Zero Hour main menu with the shell-map background and music, the footer "Modern Experience by MKRaudstein | 64-bit • DirectX 11", Windows cursors. `generals64.log` is written next to the executable
  and moved to the evidence folder when the game exits.
* **First-test checklist** (stop at the first failure; do not retry blindly - section 8.1):
  1. Preflight: `-PreflightOnly` prints PASS.
  2. Launch with the `.cmd`. Note the time until a window appears and until the main menu is drawn; note anything black, white or missing.
  3. Main menu: Zero Hour art, buttons react, music plays, footer text, cursor.
  4. Click through Solo Player to Skirmish and back; check that Back works. (Do not open Multiplayer.)
  5. Options: open all pages; move the audio sliders; check Display choices are listed. Accept once without changing the display mode. Optional: one window-mode change with the Keep / Revert dialog.
  6. Skirmish setup: choose a map, a side **and a general** for yourself (note portraits / names), add 1-2 AI opponents, check the Starting Cash selector, Start.
  7. Loading screen: appears, progresses, ends (note the duration).
  8. First world frame: terrain, base, trees, water if any, shroud, camera; HUD, radar, money and power read-outs.
  9. Camera: edge scroll, keys, mouse wheel / zoom, rotate.
  10. Select a unit and a building; command-bar buttons for that side; build a structure (placement arrow), build a unit, move and attack orders (move hint).
  11. General's powers / shortcut sidebar: visible, buttons present; use a power if one is available.
  12. Let the AI play for a few minutes: AI builds and moves; explosions, particles, shadows, shroud, bridges / roads if the map has them.
  13. Audio: music, unit voices, weapons, EVA announcements, ambient; note crackle or dropouts.
  14. In-game Options / pause; leave the match through the menu (or win / lose); the score screen; back in the main menu.
  15. Optional: a second Skirmish from the menu; one Save and Load.
  16. Exit Game from the main menu. The launcher prints "PASS: process exited with code 0". Keep `Replays\00000000.rep` (it is the match you just played).
* **Known limitations of this candidate:** no videos / cutscenes; the Enhanced HUD is inert; three Zero Hour-only Options controls are hidden on the Modern pages (use `-Classic` to see the retail page);
  some animated scrolling textures are static (mapper table, section 3); LAN / GameSpy are not modernized; the Zero Hour campaign and General's Challenge are unexercised; the registry `Version` stamp differs from Steam's.
* **Crash-isolation switches** (set before the launcher, PowerShell): `$env:GENERALS_NATIVE_DISABLE = 'particles,ghosts'` (any of `particles, ghosts, shadows, water, shroud, lines, scorches, mappers, bibs, waypoints, shellmap`),
  or `-Classic`. Do not use them on the first run; they are for isolating a crash afterwards.
* **Evidence locations:** per run, `build\dev-runtimes\native\ZeroHour\x64-45-zh-bringup-v3\normal-launch\<stamp>.exit.json` (exit code, timing, stage-restored verdict) and `<stamp>.generals64.log` (checkpoints,
  audio statistics every 30 s, special-power and delivery traces); crash dumps are moved to `normal-launch\crash-<stamp>\` and the launcher then fails the run; the engine also writes
  `Documents\Command and Conquer Generals Zero Hour Data\CrashDumps`; backups under `build\dev-runtimes\userdata-backups\` (`<timestamp>-<guid>\` full backup, `zh-native-settings-*` snapshot).

### 8.1 If it fails

Do not rerun, rebuild or restage. Leave everything as it is and keep: the whole `normal-launch` folder of the stage, any `CrashDumps` file, the `Documents\...Zero Hour Data\Replays` folder, a screenshot or phone photo of
the last thing on screen, and a sentence on the last step you completed. The launcher already moved the log and dumps out of the game folder and verified that the stage is otherwise unchanged; a crash makes
it throw "Game crashed: ... preserved" on purpose.

## 9. Findings recorded by this milestone

| Id | Finding | Class |
| --- | --- | --- |
| Z-1 | Zero Hour had no normal-launch path (section 1); fixed by the port | defect, fixed (unverified natively) |
| Z-2 | The only available Zero Hour replay does not re-simulate on any Zero Hour build (incl. 2026-10-07); Zero Hour determinism not established | open, backlog |
| Z-3 | A 64-bit process reads the 64-bit registry view: Zero Hour keys under `WOW6432Node` are invisible (language, version) | open, low (defaults are right for this machine) |
| Z-4 | Zero Hour animated UV mappers have no native form | deferred, cosmetic |
| Z-5 | Three Zero Hour-only Options controls are hidden on the Modern pages | deferred, low |
| Z-6 | `scripts/rendering` reference tests fail (8) on the X64.44 tree already | pre-existing, not investigated |
| Z-7 | Black terrain strips on the map after the shell map: the native terrain atlas version was a per-map counter (section 11; `AMD64_PORTING_FINDINGS.md` A-7) | defect, fixed, **human native validated (v3)** |
| Z-8 | The v2 replay did not re-simulate (frame-0 CRC mismatch Win64, Win32 crash); the v3 replay re-simulates cleanly on Win64 (0 mismatches over 8 741 frames) but not on Win32 (section 12.3) | open, backlog (x86/x64 equivalence; the v2 case unexplained) |
| Z-9 | Enhanced HUD is inert for Zero Hour (known; required later completeness work, not a regression) | deferred |

## 10. Deferred, risks, decision gate

* **Deferred:** everything marked DEFER above; Zero Hour endurance telemetry; a Zero Hour fingerprint benchmark; Bink playback; the Enhanced HUD for Zero Hour; mapper animations.
* **Risks:** the first run is the first time Zero Hour content meets the native renderer (new W3D prototypes, particle templates, shell map, generals' powers); Modern Options on a layout with three extra
  controls; Zero Hour's `Options.ini` is shared with the Win32 game (the backup protects it); the stage is tied to this exact build.
* **Decision gate (after the human run):** (1) startup + main menu + Skirmish world frame work -> fix whatever the checklist surfaced, then Zero Hour endurance telemetry and a fresh replay re-simulation;
  (2) a crash or blank frame -> preserved evidence first, root cause, a bounded fix, a new candidate; (3) only after Zero Hour is accepted reconsider the serial CPU phase (X64.44 decision A) with Zero Hour numbers.

## 11. First native run (2026-10-17) and the terrain defect

### 11.1 What the human saw
Candidate v2 launched normally and reached gameplay: the window, the shell, Skirmish setup, map load, the first world frame, player objects, AI activity, radar and the original Zero Hour control bar all worked and the process exited with 0 (exit receipt
`first-native-run.exit.json`, log `first-native-run.generals64.log`, both under `evidence/x64-45-zh-terrain-strips/`; no crash dump). One major defect: regular black rectangular strips across the terrain
(`human-first-native-run-1.jpg`, `-2.jpg`). Objects, shroud and UI were unaffected; the strips were aligned with the terrain's cell grid. The Enhanced HUD is inert for Zero Hour (known limitation, kept as required later work).

### 11.2 Evidence preserved before anything else
The fresh replay (`Replays\00000000.rep`, hash `52C11EA4...7BFA`), `Options.ini`, the log, the exit receipt, the candidate manifest and the human screenshots were copied to `build/evidence-archive/zh-first-native-run-20261017/` (git-ignored).

### 11.3 Root cause (proven by an unattended A/B, not guessed from the screenshots)
* The data was ruled out first: the map's 21 texture classes all resolve and decode, the atlas placement leaves nothing unplaced, the lighting is the same as Generals', the Generals and Zero Hour versions of the map have the same steepness profile.
* The same map started with `-devMode -quickGame` (no shell map before it) rendered correctly at start positions 0, 2 and 3; the human's sequence (shell map first, then Skirmish) did not. A one-switch developer aid (`GENERALS_AUTOFLOW=skirmish`, section 11.5) reproduced the
  human's sequence unattended, through the real screens: **strips present, 13.12 % pure-black pixels in the world view** (`unattended-shell-then-skirmish-BEFORE-fix.jpg`).
* `HeightMapRenderObjClass::renderNative` re-uploads its tile atlas only when `WorldHeightMap::m_nativeAtlasVersion` differs from the uploaded one. That version was a per-map counter incremented once when the atlas is built, i.e. **1 for every freshly built map**.
  The terrain render object outlives its maps and keeps its native resources when the next map has the same draw size (the vertex-buffer tiles are reused, only the vertex contents are rewritten). The map after the shell map was therefore drawn with the shell map's atlas:
  its UVs address its own class layout, which in the old atlas lands on other textures and on the uncovered (black) texels between class blocks, in the periodic pattern of the atlas grid.
* Fix (`WorldHeightMap.cpp`, one function, `buildNativeTerrainAtlas`): the version is a process-wide serial. With only that change the identical unattended sequence gives **0.06 %** black (`unattended-shell-then-skirmish-AFTER-fix.jpg`).
* **Shared or Zero Hour specific:** the code is shared with Generals. It only shows when consecutive maps keep the same draw size and have different class layouts; an unfixed Generals build did not reproduce it in the one sequence tried (different shell map, different map), so why the
  Generals runs never showed it is **not established** - the fix covers both titles. The road-buffer difference identified earlier (Zero Hour's per-segment visibility culling) was **not** involved: the strips are on the base terrain pass and disappear with the atlas fix alone.
* No retained Zero Hour stage can date the defect: the Win32 stages use the legacy renderer and the earlier native Zero Hour run (`x64-20-fix1`) was a headless/graphics smoke without a world. It appeared with the first native Zero Hour frame sequence that follows a shell map.

### 11.4 Changes
`Core/GameEngineDevice/Source/W3DDevice/GameClient/WorldHeightMap.cpp` (`buildNativeTerrainAtlas`: serial); `W3DDisplayOrdinaryHost.cpp` / `W3DDisplay.cpp` (developer capture aids, and the native Print Screen path used `"%s\Generals64_..."` where a lone backslash-G is not an escape, so the
directory separator was dropped); `scripts/native-runtime/Invoke-NativeAutoCapture.ps1`; contract tests in `test_zero_hour_native_contract.py` (unique serial, inert aids, screenshot path). Finding: `AMD64_PORTING_FINDINGS.md` A-7.

### 11.5 Developer capture aids (new, inert by default)
Only in a normal launch and only when the variable is set: `GENERALS_AUTOSHOT=<existing directory>` writes the composed frame of the shell (after 360 presented frames) and of the first live match (after `GENERALS_AUTOSHOT_FRAMES`, default 300, frames) as BMP and quits; `GENERALS_AUTOSHOT_EVERY=<frames>` writes
a numbered sequence instead; `GENERALS_AUTOFLOW=skirmish` pushes the Skirmish setup screen from the shell and presses its Start button. `scripts/native-runtime/Invoke-NativeAutoCapture.ps1 -Title ZeroHour -Name <disposable stage>` wraps them (a window is shown for about a minute), converts the frames to PNG
and prints the pure-black share of the world view (the regression indicator: 13.12 % broken, 0.06 % fixed). Run it on a disposable stage, never on a candidate meant for a human (the stage integrity check fails afterwards).

### 11.6 Validation after the fix
Win64: `z_generals`, `g_generals` and the full `x64-38` target set build; ctest 16/16; `z_googletest` 419 and `g_googletest` 420 pass. Win32 full Release: ctest 23/23, gtest 382 pass. Python: native-runtime 225 (7 skipped), performance 49, diagnostics 5 pass. The unattended capture of the final executable (disposable stage): 0.06 % black in the match frame.
Human validation of v3 is still required.

### 11.7 Fresh replay (preserved first, then examined; Win32 and Win64 `-headless -replay`)
The replay is the 934-frame match of the first run (the file stays in the git-ignored archive; the console outputs of the re-simulations are `fresh-replay-*` in `evidence/x64-45-zh-terrain-strips/`; the Win32 output is empty because the process crashed).
* Win64 re-simulation: **"CRC Mismatch in Frame 0"** - the first replay CRC comparison already differs. With `GENERALS_SIM_IGNORE_CRC=1` the simulation runs the 933 frames and two runs give the **identical** fingerprint (`FA0C5DA93229F94F`, 31 checkpoints): the simulation is self-consistent; the live recording and the re-simulation
  differ at the start state (the harness counts only the first mismatch, so later CRC agreement or disagreement is **not** established).
* Win32 re-simulation of the same file: the process crashes immediately with an access violation (0xC0000005), before any output; cause not examined.
* This is a different symptom from the old replay (recorded by a Win32 stage; mismatch at frame 100 on every Win32 build incl. 2026-10-07). Zero Hour replay determinism therefore stays **NOT ESTABLISHED**; the frame-0 start-state difference (the family of the Generals R-1 lesson: a setting the replay header does not carry, or one the headless re-simulation sets differently)
  and the Win32 crash are backlog items. No replay semantics were changed.

### 11.8 Remaining native-completeness gaps (unchanged by this fix)
Section 3 still applies: Enhanced HUD (required later work), the three Zero Hour-only Options controls hidden on the Modern pages, animated UV mappers, movies, campaign / General's Challenge unexercised, Save/Load unexercised, audio and performance never judged on Zero Hour, replay determinism (11.7). The first-run observations
(AI active and building, radar, command bar) are positive but are one human run of one map.

## 12. Candidate v3: human native validation (2026-10-17) and closeout

### 12.1 Result: PASS
The human ran `x64-45-zh-bringup-v3`: Zero Hour launched, Skirmish loaded and played, the terrain was correct, **the black strips are gone**, no other major visual defect was noticed and the game appeared normally playable. One short period of minor perceived lag or stutter was reported (brief, not severe).
The terrain atlas fix is **HUMAN NATIVE VALIDATED**; the terrain-strip issue (Z-7, A-7) is closed. The brief lag is **not diagnosed**: that run did not use endurance telemetry, so its source (render, logic, audio, a load hitch, the machine) is unknown; it is a candidate input for the later endurance milestone, not an
action item. The missing Enhanced HUD for Zero Hour was expected and is not a regression.

### 12.2 Evidence (preserved before another game could overwrite it)
`build/evidence-archive/zh-v3-native-validation-20261017/` (git-ignored): the v3 replay (`zh-v3-00000000.rep`, 2 766 997 bytes, hash `450573CA...4C0E`, copy hash-verified against the original), `Options.ini`, the normal-launch log and exit receipt, the re-simulation outputs.
Small files are committed in `evidence/x64-45-zh-v3-validation/`. Exit check: exit code 0, no timeout, no crash dump moved by the launcher (`CrashDumpCount` 0), stage restored; the log ends with `display_detach` and `WinMain complete`, 225 records, no failed phase; the audio statistics every 30 s show `starved=0 underruns=0`, 27 peak voices.
(The CrashDumps folder of the Zero Hour user data holds dumps from 13:50 of the same day: those are from the Win32 re-simulation crash of the *v2* replay during the previous analysis, not from v3.)

### 12.3 The v3 replay (8 741 frames, about 4.9 minutes; existing replay tooling; no replay semantics touched)
| Run | Result |
| --- | --- |
| Win64 `-headless -replay`, CRC enforced | **0 mismatches**, 8 741 ticks, fingerprint digest `777BC1913F7CC00A` (291 checkpoints) |
| Win64, CRC ignored, two more runs | identical digest `777BC1913F7CC00A` each time (3 of 3 identical) |
| Win32 `-headless -replay`, CRC enforced | "CRC Mismatch in Frame 0" (stops at the first comparison) |
| Win32, CRC ignored | runs 8 741 ticks, digest `0E5E16207817C4AE` (different from Win64) |
Reading: a Zero Hour match recorded by the native build **re-simulates deterministically on the native build** (first long Zero Hour determinism evidence, one match, one machine), and the 32-bit build does not reproduce the 64-bit simulation from frame 0. That is the known open cross-architecture question (`REPLAY_DETERMINISM_FINDINGS.md` R-3), now
observed for Zero Hour; the Win32 build also does not reproduce its own older replay beyond frame 100 (R-5). The v2 replay (934 frames) mismatched at frame 0 even on Win64 and crashed Win32; why v2 differs from v3 is unexplained (the v2 match was quit after 22 seconds; not investigated). Replay semantics are unchanged.

### 12.4 Remaining Zero Hour completeness backlog, by severity and dependency
**Severity 1 - correctness risk, never exercised natively (could hide crashes or wrong behaviour):**
1. *Campaign* (missions, briefings, scripted cutscene cameras; depends on how missing movies are skipped) and *General's Challenge* (frontend, generals' portraits and videos, `ChallengeMenu`, `TheChallengeGenerals`).
2. *Save / Load* (xfer of the whole world on AMD64; the Generals side is "experimental" too).
3. *Audio validation by ear* (the mixer statistics are healthy; speech, music transitions, EVA and Zero Hour-specific audio were not judged).
**Severity 2 - visible Zero Hour incompleteness, bounded work, independent of each other:**
4. Zero Hour-specific Modern Options controls (`CheckDoubleClickAttackMove`, `Retaliation`, `CheckHeatEffects`) hidden on the Modern pages (page tables + reparenting + tests).
5. Animated UV / texture mappers (Rotate, SineLinearOffset, StepLinearOffset, ZigZagLinearOffset have no native form for Zero Hour; derive from Zero Hour's own `Update_Mapper`).
6. Movies / cutscenes (no Bink decoder natively: needs an AMD64 decoder or an explicit still-image policy; touches campaign and challenge flows).
**Severity 3 - large Modern Experience feature:**
7. Enhanced HUD for Zero Hour (12 sides, a 14-slot command bar, General's powers, different command sets; a new theme review per side; currently an inert option).
**Severity 4 - later, depends on 1-7:**
8. Replay determinism follow-up (cross-architecture, the v2 anomaly), Zero Hour endurance / performance validation (reuse `GENERALS_ENDURANCE` through `Run-ZeroHourCandidate.ps1 -Endurance`; includes the brief lag), LAN / network (deferred for both titles).

### 12.5 Recommendation (not started)
**Next milestone X64.46 - Zero Hour game-mode completeness:** an offline audit first, then bounded fixes, for Campaign, General's Challenge and Save/Load, together with the movie-skip policy they depend on, using the new unattended capture aids (extend `GENERALS_AUTOFLOW` to those screens) so crashes are found without a human in the loop.
Why first: these are the largest *unverified* areas (a crash or a dead end there is worse than a missing cosmetic), their fixes are bounded and do not change the renderer's architecture, and they need no new Modern feature. Then: X64.47 Zero Hour visual parity (items 4, 5, and 6 if a decoder policy is chosen), X64.48 Enhanced HUD for Zero Hour (item 7, a separate design and review cycle),
X64.49 audio / endurance / performance and the replay follow-up (item 8). Performance work stays after all of them, as instructed.

### 12.6 Is X64.45 closed?
**Yes.** Its aims - audit, port the native normal launch to Zero Hour, first native run, fix what it showed, validate - are met and human-validated; everything left is a new, separately scoped milestone listed in 12.4. Nothing in X64.45 remains open except the backlog items above, which are tracked there, not here.
