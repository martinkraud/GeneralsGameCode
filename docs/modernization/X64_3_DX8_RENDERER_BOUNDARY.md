# X64.3 - DX8 declarations and native renderer boundary

Investigation: 2026-10-11. Baseline: `923f730fe86e0d57400872eb0e07e69e467439aa`
(`X64.2: make Windows diagnostics and stack unwinding pointer-width safe`).
Branch `dev/modern-engine`, tracking `origin/dev/modern-engine`; HEAD and the
tracking ref matched and the working tree/index were clean before changes.

## Result and limits

The pinned min-dx8 SDK declarations can compile on AMD64 without its I386
binaries. Header availability and backend implementation are separate concerns.
This task changes CMake dependency wiring only, alongside this documentation.
It does not replace math, modify renderer/gameplay source, select a new backend,
or make Win64 playable. The complete game/tool/backend graph remains enabled.
No game launch, Install target, Steam write, runtime replacement or capture
modification is part of this work.

The existing `IRenderBackend` is a useful but incomplete seam. A D3D11 device
behind it would not receive most of the engine's rendering commands. The
recommended first implementation is **RendererBoundary.1: backend-neutral
resource/state submission for the existing Render2D vertical slice**, retaining
DX8 as its working implementation. This is a proposal, not implemented here.

## Header and binary separation

`cmake/dx8.cmake` continues fetching pin
`7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc` from the existing min-dx8 repository.
Its upstream CMake combines headers, `BUILD_WITH_D3D8`, binary libraries and
linker flags in `d3d8lib`:

| Dependency | Classification | Native implication |
|---|---|---|
| `d3d8types.h`, `d3d8caps.h` | A/B: scalar enums, structures, caps, matrices | Real declarations compile; D3D semantics still leak into W3D |
| `d3d8.h` | B/C: COM interfaces and device API | Declarations compile; COM calls require a real implementation |
| `d3dx8math.h/.inl` | A/D: data, inline operators and external functions | Some math links header-only; many operations still require replacement |
| `d3dx8core.h`, `d3dx8tex.h`, umbrella `d3dx8.h` | D: FVF, shader assembly, image/texture/surface/font helpers | Declarations alone do not implement helpers |
| Bundled `d3d8.lib`, `d3dx8.lib`, `dinput8.lib`, `dxguid.lib` | E: all members reported by dumpbin are I386 (`14C`) | Never import these archives or their directory into AMD64 linkage |
| `/SAFESEH:NO`, `/NODEFAULTLIB:libci.lib`, `legacy_stdio_definitions` | E: legacy SDK link arrangement | Preserved on Win32 only; not inherited by native declarations |
| SDK `extra/` directory | E/F: old compiler supplementary headers, including basetsd | Still upstream VC6-only; not added to AMD64 |
| Windows SDK native `dinput8.lib`, `dxguid.lib` | Separate system input/GUID dependencies | Native input/GUID libraries retained explicitly; never use the bundled I386 files |

The separate game/tool CMakeLists still carry their pre-existing
`/NODEFAULTLIB:libci.lib` exclusions (and some `libc.lib` exclusions); these are
not inherited from the native header target, do not select an I386 library, and
were not changed. No `/SAFESEH:NO` occurs in the native link graph.

The bundled archive sizes are respectively 2,466; 2,150,226; 19,978; and
104,752 bytes. X64.3 rechecked all four directly: 158 reported member machine
headers, all `14C (x86)`. The installed Windows SDK native library directory is
`Windows Kits/10/Lib/10.0.26100.0/um/x64`; no d3d8/d3dx8 native library was found
in the X64.1 audit. None has been selected or supplied by this task.

`DX8Wrapper::Init` also loads `D3D8.DLL` and resolves `Direct3DCreate8` dynamically.
Absence of a static import does not remove this runtime requirement. Existing
app-local d3d8.dll identity remains unverified; an x86 shim cannot serve an AMD64
process. D3DX calls are external library calls, not supplied by that loader.
BrowserEngine DLLs and the Bink/Miles loader gates remain separate native risks.

Retained wiring:

* `dx8_headers`: an INTERFACE target with the original SDK include directory
  only. No libraries, link directories, linker flags or backend definition.
* `legacy_dx8_dependencies`: the existing consumers' build contract. Win32
  forwards to the **unchanged upstream `d3d8lib`**. Win64 forwards to
  `dx8_headers`, retains unrelated `dinput8`/`dxguid` linkage through the native
  Windows SDK, and defines `BUILD_WITH_D3D8` to keep real legacy code enabled.
  This name describes legacy compile dependencies, not a native backend.
* Win64 FetchContent uses an intentionally absent `SOURCE_SUBDIR`, so the
  upstream binary-link CMakeLists is never executed. The pin is unchanged and
  no fetched SDK file is edited or copied into tracked source.
* Five consumers change their dependency name: WWVegas common, GameEngine,
  both W3DView tools and VC6-only max2w3d. Their Win32 usage requirements remain
  identical. No renderer source is excluded or replaced with a stub.

Local native configuration reuses `build/win32/_deps/dx8-src` as a **source/header**
override, not an object/library input. Fresh configurations can fetch the same
pin. This is use of the existing repository dependency; no new SDK distribution
or legal endorsement is asserted.

## Actual renderer call graph and leakage

```text
GameClient / terrain / water / shadows / draw modules / GUI
  Display + View interfaces
    W3DDisplay / W3DView / title RTS scenes
      WW3D / mesh / sorting / ShaderClass / material / texture / Render2D
        Get_Render_Backend() -> IRenderBackend -> DX8Backend -> DX8Wrapper
        --------------------------------------------------------------
        direct DX8Wrapper state/resource/draw calls ------------------+
        DX8CALL + _Get_D3D_Device8() + COM resource methods ------------+
                                                                    |
                                             real IDirect3DDevice8 / D3DX8
```

`Backend/DX8Backend.cpp::Create_Render_Backend` always selects DX8Backend.
Its Create/destructor own Init/Shutdown and forward the small interface set.
`IRenderBackend.h` contains no SDK include or SDK type in its signatures:
Begin/End/Flip/Clear, neutral viewport, gamma, cache invalidation, ambient and
LightEnvironment. The title lightenvironment.h data is engine vectors/scalars,
with no D3D declarations. An isolated AMD64 compile of IRenderBackend.h and
Backend/RenderBackend.h succeeds without any SDK include directory.
`RenderBackendViewport` is genuinely neutral and translated
to D3DVIEWPORT8 inside DX8Backend. This seam is sufficient for those callers.
It lacks generic device selection, resource, state, shader and drawing contracts.

