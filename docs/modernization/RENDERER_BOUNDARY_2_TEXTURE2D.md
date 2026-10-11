# RendererBoundary.2 - Render2D Texture2D boundary

Date: 2026-10-11. Reference: `4911acc4cc52d75a95a3c6702ca29bea385aba12`
(`RendererBoundary.1: isolate W3D Render2D submission from DX8`), clean and
synchronized on `dev/modern-engine` / `origin/dev/modern-engine` before editing.

This stage adds a W3D texture description, mapped content lease, residency owner,
sampler intent and DX8 adapter for the existing Render2D slice. It preserves the
legacy producers and pixel conversion. It does **not** make those producers
independent of a D3D8 device. The one remaining prerequisite is described below.
No game was launched; visual equivalence has not been manually validated.

## Actual producers and reference behavior

All paths below were inspected in source, rather than inferred from terrain or
other unrelated assets. Core WW3D2 paths are relative to
`Core/Libraries/Source/WWVegas/WW3D2/`.

| Producer | Creation/content/upload | Application and updates |
| --- | --- | --- |
| UI images | `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp::drawImage` gets a shared `TextureClass` from each title's `WW3DAssetManager::Get_Texture`, requesting `MIP_LEVELS_1`, or uses an image's raw texture identity. | Render2D retains the logical texture; filename loading still uses the existing TGA/DDS, format/reduction and loader policy. |
| Sentence text | `render2dsentence.cpp::Allocate_New_Surface` allocates square 64/128/256-style A4R4G4B4 surfaces according to existing sizing/hints. `Build_Sentence` locks the surface and `FontChars::Blit_Char` writes glyph bytes using the returned pitch. | `Build_Textures` unlocks, releases its current-surface hold, creates a single-mip A4R4G4B4 texture, obtains level zero, sets clamp U/V and NONE min/mag/mip, copies pending surface content, releases the level, assigns textures to renderers, then releases temporary texture/surface holds. This entire sequence is retained, including `desc.Width` as both texture dimensions. |
| Font3D | `font3d.cpp` loads a file-backed `SurfaceClass`, measures transparency/bounds and crops/copies into A4R4G4B4 surfaces as before. `Font3DDataClass` creates `TextureClass(_surface, MIP_LEVELS_1)` then releases the source surface. | The legacy surface-to-texture constructor and its copy/mip helper remain unchanged. |
| Radar UI | `Core/GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp` allocates terrain/overlay/shroud UI textures using its existing capability-dependent format lists. These are raw UI image identities. | Clears and CPU updates obtain texture levels, lock pitched storage, write existing pixels and unlock/release. Some shroud updates retain a level only for the existing lock/unlock interval. These update routines are unchanged; the backend binds the same underlying storage. This does not migrate world terrain or world shroud rendering. |
| Video UI | `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DVideoBuffer.cpp` creates a single-mip texture in the selected video format. | `lock` obtains level zero and exposes its actual pitch to the decoder; `unlock` unlocks/releases it. No movie-decoder or conversion change. |

Both title Render2D implementations still submit the RendererBoundary.1 packet.
The DX8 command adapter marks regular 2D textures for the new application route
and then makes the **same** `DX8Wrapper::Set_Texture(0, texture)` call. This does
not initialize textures at geometry submission. The wrapper's logical reference
ownership, dirty-state/cache behavior and deferred application remain intact.

### Loading, missing and null

`TextureClass::Init` retains its foreground/background/thumbnail behavior and
access/inactivation policy. The filename constructor may initialize eagerly when
thumbnails are disabled and it is on the loader's DX8 thread. Single-mip first
use otherwise requests the existing foreground load. Background replacement
still arrives through `Apply_New_Surface`.

The shared loader chooses format/dimensions/reduction/mips with the existing DX8
capabilities. DDS and TGA decoding write to prelocked subresources, using
`DDSFileClass::Copy_Level_To_Surface`'s memory overload and
`BitmapHandlerClass::Copy_Image`. Pitch, conversion, alpha, HSV shift, mip
generation, compression fallback and content bytes are unchanged.

`MissingTexture` initialization and its surface/mip helpers are untouched,
including the legacy fill loop. Missing is a valid texture resource; the neutral
description reports it separately from absence of storage. No replacement
checkerboard, zero fill or new fallback policy is introduced.

Null logical binding still reaches `TextureBaseClass::Apply_Null`, which only
binds null and does not alter sampler state. Disabled texturing still initializes
and records a non-null texture, binds null, and applies that texture's sampler.

