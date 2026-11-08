# Zero Hour Modern HUD and Faction UI Parity (post Developer Preview 1)

Status: **implemented and automatically / natively validated; awaiting OWNER visual and gameplay acceptance.** Not accepted yet.
Developer Preview 1 is frozen at `c55234b5b` (tag `c55234b5b-dp1-rc`, local) and is not touched by this milestone; everything here is post-DP1 development.

Scope: bring the accepted Modern ("Enhanced") gameplay HUD to Zero Hour. It is **not** a HUD redesign: the accepted Generals HUD
([ENHANCED_HUD_FRAMEWORK_V1.md](ENHANCED_HUD_FRAMEWORK_V1.md)) is the design baseline and the Generals regression boundary.

## 1. Archaeology (what was found, from git history, code and the real retail data)

| # | Question | Finding |
|---|---|---|
| 1 | How is the Modern HUD enabled? | Presentation switch `Options > Gameplay > Interface: Original / Enhanced` (key `InterfaceStyle`, default Original) -> `EnhancedHud::setRequested`. `ControlBarScheme::init` calls `EnhancedHud::update(modern, classic, side, surface, scale)`; `EnhancedHudLayout::shouldBeActive` = requested && modern && !classic && supportsSide(side) && surface >= 640x480. |
| 2 | Why was it active for Generals but not Zero Hour? | **A deliberate, documented title gate** in `Core/.../ControlBarScheme.cpp`: `#if RTS_ZEROHOUR const char *const enhancedSide = nullptr;` (commit `fb4381382`, X64.45). The comment: the HUD "was built and reviewed against the Generals control bar (12 command slots, the Generals command sets and side names). Zero Hour keeps the Original bar until it has its own review". Nothing was accidentally broken; nothing was missing in the option, which was simply inert (the owner's Zero Hour `options.ini` already said `InterfaceStyle = Enhanced`). |
| 3 | Shared code | All control bar code (`ControlBar*.cpp`) and all Enhanced HUD code (`EnhancedHud*.h`, `ControlBarEnhanced.cpp`) live in `Core/` and are compiled into both titles. The only title conditional was the gate above. |
| 4 | Title-specific resources | The retail `ControlBar.wnd`, `ControlBarScheme.ini`, `CommandSet.ini`, `CommandButton.ini`, `PlayerTemplate.ini`, `Science.ini`, the shortcut-bar windows and the art are data of each title's archives. |
| 5 | Faction-specific elements | One scheme per side (`ControlBarScheme <Side>8x6`): faction logo (`SALogo` USA, `SNLogo` China, `SULogo` GLA), border colours, General button art. The HUD theme is chosen by side name prefix (`EnhancedHudTheme::factionOfSide`). |
| 6 | Zero Hour additions versus Generals | (a) **`ControlBar.wnd` has 14 command buttons (`ButtonCommand01..14`, a 7 x 2 grid of 50 x 44 on a 55 pitch) against 12 in Generals (6 x 2 of 53 x 45 on a 60 pitch)**; 409 of its 413 command sets use slot 13 or 14 (Generals: none of 92). The command window is wider (417 vs 408), and the side buttons / observer text windows are shifted. (b) **Ten additional general-specific control bar schemes** (3 USA, 3 China, 3 GLA, 1 Boss) beyond the 4 of Generals. (c) 96 sciences (53), 817 command buttons (254). (d) **Native shortcut bars of 9-11 special-power buttons per side, rising from the bottom** (`GenPowersShortcutBar{US,China,GLA}.wnd`, 48 x 41 slots, 35 pitch); Generals' bars are the project's regenerated 5-8 slot ones. (e) The `Boss` side. Unchanged: `Rank.ini` (5 ranks, 1/1/1/1/3 points) and every other window rectangle of the HUD islands (radar, info, queue, money, power, XP, General button). |
| 7 | Partial Zero Hour support already in the Modern HUD? | The theme mapping already covers the Zero Hour generals by prefix (`America*`, `China*`, `GLA*`; the framework docs say "including Zero Hour sub-generals ... not part of this review"). The Global Powers dock already measures the real windows and takes any slot count (and its compact row any visible count). |
| 8 | Missing versus merely disabled | Disabled: the gate. **Missing: the 14-slot command grid** (buttons 13 and 14 had no placement: they would have stayed at their 800 px retail position, outside the island) and the `Boss` side mapping. Everything else worked once the gate was lifted (verified natively, see section 5). |
| 9 | Zero Hour paths that fall back to the Original bar | Observer, Civilian (never HUD-enabled in Generals either), `*Small` variants (none exist in the Zero Hour data), and any unmapped side name. After this milestone every playable side is Enhanced when requested. |
| 10 | Was the deferral intentional? | Yes: X64.45 ("Zero Hour keeps the Original bar until it has its own review"), restated in `X64_36_ENHANCED_HUD_*` ("Zero Hour candidates are not part of this review") and in the Developer Preview docs as a known limitation. |

## 2. Coverage matrix (from `PlayerTemplate.ini`, `ControlBarScheme.ini`, `ControlBar.wnd` of the Zero Hour archives)

All playable sides use the same 14-slot command bar and the same island regions; the theme is the base side's. `Shortcut slots` = `SpecialPowerShortcutButtonCount`.

| Side (PlayerTemplate) | Base | Scheme (logo) | HUD theme | Shortcut bar (slots) | Special control set | Modern HUD after this milestone | Native evidence |
|---|---|---|---|---|---|---|---|
| America | USA | America8x6 (SALogo) | USA | US (10) | base USA sets | complete | captured |
| AmericaSuperWeaponGeneral | USA | AmericaSuperWeaponGeneral8x6 (SALogo) | USA | US (11) | `SupW_*` sets, own General Powers | complete | captured |
| AmericaLaserGeneral | USA | AmericaLaserGeneral8x6 (SALogo) | USA | US (10) | `Lazr_*` sets | complete | captured |
| AmericaAirForceGeneral | USA | AmericaAirForceGeneral8x6 (SALogo) | USA | US (11) | `AirF_*` sets | complete | captured |
| China | China | China8x6 (SNLogo) | China | China (10) | base China sets | complete | captured |
| ChinaTankGeneral | China | ChinaTankGeneral8x6 (SNLogo) | China | China (10) | `Tank_*` sets | complete | captured |
| ChinaInfantryGeneral | China | ChinaInfantryGeneral8x6 (SNLogo) | China | China (10) | `Infa_*` sets | complete | captured |
| ChinaNukeGeneral | China | ChinaNukeGeneral8x6 (SNLogo) | China | China (10) | `Nuke_*` sets | complete | captured |
| GLA | GLA | GLA8x6 (SULogo) | GLA | GLA (10) | base GLA sets | complete | captured |
| GLAToxinGeneral | GLA | GLAToxinGeneral8x6 (SULogo) | GLA | GLA (10) | `Chem_*` sets | complete | captured |
| GLADemolitionGeneral | GLA | GLADemolitionGeneral8x6 (SULogo) | GLA | GLA (10) | `Demo_*` sets | complete | captured |
| GLAStealthGeneral | GLA | GLAStealthGeneral8x6 (SULogo) | GLA | GLA (10) | `Slth_*` sets | complete | captured |
| Boss (FactionBossGeneral) | China | ChinaBossGeneral8x6 (SNLogo), side name `Boss` | China (explicit mapping) | China (9) | `Boss_*` sets | complete by data / unit test; **not selectable as a human in Skirmish**, so no native capture | unit test |
| Observer | - | Observer8x6 | none | - | observer windows | Original (as in Generals) | - |
| Civilian | - | - | none | - | - | Original | - |

"Complete" means: the HUD is active (`hud_active=1`), the 14 command slots are placed inside the command frame without overlap, the theme is the base faction's, and the retail windows (commands, queue, upgrades, tooltips, rank, General button, shortcut bar, popups) keep ownership of all gameplay data; the HUD only moves, sizes and draws behind them.

## 3. Architecture: shared versus title-specific

* **Shared (unchanged):** the island, regions, responsive rules, theme system, window ownership, hit testing, image policy, Global Powers dock, Battle Overview, General Powers console (data-driven: it reads the real science prerequisites, so the per-general trees of Zero Hour need no code).
* **Title-specific, as data:** the **command grid profile** `EnhancedHudLayout::CommandGrid { Standard, Wide }` (Framework v1.1, additive). `Standard` = the accepted 12-slot Generals grid, byte for byte; `Wide` = Zero Hour's 7 x 2 grid in the same command frame at 1.12 x (50 x 44 retail buttons -> 56 x 49 reference units; the Generals grid is 63 x 54). The profile is selected once in `ControlBarScheme::init` (the control bar's own property; the layout headers never name a title or a faction).
* **Side mapping:** `factionOfSide` additionally maps `Boss` -> China (its scheme is the China scheme).
* **Not blue:** the in-game faction HUD keeps USA / China / GLA identity. The blue is the Zero Hour *frontend* accent only ([MODERN_UI_3_FRONTEND.md](MODERN_UI_3_FRONTEND.md)).