Concrete bypasses that a device-only replacement would leave behind:

| Layer / source | Evidence | Classification |
|---|---|---|
| W3DDisplay.cpp | `_Get_D3D_Device8()->TestCooperativeLevel`, direct gamma/caps/backbuffer calls and shared DX8 statistics | B/C |
| W3DView.cpp | direct DX8Wrapper Clear; d3dx8math include without D3DX operation | B plus F for math include |
| Both title camera.cpp | D3DVIEWPORT8, D3DTS_VIEW/PROJECTION and wrapper transform/viewport calls | B/C |
| Both title render2d.cpp | direct VB/IB/texture/material/shader/state setup, texture-stage combiner operations and Draw_Triangles | B/C |
| texture.h / surfaceclass.h / textureloader.h | COM pointers in members, getters, setters, loader payloads and surface access | B/C |
| dx8vertexbuffer.h / dx8indexbuffer.h / dx8fvf.h | public COM buffer getters, FVF layout and lock/allocation assumptions | B/C/D |
| dx8wrapper.h::RenderStateStruct | D3DLIGHT8, D3DMATRIX, W3D resources and mutable cached state | B/C |
| Both vertmaterial.h/.cpp and mapper/matrixmapper/shader.cpp | D3DMATERIAL8, D3DMCS/D3DTSS/D3DRS values and fixed-function state translation | B/C |
| W3DShaderManager, water, terrain, snow, shadows | raw devices, shader DWORD handles/register constants, render targets and direct resource locks | B/C/D |
| W3DScreenshot.cpp | backbuffer -> surface copy -> Lock/read pixels | B/C |
| WWMath matrix3d.h/matrix4.h | public D3DMATRIX/D3DXMATRIX conversion declarations | A/B |
| Common BezierSegment/BezFwdIterator | D3DXMATRIX public basis and external D3DXVec4Transform calls | A/D; not confined to rendering |
| WorldBuilder view/previews and W3DView | extra swap chains, raw device/surface paths, font helper; tool behavior also needs preservation | B/C/D |

An exhaustive tracked-source inventory appears below. It includes conditional
and tool code; it does not claim every lexical candidate executes in Release.
Transitive includes mean additional ordinary W3D consumers receive DX8 headers
even without spelling SDK symbols. Core WWVegas and GameEngine propagate the
dependency broadly through their common interfaces; the PCH compounds this.

## D3DX math and helper audit

Do not equate including d3dx8math.h with requiring d3dx8.lib, or assume every
inline operator is self-contained. In particular D3DXMATRIX `operator*` and
`operator*=` call external D3DXMatrixMultiply from the inline file.

| Operation / actual consumers | Existing engine equivalent | Required disposition |
|---|---|---|
| D3DXVECTOR3/4 and D3DXMATRIX data/construction; WWMath To_D3D* converters | Vector3/4, Matrix3D, Matrix4x4 | 1/2: declarations/constructors compile native; preserve conversion conventions |
| D3DXVec4Dot in BezierSegment | Vector4 dot arithmetic | 1/2: currently inline; no external binary needed for dot alone |
| D3DXMatrixIdentity in water | Matrix4x4 identity | 1/2: inline; useful native header-only probe |
| D3DXVec4Transform in BezierSegment and BezFwdIterator | Matrix4x4 vector transformation | 1/3: external helper; a future replacement needs exact operation/order/FP reference proof for common code |
| D3DXVec3Transform and matrix `operator*` in sortingrenderer | engine vector/matrix operations | 1/3/4: external calls; preserve sorting depths and transform convention |
| D3DXMatrixInverse/Scaling/Translation in TerrainTex, W3DShaderManager, water | engine inverse and transform construction | 1/3/4: external; do not substitute by layout cast or assume bit equivalence |
| D3DXMatrixMultiply/Transpose in tree/water and implicit products in terrain/shaders | engine matrix multiply/inverse/explicit conversion helpers | 1/3/4: external; preserve shader register/transposition and projection semantics |
| D3DXMatrixRotationZ in pointgr | Matrix3D rotation/quaternion math | 1/3/4: external helper for particle orientation |
| Quaternion, normalization, interpolation, projection helpers | WWMath quat.h, vector headers, matrix4.h and title camera.cpp | 1: no D3DXQuaternion*, D3DX*Normalize, D3DX*Lerp/Slerp or D3DXMatrixPerspective/Ortho call found in tracked source; do not port unused SDK APIs |
| D3DXGetFVFVertexSize in dx8fvf/HeightMap | existing FVF/layout wrapper | D/3/4: external stride helper; future explicit W3D layouts replace the backend FVF convention |
| D3DXAssembleShader in water/ProfilerFrameCapture; ID3DXBuffer | existing shader manager loads assembled shader resources too | D/3/4: D3D8 assembly/register interface needs explicit modern shader translation |
| D3DXCreateTexture/CubeTexture/VolumeTexture, CreateTextureFromFileExA, FilterTexture, LoadSurfaceFromSurface | textureloader/bitmap/DDS/SurfaceClass code already owns some loading/copy work | D/3/4: preserve formats, mip filtering, palettes, compression and copy behavior; no blanket loader replacement |
| D3DXCreateFont / ID3DXFont in both WorldBuilders | game Render2DSentence is a separate font route | D/3/4: tool-specific resource/text implementation; no silent removal |
| D3DXGetErrorStringA in DX8Wrapper diagnostics | error formatting | D/3/4: backend diagnostic helper, not gameplay math |

Categories 1-4 here are the requested math dispositions; dependency categories
A-F elsewhere are distinct. Matrix3D/Matrix4x4 comments explicitly warn that
engine and D3D matrix conventions differ. Existing conversion functions must
be the reference; raw memcpy/reinterpret replacements are not authorized.
General operation equivalents are not evidence of equal floating-point results.

Real SDK headers plus both original WWMath conversion .cpp units compiled on
x86 and AMD64 using their actual compile definitions/include graph, `/Y-` and
isolated object outputs. AMD64 object machine headers are `8664`. A standalone
declaration/inline probe checks matrix/vector sizes, pointer width, construction,
identity and dot, links without DX8 libraries and returns success on both
architectures. Its AMD64 imports contain only KERNEL32.dll. This proves that
subset; it does not implement or validate the external helpers above.

