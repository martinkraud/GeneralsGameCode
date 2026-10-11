# D3D11.2: ordinary single-mip UI file textures

Implemented 2026-10-11 from clean synchronized
`0170ad1adc8594f06e953dad12894818dfa1c683`, `dev/modern-engine` tracking
`origin/dev/modern-engine`. D3D11.1 baseline validation was reproduced before
source edits: full Win32 Release, CTest 6/6, sentence/RB tests, native independent
CTest 4/4, and the accepted native full/link blocker sets.

The bounded end state is real ordinary UI file bytes -> neutral owned single-mip
content -> immutable D3D11 residency -> existing Render2D geometry and normal-alpha
pipeline -> real WARP draw/readback, with no D3D8 resource in that isolated route.
DX8 remains the game factory default. No game was launched; this is not a whole
UI/renderer conversion, playable native game, or manual visual-equivalence claim.

## Actual caller and frozen legacy semantics

`W3DDisplay::drawImage` (2770) returns for null images/fully clipped rectangles.
The ordinary branch (2801) requests `Get_Texture(filename, MIP_LEVELS_1)`;
IMAGE_STATUS_RAW_TEXTURE is a separate retained route. `setup2DRenderState` (636)
retains the texture, enables texturing, disables additive, enables normal alpha
and disables grayscale for DRAW_IMAGE_ALPHA. Existing clipping, rotated UVs,
diffuse color and quad/triangle construction remain unchanged.

Both titles' `WW3DAssetManager::Get_Texture` lower-case the name, cache by name
and Add_Ref the existing/created TextureClass. Defaults are UNKNOWN format,
compression allowed, regular 2D, reduction allowed. An already cached texture
retains its existing mip/format identity; D3D11 does not overwrite that policy.
TextureClass filename construction creates logical ownership; with thumbnails
disabled it can call Init immediately. Therefore the new content hook must be
before native allocation in Init, not solely at draw submission.

| Legacy policy | Bounded D3D11.2 treatment |
|---|---|
| TextureLoadTask::Begin_Load/Load tries DDS when compression is allowed, then TGA | Reader tries the same last-three-character `.dds` sibling first; absent DDS permits the original TGA request. Compression disabled skips DDS |
| DDSFileClass parses DXT storage; regular/cube/volume distinction; reduced source chain | Regular DXT1/3/5 top level retained byte-for-byte. Source-chain availability follows zero/one mip normalization and dropping the two lowest levels; destination has one mip |
| MIP_LEVELS_1 suppresses global reduction | Bridge requires exactly one mip; content uses level zero, independent of global texture-reduction preference |
| Validate_Texture_Size rounds to powers of two, clamps to device maxima and adjusts aspect | This initial slice accepts already-valid POT dimensions 4..8192 within the explicit neutral limits. Assets requiring rescaling/clamping/aspect adjustment reject; no new resampling policy |
| Get_WW3D_Format recognizes 24-bit BGR and 32-bit BGRA | True-color raw/RLE TGA supported, preserving byte order and orientation |
| Get_Valid_Texture_Format changes R8G8B8 to X8R8G8B8; default texture depth is 16 | 32-bit policy selects X8R8G8B8/A8R8G8B8; 16-bit policy selects R5G6B5/A4R4G4B4 with the reference bit truncation |
| DX8 capabilities can force format fallback, decompression or the NVIDIA DXT1-to-DXT2 workaround | Modern BC1/2/3 upload is explicit. Other capability-dependent conversions and DXT2/4 reject; no enum casts or silent reinterpretation |
| HSV recolors through BitmapHandler/DDS endpoint processing | Nonzero HSV rejects before file access; existing DX8 recoloring is unchanged. This is a supported-subset restriction, not HSV equivalence |
| MissingTexture is a shared native resource | Absence remains distinct from malformed/unsupported content. Native missing fallback is not migrated; the D3D11 bridge reports absence rather than rendering white or inventing fallback pixels |
| File TextureFilter defaults DEFAULT min/mag, NONE mip, REPEAT U/V | Explicit global filter intent plus repeat addressing, mip level zero; sentence NONE/clamp behavior retained |

