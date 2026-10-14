# X64.33 Modern Experience foundation: display, window, aspect, UI scale

Goal: 4:3, 16:9, 21:9 and 32:9 are ordinary configurations of one coherent presentation architecture, while the
game stays Generals. Nothing here touches GameLogic, rules, balance or replay/network state: it decides where pixels
go and what the window looks like. Offline tests and source contracts prove the geometry and the gating; native
behaviour is **not** claimed proven (see section 10).

## 1. Architecture

| Layer | File | Role |
|---|---|---|
| Geometry (pure) | `Common/DisplayGeometry.h` | physical surface, 800x600 reference canvas, uniform UI scale (Auto = quarter steps that fit, or 50-400 %), anchors, layout families (Frontend / Hud), Hor+ field of view |
| Enumeration (pure + Win32) | `Common/DisplayModes.h` | monitors, desktop mode, driver modes (>= 800x600, >= 24 bpp, progressive, refresh canonicalized 59->60 ..., de-duplicated, never invented) |
| Settings (pure) | `Common/DisplaySettings.h` | persisted keys, legacy migration, Classic defaults, `resolve()` of settings + attached monitors into window client / mode / layout, size choices |
| Runtime state | `Common/DisplayRuntime.h` | the live client size, layout and hooks; the one place engine code asks "what is the display" |
| Engine integration | `W3DDisplayOrdinaryHost.cpp`, `WinMain.cpp`, `ww3d.cpp`, `D3D11Backend.*`, `PresentationLifecycle.*`, `W3DDisplay.cpp`, `W3DView.cpp`, `GameWindowManagerScript.cpp`, control bar files, `GlobalLanguage.cpp` | see sections 3-7 |
| Options | `GameClient/ModernDisplayOptions.*`, `OptionsMenu.cpp` | runtime Generals gadgets, no new toolkit |

`DisplayRuntime::modern()` is true only in a **normal launch** with `ModernExperience::FEATURE_MODERN_DISPLAY`
(i.e. not `-classic`, not a diagnostic profile). Everywhere else the accepted fixed 800x600 logical display, the
developer review scale and the stretched swap chain are unchanged byte-for-byte (contracts below).

## 2. Legacy 800x600 assumptions, classified (no blind replacement)

| Class | Sites | Decision |
|---|---|---|
| A true logical/reference coordinate | WND `CREATIONRESOLUTION`, control-bar scheme `ScreenCreationRes`, `NativeMenuComposition` (smoke only) | kept: they define the 800x600 reference; the UI transform maps them (section 5) |
| B physical render size | `initOrdinaryHostPresentation` coercions/throws, `WW3D::Init_Native_Presentation` 800x600 gate, device/target checks, `clientIsExact`, screenshot/BMP writers, review-scale mouse mapping | modernized behind `DisplayRuntime`; classic text/receipts unchanged |
| C camera/projection | `W3DView::setWidth` 50 degree horizontal field of view | Hor+ when modern (section 4) |
| D gameplay/simulation | none found in the presentation path | untouched |
| E unrelated | object-count tolerance 800, intro wait 800 ms, `STATE_END = 800`, snow-shadow tile coordinates, 600 s time bounds | untouched |

## 3. Display enumeration and window modes

* `DisplayModes::enumerateMonitors()` uses `EnumDisplayMonitors` + `EnumDisplaySettingsEx` (GPU independent).
  The primary monitor is index 0; the Options popup lists every attached monitor with its desktop size.
* **Windowed**: client = back buffer = logical display = the chosen size; the window is centred on the chosen
  monitor's work area. A stored size that does not fit the work area is reduced (reason logged).
* **Borderless fullscreen**: `WS_POPUP` window exactly on the monitor bounds, client = desktop resolution, no
  display-mode change, no topmost, no exclusive ownership.
* **Exclusive fullscreen: deferred.** A stored `Exclusive` is reported and applied as Borderless. Reason: it
  needs DXGI mode switching, device-lost/occlusion handling and focus-loss restore that cannot be validated
  without native evidence; Borderless covers the common need safely.
* Refresh rate is **not** a user control: windowed/borderless present at the desktop rate (logged as
  `desktop_refresh`). A refresh control would be non-functional without exclusive mode, so none is exposed.