## Minimum native renderer contract

This is a requirements map from callers, not a universal API proposal. Prefer
existing W3D value/resource classes where their public contracts can be neutral.
Keep backend-only storage/COM pointers behind implementations; do not expose
D3D11 COM objects in their place throughout game code.

| Engine/W3D semantics to preserve | Current route | Backend-owned implementation details |
|---|---|---|
| Device/window lifetime, adapter/mode/capability enumeration, resolution/windowed choice, supported texture/depth/MSAA combinations | W3DDisplay, WW3D, DX8Wrapper Init/Create/Set_Render_Device | DLL/COM creation, D3D8 presentation parameters/behavior flags, native device/swap-chain ownership |
| Begin/end frame, clear, present, viewport, gamma and frame statistics | existing IRenderBackend plus direct wrappers | BeginScene/EndScene, Present/cooperative-level checks, API-specific state cache |
| World/view/projection/texture transforms, camera convention, depth bias | camera.cpp, Render2D, mapper, wrapper | D3DTS selectors, D3DMATRIX, shader constant packing/transposition |
| Vertex layout, immutable/dynamic buffers, stream/index binding, lock/update/discard semantics | DX8 vertex/index classes, dynamic accesses, FVF, sorting renderer | COM buffers, D3DFVF flags, D3DPOOL and D3DLOCK values |
| Texture kinds, formats, mips, CPU upload/copy/readback, addressing/filtering and lifetime | TextureClass, TextureLoader, SurfaceClass, TerrainTex | COM texture/surface types, managed pool policy and D3DX helper calls |
| Material/vertex color, alpha test/blend, depth/stencil/cull/fill/fog, lighting, texture stages/coordinates | ShaderClass, VertexMaterialClass, mapper, terrain, Render2D | D3DRS/D3DTSS enums, fixed-function combiners/T&L; translate actual used combinations to shaders/state objects |
| Render-to-texture, depth attachments, shadow maps, offscreen pass and restore | water, shadows, DX8Wrapper Set_Render_Target[_With_Z] | IDirect3DSurface8/SwapChain8, default depth/backbuffer references |
| Shader variants/constants and draw primitive/range/base offsets | W3DShaderManager, DX8 renderer, sorting, terrain/water | DWORD shader handles, shader assembly/bytecode/register model, Draw* variants |
| UI quads, text, clipping, pretransformed vertices, half-pixel convention and batching | both Render2D, Render2DSentence, W3DDisplay, GUI/tool fonts | D3DFVF_XYZRHW, D3D8 raster conventions and tool D3DX fonts |
| Screenshot/profiler capture and pitch-aware pixel access | W3DScreenshot, W3DProfilerFrameCapture, SurfaceClass | GetFrontBuffer/CopyRects/Lock, API readback resources and synchronization |
| Suspend/resume/resize and explicit recoverable resource ownership | WW3D, WinMain, DX8 cleanup hook, texture/buffer managers | D3D8 lost/reset/TestCooperativeLevel/managed-pool machinery must stay inside DX8; native resize/device failure is a separate policy |

The native implementation must preserve resource refcounts/ownership, draw and
transparent-sort order, image formats/filtering, coordinates, material effects,
shadows/water and UI behavior. It must not alter simulation or FP control to
accommodate rendering. Renderer shader equivalence and visual validation are
separate from x86/x64 authoritative replay/save/network/lockstep qualification.

## Recommended implementation entry point

**RendererBoundary.1 - W3D-owned resources and Render2D submission seam.**

Start with both title `WW3D2/render2d.cpp::Render`, its existing VertexBufferClass /
IndexBufferClass dynamic access objects, TextureClass, ShaderClass and material
inputs. The existing base classes are currently declared inside
`dx8vertexbuffer.h` and `dx8indexbuffer.h`; their public layout/lock contract
must be separated from the concrete DX8 subclass, not merely relabeled.
Introduce only the neutral resource/layout/state submission operations
that this real caller needs, with DX8Backend forwarding to the unchanged
DX8Wrapper implementation. Encapsulate relevant COM buffer/texture storage and
FVF conversions behind that adapter; retain existing W3D classes and lifetimes.
Keep clearly marked legacy access available for unmigrated callers; this slice
is not permission to rewrite every TextureClass consumer or silently drop them.
Use engine matrices/value types and explicit neutral blend/depth/texture state
descriptions for the slice, rather than renaming D3D enums or copying the entire
DX8Wrapper API into IRenderBackend. Preserve viewport, clipping, text and
pretransformed vertex semantics. Inventory and guard the remaining bypasses.

Acceptance: that slice's submission contract contains no SDK types, and its
submission source compiles without an SDK include directory; both titles' reference DX8 rendering path and resource
ownership remain intact; draw/state call order is covered by a reference
adapter test; Win32 full validation passes. Later authorized visual validation
must cover UI/text/images before changing backend selection. Do not delete
unmigrated legacy paths to satisfy an include check.

Then **D3D11.1** can provide a separately validated native device/swap-chain,
clear/present and that minimal resource/draw slice behind the existing backend
factory. It will be an explicit partial rendering target, not a playable-game
claim. Terrain/water/shadows/fixed-function variants and tools require further
real slices. Common Bezier external math has an independent equivalence gate.
General source-width/allocator/audio/browser gates remain necessary regardless
of renderer work. No entry-point implementation begins in X64.3.

## Build progression and remaining x64 risks

The same MSVC AMD64 Release preset/full graph was used before and after,
with four build workers and `-k20`. Counts are error **occurrences**, not unique
root causes. Ninja denominators describe incremental actions/scans, not engine
completion percentages.

| Attempt | DX8 missing-header errors | Other errors | Failed commands / last action |
|---|---:|---|---|
| Accepted X64.2 baseline reproduction | 23: d3d8.h 15, d3dx8math.h 6, d3d8types.h 2 | none before bounded stop | 23; 26/3155; exit 2 |
| First full build with real headers, before input-link correction | 0 | 30 tool ABI occurrences | 20; 4381/4557; exit 2 |
| Final full build, native input/GUID restored | 0 | Same 30 tool ABI occurrences | 20; 69/241 cached incremental actions; exit 2 |

