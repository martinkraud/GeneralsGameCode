# Settings completion audit (X64.37)

Scope: every control that Options shows in the Modern Experience (`Options > DISPLAY / GRAPHICS / AUDIO / CONTROLS / GAMEPLAY /
NETWORK`), found by reading the source (`OptionsMenu.cpp`, `OptionsTabs.cpp`, `OptionsPageLayout.h`, `ModernDisplayOptions.cpp`, the
retail `OptionsMenu.wnd`, the D3D11 backend) - nothing was run natively. Classic (`-classic`) keeps the retail page unchanged and is
not part of this table.

Classes: **A** fully functional | **B** functional but incomplete | **C** UI exists but the backend is missing | **D** intentionally
unavailable (and says so) | **E** obsolete (removed / replaced) | **F** deferred to another milestone.

## 1. Controls on the Modern pages

| page | control | class | evidence / disposition |
|---|---|---|---|
| DISPLAY | Resolution | **A** (windowed, main menu); read-only in borderless and in a match (**D**) - the label says "(desktop size)" / "(main menu)" | Real list (see 3): windowed = standard + driver sizes that fit the work area of the chosen monitor, borderless = the desktop size only. Changes the real window and swap chain (`NativeApplyDisplayMode` -> `D3D11Backend::Resize`) inside an Apply/Keep/Revert transaction. In a match the display is kept; the label says "(main menu)". |
| DISPLAY | Display Mode: Windowed / Borderless Fullscreen | **A** (main menu), **D** in a match | Real window style/position/size change inside the transaction. Exclusive fullscreen is **not offered** (section 4). |
| DISPLAY | Monitor | **A** | Real `EnumDisplayMonitors` list ("1 (Primary)  2560 x 1440 @ 144 Hz"), stable identity by Windows device name; the row is shown only when more than one monitor is attached (no greyed row). |
| DISPLAY | Render Scale (100 / 90 / 80 / 75 / 67 / 50 %, each with its effective internal resolution) | **A** (live everywhere, also in a match) | X64.37 v3. Real: the 3D world renders into an intermediate target of that size and is composited to the back buffer; HUD, text, tooltips, radar and the mouse stay in presentation space. See section 10. Default 100 % = the unchanged path. |
| DISPLAY | UI Scale (Auto, 75..300 %) | **A** (main menu), **D** in a match | Real: `DisplayRuntime::setUiScalePercent` + shell/control-bar rebuild; part of the same transaction. |
| DISPLAY | VSync | **A** | Live everywhere (`WW3D::Set_Native_Vsync`), persisted at once (not risky). |
| DISPLAY | Frame Rate Limit | **A** | The single `FramePacer` limiter (X64.33); unchanged. |
| DISPLAY | Refresh rate | **D** - no control | Windowed and borderless present at the desktop rate chosen by Windows/DWM; a selector would control nothing. The rate is shown read-only on each Monitor entry. |
| GRAPHICS | Detail (Low / Medium / High / Very High / Custom) | **B** | Drives `StaticGameLODLevel` -> engine flags that shared engine code reads; the per-flag visual effect in the D3D11 world path has not been verified individually. Disabled in a match (retail rule). |
| GRAPHICS | Custom -> Advanced panel: texture resolution, particle cap, 3D/2D shadows, cloud shadows, ground lighting, smooth water edge, extra animations, building occlusion, props, no dynamic LOD, unlock FPS | **B** | Each writes a `GlobalData` flag/`TheGameClient->setTextureLOD` that the shared engine consumes (`m_useShadowVolumes`, `m_useShadowDecals`, `m_useCloudMap`, `m_useLightMap`, `m_showSoftWaterEdge`, `m_useDrawModuleLOD`, `m_maxParticleCount`, `m_enableBehindBuildingMarkers`, `m_useFpsLimit`, ...). Not individually native-verified; unchanged. |
| GRAPHICS | Anti-aliasing (Advanced panel) | **C** -> not shown | `WW3D::Set_MSAA_Mode` configures the DX8 device; the D3D11 backend has no MSAA path. It was already not on any Modern page; it stays hidden. **F**: needs a multisampled D3D11 target. |
| GRAPHICS | Gamma | **C** -> **removed** from Modern Options | `W3DDisplay::setGamma` returns immediately on the native runtime and `D3D11Backend::Set_Gamma` is `Unsupported()`: the slider changed nothing. **F**: a gamma/brightness stage in the D3D11 present path (a change in the guarded `WW3D2/Backend` tree, so a separate reviewed milestone). |
| AUDIO | SFX / Music / Voice volume | **A** | `TheAudio->setVolume` on the shared mixer (native audio). |
| CONTROLS | Alternate Mouse | **A** | Retail handler. |
| CONTROLS | Scroll Speed | **A** | Retail handler. |
| CONTROLS | Draw scroll anchor, Move scroll anchor | **A** (restored) | Retail checkboxes with live handlers that Modern Options had hidden; now on the page. |
| CONTROLS | Keyboard Options button / key rebinding | **E** / **F** | The retail button opens `Menus/KeyboardOptionsMenu.wnd`, which does not exist in the Generals `Window.big` (Zero Hour only). Robust rebinding is a separate subsystem (input mapping, conflicts, persistence, in-match dispatch); a partial one would be fragile. Not shown, deferred. |
| GAMEPLAY | Unit Motion (Original / Smooth) | **B** | Real (`GroundTranslation::setEnabled`), default Original; Smooth is still labelled experimental. Main menu only (labelled in a match). |
| GAMEPLAY | Interface (Original / Enhanced) | **A** (live everywhere, also in a match) | Enhanced for USA, China and GLA (CP10). In a running match Accept switches the HUD in place (section 10); from the main menu it applies to the next match. |
| GAMEPLAY | Language filter | **E** - not offered | v1 restored it, and native testing showed `MISSING: 'GUILanguageFilter'`: the checkbox's token `GUI:LanguageFilter` is Zero Hour vocabulary and does not exist in the Generals string file, and nothing in Generals reads `m_languageFilterPref` (the uses are commented out). A control with no label and no effect is removed from the page. |
| GAMEPLAY | Use camera / Save camera ("Use saved camera in replays" / "Save camera in replays") | **A** (restored) | Retail handlers and Generals tokens; had been hidden by the Modern pages. |
| NETWORK | LAN IP, online IP, firewall refresh / port override, HTTP proxy, send delay | **B** / **F** | The retail block moved whole; functional where networking exists, disabled in a match and online (retail rule). Re-flow and online modernization are explicitly out of scope. |
| footer | Defaults / Accept / Back | **A** | Defaults never touches display settings. |