* Display-affecting changes apply live **only outside a running match**; in a match the persisted setting applies
  from the menus/next launch. VSync applies live everywhere.

## 4. Aspect-correct world (Hor+)

`W3DView::setWidth` used `50 degrees * (view width / display width)` horizontally at every aspect, which crops the
world vertically on wide surfaces. When modern, the base horizontal angle is
`2*atan(tan(25 degrees) * aspect / (4/3))`: the 4:3 *vertical* field of view is preserved at every aspect, so 4:3 is
identical to classic and wider surfaces see more to the sides. No stretch, no 4:3 crop, no change to camera height
or zoom, no Extended Camera. The viewport under the control bar ends `(1 - viewportHeightScale) x 600` reference
pixels above the bottom edge at the UI scale, independent of surface and scale.

## 5. UI coordinate system and scaling

* WND windows keep their authored 800x600 coordinates. `parseScreenRect` (modern) expresses a rectangle on the
  reference canvas, applies the **uniform** UI scale and an anchor, and rounds edges so neighbours never gap.
* Layout families (`layoutKindForFile`): **Frontend** (all menus, dialogs, popups) lives on the centred canvas;
  **Hud** (`ControlBar*.wnd`, `GenPowersShortcutBar*.wnd`, `InGameChat.wnd`, `ReplayControl.wnd`) shares one bottom
  origin; right-docked HUD parts (the shortcut sidebar) additionally hug the right physical edge. Descendants keep
  their root window's anchor, so a parent and children can never disagree.
* Control-bar scheme art uses the same factor and the bottom-anchored origin; HUD code that multiplied by
  `display/800` per axis (military captions, slider thumbs, window animations, resizer) uses the uniform scale.
* Fonts follow the UI scale (`GlobalLanguage`), so text keeps its proportions inside panels.
* UI scale: Auto (quarter steps, 100/125/150/175/200/225...) or 75/100/125/150/175/200/250/300 %. It is clamped
  so the canvas always fits. It never reaches logic, the world, targeting or replays.
* Not done: art for a 21:9/32:9 frontend background (the 4:3 menu art is centred; the sides are black), safe-area
  insets for TVs. Listed in the limitations.

## 6. Input

Window client == logical display == swap chain when modern, so `Win32Mouse` needs no mapping (the review-scale
`clientToLogical` stays the identity because the review percent is forced to 100). UI hit testing is by the window
rectangles produced by the same transform that draws them (tests: `DisplayRuntime.HitTestingUsesTheSameTransform...`).
Edge scrolling, selection rectangles, world picking and command targeting use physical pixels against the world
viewport/camera, which is the whole surface minus the control-bar overlap. Radar and the shortcut sidebar are
windows, so they hit-test through the same rectangles.

## 7. Options (Generals visual language)

`Options > Video`: the Resolution row is split (same technique as the Frame Rate Limit row); the right half is a
`Display Options...` button that opens a panel built from the packaged Advanced Display Options panel's own draw
data, fonts and buttons: Monitor, Display Mode (Windowed / Borderless Fullscreen), UI Scale, VSync, Unit Motion.
The Resolution list is real: fitting standard sizes plus driver-reported sizes (windowed) or the desktop size
(borderless). Frame Rate Limit stays the existing `FrameRateOptions` row (30/60/120/144/165/240 through
`FramePacer`); there is no second limiter and no new "Unlimited" value (the `FPSLimit` key already controls
uncapped). In Classic none of these controls exist.

## 8. Persistence and migration

Options.ini keys: `DisplayVersion`, `DisplayMonitor`, `DisplayWidth`, `DisplayHeight`, `RefreshRate`
(reserved), `DisplayMode`, `VSync`, `UIScale`, `SmoothUnitMotion`. Legacy `Resolution`, `FrameRateLimit` and
`FPSLimit` keep their meaning and are never rewritten by migration. Preferences without `DisplayVersion` migrate to
the modern automatic defaults (windowed, size chosen from the monitor, UI scale Auto, VSync off, motion original);
invalid or unavailable values fall back individually and the reason is logged (`display_modern_note`). Unknown keys
are never touched. Classic ignores stored values entirely.

## 9. Ground interpolation status