Seven important native archives were actually produced by the first full
header-separated build: WWMath (action 1631), Zero Hour W3D (2138), Generals W3D
(2961), Zero Hour GameEngine (3794), Generals GameEngine (3927), Zero Hour
GameEngineDevice (4057), Generals GameEngineDevice (4289). The real backend was
compiled, not stubbed. Archive creation does not resolve external dependencies.

New compiler families, all **general Windows/MFC tool ABI** defects rather than
SDK header syntax or renderer implementation errors:

| Family | Occurrences | Actual source / reason |
|---|---:|---|
| C2664 dialog callback | 6 | Both GUIEdit Source/WinMain.cpp:357/363 (CallbackEditorDialogProc, GridSettingsDialogProc), Dialog Procedures/ColorDialog.cpp:277; BOOL return is incompatible with native DLGPROC/INT_PTR |
| C2664 multimedia timer callback | 2 | Core/Tools/W3DView/GraphicView.cpp:254, fnTimerCallback uses DWORD context fields instead of native DWORD_PTR signature |
| C2065 GWL_WNDPROC | 8 | Core/Tools/W3DView/ColorBar.cpp:216/239 and ColorPicker.cpp:181/217, compiled for both titles; old window-procedure indexing/access route |
| C2555 DoModal override | 4 | Core/Tools/W3DView/DeviceSelectionDialog.h:44 in two TUs per title; int return differs from native MFC INT_PTR |
| C2440 file-dialog hook | 2 | Core/Tools/W3DView/DirectoryDialog.cpp:91; UINT hook return differs from LPOFNHOOKPROC/UINT_PTR |
| C2440 timer message map | 4 | Zero Hour WorldBuilder src/EditAction.cpp:53, EditCondition.cpp:65, MainFrm.cpp:49, wbview3d.cpp:2159; OnTimer(UINT) differs from native MFC UINT_PTR |
| C2737 message-map initializer cascade | 4 | Same WorldBuilder TUs at lines 51/62/42/2153; secondary to timer signature mismatch |

Shared W3DView code is compiled per title, and some headers fail in multiple
TUs. The bounded run does not establish that later/unvisited tool sources are
clean. These are classified for a separate Windows/tool ABI task; none is
silenced by casts or fixed as unrelated renderer work here.

To distinguish tool compilation from renderer link requirements, both native
**game targets only** were then probed separately. The default full tool graph
remained enabled; this was an additional diagnostic, not component exclusion.
Initially both failed with 21 unique unresolved symbols each: 17 D3DX helpers
plus DirectInput8Create, c_dfDIKeyboard, GUID_SysKeyboard and IID_IDirectInput8A.
The latter four are unrelated input/GUID requirements previously bundled with
D3D8. The final wiring preserves them through installed native Windows SDK
`dinput8`/`dxguid`, without adding the I386 SDK directory or archives.

The final game link probe resolves all four input/GUID symbols. Both game
links now fail on **the same 17 D3DX symbols only** (17 unresolved externals per
executable, 60 printed link errors total: LNK2019 34, LNK2001 24, LNK1120 2).
Repeated object references are not 60 distinct defects:

* D3DXVec4Transform, Vec3Transform;
* D3DXMatrixMultiply, Inverse, Scaling, Translation, Transpose, RotationZ;
* D3DXGetFVFVertexSize, AssembleShader, GetErrorStringA;
* D3DXCreateTexture, CreateCubeTexture, CreateVolumeTexture,
  CreateTextureFromFileExA, FilterTexture, LoadSurfaceFromSurface.

These are the actual math/helper/backend replacement gate, not missing
include paths. D3D8 COM calls compile against native pointer-sized declarations,
but the runtime loader still requires a real native implementation; successful
helper replacement alone would not supply a D3D11 renderer. No native game
executable linked successfully or was launched. No D3DX replacement, native
D3D8 library search or gameplay change was attempted.

Warnings/latent risks remain separate from hard build errors:

| Risk | Evidence / disposition |
|---|---|
| GUI pointer transport | Actual C4311/C4302/C4312 in GadgetTextEntry.h, GadgetListBox/ComboBox and title menu callbacks; IMEManager.cpp:1441 truncates CANDIDATELIST through UnsignedInt. Fix pointer-bearing transport locally in a dedicated task, not gameplay IDs globally |
| Thread HANDLE | Actual WWLib/thread.cpp:95/107/121 warnings converting volatile unsigned long to HANDLE; requires native handle storage/ownership audit |
| Persistence | Actual WWSaveLoad/persistfactory.h:123 pointer-to-uint32 warning. Save writes sizeof(uint32), while Load reads sizeof(T*) in the Load body: native width exposes a format mismatch. Must design stable on-disk pointer tokens/reference remapping and byte fixtures; do not widen the written token |
| BrowserEngine | Actual dx8webbrowser.cpp:196 HWND-to-long warning; device/IDL-long ABI and shipped x86 DLL remain unqualified |
| Count narrowing | Actual C4267 across matrix3d.h, string/share buffers, BitFlags and tool collections; prove bounds per use rather than blanket widening |
| Allocator/Dict layout | X64.1 alignment/header/stride findings remain; no native allocator runtime clearance follows from archive compilation |
| Bink/Miles | Existing x64 loader fallback remains; compiled archives do not demonstrate audio/video playback |
| Compression | X64.1 EAC pointer-subtraction-after-long-casts remains; cached dependency objects need not re-emit the warning in this incremental run |
| Save/replay/network/CRC/FP | Raw layout, padding, pointer order and x86 x87 versus AMD64 FP remain explicit qualification gates. No serialization, protocol, FP flags or 30 TPS behavior changed |

No warning suppression, persisted structure widening, allocator rewrite or
unrelated GUI fix was retained. The native source graph is substantially more
visible, but is not x64 runtime certification.

## Validation and reproduction

All commands below were executed in X64.3; prior-stage results are not substituted.

