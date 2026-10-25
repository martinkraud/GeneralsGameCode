# UI.2 - Modern main menu and navigation shell

Status: **IMPLEMENTED + AUTOMATICALLY VALIDATED. Native human visual acceptance pending (checklist in section 11).** Start point: HEAD `4112e62f5` = `origin/dev/modern-engine` (UI.1).
Design decisions taken by the owner before this milestone: left-docked composition over the real-time shell map; no movie / FFmpeg work; one reusable design system with title variants;
typography preserved and expressed as roles; Classic = preservation; no UI.3 / Options / HUD redesign. Read with [MODERN_UI_ARCHITECTURE.md](MODERN_UI_ARCHITECTURE.md) and [MODERN_UI_ARCHAEOLOGY.md](MODERN_UI_ARCHAEOLOGY.md).

## 1. What was built

| Layer | File | Role |
|---|---|---|
| Layout (UI.1) | `Common/ModernUiLayout.h` | rows / columns / constraints / content width (pure) |
| Focus / navigation | `Common/ModernFocus.h` | `Ring` (ordered focusables, tab order, directional moves, hover hit-test) + `Navigator` (page stack, remembered focus) - pure, deterministic, allocation-free |
| Design system | `Common/ModernTheme.h` | palette, metrics, **typography roles**, title variants (Generals / Zero Hour) - pure data |
| Menu model | `Common/ModernMainMenuModel.h` | pages and entries per title, the reducer (`apply(State, Action) -> Effect`) and the responsive geometry (`compute(surface, scale, title)`) - pure |
| Binding layer | `GameClient/ModernMenuShell.{h,cpp}` | turns the model into real windows / gadgets, relayouts on change, draws the chrome, routes mouse / keyboard |
| Title glue | `Generals/.../MainMenu.cpp`, `GeneralsMD/.../MainMenu.cpp` | the retail statics stay the owner of behaviour: `modernPerform` / `modernStartCampaign` implement the engine side of each action |
| Tooling | `Invoke-UiAudit.ps1`, autoflow `mainmenu` in `W3DDisplayOrdinaryHost.cpp`, `csf_lookup.py` | audit of the real windows at any size / scale, string-key lookup |

## 2. Binding-layer architecture

```
retail MainMenu layout (unchanged WND, callbacks, names)      Modern shell (ModernMenuShell)
   MainMenuInit  ──────────────────────────────────────────►   init(parent, title, hooks)   idempotent: finds its windows by name, creates only what is missing
   MainMenuInput (GWM_CHAR)  ──────────────────────────────►   onKey()          keyboard -> ModernFocus -> performAction()
   MainMenuSystem (GBM_SELECTED / GBM_MOUSE_ENTERING)  ────►   onSelected() / onHover()
   MainMenuUpdate  ────────────────────────────────────────►   update(leaving)   relayout only if the surface / UI scale changed; slide-in animation
        ▲                                                           │ performAction(action)
        │ modernPerform(effect)  (retail statics: campaign, difficulty, shell pushes, exit dialog)   ◄── Effect from ModernMainMenu::apply()
```
* **Ownership / lifetime**: every Modern window (panel, logo, heading, footnote, one push button per action) is a child of the retail `MainMenuParent` and dies with that layout. The shell keeps only a process-wide
  logical state (page stack, remembered focus, selected campaign) so returning from a child screen restores page and focus; the state is reset when a game starts.
* **Retail windows are hidden, not modified**: the WND files and their geometry are untouched; the retail children are hidden at runtime; the retail `MainMenuParent` (an 800x600 canvas, centred, draws nothing) is stretched to the whole
  surface at runtime, because a child can only be hit inside every ancestor's rectangle and a left-docked panel would otherwise be unreachable on a wide surface (found while auditing 3440x1440 / 5120x1440 design - the audit checks real mouse hits).
* **Reuse of retail infrastructure**: the buttons are real `GadgetPushButton`s created programmatically with the retail Main Menu button art (the three-piece `Buttons-*` images and text colours copied from the retail
  `ButtonSinglePlayer`), so hover / pressed / disabled art, text colours and the `GUIClick` sound are the retail ones, per title (Generals' gold frames, Zero Hour's blue frames come for free). Labels use the retail
  GameText keys (`GUI:SinglePlayer`, `GUI:Campaign`, `GUI:SelectDifficulty`, ... - checked with `csf_lookup.py`), so localisation is preserved; the Generals difficulty names (Normal / Hard / Brutal) come from the same keys.
