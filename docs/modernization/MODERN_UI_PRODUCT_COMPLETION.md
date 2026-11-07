# Modern Experience UI - product-completion audit (UI.4 audit)

Status: audit and one defect fix, 2026-11-06, start point `1e9ea5494`. Read with [MODERN_UI_ARCHITECTURE.md](MODERN_UI_ARCHITECTURE.md), [MODERN_UI_3_FRONTEND.md](MODERN_UI_3_FRONTEND.md) and the product-level [PRODUCT_ROADMAP.md](../PRODUCT_ROADMAP.md).
Decision of this milestone: **the Modern UI is not restarted.** UI.1 - UI.3 already contain the layout core, the design system and the main screens; this audit measures them against the product-completion brief, fixes what the measurements found and ranks what is left.
Owner native visual acceptance of UI.2 / UI.3 is still recorded as pending; nothing here replaces it.

## 1. What exists (by screen)

| Screen | Generals | Zero Hour | Responsive layout | Notes |
|---|---|---|---|---|
| Main menu / shell (Solo, Campaign, Challenge or Training, factions, difficulty, Skirmish, Multiplayer, Load, Options, Credits, Exit) | Modern | Modern | pure layout, centred composition box <= 16:9, band docked left, scale reduced to fit | shell map stays the live backdrop; footer shows "Modern Experience by MKRaudstein / 64-bit / DirectX 11"; no developer or debug control is in the menu |
| Options (7 categories) | Modern | Modern | rail + generated rows, scroll, effective scale | Network category embeds the retail block (fixed pixel size; see 3) |
| Skirmish Match Setup | Modern | Modern | three zones (Map / Players / Match Rules) from specs | rule cards generated from `kRules` |
| Load Game, Replays, Save/Load popup | Modern | Modern | list + action column | inline confirmations re-skinned |
| Message boxes / quit box | Modern | Modern | dialog fits its text at its own scale | |
| Credits | Modern | Modern | scrim band over shell map | |
| LAN lobby, LAN game setup, direct connect | **retail** | **retail** | retail 800x600 WND stretched | no Modern match options in LAN |
| Challenge general select (Zero Hour), Map select (campaign and Skirmish/LAN map lists) | retail | retail | retail | |
| Online (GameSpy) | retail | retail | retail | upstream service unavailable |
| Keyboard options (Zero Hour only), Advanced Display popup | retail | retail | retail | |
| Gameplay HUD | Enhanced HUD (USA / China / GLA) or Original | same | frozen layout, scale = min(fit, UI scale) | see 7 |

Established design decisions (kept): procedural chamfered components, no retail button art, restrained dark-steel panels with the shell map visible, one accent colour per title (Generals sand-gold, Zero Hour orange), Bahnschrift with Segoe UI / Arial fallback (no font shipped), capitals for navigation and headings, 240 ms band slide as the only animation, retail gadgets underneath keep every callback. Rejected earlier: re-skinned retail button art (UI.2), a fixed 4:3 canvas centred on a widescreen surface, per-resolution offsets.

## 2. Responsive validation (this milestone)

Method: `Invoke-FrontendMatrix.ps1` on staged native candidates (real windows, hit tests, overlap and text-fit checks, 0 = no defect). Both titles unless stated.

| Aspect | Surface | Scales | Result |
|---|---|---|---|
| 4:3 | 1024x768 | 75 / 100 / 150 % (Zero Hour also 1600x1200) | main menu, Options, Match Setup, Replay: 0 defects after the fix in 3; effective scale falls to 72 - 128 % when 150 % does not fit |
| 16:10 | 1920x1200 | 100 / 150 % | 0 defects |
| 16:9 | 1920x1080 | 100 % (UI.3 matrix: 75 - 300 %) | 0 defects |
| 21:9 | 3440x1440 | 100 / 150 % (UI.3: 75 - 300 %) | 0 defects; composition stays 16:9-wide, shell map fills the rest |
| 32:9 | 5120x1440 | 100 / 150 % (UI.3: 75 - 300 %) | 0 defects; Match Setup is compact in the middle of the surface at 100 % (readable, small), larger at higher scale / Auto |
| 4K | 3840x2160 | UI.3 matrix | 0 defects |

