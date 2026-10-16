# X64.36 Enhanced Gameplay UI - USA prototype (CP10)

Status: **ACCEPTED AND CLOSED (CP10, 2026-10-17)** - see `X64_36_CP10_CLOSEOUT.md`. The text below is the historical engineering record of the USA prototype and its refinements.
No native execution was performed by the agent. Baseline: `fa4675c36` (X64.35 / CP9 ACCEPTED AND CLOSED).

## 1. Purpose
Let the tester play a real USA Skirmish and compare **Original** against **Enhanced** in the same modernized build and answer
"Would I actually rather play with Enhanced, and does it still feel like Generals?" This is a layout/presentation
prototype, not a gameplay change and not final art.

## 2. Repository reconciliation (done first)
Clean tree at `fa4675c36`, equal to `origin/dev/modern-engine`; no commits by another agent since the X64.35 closeout;
`FIRST_PLAYABLE_ROADMAP.md` lists CP9 = X64.35 ACCEPTED AND CLOSED (2026-10-16). The repository was treated as the source of
truth.

## 3. Architecture
```
Options > Gameplay > Interface  --(DisplayConfig::Settings.enhancedHud, key InterfaceStyle)-->  EnhancedHud::requested
ControlBarScheme::init(side)    --(EnhancedHud::update: requested + modern + !classic + USA + surface fits)--> active
active      -> ControlBar::applyEnhancedLayout()  moves the EXISTING windows into the island (EnhancedHudLayout.h)
            -> drawEnhancedHudFrame()             modular procedural frame instead of the 800x600 scheme art
inactive    -> ControlBar::restoreOriginalLayout() strict no-op unless Enhanced had been applied; then exact restore
```
* **Pure geometry** `Common/EnhancedHudLayout.h` (no engine types, unit-tested) and the live switch `Common/EnhancedHud.h`.
* **No second command system.** `ControlBarEnhanced.cpp` only calls `winSetPosition/winSetSize/winHide` and
  `drawFillRect/drawLine/drawOpenRect`. Selection, command buttons, build/production queues, special powers, General XP/rank,
  radar, cash, hotkeys and tooltips are the original windows/callbacks with their original logic; a contract test forbids
  any message/logic/XP/RNG reference in that file.
* **Original is preserved.** All Enhanced code paths are gated on `EnhancedHud::active()`; with Original (the default) the
  scheme draws its art exactly as before and `restoreOriginalLayout()` returns immediately. `-classic` and diagnostic
  profiles are never Enhanced.
* **Unsupported factions fall back safely.** `supportsSide()` accepts only sides starting `America` and not the `...Small`
  squished variant; China, GLA, Observer, Europe, Russia etc. stay Original (no fake or partial Enhanced HUD).
* **Switch.** Options > Gameplay > **Interface: Original | Enhanced (USA prototype)**, persisted as `InterfaceStyle`
  (Original default; unknown values read as Original; `Defaults` selects Original). It applies from the next control bar
  build: changing it outside a match rebuilds the control bar the same way a UI Scale change does (no risky live switching);
  the combo is disabled inside a running match like the other layout options.

## 4. Enhanced geometry (exact)
Designed on a **920 x 180 reference island** and scaled uniformly by the modern UI scale, capped so the island never exceeds
82 % of the surface width (compact <= 1.45:1 surfaces may use the full width) nor a third of its height. The island is
**horizontally centred and flush with the bottom edge (island.bottom == surface height)**; nothing under it; the world is
visible and clickable on both sides. Island-local reference units (left to right):

| region | x..x+w | contents (existing windows) |
|---|---|---|
| left cap / radar frame | 8..224 | `LeftHUD` radar 167x152 at (16,14); `WinUAttack`; column of the four utility buttons (Options, Idle Worker, Beacon, Communicator) |
| divider | 228 | |
| unit info frame | 232..392 | the 141x109 selection block (`WinUnitSelected`, cameo, 5 upgrade slots, 9 production queue slots) at (244,36) |
| divider | 394 | |
| command frame | 396..836 | header: `MoneyDisplay` (408,10) and the power bar `PowerWindow` (500,16); the 6 x 2 command/build grid (12 buttons) at 1.10x from (424,44) |
| divider | 839 | |
| XP / right cap | 842..912 | `ButtonGeneral` (853,14) and the **vertical General XP bar** (`ExpBarForeground`/`GeneralsExp`, 21x107) |
| special-power column | beside the island | the existing `GenPowersShortcutBar` layout (5 buttons), bottom flush to the right of the island; if there is no room (compact surfaces) at the right edge above the island |
The three 800 px "size" toggles (Small/Medium/Large) have no Enhanced equivalent and are hidden.

Resulting physical sizes (Auto UI scale):

| surface | UI % | island scale | island W x H | side margin | power column |
|---|---|---|---|---|---|
| 800x600 | 100 | 0.87 | 800 x 157 (full width, compact) | 0 | right edge, above |
| 1024x768 | 125 | 1.11 | 1024 x 200 | 0 | right edge, above |
| 1280x1024 | 150 | 1.39 | 1280 x 250 | 0 | right edge, above |
| 1366x768 | 125 | 1.22 | 1120 x 219 | 123 | beside |
| 1920x1080 | 175 | 1.71 | 1574 x 308 | 173 | beside |
| **1920x1200** | 200 | 1.71 | **1574 x 308** | 173 | beside |
| 2560x1440 | 225 | 2.25 | 2070 x 405 | 245 | beside |
| 3440x1440 | 225 | 2.25 | 2070 x 405 | 685 | beside |
| 5120x1440 | 225 | 2.25 | 2070 x 405 | 1525 | beside |
The island never stretches with the surface width: 21:9 and 32:9 get the same physical island as 16:9 at the same height.
(For comparison the Original band at 1920x1200 is 1600 x 368.)

## 5. Visual direction and assets
* **Procedural modular frame** (no giant bitmap): base plate, four panels (radar / info / command / XP) with inset bevel and
  restrained blue accent edges, dividers, accent top edge with end caps, power-column backing. Dark steel and blue, USA
  palette, deliberately close to the Generals console language. Colours and the frame code live in
  `drawEnhancedHudFrame()`; the layout table names the same regions a future `FactionHudTheme` would parameterize.
* **Reused**: every button, cameo, radar, XP bar and gauge image of the original windows (scheme-driven button images keep
  working), so recognisability and functionality are retained.
* **New artwork**: none (frame is drawn from rectangles/lines). Final MKRaudstein artwork is explicitly deferred.

## 6. Preserved functionality (by construction and test)
Radar and radar controls, cash, selected-unit information, command/build buttons incl. Attack/Guard/Stop (original grid
windows), production queue and upgrade slots, tooltips (the popup is positioned above the island instead of the retail
spot), hotkeys (untouched), the General XP/rank state (the original `GeneralsExp`/`ButtonGeneral` windows read the
authoritative values), special-power shortcut buttons (same layout, buttons, cooldown overlays and commands), world input
beside the island. Left Original on purpose: superweapon countdown/status text and notifications.

## 7. Automated validation
* `EnhancedHudLayout.*` (13 tests): bottom flush and centred on 11 surfaces (800x600 ... 5120x1440 incl. odd/square);
  bounded width and world visible/clickable beside it; region partition order; every placed window inside its own
  region and the island for all surfaces; windows of different regions never overlap, 12 command buttons never overlap;
  hit regions (island takes input, left/right/above is world); UI Scale 50-200 % within bounds; power column placement;
  USA-only and fallback truth table; runtime switch never invents a layout; persistence, default Original, unknown values,
  Classic ignore, other keys untouched; the choice lives on the Gameplay page; the window table covers the functional surface.
