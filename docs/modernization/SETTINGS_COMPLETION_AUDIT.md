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
| DISPLAY | Resolution | **A** (main menu), **D** in a match | Real list (see 3): windowed = standard + driver sizes that fit the work area of the chosen monitor, borderless = the desktop size only. Changes the real window and swap chain (`NativeApplyDisplayMode` -> `D3D11Backend::Resize`) inside an Apply/Keep/Revert transaction. In a match the display is kept; the label says "(main menu)". |
| DISPLAY | Display Mode: Windowed / Borderless Fullscreen | **A** (main menu), **D** in a match | Real window style/position/size change inside the transaction. Exclusive fullscreen is **not offered** (section 4). |
| DISPLAY | Monitor | **A** | Real `EnumDisplayMonitors` list ("1 (Primary)  2560 x 1440 @ 144 Hz"), stable identity by Windows device name; the row is shown only when more than one monitor is attached (no greyed row). |
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
| GAMEPLAY | Interface (Original / Enhanced) | **A** | Enhanced for USA, China and GLA (CP10); applies when the next match builds its control bar (labelled "main menu"). |
| GAMEPLAY | Language filter | **A** (restored) | Retail handler; had been hidden by the Modern pages. |
| GAMEPLAY | Use camera / Save camera | **A** (restored) | Retail handlers; had been hidden by the Modern pages. |
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
