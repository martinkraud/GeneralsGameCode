# UI.3 - Modern Frontend system: full visual redesign and frontend migration

Status: **IMPLEMENTED + AUTOMATICALLY VALIDATED. Native human visual acceptance pending (checklist in section 13).** Start point: HEAD `7b6402506` = `origin/dev/modern-engine` (UI.2 + its package record).
Key acceptance criterion given by the owner: *Modern Experience must look and behave like a newly designed modern Generals frontend, not the retail 800x600 frontend rearranged for a modern resolution - preserve the soul of Generals, replace the constraints of its 2003 interface.*
Read with [MODERN_UI_ARCHITECTURE.md](MODERN_UI_ARCHITECTURE.md), [MODERN_UI_2_MAIN_MENU.md](MODERN_UI_2_MAIN_MENU.md) and [MODERN_UI_ARCHAEOLOGY.md](MODERN_UI_ARCHAEOLOGY.md).

The owner reviews the look next. **If the look is disliked the next step is a visual revision of this system, not HUD work** (section 12).

## 1. Architecture in one picture

```
 pure, engine free (Core/GameEngine/Include/Common)                       engine side (Core/GameEngine/Source/GameClient/GUI)           title glue (Generals / GeneralsMD)
 ModernTheme.h        palette, typography ROLES, metrics, title variants    ModernDraw.{h,cpp}   draw functions + skinning of retail    MainMenu.cpp          (UI.2 hooks)
 ModernComponents.h   Kind x State -> Style (ONE table), chamfer geometry     gadgets + Decor (the chrome of a screen)            OptionsMenu.cpp       update / key / wheel / Back-confirmation
 ModernScreenLayout.h settings screen, modal dialog, list browser geometry  ModernMenuShell    main menu band (UI.2 binding, redrawn)  SkirmishGameOptionsMenu.cpp  create / update / forget
 ModernMatchSetupLayout.h MAP | PLAYERS | MATCH RULES                       OptionsTabs        Options: rail + generated rows         ReplayMenu.cpp, PopupSaveLoad.cpp  browser create / update / forget
 ModernMainMenuModel.h / ModernFocus.h / ModernUiLayout.h (UI.1 / UI.2)     ModernMatchSetup   Match Setup over the retail gadgets    CreditsMenu.cpp       scrim band + type roles
                                                                            ModernBrowser      Load Game / Replays + inline confirms   (GameWindowManager::gogoMessageBox -> ModernDialog)
                                                                            ModernDialog       every MessageBox / QuitMessageBox       ModernCredits      credits scrim band
```
**Rule of the milestone: the retail gadgets keep every callback, message, sound and game-state update; Modern code only skins, moves, hides and draws.** No screen draws a retail button image: buttons, tabs, fields, check boxes, sliders, list and combo chrome are
procedural (`ModernDraw`). The only retail image in the Modern frontend is the title's own logo (`GeneralsLogo`) on the main menu. A screen that is Modern for the player is the same retail screen underneath, which is what keeps automation, saves, replays and Classic safe.

## 2. Typography audit

Finding (checked with `ui_archaeology.py` and Windows font enumeration): the retail "Generals" font family named by the retail WND files and GlobalLanguage is **not installed anywhere** (no TTF in the game or in Windows), so the retail menus were *always* drawn with the GDI
substitute. UI.2 expressed the typography as roles but still named the missing family. UI.3 audits the real runtime font: every `TypeRole` names an ordered candidate list of **real, legally available Windows fonts** - `Bahnschrift` (the DIN-style industrial sans bundled with
Windows 10/11: signage / vehicle-plate feel, fits the field-hardware direction), `Segoe UI`, `Arial` - and `ModernDraw::resolvedFamily()` picks the first one that `EnumFontFamiliesExA` reports installed (a GDI substitution is detected, never silently accepted). The choice is logged at startup
(checkpoint `modernmenu fonts nav=... heading=... body=...`; on the development PC `nav=Bahnschrift heading=Bahnschrift body=Bahnschrift`). **No third-party font file is shipped.**