* Contract tests (`test_enhanced_hud_contract.py`): presentation-only, every entry point gated/no-op in Original, scheme
  hooks keep the original drawing, USA-only/never classic, persistence/Options wiring.
* Matrix: Win64 ctest 16/16, g 262 and z 261 gtests; Win32 reference ctest 23/23, g 224 and z 224; all Python contracts
  and PowerShell workflow tests pass; X64.33/X64.34/X64.35 contracts unchanged.

## 8. Known prototype limitations (not hidden)
* Visual layout is verified geometrically and by tests, **not** by seeing the running game; the first look will find
  rough edges (text fit inside scaled buttons, radar/queue spacing, popup placement) that need native review.
* Frame art is procedural placeholder art; no per-faction theme data yet; only USA.
* The Munkee/CenterBackground art windows are hidden rather than themed; the three size toggles are hidden.
* Notifications, superweapon timers and the player info list remain at their Original anchors and may overlap the
  island on some surfaces (left Original by request).
* Interface changes take effect when the control bar is rebuilt (Options Accept) / next match, not live.
* The Zero Hour generals variants whose side starts with `America` (e.g. `AmericaAirForceGeneral`) are allowed by the
  side test but ZH bring-up is out of scope and was not exercised.

## 9. Candidate and native test
`build/dev-runtimes/native/Generals/x64-36-enhanced-hud-usa` (staged and preflighted, not run by the agent), built from
`4067ccdd3`:
* `game/generalsv.exe` SHA256 `AAC4EF7A6D1DF407B9838E599A0D4347B89816985C6588836751C2D49E99A794`
* `game/generalsv.pdb` SHA256 `384DD8816FEBDEAC6336EF57A19A1A4AA9420373D114443AF40F35695111135F`
Steam untouched.
```
pwsh scripts\native-runtime\Invoke-NativeNormalLaunch.ps1 -Title Generals -Name x64-36-enhanced-hud-usa -RunNormalLaunch
```
1. Options > Gameplay > Interface = **Original**, Accept. Start Skirmish as USA; play for a few minutes (select infantry,
   vehicles, buildings; build structures; produce units; Attack/Guard/Stop; radar; special powers; General XP). Exit.
2. Options > Gameplay > Interface = **Enhanced**, Accept. Repeat the same session at 1920x1200 on the laptop, and at least
   one other UI Scale (Options > Display).
3. Try one non-USA faction with Enhanced selected: it must show the Original bar.
4. Answer: would I rather play with Enhanced? what felt wrong (list)? does it still feel like Generals?

## 10. Native review 1 - command/build hit-test defect (root cause and fix)
**Observation (human, `x64-36-enhanced-hud-usa`):** the Enhanced HUD rendered correctly, but on the top row of the
command/build grid (incl. the bulldozer production button) the visible upper part of every button did not register
clicks; the lower part did.

**Root cause (coordinate path).** `ControlBar.wnd` nests the command windows and all twelve buttons as
`ControlBarParent > CenterBackground > CommandWindow > ButtonCommandNN`. `CenterBackground` is an unplaced art container
with the retail rectangle (0,479)-(800,600), i.e. island-relative (0,63) 800 x 121 *unscaled* pixels. The Enhanced layout
placed the children with `placeAbsolute` (screen rectangle -> parent-relative offset), which is why they were *drawn*
exactly right whatever their ancestors were. But `GameWindow` hit testing (`GameWindowManager::getWindowUnderCursor` ->
`GameWindow::winPointInChild`) descends the tree: a child can only be hit inside every ancestor's region. The interactive
region of each button was therefore its rectangle intersected with the stale 800 x 121 `CenterBackground` rectangle, whose
top edge is at 63 px below the island top while the scaled top row starts at 44 x scale: the part of the top row above 63 px
(scale < 1.43; 19 px dead at scale 1.0) never reached the button, and at larger scales the grid ran past x = 800.
Visual and interactive geometry diverged because only the drawn rectangle had been mapped.

**Audit of the other moved windows.** Radar/`LeftHUD`, `ButtonGeneral`, the XP bar, the utility buttons, `MoneyDisplay`,
`PowerWindow` and the selection block (`RightHUD > WinUnitSelected/ProductionQueueWindow`, cameo, upgrades, queue buttons)
are children of a window the layout places, so they were not clipped. Two further defects of the same family were found
and fixed:
* descendants of the command containers that had no table entry (beacon text/buttons, cancel-construction button,
  observer player lists, OCL timer sell button) kept their retail size/offset and landed over the General button and
  outside the island;
* the three NO_INPUT draw markers (`BackgroundMarker`, `ForegroundMarker`, `OnTopDraw`) keep their retail island-relative
  offsets and sat over the bottom-left corner of the radar; `GameWindowManager` *discards* a click whose topmost window is
  NO_INPUT, so those pixels were dead.

**Fix (shared mechanism, no separate hitboxes).** `Common/EnhancedHudWindows.h` (templated over the GameWindow position
API, used by the engine and the tests):
1. `apply()` places the table windows, then maps unplaced descendants of the command containers with the same
   `commandMap` as the grid (their retail screen rectangles are read before anything moves), then
2. `fitContainers()` (post-order) makes every container contain its children on screen: a placed container is extended if
   needed, an unplaced one (CenterBackground) is replaced by the bounding box of its children, children keep their screen
   rectangles;
3. the draw markers are table entries (1 x 1 at the island corner; their draw callbacks stay wired);
4. the special-power column uses `applyPowerColumn()` (uniform scale + the same container fit).
Everything is recorded by the existing remember/restore store, so Original is restored exactly; with Original selected no
Enhanced code path runs (`restoreOriginalLayout()` is a strict no-op), so the Original control bar is byte-for-byte the
same as before.

**Tests that would have caught it** (`EnhancedHudInteractiveTest.cpp`, 9 tests): a mock window tree with GameWindow
position/size/hit-test semantics and the real ControlBar.wnd / GenPowersShortcutBarUS.wnd hierarchy and retail rectangles,
9 surfaces (800x600 ... 5120x1440) x 5 UI scales x every command context x both unit contexts: every point inside a
visible leaf's rectangle (centre, four corners, four edge midpoints) hits that window, points just outside do not; the
12 buttons hit exactly their layout rectangle; radar, General, XP, utility buttons and a special-power button; containers
contain their children; no NO_INPUT marker overlaps a control; and a regression test that reproduces the old
placement-without-fit and proves the harness detects it.

## 11. Visual review notes (recorded, NOT addressed in this batch)
From the first native look; deliberately left for the next iteration:
* large unused / empty centre area of the island;
* the command/build area lacks strong visual grouping;
* radar proportions need refinement;
* the XP/General region needs better integration;
* the special-power column looks visually detached from the island;
* the procedural frame is prototype quality (final artwork deferred).

## 12. Visual/layout refinement 1 (after native review 2: hit testing PASSED)
Native feedback: stars compressed on the right, the large retail red button beside the minimap visibly low resolution,
cash needs a deliberate presentation, empty centre, detached power column. Kept: centred bottom-flush island, bounded
width, switch, USA-only fallback, the corrected hit-test architecture (`fitContainers` etc.).

**Revised geometry** (island 1000 x 180 reference units; was 920 x 180):

