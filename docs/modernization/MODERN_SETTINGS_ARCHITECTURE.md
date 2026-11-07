# Modern Settings - architecture record (Settings / UX Completion)

Status: audit + completion work, 2026-11-07, start point `067b1c491`. Read with [SETTINGS_COMPLETION_AUDIT.md](SETTINGS_COMPLETION_AUDIT.md) (X64.37: control-by-control disposition of the display / graphics pages), [MODERN_UI_3_FRONTEND.md](MODERN_UI_3_FRONTEND.md) (the Options screen) and [GAME_SPEED.md](GAME_SPEED.md) (match rule). Nothing here adds a second settings system: the retail `OptionsMenu` gadgets and `OptionPreferences` remain the single source of truth; Modern Options is a presentation, validation and help layer over them.

## 1. Layers

| Layer | Files | Role |
|---|---|---|
| Retail Options screen | `Generals{,MD}/.../Menus/OptionsMenu.cpp`, `OptionsMenu.wnd` | owns every gadget callback, `saveOptions()` (writes `OptionPreferences`, applies to the engine), `setDefaults()`, Cancel (`leaveWithoutSaving`) |
| Preferences | `Core/.../OptionPreferences.cpp`, `UserPreferences.cpp` | key/value file `Options.ini` in the user data folder (`Command and Conquer Generals Data\` / `... Zero Hour Data\`); read at start-up into `TheGlobalData` / subsystems |
| Display configuration | `Common/DisplaySettings.h`, `ModernDisplayOptions.cpp`, `DisplayTransaction.h` | Modern display keys in the same `Options.ini`; transactional apply with Keep / Revert |
| Page tables | `Common/OptionsPageLayout.h` | which retail control lives on which Modern page (pure data, unit-tested); the Keyboard page is generated from `TheMetaMap` at run time |
| Modern screen | `OptionsTabs.cpp`, `Common/ModernScreenLayout.h`, `ModernDraw` | rail + generated rows, scrolling, dirty tracking (`ModernSettings::Tracker`), help strip, Accept / Defaults / Cancel skin |
| Help | `Common/SettingHelp.h` | one factual sentence per exposed control (GameText key + fallback), unit-tested for coverage and tone |

Rule kept from UI.3: the retail gadgets keep every callback, message, sound and state update; Modern code only skins, moves, hides and draws. Classic never creates any Modern layer.

## 2. Where each setting lives

| Category / setting | Backing | Persisted as (`Options.ini` unless noted) | Applied | Title |
|---|---|---|---|---|
| Display: mode, monitor, resolution, UI scale, VSync | `DisplayConfig::Settings` + `ModernDisplayOptions` | `DisplayMode`, `DisplayMonitor(Name)`, `DisplayWidth/Height`, `Resolution` (legacy), `UIScale`, `VSync` | on Accept; mode / monitor / resolution / UI scale only from the main menu (labels say "(main menu)"); a display change shows Keep / Revert with automatic revert | both |
| Display: frame rate limit | `FramePacer` render cap | `FrameRateLimit` (30..240, Unlimited) | on Accept (any time); never changes the simulation | both |
| Graphics: Detail | `GameLODManager` static level | `StaticGameLOD` (+ the Advanced panel keys: `UseShadowVolumes`, `UseShadowDecals`, `UseCloudMap`, `UseLightMap`, `ShowSoftWaterEdge`, `ExtraAnimations`, `DynamicLOD`, `HeatEffects` (Zero Hour), `BuildingOcclusion`, `ShowTrees`, `TextureReduction`, `MaxParticleCount`) | on Accept, from the main menu (load-time LOD) | both (Heat Effects: Zero Hour) |
| Graphics: Render Scale | `WorldRenderScale` (D3D11 intermediate target) | `RenderScale` (100/90/80/75/67/50) | live, also in a match | both |
| Audio: Music / SFX / Voice | `TheAudio->setVolume` per `AudioAffect` | `MusicVolume`, `SFXVolume`, `SFX3DVolume`, `VoiceVolume` | on Accept (retail has no live preview) | both |
| Controls: Alternate Mouse | `TheGlobalData->m_useAlternateMouse` | `UseAlternateMouse` | on Accept | both |
| Controls: Double-click attack move, Retaliation | `m_doubleClickAttackMove`, `m_clientRetaliationModeEnabled` | `UseDoubleClickAttackMove`, `Retaliation` | on Accept | **Zero Hour only** (the retail Generals screen has neither) |
| Controls: Scroll speed | keyboard scroll factor | `ScrollFactor` | on Accept | both |
| Controls: Draw / Move scroll anchor | `TheInGameUI` | `DrawScrollAnchor`, `MoveScrollAnchor` | **at the click** (retail), restored by Cancel (new) | both |
| Gameplay: Use / Save camera in replays | `m_useCameraInReplay`, `m_saveCameraInReplay` | `UseCameraInReplays`, `SaveCameraInReplays` | use: at the click (restored by Cancel); save: on Accept | both |
| Modern Experience: Unit Motion | `GroundTranslation` | `SmoothUnitMotion` | on Accept, main menu | both |
| Modern Experience: Interface (Original / Enhanced HUD) | `EnhancedHud` | `InterfaceStyle` | live, also in a match | both (USA / China / GLA) |
| Network: LAN address | `OptionPreferences::setLANIPAddress` | `IPAddress` | on Accept | both |
| Network: online address, firewall port, refresh NAT | GameSpy `FirewallHelper` | `GameSpyIPAddress`, `FirewallPortOverride` | on Accept; **the original online service is not available** | both |
| Match rules (Starting Cash, AI Behaviour, Superweapons, Game Speed) | `GameInfo` | `Skirmish.ini` (`GameSpeed`, `Superweapons`, `AIBehavior`, ...) | per match | both; **not global settings, never on the Options pages** |
| Key bindings | `TheMetaMap` (`CommandMap.ini` of the game) | none (read-only) | start-up | both |

Command-line overrides that touch settings: `-classic` (Modern off), `-fps N` (render cap), `-noaudio`, `-xres/-yres`, `-win`; developer: `GENERALS_FORCE_UI_SCALE`, `GENERALS_FORCE_CLIENT_SIZE`, `GENERALS_FORCE_GAME_SPEED`. Registry: install paths and the retail `Preorder` flag only; no option is stored there.

Restart-required: none. Settings that need the main menu are disabled in a match with the reason in the label; the rest apply as in the table. Defaults: `setDefaults()` stages retail defaults in the controls (nothing is persisted until Accept) and never touches display settings.

## 3. Transaction model (audited, then made honest)

* **Accept** (button, formerly captioned "Apply"): `saveOptions()` + `acceptFrameRateOptions()` + `pref->write()`, then the screen closes. It never stayed open, so the caption was misleading; it is now **Accept** (the retail word), the hint and the unsaved-changes line say "Accept saves and closes. Cancel discards your changes."
* **Cancel** / Back: nothing is written; the Modern screen asks first when something changed ("Leave Options and discard the changes you made?"). **Fix**: the scroll anchors and the replay-camera switch take effect at the click in retail and were never undone by Cancel; the state in force when the screen opened is now restored (Modern only; Classic keeps retail behaviour).
* **Defaults**: stages defaults in the gadgets (visible as unsaved changes, revertable with Cancel); display settings are left alone.
* **Hint line** corrected: it claimed "Volume and scroll speed apply as you move them", which is not what the retail code does (they apply with Accept).
* A separate Apply-without-closing was **not** added: the display transaction (Keep / Revert, resolution dialog) is built around leaving the screen; changing that is not a Preview requirement.

## 4. Modern Options categories (final)

Display, Graphics, Audio, Controls, Keyboard, Gameplay, Modern Experience, Network. Rows are generated from the page tables; a control that does not exist in a title is simply not a row (Retaliation / double-click attack move in Generals). Every row has a help sentence shown in the help strip when it is hovered or focused ([SettingHelp.h](../../Core/GameEngine/Include/Common/SettingHelp.h)). Display Advanced panel (Detail = Custom) remains the retail popup (documented limitation).

* **Network**: converted from the whole retail block to generated rows: LAN address (the setting that matters for LAN play), online address, firewall port override (text entry), Refresh NAT (button). The retail HTTP proxy and send-delay controls stay hidden (obsolete online-service switches). No networking behaviour changed; the three online items are labelled by their help text as belonging to the unavailable online service.
* **Keyboard**: see 5.

## 5. Keyboard / controls

Finding (archaeology): **the retail engine has no key rebinding.** `ButtonKeyboardOptions` exists in `OptionsMenu.wnd` of both titles but is `HIDDEN` and off-screen; `KeyboardOptionsMenu.wnd` exists only in Zero Hour's `Window.big` (not in Generals) and its Assign button ("check grammar in text field") and Reset button do nothing to the bindings; `MetaMap` records are read from `CommandMap.ini` once and have no setter. Building rebinding would be a new input subsystem (storage, conflict policy, replay-safe behaviour), which the milestone excludes.
Decision (smallest robust, honest): Modern Options gets a **Keyboard** page, a read-only reference of the live bindings grouped by category (Control, Selection, Teams, Interface, Information, Taunts, Miscellaneous), generated from `TheMetaMap` so it is correct for both titles and for any edited `CommandMap.ini`. Keys are written as a player reads them ("Ctrl+Shift+F5", "Num +"; `HotkeyText.h`, unit tested); unassigned commands show "-". Debug commands are not listed. Rebinding remains possible the way it always was (edit the game's `CommandMap.ini`); a Modern rebinding screen is a post-Preview candidate. Classic never shows the page and keeps its retail (hidden) button state.

## 6. Accessibility / readability (Preview pass)

11 pt text floor and 11 pt helper text (previous milestone) kept; help strip is Helper-role text wrapped to two lines; selected tab, focus ring and disabled contrast come from the component table (contrast unit-tested). 1024x768 fits (panel scale reduced, never clipped); long help strings are bounded by the unit test (230 characters). Not done: localisation of the new strings (fallback English is used when a GameText key is missing; the keys are named `GUI:Help...`).

## 7. Known limitations

* Advanced Display popup is the retail popup (centred over the panel).
* No live preview of volume (retail behaviour); no Apply-without-closing.
* The three online Network items do nothing without the original service.
* No key rebinding (see 5).
* The new help / keyboard strings exist only as English fallbacks.

## 8. Evidence

* Unit tests: `SettingHelpTest` (coverage, tone, Network / Controls ownership, match rules are not global settings), `HotkeyTextTest`, `ModernScreenLayoutTest` (help rect, 8 tabs at every surface and scale), `ModernSettingsModelTest`, existing page-table tests. Source contracts: `test_settings_completion_contract.py`, `test_modern_settings_contract.py`.
* Native audit (final binary, both titles): every page (8) at 1024x768, 1920x1080, 1920x1200, 2560x1440, 3440x1440 and 5120x1440 at the default Auto scale; 18-run fixed-scale matrix per title (Options, Main Menu, Match Setup at 75 / 150 %, 1024x768 / 1920x1080 / 5120x1440): 0 defects; the Cancel transaction step (scroll anchor applies at the click, Cancel + Yes restores it); user-data guard: 8 files checked, 0 changed. Classic smoke: the retail Options screen, no Modern chrome (the only Match Setup "defects" are the Modern dropdowns Classic correctly lacks).
* Screenshots: `evidence/ui-6/` (4:3 Display / Controls / Network with help, 16:10 Keyboard, 16:9 Modern Experience, Generals 4:3 / 21:9 / 32:9, Classic Options).
* Not covered: a human check of the new pages and the help wording (owner review), localisation of the new strings.