| Role | Reference size (pt at 100 %) | Weight / case | Used for |
|---|---:|---|---|
| Title | 26 | bold, capitals | screen title (Options, Skirmish, Load Replay) |
| MajorHeading | 19 | bold, capitals | page heading of the main menu, dialog titles, credits titles |
| SectionHeading | 13 | bold, capitals | zone headings (Map / Players / Match Rules), credits positions |
| MenuPrimary | 18 | bold, capitals | main menu navigation |
| MenuSecondary | 14 | bold, capitals | rail tabs, action buttons, Back / Exit |
| Body | 12 | regular | dialog text, credits lines |
| Label / Value | 12 | regular / bold | setting captions / the value a control shows |
| Helper | 10 | regular | hints, footers, column heads |
| Status | 11 | bold, capitals | the dirty-state line |

Fonts are created for the effective UI scale (`fontAt(role, scale)`); a dialog lays itself out at its own effective scale. Labels keep their GameText keys (localisation preserved); capitals are applied at display time.

## 3. Design language and tokens

Military command-system / tactical-terminal DNA, restrained: **chamfered, asymmetric geometry** (cut corners on panels and buttons - never rounded, no glass, no Material elevation), **hairlines** and **accent ticks** instead of boxes, a dark steel scrim that lets the live shell map stay prominent,
the **title accent** as the only colour that differs between the games.

| Token | Value | Notes |
|---|---|---|
| scrim | (6,10,16) alpha 176 | the band behind the main menu navigation; the shell map shows through |
| panel / panelRaised / field | (12,18,27,214) / (24,33,46,224) / (5,8,13,232) | content panels, zone / selected surfaces, the inside of an input |
| hairline / edge | (72,86,104) / (116,132,152) | quiet separators / outlines |
| text / textSecondary / textMuted / textDisabled | (236,240,244) / (178,190,204) / (128,142,158) / (88,98,112) | contrast against the shell map is a unit test (>= 4.5:1, 3:1 on pressed / accent fills) |
| accent | Generals sand-gold (214,178,74), Zero Hour signal orange (232,118,40) | the *only* per-title difference: tick, rules, selection, primary action, headings, value labels |
| danger / warning / ok / focus | (214,84,70) / (236,170,60) / (120,200,110) / title accent tint | Exit Game, dirty state |
| chamfer / chamferSmall / accentTick | 12 / 6 / 4 reference units | all metrics are reference units x the effective UI scale |

`ModernComponents::style(Kind, State, Palette)` is the single table behind every component: kinds **Nav, NavMinor, NavDanger, Primary, Secondary, Danger, Tab, Field, Row**; states **Normal, Hover, Focus, Pressed, Disabled, Selected** with one resolution order
(disabled > pressed > focus > hover > selected). Components built on it: navigation items (accent tick + wash), primary / secondary / danger buttons, rail tabs, combo boxes (field + chevron + list), check boxes, sliders (track, accent fill, handle, value label), list boxes with
vertical scroll bar, text entries, list rows, dialogs (scrim + panel + title + rule + buttons), value labels, status line, scroll indicator. Help text: the helper line of a screen (Options hint / Match Setup hint); the retail pointer tooltips are unchanged.

## 4. Screen migration status