| region | x..x+w | contents |
|---|---|---|
| radar frame | 8..224 | radar 167x152 at (16,14) in a recessed bezel; four utility plates (32x26) in a column at x 188 |
| info frame | 230..395 | selection block scaled 1.10 to (235,34) (141x109 -> 155x120) |
| command frame | 401..845 | **cash panel** (409,10,152x26), power bar (570,17,263x7), 6x2 grid at 1.20x from (409,44) |
| XP / General frame | 851..992 | General button 67x49 at (888,14), XP trough+bar (858,68,36x102 / 864,74,24x90), **rank ladder** |
Scale is `min(UI scale, 82 % width cap / 1000)`; e.g. 1920x1200 -> island 1574 x 284 at scale 1.57 (previously 1.71 / 308 high
because the island was narrower); 3440x1440 and 5120x1440 -> 2250 x 405 (same island, bounded).

**Cash.** A dedicated recessed panel with an accent edge hosts the existing `MoneyDisplay` window; in Enhanced the value is
formatted with thousands grouping (`$ 45,200`) at the single text update (InGameUI, both titles), gated on Enhanced; value,
update cadence and the income-per-minute option are untouched; Original keeps the retail `GUI:ControlBarMoneyDisplay` call.

**General / XP.** The retail 21x107 strip (stars baked into the bitmap) is replaced by a trough behind the real
`GeneralsExp` progress window (now 24 wide) and a **rank ladder**: one plate per rank above the first, 86x20 units, each
with a square star glyph (18x18 units, never squeezed), lit up to the player's authoritative `getRankLevel()`; the number of
plates comes from `TheRankInfoStore->getRankLevelCount()` (capped at 4). Rank/XP mechanics are only read.

**Radar-area buttons.** The four utility buttons (Options, Idle Worker, Beacon, Communicator) no longer show their retail
30x22 bitmaps in Enhanced; they are drawn as bevelled procedural plates with simple glyphs, with hover/pressed/disabled
states from the live mouse/window state. The windows, commands, tooltips and hit regions are the existing ones. The bitmaps
are cleared only inside the Enhanced apply path; the scheme re-applies them on every init, so Original is untouched. The
General button keeps its retail image (it scales acceptably, scaled 1.2x; revisit with final art).

**Special power column.** Sits flush against the island's right end (or on its top-right corner on compact surfaces), bottom
aligned, drawn with the island's plate/bevel and a joint that carries the base plate across, so it reads as part of the
console. Its buttons, cooldown overlays and commands are unchanged.

**Tests.** `EnhancedHudLayout` (+8): cash panel containment and separation, thousands grouping, rank ladder containment /
square stars / no overlap, radar controls, power column attachment, rebalanced fill ratios; `EnhancedHudInteractive` (9) runs
unchanged against the new geometry (the same placement mechanism, all contexts, 9 surfaces x 5 UI scales). Contract tests:
read-only rank access, gated cash formatting in both titles, Original image path untouched, guarded-source check.

**Known limits.** Visual result is verified geometrically only; star/glyph/plate art is procedural placeholder; the General
button and the XP fill still use retail bitmaps; the control-bar hide/show transition is not followed by the procedural frame.
The reference-token guard failures are baselined in `REFERENCE_TOKEN_GUARD_BASELINE.md`.

## 13. Refinement 2: retail image aspect-ratio / scaling pass (after native review of `x64-36-enhanced-hud-refine1`)
**Finding.** The retail draw code maps every bitmap onto its GameWindow rectangle (`TheDisplay->drawImage(image, start, end)`),
so moving/resizing a window for the Enhanced layout silently re-stretched the art. Concretely: the three-star General
button (`SUGeneral/H/P` are **65 x 29 px**, aspect 2.24) was drawn into a 56 x 41 -> 67 x 49 plate (aspect 1.37): the stars were
squeezed horizontally and stretched vertically. The "large red button" is the **attack warning lamp** (`WinUAttack`,
`SUUAttackI` idle / `SUUAttackH` lit, both **22 x 24 px**): the scheme sizes it at 22 x 24, Enhanced had placed it in a 45 x 45
window over the radar corner, i.e. a 22 px bitmap blown up about 3x and slightly stretched. The power gauge tiles and the
XP fill tiles are drawn at native tile size along one axis and window size along the other, so any window whose thickness
differs from the tile (Enhanced scale) distorted them.

**Mechanism (one reusable policy, no per-button numbers).** `Common/EnhancedHudImageFit.h` (pure, tested) classifies every
placed window (`policies()`; a unit test requires that every window of the layout tables is classified) and provides
`containFit`, `nativeFit`, `gaugeScale`. The window stays the full interactive plate; only the *bitmap* rectangle is narrowed:
`GameClient/EnhancedHudImage.h` (registry filled by `applyEnhancedLayout`, empty for Original) is called from the push-button
image draw (`fitImage`, aspect-fit + centre) and from the power / General-XP gauge draws (`gaugeScale`, uniform tile scale).
No window size/position/hit region is changed by the fit and no command/state code is touched.

**Audit (every reused retail image class in the USA Enhanced HUD):**
| control | source image(s) | was | now |
|---|---|---|---|
| General button (`ButtonGeneral`) | SUGeneral/H/P 65x29 | **distorted** (2.24 into 1.37) | aspect-fit; plate reshaped to 86x38 (2.26) so it fills it |
| General rank stars (retail strip `ExpBarForeground`) | SUExpBar 21x107 (stars baked in) | squeezed | procedural (rank ladder, square stars, real rank) |
| XP fill (`GeneralsExp`) | GenExpBar1 / Top1 / Bottom1 tiles | tiles stretched across the widened trough | gauge: tiles scaled uniformly from trough width, only fill length varies |
| power bar (`PowerWindow`) | PowerPoint Y/R/G tiles, PowerBarSlider | tiles stretched to the window thickness | gauge: tile + needle scaled uniformly from thickness |
| command/build buttons x12 | per-command icons (typ. 64x48) | stretched to the plate | aspect-fit inside the full plate, recessed plate shows around the icon |
| cameo, upgrades x5, queue x9 | unit/upgrade icons | uniform 1.1x of retail mapping | aspect-fit |
| special-power buttons x5 | command icons | uniform | aspect-fit; cooldown overlay still covers the whole plate |
| special-power frames (`ButtonParent1-5`) | frame art | uniform with the column | frame art, same proportions as Original (not re-stretched) |
| radar / minimap | TheRadar (map texture) | self-fitting | unchanged (draw code keeps the map aspect) |
| utility buttons x4 | 30x22 px bitmaps | blown up | procedural plates + glyphs (refinement 1) |
| attack warning lamp | SUUAttackI/H 22x24 | **blown up ~3x** | procedural lamp, see below |
| size toggles x3, markers | - | hidden / no image | unchanged |
Native-size (`NativeSize`) is implemented and tested (1:1 or whole multiple) but no control needs it in this pass; nothing is
classified intentionally stretchable except the gauges, which stretch **only along their length** by design. Button highlight /
pressed overlays (`Cameo_hilited/_push`), clock sweeps and borders are effects meant to cover the whole plate and still do.
Windows not registered (observer lists, beacon text buttons, cancel-construction...) keep the exact retail mapping scaled
uniformly, i.e. the same proportions as Original.

**Attack warning (reported separately).** Behaviour is untouched: `Radar` still calls `triggerRadarAttackGlow()`, which
toggles the retail `WinUAttack` window's enable state (disabled = lit); Enhanced only reads that state when drawing. The
retail 22x24 bitmaps are cleared in Enhanced and replaced by a **solid red lamp with a white warning triangle** at the radar
frame's lower right (32x28 units, clear of the radar and the utility buttons), and the **radar bezel turns red** while lit, so
"under attack" reads in peripheral vision. Idle it is a dark red lamp. Original keeps SUUAttackI/H exactly.

