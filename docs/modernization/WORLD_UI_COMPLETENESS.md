# X64.30 World/UI Completeness (Generals)

Status: IN PROGRESS (offline batch). Baselines preserved: X64.28 First Playable, X64.29 fix2 (accepted).
Evidence of the accepted baseline: `docs/modernization/evidence/x64-29-gameplay-render-completeness-fix2/`.

## 1. Radar / minimap

### 1.1 Why the radar was empty

`GameEngine.cpp` creates the radar with `createRadar(TheGlobalData->m_headless)`; the native play profile keeps
`m_headless` TRUE (accepted X64.22 architecture), so the factory returned `RadarDummy` (an empty bezel).
Even a real `W3DRadar` could not have worked: its three textures are `TextureClass(w, h, format)` with
`SurfaceClass` Lock/Clear/Draw_Pixel updates, which are DX8 storage (no device in the native build), and its
format selection asks `DX8Wrapper::Get_Current_Caps()`.

### 1.2 Architecture (preserves the original W3DRadar)

The original `W3DRadar` stays the radar: same three textures (terrain, object overlay, shroud), same
refresh rate, same marker/stealth/event/view-box/hero-icon drawing, same `Radar` base class (events,
navigation, `localPixelToRadar`, `objectUnderRadarPixel`, control-bar click handling). Nothing is redesigned.

| Layer | Change |
| --- | --- |
| Factory | `Win32GameEngine::createRadar`: `RadarDummy` unless the play profile (and the `radar` native feature, `GENERALS_NATIVE_DISABLE=radar` falls back to the dummy). Other profiles unchanged. |
| Shared dynamic UI texture residency (D3D11 backend, reusable for video/other mutable UI textures and Zero Hour) | `IRenderBackend::Supports_Dynamic_Render2D_Content()` (false on DX8). `TextureClass(w,h,fmt,MIP_1)` is CPU-backed (`Texture2DCPUContent`, zero-filled) when the backend supports it; `Get_Surface_Level()` returns a CPU-backed `SurfaceClass` (Get_Description/Lock/Unlock/Clear + Draw_Pixel/Draw_H_Line, the calls the radar uses). `Texture2DCPUContent::Revision()` is bumped on every Unlock. `D3D11Backend::Create_Dynamic_Content_Residency` (DEFAULT usage RGBA8) + `Update_Content_Residency` (UpdateSubresource, same conversion as the immutable route: A8R8G8B8/X8R8G8B8/R5G6B5/A4R4G4B4). The texture bridge re-uploads in `Apply_Render2D_Texture` only when the content revision differs from the last upload. Counter: `Dynamic_Uploads()`. |
| Formats | `W3DRadar::findFormat` on the native backend picks from the legacy preference lists skipping R8G8B8 (terrain X8R8G8B8, overlay/shroud A8R8G8B8), what the original selects on any GPU without 24-bit textures. |
| Shroud | `W3DRadar::setShroudLevel` needs the logic cell size where legacy asked `W3DShroud::getCellWidth()`; native play has no `W3DShroud`, so `NativeWorldEffects::shroudCellWorldSize()` (the partition cell size, the same value W3DShroud is initialized with). |
| Pure raster rules | Moved unchanged into `W3DRadarRaster.h` (`shadeForHeight`, `plotMarker`, `stealthAlpha`, `shroudAlpha`, `shroudCellRect`) and used by `W3DRadar`; tests drive the same code. |

Unchanged original behavior: radar availability (`rts::localPlayerHasRadar`, forced/hidden state), shroud/fog
visibility of markers (`canRenderObject`: shrouded status, local-unit-only priority, undetected stealth), team
colors (`RadarObject::getColor`), stealth pulse, event triangles (beacon pulse / generic) and `RadarEvent`
audio, view box with clipped lines, hero reticle icons, terrain texture including water and bridge colors,
click navigation / unit selection through the control bar callbacks.

### 1.3 Offline coverage

`NativeRadarTest` (6 tests, hermetic, WARP): CPU-backed texture and surface lock contract; a deterministic
scene (height-shaded terrain, two markers at known world positions, a shrouded and a fogged logic cell) read
back from the D3D11 target and checked at the exact pixels; upload counting (no upload when unchanged, one per
changed texture, one per shroud batch); raster formulas; shroud rectangle vs `Radar::worldToRadar` over four
map shapes and two cell sizes; aspect-ratio letterboxing. `D3D11UITextureTest.DynamicContent...` covers the
backend route for all four formats and rejects updating immutable residencies.