`-groundInterpolation` stays a developer flag; the Modern Experience adds `Options > Unit Motion` (`SmoothUnitMotion`)
as a **disable-able, default-off** switch that drives the same `GroundTranslation::setEnabled`. It is off by default
because native visual acceptance of the audited implementation is still pending (ground units only; no turret,
aircraft or projectile interpolation). Regression coverage: the existing `GroundTranslation*` test suite plus the
settings tests for the key. Replay/simulation state is untouched (presentation-only interpolation of completed
logic frames).

## 10. Classic (`-classic`) exactly

Disables or changes relative to the Modern Experience: native-resolution window (Classic is the 800x600 logical
display in a 1200x900-class review window with the DXGI stretch), borderless fullscreen, UI scale and anchoring
(original anisotropic WND scaling), Hor+ (50 degree horizontal field), VSync control (off), Unit Motion option
(and stored `SmoothUnitMotion`), the Display Options popup, the shortcut-sidebar data overlay and the Skirmish
Starting Cash selector (X64.32 behaviour). Classic is still AMD64/D3D11 with the native audio and engine fixes; it
is not the old DX8/32-bit engine.

## 11. Tests and contracts

* gtest (`g_googletest`/`z_googletest`): `DisplayGeometry.*`, `DisplayModes.*`, `DisplaySettings.*`,
  `DisplayRuntime.*` at 800x600, 1024x768, 1280x720, 1920x1080, 2560x1440, 2560x1080, 3440x1440, 3840x2160,
  3840x1080, 5120x1440 and odd sizes (1111x777, 1777x999).
* python: `test_modern_display_contract.py` (gating, Classic paths, single limiter, Zero Hour untouched),
  `x64_33_deltas.py` lists the *only* hunks the older byte-identity contracts now allow in `ww3d.cpp`,
  `PresentationLifecycle.*` and `GameWindowManagerScript.cpp`; every other difference still fails them.

## 12. Known limitations / not done

* Exclusive fullscreen and a refresh-rate control (see 3).
* No render-scale/"internal resolution" independent of the window size.
* Frontend art is not extended on ultrawide (centred 4:3 canvas, black sides).
* Superweapon/named timers and the player-info list use the INI fractions of the display width; on 32:9 they sit at
  those fractions of the physical width rather than beside the sidebar.
* Live resolution switching rebuilds window layouts with the existing `recreateWindowLayouts` path; not run natively.
* Zero Hour is unchanged.

## 13. Native review (candidate `x64-33-modern-display`)

Staged and preflighted, not run: `build/dev-runtimes/native/Generals/x64-33-modern-display`

* `generalsv.exe` SHA256 `B9BBACFE03F39958722A11D392391DD24E991D2D4D7C696E0216C52199A0670F`
* `generalsv.pdb` SHA256 `DF2B99ECD853561CF7A285556F6B03F3D65F8B0EE7BAF0F2B2D2D7C104819F68`
* built from `d99142fdf` (docs-only commits follow); staged with the shortcut-sidebar overlay.

Review sequence (one session): launch normally; check `generals64.log` for `display_modern` (client/mode/monitor/
scale/aspect); frontend centred and clickable; Options > Resolution row > `Display Options...` (Monitor, Display
Mode, UI Scale, VSync, Unit Motion); change Resolution and Accept; switch Borderless; change UI Scale; VSync on/off
with the FPS counter; start a Skirmish and check wide world, bottom bar, radar, shortcut sidebar at the right edge,
edge scrolling and selection boxes; `-classic` launch for the unchanged 800x600 presentation.

## 14. First native review (x64-33-modern-display) and FIX1 (x64-33-fix1-ui)

The first native run reached the frontend and a Skirmish match and exposed four connected UI problems. All four came
from the same cause: runtime-built or runtime-sized UI was positioned with numbers that did not follow the window
system's own scale and draw order. FIX1 corrects them on top of the unchanged display architecture.