## Neutral contract

`Texture2D.h` uses W3D `WW3DFormat` and existing `TextureFilterClass` policy enums.
It contains no SDK declarations, native pointers, device filter/address enums,
pool enums or COM interfaces.

* `Texture2DDescription`: actual resident level-zero width/height, actual mip
  count, W3D format, and missing-resource indication. `Describe` returns false
  if there is no storage; it does not force a load. The actual mip count is
  queried from storage, rather than treating the ALL-mips request value as a count.
* `Texture2DRegion`: left/top and exclusive right/bottom, for an existing
  SurfaceClass-style subrectangle lock.
* `Texture2DMapping`: data pointer and signed byte row pitch. Pixel format and
  mip dimensions determine the active row/content span; padding is not assumed
  tightly packed. No format reinterpretation or pitch normalization occurs.
* `Texture2DContent`: synchronous writable mapped-subresource lease. Destroying
  it unlocks and releases the backend surface reference. Bytes written through
  the mapping become visible at unlock, using the existing storage semantics.
  No caller may keep its data pointer after lease destruction or keep a lease
  across replacement/reset. `Map` does not initialize a texture and returns null
  for absent storage or an unavailable mip. Regions must satisfy the original
  format/alignment and bounds requirements.
* `Texture2DResidency`: backend-owned storage implementation, with only neutral
  description/mapping visible to W3D. Its logical identity belongs to the W3D
  texture; a residency reference is borrowed for synchronous use and must not
  survive invalidation/replacement or destruction of that texture.
* `Texture2DResidencyOwner`: creates one residency lazily, retains it on repeated
  use, and resets/destroys it exactly once. It cannot be copied.
* `Texture2DSampler`: min/mag/mip W3D policy plus U/V address intent. DEFAULT and
  BEST remain policy choices, rather than copied device enum values.

The existing producers already write through SurfaceClass; this stage preserves
those operations. The mapped lease provides an explicit content/update route
without silently reading back or copying all texture assets at submission.
Sentence's actual upload now calls `IRenderBackend::Copy_Render2D_Surface` with
the existing W3D source/destination surfaces. The adapter performs the same
whole-surface CopyRects operation. This synchronous borrowed-surface copy does
not retain either surface or alter their acquisition/release order.

## Residency and native ownership

The backend adopts the **existing** native reference on first deferred Render2D
application. There is no extra AddRef, second GPU allocation, duplicate upload
or change to the logical texture reference count. DX8Texture2DResidency owns that
reference; the old TextureBaseClass native pointer is retained as a compatibility
alias for unmigrated consumers of a shared texture. It is not exposed by the new
boundary. Removing every legacy native accessor is outside this slice.

`TextureBaseClass::Release_D3D_Storage` releases either the residency owner or the
original legacy reference, then clears the alias. It never releases both.
The existing destruction, successful invalidation, Set_D3D_Base_Texture,
Load_Locked_Surface and regular TextureClass::Apply_New_Surface release points
use this helper. Loader replacement and loss/reset therefore destroy residency
before accepting new storage. The residency is recreated lazily when needed.
Procedural invalidation guards, active loader-task guards, reset pool tracking,
and native resource release order are retained. The neutral owner is in the
base so it remains alive during the original base destructor's release point.

Cube/volume/Z textures do not enter this new application path. The shared release
helper falls back to their original Release behavior when no residency exists.
Regular textures shared with 3D consumers keep the same underlying native object;
after first Render2D use their regular Apply uses the equivalent new adapter.
Existing surface writes update that same object, so no revision cache or stale
shadow copy is introduced. GPU content remains governed by the original pool
and reset rules. Device-independent producer backing is still a follow-up.

## Application and sampler mapping

`Execute_Texture2D` is the actual deferred application sequence used by the DX8
adapter and recording tests: prepare/lazy Init, LastAccessed update, statistics,
residency, enabled/null binding, sampler. The texturing-enabled query occurs after
preparation. Non-Render2D legacy Apply remains unchanged.

`Execute_Texture2D_Sampler` applies MIN, MAG, MIP, U, V in the original order.
DX8SamplerCommands uses the same `_MinTextureFilters`, `_MagTextureFilters` and
`_MipMapFilters` tables, and maps repeat/clamp to the original wrap/clamp states.
The complete existing filter initialization/capability/aniso/default policy is
unchanged. NONE still means the legacy point min/mag and no mip filter policy;
it is not reinterpreted as a modern API's unrelated numeric enum.