| Check | Actual result |
|---|---|
| Full Win32 Release default build (both games, tools and tests), no Install | PASS initially (64 incremental actions) and again on final source (47 actions) |
| Win32 CTest | PASS 3/3 |
| Direct g_googletest / z_googletest | PASS 132 each; six pre-existing disabled tests each |
| Focused RetailOpenList / PerformanceProfiler / PathProfiler / PathPhaseProfiler / DeveloperHarness | PASS 58 each title |
| Python performance/reference unittest discovery | PASS 16 |
| PowerShell parser validation | PASS all nine scripts |
| git diff --check | PASS |
| Existing preservation audit | PASS accepted engine/input/UI token checks, 391 prior runtime/evidence file hashes, accepted A.2 capture hashes and empty index |
| Native windows_diagnostics_test CTest | PASS 1/1 target (five tests) |
| Original matrix3d.cpp / matrix4.cpp compile-only probes | PASS both x86 and AMD64; real include/define graph, no fake PCH or renderer stub |
| Real SDK header/inline construction/identity/dot link-and-run probe | PASS x86 and AMD64, no legacy libs; native imports only KERNEL32 |
| IRenderBackend.h / Backend/RenderBackend.h isolated compile | PASS AMD64 with no SDK include directory |
| Win32 compile/link reference comparison | All 7,182 compile commands identical; all LINK_LIBRARIES (18), LINK_FLAGS (48), LINK_PATH (16) entries identical |
| Native generated link graph inspection | Native SDK input/GUID retained; no bundled SDK link directory, d3d8/d3dx8/d3d8lib input, legacy_stdio_definitions or /SAFESEH:NO |

MSVC developer shells use `VsDevCmd.bat -arch=x64 -host_arch=x64` for native and
`-arch=x86 -host_arch=x64` for Win32. Existing native source overrides for
GameSpy/stb/gtest remain; DX8 adds the following source-only override:

```powershell
cmake --preset win64 -DFETCHCONTENT_SOURCE_DIR_DX8=C:/Users/buskr/Documents/ProjectsX/GeneralsGameCode/build/win32/_deps/dx8-src
cmake --build --preset win64 --parallel 4 -- -k20
cmake --build --preset win64 --target z_generals g_generals --parallel 4 -- -k20
```

The initial reproduction used that build command before the override/source
changes. Win32 used `cmake -S . -B build/win32`,
`cmake --build build/win32 --config Release --parallel 4`, and
`ctest --test-dir build/win32 -C Release --output-on-failure`. Direct Google
executables are in `build/win32/Tests/Google/Release`. Python used
`-B -m unittest discover -s scripts/performance -p 'test*.py'`.

All native probes, inventories, logs and generated outputs stay ignored under
`build/x64-3-*`. Main logs: `x64-3-before.log`, `x64-3-after-headers.log`, `x64-3-after.log`,
`x64-3-game-link-before-input.log`, `x64-3-game-link-probe.log`,
`x64-3-win32-validation.log`, `x64-3-win32-final.log`, `x64-3-math-x64.log`, `x64-3-math-x86.log`,
`x64-3-interface-platform.log`, `x64-3-preservation.log`. Test logs use
`x64-3-google-*`, `x64-3-focused-*` and `x64-3-python.log`.
Final source also repeats Win32 configure/build/CTest and preservation checks;
compile and link comparisons remain identical after the native-only input fix.
No captured evidence or runtime candidate was regenerated. No Debug, VC6,
MinGW, vcpkg or native gameplay/replay/multiplayer compatibility claim is made.
MinGW's existing x64 rejection and x86 d3dx8d alias are unchanged.

## Retained file scope

Build configuration only:

* CMakeLists.txt
* cmake/dx8.cmake
* Core/Libraries/Source/WWVegas/CMakeLists.txt
* Core/GameEngine/CMakeLists.txt
* Core/Tools/WW3D/max2w3d/CMakeLists.txt
* Generals/Code/Tools/W3DView/CMakeLists.txt
* GeneralsMD/Code/Tools/W3DView/CMakeLists.txt

Documentation only:

* docs/modernization/X64_3_DX8_RENDERER_BOUNDARY.md
* docs/modernization/X64.md
* docs/modernization/RENDERER.md
* docs/modernization/ROADMAP.md
* docs/modernization/AGENT_HANDOFF.md

No gameplay, simulation, math, renderer implementation, input handler,
pathfinding, timing, persistence or protocol source file is modified. Generated
builds, probes, inventories, runtime copies and performance evidence are excluded.

## Exhaustive source consumer inventory

The scan covers tracked C/C++ headers, implementations and inline files in Core,
Generals and GeneralsMD, with comments removed and conditional/tool code retained.
It found 151 candidates, including 42 files with direct SDK includes. The three
F-only label/name matches below are recorded as exclusions, not renderer users.
Include-only or unused-helper dependencies are marked F alongside any real use.
A = math/type; B = public/API leakage or direct wrapper consumer; C = actual
backend/device/resource work; D = D3DX helper; E = binary (listed above);
F = unnecessary/transitive/lexical-only dependency. Tags can overlap.

