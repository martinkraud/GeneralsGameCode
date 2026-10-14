# X64.33 Modern Experience foundation (in progress)

## Architecture
* `Common/DisplayGeometry.h` — single source of truth: physical surface, 800x600 reference canvas, uniform UI
  scale (Auto = quarter steps that fit, or 50–400 %), anchors (edge-keeping), world viewport = whole surface,
  Hor+ field of view. Pure header; covered by `DisplayGeometry.*` gtests at 800x600 … 5120x1440 and odd sizes.
* `Common/DisplayModes.h` — monitor/mode enumeration (EnumDisplayMonitors/EnumDisplaySettingsEx, GPU independent)
  plus pure filtering: >= 800x600, >= 24 bpp, progressive, refresh canonicalized (59->60, 119->120, 143->144,
  164->165 ...), de-duplicated, no invented modes. `DisplayModes.*` gtests.

## Classic
Classic (`-classic`) = presentation defaults of the original experience on AMD64/D3D11: 800x600 reference
presentation at review scale, no modern display options. It is not the old engine.

## Status
See the end-of-batch report; items needing native evidence are listed there.