`Backend/DX8Texture2D.cpp` contains the native storage owner, description query,
surface mapping, sampler translation, binding and CopyRects adapter. It requires
the SDK. `texture.cpp` retains unmigrated legacy constructors/loaders/native
accessors and also requires it. `Texture2D.cpp` and the sentence producer do not.
No D3DX helper was replaced. Surface-to-texture/mip operations still require the
legacy creation/filter/load helpers where previously used.

## Tests and validation

| Validation on the retained source | Result |
| --- | --- |
| Baseline Win32 full Release build and CTest | PASS, 4/4 targets |
| Final Win32 full Release graph, both games/tools/tests | PASS |
| Final Win32 CTest | PASS, 4/4 targets |
| Direct Generals / Zero Hour Google tests | PASS, 147 each; six pre-existing disabled tests each |
| Focused profiler/path/developer/Render2D/Texture2D tests | PASS, 73 each title |
| Independent native recording + diagnostics CTest | PASS, 2/2 targets |
| Direct independent recording executable, x86 and AMD64 | PASS, 15 tests each (eight Render2D + seven Texture2D) |
| Python rendering/reference tests | PASS, 13 |
| Python performance-tooling tests | PASS, 16 |
| PowerShell parser | PASS, nine scripts; no launcher execution |
| SDK-free actual-source probes | PASS, five x86 + five AMD64; COFF machine 0x14c / 0x8664 |
| Preservation audit | PASS, 391 prior runtime/evidence files, accepted A.2 capture hashes, original gated engine/input/UI tokens, developer/offline guards; empty index before staging |
| git diff --check | PASS |
| Final bounded Win64 full graph | Expected failure; exact same 30 tool error occurrences as the reproduced baseline |
| Both native game link probes | Expected failure; exact same D3DX symbol/error occurrences as baseline |

The frozen-source Win32 build/CTest log is
`build/renderer-boundary-2-final3-win32.log`; direct/focused title logs have
`renderer-boundary-2-direct-{g,z}` / `renderer-boundary-2-focused-{g,z}` names.
Final native logs are `renderer-boundary-2-final2-win64.log`,
`renderer-boundary-2-final2-link.log` and `renderer-boundary-2-native-tests.log`.
SDK-free `/showIncludes` logs have `renderer-boundary-2-sdkfree-{win32,win64}`
names. Their include traces contain no DX8 SDK headers. These are ignored local
validation outputs, not committed evidence artifacts.

The native build compiles the new adapter and neutral implementation in both
title W3D archives and reaches both game links. No new retained native compile
error is exposed. The same Windows/MFC tool errors remain: six BOOL dialog
callbacks, two DWORD timer callbacks, eight GWL_WNDPROC identifiers, four DoModal
return types, two file-dialog hooks, four OnTimer message-map casts and four
associated C2737 diagnostics. The bounded graph does not certify unvisited tool
paths or native runtime correctness.

Each native game still has these **17** unresolved D3DX helpers:
`D3DXAssembleShader`, `D3DXCreateCubeTexture`, `D3DXCreateTexture`,
`D3DXCreateTextureFromFileExA`, `D3DXCreateVolumeTexture`, `D3DXFilterTexture`,
`D3DXGetErrorStringA`, `D3DXGetFVFVertexSize`, `D3DXLoadSurfaceFromSurface`,
`D3DXMatrixInverse`, `D3DXMatrixMultiply`, `D3DXMatrixRotationZ`,
`D3DXMatrixScaling`, `D3DXMatrixTranslation`, `D3DXMatrixTranspose`,
`D3DXVec3Transform`, `D3DXVec4Transform`. None disappears naturally in this stage.
Together the probes reproduce 34 LNK2019, 24 LNK2001 and two LNK1120 errors.
The bundled libraries remain I386 and were not added to native links. Existing
x64 persistence/pointer/HANDLE, allocator alignment, FP/lockstep and other runtime
qualifications from X64.3 remain unresolved and outside this stage.

The final A.2 capture remains at `build/performance/stage4a2-B-final-2` with its
five reports and command.txt. The synchronized WPR ETL remains
2,469,396,480 bytes. Both remain ignored and unmodified. No runtime was prepared,
installed or launched.

