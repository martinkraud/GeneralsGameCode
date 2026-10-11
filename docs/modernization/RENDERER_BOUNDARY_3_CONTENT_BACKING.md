# RendererBoundary.3 - device-independent content backing

Reference frozen before producer changes: 2026-10-11,
`4a412d1104763a40a5732f0b7830dc0a56bfbdb9`, clean and synchronized
`dev/modern-engine` / `origin/dev/modern-engine`.
Baseline Win32 Release full build and CTest passed (4/4, including RB1/RB2).
The bounded native build and both game link probes reproduced their expected
failures; exact diagnostic comparisons are recorded with final validation below.

## Frozen content semantics

The preferred proof is the existing Render2DSentence / FontChars pipeline.
Its A4R4G4B4 glyph bytes are already produced by CPU/GDI; allocation of the
destination SurfaceClass and TextureClass currently requires D3D8.

* Sentence allocation: unchanged sizing loop considers powers 6 through 8,
  accounts for font height/spacing, then takes the maximum with TextureSizeHint.
  Both allocated and logical dimensions are the resulting square size. One mip.
* Surface creation does not clear pixels. Initial unused bytes and overlap
  destinations are unspecified in the reference; do not introduce a zero fill
  or claim a byte baseline for unwritten pixels.
* FontChars creates the existing 24-bit top-down GDI DIB, using 96 DPI, current
  Arial/Generals mapping, font width, antialiasing and overlap policy. Its DIB
  stride is `(width * 3 + 3) & ~3`.
* Store_GDI_Char uses the first intensity byte of each RGB triplet. Nonzero
  intensity sets RGB to 0xFFF; the high intensity nibble becomes alpha bits
  15..12. Zero intensity produces zero. Do not premultiply or change rounding.
* Blit_Char computes destination rows using `dest_stride >> 1`, adds x/y, copies
  complete glyph rows and ORs existing pixels in columns below PixelOverlap.
  Character-W origin adjustment, unicode fallback and glyph cache are retained.
* Build_Sentence locks only when it needs to write, passes the actual byte pitch
  to Blit_Char and keeps that lock until allocation/reset/build-textures unlocks.
  Layout/crop/UV generation uses the existing offsets, wrapping and clipping.
* Build_Textures unlocks and releases the current-surface hold, snapshots pending
  content into a square width-by-width single-mip texture, sets clamp U/V and
  NONE min/mag/mip, then assigns textures to renderers in the existing order.
  Pending, sentence-data and temporary references have distinct lifetimes.
* The original native upload is a whole-surface CopyRects into the managed
  texture's level zero. It performs no requested format conversion for this
  A4R4G4B4 sentence path. Producer pitch and destination upload pitch may differ.

Other audited paths remain subject to their existing policies:

| Producer | Frozen behavior / separate requirement |
| --- | --- |
| Font3D image fonts | File-backed SurfaceClass, transparency/bounds measurement, crop/copy into A4R4G4B4, then surface-to-texture creation. This is separate from FontChars sentence glyph generation. |
| Ordinary UI filenames | Both asset managers share filename TextureClass identities; UI requests one mip. TextureLoader's DDS/TGA paths retain capability-selected format, reduction, compression/fallback, HSV, thumbnail/background policy and mip conversion. Decoder output currently targets prelocked native storage. |
| Radar UI | Capability-dependent terrain/overlay/shroud image formats, pitched full-level locks, clears, regional CPU writes and existing shroud lock lifetimes. World terrain/shroud rendering is outside this task. |
| Video UI | Selected decoder pixel format, separately tracked logical/allocated dimensions, one mip, and pitched lock/write/unlock. Movie decoding is outside this task. |

## Implemented bounded path

`FontChars.h/.cpp` contains the actual pre-existing GDI rasterizer and glyph
cache, extracted from render2dsentence.cpp. The original three-argument font
initializer still passes WW3D's window; a four-argument overload accepts that
window explicitly, including null for a desktop DC in the isolated test.
GetDC/ReleaseDC use the same supplied handle. Explicit `std::min/max` resolves
inherited engine/standard overload ambiguity without changing scalar choices.
All other glyph implementation tokens are pinned to the accepted RB2 source.