The entire actual caller, both asset managers/render2d/DDS implementations,
Targa decoder, loader, BitmapHandler, format/capability policy, missing resource
and DX8 adapters are token-frozen against the accepted baseline. TextureClass
Init differs only by an optional backend hook; removing that exact hook and its
include reproduces the old source. DX8 inherits a false-returning hook, leaving
its foreground/background/thumbnails/refcount behavior intact.

## CPU content and engine bridge

`UITextureContent` is a bounded extracted equivalent of the audited TGA raw/RLE
decode/orientation and DDS top-mip content route. Its reader is injected file
access, not a graphics abstraction. The engine adapter uses the actual
`_TheFileFactory`/`file_auto_ptr`, retaining archived/search-path resolution.
The isolated proof uses ordinary file I/O with the very same content producer.
File bytes and all read status are owned; malformed/truncated packets and
unsupported layouts reject before GPU residency. Reader size is bounded at
256 MiB; content dimensions at 8192 in this file slice.

`Texture2DCPUContent` keeps its original A4R4G4B4 sentence constructor and adds
an explicit-format overload for A8R8G8B8, X8R8G8B8, R5G6B5 and DXT1/3/5.
Pitch, row bytes and storage rows are explicit; compressed rows count blocks,
not pixels. Snapshot copies owned storage, Copy_Rows respects padding, and
uncompressed region maps use the correct pixel size. Compressed mapping rejects
rather than exposing a false pixel layout. Storage arithmetic checks overflow.

`IRenderBackend::Prepare_File_Texture` defaults to false. The D3D11 override
requires the active backend, regular nonprocedural single-mip file content and
no existing native storage. It rejects unsupported explicit format requests,
captures compression/texture-depth/HSV intent, decodes once and installs a CPU
snapshot in the actual TextureClass before its legacy loader can allocate.
The generic CPU-content constructor now retains the content's W3D format.

Installation snapshots first, then releases old residency/content and updates
dimensions/format/initialized state. Existing invalidation clears residency and
marks the file texture uninitialized; next Init reloads/replaces the CPU snapshot.
The old CPU copy remains until replacement/destruction, so there is no dangling
bound pointer. Backend SRV references and the logical residency owner retain the
existing D3D11.1 lifetime discipline. Mixed-backend reuse is rejected.

The FileFactory/TextureClass bridge compiles in both titles but is not executed
through a running WW3D/game here. Actual Get_Texture cache, W3DDisplay batching,
archive resolution and in-game lifetime/visual behavior remain runtime gates.
The isolated shared producer/upload/draw proof does not claim that whole chain
was executed end-to-end.

## D3D11 upload, sampler and rendering

A8/X8 content converts BGR(A) to RGBA8 explicitly, forcing opaque X8 alpha.
A4R4G4B4 uses the already-tested nibble expansion; R5G6B5 preserves quantized
normalized channel intent in RGBA8 (readback tolerance one UNORM unit).
DXT1/3/5 upload unchanged compressed blocks as BC1/2/3 UNORM. DXT2/4,
premultiplied and unknown formats never masquerade as straight-alpha images.
The DDS parser uses the documented [DDS header layout](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header).
No native types were added to the neutral content or renderer API.

The existing D3D11.1 normal-alpha shader, color swizzle, culling, depth behavior,
viewport, indexed vertex writer and half-pixel correction are unchanged.
Other ShaderClass permutations still fail closed. File DEFAULT filtering uses
the actual WW3D global mode/anisotropy: NONE/POINT -> point, BILINEAR/TRILINEAR ->
linear within the sole mip, ANISOTROPIC -> explicit anisotropic intent. UI repeat
and sentence clamp are separate supported sampler tuples; other tuples reject.
Sampler states are cached by bounded mode/anisotropy, not created per draw.