| Finding | Root cause | Fix |
|---|---|---|
| Options controls overlap | The Display Options panel used fixed pixel metrics (row pitch 52, top 58, pad 24) next to scaled fonts and control heights | `OptionsPanelLayout` derives every metric from reference units x the live UI scale and the panel's own size; tests at 75-300 % |
| Dropdown lists behind other controls | The window system draws a parent's children tail-to-head, so the head is the front. Window files create lower controls later, which is why original dropdowns open over what is below them. Runtime gadgets are created last and therefore land in FRONT of the Resolution/Detail dropdowns | `GadgetLayering::placeBehind` places each runtime gadget deliberately (frame-rate row behind Detail, Display Options entry behind Resolution); popup rows are created bottom-to-top; the Starting Cash combo is brought to the front |
| Black regions beside the control bar | The bar art is an 800 reference-unit band, centred; nothing drew or blocked input at the sides | `HudExtensionLayout`: the band is extended to both surface edges with a two-texel slice of the bar's own console panel (stretching one column cannot distort the radar, buttons or decoration); two top-level windows with the engine's `GameWinBlockInput` follow the bar's visibility so the extension is also solid for input |
| "Slower / Faster" returned | The geometric remnant filter used fixed 14 px / 140 px margins; the captions are 12 / 75 reference px away, i.e. outside the rectangle as soon as the UI scale was not 100 % | The rectangle comes from `MatchOptionsLayout` in the header's own scaled coordinates, is applied before the new controls are created, and no longer depends on the Starting Cash extension (Classic included) |

### Skirmish: Generals vs Zero Hour
Zero Hour's Skirmish screen keeps the Generals structure and adds a match-options row under the player grid at y 334-360
(Game Speed slider, "Starting Money" caption + combo at x 453-557, "Limit Superweapons" checkbox at x 593-745) and shifts
the Battle Honors statistics down 7 px. Generals keeps its own left column: the retired Game Speed header becomes the
**Match Options** header; row 0 is Starting Cash (label + 24 px combo on the grid rhythm); row 1 is reserved; the honors
statistics move down 7 reference px exactly as in Zero Hour. Starting Cash keeps its behaviour (Modern Experience only,
retail amount default, Zero Hour choice list, human and AI). The Zero Hour screen is a reference, not an import: no
Zero Hour data or content is used.

### Superweapon match option: deferred (feasibility documented)
Shared plumbing exists in base: `GameInfo::m_superweaponRestriction` (serialised in the game-info string and savegames),
`ThingTemplate` keyword `DeterminedBySuperweaponRestriction`, `SuperweaponRestrict` preference key, `GameLogic::getSuperweaponRestriction()`.
What base Generals lacks: a `GameLogic` restriction member/setter (this is simulation state and a savegame-format change)
and Generals Object INI that uses the keyword (the three superweapon buildings have no `MaxSimultaneousOfType`; it would
be a data overlay of `FactionBuilding.ini`). That is a rules-state change, so it is not slipped into a UI pass: the row
is reserved, the contract test refuses a fake checkbox, and the option stays a separate, explicitly designed milestone.

### Timers
Superweapon and named timers, the player-info list and popup messages used INI fractions of the physical width. They now
use `DisplayRuntime::hudX/hudY`: the same fraction of the bottom-anchored reference block at the UI scale (Classic: the
old fraction of the surface), so they keep their relation to the control bar on every aspect. Native-unverified.

### Extensibility
HUD regions are named by geometry, not by monitors: left extension / core / right extension
(`HudExtensionLayout`), the shortcut sidebar (right-edge anchor), the timer block (`hudX/hudY`). Match Options has
`kRowCapacity = 2` rows. Options and Skirmish share the same discipline: reference units x scale, parent-derived rects,
explicit z-order.

### Not changed on purpose
In-game special-power sidebar (accepted X64.32), ground interpolation (still optional/default-off), gameplay, Classic.

### Still native-unverified
Everything visual: the dropdown stacking in the real Options screen, the extension art seam at the bar edges for all
three factions, input blocking over the extension, the Match Options rows, the honors shift, timer positions.

### FIX1 candidate
`build/dev-runtimes/native/Generals/x64-33-fix1-ui` (staged and preflighted, not run), built from `ffdb66083`:
* `generalsv.exe` SHA256 `8F54838FA1C160B019DE30C737DE11AB763CAF968498C7C3F43C386BEEA10BBE`
* `generalsv.pdb` SHA256 `46BCFB582A49D948678A639FF16FCAA6B5ABD8F467859D9E3B95385CC95CB5AF`