Unexplained grey controls remaining in the main menu Options: **none**. In a match the controls that cannot change are labelled
"(main menu)" or are the retail network/detail rules.

## 2. Findings that changed the code

1. **Display size was not updated by a mode/monitor change.** `ModernDisplayOptions::apply` called the window hook for a switch to
   borderless/another monitor, but only `W3DDisplay::setDisplayMode` (the resolution path) told `Display` its new size. A desktop of another
   size therefore left `TheDisplay` width/height, the mouse limits and the tactical view proportions stale. `NativeApplyDisplayMode`
   now updates the logical display whenever the client size changes.
2. **No confirmation for mode/monitor/scale changes**, and the retail resolution dialog had no timeout (its timer is commented out) and
   the new resolution was persisted before the player confirmed. Replaced by one transaction (section 5).
3. **Monitor identity was an index.** Replaced by the Windows device name (index kept as the fallback for older files).
4. **Command-line display flags were ignored** in the Modern path and undefined with the stored values. Defined in section 6.
5. **Hidden retail options** (language filter, camera, scroll anchors) restored; the dead gamma slider removed.

## 3. Resolution list

`DisplayConfig::sizeChoices`: windowed = standard sizes of the 4:3, 5:4, 16:10, 16:9, 21:9 and 32:9 families (800x600 ... 7680x4320, e.g.
1920x1080, 1920x1200, 2560x1440, 3440x1440, 5120x1440) plus every size the driver reports for that monitor, filtered by the monitor's work
area minus the window frame, the current size always included, de-duplicated and sorted; labels carry the aspect family
("3440 x 1440  (21:9)"). Borderless = the desktop size only (no fake modes). The list is rebuilt when the Mode or Monitor selection changes,
before Apply.

## 4. Display modes (honest naming)

* **Windowed** - decorated window, client = back buffer = logical display.
* **Borderless Fullscreen** - `WS_POPUP` exactly on the monitor, flip-model swap chain at the desktop resolution, no display-mode change,
  no exclusive ownership. This is the native "fullscreen" of a D3D11 title: instant alt-tab, no mode switch, DWM-composited.