Null texture remains the explicit diffuse/white-texel route. Absent files,
legacy missing identity and unsupported D3D11 content are not converted to null.
The old MissingTexture::_Init writes `0x7FFF00FF` to a 128x128 native texture;
its row-pointer update repeats row zero and leaves the final row unspecified.
This slice neither changes that legacy behavior nor creates a checkerboard or
claims a deterministic CPU equivalent of the unspecified row. Missing assets
are an explicit D3D11 compatibility limitation, with an Absent status and error.

## Real file/device proof and output validation

`d3d11_ui_test` adds nine tests using real WARP D3D11 devices:

- Raw/RLE 24/32-bit TGA, all four origin combinations and repeated packets.
- DDS sibling priority, compression-disabled TGA, exact top-level bytes,
  source-chain truncation rejection, one mip and immutable snapshots.
- Separate absent, invalid and unsupported outcomes; no fallback white texture.
- Padded rows, region offsets, copy padding and independent snapshot ownership.
- Actual repository `Core/Tools/W3DView/Res/grab.tga`: real UI cursor file,
  raw-byte decode comparison and real normal-alpha draw/readback/reuse.
- Actual BC1/BC2/BC3 rendering, including partial BC2/BC3 alpha equations.
- Repeat UV addressing and point versus bilinear/trilinear output.
- Padded A8R8G8B8 GPU upload and all 16 source alpha levels with straight blending.
- Actual default 16-bit selection, reference bit truncation and real GPU output.

An additional local proof extracted **SAUserInterface512_001.tga** read-only
from installed `EnglishZH.big`, with its definition from `INIZH.big`:
`SAChinook_L`, `SAsentry_L`, `SACDozer_L` and other mapped UI images reference
that atlas. No sibling occurs in TexturesZH.big. The 512x512, 32-bit TGA hash is
`d427eabac75217ee0b082138f69ff1e0935bcf38a2f2030127082c0c65b47418`.
The real-file proof passed with **230,400 visible pixels**, no partial source
alpha pixels (vertex alpha still exercises blending), exact independent raw
decode checks and bounded GPU output comparisons. It ran on x86 and AMD64.

The extracted atlas/INI/receipt, XML and logs stay ignored under
`build/d3d11-2-retail-ui*`; no retail assets are committed. Default CTest uses
the existing repository cursor asset and does not require a game installation.
`D3D11_UI_TEST_ASSET` selects the additional local atlas proof. This variable is
scoped to the test process; it does not alter game settings or assets.

Output comparisons use regions/pixels and one/two-UNORM tolerances, not
cross-driver golden hashes. Repeated draws reuse one GPU texture and two dynamic
geometry buffers; replacement creates a new texture without buffer recreation.
CPU decode, GPU resource/upload and actual draw/readback are separate proofs.
WARP is a real software D3D11 implementation; hardware-driver and final manual
DX8 visual-equivalence coverage remain unverified.

## Validation and native observation

Release, VS Community 18/MSVC 19.51, toolset 14.51.36231, SDK 10.0.26100.
The complete graph was rerun after the final source edits. Ignored logs use
`build/d3d11-2-*`, with separate `d3d11-2-baseline-*` evidence.

| Validation | Result |
|---|---|
| Full Win32 game/tool/test graph | PASS |
| Win32 CTest | 7/7 PASS |
| Direct title tests | 147 PASS/title; six existing disabled tests/title |
| Focused renderer/path/profiler/developer | 73 PASS/title |
| Independent RB1/RB2 / real RB3 producer | 15 / 5 PASS |
| Real D3D11 sentence / file UI tests | 7 / 9 PASS on x86 and AMD64 |
| Native independent CTest | 5/5 PASS |
| Real retail atlas additional proof | PASS x86 and AMD64 |
| Rendering / performance Python | 25 / 16 PASS |
| PowerShell parsing | Nine scripts PASS |
| Preservation audit | 391 prior runtime/evidence files and accepted A2 hashes unchanged |
| Whitespace/source inventory | PASS; no generated artifacts staged |