Review sequence: Options (open every dropdown, Resolution row, Display Options panel at UI scales Auto / 100 / 150 /
200 %); Skirmish (Match Options header and Starting Cash row, no Slower/Faster, honors statistics intact, dropdown over
them); start a match and check both sides of the control bar (continuous console, clicking the extension must not give
orders), radar and shortcut sidebar, superweapon timer position; repeat at a second aspect (borderless on an ultrawide
and a 16:9 window); `-classic` launch for the unchanged 800x600 presentation. X64.33 is NOT accepted yet.

## 15. FIX1 native findings and FIX2 Modern UI Composition (x64-33-fix2-modern-ui)

### FIX1 screenshot findings
Borderless/windowed presentation and the frontend composition are good: the display foundation stays. But
* **Options** still looked crowded: text left its controls, the Display Options entry was squeezed into the Video
  block next to Frame Rate Limit; the human asked for category tabs.
* **Skirmish**: Match Options is the right direction; "Starting Cash" wrapped onto two lines.
* **HUD**: the stretched console extension was technically filled but visually wrong: the bar looked pulled apart,
  the band dominated the lower quarter of the screen, and on 16:9 it already looked absurd.

### Decision
Stop stretching the 2003 composition. Keep the identity, the assets, the interaction concepts and the original
proportions of every *functional* group, and compose them for a wide surface.

### Text/control metrics root cause
Beyond fonts, the window system carries unscaled pixel constants. `W3DStaticText` wrapped text at `width - 10` and
placed it with fixed left/top margins while the window and its font were scaled by the UI scale, so labels wrapped
and ran out of their controls (the two-line "Starting Cash"). Wrap width and margins now follow the UI scale (Classic
unchanged: the scale is 1.0 there). New labels are also left-aligned and sized from the page layout, not from the
retail label rectangles.

### Options: categories
Modern Experience only (Classic keeps the retail page). A Generals-styled tab strip (the retail button art,
check-like selected state) over one content rectangle; every category is a column of label + control rows:

| Category | Contents | Source |
|---|---|---|
| DISPLAY | Resolution, Display Mode, Monitor (only with >1 monitor), UI Scale, VSync, Frame Rate Limit | retail Resolution combo + new gadgets + existing FrameRateOptions |
| GRAPHICS | Detail (Custom opens the retail Advanced panel), Gamma | retail |
| AUDIO | SFX, Music, Voice volume | retail |
| CONTROLS | Alternate Mouse, Scroll Speed | retail (key rebinding is a later milestone) |
| GAMEPLAY | Unit Motion (Original / Smooth) | new gadget (presentation preference; default off) |
| NETWORK | the retail IP / firewall / proxy rows, moved as a block | retail |

No renderer feature is faked: Refresh Rate and Exclusive stay deferred; no render scale, no DLSS. Match rules
(Starting Cash, future superweapon rule) are Skirmish creation settings and are not on the Gameplay tab.
Geometry is `Common/OptionsPageLayout.h` (reference units x UI scale; rows 24 on a 44 pitch; label 200 / control 225;
content 485 x 408 - the page never grows with the surface). Upper rows are brought to the front last, so a dropdown
always opens over the rows below it.

### Skirmish
Match Options keeps FIX1's structure (header, row 0 Starting Cash, row 1 reserved, honors shift as in Zero Hour).
The label column is 100 reference units (Zero Hour gave its caption 93) and the combo 130, so "Starting Cash" fits one
line at supported scales. Superweapon rule: still deferred (no GameLogic restriction state in base Generals).

### HUD composition: alternatives considered
* **A. Centred core** (chosen): the original control bar, scaled by the UI scale, centred; world on both sides.
* **B. Edge-anchored groups** (radar left / commands centre / info right with a maximum separation): needs the bar art
  cut into three pieces and new filler art; the art is one 800 unit band with silhouette overlaps, so cutting is lossy and
  the radar/commands relationship (which the game teaches) is broken. Rejected.
* **C. Hybrid / Zero Hour**: Zero Hour does not solve wide aspect ratios (it stays 4:3 with a taller sidebar); it only
  informs the sidebar and timer placement, which are already used.
* **FIX1 stretched extension**: rejected by native review.