Run totals this milestone: Zero Hour 24 (4:3) + 32 + 9 runs, Generals 32 + 8 runs; one defect (3), fixed and re-run clean. Representative screenshots are kept in `evidence/ui-4/` (JPEG, a handful); the full capture sets were deleted by the runner.

## 3. Defect found and fixed

Options > Network at 1024x768 / 150 %: one retail Network-block window lay outside the content viewport (the block is retail pixels and does not scale with the UI scale). Fix: `ModernScreen::Input` gained `minContentW/H`; the Network category passes the block's measured extent and the layout reduces the scale (the existing "reduce until it fits, never clip" rule) until the viewport holds it. Other categories are unaffected; unit tests cover 4:3 surfaces at every scale and the fixed-size-block rule.

## 4. Skirmish and readiness for Game Speed

Current Match Rules: Starting Cash, AI Behaviour (Original / Enhanced), Superweapons; career record. The zone is generated from `kRules`, so a fourth rule adds a card without moving the others (tested). Game Speed needs, in a later milestone: a rule spec plus a programmatically created combo (the `MatchOptionControls::createCombo` pattern, since `SkirmishGameOptionsMenu.wnd` has no window for it), `Skirmish.ini` / replay header / save serialisation, and the FramePacer seam in [GAME_SPEED_DESIGN.md](GAME_SPEED_DESIGN.md). **No placeholder was added**: a control that does nothing would be dishonest and the layout needs no redesign to take it.
Not yet generated from the specs: per-rule help text (generic hint only), a validation line explaining a disabled Play button.

## 5. Multiplayer UI - state and architecture notes

The LAN screens are untouched retail and are the biggest remaining UI gap. Findings that shape their Modern version:
* `ModernMatchSetupScreen` is bound to the Skirmish WND names (`collectWindows`, the backdrop name, the 8 x 4 cell table). LAN game options use a different WND with host / client states, accept (ready) flags, a chat list, map-status per slot and no AI rows for clients. The pure layout (`ModernMatchSetupLayout.h`) is reusable; the binding layer needs a per-screen window table and a status strip (host / client, ready, connection, build match).
* Needed room (from the LAN test plan): host / client state, slots, factions, teams, positions, map, match options incl. Game Speed, build / version compatibility (no such check exists in the engine today), ready state, connection status.
* Not done on purpose: no new networking, no lobby protocol field. Match options in LAN need a protocol decision (Game Speed design section 2) and real two-PC validation first.
* Recommendation: build the Modern LAN setup only after the two-PC validation of the current candidate, so UI changes are not stacked on an unvalidated network layer. **Real two-PC multiplayer validation of `2071ad765` remains pending.**

## 6. Options

Seven categories, rows generated from page tables, dirty state, Apply / Defaults / Cancel, only settings that change behaviour (see [SETTINGS_COMPLETION_AUDIT.md](SETTINGS_COMPLETION_AUDIT.md): gamma and anti-aliasing are not offered because they do nothing). Display covers resolution, mode, monitor, Render Scale, UI Scale, VSync, frame limit; refresh rate is not offered (nothing to control). Remaining: per-setting help text, Network block as generated rows, keyboard rebinding (Zero Hour retail popup only), localisation check of long strings.

## 7. HUD boundary