The bounded Win64 full graph and separate g_generals/z_generals link probes
were run and still fail on the exact baseline multisets: 30 tool ABI C++ errors,
17 D3DX helper names per title; aggregate LNK2019=34, LNK2001=24, LNK1120=2.
The decoder, CPU formats, bridge and backend compile AMD64; isolated native
targets link and run. No legacy helper was naturally removed and no new blocker
was exposed. No I386 SDK library was substituted into native linkage.

No performance gain is claimed. Decode/snapshot/upload happen at initialization
or replacement, not per draw; samplers/residency/geometry buffers are reused.
CPU backing increases memory residency and temporarily duplicates bytes during
installation/upload. No synchronous readback was added to normal rendering.
No batching, instancing, aspect-specific UI policy or graphics effects changed.
Accepted captures/ETL/runtime evidence were preserved; no runtime candidate or
game launch occurred. Simulation, pacing, pathfinding, save/replay/network are
unchanged.

Unsupported scope: nonzero HSV, forced arbitrary formats, resized/non-POT or
out-of-limit assets, paletted/grayscale TGA, DXT2/4/DX10/cube/volume DDS, legacy
missing fallback, other sampler tuples/draw modes, mutable radar/video, Font3D,
world rendering, display integration and recovery. These limits prevent a
whole-game D3D11 compatibility claim; they do not change the DX8 reference.

## Next-task gate: return to x64

**Recommend exactly one next task: X64.4 - remove the eight remaining external
D3DX math helper dependencies with a tested native CPU compatibility boundary.**

The native links still need MatrixInverse, MatrixMultiply, MatrixRotationZ,
MatrixScaling, MatrixTranslation, MatrixTranspose, Vec3Transform and
Vec4Transform. Actual callers include Common/Bezier/BezierSegment.cpp,
W3DWater.cpp, W3DTreeBuffer.cpp, W3DShaderManager.cpp and W3D sorting code. These
math calls can progress independently of another UI renderer slice. WWMath
already compiles natively; the new D3D11 UI slice does not remove these callers.

Freeze x86 results and operation/FP ordering, audit layout/aliasing, validate
matrix conventions, singular handling and in-place calls before changing them.
Bezier participates in game-side math: replay/lockstep equivalence must be
explicitly tested, not inferred from compilation. Keep the actual DX8 renderer
and all resource/shader functions live; no no-op compatibility stubs.

This recommendation removes a concrete native link dependency group; it is
not a promise of first playable native linkage. Nine non-math D3DX helper names
and 30 tool ABI errors remain separate gates. Another isolated UI feature would
not resolve those link failures. No X64.4 work was started. Pathfinding 2.0
remains backlog; Stage4A.3 remains PAUSED.

## Exact changed-file inventory

- `Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.h`
- `Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11TextureBridge.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt`
- `Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h`
- `Core/Libraries/Source/WWVegas/WW3D2/Texture2D.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/Texture2DCPUContent.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/Texture2DCPUContent.h`
- `Core/Libraries/Source/WWVegas/WW3D2/texture.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/texture.h`
- `Core/Libraries/Source/WWVegas/WW3D2/UITextureContent.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/UITextureContent.h`
- `Tests/Google/Dependencies/CMakeLists.txt`
- `Tests/Google/Core/Libraries/D3D11UITextureTest.cpp`
- `scripts/rendering/test_d3d11_reference.py`
- `scripts/rendering/test_texture2d_reference.py`
- `scripts/rendering/test_ui_texture_reference.py`
- `docs/modernization/D3D11_2_FILE_UI_TEXTURES.md`
- `docs/modernization/RENDERER.md`
- `docs/modernization/X64.md`
- `docs/modernization/ROADMAP.md`
- `docs/modernization/AGENT_HANDOFF.md`
