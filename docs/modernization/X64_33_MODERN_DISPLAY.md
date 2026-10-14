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