* **Exclusive fullscreen: not offered.** It needs DXGI mode switching (`SetFullscreenState`), occlusion / focus-loss restore and a recovery
  path that cannot be validated without native evidence on several drivers, and it would be the only mode in which a refresh-rate selector
  means anything. A stored `Exclusive` is reported and run as borderless. Borderless is never labelled exclusive.

## 5. Apply / Keep / Revert (`DisplayTransaction.h`, `ModernDisplayOptions.cpp`)

States: **LAST KNOWN GOOD** (accepted or startup configuration), **PENDING** (applied, awaiting confirmation), **CURRENT** (pending while
a confirmation is open, last known good otherwise). Accept applies size + mode + monitor + UI scale as one change; a dialog (the engine's own
message box) offers Keep / Revert with a 15 s countdown; an unanswered dialog reverts by itself (`Shell::update` -> `ModernDisplayOptions::update`).
An apply that fails restores the last known good; if that fails too the safe fallback (800x600 window on the primary monitor) runs. The risky
fields (size, mode, monitor, UI scale) are written to Options.ini only on Keep; VSync, unit motion and interface are persisted at once.

## 6. Persistence and precedence

Highest first: command line of this launch (`-xres` with `-yres`, `-win`, `-fullscreen` = borderless), stored `Options.ini`, automatic defaults.
Overrides are session-only: `DisplayRuntime::State::stored` is what the file holds, `settings` is what runs. Accept persists only the fields
the player actually changed (`planPersistence`), so an override never leaks into the file. Original (Classic) ignores all of it.

## 7. Resource lifetime for a live change (smallest correct operation)

`D3D11Backend::Resize` releases only the back-buffer views and depth buffer and calls `IDXGISwapChain::ResizeBuffers`; the device, context,
textures, buffers, fonts and the UI texture caches are untouched (D3D11 has no DX8 device-loss). Then `Render2DClass::Set_Screen_Resolution`,
`Display::setDisplayMode` (width/height, 2D range, mouse limits, tactical view proportions), `DisplayRuntime::setClient` (UI layout),
`TheHeaderTemplateManager`/`TheMouse` resolution hooks, `Shell::recreateWindowLayouts`, `InGameUI::recreateControlBar` (Original or Enhanced
HUD, popups). Radar and world targets are not tied to the back buffer size. A change is refused (and rolled back) while a scene is open.
**In-match live changes are not offered**: they would also have to rebuild the in-game view, selection and the control bar mid-simulation;
that needs native evidence and is deferred (the retail game also locks resolution in a match).

## 8. Deferred

Exclusive fullscreen and a refresh-rate selector; in-match display changes; gamma/brightness; anti-aliasing; key rebinding; Network page
re-flow / online modernization.

## 9. v2 native findings (x64-37-modern-settings-v1) and the six-tab audit

| finding | root cause | fix |
|---|---|---|
| **Interface: "Enhanced" could not be selected** | v1 restored four retail checkboxes (camera, scroll anchors) that are children of the Options root, while the page's dropdowns live in the host panel. The Interface dropdown is the last row, so its list opens over the rows below - and a sibling window of the host panel that overlaps an open dropdown takes its clicks (`GameWindow::winPointInChild` returns the first child whose rectangle contains the point; `winBringToTop` only orders siblings). Before v1 nothing overlapped the list. The persistence path (`planPersistence` -> `InterfaceStyle`, `EnhancedHud::setRequested`, next match) was verified correct and is now covered by tests. | The restored checkboxes are moved into the host panel of their page (`OptionsPageLayout::kReparented`, `winSetParent`), checkboxes come before dropdowns in every page table, and a test enforces the order. |
| **Advanced Display panel overlapped the tab strip** | The retail panel (485 x 492 reference units) was centred in the 568 unit Modern panel and covered the tab strip and header while the page, tabs and Back / Accept / Defaults stayed visible around it. | It is a sub-screen: positioned below the title rule (`advancedPopupRect`, inside the panel at every scale and surface), the tab strip, the page and the bottom row are hidden while it is open and restored on its Accept / Cancel. Its settings are untouched. |
| **`MISSING: 'GUILanguageFilter'`** | see the Language filter row. Every other token of every control on the Modern pages was checked against the Generals string file. | control removed; a contract test checks all page-control tokens against the installed Generals data (skipped when it is absent) and that runtime captions can only use `FETCH_OR_SUBSTITUTE`. |
| Borderless resolution looked broken | By design the list is only the desktop size, but the combo looked selectable. | Disabled with the reason in its label ("(desktop size)"); windowed stays selectable; in a match the label says "(main menu)". |
| In-match controls | Disabled by two lists in two places. | One table (`kMainMenuOnly`): the controls are disabled and their labels carry "(main menu)". |