### HUD: selected model
`Common/HudComposition.h`: core = 800 x scale wide, centred, bottom-anchored; the world uses the whole surface and
the tactical view is full height; under the core an opaque backing reproduces the retail black below the 80 % viewport
line; nothing outside the core blocks input (the FIX1 input walls are gone); timers/notices have named regions above
the core inside its width; the shortcut sidebar keeps its right-edge anchor (accepted X64.32) and the superweapon /
named timers, player-info list and popup messages keep the core-relative `hudX/hudY` anchors (stable at any aspect).

### Known limitations / native-unverified
* Everything visual; in particular the tab art (button images scaled to tab size), the Options pages at all scales,
  the 16:9 / 21:9 / 32:9 look of a centred core with the world beside it, and whether a subtle side treatment is wanted.
* The tab strip uses the Defaults button art; a dedicated tab frame would be new art.
* Network page is a moved retail block (not re-flowed).
* Live resolution switching still reuses `recreateWindowLayouts`.

### FIX2 candidate
`build/dev-runtimes/native/Generals/x64-33-fix2-modern-ui` (staged and preflighted, not run), built from `bd9953100`:
* `generalsv.exe` SHA256 `2EC09BF3F24656F8EE26779B094ABA518C13F27A783444B9EE38BA0986406008`
* `generalsv.pdb` SHA256 `5365DAF8E385505CD70D1922C1C54848A7209D246970CABD743EE2C6D1B2ABD3`
X64.33 is NOT accepted yet.

## 16. FIX2 native review and FIX3 (x64-33-fix3-ui-polish)

### Recorded native findings (x64-33-fix2-modern-ui)
* **Main menu**: the modern borderless presentation looks good - a strong indication of the intended direction
  (Generals identity + modern widescreen world). Not touched in FIX3.
* **Options**: the tabbed concept is accepted. The implementation was not: tab labels overlapped, tab widths ignored
  their text, tabs looked displaced/missing depending on the page, value text of Resolution/Detail/Unit Motion was
  covered by brown segmented slider art, other pages' controls drew through the active page.
* **Skirmish**: Match Options accepted; Starting Cash on one line; Slower/Faster gone; honors intact. Locked.
* **HUD**: the centred "HUD island" with the world beside it is accepted. One lifecycle defect: a few milliseconds of
  black HUD/backing before the HUD appears.

### Options root cause
FIX2 decided visibility by iterating the children of three panels it knew about. The volume sliders, the scroll
slider and the unnamed retail section headers are not children of those panels. After a page switch they were never
hidden, so they kept drawing at the row positions of the page they had last been placed on, which coincide with the
Display/Gameplay rows: segmented slider art through Resolution, Detail and Unit Motion; the "VIDEO"/"AUDIO" headers
(retail y 69-97) sat exactly in the tab strip row, which read as overlapping tab captions. (Identified by reading the
FIX2 visibility loop against the retail layout; the layout dump tool is `build/wnd_tree.py`-style, not shipped.)

### Ownership model
`OptionsTabs.cpp::apply` decides visibility for the WHOLE layout tree on every page switch, by exclusion: a window is
visible only if it is shared chrome (parent panel, Defaults/Accept/Cancel, version label, title), an item of the active
page, the retail Network block while Network is active (with retail-hidden members staying hidden), a tab, or an
ancestor of one of those. Gadgets are never recursed into (their edit box, arrow and list children are not page
items); the retail Advanced Display panel manages itself. Page membership is the pure table in
`OptionsPageLayout.h` (`ownerOf`, `ControlKind`), so the "one active page" invariant and "sliders only on
Graphics/Audio/Controls" are unit-tested. Pages are built at the end of the Options init, after every retail
show/hide decision. Windows are created once and switched (no per-switch creation, so nothing accumulates).

### Retail control reuse
The retail Resolution/Detail/Gamma/volume/scroll/network windows are still reused (they are bound to the retail save
and accept paths); the defect was ownership/visibility, not reuse, and rebuilding them would duplicate the retail
handlers. Classic is unaffected.

### Tab strip
Not six fixed buttons any more: caption widths are measured with the live font, each tab is `measured + 2 x pad`
(minimum width), the slack is shared so the strip is aligned with the content on both edges, and widths shrink
proportionally if captions ever cannot fit (`computeTabStrip`, tested 75-300 %). Tabs are drawn from primitives (dark
opaque fill, orange line work, gold border/text for the selected tab, white text on hover) instead of the large retail
Defaults button art, and their hit rectangle is the drawn rectangle. The Modern Options panel keeps the retail frame
width: six padded tabs fit in the 485 unit content width at every supported scale (a wider panel would stretch the
frame art; allowed, not needed).