| Tracked source (repository-relative) | SDK include(s) | Class / evidence |
|---|---|---|
| `Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DShaderManager.h` | transitive | B/C: IDirect3DSurface8 / IDirect3DTexture8 |
| `Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DSnow.h` | transitive | B/C: IDirect3DVertexBuffer8 |
| `Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DWater.h` | transitive | B/C: LPDIRECT3DDEVICE8 / LPDIRECT3DINDEXBUFFER8 / LPDIRECT3DTEXTURE8 |
| `Core/GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp` | `d3dx8core.h` | B/C/F: D3DCOLORWRITEENABLE_ALPHA / D3DCOLORWRITEENABLE_BLUE / D3DCOLORWRITEENABLE_GREEN |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/CameraShakeSystem.cpp` | `d3dx8core.h` | F: include only |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/FlatHeightMap.cpp` | `d3dx8core.h` | B/C/F: D3DCOLORWRITEENABLE_BLUE / D3DCOLORWRITEENABLE_GREEN / D3DCOLORWRITEENABLE_RED |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp` | `d3dx8core.h` | B/C/D: D3DXGetFVFVertexSize, wrapper/device rendering |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/TerrainTex.cpp` | `d3dx8tex.h` | B/C/D: D3DXFilterTexture, D3DXMatrixInverse, D3DXMatrixScaling, D3DXMatrixTranslation, wrapper/device rendering |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDebugIcons.cpp` | transitive | B/C: D3DTS_WORLD |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp` | transitive | B/C: D3D_OK |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DMouse.cpp` | transitive | B/C: D3DCURSOR_IMMEDIATE_UPDATE / LPDIRECT3DDEVICE8 |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DProfilerFrameCapture.cpp` | `d3dx8core.h` | B/C/D: D3DXAssembleShader, wrapper/device rendering |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DScorch.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DScreenshot.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp` | `d3dx8tex.h` | B/C/D: D3DXMatrixInverse, D3DXMatrixScaling, D3DXMatrixTranslation, wrapper/device rendering |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DSmudge.cpp` | transitive | B/C: D3DFVF_DIFFUSE / D3DFVF_TEX1 / D3DFVF_XYZRHW |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DSnow.cpp` | transitive | B/C: D3DFVF_POINTVERTEX / D3DFVF_XYZ / D3DLOCK_DISCARD |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainBackground.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainTracks.cpp` | transitive | B/C: D3DTS_WORLD |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp` | `d3dx8tex.h` | B/C/D: D3DXFilterTexture, D3DXMatrixMultiply, D3DXMatrixTranspose, wrapper/device rendering |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp` | `d3dx8math.h` | B/C/F: DX8Wrapper consumer |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp` | `d3dx8math.h` | B/C/D: D3DXAssembleShader, D3DXMatrixIdentity, D3DXMatrixInverse, D3DXMatrixMultiply, D3DXMatrixScaling, D3DXMatrixTranslation, D3DXMatrixTranspose, wrapper/device rendering |
| `Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWaterTracks.cpp` | transitive | B/C: D3DCMP_EQUAL / D3DCMP_LESSEQUAL / D3DLOCK_DISCARD |
| `Core/GameEngine/Include/Common/BezierSegment.h` | `d3dx8math.h` | A: public D3DXMATRIX basis |
| `Core/GameEngine/Source/Common/Bezier/BezFwdIterator.cpp` | transitive | A/D: D3DXVec4Transform |
| `Core/GameEngine/Source/Common/Bezier/BezierSegment.cpp` | `d3dx8math.h` | A/D: D3DXVec4Dot, D3DXVec4Transform |
| `Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.cpp` | transitive | B/C: D3DVIEWPORT8 |
| `Core/Libraries/Source/WWVegas/WW3D2/colorspace.h` | transitive | B: DX8Wrapper consumer |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8caps.cpp` | transitive | B/C: D3DADAPTER_IDENTIFIER8 / D3DCAPS2_FULLSCREENGAMMA / D3DCAPS8 |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8caps.h` | `d3d8.h` | B/C: D3DADAPTER_IDENTIFIER8 / D3DCAPS8 / D3DDevice |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8fvf.cpp` | `d3dx8core.h` | B/C/D: D3DXGetFVFVertexSize |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8fvf.h` | `d3d8.h` | B: D3DDP_MAXTEXCOORD / D3DFVF_DIFFUSE / D3DFVF_NORMAL |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8indexbuffer.cpp` | transitive | B/C: D3DFMT_INDEX16 / D3DLOCK_DISCARD / D3DLOCK_NOOVERWRITE |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8indexbuffer.h` | transitive | B/C: IDirect3DIndexBuffer8 |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8polygonrenderer.h` | transitive | B: DX8Wrapper consumer |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8renderer.cpp` | transitive | B/C: D3DBLEND_DESTCOLOR / D3DFVF_DIFFUSE / D3DFVF_NORMAL |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8texman.h` | transitive | B: D3DPOOL_DEFAULT |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.cpp` | `d3dx8core.h` | B/C/F: D3DFVF_DIFFUSE / D3DFVF_NORMAL / D3DFVF_TEX1 |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.h` | transitive | B/C: D3DFVF_DIFFUSE / D3DFVF_NORMAL / D3DFVF_TEX2 |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8webbrowser.cpp` | transitive | B/C: D3DRender / D3DUpdate |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8webbrowser.h` | `d3d8.h` | B/C: IDirect3DDevice8 |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp` | `d3dx8core.h` | B/C/D: D3DXCreateCubeTexture, D3DXCreateTexture, D3DXCreateTextureFromFileExA, D3DXCreateVolumeTexture, D3DXFilterTexture, D3DXGetErrorStringA, D3DXLoadSurfaceFromSurface, wrapper/device rendering |
| `Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.h` | `d3d8.h` | B/C: D3DADAPTER_IDENTIFIER8 / D3DCOLOR / D3DCOLOR_COLORVALUE |
| `Core/Libraries/Source/WWVegas/WW3D2/dynamesh.cpp` | transitive | B/C: D3DTS_WORLD |
| `Core/Libraries/Source/WWVegas/WW3D2/dynamesh.h` | transitive | B: DX8Wrapper consumer |
| `Core/Libraries/Source/WWVegas/WW3D2/formconv.cpp` | transitive | B/C: D3DFMT_A1R5G5B5 / D3DFMT_A4L4 / D3DFMT_A4R4G4B4 |
| `Core/Libraries/Source/WWVegas/WW3D2/formconv.h` | `d3d8.h` | B: D3DFORMAT / D3DFormat_To_WW3DFormat / D3DFormat_To_WW3DZFormat |
| `Core/Libraries/Source/WWVegas/WW3D2/line3d.cpp` | transitive | B/C: D3DTS_WORLD |
| `Core/Libraries/Source/WWVegas/WW3D2/matpass.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/Libraries/Source/WWVegas/WW3D2/missingtexture.cpp` | `d3dx8core.h` | B/C/D: D3DXLoadSurfaceFromSurface, wrapper/device rendering |
| `Core/Libraries/Source/WWVegas/WW3D2/missingtexture.h` | transitive | B/C: IDirect3DSurface8 / IDirect3DTexture8 |
| `Core/Libraries/Source/WWVegas/WW3D2/pointgr.cpp` | `d3dx8math.h` | B/C/D: D3DXMatrixRotationZ, wrapper/device rendering |
| `Core/Libraries/Source/WWVegas/WW3D2/rddesc.h` | `d3d8caps.h`, `d3d8types.h` | B: D3DADAPTER_IDENTIFIER8 / D3DCAPS8 |
| `Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/Libraries/Source/WWVegas/WW3D2/ringobj.cpp` | transitive | B/C: D3DTS_WORLD |
| `Core/Libraries/Source/WWVegas/WW3D2/seglinerenderer.cpp` | transitive | B/C: D3DTS_VIEW / D3DTS_WORLD |
| `Core/Libraries/Source/WWVegas/WW3D2/shattersystem.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/Libraries/Source/WWVegas/WW3D2/sortingrenderer.cpp` | `d3d8.h`, `d3dx8math.h` | B/C/D: D3DXVec3Transform, wrapper/device rendering |
| `Core/Libraries/Source/WWVegas/WW3D2/sphereobj.cpp` | transitive | B/C: D3DTS_VIEW / D3DTS_WORLD |
| `Core/Libraries/Source/WWVegas/WW3D2/statistics.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/Libraries/Source/WWVegas/WW3D2/streakRender.cpp` | transitive | B/C: D3DTS_VIEW / D3DTS_WORLD |
| `Core/Libraries/Source/WWVegas/WW3D2/surfaceclass.cpp` | `d3dx8.h` | B/C/D: D3DXLoadSurfaceFromSurface, wrapper/device rendering |
| `Core/Libraries/Source/WWVegas/WW3D2/surfaceclass.h` | transitive | B/C: D3DSurface / IDirect3DSurface8 |
| `Core/Libraries/Source/WWVegas/WW3D2/texproject.cpp` | transitive | B/C: IDirect3DSurface8 |
| `Core/Libraries/Source/WWVegas/WW3D2/texture.cpp` | `d3d8.h`, `d3dx8core.h` | B/C/F: D3DFormat_To_WW3DFormat / D3DFormat_To_WW3DZFormat / D3DPOOL |
| `Core/Libraries/Source/WWVegas/WW3D2/texture.h` | transitive | B/C: D3DTexture / IDirect3DBaseTexture8 / IDirect3DCubeTexture8 |
| `Core/Libraries/Source/WWVegas/WW3D2/texturefilter.cpp` | transitive | B/C: D3DCAPS8 / D3DPTFILTERCAPS_MAGFANISOTROPIC / D3DPTFILTERCAPS_MAGFLINEAR |
| `Core/Libraries/Source/WWVegas/WW3D2/textureloader.cpp` | `d3dx8tex.h` | B/C/F: D3DCAPS8 / D3DCUBEMAP_FACES / D3DLOCKED_BOX |
| `Core/Libraries/Source/WWVegas/WW3D2/textureloader.h` | transitive | B/C: D3DTexture / IDirect3DBaseTexture8 / IDirect3DCubeTexture8 |
| `Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp` | transitive | B/C: D3DERR_DEVICELOST / D3DERR_DEVICENOTRESET / D3DFILL_POINT |
| `Core/Libraries/Source/WWVegas/WW3D2/ww3dformat.cpp` | `d3d8.h` | B/C: DX8Wrapper consumer |
| `Core/Libraries/Source/WWVegas/WWMath/matrix3d.cpp` | `d3d8types.h`, `d3dx8math.h` | A/B: matrix conversion declaration/data; no external D3DX call |
| `Core/Libraries/Source/WWVegas/WWMath/matrix3d.h` | transitive | A/B: matrix conversion declaration/data; no external D3DX call |
| `Core/Libraries/Source/WWVegas/WWMath/matrix4.cpp` | `d3d8types.h`, `d3dx8math.h` | A/B: matrix conversion declaration/data; no external D3DX call |
| `Core/Libraries/Source/WWVegas/WWMath/matrix4.h` | transitive | A/B: matrix conversion declaration/data; no external D3DX call |
| `Core/Tools/W3DView/GammaDialog.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/Tools/W3DView/MainFrm.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Core/Tools/W3DView/ScreenCursor.cpp` | transitive | B/C: DX8Wrapper consumer |
| `GeneralsMD/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DShroud.h` | transitive | B/C: IDirect3DSurface8 |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DBufferManager.cpp` | transitive | B/C: D3DFVF_DIFFUSE / D3DFVF_NORMAL / D3DFVF_TEX1 |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DProjectedShadow.cpp` | `d3dx8math.h` | B/C/F: D3DBLEND_DESTCOLOR / D3DBLEND_INVSRCALPHA / D3DBLEND_SRCALPHA |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DShadow.cpp` | `d3dx8math.h` | F: include only |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DVolumetricShadow.cpp` | `d3dx8math.h` | B/C: D3DBLEND_DESTCOLOR / D3DBLEND_ONE / D3DBLEND_ZERO |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DBibBuffer.cpp` | transitive | B/C: D3DLOCK_DISCARD |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DBridgeBuffer.cpp` | transitive | B/C: D3DLOCK_DISCARD |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp` | transitive | B/C: D3DTS_WORLD |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DRoadBuffer.cpp` | transitive | B/C: D3DLOCK_DISCARD |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DScene.cpp` | transitive | B/C: D3DBLEND_INVSRCALPHA / D3DBLEND_ONE / D3DBLEND_SRCALPHA |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DShroud.cpp` | transitive | B/C: D3DLOCKED_RECT / D3DLOCK_NO_DIRTY_UPDATE / D3D_OK |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DStatusCircle.cpp` | transitive | B/C: D3DBLENDOP_ADD / D3DBLENDOP_REVSUBTRACT / D3DBLEND_DESTCOLOR |
| `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DWebBrowser.cpp` | `d3dx8.h` | F: include only |
| `GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp` | transitive | F: D3DSound field / localized D3DFailure* string labels, not an SDK reference |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/assetmgr.cpp` | `d3dx8core.h` | B/C/F: D3DFMT_A1R5G5B5 / D3DFMT_A4L4 / D3DFMT_A4R4G4B4 |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/boxrobj.cpp` | transitive | B/C: D3DTS_WORLD |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/camera.cpp` | transitive | B/C: D3DTS_VIEW / D3DVIEWPORT8 |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/dazzle.cpp` | transitive | B/C: D3DTS_PROJECTION / D3DTS_VIEW / D3DTS_WORLD |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/ddsfile.cpp` | transitive | B/C: D3DFORMAT / D3DFormat_To_WW3DFormat / D3DLOCKED_RECT |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/ddsfile.h` | transitive | B/C: IDirect3DSurface8 / IDirect3DVolume8 |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/decalmsh.cpp` | transitive | B/C: D3DTS_WORLD |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/linegrp.cpp` | transitive | B/C: D3DTS_VIEW / D3DTS_WORLD |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/mapper.cpp` | transitive | B/C: D3DTRANSFORMSTATETYPE / D3DTSS_BUMPENVMAT00 / D3DTSS_BUMPENVMAT01 |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/matrixmapper.cpp` | transitive | B/C: D3DTRANSFORMSTATETYPE / D3DTSS_TCI_CAMERASPACENORMAL / D3DTSS_TCI_CAMERASPACEPOSITION |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/mesh.cpp` | transitive | B/C: D3DTS_WORLD |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/meshmatdesc.cpp` | transitive | B/C: DX8Wrapper consumer |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/meshmdl.cpp` | transitive | B/C: DX8Wrapper consumer |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/meshmdlio.cpp` | transitive | B/C: DX8Wrapper consumer |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp` | transitive | B/C: D3DRS_TEXTUREFACTOR / D3DTA_ALPHAREPLICATE / D3DTA_CURRENT |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/scene.cpp` | transitive | B/C: D3DFILL_WIREFRAME / D3DRS_FILLMODE / D3DRS_ZBIAS |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/shader.cpp` | transitive | B/C: D3DBLEND / D3DBLEND_DESTCOLOR / D3DBLEND_INVSRCALPHA |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/vertmaterial.cpp` | transitive | B/C: D3DMATERIAL8 / D3DMCS_COLOR1 / D3DMCS_COLOR2 |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/vertmaterial.h` | transitive | B: _D3DMATERIAL8 |
| `GeneralsMD/Code/Tools/WorldBuilder/include/wbview3d.h` | transitive | B/D: ID3DXFont |
| `GeneralsMD/Code/Tools/WorldBuilder/src/DrawObject.cpp` | transitive | B/C: D3DCULL_NONE / D3DFILL_SOLID / D3DFILL_WIREFRAME |
| `GeneralsMD/Code/Tools/WorldBuilder/src/ObjectPreview.cpp` | transitive | B/C: D3DLOCKED_RECT / D3DLOCK_READONLY / D3DSURFACE_DESC |
| `GeneralsMD/Code/Tools/WorldBuilder/src/wbview3d.cpp` | `d3dx8.h` | B/C/D: D3DXCreateFont, wrapper/device rendering |
| `Generals/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DShroud.h` | transitive | B/C: IDirect3DSurface8 |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DBufferManager.cpp` | transitive | B/C: D3DFVF_DIFFUSE / D3DFVF_NORMAL / D3DFVF_TEX1 |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DProjectedShadow.cpp` | `d3dx8math.h` | B/C/F: D3DBLEND_DESTCOLOR / D3DBLEND_INVSRCALPHA / D3DBLEND_SRCALPHA |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DShadow.cpp` | `d3dx8math.h` | F: include only |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DVolumetricShadow.cpp` | `d3dx8math.h` | B/C: D3DBLEND_DESTCOLOR / D3DBLEND_ONE / D3DBLEND_ZERO |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DBibBuffer.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DBridgeBuffer.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp` | transitive | B/C: D3DTS_WORLD |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DRoadBuffer.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DScene.cpp` | transitive | B/C: D3DBLEND_INVSRCALPHA / D3DBLEND_ONE / D3DBLEND_SRCALPHA |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DShroud.cpp` | transitive | B/C: D3DLOCKED_RECT / D3DLOCK_NO_DIRTY_UPDATE / D3D_OK |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DStatusCircle.cpp` | transitive | B/C: D3DBLENDOP_ADD / D3DBLENDOP_REVSUBTRACT / D3DBLEND_DESTCOLOR |
| `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DWebBrowser.cpp` | `d3dx8.h` | F: include only |
| `Generals/Code/GameEngine/Source/Common/GameEngine.cpp` | transitive | F: D3DSound field / localized D3DFailure* string labels, not an SDK reference |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/assetmgr.cpp` | `d3dx8core.h` | B/C/F: D3DFMT_A1R5G5B5 / D3DFMT_A4L4 / D3DFMT_A4R4G4B4 |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/boxrobj.cpp` | transitive | B/C: D3DTS_WORLD |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/camera.cpp` | transitive | B/C: D3DTS_PROJECTION / D3DTS_VIEW / D3DVIEWPORT8 |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/dazzle.cpp` | transitive | B/C: D3DTS_PROJECTION / D3DTS_VIEW / D3DTS_WORLD |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/ddsfile.cpp` | transitive | B/C: D3DFORMAT / D3DFormat_To_WW3DFormat / D3DLOCKED_RECT |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/ddsfile.h` | transitive | B/C: IDirect3DSurface8 / IDirect3DVolume8 |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/decalmsh.cpp` | transitive | B/C: D3DTS_WORLD |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/mapper.cpp` | transitive | B/C: D3DTRANSFORMSTATETYPE / D3DTSS_BUMPENVMAT00 / D3DTSS_BUMPENVMAT01 |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/matrixmapper.cpp` | transitive | B/C: D3DTRANSFORMSTATETYPE / D3DTSS_TCI_CAMERASPACENORMAL / D3DTSS_TCI_CAMERASPACEPOSITION |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/mesh.cpp` | transitive | B/C: D3DTS_WORLD |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/meshmatdesc.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/meshmdlio.cpp` | transitive | B/C: DX8Wrapper consumer |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp` | transitive | B/C: D3DRS_TEXTUREFACTOR / D3DTA_ALPHAREPLICATE / D3DTA_CURRENT |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/scene.cpp` | transitive | B/C: D3DFILL_WIREFRAME / D3DRS_FILLMODE / D3DRS_ZBIAS |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/shader.cpp` | transitive | B/C: D3DBLEND / D3DBLEND_DESTCOLOR / D3DBLEND_INVSRCALPHA |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/vertmaterial.cpp` | transitive | B/C: D3DMATERIAL8 / D3DMCS_COLOR1 / D3DMCS_COLOR2 |
| `Generals/Code/Libraries/Source/WWVegas/WW3D2/vertmaterial.h` | transitive | B: _D3DMATERIAL8 |
| `Generals/Code/Tools/WorldBuilder/include/wbview3d.h` | transitive | B/D: ID3DXFont |
| `Generals/Code/Tools/WorldBuilder/src/DrawObject.cpp` | transitive | B/C: D3DCULL_NONE / D3DFILL_SOLID / D3DFILL_WIREFRAME |
| `Generals/Code/Tools/WorldBuilder/src/ObjectPreview.cpp` | transitive | B/C: D3DLOCKED_RECT / D3DLOCK_READONLY / D3DSURFACE_DESC |
| `Generals/Code/Tools/WorldBuilder/src/wbview3d.cpp` | `d3dx8.h` | B/C/D: D3DXCreateFont, wrapper/device rendering |