## 4. Implementation summary

* `EnhancedHudLayout.h`: `CommandGrid` profile, `commandSlots()`, the Wide table (`commandGroup`), the Wide `commandMap` (origin 223,494; 1.12 x) and the container bounding box.
* `EnhancedHud.h`: `update(..., CommandGrid)` selects the profile before the layout is computed.
* `EnhancedHudTheme.h`: `Boss` mapping.
* `EnhancedHudImageFit.h`: `ButtonCommand13/14` aspect-fit entries.
* `ControlBarEnhanced.cpp`: the command bay draws `commandSlots()` plates (window list extended with buttons 13-14).
* `ControlBarScheme.cpp`: the Zero Hour gate is removed; `RTS_ZEROHOUR` now selects the Wide grid, every side is offered to the HUD.
* Dev-only harness (no gameplay effect): `GENERALS_AUTOFLOW_SIDE` (army combo entry of the human Skirmish slot, through the real combo box) and a `hud` log checkpoint (side, active, faction, command slots, island) at the match capture.
* Tests: see section 6.

## 5. Native evidence (this machine, real retail data, Auto UI scale; images in `evidence/zh-hud/`)

* **Coverage:** before the silhouette revision all 12 selectable Zero Hour sides were captured at 1920 x 1080 (every side logged `hud_active=1 command_slots=14`, with the side's own command buttons, the base faction's theme and logo art); after the revision the USA / China / GLA base families (all six resolutions) and the AirForce, Nuke and Stealth generals (1920 x 1080 and 5120 x 1440) were re-captured (`silhouette-zh-*.jpg`, `silhouette-zh-generals-*.jpg`).
* **Responsive matrix** (USA, China, GLA base families, stepped silhouette): 1024 x 768, 1280 x 720, 1920 x 1080, 2560 x 1440, 3440 x 1440, 5120 x 1440: the island is whole and bottom-flush at every size (the whole width at 4:3, centred on ultrawide), the stepped chassis shows at every size and in every theme, the 14 slots sit inside the command frame without overlap or clipping, and the cash / power header, queue, unit information and rank module are intact. Owner review captures: `review-zh-usa-*.jpg`, `review-generals-usa-*.jpg` (full frames, 1920 x 1080 and 5120 x 1440).
* **Generals regression boundary:** the new build and the frozen DP1 executable were captured in the same Skirmish at 1920 x 1080 and 5120 x 1440 and compared inside the HUD island: **GLA is pixel-identical (0.00 % of pixels differ, island, command frame, info block and XP module) at both resolutions.** The DP1 executable cannot select a side without editing the owner's `Skirmish.ini` (it has no override), so its saved side (GLA) was the only baseline; the USA and China Generals HUDs are covered by the exact-geometry tests (`WideGridKeepsTheFrozenRegionsAndTheGeneralsGridExactlyAsAccepted`, `AcceptedMajorGeometryIsFrozen`, the 12-slot v1 table pinned by `test_zero_hour_hud_contract.py`) and by inspection of the new Generals captures (6 x 2 grid unchanged).
* **Classic** (`silhouette-classic-both-titles.jpg`, re-captured after the revision): `-classic` Skirmish, both titles: the original retail 800 x 600 control bar, no Modern footer, `hud_active=0`.
* **Focused performance:** the Enhanced HUD frame draw (the only per-frame HUD code) costs **about 54 microseconds per frame in Zero Hour (14 slots) and 42 microseconds in Generals (12 slots)**, averaged over 1199 frames, three runs each, USA at 1920 x 1080: about 0.65 % of a 120 Hz frame budget. The change adds no per-frame allocation, no lookup (the window list is resolved once per control bar) and no layout recomputation (the layout is computed when the scheme initialises); it adds two plates (a handful of rectangles each).
* **Determinism / golden AI:** not rerun. No authoritative source was touched: the changes are the Enhanced HUD layout / theme headers, the HUD renderer (`ControlBarEnhanced.cpp`, which a contract test forbids from touching the message stream, players, money, ranks, randomness or `Xfer`), the scheme init hook, tests, and a developer-only autoflow harness. No `GameLogic`, `GameNetwork`, AI, object, weapon, save or replay code changed.

