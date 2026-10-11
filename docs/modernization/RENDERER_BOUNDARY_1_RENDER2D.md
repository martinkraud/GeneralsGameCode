# RendererBoundary.1 — W3D Render2D submission

Date: 2026-10-11. Starting baseline: `91c1e3e12c31f119354eb8ac6d33cb5013dd66f3`
(`X64.3: separate DX8 declarations from x86 linkage`), clean
`dev/modern-engine`, synchronized with `origin/dev/modern-engine`.

This implements the existing Render2D slice for both titles through the existing
`IRenderBackend`. DX8 remains the only device implementation. No game was
launched; tests establish submission/reference equivalence, not visual output
equivalence. No gameplay, timing, pathfinding, save, replay or network code changed.

## Original path and exact ordering

Both title-specific `WW3D2/render2d.cpp::Render2DClass::Render` implementations
used this sequence directly. The static reference tests read both originals from
the baseline Git commit, rather than deriving an expected result from the adapter.

1. Return when indices are empty or the renderer is hidden.
2. Save view, then projection using `DX8Wrapper::Get_Transform`.
3. Set viewport, depth range `[0,1]`. **Generals** uses `ScreenResolution.Left`,
   `Top`, `Width()` and `Height()`, converted to unsigned coordinates.
   **Zero Hour** uses `(0,0)` and `WW3D::Get_Device_Resolution` width/height.
4. Bind W3D `TextureClass*` to stage 0, including explicit null binding.
5. Obtain `VertexMaterialClass::PRELIT_DIFFUSE`, set material, release local ref.
6. Set world identity, view identity, then projection identity.
7. Construct dynamic VB access with `BUFFER_TYPE_DYNAMIC_DX8`,
   `dynamic_fvf_type` and the legacy unsigned-short vertex count. Lock, write
   position, diffuse and UV0 using FVF offsets/stride, then unlock.
8. Construct dynamic IB access with dynamic DX8 type and unsigned-short count.
   Lock, copy unsigned-short indices unchanged, unlock.
9. Bind VB, then IB with base offset 0.
10. Ordinary drawing: set the caller's entire `ShaderClass` state. Grayscale:
    set `_PresetOpaqueShader`, flush `Apply_Render_State_Changes`, query DOT3,
    then apply the grayscale overrides listed below.
11. `Draw_Triangles(0, index_count / 3, 0, vertex_count)`, preserving the wrapper's
    unsigned-short argument conversions and triangle-list topology.
12. Restore view, then projection. Grayscale additionally invalidates the shader
    cache to reset both stages on the next shader application.
13. Destroy IB access, then VB access. World identity, viewport, texture,
    material and bindings are deliberately **not** restored, matching legacy.

The full dynamic FVF is `XYZ | NORMAL | DIFFUSE | TEX2` (44-byte legacy format),
but this path writes only XYZ, ARGB32 and UV0. Normal and UV1 bytes remain
untouched. These are **not XYZRHW/pretransformed screen vertices**: geometry
generation has already mapped coordinates into clip space and applied the
existing half-pixel bias; identity transforms pass XYZ through. The existing
CPU clipping/text/quad generation is unchanged. No new scissor is introduced.

The original DX8 dependencies specifically in Render() were `D3DVIEWPORT8`,
`D3DTS_VIEW/PROJECTION`, FVF flags/info, dynamic DX8 buffer access, wrapper
state/bind/draw calls, DOT3 caps, `D3DRS_TEXTUREFACTOR`, `D3DTSS_COLORARG0/1/2`,
`D3DTSS_COLOROP`, `D3DTA_*`, and `D3DTOP_*`. These now reside in the adapter;
the title source no longer includes the DX8 wrapper/buffer/FVF/caps headers.

### Grayscale reference

| Mode | Exact retained color overrides, in order |
| --- | --- |
| DOT3 | Texture factor `0x80A5CA8E`; stage 0 arg0 = factor alpha replicated, arg1 = texture, arg2 = factor alpha replicated, op = multiply-add; stage 1 arg1 = current, arg2 = factor, op = DOT3 |
| Fallback | Texture factor `0x60606060`; stage 0 arg1 = texture, arg2 = factor, op = modulate; stage 1 color op = disabled |