* **Layout is computed, never hard-coded**: `compute()` (pure) is called when the surface, the UI scale or the first show changes (measured: once per run, about 1 - 4 ms); per frame the shell only advances the slide animation and draws.
* **Automation stays valid**: `performRetail()` maps a retail control id (`ButtonSinglePlayer`, `ButtonUSA`, `ButtonEasy`, ...) to the same Modern action, so the existing autoflows (campaign, challenge, save/load) keep working with the retail names.
* **No dead buttons**: a unit test walks every entry of every page of both titles and requires an effect; the retail "recent save" buttons (which had no handler in retail either) are not offered.

## 3. Components implemented (minimum set for the shell)
Panel / container (`panelDraw`: translucent console surface, steel frame, accent corner brackets), menu button (retail art, states: normal / hover / focus / pressed / disabled via the gadget + the focus bracket), section heading + accent rule,
label / helper text (footnote), separator (footer rule), focus indicator (corner bracket in the variant's focus colour, drawn around the focused entry; keyboard focus and hover focus look the same), navigation list (page of entries + footer, `ModernFocus::Ring`),
modal integration (the retail exit confirmation `QuitMessageBoxYesNo` is reused). "Selected" is not used by this screen (documented; the difficulty page pre-focuses the last used difficulty).

## 4. Focus and navigation model (`ModernFocus.h`)
Deterministic tab order = listing order; disabled / hidden entries never take focus and are skipped by every command; directional moves use an explicit neighbour when a screen declares one (`kBlocked` stops), otherwise a geometric rule on item centres
(no fixed coordinates; tested at two scales and offsets); vertical moves wrap; pointer hit-testing gives hover = focus; `Navigator` keeps a page stack with the focused item of each page that was left (`back()` + `restoreFocus()` restore it, falling back to the first
focusable item). Mouse, keyboard (Up / Down / Tab / Shift+Tab / Enter / Esc) and a future controller all reduce to the same commands. Escape at the root does nothing (no accidental exit).

## 5. Main menu (both titles)
Left-docked command panel over the live shell map: logo (the title's own `GeneralsLogo` mapped image), a heading row, five reserved entry rows (so the panel never changes size between pages), a rule and the footer entry.
Pages: **Root** (Solo Play, Multiplayer, Load, Options, Credits / Exit Game) -> **Solo Play** (Campaign, [Challenge: Zero Hour], Skirmish) -> **Campaign** ([Training: Generals], USA, GLA, China) -> **Difficulty** (the retail Easy / Medium / Hard keys);
**Multiplayer** (Network [LAN], Online); **Load** (Load Game, Load Replay). Every entry performs the retail action: Skirmish / Network / Load Game / Load Replay / Credits push the same screens as the retail buttons, Options opens the retail options overlay,
Exit uses the retail confirmation, campaigns and difficulties set the campaign manager and start the game exactly as `prepareCampaignGame` does (without the retail window transitions, whose windows are hidden).
Preserved: the shell map, the `GUIClick` sounds, `GUIButtonsFadeIn` on the entrance, the screen fade into a campaign (`FadeWholeScreen`), the first-interaction reveal (the menu waits for the first mouse movement / key like retail).
Modernized: composition, spacing, hierarchy, focus / hover feedback, keyboard navigation, use of wide surfaces (content composed in a centred 16:9 box, panel about a quarter of its width, sidebar-height capped at 62 % of the surface; the requested UI scale is reduced to fit, never clipped).

**Zero Hour vs Generals**: Zero Hour offers Challenge (General's Challenge via the difficulty page, as retail) and no Training; Generals offers the Training campaign and no Challenge; the logo image and the retail button art differ per title (data); the accent colour is a title token
(Generals gold, Zero Hour orange). Everything else is the same code.

## 6. Classic isolation (evidence)
* `-classic` -> `ModernExperience::classic()` -> `ModernMenuShell::active()` is false: the shell is never built. Native evidence (autoflow `mainmenu`, both titles): "the Modern shell was not instantiated", "no Modern panel window exists", "the retail Solo Play button is visible" (`evidence/ui-2/classic-*.txt`).
* The same switch hides the Modern menu when the display is not the modern one (diagnostic profiles) and `GENERALS_MODERN_FRONTEND=0` switches it off for comparison.
* Static contract (`test_modern_ui_contract.py`): every shell call in both `MainMenu.cpp` files sits behind the gate, the retail `else` branches and the retail `buttonPushed` guard remain, no retail WND is modified, the pure layers are engine free.

### Existing switch fragmentation (documented, NOT redesigned in UI.2)
Modern behaviour is currently selected by: (1) `-classic` (`ModernExperience::classic()`); (2) per-feature `ModernExperience::enabled(Feature)` (= !classic, no independent setting); (3) `DisplayRuntime::modern()` (normal native launch vs the diagnostic profiles);
(4) saved preferences with their own semantics (`InterfaceStyle`, `SmoothUnitMotion`, `DisplayMode`, the Skirmish `AIBehavior` / `Superweapons`, and the X64.52B edge-scroll rule); (5) developer environment variables (`GENERALS_MODERN_FRONTEND`, `GENERALS_FORCE_UI_SCALE`, ...).
Proposed convergence (a later milestone): one `Experience { Classic, Modern }` profile resolved once at startup from the command line, the saved choice and the launch mode, with per-feature overrides stored only under Modern; every feature asks `Experience::has(feature)`; the Options screen gets one honest "Experience" control.

## 7. Resolution and UI-scale results
Real-window audit (autoflow `mainmenu`, `Invoke-UiAudit.ps1`) at 1920x1080, 2560x1440, 3440x1440, 3840x2160 and 5120x1440 x UI scale 75 / 100 / 150 / 300 %, both titles: **40 configurations, 66 checks each (2 640 checks), 0 defects** (`evidence/ui-2/matrix/`, summary `matrix-summary.txt`). Each run checks, for every page: panel inside the display, every button inside the panel, text fits its button (measured with the real font), no button overlaps another, the real mouse hit-test at the centre of every button lands on that button, the page lists exactly the entries of the model plus the footer, a focused entry exists; plus keyboard navigation, back navigation and focus restoration. Requested scales above what a sidebar may take are reduced (e.g. 300 % requested -> 126 % at 1080p, 170 % at 1440p, 253 % at 2160p), never clipped. Screens: `evidence/ui-2/screens/`. Design numbers: panel width 300 - 420 reference units x scale, never more than 60 % of the composition box; panel height at most 62 % of the surface (scale reduced to fit); composition box = the surface up to a 16:9 aspect.

## 8. Cost
`ModernMenuShell` records update and draw time. Idle main menu (both titles, 1920x1080 .. 5120x1440): `update()` about 0.2 - 2 microseconds per frame, the panel chrome draw about 20 - 30 microseconds per frame (about 0.15 % of a 60 Hz frame), geometry recomputed once per run (1 - 4 ms),
no allocation per frame (the layout core is plain data; windows are created once). The simulation is untouched.

## 9. Validation
| Gate | Result |
|---|---|
| Google Test (Generals / Zero Hour) | 507 / 506 passed: +27 new (`ModernFocus` 12, `ModernMainMenu` model / reducer / geometry / theme 15, plus the 12 `ModernUiLayout` of UI.1 = 39 Modern UI tests) |
| Python suites (native-runtime / performance / diagnostics) | 309 / 49 / 5 OK (7 skipped) incl. `test_modern_ui_contract.py` (Classic isolation, gating of every shell call, title glue, no dead buttons contract) |
| Original / Enhanced golden AI gate (headless replays, reference = the X64.51 executables) | **PASS in Generals and Zero Hour**: Original digests equal the reference in every scenario, every run repeatable (`evidence/ui-2/gates/`) |
| Classic isolation (native, both titles, 1920x1080 and 5120x1440) | PASS: the Modern shell is not instantiated, no Modern window exists, the retail Solo Play button is visible (`evidence/ui-2/classic/`) |
| Real-window audit matrix | 40 configurations, 0 defects (section 7) |
| Menu cost | idle main menu: `update()` 0.2 - 2 us / frame, panel draw 20 - 30 us / frame, geometry recomputed once per run (1 - 4 ms) |

Not run: the real-input camera gate (UI.2 does not touch the in-match camera or input path); a real-input mouse/keyboard click test of the Modern menu through the OS (the audit sends the same GBM_SELECTED / GWM_CHAR messages the input system produces and hit-tests the real window tree; the owner's native test covers the physical devices).

## 10. Known limitations
* Online (GameSpy) keeps the retail behaviour (`StartPatchCheck`); the service itself is unavailable upstream, so the entry has the retail outcome.
* The retail "recent save" shortcuts per faction never had handlers and are not offered; Load Game / Load Replay are under Load.
* Options, Load Game, Replays, Skirmish, LAN and Credits are still the retail screens (migrated in UI.3 / UI.4).
* The slide-in entrance is a single ease-out of the panel; the retail per-button flash transitions are not reproduced (they address retail windows by name).
* Typography uses the retail font families through roles; if the "Generals" font family is missing on a PC the GDI substitution applies exactly as in the retail menu (the screenshots show a sans-serif face, to be compared with Classic by the owner).
* Pure checks and the audit cannot judge taste: composition, panel proportions, accent colours and the focus bracket need the owner's eyes.
* Known native-test findings are preserved in `PROJECT_STATUS.md` (allied AI towers, enemy colour flash, Zero Hour Enhanced GLA base, initial command-UI delay, camera / pacing observation).

## 11. Native visual-test checklist (owner)
1. Generals and Zero Hour: start the Modern build; the menu waits for the first mouse movement / key, then the panel slides in from the left over the shell map; compare with `-classic`.
2. 1920x1080 and your widest practical resolution, at least one larger UI scale (Options > Display): the panel stays a left-docked sidebar, not hugging the screen edge on wide surfaces, nothing clipped; the shell map stays prominent.
3. Mouse: hover each entry (bracket + retail hover art + text colour), click Solo Play > Campaign > a faction > difficulty (the campaign must start), Back, Skirmish, Multiplayer > Network, Load > Load Game / Load Replay, Options, Credits, Exit Game (confirmation).
4. Keyboard: Up / Down / Tab / Shift+Tab move the focus, Enter activates, Escape goes back one page (and does nothing at the root), focus returns to the entry you came from after a child screen.
5. Zero Hour: Solo Play shows Challenge; Generals: Campaign shows Training.
6. Sounds and transitions: click sounds, the entrance sound, the screen fade into a campaign.
7. Anything that looks un-Generals: tell us what (panel opacity, accent, fonts, spacing).

## 12. Native-test packages (source commit `4f5e092c1`, one `New-MKRaudsteinDistribution.ps1` run, 26 of 26 validator checks per title)

| file | size (B) | SHA-256 |
|---|---:|---|
| `MKRaudstein-Generals-x64-Preview-4f5e092c1.zip` | 4 071 538 | `4CEA7E8FC9581E5AB6F7CE9AA8AF9C231E17D794476E978406CC061AA5FC34D8` |
| `MKRaudstein-ZeroHour-x64-Preview-4f5e092c1.zip` | 4 177 016 | `7EF1438B48C9E83C3DC24FB7059B2793BB2DA3B73308A50B9C4313E5031EFEB2` |
| `MKRaudstein-Generals-x64-Preview-4f5e092c1-Symbols.zip` | 22 197 882 | (manifest) |
| `MKRaudstein-ZeroHour-x64-Preview-4f5e092c1-Symbols.zip` | 22 638 073 | (manifest) |

The `Generals.exe` inside each ZIP is byte-identical to the freshly built executable of that title; no PDB in the player ZIPs. Retention kept this distribution and the previous one (`f38ba6418`).
Launch the staged Zero Hour candidate: `build\dev-runtimes\native\ZeroHour\dist-4f5e092c1-zh\game\generalszh.exe` or `pwsh scripts/native-runtime/Run-ZeroHourCandidate.ps1 -Name dist-4f5e092c1-zh` (Generals: `build\dev-runtimes\native\Generals\dist-4f5e092c1-gen`), or install the two ZIPs over the Steam installations as before; `-classic` shows the retail frontend for comparison.