Recording tests exercise the actual neutral application and sampler templates,
and the actual residency owner. They check first/repeated use, initialization,
null/disabled behavior, policy preservation, dimensions/format/mips, pitched
content/alpha bytes, region writes, lease release, missing/invalidated storage,
and exactly-once owner destruction/reset/recreation. Mock content tests establish
the contract; they are not a claim that a GPU lock or CopyRects ran without a
device. RendererBoundary.1 recording tests continue to protect draw ordering.

Static/reference checks compare against local accepted history. They pin all
legacy texture code except the explicit new branch/release hooks; independently
pin the exactly-one-owner release helper; compare sampler calls to legacy Apply;
and compare the complete sentence source after reversing its one copy-call and
include migration. Loader, surface, missing, Font3D, both asset managers and
dynamic UI producer implementations remain token-identical. These checks replace
the old whole-texture.cpp invariant with the narrowly authorized changes, while
retaining every other RendererBoundary.1 reference check.

SDK-free probes compile the actual two title Render2D sources,
Render2DSubmission.cpp, Texture2D.cpp and render2dsentence.cpp on x86 and AMD64.
Only the SDK include path, DX8-containing PCH and its compiler output paths are
removed from real Release compile commands. Original includes, feature defines
and forced BasePrecomp remain; no shim headers or disabled legacy code are used.
Sentence now includes ww3d.h explicitly, rather than depending on dx8wrapper.h
or the PCH to declare WW3D. The independent recording target also has no SDK
dependency. Build/probe/test logs and helper scripts remain ignored under build/.

## D3D11.1 readiness: NO - one bounded prerequisite

The neutral submission, geometry, residency, content mapping and sampler
contracts can support a different backend. Existing content **production** still
creates or locks D3D8 storage before that backend could consume it:

1. Sentence Allocate_New_Surface -> SurfaceClass(width,height,format) ->
   DX8Wrapper::_Create_DX8_Surface; glyph output locks that native surface.
2. Sentence TextureClass(width,height,format) and Font3D's surface constructor
   allocate native textures; file-backed SurfaceClass uses the native immediate
   surface loader before cropping/font processing.
3. UI filename textures -> TextureLoader's 2D begin/lock/load/end operations
   create/lock native textures before the CPU decoder writes to their pitches.
4. Radar/video updates likewise currently lock native levels of native-created
   textures. The new mapped lease preserves this reference behavior; it does
   not turn the backing storage into CPU memory.

Recommend exactly **RendererBoundary.3 - device-independent 2D/font/UI content
backing and loading** before D3D11.1. Bound it to these audited producers: W3D CPU
surfaces/subresources, existing byte-exact DDS/TGA/font/video/radar writes and
mip/format selection, then backend upload/copy/allocation through this boundary.
Preserve DX8 reference copies/format policy and shared legacy interoperability;
do not migrate cube/volume, terrain/water, render targets, screenshots/readback
or all assets. This is one producer-storage prerequisite, not a second sampler
or draw abstraction. Its acceptance test must demonstrate an existing textured
Render2D producer yielding actual bytes without a D3D8 device/resource.

Device, DXGI swap chain, clear/present, viewport, minimal vertex/index resources,
sampler state, shaders/pipeline and indexed draw can then be implemented in
D3D11.1. Claiming that complete existing textured path is ready today would be
incorrect. No D3D11 work is started by this stage.

## Changed-file inventory

* Core/Libraries/Source/WWVegas/WW3D2/Texture2D.h
* Core/Libraries/Source/WWVegas/WW3D2/Texture2D.cpp
* Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Texture2D.cpp
* Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.h
* Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Render2D.cpp
* Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h
* Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt
* Core/Libraries/Source/WWVegas/WW3D2/texture.h
* Core/Libraries/Source/WWVegas/WW3D2/texture.cpp
* Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp
* Tests/Google/Core/Libraries/Texture2DTest.cpp
* Tests/Google/Core/CMakeLists.txt
* Tests/Google/Dependencies/CMakeLists.txt
* scripts/rendering/test_texture2d_reference.py
* scripts/rendering/test_render2d_reference.py
* docs/modernization/RENDERER_BOUNDARY_2_TEXTURE2D.md
* docs/modernization/RENDERER.md
* docs/modernization/X64.md
* docs/modernization/ROADMAP.md
* docs/modernization/AGENT_HANDOFF.md

No capture, ETL, build/runtime output, generated helper or log belongs in this
commit. Gameplay/simulation/pathfinding/timing/save/replay/network behavior,
Stage4A.3's paused status and Pathfinding 2.0 backlog are unchanged.