**Low-resolution retail assets that still scale acceptably (recorded, kept):** command and cameo icons (64x48-class
sources, ~1.2-1.6x), General button (65x29, ~1.3x). Replaced procedurally because they cannot scale cleanly: utility
buttons (30x22), attack lamp (22x24), XP strip (21x107).

**Tests.** `EnhancedHudImageFit.*` (8): aspect preserved / inside / centred for nine real source shapes in five areas,
squares stay square, native multiples, gauge scale, every placed window classified, General plate matches the 65x29
bitmap on all surfaces x UI scales, icons stay inside their full hit plates; plus the lamp placement test and the contract
test. The 9 interactive hit-test tests are unchanged and pass against the new General/lamp geometry.

## 14. Refinement 3: composition pass (after native review of `x64-36-enhanced-hud-refine2`)
Kept as accepted direction: the image-fit policy (section 13), the procedural attack-warning lamp and red radar bezel, the
dedicated cash panel with thousands grouping, the centred bottom-flush bounded island. Everything below is presentation
only; Original is untouched (`restoreOriginalLayout()` restores the island, the power windows **and the power window's
original draw function**; with Enhanced off no code path runs).

**Global Powers: an independent surface (supersedes "attached power column").**
* The five real shortcut windows (`GenPowersShortcutBarUS.wnd`: `ButtonParent1-5` > `ButtonCommand1-5`) are moved by the
  shared placement mechanism: `EnhancedHudWindows::applyPowerColumn` (vertical) or `applyPowerRow` (compact). No state,
  command, cooldown, tooltip or hotkey is touched; the clock/disabled overlays are the buttons' own.
* Geometry (`EnhancedHudLayout::compute`): safe region = at most 16:9 of the surface height, centred; the frame (panel +
  `kPowerPad`) keeps `kPowerMargin` from the safe edge and at least `kPowerGap` of clear space from the island, and never
  touches the bottom edge. Non-compact surfaces: a vertical column at the upper right of the safe region (y ~ 18 % of the
  height); if its x range overlaps the island's footprint (32:9 keeps it near the action, not at x = 5120) it is stacked above
  the island with the gap. Compact surfaces (<= 1.45:1, island spans the full width): a **row of five** above the island's
  right end (a tall column would cover ~40 % of a 4:3 battlefield). Deterministic, unit tested for all surfaces x UI scales.
* Presentation: the frame is the shortcut parent window's **own draw function** while Enhanced is active
  (`powersPanelDraw`): module plate with accent cap, one recessed well per visible slot, a bright outline on ready (enabled)
  slots. Because it is the window's draw function it follows the window's slide-in animation and hidden state exactly; no
  second visibility state exists.

**Command console.** Twelve fixed slot wells are always drawn while the command context is shown, inside one recessed bay;
slots whose button window is visible are lit plates, the others are empty wells, so 2 commands and 12 commands both read as
a bay with fixed positions. Command-to-slot assignment is the retail one (indices `ButtonCommand01..12` untouched); nothing
re-orders, swaps or remaps commands. Cash and the power bar share one header strip above the bay; the cash panel keeps its
own recess and accent edge. Other contexts (beacon, under construction, observer, OCL timer) keep their windows inside the
same bay without slot wells.

**Unit information.** The selection block (cameo, upgrades, queue, health/status windows - all retail) sits in a single
recessed well inside its module; nothing is added or removed.

**General module (one piece).** XP trough with scale ticks on the left (real `GeneralsExp` gauge, tiles scaled from the
trough width); the General Powers entry button (retail 65x29 three-star bitmap, aspect-fitted, in its recessed plate) on top
of the right column; below it a compact 2x2 rank grid (42-unit cells, 28-unit square stars) lit up to the real
`getRankLevel()`; cell count from `getRankLevelCount()`. The stars of the bitmap are now only the entry icon; the rank is
carried by the grid.

**Frame system.** One outer console plate (lighter upper band, single accent edge) holds four modules (radar, info, command,
General). Hierarchy: console > module (panel + hairline + cap) > well > plate/icon. No divider lines (gaps are the dividers),
one outline weight, wells instead of more borders. The frame is drawn at the real control bar window's position
(`windowRect(s_barMaster)` + `EnhancedHudLayout::translated`), so the low stage and the hide/show slide move it with the
windows; the control bar stays the only authority on its state.

**Radar module.** Unchanged behaviour: bezel well (red while the attack warning is lit), four procedural utility plates,
the accepted attack lamp.

**Limits.** Procedural placeholder art; frame/lamp/glyphs not final; the row fallback and the 2x2 rank grid are
geometrically tested but only reviewable natively; hide/show following is by construction (live master offset), exercised
only by unit tests of the translation, not natively.

## 15. Refinement 4: Enhanced HUD core + USA Theme v1 (after native review of `x64-36-enhanced-hud-refine3`: positive)
The refine3 composition is the candidate common template and is not re-laid-out here. This batch (a) separates the shared core
from faction presentation, (b) gives the USA a first real theme, (c) replaces the tall Global Powers frame by individual
plates and (d) finds and fixes the radar black bars. Original is untouched (all new code is behind `EnhancedHud::active()`;
`restoreOriginalLayout()` also restores the power slots' retail frame art).

**Enhanced HUD core (faction independent).** Modules: Radar, Selection/Unit Info, Economy/Cash, Command Bay, General/XP,
Global Powers.
| layer | file | owns |
|---|---|---|
| geometry / responsive rules | `Common/EnhancedHudLayout.h` | module rectangles, scale rules, bounded island, Global Powers placement, plate rule |
| placement + hit testing | `Common/EnhancedHudWindows.h` | moving the real windows, container fit, power column/row |
| bitmap policy | `Common/EnhancedHudImageFit.h`, `GameClient/EnhancedHudImage.h` | aspect-fit, gauges, classification |
| faction presentation | `Common/EnhancedHudTheme.h` | `Theme` data (colours, bevel/outline/cap switches), `Faction`, `implemented()` |
| renderer | `ControlBarEnhanced.cpp` | the one procedural renderer; reads the active theme, owns no behaviour |
There is no USAControlBar/ChinaControlBar: a faction adds a `Theme` constant, flips `implemented()` and, later, textures.

**Faction theme architecture.** `Theme` is plain data (static-asserted trivially copyable / standard layout, no includes of engine
or gameplay headers - contract tested): console plate/band, module, recessed well, outline, accent, bevel, content, rank,
ready/warning colours plus decorative switches (module caps, bevel, corner brackets). `EnhancedHud::update` records the faction
of the side; the renderer reads `EnhancedHud::theme()`; the renderer contains exactly one colour constructor (the Rgba
conversion), so no colour is hard-coded. `Faction` knows USA, China, GLA, Europe, Russia (Europe/Russia are reserved prefixes;
they are not retail sides). Only the USA is `implemented()`; every other faction keeps the Original control bar and
`themeFor()` returns the USA theme only as a safe default (no placeholder art). A faction theme cannot move a control, change
a command or read gameplay state.

**USA Theme v1.** Dark navy/gunmetal console (`plate` 12,16,24 / `band` 21,27,39), modules one step lighter, wells darker than
modules (hierarchy console > module > well > content, asserted by test), restrained blue accent used for state and one cap per
module, metallic bevel (light top/left, dark bottom/right) on modules, L-shaped technical brackets at the console corners, warm
gold only for earned rank, red only for the attack warning (stronger than any other colour by test). No glow, no gradients.

**Global Powers: individual plates, no enclosing frame.** `powersPanelDraw` draws one `powerPlateRect` (the real button plus an
even margin) per visible slot - ready (enabled) plates get the ready outline and accent bevel, others the dim outline;
cooldown sweep/tooltips remain the buttons'. The slot's retail 59x56 frame art is cleared in Enhanced (remembered and restored
for Original) so there is one plate per power, not two boxes. The group's extent is the plates only (tested for 1..5 visible
slots, column and row: no overlap, even gap, mostly plate, within the reserved placement frame). Placement unchanged from
refine3 (safe region, column / compact row, detached from the island); the reserved frame is now only the clearance contract.