Bounded audit of all six tabs (no new features): DISPLAY - selection, rebuild-on-mode-change and gating verified in source; GRAPHICS - only Detail (+ the Advanced sub-screen, now bounded); AUDIO - no dropdowns, unchanged; CONTROLS - checkboxes now share the slider's panel; GAMEPLAY - the dropdown/checkbox overlap above; NETWORK - the retail block moved whole, its tokens exist, disabled in a match by the retail rule. No other missing token, overlapping control or non-persisting setting was found in the source.

## 10. X64.37 v3: Render Scale, live changes in a match, live HUD switching, the compact Global Powers dock

### 10.1 Display resolution vs render resolution
* **Display resolution** = the size of the window client / swap chain = the presentation space. Windowed: selectable (the list of section 3).
  Borderless Fullscreen: the monitor's desktop size, read-only (label "(desktop size)"); nothing pretends the desktop mode changed.
* **Render resolution** = the size the 3D world is drawn at before it is composited into the presentation. It is independent of the display
  resolution and of the UI scale (`RenderScaleSettings.RenderScaleAndUiScaleAreIndependentSystems`): the HUD does not grow when the world is
  rendered smaller, the mouse keeps working in presentation coordinates, text and the 2D interface are never rendered at the lower size.

### 10.2 Render Scale architecture (audit of the D3D11 path)
* The D3D11 backend has exactly two submission kinds: `Submit_Render2D` (all UI, fonts, tooltips, radar, control bar, sentences) and
  `Submit_World` (terrain, roads, bridges, shore, water, objects, particles, decals, shadows, shroud through `NativeWorldObjects` /
  `NativeWorldEffects` / `HeightMap`). Both draw into the swap-chain back buffer; `W3DView::draw` renders the scene (`doRender`) before the
  interface. That is the natural seam: world space vs presentation space.
* Implementation (`D3D11Backend`, X64.37 v3; the pure arithmetic is `WW3D2/Backend/WorldRenderScale.h`): while the scale is below 100 %
  every `Submit_World` draws into a lazily created colour + depth/stencil target of `round(size * percent)` (minimum 64 px, never above the
  presentation size, aspect preserved) with its viewport mapped into it (projection unchanged: same aspect). The first 2D submission,
  the present, the end of the scene and a readback first composite what the world drew into the back buffer with a bilinear full-screen
  triangle, only inside the rectangle the world covered. The intermediate target is cleared with the last clear colour / depth at the start
  of each picture, depth-only clears apply to it too, and a resize drops it (recreated at the new size).
* Not implemented, by decision: DLSS / FSR / XeSS, temporal accumulation or reconstruction, frame generation, sharpening.
* Limits: it is GPU work only (it does not help a CPU-bound simulation). A world draw after a 2D draw in the same picture is composited again
  over the region (the engine draws the world first, then the interface; no interleaving was found in the native path). Visual quality and
  the performance benefit are **not natively verified**.
* Screenshots: `Readback_RGBA8` reads the back buffer after the composite, so a screenshot is the presented picture at display resolution
  with the full-resolution interface (never the low-resolution world target).

### 10.3 Live changes during a match: audit and support matrix
What a display change touches (read from the source): `NativeApplyDisplayMode` (window, swap chain `ResizeBuffers`, `Display::setDisplayMode`:
display size, 2D range, mouse limits, tactical-view proportions) is safe between frames. But the in-game windows are **created at match start
from WND files with the geometry transform of that moment** (`parseScreenRect`): `ControlBar.wnd` (`InGameUI::createControlBar`),
`ReplayControl.wnd`, `GeneralsExpPoints.wnd` and the shortcut bar / tooltip layouts (`ControlBar::init`), `InGamePopupMessage.wnd`.
`InGameUI::recreateControlBar` replaces `ControlBar.wnd` and the `ControlBar` object (`delete TheControlBar; new ControlBar`) but not
ReplayControl or the popup message, and the new object has none of the player-specific initialisation (`setControlBarSchemeByPlayer`,
`initSpecialPowershortcutBar`) that `changeLocalPlayer` performs. Proving that mid-simulation (selection, command context, observer, the
Enhanced HUD statics, the popup, tooltips) needs native evidence the project does not have, so these stay **main menu only**, labelled
"(main menu)":