Already Modern-aware: Enhanced HUD (island 1000 x 180 reference units, fitted to surface, UI-scale rule, ultrawide / compact rules, Global Powers dock, Battle Overview, General Powers console) for USA, China and GLA; Original / Enhanced switch live in a match; frozen framework v1. Still legacy: Original control bar (selectable), in-match popups (quit, diplomacy, chat), the "Command Menu" button of the USA Enhanced HUD (known issue 6). Needed before a Developer Preview: nothing structural; the known-issue-6 control and the 1 - 2 s command-UI delay (known issue 5) are polish. Safe to defer: the cross-faction visual polish pass ([ENHANCED_HUD_POLISH_BACKLOG.md](ENHANCED_HUD_POLISH_BACKLOG.md)) and any HUD reskin.

## 8. Classic isolation

Every Modern entry point is gated by `DisplayRuntime::modern()` and `!ModernExperience::classic()`; contract tests pin them. Native Classic runs of this milestone: see the validation section of the final report / changelog entry (Classic main menu, Options, Match Setup, Replay, Load, Credits show no Modern windows).

## 9. Remaining UI work, ranked

1. **Owner native visual review of UI.2 / UI.3** (blocks any visual revision; cheapest and highest value).
2. **Developer Preview blockers in UI: none structural.** The retail LAN lobby / setup works as in retail; it is the last visibly 2003 screen a tester will meet.
3. Modern LAN lobby + LAN game setup (after two-PC validation) - Beta-or-Alpha target, includes the status strip and Game Speed / match options.
4. Modern Match Options + Game Speed (next Product Completion milestone; Skirmish first).
5. Challenge general select and Map select compositions (Zero Hour portraits, map list with preview).
6. Options: per-setting help text, Network block as generated rows, keyboard options.
7. Start-button validation line, long-string / localisation pass.
8. HUD polish (Beta); Online screens only if the service ever returns; movies (FFmpeg, Beta).

## 10. Match Options, ultrawide and readability polish (2026-11-07)

Owner visual review approved the direction (shell map, dark restrained panels, Bahnschrift, chamfers, accents, three-zone Match Setup, responsive Modern layout). Changes made, nothing redesigned:

| Item | Before | After |
|---|---|---|
| Match Rules | 3 rules | 4 rules (Starting Cash, AI Behaviour, Superweapons, Game Speed) from the generated spec list; career block set in the muted helper style |
| Match Setup status line | one static hint | start-validation reason (unknown map / too many players: the two checks `startPressed()` already makes; Play stays enabled), else one-sentence help for the focused rule, else the hint |
| Auto UI scale, Match Setup, 21:9 / 32:9 (1440 tall) | 186 % (reduced to fit a 16:9 box) | 198 % (composition up to 21:9; the panel now fills 86 % of the height and about half of a 32:9 width, shell map still visible) |
| Auto UI scale, main menu, 1440 tall (any aspect) | 190 % | 198 % (4 % reduction steps find the largest scale that fits) |
| Smallest text | 10 pt Helper, 7 pt floor | 11 pt Helper, 11 pt floor for all Modern text; footer alpha 150 / 180 -> 210 |

Finding worth knowing: the Auto scale is height-based, so the same 1440-pixel-tall surface gets the same physical UI at 16:9 and 32:9 (and the owner's options file says `UIScale = Auto`); the earlier "tiny" 32:9 screenshots were the explicit 100 % audit shots. A larger Auto cannot be had without exceeding the surface height; a physical-size (DPI-aware) preference would be the next lever and needs the owner's monitor data.

Validation: 36 native audit runs (both titles, 1024x768 / 1920x1080 / 5120x1440 x 75 / 150 %: MainMenu, Options, MatchSetup) with 0 defects; Auto-scale shots at 1024x768 .. 5120x1440; Classic smoke (main menu, Options, Match Setup, Replay, both titles) shows the retail screens (the Match Setup audit's only "defects" are the Modern dropdowns Classic correctly lacks). Evidence: `evidence/ui-5/` (before / after at 21:9 and 32:9, 4:3, 16:9, Generals, Classic).

Known pre-existing Classic detail: the retail Skirmish screen still shows its "Game Speed" caption with no slider (the slider has been hidden since X64.52A because it was the FPS cap); left untouched (Classic preservation).