| Screen | Status | Notes |
|---|---|---|
| Main Menu (both titles) | **Modern, redrawn** | full-height band docked at the left of the centred 16:9 composition box; logo, page heading, text-forward navigation items, danger-coloured Exit Game; Solo Play / Campaign / Challenge (Zero Hour) / Training (Generals) / factions / difficulty / Skirmish / Multiplayer / Load pages unchanged in behaviour |
| Single Player / Campaign / Difficulty navigation | **Modern** | pages of the shell above (Generals: Campaign, Training, USA / China / GLA, difficulty, Skirmish; Zero Hour: Campaign, Challenge, factions, difficulty, Skirmish) |
| Options | **Modern, new** | category rail (Display / Graphics / Audio / Controls / Gameplay / Modern Experience / Network), label + control rows generated from the page tables, scrolling when a page does not fit, value labels on sliders, Apply / Defaults / Cancel, dirty-state line and Back confirmation, Page Up / Down + Ctrl+Home / End category keys, wheel scrolling |
| Skirmish / Match Setup | **Modern, new** | MAP (preview with the retail start-position markers, name, Select Map) \| PLAYERS (8 rows x Player / Color / Faction / Team) \| MATCH RULES (cards generated from a spec list: Starting Cash, AI Behaviour, Superweapons; career record) + Main Menu / Play Game |
| Load Game, Replays | **Modern, new** | list + action column; the retail inline confirmations (load, overwrite, description entry, delete) are re-skinned as Modern dialogs over the panel; the full-screen and the in-game popup Save / Load both |
| Common dialogs (confirm, exit, warnings, OK boxes) | **Modern** | every `MessageBox` / `QuitMessageBox` through `GameWindowManager::gogoMessageBox` |
| Credits | **Modern** | the credits roll over the shell map with a scrim band; Modern type roles and title palette |
| Multiplayer / LAN navigation | **Modern navigation, retail destination** | Multiplayer > Network / Online are Modern shell pages; the **LAN lobby / LAN game options** and **Online** (GameSpy, upstream service unavailable) screens are the retail screens |
| Challenge general select, Map select, Advanced Display popup, keyboard options, Network options block, Online screens, in-game menus (quit, popups other than the above) | **intentionally still retail** | each needs its own composition work (portraits, map lists, ...); none falls back silently - they are listed here and in the visual checklist |

Zero Hour vs Generals: the same code and layouts; differences are data (accent colour, logo image, page content: Challenge vs Training, retail GameText keys, the faction lists the retail screens provide).

## 5. Navigation and input

Device-agnostic commands (Tab / Shift+Tab, Up / Down / Left / Right, Enter, Escape, hover = focus, click) reduce to the same model on every Modern screen: the main menu uses `ModernFocus` (ring + page stack with focus restoration, hidden / disabled items skipped, deterministic order); the settings / browser / dialog screens keep the
retail window manager's focus and tab order over the skinned gadgets (all controls are real gadgets, so Tab / Enter / Space / Escape are the retail ones) and add: Page Up / Down and Ctrl+Home / End (Options categories), mouse wheel (Options rows), Escape = Cancel / Back with the discard confirmation on a dirty Options screen.
Back navigation restores the page and the focused entry of the main menu (audited). Existing sounds are unchanged (`GUIClick`, `GUIButtonsFadeIn`); transitions are one 240 ms ease-out slide of the menu band - no other animation.

## 6. Responsive rules (all pure, tested at 1920x1080, 2560x1440, 3440x1440, 3840x2160, 5120x1440 x UI scale 75 / 100 / 150 / 200 / 300 %)
* Content is composed in a centred box of at most 16:9 (`kCompositionMaxAspect`); on ultrawide the extra width stays the live shell map, nothing is stretched. The main menu band docks at the left edge of that box.
* The effective scale is the requested UI scale **reduced until the screen fits** (never clipped): Options needs the rail and four rows, Match Setup the three zones, browsers a list and the action column, dialogs their text and buttons; the effective scale is logged in every audit line.
* Rows / cards / cells are generated from counts (`rowCount`, `ruleCount`); adding one never moves the elements before it (tested relative to the zone); content that does not fit scrolls (`firstRowToReveal`, `clampFirstRow`) - it is never clipped.
* A control is hit only inside every ancestor, so the screens stretch the retail container windows over their composition (the audit hit-tests the real window tree at the centre of every control).

## 7. Classic / Modern boundary
`-classic` (`ModernExperience::classic()`) and the non-modern display profiles create **none** of the above: `OptionsTabs::create`, `ModernMatchSetupScreen::create`, `ModernBrowser::create` return false, `ModernDialog::apply` / `ModernCredits::attach` return at once, the main menu shell is not built.
Evidence: the audit flows check, per screen, that no Modern backdrop window exists in Classic and that the retail controls are visible (`evidence/ui-3/*-classic/`), plus the static contract `test_modern_frontend.py` (every entry point gated, no retail art in the draw layer, hooks in both titles).
Static destructors of the Decor objects never touch the display-string manager (a crash found at process exit during development).