| setting | in a match | reason |
|---|---|---|
| Render Scale | **live** | backend-internal; no window, layout or UI object changes |
| VSync, Frame Rate Limit | **live** | already live (present flag / frame pacer) |
| Interface (Original / Enhanced) | **live** | scheme re-init of the same player (10.4) |
| Resolution, Display Mode, Monitor, UI Scale | main menu | the in-game windows above keep the old geometry; control-bar recreation mid-match is unproven |
| Unit Motion | main menu | per-object interpolation snapshots start at match load; enabling them mid-match is not audited |
| Detail | main menu | load-time LOD (retail rule) |

### 10.4 Live Original <-> Enhanced HUD switching
`ModernDisplayOptions::apply` -> `refreshHudPresentation()`: `ControlBar::setControlBarSchemeByPlayer(viewed player)` (observer template for
an inactive player). The same-side branch calls `ControlBarScheme::init()`, which runs `EnhancedHud::update()` for the new request, replaces the
scheme placement and then either `applyEnhancedLayout()` (windows remembered, moved into the island, Global Powers re-anchored, popup skinned) or
`restoreOriginalLayout()` (remembered geometry and visibility back, draw functions and images restored); `markUIDirty()` makes the game
re-evaluate its context. Nothing is destroyed or created: selection, command state, cash, power, rank, XP, General Points, purchased
powers, cooldowns, radar, camera, groups, audio and the simulation are untouched (the control bar stage returns to its default). This is the
proven mechanism of observer/player switching. Cancel does not call `apply`, so an unaccepted choice is not applied or persisted; Accept persists
`InterfaceStyle` at once. Tested by mock-window cycles Original -> Enhanced -> Original -> Enhanced for USA, China and GLA (exact retail
geometry and hit regions, deterministic Enhanced geometry, an untouched game-state record). **Not natively verified.**

### 10.5 Compact Global Powers dock (native finding at 800x600)
Cause (read from the code): the retail shortcut bar fills its slots from the left (`populateSpecialPowerShortcut`: `currentButton++`), the
compact row positioned slot k at `k * pitch` of a five-slot row whose right end is the island's right end, and the row was applied only
when the bar was created - before the buttons became visible. Two visible powers therefore hung in the middle of the row, away from the
island. Fix (framework layout untouched, hash-pinned): `EnhancedHudPanels::compactDock` packs the visible plates against the island's right
end (inset by the shoulder), the layout is re-applied after every populate, and the dock draws a mounting rail from the plates to the island's
top edge. The side column on wider surfaces is unchanged. Identical for USA, China and GLA.

## 11. v3 native failure: black world with Render Scale 75 % (recorded; fixed in v4, NOT yet natively verified)

**Native evidence (x64-37-modern-settings-v3, 1920x1200 borderless, 75 % = 1440x900, 120 FPS limit):** the whole 3D world black in a
Skirmish while the Enhanced HUD, radar, Global Powers, selection circle and FPS overlay rendered correctly; after leaving the match the main
menu background (a shell-map world) was black as well, with the 2D menu frame, logo and buttons intact. The Options arithmetic and labels were
correct. Render Scale was **not** natively successful in v3.

**Root cause (from the source):** the composite that resamples the reduced world target into the back buffer draws one full-screen
triangle generated in the vertex shader, wound clockwise. It bound the backend's shared UI raster state, which is `CULL_BACK` with
`FrontCounterClockwise = TRUE` (the legacy D3D winding the 2D and world paths are authored for). The triangle was therefore back-facing and
**culled completely**: the composite ran, sampled a valid world target, and wrote nothing, so the back buffer kept its clear colour (black)
and everything drawn afterwards at full resolution (UI, radar, overlays) looked fine. The main menu was black for the same reason, not because
of state leaking out of gameplay: the shell map is a real 3D world drawn through the same `drawViews()` / `Submit_World` path, and the
setting was still 75 %.

**Why the automated tests passed:** they covered the arithmetic, the settings and source-text contracts; nothing could observe pixels or
rasterizer state, and no test modelled the winding of a generated triangle against the raster state it was bound with.