All 13 raw override calls/arguments, including the fallback stage-1 disable,
match both original sources. Alpha/depth/blend/cull behavior continues to come
from the same opaque preset or caller ShaderClass. The ordinary default remains
depth-write disabled, depth-compare always, source-alpha/inverse-source-alpha
blending, fog disabled, modulated primary gradient and texturing enabled.
Arbitrary caller shader bits are retained, rather than replaced with that default.
Texture filtering/addressing still comes from the W3D texture's existing Filter.
Fill state is inherited just as before; Render() never set a fill mode.

## New path and neutral contracts

```text
Generals / Zero Hour Render2DClass::Render
  -> IRenderBackend::Submit_Render2D(Render2DSubmission)
     -> DX8Backend::Submit_Render2D
        -> Execute_Render2D<DX8Render2DCommands>
           -> unchanged DynamicVBAccessClass / DynamicIBAccessClass
           -> unchanged DX8Wrapper and W3D material/shader/texture application
```

`Render2DSubmission.h` introduces only the synchronous packet and the actual
slice's command/resource concept. It exposes no SDK enums, SDK structures or COM
pointers. `TextureClass`, `ShaderClass` and `Vector2` are forward-declared W3D
types; `IRenderBackend.h` only forward-declares the submission packet.

| Contract | Semantics |
| --- | --- |
| `Submit_Render2D(const Render2DSubmission&)` | Consume borrowed positions/UVs/colors/uint16 indices and W3D texture/shader identity before returning; no retained packet/array pointers |
| Packet | Original counts, Z, viewport and grayscale flag; original W3D shader carries blend/depth/cull/texturing/detail/alpha/fog semantics without translating them to a new API-specific enum |
| `Render2DVertexMapping` | Write-only range pointer, byte stride, XYZ/diffuse/UV0 offsets; no FVF token in this new contract |
| `Write_Render2D_Vertices` | Actual SDK-free upload source, writes only those three fields, preserves all remaining bytes |
| `Execute_Render2D<Commands>` | Shared executable ordering for save/setup/upload/bind/shader/combiner/triangle draw/restore/invalidate/release |
| `Commands::VertexAccess` / `IndexAccess` | Stack-scoped dynamic range ownership, unsigned-short count; constructors allocate, destructors retire/release the temporary range |
| Nested `WriteLock` / `Map()` | Scoped update lock; map starts at the allocated range, unlock before any binding/draw. Vertex mapping is semantic; index mapping is uint16 |
| Commands | Saved view/projection storage; viewport; stage-0 texture identity; prelit material; identity transforms; ordered resource bindings; caller/opaque shader; state flush; grayscale capability/combiner; indexed triangle draw; restoration/invalidation |

The compile-time command concept keeps concrete dynamic accessors stack-scoped;
there is no per-draw heap allocation or new virtual lock layer. The runtime
backend selection boundary remains the existing `IRenderBackend` object.
Device-specific saved matrices and buffer objects stay inside its adapter.
The two grayscale combiners are W3D-specific operations, not a copy of the entire
DX8 texture-stage API. Required inputs are the same valid array/count combinations
as before; no new clipping, truncation policy, validation or error recovery is
introduced. A shader is required for ordinary submissions. Empty submissions
perform no operations; hidden renderers return at the title entry point.

Viewport scalar construction/device-resolution lookup now happens when building
the packet, before saving transforms. The lookup reads WW3D's stored resolution;
it changes no renderer state. The relative order of device/wrapper operations
above is unchanged. Title viewport choices remain distinct.

## Resource ownership and DX8 mapping

`vertexbuffer.h` and `indexbuffer.h` now declare the existing W3D reference-counted
base resources and their scoped locks outside the concrete DX8 headers.
Declarations, field ordering, types, signatures and implementations are unchanged.
The concrete headers include them for every unmigrated legacy caller.

This is bounded declaration separation, **not** a claim that all old resource APIs
are backend-neutral: `VertexBufferClass` retains the legacy unsigned FVF
constructor and forward-declared `FVFInfoClass& FVF_Info()` accessor. Their code
remains in `dx8vertexbuffer.cpp`. New submission/upload code uses the semantic
mapping instead. Concrete COM getters, FVF conversion, sorting buffers and legacy
type selectors remain available in their original DX8 headers/implementations.

The DX8 adapter in `Backend/DX8Render2D.cpp` maps:

