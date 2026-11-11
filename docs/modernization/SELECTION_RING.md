# Selection ring - archaeology, Modern appearance and the Selection Ring setting

Status: implemented as part of the Modern Gameplay UI and UX polish closure (owner finding 2, after the build `9ec2a95f2`). Read with [GAMEPLAY_UI_AUDIT.md](GAMEPLAY_UI_AUDIT.md) and [MODERN_SETTINGS_ARCHITECTURE.md](MODERN_SETTINGS_ARCHITECTURE.md).

## 1. Archaeology (before any change)

| Question | Finding |
|---|---|
| What draws the ground circle? | `W3DView::drawNativeWorldOverlays` (`Core/GameEngineDevice/.../W3DView.cpp`, Win64 native renderer). The comment there states the history: the legacy ring was a projected terrain decal (`W3DProjectedShadowManager`), which the native profile does not have; an equivalent circle is drawn with the 2D line path. One implementation serves **Generals and Zero Hour** (shared `Core`). |
| Geometry | A 28-segment polygon on the terrain around the drawable's position, radius = the drawable's bounding-circle radius (minimum 6 world units), each vertex lifted 1 unit above the ground height and projected to the screen (`worldToScreen`), segments drawn with `Display::drawLine`. |
| Thickness | Constant **2.0 px** at every resolution. |
| Colour source | Hard-coded in that function, by relationship of the owner to the local (or observed) player: **own** `(40, 255, 40)` green, **enemy** `(255, 40, 40)` red, **other** (neutral / allied) white. Fully **opaque** (alpha 255). It is not faction colour, not player colour, not an INI value. |
| Infantry / vehicles / structures | The same code. The radius differs only because the bounding circle of a drawable differs (a structure's ring is large; a soldier's is the 6-unit minimum). No type-specific branch. |
| Multi-selection | One ring per selected drawable (`TheInGameUI->getAllSelectedDrawables()`), hidden drawables skipped. |
| Other gameplay uses of circles | None share this code. Radius decals (superweapon / ability radii, `RadiusDecal`), horde / chemsuit terrain decals (`Drawable::setTerrainDecal`), the status circle (`W3DStatusCircle`) and debug circles are separate systems and are untouched. |
| The overhead green bar | `Drawable::drawHealthBar` (and ammo / veterancy / group icons, `drawIconUI`). It is **health feedback**, drawn for selected drawables **and** for mouse-overed ones when `m_showObjectHealth` is set; it is not part of the ring and does not depend on it. |
| Presentation only? | Yes. The function reads the selection list and positions and writes only to the 2D display batch. |
| Can disabling it change picking / selection? | No. Picking and box selection run in `SelectionXlat` / the view's ray and region tests on drawable geometry; selection state lives in `Drawable` / `InGameUI`. The ring is a consumer of that state, never a producer. A contract test asserts that the selection and dispatch sources do not reference the ring. |

## 2. Why it dominates

Pure primary colours at full opacity, a 2 px line that does not follow resolution, and the structure ring at the full bounding circle. On bright desert terrain next to the stepped Modern HUD it is the loudest object on the screen, louder than the unit it marks.

## 3. Modern appearance (conservative, same system)

Rules live in `Common/SelectionRingStyle.h` (pure, engine-free, unit tested):

| | Classic / retail | Modern, Selection Ring On |
|---|---|---|
| Own | `(40, 255, 40)` a 255 | `(120, 214, 128)` a 150 |
| Enemy | `(255, 40, 40)` a 255 | `(228, 104, 96)` a 150 |
| Other | white a 255 | `(226, 226, 226)` a 150 |
| Width | 2.0 px | `1.5 px x (height / 1080)`, clamped to 1.5 .. 3.0 |
| Radius, segments, placement | unchanged | unchanged |

Restrained green (not frontend blue or orange: the ring is gameplay feedback), lower saturation and brightness, translucency and a thinner line. No glow, no new geometry, no different selection system.

## 4. Setting: Options > Modern Experience > Selection Ring

* Key `SelectionRing` (`yes` / `no`) in the same `Options.ini` block as `InterfaceStyle` / `SmoothUnitMotion` (`DisplayConfig::Settings::selectionRing`). **Absent key = On**; any value other than an explicit off is On, so no old file needs resetting.
* One source of truth: the existing `ModernDisplayOptions` page (a dropdown beside Unit Motion and Interface; dropdowns stay last on the page). Same Accept / Cancel / Defaults contract as the other Modern controls: nothing is written before Accept, Cancel discards, **Defaults stages On**.
* Applies on Accept, live (also in a match, replays and LAN lobbies): it is a cached plain flag (`SelectionRing::preference()`) read once per frame; there is no per-frame preference lookup or allocation.
* Help (factual): "Shows a ground indicator around selected units and structures. Turning it off hides only the ring; health bars and the command UI stay. Does not affect gameplay."
* **Off** removes only the ring. The health bar, the selected-object panel, the command UI and every selection mechanic are unchanged.
* **Classic ignores the setting completely**: the retail ring is drawn exactly as before, whatever the stored value says.
* Not in replay metadata, saves, network packets or any authoritative path: the value exists only in the local `Options.ini` and in `SelectionRing::preferenceRef()`.