### Combo internals
Combo children are laid out by the engine on resize (edit box = width - arrow width; list box matches); the earlier
"value text under art" was the stale slider art, not the combo. Dropdown z-order: upper rows are brought to the front
last on every page switch; tabs are brought to the front after the pages.

### HUD black flash
Root cause (from the code): the backing was anchored to the surface bottom while the bar slides in through the
control-bar animation (the art is offset by the marker delta), so the black backing sat at its final place before the
art arrived. The backing is now part of the band: the lower `(1 - viewportHeightScale) x height` of the band wherever
the band currently is, clipped to the surface; when the band rests on the edge it continues to it (the art ends one
reference pixel above the edge); `backingVisible(parentVisible, bandArtAvailable)` states the lifecycle rule. No delay,
no timer. The backing itself is kept: the console panels are translucent, and without it the world would show through
them where the retail viewport used to end.

### Still native-unverified
Tab look and hit areas, every page at several scales, dropdown stacking inside pages, the HUD slide-in with the
backing, the Network page as a moved retail block.

### FIX3 candidate
`build/dev-runtimes/native/Generals/x64-33-fix3-ui-polish` (staged and preflighted, not run), built from `e79c2989f`:
* `generalsv.exe` SHA256 `DAD2FA18DC6311E05893142AC4432D02A2891EE91232AA1417CBD72ECC978FFA`
* `generalsv.pdb` SHA256 `F5615485892A2C70CF88C2DDBEFA3896773C88C31B5BBCD8B99A5244B1A04D9A`
X64.33 is NOT accepted yet.

## 17. FIX3 native review and final polish (x64-33-final)

### Native evidence (x64-33-fix3-ui-polish)
* Tabbed Modern Options works; controls of inactive pages no longer leak; Resolution/Detail/other combo values are clean.
* Skirmish Match Options and the one-line Starting Cash stay clean.
* The centred coherent HUD is accepted; **HUD black pre-flash: FIXED / NATIVE VERIFIED**.
* The Modern widescreen main menu remains accepted.
* Remaining requests: tab spacing (captions read as one run) and the footer branding.

### Changes
* **Tab rhythm**: gap 4 -> 10 reference units, padding 10 -> 8 per side, strip 485 -> 505 units wide (centred on the
  content, inside the 541 unit frame art). Captions keep comfortable padding and a visible gap; slack is still shared
  so the strip is aligned on both edges; hit rectangle == drawn rectangle. Tested at 75-300 %.
* **Footer**: Modern Experience shows `Modern Experience by MKRaudstein | 64-bit • DirectX 11` (Options and Main Menu
  version labels). The product/version/hash string is written to the log (checkpoint `version`). The Modern window
  caption no longer carries product/author wording. Classic keeps the retail-style label.

### Remaining X64.33 limitations
Exclusive fullscreen and refresh-rate control (deferred); Network page is the retail block moved; frontend side art
is not extended; live resolution change reuses `recreateWindowLayouts`; superweapon match option deferred (rules state);
smooth unit motion remains opt-in until accepted.

### Final candidate
`build/dev-runtimes/native/Generals/x64-33-final` (staged and preflighted, not run), built from `2f5ead36e`:
* `generalsv.exe` SHA256 `0E05C2A6A25903EB7C90AD31CD8AF956BCCD5B65245CE63C31DADE88EB98976C`
* `generalsv.pdb` SHA256 `C0847976487F1B935FFD33EE808A7376B6FB49C5561F85D66817496BAD94229D`
Pending only the human check of tab spacing and footer; X64.33 is not marked accepted before that.

## 18. x64-33-final review and the final Options geometry pass (x64-33-final2-options)

### Native finding (x64-33-final)
Footer accepted (`Modern Experience by MKRaudstein | 64-bit • DirectX 11`). Options functionality correct, but the tab
strip was still visually crowded: CONTROLS and GAMEPLAY pressed against / exceeded their tab rectangles and adjacent
captions collided, while the panel had unused vertical space and too little horizontal room. A geometry problem, not
ownership. Design clarification from the human: preserve Generals *identity* (palette, translucent panels, gold line
work, typography, sounds), not the 2003 dialog geometry; a larger redesigned Modern Options is preferred.

