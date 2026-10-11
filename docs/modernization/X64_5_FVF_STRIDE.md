# X64.5: native integer FVF stride compatibility

Date: 2026-10-11. Baseline: `1bddbfc42fafc80949e5b988f19387dcc8272cf3`
(`X64.4: replace external D3DX math with native CPU compatibility`). Before
edits, HEAD, dev/modern-engine, origin/dev/modern-engine, clean status and
zero ahead/behind were verified. The native baseline was rebuilt and probed.

## Result and scope

All three production D3DXGetFVFVertexSize calls now use
`LegacyD3DX::FVFVertexSize`. This header-only, constexpr uint32 operation
includes only cstdint. It has no SDK, D3DX implementation, device, allocation,
floating-point or D3D11 dependency. Exact reference tests pass on Win32 and
AMD64. No vertex declaration, offset, packing, buffer policy or resource helper
changed. Existing d3dx8core includes are retained; unrelated cleanup is excluded.

Both native games compile through the changed sources but still fail linkage:
exactly eight external D3DX helpers remain per title. This is linker progress,
not playable native support. DX8 stays default. No game was launched.

## Actual callers and stride consumers

| Source / route | FVF and consumption | Classification |
|---|---|---|
| WW3D2/dx8fvf.cpp, Get_FVF_Vertex_Size -> FVFInfoClass constructor | Named engine formats and mesh-generated FVF; stores fvf_size. DX8 vertex-buffer allocation uses vertex count times stride; locks, copies, stream binding, backing size and accounting also consume it. Dynamic buffers use the same FVFInfo route. | Presentation-only |
| GameClient/HeightMap.cpp, HeightMapRenderObjClass::Render | PRE_TRANSFORM_VERTEX route: XYZRHW + DIFFUSE + TEX2 = 0x244, 36 bytes; SetStreamSource stride after ProcessVertices. | Presentation-only |
| Same file, renderTerrainPass | Same conditional pre-transformed format and 36-byte stream stride. | Presentation-only |

The two HeightMap calls are compiled out in the default configuration, but
are migrated and their input is explicitly tested. Neither call allocates a
buffer at the changed statement. The FVFInfo consumer does affect render-buffer
allocation sizes, so exact equality matters. Its independent offset calculation
is untouched, as are dx8vertexbuffer.cpp, dx8indexbuffer.cpp and DynamicVB/IB.

The returned stride is not written to snapshot/save/network/replay state by
these routes. No authoritative simulation, RNG, FP, timing, pathfinding or
serialized field changes. Unit tests do not certify multiplayer or runtime
visual equivalence; neither is claimed. HeightMap's broader shared responsibilities
do not make these two render-only stride uses authoritative.

### Production layout coverage

The tests include the actual enum declarations from dx8fvf.h, rather than
redeclaring their values. Recorded byte sizes:

| DX8_FVF suffix | Bytes |
|---|---:|
| XYZ | 12 |
| XYZN | 24 |
| XYZNUV1 | 32 |
| XYZNUV2 | 40 |
| XYZNDUV1 | 36 |
| XYZNDUV2 | 44 |
| XYZDUV1 | 24 |
| XYZDUV2 | 32 |
| XYZUV1 | 20 |
| XYZUV2 | 28 |
| XYZNDUV1TG3 | 72 |
| XYZNUV2DMAP | 52 |
| XYZNDCUBEMAP | 28 |

Also covered: both titles' shadow-buffer XYZ/XYZRHW, optional diffuse and
0..2 UV sets; dx8renderer.cpp Define_FVF's XYZ, optional normal/diffuse/specular
and 0..8 two-dimensional UV sets; sorting/dynamic XYZNDUV2 (0x252, 44 bytes);
and the terrain pre-transform format. Compile-time assertions freeze all 13
vertex-structure sizes and the common normal/diffuse/UV layout.