**Fix (v4):** the composite owns a dedicated raster state (`CULL_NONE`, `blitRaster`) so it cannot depend on a winding convention; the
per-picture lifecycle of the intermediate target is a pure state machine (`WorldRenderScale::PassTracker`) that the backend delegates to and
the tests exercise; the end of the world phase is now explicit (`D3D11Backend::End_World_Phase`, called by `W3DDisplay::draw` right after
`drawViews()`, via `NativeEndWorldPhase`), with the first 2D submission, end of scene, present and readback kept as defence in depth;
`Begin_Scene` resets the tracker so nothing survives from an earlier picture or scene; 100 % returns before any of it (`FlushWorld` exits at
`worldScale >= 100`, the intermediate target does not exist). First scaled world draw and first composite write a `render_scale` line to the
native log (`world_target`, `composite`) so a retest can prove both happened.

**Native pass order found in the source (`W3DDisplay::draw`):** `WW3D::Begin_Render` (colour + depth clear, `Begin_Scene`) -> `drawViews()`
(all world: `W3DView::draw` -> `doRender`: terrain, roads, bridges, shore, water, objects, particles, decals, shadows, shroud, every one a
`Submit_World`; only `HeightMap.cpp` and `NativeWorldObjects.cpp` call it) -> `TheInGameUI->DRAW()` / `TheGameClient->DRAW()` / mouse / video /
letterbox / debug text (all `Submit_Render2D`) -> `WW3D::End_Render` (`End_Scene` + present). Screenshots read the back buffer afterwards.

**The 20 questions, answered from the source:**

| # | answer |
|---|---|
| 1 | the reduced colour+depth target is bound by `Submit_World` itself (`OMSetRenderTargets(1, &rtv, drawDsv)` per draw), from the first scaled world draw of a picture |
| 2 | every world pass: all 3D goes through `Submit_World` (two producer files), so yes |
| 3 | cleared by the first world draw of a picture with the last `Clear` colour / depth (`PassTracker::fresh`) |
| 4 | a matching D24S8 target, bound with the colour target; depth-only clears are mirrored to it |
| 5 | not unbound explicitly: every later draw (2D `Draw`, the next `Submit_World`) binds its own targets; the composite binds the back buffer |
| 6 | yes: the composite binds the back buffer RTV with no DSV before sampling |
| 7 | the SRV of the same texture that was the RTV, correct |
| 8 | no alias: the RTV is unbound by `OMSetRenderTargets(back buffer)` first, and the SRV slot is cleared afterwards |
| 9 | yes, whenever a scaled world draw happened (counted and logged in v4) |
| 10 | the composite sets its own viewport (the union of the world viewports); no raster state enables the scissor test; every draw sets its viewport |
| 11 | **this was the defect**: raster state. Blend / depth / shaders / IA are set completely by the composite and by every draw |
| 12 | no: the only colour clear is in `Begin_Render`, before the world |
| 13 | no world pass after a 2D submission was found; 2D overlays inside `W3DView::draw` (debug paths) composite first |
| 14 | the menu background is the shell-map world: `Submit_World` through the same path (the 2D frontend windows are `Submit_Render2D`) |
| 15 | no state leaked: the scale persisted and the shell map takes the same path; v4 additionally resets the tracker at every `Begin_Scene` |
| 16 | see 15 |
| 17 | present/end-of-scene composited a valid target; the triangle was culled |
| 18 | the triangle covers the whole clip square (checked by the contract); the viewport limits it to the world rectangle |
| 19 | yes: `SV_Position.xy / presentation size`, target sized by `internalSize` |
| 20 | `EnsureLow` recreates on a size mismatch; `Resize` and a scale change release it and reset the tracker |

Radar, HUD, text, the selection circle and the overlays are `Submit_Render2D` (they stayed correct in v3, which is the evidence), so they are
full-resolution presentation. Overlays computed from the world (`worldToScreen`) use the presentation-space camera viewport and stay attached;
anything drawn through `Submit_World` is inside the scaled world and aligned by construction (same projection, viewport mapped).

**Verified by the harness:** the lifecycle decisions (`WorldRenderScalePass.*`: one clear and one composite per picture, no leak across
Main Menu -> Game -> Main Menu -> Game, abandoned pictures, scale transitions), the arithmetic, and source contracts including the evaluated
winding of the composite triangle against a culling raster state (`test_composite_triangle_is_not_culled`). **Not verified by it:** the actual
D3D11 pixels. That needs the native retest.

## 12. Native result of v4

v4 passed the human native test: Render Scale now renders the world correctly (the section 11 fix was confirmed), settings are usable and
Interface selection works. Settings completion is ACCEPTED AND CLOSED.
