# X64.36 Enhanced Gameplay UI - USA prototype (CP10)

Status: **IN REVIEW. First native review found a systematic command/build hit-test bug (fixed in the follow-up batch below); awaiting a second native review. NOT accepted.**
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