`Texture2DCPUContent` is deliberately only a single-mip A4R4G4B4 allocation.
It records width/height, explicit positive signed byte pitch, and owned bytes.
The normal sentence pitch is width*2; padded positive pitches are supported.
It does not select formats, clear memory, decode files, generate mipmaps or
pretend to be a native surface. Lock/Unlock retain the sentence's existing
write cadence. Map returns a synchronous region lease using exclusive right/
bottom, original pitch and an offset pointer, rejects invalid/empty regions or
a second lock, and retains the backing until lease destruction. No revision
counter is needed: the renderer receives an immutable snapshot, not live edits.

Render2DSentence's pending/layout/current surface identities and references now
refer to CPU content. Allocation, wrapping, clipping, geometry, UV calculations,
glyph calls and sampler setters remain unchanged. Build_Textures snapshots
unlocked bytes into the new TextureClass CPU-content constructor, then assigns
the logical texture to the same renderers in the same order. Future producer
writes cannot alter a previously assigned texture. CPU content uses existing
W3D allocation/reference-count conventions; no allocator modernization occurs.

### Backend upload and interoperability

The new TextureClass constructor calls only the existing resource-free base
constructor, initializes the same managed/procedural/single-mip fields, and
retains a snapshot. It allocates no D3D8 texture or surface. Existing Render2D
submission and deferred Apply ordering remain unchanged.

At first backend residency creation DX8Backend consumes the neutral description
and bytes, allocates a managed A4R4G4B4 texture with the original helper, installs
its legacy alias, creates an upload surface, locks it, copies width*2 bytes per
row respecting both pitches, unlocks, and uses the original whole-surface
CopyRects into level zero. Temporary destination/source references are released;
DX8Texture2DResidency adopts the texture reference exactly as in RB2. Subsequent
use reuses that residency. GPU padding is not copied from CPU row padding.
The producer never creates a native surface merely to feed the backend.

Before first residency creation the legacy native pointer is null. Explicit
Get_Surface_Level on a CPU-backed texture requests residency before accessing
level zero; installation precedes that query, avoiding recursion. The retained
CPU snapshot is the immutable upload seed, not a mutable GPU readback cache.
Legacy native mapping/updates remain native; they do not claim to revise that
seed. Explicit Apply_New_Surface or virtual Set_D3D_Base_Texture discards the
seed before replacing native storage, preventing its accidental re-upload over
replacement content. The backend alone deliberately calls the base setter when
installing the initial seed. Existing non-sentence textures retain RB2 behavior.

Sentence textures remain managed and procedural: normal unused-texture
invalidation still skips them; DX8 managed storage retains the existing reset/
loss rules. No new default-pool reset tracker is needed. Destruction releases
the CPU seed in TextureClass, then base destruction resets the backend owner
before clearing its legacy alias. A pending producer may outlive its texture
snapshot or vice versa; neither holds the other's mutable storage. Existing
RB2 owner tests protect exactly-once reset/replacement/release. This is not a
general mutable CPU/GPU synchronization mechanism or a backend hot-swap system.

### Producers retained at compatibility boundaries

* **Font3D:** This bitmap-file font path is not used by FontChars sentences.
  SurfaceClass(filename), FindBB/Is_Transparent_Column and crop/copy depend on
  decoded native surface formats and the existing immediate DDS/TGA loader.
  Its variable source format and crop conversion materially differ from the
  fixed A4R4G4B4 sentence seed. Existing code remains token-identical.
* **Filename UI:** Asset managers share ordinary TextureClass identities with
  legacy consumers. TextureLoader::Begin_Compressed_Load/Begin_Uncompressed_Load
  select format/size/reduction using current DX8 capabilities, create storage,
  then lock levels for DDS/TGA load and conversion. Existing background/thumbnail,
  HSV, compressed fallback, missing texture, mip policy and decoding are retained
  unchanged behind the RB2 residency compatibility path. Moving only decoding
  would not establish device-independent capability selection; no future backend
  format policy is invented here. This remains a prerequisite for extending a
  future backend to arbitrary file-backed UI, not for the sentence proof.
* **Radar:** W3DRadar's UI images select supported RGB/R5G6B5 and alpha overlay/
  shroud formats and perform mutable pitched native level updates/clears with
  distinct lock lifetimes. They require format selection and update synchronization,
  rather than an immutable sentence seed. Retained unchanged; world terrain and
  shroud rendering are excluded.
* **Video:** W3DVideoBuffer's decoder-selected format, logical/allocated dimensions
  and repeated mutable Lock/Unlock path require the same additional update policy.
  Retained unchanged; the decoder itself is excluded. A future migration may
  give the decoder a CPU mapped lease, but this task does not invent that policy.

