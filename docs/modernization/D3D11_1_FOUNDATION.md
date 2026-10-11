# D3D11.1: device/DXGI and sentence Render2D foundation

Implemented and validated 2026-10-11 from clean, synchronized
`564787154a13e996dd73002c9db78c4db7c0cefa` on `dev/modern-engine`, tracking
`origin/dev/modern-engine`. The accepted RB3 validation was reproduced before
editing. No Generals or Zero Hour executable was launched. DX8 remains the
game factory default and functional reference. This is an isolated real D3D11
slice, not a playable D3D11 game or completed x64 port.

## Architecture and ownership

`D3D11Backend` implements the existing `IRenderBackend`; neutral contracts,
Render2D geometry writer, CPU sentence producer and DX8 adapter are unchanged.
Its SDK-free header has a private implementation. All device/context/swap-chain,
native formats/state and COM ownership live in `D3D11Backend.cpp`. The independent
`w3d_d3d11` library links Windows SDK `d3d11`, `dxgi`, `d3dcompiler`, with no
legacy SDK or title PCH in its compile command. Backend allocations use ordinary
C++ ownership; this does not refactor engine memory allocation.

`D3D11TextureBridge.cpp` compiles with both title archives and accesses real
TextureClass CPU content, retained residency and sampler intent. `Create_For_W3D`
requires this backend to be active, stage zero, CPU content, enabled texturing
and normal culling before preparation. Unsupported file assets fail before
legacy loading. The existing TextureClass residency owner controls destruction;
the backend holds a COM reference to the bound SRV rather than a borrowed
residency pointer. Foreign-device residency is rejected. There is no mixed
DX8/D3D11 texture routing or backend hot swap.

The isolated target deliberately uses the neutral CPU-content overload to avoid
linking unrelated legacy loaders/world rendering. It executes actual FontChars
GDI production, CPU content, residency owner, upload, vertex writer, shaders and
indexed draw. The TextureClass/WW3D bridge is compiled and reference-checked,
not exercised end-to-end here. Complete Build_Sentence/layout integration and
in-game visual equivalence remain unverified.

## Device and DXGI lifecycle

Creation requests feature levels 11_0, 10_1, 10_0, using shader model 4_0.
The stage name D3D11.1 does not require feature level 11_1. Hardware is the default;
WARP is selected explicitly for automated validation. Hardware creation failure
is propagated, with no silent software or DX8 fallback. Hardware drivers and
minimum-feature-level hardware were not tested.

A null window creates an RGBA8 offscreen target/RTV. A controlled HWND creates a
windowed, one-buffer, single-sample RGBA8 discard swap chain and back-buffer RTV.
The immediate context handles viewport, clear, indexed draw and Present(0,0).
Hidden/occluded nonfailure Present results are allowed. Positive dimensions are
bounded at 16384; device-specific API limits still apply. There is no aspect-ratio
assumption. Resize outside an active scene unbinds/releases the target, resizes
buffers and recreates the RTV/viewport. Failure throws; device-loss recovery,
fullscreen, Alt-Tab policy, settings and minimized-window handling are deferred.
Destruction clears and flushes context state and releases owned COM resources.

## Geometry, texture and pipeline semantics