| Neutral operation | Reference implementation |
| --- | --- |
| Save/identity/restore transforms | Same `Matrix4x4`, wrapper view/projection reads, world/view identity, projection identity and view/projection restores |
| Viewport | Field-for-field `RenderBackendViewport` -> `D3DVIEWPORT8` |
| Texture/material | `Set_Texture(0, texture)`; same prelit preset `Get_Preset`, `Set_Material`, local `REF_PTR_RELEASE` |
| Vertex access | Same `DynamicVBAccessClass(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, count)`; same nested write lock; map offsets and stride from its FVFInfo |
| Index access | Same `DynamicIBAccessClass(BUFFER_TYPE_DYNAMIC_DX8, count)` and nested write lock |
| Bind | Same dynamic overloads, preserving wrapper ring offsets, stream-0 state and engine references; index base = 0 |
| Ordinary shader | Same full caller ShaderClass passed unchanged to wrapper |
| Grayscale | Same opaque preset, state flush, cap query and raw overrides, then shader invalidation after transform restoration |
| Draw | Same four-argument `Draw_Triangles` overload and triangle-list draw |

No buffer allocator/lock/destructor implementation changed. VB ring allocation
retains in-use checks, growth/recreation, wrap-to-zero and ref acquisition.
Its lock retains `NOSYSLOCK | (offset == 0 ? DISCARD : NOOVERWRITE)`. IB retains
the corresponding range allocation and `DISCARD/NOOVERWRITE` lock/ref behavior.
Destructors advance the same ring offsets and release the same references.
The wrapper continues to retain cached buffer/texture/material references and
engine refs after the local accessors are destroyed. No eager release/unbind is
added. Normals/UV1 are not initialized as an incidental cleanup.

`TextureClass` stays W3D-owned and reference-counted. The packet borrows its
identity. DX8's existing stage binding obtains its own reference through the
unchanged wrapper. The title renderer continues to own/release its texture in
the original Set_Texture/destructor. No texture lifecycle/backend payload rewrite
is hidden in this migration.

## Tests and SDK independence

`Render2DSubmissionTest.cpp` records the actual shared submission execution:
resource creation/lock/unlock/binding/draw/release order; texture identity/null;
viewport; shader bits including blend/depth and arbitrary nondefault bits;
triangle count/ranges; both grayscale branches/flush/invalidation ordering;
view/projection restoration with world deliberately left at identity; empty
submissions; and exact uploaded bytes with a deliberately different padded layout
to detect assumptions about FVF offsets. The sentinel bytes prove untouched
normal/UV1/padding. Index ordering is checked independently of triangle division.

`scripts/rendering/test_render2d_reference.py` adds six static reference tests
against both real pre-migration implementations in the accepted Git baseline.
It pins all grayscale calls/arguments and branch selection, wrapper mapping order,
viewport field mapping, material ownership, dynamic constructors/FVF offsets,
draw arguments and shader invalidation. It verifies declaration extraction and
that allocator/lock/wrapper/texture/FVF implementations and all non-Render()
geometry, clipping, text, bias and default-shader code are unchanged.
It requires that baseline commit in local Git history; it uses no network.

The standalone `render2d_submission_test` target compiles the real upload source
and recorder using real W3D vector/shader/base-resource declarations. It links
only Utility and GoogleTest. It does not depend on `legacy_dx8_dependencies`,
`core_wwcommon`, the title PCH or the DX8 SDK directory. The same eight tests also
run inside both title Google executables with the real renderer build.

Additional full-source probes compile both complete title `render2d.cpp` files
and `Render2DSubmission.cpp` under x86 and AMD64. Each uses its real Release
compile command with the DX8 include directory, cached PCH and DX8-containing
generated PCH forced include removed; `/Y- /showIncludes` is added and object/PDB
outputs isolated under build. Real BasePrecomp/precompiled support and all
original feature definitions, including `BUILD_WITH_D3D8`, remain. No shim
headers, fake types, source exclusions or alternative implementations are used.
The retained Texture/Surface headers still forward-declare legacy COM types;
this probe proves SDK-directory independence, not removal of every legacy
declaration from the entire transitive W3D graph.

### Actual validation results