No cube/volume/world assets, render targets, readback, shadows, D3DX implementations,
external SDK libraries or gameplay/timing/persistence behavior were changed.

## Acceptance proof and limitations

The separate `sentence_content_test` executable compiles and executes the actual
FontChars.cpp, Texture2DCPUContent.cpp and wwstring.cpp. It links GTest/utility,
User32/GDI32 and the normal C++ runtime, with DISABLE_GAMEMEMORY solely for the
standalone allocator. It has no engine archive, DX8 SDK include directory,
D3D8 device, native graphics fake or D3D graphics import. Actual Arial glyphs
are rasterized by GDI, converted by the unchanged Store_GDI_Char, and written by
the unchanged Blit_Char into the same CPU backing now used by Render2DSentence.
This establishes real existing sentence glyph content production, not just an
API model. It does not execute the complete game, Build_Sentence layout, a GPU
upload, or a draw in the isolated executable; those routes are compiled and
protected by reference/recording checks rather than falsely called runtime proof.

Five tests cover real glyph dimensions/alpha/RGB invariants, byte-exact tight vs
padded writes including legacy overlap OR and untouched padding, repeated cached
blits, offset region mapping/lifetime/invalid regions, independent snapshots,
row upload/repeated upload and replacement bytes. Font bitmaps are compared within
the same actual rasterization environment; no cross-machine fixed GDI hash is
claimed. Unwritten allocation bytes remain unspecified as in the reference.

RB1/RB2 recording tests continue to protect submission, null/disabled/missing,
sampler, residency creation/reuse/reset and balanced ownership. Python checks
reverse only the explicit storage/window seams and compare every original sentence
and glyph token, plus the retained complete loader/surface/font3d/dynamic UI code.
They independently pin upload ordering and original CopyRects, sampler mappings,
and all other texture behavior. They do not prove driver execution or visuals.

## Validation

All final source validation used the source recorded in this commit, VS 18
Community/MSVC 19.51 (14.51.36231) and Windows SDK 10.0.26100.0. No runtime
candidate was prepared or launched.

| Check actually run | Result |
| --- | --- |
| Full Win32 Release game/tool/test graph, `cmake --build build/win32 --config Release --parallel 4` | PASS, both games and enabled tools/tests |
| Win32 CTest | PASS 5/5 |
| Direct g_googletest / z_googletest | PASS 147 each; six existing disabled each |
| Focused path/profiler/developer/RB1/RB2 filter | PASS 73 each title |
| Independent RB1/RB2 render2d_submission_test | PASS 15 |
| Independent real sentence_content_test, x86 and AMD64 | PASS five each (native through CTest) |
| Native independent CTest: diagnostics/submission/content | PASS 3/3 |
| Python rendering reference checks | PASS 17 |
| Python performance tests | PASS 16 |
| PowerShell parser | PASS nine scripts |
| Preservation audit | PASS 391 prior runtime/evidence files, accepted A.2 hashes, empty index |
| Actual SDK-free source probes | PASS seven per architecture, x86 0x14c / AMD64 0x8664 |
| Producer executable import inspection | GDI/User32/runtime imports; no D3D/DXGI import, both architectures |
| git diff --check | PASS |

The SDK-free probes compile the actual two title Render2D sources,
Render2DSubmission.cpp, Texture2D.cpp, render2dsentence.cpp, FontChars.cpp and
Texture2DCPUContent.cpp. They retain real Release definitions/includes and forced
BasePrecomp, removing only the SDK include directory, SDK-containing PCH and
output paths. No shim headers or disabled rendering branches are introduced.
Header traces contain no DX8 SDK headers. This establishes SDK-header
independence separately from the executed real GDI device/resource-free proof.

Local ignored final logs: `build/renderer-boundary-3-final-win32.log`,
`renderer-boundary-3-win32-ctest.log`, `renderer-boundary-3-direct-{g,z}.log`,
`renderer-boundary-3-focused-{g,z}.log`, `renderer-boundary-3-producer-win32.log`,
`renderer-boundary-3-render2d-win32.log`, `renderer-boundary-3-final-win64.log`,
`renderer-boundary-3-final-link.log`, `renderer-boundary-3-native-ctest.log`,
`renderer-boundary-3-final-sdkfree-{win32,win64}.log`,
`renderer-boundary-3-{win32,win64}-producer-imports.log` and
`renderer-boundary-3-results.log`. Helpers/objects/logs are not committed.
An early standalone compile exposed missing inherited includes and min/max
ambiguity; these were resolved before final full validation. An early CTest
attempt overlapped final links and could not start three locked executables;
the final post-link CTest and direct runs passed. An SDK-probe helper invocation
initially used the wrong interpreter; final clean probe logs supersede it.