| Boundary | Bounded implementation |
|---|---|
| Vertex | Actual `Write_Render2D_Vertices`: XYZ at byte 0, ARGB at 12, UV at 16, 24-byte stride |
| Color | R8G8B8A8 normalized input plus VS `.bgra` restores Windows little-endian ARGB semantics |
| Geometry | Dynamic vertex/index buffers, WRITE_DISCARD, R16_UINT triangle list, DrawIndexed; buffers grow and persist |
| Placement | Existing W3D clip positions plus (+1/viewport width, -1/viewport height), translating the existing DX8 half-pixel bias by +0.5 screen pixels |
| Texture | Pitch-aware immutable A4R4G4B4 CPU snapshot converted to RGBA8; each nibble expands by 17; no premultiplication |
| Residency | One mip, immutable texture/SRV retained across draws; replacement requires a new residency |
| Null/missing | Null texture uses a retained white texel and diffuse color. Missing/non-CPU/file texture is explicitly unsupported; never silently replaced with white |
| Sampler | Explicit NONE/NONE/NONE to point min/mag/mip with MinLOD=MaxLOD=0; clamp U/V/W; unsupported intent rejected |
| Shader | Embedded HLSL compiled by D3DCompile, VS clip transform/color/UV and PS texture times diffuse |
| Blend | Straight SRC_ALPHA / INV_SRC_ALPHA, ADD for RGB and alpha, RGBA writes. Alpha output is As*As + Ad*(1-As), matching legacy separate-alpha-disabled behavior |
| Depth | Disabled, no depth writes or DSV, comparison always; depth clear rejected |
| Raster | Solid, back-face cull, FrontCounterClockwise TRUE to match reference clockwise culling; depth clip on, no MSAA/scissor |

Only the exact default sentence ShaderClass bits are accepted: depth write off,
always comparison, modulate primary gradient, texturing on, fog off, normal
alpha blend and default culling/secondary/detail/NPatch settings. Grayscale,
alpha test, additive/solid variations and other permutations fail closed.
Invalid viewport/index/count/z values also fail before drawing. Each draw binds
its complete supported pipeline explicitly. No fixed-function FVF was added to
the neutral API.

The pixel-center translation follows Microsoft's
[coordinate convention](https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-coordinates).
Explicit [rasterizer winding](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_rasterizer_desc)
and [RGB/alpha blend state](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_render_target_blend_desc)
are checked against actual W3D source and rendered output below.

## Real-device and output proof

`d3d11_sentence_test` runs seven tests on real WARP D3D11 devices, on x86 and
AMD64. No graphics fake satisfies these tests. Binary machine types are I386
and AMD64 respectively; both import D3D11 and the modern shader compiler, with
no D3D8/D3DX8 import. WARP is a real software implementation, not evidence of
hardware GPU speed or driver coverage.

1. All 65,536 A4R4G4B4 values, with padded source pitch, convert exactly.
2. Real Arial GDI glyph content renders into a known 128x96 background. Pixel
   regions verify placement, diffuse color, coverage and straight alpha (within
   two UNORM units). Repeated clear/draw gives identical output and reuses one
   texture and two dynamic buffers; resetting residency creates a second texture
   without recreating buffers.
3. All 16 texture alpha levels render with expected RGB and alpha equations.
   CPU edits do not mutate the uploaded texture; replacement residency uploads
   new content correctly. This avoids assuming desktop GDI antialiasing settings.
4. An offset, nonsquare viewport verifies exact coverage and half-pixel mapping.
5. Unsupported states, foreign residency and invalid submissions reject without
   a draw; offscreen Present and depth clear reject.
6. Null texture preserves diffuse color and reverse winding is culled.
7. A real hidden HWND/swap chain clears/presents, resizes to 160x80, reads back
   the expected color, presents again and destroys the backend before the window.

CPU-byte correctness, native resource creation and actual draw/readback are
proven separately. Hidden Present proves the API/lifecycle route, not visible
desktop presentation. No brittle cross-driver whole-image golden is used.
No manual DX8/D3D11 visual-equivalence claim is made.

## Validation actually run

Environment: VS Community 18, MSVC 19.51/toolset 14.51.36231, Windows SDK
10.0.26100; Release, existing Win32/Win64 CMake graphs. Ignored local logs are
under `build/d3d11-1-*`; baseline logs use `d3d11-1-baseline-*`.