## 5.1 Stepped silhouette revision (owner visual review, before acceptance)

The owner found the whole Modern HUD "one long flat control strip" and asked for an intentional stepped silhouette, mainly a taller and genuinely larger minimap. Shared by both titles (Generals and Zero Hour), Classic untouched.

* **Geometry** (island-local reference units; the 180 unit *band*, i.e. the command region, keeps its accepted size and position relative to the bottom): the island is now an envelope of band + rise. The radar module rises **36** above the band (frame 164 -> 200, +22 %), the selection / information module **16**, the command region **0**, the General module **14**. The radar module is also **20 wider** (a landscape map is width limited, so a taller window alone would not enlarge the map): the radar window is **187 x 188 against 167 x 152** (a square map's usable side 152 -> 187, about +51 % area), and every module to its right moved 20 units right (`kRadarExtraW`). Module widths, gaps and the 12 / 14-slot command grids are unchanged (only their x origin moved with the shift).
* **Rendering:** the single chassis rectangle became four steps (`EnhancedHudLayout::shellSegments`, one definition shared by the renderer and the tests), each topped 8 units above its module. A corner is machined (chamfered) only where its neighbour is lower or absent, so no notch opens against a taller wall; ribs, girders, patches, caps, vents and lamps follow the new tops. Faction themes, typography and component geometry are untouched.
* **Behaviour kept:** the master window's input callback ignores clicks (`ControlBarInput` returns `MSG_IGNORED`) and only the child windows take them, so the empty notches beside the taller steps are not dead zones; the radar window is the only window that grew. Tooltips are placed above the command region (`bandTop`), the General Powers popup above the information step (`infoTop`), the compact Global Powers row above the General module (`rightTop`); the island still slides with the real control bar window on hide / show.
* **Viewport:** the envelope stays capped at a third of the surface height, the tower is the radar's 23 % share of the width, and the band height (what covers the world in front of the command region) is unchanged.
* **Minimap measurements** (analytic, asserted by `EnhancedHudSilhouette.TheMinimapRegionAndTheActualMinimapAreGenuinelyLargerThanTheAcceptedOnes`): at 1920 x 1080 the radar window is 289 x 290 px against 263 x 239 (a square map's side 239 -> 289 px, about +45 % area; a 2:1 map about +21 %); at 5120 x 1440 a square map's side 342 -> 415 px (about +48 % area). The module's scale is 2 % smaller at 1080p because the island is 20 units wider (the width cap binds), which is why the commands are 2 % smaller there; the command region's reference size is unchanged.
* **Validation boundary:** hide / show is covered by the existing slide tests (`translated`) and was not captured natively; the General Powers console and popup placement are covered by their layout tests, not by a native capture: both are on the owner checklist.
* **Header pin:** the `EnhancedHudLayout.h` content pin was updated again for exactly this, and `AcceptedMajorGeometryIsFrozen` now pins the new geometry and the unchanged command region (`EnhancedHudSilhouette.*` tests add: step order, steps tile the island, minimap enlargement, command grids inside the command frame, viewport share).

## 6. Tests

* gtests `EnhancedHudZeroHour.*`: Standard stays the default with no 13th slot; the Wide grid places 14 slots inside the command frame, without overlap, clear of the cash display and inside every command container on 800x600 .. 5120x1440; Generals geometry restored exactly after Wide; every Zero Hour side is supported and mapped; the decision selects the grid and Original / Classic / non-requested stay Original; the compact dock holds 1..11 slots on 4:3 surfaces; the column dock frame never touches the island.
* Python contracts: the Zero Hour gate is gone and replaced (`test_zero_hour_native_contract.py`), the 12/14-slot bay (`test_enhanced_hud_contract.py`), the v1.1 header pin and the Zero Hour HUD contract (`test_zero_hour_hud_contract.py`).

## 7. Known limitations / follow-ups

* The Zero Hour tooltip art is the retail Zero Hour tooltip (navy) while the Generals one is black: retail data, deliberately untouched.
* The `Boss` side cannot be selected as a human in Skirmish (verified in the army list), so its HUD has no native capture.
* General Powers console and Battle Overview are shared and data-driven; their per-general content is covered by the owner checklist, not by an automated capture.
* `ui_residency_release` (main-menu flow shutdown report) is unchanged and unrelated.

## 8. Owner acceptance

Required. The owner-test build and the manual checklist are in the milestone report; nothing here is marked accepted until the owner accepts it.