### Native observation, not native success

Ran full `cmake --build --preset win64 --parallel 4 -- -k20`, then separate
`--target z_generals g_generals --parallel 4 -- -k20` link probes. Both titles'
new neutral/producer/adapter sources compile; the native games reach linkage.
Exact diagnostic multisets match the pre-edit RB3 baseline: 30 tool C++ errors
(six BOOL dialog callbacks, two DWORD timers, eight GWL_WNDPROC identifiers,
four DoModal returns, two file-dialog hooks, four OnTimer casts and four
associated C2737 diagnostics). No new native compile blocker is retained.

Both game probes still fail on the same 17 D3DX helpers each:
`D3DXAssembleShader`, `D3DXCreateCubeTexture`, `D3DXCreateTexture`,
`D3DXCreateTextureFromFileExA`, `D3DXCreateVolumeTexture`, `D3DXFilterTexture`,
`D3DXGetErrorStringA`, `D3DXGetFVFVertexSize`, `D3DXLoadSurfaceFromSurface`,
`D3DXMatrixInverse`, `D3DXMatrixMultiply`, `D3DXMatrixRotationZ`,
`D3DXMatrixScaling`, `D3DXMatrixTranslation`, `D3DXMatrixTranspose`,
`D3DXVec3Transform`, `D3DXVec4Transform`. The symbol-occurrence multiset is
unchanged: 34 LNK2019, 24 LNK2001, two LNK1120. No dependency disappears
naturally; no I386 SDK library is linked into native targets. The pinned
min-dx8 dependency remains `7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc`.
Native tool ABI, persistence/pointer/HANDLE, allocator/alignment, FP/lockstep
and audio/runtime qualifications remain outside scope. This is not a playable
Win64 game or a runtime determinism certification.

The five reports plus command.txt in `build/performance/stage4a2-B-final-2`
remain available, ignored and unchanged. `build/performance/stage4a1-B-cpu/B-cpu.etl`
remains unchanged at 2,469,396,480 bytes. No capture, ETL, generated helper,
build output, analysis artifact or runtime copy is part of this change.

## D3D11.1 readiness: YES for the bounded sentence textured Render2D path

A new backend can consume the actual existing W3D glyph bytes, dimensions,
format, pitch and sampler intent through TextureClass/Get_Render2D_CPU_Content,
create its own Texture2D residency and execute RB1 submission without requiring
a D3D8 device/resource in this content path. The window/GDI rasterizer is a CPU
producer, not D3D8. This conclusion is bounded to sentences; it does not claim
all UI assets, Font3D, radar/video or the whole renderer are device-independent.

Recommend **D3D11.1** next: native device/DXGI swap chain, clear/present, viewport,
minimal vertex/index resources, backend-created Texture2D upload from this real
sentence content, sampler state, shaders/pipeline and indexed Render2D draw.
Retain DX8 as the reference. Do not broaden that first slice to all legacy assets,
fix unrelated native blockers or claim a playable Win64 game. No D3D11 work has
begun, no game was launched, and no visual equivalence is claimed.

## Exact changed-file inventory

* Core/Libraries/Source/WWVegas/WW3D2/FontChars.h
* Core/Libraries/Source/WWVegas/WW3D2/FontChars.cpp
* Core/Libraries/Source/WWVegas/WW3D2/Texture2DCPUContent.h
* Core/Libraries/Source/WWVegas/WW3D2/Texture2DCPUContent.cpp
* Core/Libraries/Source/WWVegas/WW3D2/Texture2D.cpp
* Core/Libraries/Source/WWVegas/WW3D2/texture.h
* Core/Libraries/Source/WWVegas/WW3D2/texture.cpp
* Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.h
* Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp
* Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Texture2D.cpp
* Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt
* Tests/Google/Core/Libraries/SentenceContentTest.cpp
* Tests/Google/Dependencies/CMakeLists.txt
* scripts/rendering/test_sentence_content_reference.py
* scripts/rendering/test_texture2d_reference.py
* docs/modernization/RENDERER_BOUNDARY_3_CONTENT_BACKING.md
* docs/modernization/RENDERER.md
* docs/modernization/X64.md
* docs/modernization/ROADMAP.md
* docs/modernization/AGENT_HANDOFF.md