An existing dormant mismatch is preserved explicitly: VertexFormatXYZNUV2DMAP
is 48 bytes, while its declared FVF requests 52 bytes (1D + 4D + 2D texture
coordinates). XYZNDCUBEMAP's texture bits are commented out in its declaration,
so its stride is 28. This task neither repairs nor activates either format.

## Pinned legacy characterization, performed before replacement

Oracle: actual I386 d3dx8.lib from pinned min-dx8-sdk commit
`7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc`, fetched under build/win32/_deps/dx8-src.
The legacy-only recorder does not include the replacement. Disassembly of
`_D3DXGetFVFVertexSize@4` and executed library results agree:

| Bits / interpretation | Exact legacy result contribution |
|---|---|
| fvf & 0x0e = 0 | 0 |
| XYZ (2) | 12 |
| XYZRHW (4), XYZB1 (6) | 16 |
| XYZB2/3/4/5 (8/A/C/E) | 20/24/28/32 |
| NORMAL / PSIZE / DIFFUSE / SPECULAR | 12 / 4 / 4 / 4 |
| Texture count | (fvf >> 8) & 15, not clamped to eight |
| Per-slot dimension encoding 0/1/2/3 | 8/12/16/4 bytes |
| Reserved bit 0, LASTBETA_UBYTE4, reserved 0xe000 | Ignored; add no bytes |

The upper word shifts right two bits per texture slot. Counts 9..15 consume
zero encodings after slot eight, each adding eight bytes. There is no format
validation: NORMAL + TEX1 with no position returns 20; count 15 with no
dimension bits returns 120; 0xffffffff returns 144. Newer SDK position masks
or validation policies must not be substituted. The maximum result is bounded
at 240 bytes. The implementation remains an integer sizing function for all
32-bit input words, including nonsensical combinations.

## Reference and native proof

The deterministic sweep has **1,245,282 inputs**: actual production formats,
shadow and mesh combinations, every low 16-bit word with two dimension patterns,
every possible eight-slot dimension pattern at every count 0..15, 65,536 seeded
full-word inputs, and all bits set. This covers all supported bitfields and
reserved/count behavior; it is not an exhaustive enumeration of 2^32 words.

LegacyFVFStrideReference.inc intentionally versions the actual library's
256 low-byte results, four dimension results, 13 production results and raw
whole-sweep fingerprint `0x8d9347ea83c19311`. The independently executed Win32
live test compares every input to D3DX and checks the composed recorded oracle
too. Native tests compare exact integers to those tables for every input;
the fingerprint is supplementary, not the sole oracle. The decomposition is
supported by the disassembly and checked against all live sweep results.
The CMake-built legacy-only recorder exactly reproduces the fixture.

Win32: four independent tests and five live comparison tests pass. AMD64: all
four independent tests pass. dumpbin shows machine 8664 / PE32+. Its link edge
contains GoogleTest and Windows system libraries, no D3DX archive, D3D8 or
D3D11 implementation. Imports contain only Windows/CRT DLLs, no D3D/D3DX/DXGI.
The test uses SDK headers and actual engine structs for assertions; a separate
AMD64 compile/run of the production header alone, with only a repository include
path and the standard library, also passes. No graphics device is required.

## Regression and native linker acceptance

Actually executed with VS18/MSVC 19.51, SDK 10.0.26100, Release:

- Full Win32 graph passes, including games and tools.
- Win32 CTest **11/11** passes, including old LegacyD3DX math/reference tests.
- Direct GoogleTest **149/title** passes; six pre-existing disabled tests/title.
- Focused renderer/path/profiler/developer/Bezier tests **75/title** pass.
- RB1 Render2D, RB2/RB3 submission/content producer checks pass.
- D3D11.1 sentence and D3D11.2 UI real WARP tests pass on Win32 and AMD64.
- Independent native CTest **7/7** passes, retaining all six prior native tests.
- Rendering Python **25/25**, performance Python **16/16**, PowerShell parser
  **9 files**, preservation audit and git diff --check pass.