| Check | Final result |
|---|---|
| Full Win32 Release graph | PASS, both games and enabled tools/tests build |
| Win32 CTest | 6/6 PASS, including real D3D11 target |
| Direct title Google tests | 147 PASS per title; six pre-existing disabled tests per title remain disabled |
| Focused renderer/path/profiler/developer tests | 73 PASS per title |
| Independent RB1/RB2 and real RB3 producer | 15 and 5 PASS |
| Real D3D11 tests | 7/7 PASS x86; AMD64 target also passes CTest |
| Native independent CTest | 4/4 PASS: diagnostics, submission, content, D3D11 |
| Rendering Python checks | 21 PASS, including four new reference/boundary checks |
| Performance Python checks | 16 PASS |
| PowerShell parsing | Nine scripts PASS |
| Preservation audit | 391 prior runtime/evidence files and accepted A2 hashes unchanged |
| Diff whitespace | PASS |

The final bounded full Win64 graph and separate native game link probes were
run, not represented as passing. Exact error multisets match the freshly
reproduced baseline: 30 tool C++ ABI errors, 17 unique D3DX helper names in each
game link probe. Aggregate link diagnostics remain LNK2019=34, LNK2001=24,
LNK1120=2. The D3D11 implementation and bridge compile AMD64; its isolated
executable links and runs. No new blocker or naturally removed legacy helper
was observed. Legacy world rendering still needs those helpers; no I386 SDK
binary was substituted into native linkage.

Accepted performance captures, ETL and runtime copies were preserved. No runtime
candidate was prepared, and no game was launched. Build logs, temporary helpers,
binaries and captures remain ignored and are not part of the source change.

## Performance and unsupported scope

There is no performance-improvement claim. Textures, samplers, shaders/states
and growable geometry buffers are reused; no per-triangle resources are created.
Explicit per-draw state binds, constant updates, upload cost and batching remain
future measurement opportunities. Readback allocates a staging texture solely
for bounded validation and is unsuitable for the normal render loop.

Unsupported: world/mesh/material rendering, additional ShaderClass states,
file-backed/missing assets, Font3D, mutable radar/video textures, mip chains,
compression, lighting/ambient/gamma, native surface copy, fullscreen/settings,
device recovery and whole-display integration. DX8 remains the game backend;
no simulation, pacing, pathfinding, save/replay/network or asset policy changed.

## One recommended next task

**D3D11.2: ordinary file-backed UI Texture2D CPU loading for the normal-alpha,
single-mip Render2D image slice.**

Actual caller evidence: `W3DDisplay::drawImage` at lines 2770/2801 requests
`Get_Texture(filename, MIP_LEVELS_1)` and calls `setup2DRenderState` (line 636).
Its normal DRAW_IMAGE_ALPHA case uses the existing normal alpha Render2D path;
other modes/grayscale remain separate. Today the D3D11 bridge rejects that
texture because it lacks CPU backing. The remaining loader crosses native
storage in `TextureLoadTaskClass::Begin_Compressed_Load` (1514),
`Begin_Uncompressed_Load` (1561), `Lock_Surfaces` (1631),
`Load_Compressed_Mipmap` (1677), `Load_Uncompressed_Mipmap` (1714).

Audit and preserve DDS/TGA decoding, format/capability/reduction/compression,
HSV and missing-texture policy while bounding the next executable proof to
ordinary single-mip normal-alpha UI images. This unlocks a real high-fanout
remaining caller without prematurely converting mutable resources, all material
permutations or the whole display. It is a recommendation only, not implemented
in D3D11.1. Pathfinding 2.0 remains backlog; Stage4A.3 remains PAUSED.

## Exact changed-file inventory

- `Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt`
- `Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.h`
- `Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11TextureBridge.cpp`
- `Tests/Google/Dependencies/CMakeLists.txt`
- `Tests/Google/Core/Libraries/D3D11SentenceTest.cpp`
- `scripts/rendering/test_d3d11_reference.py`
- `docs/modernization/D3D11_1_FOUNDATION.md`
- `docs/modernization/RENDERER.md`
- `docs/modernization/X64.md`
- `docs/modernization/ROADMAP.md`
- `docs/modernization/AGENT_HANDOFF.md`