### Root cause of the overflow
Tabs were sized from caption widths measured with a separate display string and a non-budgeted padding, inside a strip
fixed to the retail frame width; the drawn caption (own display string, own font) could be wider than its measured
value, and nothing guaranteed that "caption + padding" fits the tab.

### New composition (`OptionsPageLayout.h::computePanel`)
* **Frame strategy: A+runtime geometry.** The retail Options frame is not art: `OptionsMenuParentOld` is a plain colour
  rectangle with a gold border over a dim full-screen fill. Modern simply resizes it, so any width is safe; no stretching
  of proprietary art. (Classic keeps retail geometry.)
* **Width is derived**: `need = sum(max(safeCaption + 2 x 14, 72)) + 5 x 14` gaps; panel width = `max(need + 2 x 36 margins,
  760)` capped at 780 reference units (retail: 541). Height 568 (retail 585), centred on the reference canvas, so the
  panel fits the surface at every UI scale (the canvas does).
* **Captions fit by construction**: each caption is measured from the tab's own display string and budgeted
  `measured + 6 % + 2 units`; padding 14 units each side; gap 14 units; widths are content sized (AUDIO is narrower than
  CONTROLS/GAMEPLAY), slack is shared so both strip edges meet the content edges. `captionsFit` is reported in the log
  (`options_tabs ... captions_fit=`).
* **Reflow**: content 688 x 380 units; label column 38 % of the content width, control column up to 300 units; rows 24 on
  a 44 pitch (drop-down room below the last row stays inside the hit area). Defaults / Accept / Cancel, the footer, the
  title and rule line are placed from the same geometry; the retail Advanced Display panel is centred in the frame.
* Selected tab, hit rectangles (== drawn), page ownership: unchanged from FIX3.

### Tests
Caption containment (`safeCaption + 2 x pad <= tab width`) for all six captions at 75-300 %; minimum gap; content-sized
tabs; oversized captions shrink without overlap and report `captionsFit = false`; the panel is centred, inside the
canvas/surface and its bands do not overlap at 13 surface sizes x 6 UI scales (800x600 ... 5120x1440, 1111x777, 1777x999).

### Final2 candidate
`build/dev-runtimes/native/Generals/x64-33-final2-options` (staged and preflighted, not run), built from `a96092049`:
* `generalsv.exe` SHA256 `288BE8FC23982948A82DC898E6921F2C49B0278B6173686174499A3B303C6428`
* `generalsv.pdb` SHA256 `58B84E8DC86F7A071A9C92B30F55AF33DAABF6423814347E894F07B3D052CDA9`
X64.33 is NOT accepted yet.

## 19. X64.33 ACCEPTED AND CLOSED (2026-10-14)

Native review of `x64-33-final2-options` (generalsv.exe SHA256 `288BE8FC23982948A82DC898E6921F2C49B0278B6173686174499A3B303C6428`):
the wider Modern Options composition, tab layout, controls and footer are accepted.

Accepted in X64.33 (native-verified across the review series):
* Modern display foundation: native resolution, Windowed/Borderless, Hor+ world, UI scale + anchors, VSync, display
  enumeration and persisted settings, Classic fallback.
* Modern Options: tabbed categories (Display / Graphics / Audio / Controls / Gameplay / Network), whole-tree page
  ownership, clean combos and dropdown layering, wide runtime-composed frame with measured content-sized tabs.
* Skirmish Match Options (Starting Cash on one line, reserved row, Slower/Faster removed).
* Centred coherent HUD with full-width world, side-world input, backing lifecycle (black pre-flash fixed).
* Understated Modern footer / plain window caption; Modern main menu presentation.

Deferred beyond X64.33 (unchanged): exclusive fullscreen and refresh-rate control, key rebinding, superweapon match
option, Network page re-flow, frontend side art, smooth unit motion acceptance (opt-in).

### Design direction recorded at X64.34 closeout
The centred HUD island is retained. A later Modern HUD pass may rebuild/redesign the control panel for modern
displays while keeping the Generals visual identity; Classic stays the preservation path; the HUD is not to be
stretched across ultrawide displays just to fill horizontal space.