**Radar black bars: root cause and fix.** `W3DRadar::draw` (retail, `// draw black border areas where we need map`) keeps the map
aspect inside the radar window (`Radar::findDrawPositions`) and fills the unused part of the window with opaque black plus
grey edge lines. The retail window is 167x152 (1.10:1), so any map that is not that shape - e.g. a square map - shows black bars
left/right (taller maps) or top/bottom (wider maps). Enhanced exposes it because the module is large and dark. Fix: the radar
WINDOW is fitted to the map's aspect inside the module's radar slot every frame (uniform `containFit`, centred; map extent via
the new const getter `Radar::getMapExtent()`), through the shared placement function. The radar then fills its window (no black
bar by construction - tested with a replica of `findDrawPositions` for six map shapes on all surfaces/UI scales), the mouse
mapping (`localPixelToRadar` uses the window size) stays exact, hit region = the actual radar, the map is never stretched. The
slot behind it is module material with a bezel around the real radar (red while the warning is lit). Original keeps the retail
window and its black bars (W3DRadar/Radar.cpp are not edited; contract tested).

**Command, info, cash, General, attack warning.** Refine3 composition kept: 12 fixed slot wells (retail indices), lit occupied
slots, recessed selection well, cash strip, General module (entry icon, trough, 2x2 rank grid), accepted attack lamp - all now
coloured from the theme.

**Not done on purpose.** Enhanced General Powers popup: the theme layer is ready for it (same data/mechanics, themed
presentation) but it is a separate, bounded task and is the next refinement. No China/GLA/Europe/Russia art.

**Future factions.** Shared: module roles, interaction model, command/XP/power behaviour, responsive layout, hit-test and
image-fit architecture. Per faction: Theme colours/weights/switches now; later textures, emblem, typography, plate shape, warning
styling, Global Powers plate styling, General Powers popup styling. USA: clean blue/gunmetal terminal; China: heavy industrial
red/gold; GLA: improvised/worn; Europe: refined modular; Russia: heavy industrial olive/dark. A faction skin must not move core
controls (the layout is shared and tested).

**Limits.** Procedural placeholder art; theme carries colours/switches only (no textures/fonts yet); no General Powers popup;
radar fit is verified by arithmetic equivalence with the radar draw code, not by a native run.

## 16. Refinement 5: USA Enhanced HUD Visual Design v1 (after native review of `x64-36-enhanced-hud-refine4`: positive)
The refine4 geometry is the accepted framework and is **frozen**: island 1000x180 reference units, module frames, the 12-slot
command bay (retail indices), 1.2x/1.1x scales, rank grid, responsive rules. `AcceptedMajorGeometryIsFrozen` pins the numbers
(and physical island sizes on 1920x1200 / 3440x1440 / 5120x1440); changing them is a deliberate redesign. Only small internal
padding is used by the artwork; the footprint is unchanged and the bottom stays flush.

**Global Powers: screen-edge anchored (supersedes the safe-region rule of sections 14/15).** Human preference: on every
non-compact surface, 21:9 and 32:9 included, the plate column is mounted against the RIGHT screen edge (the extent the plates can
occupy ends 1 px from the edge; the plates themselves a few px from it). Trade-off, documented on purpose: on extreme
ultrawide (32:9) the powers are physically far from the centre of the action. Compact (<=1.45:1) surfaces keep the row above the
island's right end. Only visible slots draw a plate (retail populate order), gaps are the retail pitch, there is no enclosing
background. The shortcut window itself is wider than the plate column, so it extends past the screen edge; its extra width draws
nothing (retail slot frame art is cleared) and hit regions are the buttons. The shared mechanism (`applyPowerColumn/Row`) and the
real windows are unchanged.

**Theme architecture (more than a recolour).** `Theme` now carries a shape/material language next to the colours:
`shellCorner/moduleCorner/plateCorner` (+size), `moduleCaps`, `bevelModules`, `cornerBrackets`, `fasteners`, `seams`, `vents`,
`tickMarks`, `stateRails`, `railBreaks`, `bezelRef`, `wellDepthRef`. The single renderer reads only these (contract tested: no
faction names in the renderer). `differsBeyondColour(a, b)` tells a recolour from a different construction language (tested: a
red recolour is detected as a recolour; square corners + deeper wells + thicker bezel is not). The theme still cannot move a
control or touch state; shape sizes are bounded reference units.

**USA material language (all procedural).**
* Outer console: ONE gunmetal shell, machined (chamfered) top corners, lighter upper band, lit top edge, an accent rail broken
  into one segment per module, four corner screws, short machined seams (with a break) in the gaps between modules. Bottom is
  square and flush.
* Modules: chamfered structural panels with bevel, an inset header rail and a short accent cap; vent groups in the radar and
  General headers.
* Wells: recessed with inner shadow (depth from the theme) and a faint lit lip; content looks mounted inside.
* Radar: a military display bezel around the (aspect-fitted) radar - thick machined frame, deep recessed screen, four screws,
  graduation ticks; the whole bezel turns red while the attack warning is lit. The lamp sits in its own recessed housing with two
  screws; the accepted lamp (red plate, white triangle) is unchanged. Utility controls are hardware keys: chamfered plate, LED
  strip (blue on hover), glyph; pressed = depressed plate, disabled = muted.
* Command bay: one recessed keypad (framed bezel + deep recess) with the 12 fixed cells. Empty cells are subdued wells with tiny
  corner ticks; occupied cells are mounted machined plates with a bottom state rail; hover = blue rim, pressed = depressed,
  disabled = muted. Retail overlays/cooldown/tooltips are the buttons'. Dense (12) and sparse contexts use the same cells.
* Cash: inset readout with a blue status edge, three status segments and header-rail ticks (value, cadence and income option
  untouched; thousands grouping Enhanced-only).
* General: XP gauge with minor/major instrument ticks, the aspect-fitted General Powers entry in its own well, rank grid where
  earned ranks are gold on a mounted plate with a gold rail and unearned ranks recede.
* Global Powers: each plate is a machined module: cut corners on the battlefield side, square on the screen edge, recessed icon
  well, blue state rail (ready) / dim (not ready), hover rim, pressed depressed; cooldown sweep is the button's own.

**Still procedural / placeholder.** All of the above is drawn from rectangles/lines (no textures, no typography change, no
animation); detail density was kept low so it reads at native size. No retail or EA art is added to the repository.

**General Powers readiness.** The popup can reuse these primitives unchanged: `Theme` (colours + shape), `drawBevelPlate`,
`drawRecess`, `drawModule`, `drawScrew`, `drawVents`, the state-rail and hover/pressed conventions, and the image-fit policy. It
is not modernised in this batch; it is the next milestone.

**Future factions.** China/GLA/Europe/Russia can differ materially by choosing different shape language (square vs chamfered vs
asymmetric corners, no/many fasteners, seams vs welds, rail style, bezel and well depth) in addition to colour; the layout,
interaction, hit testing and image fitting stay identical. Not implemented now.

## 17. Refinement 6: USA Design2 (after native review of `x64-36-enhanced-hud-usa-design1`)
Geometry is unchanged (pinned by `AcceptedMajorGeometryIsFrozen`); this batch is art direction plus one correctness fix.
The accepted layout is the intended **shared Enhanced HUD framework** for every faction (structure frozen, faction art not).
It is NOT formally declared "Enhanced HUD Shared Framework v1" until the human accepts the native result of this candidate;
at that point the frozen geometry, module boundaries, responsive rules, hit-test contracts, command slots, radar behaviour,
image-fit policy, Global Powers rules, theme API and renderer primitives are to be recorded as v1.