| Check run in this task | Result |
| --- | --- |
| Unchanged baseline Win32 configure/full Release build | PASS, 64 build actions; both games/tools/tests |
| Retained-source Win32 full Release build | PASS; both games, both WorldBuilders/W3DViews and remaining enabled tools/tests |
| Final Win32 incremental full build + CTest | PASS, 52 actions; CTest 4/4 |
| Direct `g_googletest` / `z_googletest` | 140/140 each; six pre-existing disabled tests each |
| Focused retail-open-list/performance/path/path-phase/developer/Render2D | 66/66 each title |
| Standalone SDK-independent recording test | 8/8 on x86; AMD64 CTest also passes |
| Python performance/reference suite | 16/16 |
| Python renderer baseline-reference suite | 6/6 |
| Existing PowerShell parser validation | 9/9 scripts |
| Full title-source and upload no-SDK probes | All six compile: three I386 (`0x014c`) and three AMD64 (`0x8664`) COFF objects; include traces contain no DX8/D3DX SDK header/directory |
| Standalone target command inspection | Two Release source entries per architecture, no DX8 SDK include and no title PCH |
| Final bounded full native build | Expected FAIL, 20 failed commands / 30 tool ABI compile errors; no new error family |
| Actual native game link probes, both titles | Expected FAIL, same 17 unique D3DX helpers each |
| Native standalone recording/platform-diagnostic CTest | PASS, 2/2 (eight Render2D and five diagnostics tests) |
| Preservation | PASS, 391 prior runtime/evidence hashes, accepted final A.2 capture hashes, engine/input/UI token audit, bounded developer adapter checks; index empty before staging |
| `git diff --check` | PASS |

Build/test/probe evidence remains ignored under `build/renderer-boundary-1-*`:
`baseline.log`, `win32.log`, `win32-final.log`, `win64.log`, `win64-final.log`,
`native-link.log`, `native-tests.log`, `g-google.log`, `z-google.log`,
`g-focused.log`, `z-focused.log`, `sdk-free-test.log`, `reference.log`,
`python.log`, `powershell.log`, `preservation.log` and both `*-no-sdk.log` files.
The first standalone-target attempt (`sdk-free.log`) correctly exposed a missing
real `rts/profile.h` include root; adding `Core/Libraries/Include` to that
target fixed it. No shim or disabled source was used. The final full builds and
test runs above include that correction.

The first retained native build rebuilt both W3D and GameEngineDevice archives;
the final full build rebuilt both W3D archives again. Both GameEngine archives
remained available from the accepted baseline (their sources did not change).
Both migrated Render2D, neutral upload and DX8 adapter sources compile natively.
The new standalone tests execute on AMD64 without linking any renderer device.

The final full native log's **30 exact tool error locations/messages match
`build/x64-3-after.log`**, not merely its error count:

- Both GUIEdit titles: BOOL dialog callbacks vs INT_PTR DLGPROC (six C2664).
- Both W3DView titles: timer callback DWORD vs DWORD_PTR (two C2664),
  GWL_WNDPROC (eight C2065), int DoModal vs INT_PTR (four C2555),
  UINT file-dialog hook vs UINT_PTR (two C2440).
- Zero Hour WorldBuilder: OnTimer(UINT) vs UINT_PTR (four C2440) and four
  secondary C2737 errors. The bounded attempt does not prove unvisited tools clean.

Both native link probes still require:

```text
D3DXAssembleShader             D3DXCreateCubeTexture
D3DXCreateTexture              D3DXCreateTextureFromFileExA
D3DXCreateVolumeTexture        D3DXFilterTexture
D3DXGetErrorStringA            D3DXGetFVFVertexSize
D3DXLoadSurfaceFromSurface     D3DXMatrixInverse
D3DXMatrixMultiply             D3DXMatrixRotationZ
D3DXMatrixScaling              D3DXMatrixTranslation
D3DXMatrixTranspose            D3DXVec3Transform
D3DXVec4Transform
```

Together the probes emit 34 LNK2019, 24 LNK2001 and two LNK1120 diagnostics,
with 17 unique unresolved helpers per title. Native input/GUID linkage remains
resolved; bundled I386 D3D8/D3DX libraries were not added to the native link.
Full native builds return 2; the link probe command reports -1 and its log
explicitly ends in unresolved-symbol failures. No native game linked or ran.
No new x64 compile failure is exposed by this slice. Existing pointer-width,
size narrowing, persistence token/allocator, browser, audio and FP compatibility
risks remain unqualified; successful archives/tests do not resolve them.

Accepted captures, the synchronized ETL and runtime candidates were neither
regenerated nor copied into the commit. No Install target, runtime deployment,
game launch or visual validation was performed.