The rendering source-preservation check initially rejected the required
dx8fvf.cpp migration. It now reverses only the one expected include and one
expected helper substitution, asserting their counts, before its original
whole-source token comparison. All other protected files remain exact.
Both changed production files also match the baseline after reversing just
their includes/helper substitutions, accounting for Git newline normalization.

Before and after: bounded full Win64 graph (`-k20`), then separate
z_generals/g_generals link probes. Expected failing graph/link results are
reported independently of the passing native tests:

| Native game diagnostic measure | Baseline | X64.5 |
|---|---:|---:|
| Unique D3DX helpers per title | 9 | 8 |
| Combined LNK2019 | 18 | 16 |
| Combined LNK2001 | 8 | 8 |
| Combined LNK1120 | 2 | 2 |

Only D3DXGetFVFVertexSize disappears. Every remaining unresolved-symbol
occurrence is unchanged. No new game compiler or linker dependency appears.
Neither game links yet. The full graph's exact compiler diagnostic multiset
still has **30 tool ABI errors**: C2065=8, C2440=6, C2555=4, C2664=8, C2737=4,
in W3DView/GUIEdit/WorldBuilder. These are separate from the game link graph.
They were not fixed or hidden.

Ignored evidence: build/x64-5-baseline-*, x64-5-final-*, x64-5-native-*,
x64-5-fvf-*, x64-5-win32-ctest.log, x64-5-direct-*, x64-5-focused-*,
x64-5-header-proof.* and x64-5-validated-source.json. Analysis/generation helpers
and disassembly also stay in build/. The 391 preserved prior runtime/evidence
files and accepted A.2 capture hashes pass their audit. The final A.2 capture
remains available; B-cpu.etl remains 2,469,396,480 bytes and ignored.

## Remaining helper classification

Counts are direct production source call sites, shared across both games,
including explicit retry calls. They are not per-title link occurrence counts.
WorldBuilder-only D3DXCreateFont calls are outside the game-helper inventory.
All eight rows below remain actual link blockers in **both** games.

| Helper | Calls / locations | Dependency, existing help and estimated migration risk |
|---|---|---|
| D3DXCreateTexture | 4, dx8wrapper.cpp (ordinary/RT plus retries) | Device resource. Existing wrapper/caps/format APIs help; preserve capability-driven format/dimension/mip/pool policy and failures. Medium-high. |
| D3DXCreateCubeTexture | 4, dx8wrapper.cpp (ordinary/RT plus retries) | Device resource; shares creation requirement policy with 2D but adds cube caps/levels. Medium-high. |
| D3DXCreateVolumeTexture | 2, dx8wrapper.cpp (initial/retry) | Device resource; shared policy plus volume extent/format/caps. Medium-high. |
| D3DXCreateTextureFromFileExA | 1, dx8wrapper.cpp | CPU decode/convert plus device upload. Existing TGA/DDS loader helps partially; D3D11.2 top-mip restrictions do not match defaults, full mip/filter/conversion or missing fallback. High. |
| D3DXFilterTexture | 6: dx8wrapper.cpp 1, W3DTreeBuffer.cpp 1, TerrainTex.cpp 4 | Device mip reads/writes plus CPU filtering. Existing format/conversion routines help partially; box filtering across formats/palettes/pitches is not already equivalent. High. |
| D3DXLoadSurfaceFromSurface | 4: dx8wrapper.cpp 1, missingtexture.cpp 1, surfaceclass.cpp 2 | Device surface access, rect scaling/conversion/filtering (BOX/NONE/TRIANGLE). Existing SurfaceClass helpers do not prove matching semantics. High. |
| D3DXAssembleShader | 4: W3DWater.cpp 3; W3DProfilerFrameCapture.cpp 1 under PROFILER_ENABLED | CPU assembler, device consumes tokens. Three fixed ps.1.1 water strings and one fixed ps.1.4 swizzle string allow a bounded exact-bytecode route, not a general assembler. D3D11 HLSL is unrelated. Low-medium with pinned bytecode/conditional/reset/error coverage. |
| D3DXGetErrorStringA | 2, dx8wrapper.cpp fatal/nonfatal logging | CPU diagnostics. Existing engine logging helps, but HRESULT/text/truncation/return semantics need a pinned reference. Low; only removes one helper. |