**Global Powers: true right-edge dock - root cause and fix.** Design1 left a visible strip between the plates and the screen.
Cause: the layout positioned the shortcut window from nominal numbers and deliberately kept a 1 px inset plus a rounding
tolerance on the screen side of its frame; the real child windows are rounded when they are scaled, so the DRAWN plates
(button + `powerPlateRect` margin) ended 4-6 px short of the edge. Fix (shared mechanism, `EnhancedHudWindows.h`):
`applyPowerColumn(..., edgeRight, plateMargin)` measures the real leaf button windows after the layout and shifts the parent by
the exact remainder so the right-most drawn plate edge equals `edgeRight` (= the surface width); the layout no longer keeps an
inset or tolerance on that side (`powerFrame.right == surfaceW`). The real drawn rectangle is tested (mock windows with the
loader's rounding): `DrawnGlobalPowerPlatesEndExactlyAtTheScreenEdge` - 9 surfaces x 5 UI scales x 1..5 visible powers, outer
edge within 0-1 px, never past the surface. Compact surfaces keep the row.

**Design2 changes (all procedural, shared renderer, theme-driven).**
* Depth hierarchy (theme colours and test): level 0 chassis (mid gunmetal, vertical grade, fine grain, stepped machined top
  corners, lit top edge); level 1 modules as RECESSED navy panels set into the chassis (inner shadow, lit lip, header band with
  status lights); level 2 wells (near black); level 3 RAISED machined control plates and bezels (lighter than the chassis);
  level 4 icons / LEDs / rank. Previously the modules were lighter boxes on a dark plate; now they read as windows in one chassis.
* Raised structural ribs between the modules (with a broken centre seam and a status light) join the modules into one device;
  blue is illumination (LED groups, hover, ready rails) and no longer traces every rectangle; screws reduced to 2 (shell) + 2
  (radar bezel) and none on the lamp.
* Radar: raised bezel around a deep recessed screen, graduation ticks, LED group, red bezel when the attack warning is lit; the
  accepted lamp sits in a raised housing. Selection: raised bezel around a recessed information screen. Cash: raised bezel around
  a deep data readout with a blue status edge and segments. Command bay: raised frame around a deep keypad recess; empty cells
  are shallow wells without borders, occupied cells raised plates with a state rail. General: XP gauge and General Powers entry in
  raised housings, rank grid in a recessed bay (earned = raised plate with gold rail, unearned recede). Global Powers: raised
  machined plates, cut corners on the battlefield side only, square against the screen edge, recessed icon well, blue ready rail.
* Labels (FUNDS, RANK ...): audited and deliberately NOT added - the procedural renderer has no safe font path (labels need
  DisplayString/fonts); they remain a candidate for the first asset/typography pass.

**Theme architecture.** New theme data: raised control colours, chassis grain colour, `brushedChassis`, `junctionRibs`,
`moduleLeds`; removed unused `moduleCaps/capPermille/cornerBrackets/railBreaks` (every field is read by the renderer; contract
tested). `differsBeyondColour()` still separates a recolour from a different construction language. Planned expectations,
documented here so future factions are not recolours: China - heavy industrial (square massive framing, thick bezel/deep wells,
red lighting); GLA - improvised (asymmetric, welded/bolted, worn, exposed fasteners, green/amber instruments); Europe (future) -
refined modular; Russia (future) - heavy olive/dark industrial. China and GLA still fall back to Original; nothing for Europe/Russia.

## 18. Refinement 7: USA Design3 + General milestones, General Powers popup and Battle Overview (after native review of `design2`)
Native result of Design2: Global Powers flush against the right edge (accepted), geometry and footprint good, direction right.
The major HUD geometry stays frozen (`AcceptedMajorGeometryIsFrozen`); this batch is presentation / information architecture.
"Enhanced HUD Shared Framework v1" is still NOT declared; it waits for the human's native acceptance of this candidate.

**Design3 (HUD).** The Design2 hierarchy (chassis > recessed modules > wells > raised controls > LEDs) is kept; added only where it
carries meaning: a ventilation grille on the left end cap and two power lamps on the right end cap (asymmetric), grilles in the
info and General module headers, structural ribs/bolts/status lights between the modules (Design2) - no outlines on every
rectangle, screws stay at 2 + 2. All of it comes from theme switches (`vents`, `junctionRibs`, `moduleLeds`, ...).

**The four General cells: audit and meaning.** Audit of the retail data model: the General Powers are grouped by the rank at
which they open (sections for rank 1, 3 and 8 of `GeneralsExpPoints.wnd`), every power is the same kind of purchase; there are no
four honest power categories, so none were invented. The model does expose (local player, read-only): rank level, skill points,
cumulative skill points per level (`RankInfo::m_skillPointsNeeded`), unspent science purchase points. Therefore the four cells are
the General's **promotion milestones**: the rank ladder from rank 2 to the top is split into four equal steps
(`EnhancedHudPanels::milestoneRank`; Generals' 5 levels -> ranks 2,3,4,5; a longer table -> quartiles ending at the top rank).
Earned = raised plate, gold star and rail; Next = recessed cell filled with the REAL xp progress toward that milestone
(`milestoneProgressPermille`); later = dim star. Unspent General Points are five pips under the General Powers entry (lit up to
the real count, saturating) and the entry gets a blue rim while points are available. They are indicators, not controls - nothing
is clickable that is not a retail window. Gold remains rank. Mapping and arithmetic are unit tested.

**General Powers popup (`GeneralsExpPoints.wnd`).** Enhanced-only skin = the popup parent's draw function (retail background
bitmap cleared and restored for Original, original draw function restored): chassis, one recessed module per section (header,
1-star, 3-star, 5-star + exit row), raised housing around the experience bar, raised plate behind every visible power with
the state read from the flags `ControlBar::updateContextPurchaseScience` already sets (enabled = available, disabled +
ALWAYS_COLOR = purchased/gold, disabled = locked), hover rim, pressed state, exit button housing. The popup's texts, progress bar,
buttons, tooltips and purchase logic are the retail windows - every power stays reachable, rank requirements and point behaviour
are untouched; hit regions are the retail buttons. Power icons are aspect-fitted. `popupSectionsHoldEveryRetailWindow...` pins that
every retail window rectangle lies in exactly one section.

**Battle Overview (`Diplomacy.wnd`).** Audit: the retail overlay is the in-game player list (Mute / Player / Side / Team / Status;
multiplayer) or the mission briefing list (solo). Live and safe data: player name, faction, team, human/CPU, alive / defeated /
observer / gone, mute state, elapsed game time (shared), map name. NOT exposed, with reasons: army value, unit/building counts,
income, money, resources (hidden information of other players), kills/losses/efficiency (post-game ScoreKeeper data), ping (not
available per slot in every mode). So only the PLAYERS tab is built; ARMY / ECONOMY / COMBAT exist in the model
(`overviewTabs`) as unavailable with their reason and are not drawn - no fake statistics. Enhanced skin: chassis, header band
(BATTLE OVERVIEW, map, elapsed time) drawn with the game's own fonts, tab strip (PLAYERS), recessed table module with a plate
behind every visible player row, raised housing around the retail Done button. The retail windows, mute buttons and
`PopulateInGameDiplomacyPopup` are untouched. Enhanced additionally shows the player table in SKIRMISH (retail shows the
briefing list there); that branch is gated on Enhanced and skirmish and adds no information (the slot list is the setup screen).
Faction icons were not added (no confirmed legal runtime asset path); human vs CPU is carried by the retail team text ("AI").

**Shared framework preparation.** Shared (all factions): geometry, placement/hit testing, image-fit, popup and overview
architectures, models in `EnhancedHudPanels.h`. Faction (theme): materials, corner/edge/fastener/seam/vent language,
illumination, plate shapes. China/GLA fall back to Original; Europe/Russia are not implemented.

**Limits.** Procedural placeholder art; labels use the game's Arial display strings (no faction typography yet); the popup/overview
are skins on the retail layout (no re-layout); unavailable overview tabs wait for engine statistics that are safe to expose.

## 19. Refinement 8: USA Design4 / Shared Framework candidate (after native review of `design3`: positive)
Native result of Design3: bottom HUD, radar, selection, command bay, cash, Global Powers edge dock and Battle Overview accepted;
two items remained: the power meter and the General Powers popup (still the retail layout with a new skin). Geometry stays frozen
(`AcceptedMajorGeometryIsFrozen`); the popup is NOT part of the frozen geometry and is re-laid out. Status remains CP10 IN REVIEW;
"Enhanced HUD Shared Framework v1" is NOT declared - it is the next step only if the human accepts this candidate.

**Power meter.** `PowerWindow` (the retail gauge that carries the real production / consumption in the game's own green / yellow /
red, and the retail tooltip) is now permanent instrumentation: taller (14 units instead of 7), in a raised housing with a recessed
track of `kPowerSegments` = 20 segments that is ALWAYS drawn - an empty or nearly empty meter still reads as a meter. The gauge
draw adds segment separators over fill and track through a gated hook (`EnhancedHudImage::drawGaugeSegments`, active only for the
registered power window in Enhanced). A bolt symbol sits left of it: ready-blue while `Energy::hasSufficientPower()` (read only,
the same state the gauge colours by), warning-red when not. No numbers are shown (the engine exposes the values, but the main HUD
stays uncluttered) and nothing is written to any power state. Header ticks that did not mean anything were removed.

**General Powers popup: real re-layout.** The retail popup is one 296 x 432 column. In Enhanced the SAME windows (texts, progress
bar, 16 power buttons, exit) are moved by `EnhancedHudPanels::popupTarget` to a compact 536 x 300 console: header module (rank
name and numeric level, experience label and bar, remaining General Points), 1-star tier top left, 5-star tier bottom left with
Done, the 3-star grid on the right - its 4 columns x 3 rows are science chains and keep their exact relative arrangement. The popup is
centred above the console (`popupPlacement`), sized from the UI scale and always reduced to fit the surface (<= 95 % width,
<= 90 % height; typically < 30 % of the screen area), so it never goes full screen. "Level" is the numeric rank (1-5) beside the rank
title, so both stay; nothing was hidden. Aspect-fitted icons; states: available = raised blue plate, purchased = raised gold plate,
locked = recessed and dimmed, hover rim, pressed depressed; the retail bitmaps of the decoration windows are cleared and restored.
Purchase logic, point costs, rank requirements, tooltips and command messages are the retail windows' and untouched; geometry is
recorded before it changes and restored exactly (positions and sizes; the game owns shown/hidden). Tests (mock windows with the
retail hierarchy and rectangles, 9 surfaces x 5 UI scales): all 16 powers + Done present, inside the popup, never overlapping, every
probe point of every button hits that button, tiers/chains preserved, popup on the surface and centred. Original shows the retail popup.

**HUD detail audit.** Reviewed for missing meaningful details; deliberately nothing was added beyond the power instrumentation:
an empty command bay when nothing is selected is correct, no fake telemetry or buttons. Theme gained `windowCorner` so popup and
overview construction can differ per faction.

**Candidate shared framework (not frozen).** Macro-geometry and function that Design4 proposes to freeze if accepted:
bottom island (1000 x 180 reference, 82 % width cap, bottom flush, centred), radar module and utility controls, selection
module, header (cash + power meter), 12-slot command bay with retail slot indices, General module (entry, XP, milestone cells,
point pips), Global Powers (column flush at the right edge, row on compact), General Powers popup structure (header, three tiers,
chains), Battle Overview structure (PLAYERS only), hit-test and image-fit rules, UI scale, Original/Enhanced switch and restore.
Acceptance criteria for freezing: human native acceptance of the candidate; matrix green; reference-guard set unchanged.

**Faction design contract.** All factions share the framework; only the Theme (and later assets) differs, by construction language,
not by colour. USA: advanced military electronics, precision-machined, dark navy/gunmetal, cool blue light, restrained gold rank,
red only for warnings. China (later): heavy industrial military console - darker, heavier steel, thicker frames and ribs, reinforced
edges, red status light with restrained amber/gold, monumental symmetry; NOT USA painted red. GLA (later): improvised field
equipment - salvaged and patched plates, asymmetry, welded seams, exposed fasteners, rough cut corners, amber/green practical lamps,
worn dark metal; NOT USA painted green; still as clear to use as USA (same grid, hit regions, hierarchy). Europe/Russia: future.

## 20. Refinement 9: USA Design5 - General Powers console (after native review of `design4`)
Native verdict on Design4: bottom HUD, power meter, Global Powers dock, Battle Overview good; the General Powers popup was
not (too wide and bulky, tall header, dead space, 1-star cramped, 5-star isolated, 3-star dominating, tier header texts wrapping and
overlapping). The bottom HUD and the Overview are untouched in this batch.

**Audit of the real hierarchy.** `SCIENCE_<SIDE>_CommandSetRank1/3/8` (CommandSet.ini): rank 1 = 3 powers in a row; rank 3 = 12
positions assigned column-major ("1 4 7 10 / 2 5 8 11 / 3 6 9 12" in the INI comment); USA: position 1 = Pathfinder alone, then
Paradrop 1-3, A-10 Strike 1-3, Emergency Repair 1-3 as the three levels of one science per column; rank 8 = 1 power (Daisy Cutter).
So the columns of the 3-star grid are real upgrade chains; the tiers open at ranks 1 / 3 / 8 and are labelled 1 / 3 / 5 star.
Nothing was added, removed or regrouped.

**New information architecture (same windows, moved by `popupTarget`):** a narrow promotion console (reference 320 x 474, UI-scaled
and always fitted: <= 95 % width, <= 90 % height; typically ~1/3 of the screen width) stacked in rank order:
1. header - rank name + numeric level (retail windows), experience track (raised housing, recessed, ticked; visible when empty)
   with "NEXT <next rank name>" (localised rank table) and the real numbers `skill / next level`, or MAXIMUM RANK at the top;
   remaining General Points (retail count window) in its own readout that gets a blue rim when points are unspent;
2. 1 STAR GENERAL - the three powers in a row; 3. 3 STAR GENERAL - the four chains (columns, retail order); 4. 5 STAR GENERAL - the
   power, with Done at the console's foot beside it.
Each tier has a short Enhanced-only label (stars drawn for the tier + `GUI:EnhancedTier1/3/5` with English fallback, unlocked/locked
lamp and word from the real rank) instead of the retail "RANK REQUIRED - n Star General" texts, which are hidden by this layout
(and only those three, recorded and un-hidden for Original). Labels are measured and shrunk to fit their module
(`fitPointSize`), so no heading wraps or crosses a border. New strings are presentation-only (documented fallbacks, not in the
retail text files).
**Chains.** For visible powers of the 3-star grid that share a column and are neighbours, a recessed channel and a rail join them; the
rail turns gold when the upper level is owned. Only real column neighbours are joined; no relationship is invented.
**States.** locked = recessed and dim; available = raised plate with a blue rail (hover rim, pressed depressed); purchased = raised
plate with a gold rim and rail; icons aspect-fitted. **Silhouette.** chassis with machined corners on all four corners, stacked recessed
modules joined by raised ribs with a broken seam; Done in the foot; construction from the theme (`windowCorner`, ribs, seams, bevels).
**Preservation.** Positions/sizes, images, draw function and the three header windows' visibility are recorded and restored for
Original; all gameplay reads are read-only. **Tests.** popup placement/fit on 9 surfaces x 5 UI scales, 16 powers + Done present,
inside, non-overlapping, full-plate hit probes, tiers in rank order, chains kept, only the three headers hidden, rectangles
recognised exactly once, label fitting arithmetic, tier/rank constants.

**Sequence after human acceptance (not done now):** freeze Shared Enhanced HUD Framework v1; keep USA as the reference; implement
China (shared geometry, heavy-industrial construction language), native review; implement GLA (improvised language), native review;
only then consider CP10 closure.

## 21. Refinement 10: USA Design6 - General Powers / promotion console (after native review of `design5`: console rejected)

Native review of Design5: main bottom HUD, Battle Overview, Global Powers edge dock, power meter and the USA visual language were
accepted; the General Powers console was not. Shared Enhanced HUD Framework v1 stays UNFROZEN; CP10 stays IN REVIEW.

**Audit of the retail UI (Science.ini, Rank.ini, CommandSet.ini, GeneralsExpPoints.wnd).** 14 USA powers in three tiers:
1 star (rank 1): Paladin Tank, Stealth Fighter, Spy Drone - no prerequisites. 3 star (rank 3): Pathfinder (standalone) and three
chains of three steps - Paradrop I > II > III, A-10 Strike I > II > III, Emergency Repair I > II > III. 5 star (rank 5): Daisy Cutter
(standalone). Every power costs one General Point; `Rank.ini` has 5 ranks (the command sets are legacy-named Rank1/Rank3/Rank8, but the
5 star tier opens at rank 5). A retail button is enabled = purchasable now, `ALWAYS_COLOR` = owned, otherwise disabled; the retail
tooltip is the science name, description and cost.

**What was wrong with Design5.** (1) The connector loop joined any lower button in the same column, including the 5 star power, drawing a
long blue line across unrelated sections. (2) The tier lamp for the 5 star tier used rank 8, which does not exist, so it always read
LOCKED. (3) A forced 320 px width squeezed the content instead of letting content size the console.

**Design6 geometry.** Reference 440 x 500 (about 0.45 of a 1920 surface wide; placement fits to 90% of the width and 80% of the height,
centred above the HUD, never fullscreen). Header (rank, XP track with `n / m`, NEXT promotion or MAXIMUM RANK, General Points readout),
then stacked tiers 1/3/5 star, Done in the foot. The 3 star grid keeps four columns - a column is a chain - with a 58 unit row pitch so
the connector between steps is readable. All 14 powers + Done stay retail buttons (purchase commands only come from them).

**Chains.** A connector exists only between two shown powers when `ScienceStore::getPrerequisiteSciences` (new read-only getter, both
titles) says one is a prerequisite of the other AND they are vertically adjacent in the same column with nothing between. Chained powers
carry a step numeral I/II/III; the connector is gold when the upper step is owned and the lower one is not yet (next step) in blue.
Pathfinder, the 1 star trio and the Daisy Cutter get no connector and no numeral.

**General Points / XP.** The points readout is a dedicated module: label, large number; dim at zero, blue/gold with a rim when points are
unspent; read-only. The XP track is always visible (recessed with ticks) with a real fill from the retail progress bar; Level n is
secondary text ("RANK n / N").

**States.** Locked (rank or an upgrade step missing: recessed, dim), Available (blue raised, hover rim, pressed), NeedsPoints (prerequisites
met, no point: flat, engaged outline, pending lamp), Purchased (gold). A detail strip in the 5 star foot shows name, cost, required science and
rank of the hovered power - real data only.

**Original preservation.** The six retail windows replaced by the skin (Rank1/3/8Required, Level, points label and count) are hidden by
the Enhanced layout only if they were shown, remembered, and unhidden for Original (Enhanced > Original > Enhanced is exact; mock
transition test). Faction look comes from `EnhancedHudTheme`; layout is shared; China/GLA fall back to Original.

**Tests.** Panels: tier ranks {1,3,5}; the 14 USA powers placed exactly once; exactly six real connectors; no connector for standalone
powers or unrelated/separated plates; chain depth/membership; detailed states. Interactive: placement/fit on 9 surfaces x 5 UI scales,
16 powers + Done present/inside/non-overlapping with centre, corner and edge probes, the six hidden windows, transition. Contract:
connectors only through `chainConnectors` + the science relation, read-only getter, restore of hidden windows.

## 22. Refinement 11: USA final visual-state cleanup (after native review of `design6`: "very good")

Geometry, hierarchy and every other surface are untouched; only the meaning of blue rims changed.

**Audit of every blue border.**

| where | source before | class |
|---|---|---|
| General entry button, permanent rectangle | `drawOpenRect(..., c.ready)` whenever points > 0 | B (available action) drawn like D (focus) - changed |
| General entry button, flashing | retail `m_generalButtonHighlight` art swapped by `ControlBar::getStarImage` | G (inherited retail art) - suppressed only while Enhanced is active |
| General entry button, hover/pressed | retail H/P bitmaps only | C/E - now also an Enhanced hover rim and pressed inset |
| Rank/star module cells | gold rail (earned), `c.ready` fill line on the next cell | F/progress - kept |
| XP gauge ticks | `c.accent` major tick | A (theme decoration) - kept |
| General Points readout | full `c.ready` rectangle when points > 0 | B drawn like D - changed to top/foot status rails |
| Power plates | available resting rim was `c.ready` (same as hover accent family) | B - changed: resting available is a muted rim with the blue rail, hover/pressed is the bright rim |
| Needs-points plate | hover/rest shared the available colour | changed: neutral at rest, muted on hover, never bright |
| Purchased plates | gold rim | F - kept |
| Done / command keys / Global Powers / overview | hover-only accent rim | C - kept |

No Win32 keyboard-focus drawing exists in the Enhanced paths (`winGetFocus` is never consulted); the "selected" look came from the
rectangle above, the retail blink art and the bright resting rim of available powers.

**Result.** Resting General button: no outline. Hover: blue rim. Pressed: sunk edge. Unspent points: a small blue status lamp on the
housing plus the existing pips (the retail blink is suppressed in Enhanced only). Points readout: instrument rails, dim at zero,
not clickable. Power states are separated by a semantic outline role (`EnhancedHudPanels::powerOutline`): locked none, needs points
neutral, available muted at rest / active on hover-press, purchased gold; tests assert the roles and not the colours, so faction
themes remain free to choose them. Original is untouched (every change is behind `EnhancedHud::active()` / the Enhanced draw hooks).

## 23. USA accepted; Shared Framework v1 frozen; China begins

Native review of `201b126b1`: **USA Enhanced HUD ACCEPTED.** Small visual refinements may follow in a future cross-faction polish
pass; USA prototyping is not reopened. Shared Enhanced HUD Framework v1 is now FROZEN (`ENHANCED_HUD_FRAMEWORK_V1.md`,
`EnhancedHud::kFrameworkVersion`, pinned shared headers). China: `X64_36_ENHANCED_HUD_CHINA.md`. GLA follows only after China passes.
CP10 stays IN REVIEW until USA, China, GLA and Original preservation have all passed native review.