## Remaining bypasses, native blockers and D3D11.1 readiness

Everything outside this slice retains its original device access: camera,
mesh/sorting renderer, terrain/water/shadows/snow, direct display/view clear,
cooperative/reset/caps/fill/MSAA, texture/surface creation and upload,
screenshots/readback, shader/material application and tool-specific rendering.
`dx8fvf.cpp` still uses `D3DXGetFVFVertexSize`; matrix/texture/shader helpers and
Common Bezier external D3DX dependencies are not replaced by this task.
See [X64.3](X64_3_DX8_RENDERER_BOUNDARY.md) for the full consumer inventory and
separate persistence/allocator/GUI pointer/browser/audio/FP-lockstep risks.

**Readiness verdict: the submission/dynamic-geometry seam is ready, but the
existing textured Render2D path is not yet sufficient for D3D11.1. Exactly one
additional bounded prerequisite is recommended: RendererBoundary.2 — Render2D
Texture2D residency/upload and sampler boundary.**

The concrete reason is `TextureClass::Apply -> Init/TextureLoader`,
`Peek_D3D_Base_Texture`, `DX8Wrapper::Set_DX8_Texture` and `Filter.Apply`.
Existing W3D texture identity can now cross the neutral boundary, but its
load/update/default/null/residency/filter application still requires a DX8
texture, and some existing Render2D/font producers update SurfaceClass directly.
A new backend cannot obtain/upload those existing UI texture contents simply by
receiving their identity. Drawing only untextured triangles or using dummy
textures would not establish the requested existing Render2D resource slice.

Bound RendererBoundary.2 to the actual 2D/font texture creation/update/first-use
routes: W3D-owned format/dimensions/mip/pitch/content and sampler intent, opaque
backend residency lifetime, null/default texture behavior and loss/reset/update
handling, retaining the current DX8 loader/filter behavior. Audit the concrete
font/surface producers first; do not migrate volume/cube/terrain/readback or
redesign the entire asset system. Require recording/reference upload/sampler
tests, SDK-free source proof, and the same Win32 validation. No D3DX replacements,
tool ABI fixes or D3D11 implementation are authorized by this recommendation.

After that prerequisite, the precise D3D11.1 entry point is a separately selected
implementation created by `Create_Render_Backend`/owned by WW3D, implementing
the existing scene/viewport/clear/present methods and `Submit_Render2D` through
`Execute_Render2D<D3D11Render2DCommands>`. Its stack accessors implement the same
mapping/index contract, and its shaders/pipeline reproduce the W3D ShaderClass,
XYZ/ARGB/UV0 and grayscale semantics. The texture prerequisite supplies actual
W3D UI texture residency and sampler intent. Start with native device/swap-chain,
clear/present and this minimal resource/draw slice in an isolated validation
entry; do not claim the whole native game is playable while unmigrated DX8/D3DX
and the independent x64 compatibility gates remain. No D3D11 work starts here.

## Changed-file inventory

All paths below are repository-relative. Build logs, probes, response files,
objects, captures, runtime copies and analysis outputs are ignored/uncommitted.

1. `Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.h`
2. `Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Render2D.cpp`
3. `Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt`
4. `Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h`
5. `Core/Libraries/Source/WWVegas/WW3D2/Render2DSubmission.h`
6. `Core/Libraries/Source/WWVegas/WW3D2/Render2DSubmission.cpp`
7. `Core/Libraries/Source/WWVegas/WW3D2/vertexbuffer.h`
8. `Core/Libraries/Source/WWVegas/WW3D2/indexbuffer.h`
9. `Core/Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.h`
10. `Core/Libraries/Source/WWVegas/WW3D2/dx8indexbuffer.h`
11. `Generals/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp`
12. `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp`
13. `Tests/Google/Core/CMakeLists.txt`
14. `Tests/Google/Core/Libraries/Render2DSubmissionTest.cpp`
15. `Tests/Google/Dependencies/CMakeLists.txt`
16. `scripts/rendering/test_render2d_reference.py`
17. `docs/modernization/RENDERER_BOUNDARY_1_RENDER2D.md`
18. `docs/modernization/RENDERER.md`
19. `docs/modernization/X64.md`
20. `docs/modernization/ROADMAP.md`
21. `docs/modernization/AGENT_HANDOFF.md`