Existing TextureLoader::Validate_Texture_Size and Get_Valid_Texture_Format
already supply useful caps/format context. They are not proven substitutes for
D3DX's requirement negotiation. The creation wrappers explicitly promise
closest supported formats, and their OUTOFVIDEOMEMORY invalidation/retry and
render-target NOTAVAILABLE behavior must remain intact. The three creation
helpers are a coherent allocation-only group; file decoding, filtering and
surface copying are separate, substantially broader contracts.

IRenderBackend and D3D11Backend remain useful renderer seams, but their D3D11
resources cannot satisfy existing IDirect3DTexture8/cube/volume return contracts.
The bounded CPU UI content loader likewise is not a replacement for general
DX8 resource helpers. No fake success or reinterpretation of resources is safe.

## Exactly one recommended next task

**X64.6: reference-tested DX8 texture creation compatibility for
D3DXCreateTexture, D3DXCreateCubeTexture and D3DXCreateVolumeTexture.**

This group has ten direct calls, one shared wrapper location and a common
capability/format/dimension/mip negotiation problem. Natural completion would
remove three blockers per title (8 -> 5) and provide the allocation prerequisite
for the later file/filter/copy routes. Bound the task to creation and requirement
negotiation; retain actual DX8 COM resource creation and wrapper retry/ownership
behavior. It must not expand into file loading, filters, shader assembly, tool
ABI fixes or a D3D11 renderer rewrite.

Start by characterizing the pinned Win32 helpers against controlled capability
and creation responses before implementing policy. Compare HRESULTs, requested
and negotiated format/dimensions/levels/usage/pool, call order and ownership,
including default/zero sizes, limits, unsupported formats and failures. Use
recording device interfaces for bounded deterministic CPU reference tests and
plan separate real-device resource validation; mock success alone cannot
establish hardware compatibility. Do not merely call CreateTexture unchanged
or assume the engine's partial normalization equals D3DX. Require exact native
helper reduction and the existing full regression suite. If characterization
does not bound the contract safely, stop at that evidence gate rather than
inventing resource behavior.

D3DXGetErrorStringA is smaller, but alone unlocks no resource route; choosing it
next would defer the shared allocation prerequisite for only a one-helper gain.
Fixed shader bytecode is a credible bounded alternative and safer than a
general assembler, but removes only one helper and does not help the six
resource dependencies. The coherent creation trio offers more useful native
link progress without taking on the entire six-helper resource subsystem.
Its higher compatibility risk warrants reference characterization first.
No next task has begun. Native runtime/audio/video and eventual replay/save/
multiplayer verification remain separate gates after executable linkage.

## Exact changed-file inventory

- Core/Libraries/Source/WWVegas/WW3D2/LegacyFVFStride.h
- Core/Libraries/Source/WWVegas/WW3D2/dx8fvf.cpp
- Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp
- Tests/Google/Dependencies/CMakeLists.txt
- Tests/Google/Core/Libraries/LegacyFVFStrideCases.h
- Tests/Google/Core/Libraries/LegacyFVFStrideReference.cpp
- Tests/Google/Core/Libraries/LegacyFVFStrideReference.inc
- Tests/Google/Core/Libraries/LegacyFVFStrideTest.cpp
- scripts/rendering/test_render2d_reference.py
- docs/modernization/X64_5_FVF_STRIDE.md
- docs/modernization/X64.md
- docs/modernization/ROADMAP.md
- docs/modernization/AGENT_HANDOFF.md

The .inc is intentionally versioned test reference data. Captures, ETL, runtime
copies, binaries, logs and analysis helpers are excluded from the commit.