## 8. Performance
Instrumented: `ModernDraw::stats()` (decor draws, button draws, time, display-string allocations), `ModernScreen::computeCount()` (geometry solves), `ModernMenuShell::stats()` (update time, relayouts) - reported by every audit flow (`cost frontend ...`). @@COST@@

## 9. Validation
@@VALIDATION@@

## 10. Known limitations
* Screens listed as intentionally retail in section 4 keep their 2003 look; Online (GameSpy) keeps its retail behaviour and the upstream service is unavailable.
* Options: the Network category embeds the retail Network block unchanged (it is laid out in retail units at the retail UI scale); the Advanced Display popup is the retail popup, centred over the panel. The dirty state covers every Modern-owned control (combos, sliders, check boxes of the six pages), not the retail Network text entries. Volume and scroll speed apply as they move in retail and are not reverted by Cancel - the status line says so.
* Pointer tooltips are the retail tooltips; per-setting help text is not authored yet (the status line carries one generic hint).
* `OptionsPageLayout.h` still contains the retired X64.33 panel geometry (computePanel / computePage / computeTabStrip) because its tests pin the page-table contract; the runtime uses `ModernScreenLayout.h`.
* The dialog / list-row look is judged on screenshots and the contrast unit test; taste (accent, opacity, density) needs the owner's eyes.
* Known native-test findings of earlier milestones are unchanged (enemy structure colour flash, allied AI vs China towers, Zero Hour Enhanced GLA weakness, 1 - 2 s command-UI delay, camera / edge-scroll native question): see `PROJECT_STATUS.md`.

## 11. How to add things (the point of the system)
* **A setting**: add a `PageItem` row to the page table in `OptionsPageLayout.h` (label window, control window, kind). It appears on its page, skinned, placed, scrolled and dirty-tracked with no layout code.
* **A match rule**: add a `RuleSpec` (caption key, control window, optional `replacedBy`) to `kRules` in `ModernMatchSetup.cpp`; the layout takes only the number of rules.
* **A list screen**: fill a `ModernBrowser::Spec` (list, actions, optional inline confirmations) and call `create / update / forget` from the screen's three callbacks.
* **A component**: add a `Kind` and its state rows to `ModernComponents::style()` (test the contrast), draw it with `ModernDraw` primitives.

## 12. Future HUD path (not part of this milestone)
The same system is the base for the in-match HUD: the tokens, the type roles, the component table and the chamfer primitives are not frontend specific; the HUD work (UI.5) will add its own pure layout (`EnhancedHudLayout` already exists and is native-accepted and frozen) and reuse `ModernDraw` for the chrome. **Nothing in the HUD changed here.**

## 13. Native visual checklist (owner) - please answer these after trying both titles
1. Does anything still **resemble the retail frontend** (button art, boxes, fonts)? Which screen?
2. Does it **feel like Generals** (military command terminal) in both titles? Is the Zero Hour orange / Generals gold accent right?
3. **Main menu composition**: left band, logo, navigation size, the amount of shell map visible, the danger-coloured Exit Game.
4. **Typography** (Bahnschrift): readable, right weight, right size at your resolution? Capitals everywhere acceptable?
5. **Opacity** of the band and panels; can the shell map still be enjoyed behind them?
6. **Component styling**: buttons, tabs, dropdowns, sliders, check boxes, list rows, dialogs.
7. **Options** (all categories, Apply / Cancel / Defaults, unsaved-changes dialog) and **Match Setup** (map, 8 players, rules): usable with mouse and keyboard?
8. **Ultrawide** (32:9 / 21:9): is the composition useful?
9. **Animation**: the 240 ms slide-in; anything missing or too much?
10. Any screen that **still falls back to the retail look** unexpectedly (a list is in section 4).
11. In-match: nothing should have changed (HUD, camera, input) - report anything that did.

@@PACKAGES@@
