# X64.9: pinned file/filter/surface characterization

Date: 2026-10-11. **Characterization complete: X64.9D closes 13/13 compressed
and 9/9 overall readiness gates. No production helper was replaced. No game
was launched. The final X64.9D section supersedes historical OPEN decisions.**

## Baseline and native preservation

Started clean on `dev/modern-engine`, tracking `origin/dev/modern-engine`,
0 ahead / 0 behind, at `3283218029fea5eb5e1785d6220a661d5d965815`
(`X64.8: replace external D3DX error strings with native CPU compatibility`).
The following initial and B/C sections record historical checkpoints. The final
D section closes the conditional acceptance gate and specifies the combined
characterization commit; its hash is available through git log.

Actual before/after bounded native full-graph and independent title probes agree:

| Diagnostic | Before | After |
| --- | ---: | ---: |
| Unique external D3DX helpers/title | 3 | 3 |
| Combined LNK2019 | 6 | 6 |
| Combined LNK2001 | 8 | 8 |
| Combined LNK1120 | 2 | 2 |
| Separate tool compiler errors | 30 | 30 |

The complete symbol occurrence counters agree, not just unique names:
`D3DXFilterTexture=6`, `D3DXLoadSurfaceFromSurface=6`,
`D3DXCreateTextureFromFileExA=2`. Neither native title links. The exact compiler
diagnostic-line multiset is unchanged: C2065=8, C2440=6, C2555=4, C2664=8,
C2737=4. Tool ABI fixes remain outside this task.

Oracle: actual x86 `d3dx8.lib` from pinned dx8 dependency commit
`7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc`; library SHA256
`39a8e21889a7c1f0b966f04a9e7d392de14ddebb3e091dfa1e5ce3e19564fc28`.
MSVC 19.51 / VS18 / SDK 10.0.26100, Win32 Release. The test-only target uses
the existing legacy link bridge and never substitutes a helper implementation.

## Frozen caller/reachability matrix

A = demonstrated game/engine route; B = public/shared API with plausible inputs
but incomplete runtime reachability proof; C = dormant/general case not shown
reachable; D = outside the production call shape. Source positions are relative
to the repository. These classes describe evidence, not permission to delete B/C.

| Route and position | Class; input and resource policy |
| --- | --- |
| `dx8wrapper.cpp:2490`, filename overload | B. Public static overload in `dx8wrapper.h:415`; default dimensions, caller mip enum, usage 0, UNKNOWN, MANAGED, BOX/BOX, key 0, null info/palette. ANSI OS filename access. No direct invocation established. Ordinary named TextureClass loading instead uses TextureLoader. Retain this public/link-visible route. |
| `dx8wrapper.cpp:2542`, surface overload | A via `TextureClass(SurfaceClass*,MipCountType)` at `texture.cpp:754`, including `bmp2d.cpp:171`'s `NEW_REF` constructor. Source descriptor dimensions/format; X64.6 allocation may negotiate destination format/dimensions; managed 2D destination, full rects, BOX. Source can include DXT: constructor explicitly sets compression allowance. |
| `dx8wrapper.cpp:2548`, same overload | A; filter source 0/BOX/null palette when requested mip enum != MIP_LEVELS_1. Requested count and actual level count are distinct. |
| `missingtexture.cpp:105`, `_Init` | A, initialization route. A8R8G8B8 requested, full mip chain; adjacent surfaces i-1 -> i, BOX/full rect. Allocation can negotiate format. Surfaces AddRef'd by GetSurfaceLevel and released individually. |
| `surfaceclass.cpp:477`, `Copy` | A. NONE/explicit rects; clips right/bottom independently to each descriptor. Same-format/same-dimension resources use wrapper CopyRects instead of this D3DX call. Other combinations enter D3DX. Null palettes, key 0. Image, texture-level and wrapped device surfaces are possible. |
| `surfaceclass.cpp:519`, `Stretch_Copy` | B. Public method; no invocation established, no proof of removability. Explicit unequal rect extents, TRIANGLE, null palettes/key 0; no local clipping. |
| `W3DTreeBuffer.cpp:193`, texture update | A. Width/height supplied by tree atlas construction at :605, A8R8G8B8 requested, all mips, level-0/BOX/null palette. Locked top level filled before filtering; release precedes filter. |
| `TerrainTex.cpp:199`, current update | A. TEXTURE_WIDTH x height, A1R5G5B5 requested, 3 mips; WorldHeightMap supplies pow2Height (:2128). BOX/0/null palette. |
| `TerrainTex.cpp:356`, old update | C. Inside `#if 0 // old version` at :206. **Not a currently compiled caller.** |
| `TerrainTex.cpp:429`, updateFlat | A. Width/height constructor requests A1R5G5B5/all mips; WorldHeightMap :2194 supplies square pow2Height. Update checks cellWidth*pixelsPerCell dimensions. BOX/0/null palette. |
| `TerrainTex.cpp:841`, AlphaEdge update | A. TEXTURE_WIDTH x pow2Height from WorldHeightMap :2141; **A8R8G8B8**, three mips at this caller (constructor default MIP_LEVELS_3), BOX/0/null palette. |

Inventory: **1 file call, 4 surface calls, 6 filter source sites of which 5
are compiled.** This corrects X64.8's source-site inventory: the old terrain
variant must not be counted as active. AlphaEdge is a 32-bit request, not a
second A1R5G5B5 texture. Most non-wrapper callers operate on managed TextureClass
resources. Requested formats do not prove actual formats on every adapter.

SurfaceClass::Copy is connected to both titles' W3DAssetManager colorization
(:649/:1537 Generals, :679/:1564 ZH), projected-shadow surface copies
(:2182/:2186), shared `bmp2d.cpp:170`, `font3d.cpp:149`, and compressed
Is_Monochrome conversion (`surfaceclass.cpp:855`). Some individual calls take
the CopyRects branch; the method's entry alone is not proof of D3DX execution.
W3DSmudge creates a POOL_DEFAULT render-target background (:89), obtains
back-buffer/texture surfaces, and calls Copy (:443). This makes device access
part of the contract even though equal descriptor cases can bypass D3DX.
SurfaceClass can wrap an existing IDirect3DSurface8; it does not guarantee a CPU
lock or tight pitch. No function-pointer/virtual dispatch proof makes the
filename/Stretch_Copy routes definitely active; both remain B.

All surface calls omit palettes and use color key 0. All filter calls use BOX,
source level 0 and null palette. Wrapper surface/mip errors are logged/asserted
through DX8_ErrorCode; they are not transactional rollback. Filename errors
return an AddRef'd missing texture; successful P8 is explicitly released and
rejected by that wrapper. Caller-owned source resources remain caller-owned.

| Input family | Required classification / evidence boundary |
| --- | --- |
| A8R8G8B8, A1R5G5B5 procedural mips; clipped NONE colorization/UI copies | A; must preserve exact bytes and partial-write behavior. |
| X8R8G8B8, R5G6B5, A4R4G4B4, R8G8B8 | A/B depending on negotiated device/asset format. Six-format matrix prioritized; no universal fallback assumption. |
| DXT1/3/5 DDS and compressed SurfaceClass conversion | A/B; explicit engine compressed routes exist. DXT1 measured here, DXT3/5 exact conversion not yet bounded. |
| L8/A8L8, palette, bump/V8U8 | B; shared format enums/loader admit them; secondary CPU probes are observations, not a complete success matrix. |
| Odd sizes, 1D, arbitrary API rects/TRIANGLE, aliasing | B; not safely removable from public contracts. Bounded samples measured; general contract unresolved. No distinct-resource production call above demonstrates aliasing. |
| Dormant terrain implementation, arbitrary cube/volume file loading | C; no active call here. |
| Nonzero color key, supplied palettes, arbitrary filter flags, source mip !=0 | D for these calls; no implementation requested. |

## Fixtures and recording harness

`legacy_resource_characterization` is Win32-only. It calls the actual pinned
helpers using SDK-exact fail-fast interfaces. No test converter supplies pixels.
The device observer explicitly reports unavailable CopyRects/staging creation;
all unobserved COM methods abort. GetDevice returns a valid balanced interface;
standalone GetContainer returns E_NOINTERFACE; texture-owned surfaces report
their actual owner. Successful locks validate rect bounds, reject nested locks,
use align4(row bytes)+8 pitch, and retain A5 padding/end guards.

Failure injection includes source/destination GetDesc, LockRect, UnlockRect and
GetSurfaceLevel failures at mip 1/2. Failed GetDesc deliberately still fills a
defined descriptor: the pinned helper otherwise reads undefined payload. An
earlier undefined-payload trial is not an accepted reference. GetDevice returning
failure/null likewise was unsafe for this helper and is not a valid-device proof.

Final default CPU evidence: **3,010 rows**: 2,690 surface operations, 312 mip
level rows (96 chains), eight partial-failure mip rows. Six primary formats,
three filters, four patterns, six dimensions = 2,592 base surface calls.
Additional rows cover rects, secondary formats, failures, all 65,536 packed words
for each of three formats, 256-value quantization ramps, 75 impulse calls and
transparent-red/opaque-blue blending. Every successful source/refcount/lock
check passes. Mip level zero is checked unchanged. Full recording rows retain
source and destination bytes, including padding and untouched regions.

Patterns: channel-distinct ramps, checkerboards, central impulses and alpha
ramps/endpoints. Shapes: 4x4->4x4, 5x3->2x1, 3x2->5x3, 1x7->1x3,
7x1->3x1, 5x3->3x2. Offset source [1,1,4,4), destination [2,1,5,4) and negative
source rects; 1D impulse maps for 3->5, 5->3, 5->2, 7->3, 3->2, 2->5.
Mip shapes 8x4, 5x3, 1x7, 7x1, each format/pattern. Dimensions are bounded
test inputs, not an assertion about adapter-wide capabilities.

Generated files: controlled bottom/top TGA, BGR24, BGRA32, grayscale type 3,
indexed type 1 with alpha palette, 1D, controlled ARGB/DXT1 DDS four-level chains,
malformed/unsupported and deliberately missing paths. Seven tracked TGAs are
read in place: AddProjectorGradient, MultProjectorGradient, w3d_missing_texture,
and W3DView cursor/grab/orbit/zoom. Tracked inventory remains 7 TGA / 66 BMP /
no DDS/JPEG/PNG/PCX; this is not a retail corpus. No retail assets were copied
or used to claim broad decode coverage.

The compact `LegacyResourceReference.jsonl` contains **58 complete deterministic
CPU rows, 40,413 bytes**. Exact JSON/pixels/transcripts are compared on every
default run, and all reference IDs must be exercised. Exhaustive packed expansion
also has whole-buffer assertions including padding. Bulk files remain ignored:
`build/x64-9-resource-evidence/{recording,device}.jsonl`, generated fixtures and
isolated file reports; logs/drivers/analysis remain `build/x64-9-*`.

## Measured pixel and sampling rules

These are measured bounds, not a claim that every possible image is characterized.

* Packed expansion: for an n-bit channel x, `floor((255*x + ((1<<n)-1)/2) /
  ((1<<n)-1))`. All 196,608 packed input words agree. R565 absent alpha is 255;
  A1555 alpha is 0/255; A4444 channels expand exactly by 17. Simple bit replication
  disagrees on 23,200 R565 words and 21,632 A1555 words.
* Packing from the 256-entry all-channel byte ramp uses
  `floor((x*((1<<n)-1)+127)/255)` for 1/4/5/6-bit channels. Truncating high bits
  disagrees at 74 R565, 52 A1555 and 56 A4444 ramp values. This establishes these
  scalar boundaries; filter accumulation order must be established separately.
* NONE performs top-left overlap transfer in unequal extents, not nearest-neighbor
  resizing. In 5->3 impulses it retains source positions 0/1/2 and ignores 3/4;
  2->5 writes the two source positions and leaves the rest untouched. Rects use
  exclusive right/bottom edges. Negative source rect returns 0x8876086c and leaves
  the prior destination unchanged. Production Copy clips before calling D3DX;
  that is distinct from helper-side invalid-rect handling.
* XRGB -> ARGB synthesizes alpha 255. ARGB -> XRGB conversion writes zero to
  the unused byte in measured cross-format cases. Same-format NONE XRGB copy
  preserves the original unused byte. Do not canonicalize every XRGB byte.
* BOX is not a universal area average. At 5->2 its 255 impulses produce
  [128,0], [128,0], [0,128], [0,128], [0,0]: the final odd source pixel is omitted
  on this route. At 3->2 BOX and TRIANGLE both yield [149,21], [85,85], [21,149].
* At 5->3 BOX and TRIANGLE both yield [134,0,19], [100,53,0], [2,149,2],
  [0,53,100], [19,0,134]. Opposite-edge contributions rule out an assumed clamp
  kernel. At 5->2 TRIANGLE instead yields [89,13], [102,0], [51,51], [0,102],
  [13,89]. These exact measured maps are retained; filter labels are not synonyms.
* BOX transparent red + opaque blue -> one pixel is BGRA `80 00 80 80`.
  Color is averaged independently of alpha in this case, not premultiplied.
  It does not establish arbitrary compressed premultiplication semantics.

**Unresolved required contract:** precise general BOX route selection and
TRIANGLE coefficient/coordinate/edge/rounding rules, especially odd extents and
separable 2D mixed-channel accumulation. Impulses alone do not prove rounding on
combined inputs. The measured opposite-edge contributions cannot be replaced
with an intuitive bilinear/area filter. General compressed conversion/encode
rules (particularly DXT3/5), aliasing and complete staging-failure behavior are
also unproven. No generic converter or resampler was written.

## Mips, file decode, device access and failures

Recording confirms D3DXFilterTexture walks adjacent levels (0->1->2->3), not
independent resampling from level zero. It uses the same source-lock then
destination-lock/convert/unlock route as adjacent BOX surface copies, releases
the prior surface at each step and releases the last surface. Source zero is
unchanged. Packed quantization therefore happens at each intermediate level.
Mip dimensions floor-divide with minimum 1 in the measured chains.

GetSurfaceLevel failure at 1 leaves lower levels untouched; failure at 2 leaves
level 1 generated and later levels untouched. E_FAIL propagates and acquired
interfaces are released. Source lock failure stops before destination access;
destination lock failure releases the source lock. Defined-payload GetDesc
failure and UnlockRect E_FAIL are **ignored** on the measured CPU conversion
route (S_OK and completed writes). No universal failure-normalization is allowed.
CPU source locks use flags 2064, destinations 2048; destination unlock precedes
source unlock. Equal-size/equal-format transfers can first attempt device
CopyRects; failure then falls back to CPU access. Failure transcripts are part
of the compact reference, except unsafe/undefined-payload exploratory cases.

Real Win32 test uses system d3d8.dll, hidden 64x64 window, HAL software vertex
processing and FPU_PRESERVE. Actual adapter: Intel Graphics,
`igd9trinity32.dll`, vendor 32902, device 32069, driver version integer
9007199261369161. Real output is **371 rows** including adapter identity:
60 file calls, 146 file mip rows, 108 copies, 40 readable mip rows,
three texture-creation failures, five initialization-lock failures, three
device-copy rows, one P8 policy row and four compressed-filter rows.

* 51 file calls succeed; missing, malformed and unsupported files (three mip
  requests each) return 0x88760b59 and leave the pre-existing output pointer and
  its refcount unchanged. Success replaces the pointer without releasing the
  old resource; new texture AddRef/Release reports 2/1. A separate sequential
  AddRef/Release avoids unspecified expression evaluation order in the harness.
* Default dimensions retain 5x3, 1x7, 7x1 on this adapter; mip request 0 gives
  full chain, 1 gives top only, 3 gives three available levels. BGRA TGA chooses
  A8R8G8B8, grayscale L8, indexed input expands to A8R8G8B8 here. Bottom-origin
  input is inverted vertically; top-origin is retained. Alpha endpoints survive.
* Explicit requested P8 succeeds but actually chooses **A8R8G8B8 (21)** here.
  A nonnull output does not prove P8 support; production's P8 rejection remains
  required but this adapter did not exercise an actual P8 result.
* Controlled ARGB DDS keeps authored mip bytes. DXT1 retains format and level
  dimensions, but low 2x2/1x1 block bytes are canonicalized; never promise a raw
  byte copy of every source block. Subsequent BOX filtering regenerates lower
  compressed levels and changes bytes. Exact bounded outputs are in device.jsonl;
  a general DXT encoder contract has not been established.
* All **108** real system-memory copy cases exactly match CPU active pixels.
  All **40** readable managed/system-memory mip levels match CPU active pixels.
  Device rows contain tightly concatenated active pixels plus the actual observed
  pitch; recording rows contain padded allocation bytes. Comparison uses the
  appropriate layout rather than treating both as pitched dumps.
* R8G8B8 texture creation fails in all three tested pools. Five other default-pool
  textures create but cannot be initialized by LockRect (0x8876086c). These
  resources are not filtered/read as though initialization succeeded.
* Back-buffer -> system-memory and non-lockable render-target -> system-memory
  full NONE copies succeed; every read pixel matches the cleared color.
  System-memory -> non-lockable target also returns S_OK. This is bounded
  constant-color/access evidence; arbitrary rect/scaled/device failure policy
  and post-write target readback are not thereby proven.

**Unsafe isolated oracle case:** a generated type-10 RLE TGA with one raw packet spanning rows returns
0x88760b59 then terminates with 0xc0000374 heap corruption in each of three
isolated mip-request processes. Cause inside the pinned decoder is not claimed
proven. Files/logs remain under `isolated/`. It is excluded from default in-process
tests, explicitly selectable using X64_9_FILE_CASE. This does not justify changing
production decode behavior, treating all RLE as unsupported, or copying the crash
into a replacement. A separate row-bounded raw-packet RLE TGA succeeds for all three mip requests
and is included in default coverage. Packet layout is therefore a measured
distinction; do not generalize the unsafe result to all type-10 input.
No tracked RLE fixture or definitely active filename-wrapper
caller was demonstrated, so this anomaly alone is not the production blocker.

## Shared pipeline and existing-code reuse

| Candidate | Classification and reason |
| --- | --- |
| X64.6 LegacyTextureCreation negotiation/device creation | Directly reusable for already-proven allocation/caps policy. Does not decode/convert/filter or establish lockability. |
| D3D11.2 CPU storage, deterministic fixture patterns | Reusable after extension/test support for pitched CPU data and bounded inputs. No D3D11 resource ownership belongs in DX8. |
| UITextureContent TGA/DDS parser | Useful reference/test support; extension required. Power-of-two >=4, single retained top mip, narrow formats and engine file/fallback policy differ from D3DX OS filenames, L8/palette/odd dimensions/authored chains. |
| UITextureContent 16-bit pack | Not equivalent: high-bit truncation differs from measured nearest scaled D3DX packing. |
| SurfaceClass locking/refcounts | Existing caller contract/reference support; cannot guarantee lockability or replace helper staging. Preserve independent clipping and CopyRects branch. |
| TextureLoader/Targa/DDSFileClass | Reference support / extension candidates. Asynchronous/reduction/format/compression/asset lookup policy is different; do not redesign loader to satisfy this task. |
| BitmapHandler Read/Write/Combine/mip routines | Not demonstrated equivalent. Fixed 2x2 loops, HSV behavior and packed conversion/rounding must not be substituted on similarity alone. |

Later implementations can share proven unpack/pack scalars and adjacent-level
iteration, and eventually a proven CPU resampler/access layer. Helper-specific
file format negotiation, authored-chain handling, rect validation, surface/device
access, ownership, P8 rejection and failure/partial-write behavior must remain
separate. Current evidence does **not** justify one combined migration yet.

## Regression actually run

| Check | Result |
| --- | --- |
| Full Win32 Release graph | PASS on final observer source |
| Win32 CTest | 20/20 PASS, new target contains two passing tests; no real-device skip |
| Direct title tests | 149/title PASS; existing six disabled/title |
| Focused renderer/path/profiler/developer/Bezier | 75/title PASS |
| Legacy math/reference, FVF/reference, texture/reference/device, shader/reference/device, error-string tests | PASS through CTest |
| RB1/RB2/RB3, producer, D3D11.1 sentence and D3D11.2 UI/WARP | PASS Win32; independent AMD64 CTest 10/10 PASS |
| Rendering / performance Python | 28/28 and 16/16 PASS |
| PowerShell parser | Nine scripts PASS |
| Preservation audit | PASS: 391 prior runtime/evidence files, accepted A.2 hashes, empty index; existing gated-adapter safety checks PASS |
| Native graph / separate title probes | Expected failures; exact before/after multisets above |
| git diff --check | PASS |

No production source, build configuration for production targets, resource
helper, gameplay/timing/pathfinding/network/save contract or tool ABI changed.
DX8 remains default; no D3D11.3 work. Accepted runtime/capture files remain local
and ignored, including stage4a2-B-final-2 and B-cpu.etl. No generated evidence is
staged. Test-generated outputs are confined to the new ignored evidence folder.

## Reproduction and readiness decision

From the repository root, use the existing configured Win32 Release environment:

```powershell
cmake --build build/win32 --config Release --target legacy_resource_characterization
ctest --test-dir build/win32 -C Release -R '^legacy_resource_characterization$' --output-on-failure
```

This generates recording/device JSONL and controlled fixtures under ignored build.
Seven repository assets are read without modification. The optional unsafe RLE
case must run in a separate process, outside CTest; do not put it into an automated
successful regression sequence. Set X64_9_FILE_CASE to `rgba-rle.tga-0`, `-1` or
`-3` and select `*.RealWin32DeviceFilesCopiesAndMips` only if investigating that
specific oracle failure. Clear the environment variable for normal coverage.

**Decision C: additional characterization. Exactly one next task:** complete
the resource-group sampling/access contract before any migration. Use this same
oracle harness, not a new implementation: bound BOX fast-path dispatch and the
general TRIANGLE coordinate/edge/rounding path with mixed-channel 2D threshold
cases and independent pinned-code inspection; extend only the demonstrated
compressed SurfaceClass path (DXT3/5) and scaled/non-lockable staging boundaries
needed by that matrix. Resolve whether plausible public aliasing needs support
or can be excluded by a documented call contract. Preserve the RLE anomaly as a
separate uncertainty unless reachability makes it required.

The immediate blocker is an exact general filter/access contract, not symbol
count, build progress or lack of a visual game test. Do not implement all possible
D3DX formats, migrate a guessed subset, or start X64.10 implementation. The
existing evidence already fixes packed conversion mistakes and highlights odd-edge
behavior; repeating bulk fixtures without resolving those rules is unnecessary.

## Exact changed-file inventory / disposition

* Tests/Google/Dependencies/CMakeLists.txt
* Tests/Google/Core/Libraries/LegacyResourceCharacterization.h
* Tests/Google/Core/Libraries/LegacyResourceCharacterizationTest.cpp
* Tests/Google/Core/Libraries/LegacyResourceReference.jsonl
* docs/modernization/X64_9_RESOURCE_CHARACTERIZATION.md
* docs/modernization/X64.md
* docs/modernization/RENDERER.md
* docs/modernization/ROADMAP.md
* docs/modernization/AGENT_HANDOFF.md

All nine files remain unstaged for developer review. Commit SHA/subject: none.
Push: not attempted. Baseline remains HEAD. Characterization gates have not all
passed, so the user's conditional commit/push instruction has not been invoked.

## X64.9B continuation (2026-10-11): scope and preserved evidence

This section supersedes the initial X64.9 readiness/disposition above; the initial
measurements and assertions remain preserved. This is characterization, not a
compatibility implementation. No production helper or caller was changed.

Before extension, the original two-test target passed again. HEAD and origin
remain `3283218029fea5eb5e1785d6220a661d5d965815`, branch `dev/modern-engine`,
0 ahead/behind, empty index. The initial five modified/four untracked inventory
was inspected. No generated build evidence is tracked. Initial worktree hashes
and copies of the original recording/device output were saved under ignored
`build/x64-9b-evidence`. After extension, the original 3,010 CPU and 371 device
JSONL records are **byte-for-byte identical** to those saved outputs. The old
58-row reference file and its assertions were not replaced or weakened.

### X64.9B pinned-code inspection and confidence

Inspected the actual pinned `build/win32/_deps/dx8-src/d3dx8.lib`, dependency
commit `7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc`, SHA256
`39a8e21889a7c1f0b966f04a9e7d392de14ddebb3e091dfa1e5ce3e19564fc28`.
MSVC lib/dumpbin extracted/disassembled only `cd3dxblt.obj`, `cd3dxcodec.obj`,
`cd3dxlock.obj`, and `d3dx8tex.obj`; generated objects/listings remain ignored.

Confidence labels below are deliberate: **observed** means exact behavioral
output/transcript; **code-supported** means an identified branch/instruction in
this binary; **inferred** does not constitute a proven compatibility rule.

| Binary routine | Code-supported finding |
| --- | --- |
| D3DXLoadSurfaceFromSurface, offsets 0..1A3 | Except BOX, equal rect extents with null key/matching palettes can attempt device CopyRects before CPU work. Successful CopyRects returns immediately; failure falls through. BOX skips this outer attempt. |
| CD3DXBlt::Blt, offsets 0..117 | Same/copy/NONE/POINT handlers precede BOX2D/3D, linear, then TRIANGLE2D/3D. BOX is not universally an area box filter. |
| BltBox2D, offsets 20..C0 | Each destination axis must equal floor(source/2), or both lengths be one. Odd source lengths greater than one are trimmed to even. Both depths are one. |
| TF_SetupTriangle, offsets 0..2BC | Ratio, half-pixel coordinates, integrated ramp contributions, edge folding, coefficient coalescing and small-weight cutoff are generated with x87 operations and explicit float stores. |
| BltTriangle2D, offsets 0..3A1 | Decodes source rows to D3DXCOLOR; combines horizontal/vertical coefficient products into four independent float accumulators; stores after contributions and encodes destination rows. |
| CD3DXCodec_A8R8G8B8::Encode, offsets 4E..11D | Temporarily selects x87 truncation, multiplies by 255, adds selected bias, stores float before integer conversion, and restores the control word. The inspected `_g_fDitherOff` data are sixteen exact 0.5 floats. |
| CD3DXLockSurface::Lock/Unlock | Owned DEFAULT-pool textures can use full-resource system-memory staging, preservation/readback CopyRects, CPU locking, and upload CopyRects. Unlock ignores the final upload result. |

Standalone DEFAULT surfaces have a legacy private-layout check if GetContainer
fails. **Do not copy that private-layout dependency into a replacement.** Raw
real interfaces were used for standalone/back-buffer experiments. The forwarding
observer is restricted to an owned DEFAULT texture where GetContainer succeeds.

### X64.9B BOX and TRIANGLE contract

**Observed:** 78 targeted mixed-channel threshold operations cover 13 ratios,
three input patterns and BOX/TRIANGLE. Shapes are 4->2, 5->2, 4->3, 5->3, 3->5,
6->4 horizontally; 4->2, 5->3 and 3->5 vertically; and true 2D 4x4->2x2,
5x3->3x2, 4x6->3x5 and 3x5->6x4. Fixtures deliberately use values
0,1,2,3,7,8,15,16,31,32,63,64,126,127,128,129,254,255; channel/axis-asymmetric
ramps; transparent white versus opaque black and low-bit checker patterns.
Inputs, descriptors, HRESULT, complete output allocations including unchanged
padding/guards and access transcripts are recorded exactly.

**Code-supported BOX rule:** keep the early equal-size/copy routes; otherwise
use the special half-size BOX path only for the dimensions above. That path
omits the last odd row/column. Same-format native packed/ARGB fast paths are
selected when both source axes are at least two and dithering is absent.
Integer fast paths add a half-divisor bias before shifting, equivalent to
`(sum+2)>>2` per native channel over a 2x2 block. One-dimensional half-size
and format-converting paths use the generic codec route. Other production BOX
ratios fall through to the general triangle machinery. Do not substitute a
universal area-average kernel or assume 5->2 includes source pixel four.

**Code-supported general TRIANGLE rule:** for source count S and destination
count D, the table generator uses r=D/S and two source-basis halves starting
at i-0.5 and i+0.5, mapped into destination coordinates. It integrates linear
ramps over overlapped destination pixel bins: a basis centered at
`(i+0.5)*r`, with support radius r. This is not point-sampled bilinear resizing.
For the production flags, out-of-range destination indices fold by D, explaining
the measured opposite-edge contribution. The alternate edge branch must not be
silently substituted. Adjacent contributions to the same index are coalesced;
the inspected coefficient cutoff is float bits `0x3727c5ac` (about 1e-5).
There is no justification for an extra per-output normalization division or a
different edge renormalization policy.

Preserve coefficient construction, table iteration, accumulator store points and
final rounding order; algebraically equivalent arithmetic need not be bitwise
equivalent. x87 intermediate arithmetic must not be casually replaced by a
different SSE/double summation order. The bounded reference corpus is an exact
acceptance oracle for a later implementation, not proof of every possible size.
BOX and TRIANGLE can share the general fallback coefficient/accumulation route,
but BOX requires distinct dispatch, odd-edge trimming and native averaging paths.

### X64.9B mixed-channel/2D precision and packed interaction

**Observed:** all 24 true-2D results differ from a deliberately byte-materialized
horizontal-then-vertical two-call experiment. This alone is **not** proof that
horizontal quantization causes every difference: a separate helper call can
also filter the unchanged axis, so the experiment has that dispatch confound.

**Code-supported:** the actual general 2D route multiplies x/y coefficients and
accumulates channel contributions into D3DXCOLOR fields, storing float32 after
individual contributions. It has no byte-quantized intermediate horizontal
image. Four channels are handled independently, including alpha; the measured
transparent-color cases and the inspected route have no premultiplication.
Final codec quantization follows accumulation. Adjacent mip levels still
quantize/re-encode before becoming the next source, as already established.

Sixty direct packed-filter cases were compared with sixty deliberate
pack->expand to ARGB8->filter->repack operations. Three differed:

| Direct case | Direct active bytes | Byte-intermediate active bytes |
| --- | --- | --- |
| R5G6B5, 4x4->2x2, pattern 0, BOX | `43728d49ec68a543` | `436a8d49cc68a543` |
| R5G6B5, same shape/pattern, TRIANGLE | `056aeb494a61074b` | `256aeb494a61074b` |
| A1R5G5B5, same shape/pattern, BOX | `2339cd246c34c521` | `2335cd246c34c521` |

This falsifies using rounded 8-bit expansion as a universal filtering
intermediate. **Code-supported:** general packed decode normalizes native fields
directly (e.g. R/B /31 and G /63 for 565); the BOX fast paths average native
fields, including native alpha fields. General encode quantizes to the target
field width after filtering. A4444 agreement in these fixtures does not justify
replacing its native route with an unproven byte-intermediate route. Earlier
nearest-scaled scalar pack/unpack evidence remains valid for those conversions.

### X64.9B DXT3/5 boundary: remaining implementation blocker

Production reachability is unchanged: the SurfaceClass-based TextureClass
constructor retains compression allowance and can preserve a compressed source
format. The surface overload copies with BOX and then filters mip levels.
Therefore compressed output/re-encoding cannot be safely excluded merely because
many ordinary engine DDS loads use a separate loader.

Controlled literal DXT3/5 blocks cover all sixteen DXT3 alpha nibble values,
both DXT5 endpoint orderings and all eight DXT5 alpha codes, red/green RGB565
endpoints and asymmetric color indices. DXT3's order labels intentionally use
identical blocks; they are not two independent alpha-order cases. Seventy-two
decode/conversion operations cover ARGB8/A4444, NONE/BOX/TRIANGLE, 8x8->8x8,
8x8->3x2 and odd 5x3->3x2. All controlled-interface operations return S_OK.
Forty-eight aligned compressed-source cases match the real device exactly;
the adapter rejects the 24 odd-source compressed image creations with
`D3DERR_INVALIDCALL`. This does not invalidate the controlled odd-block evidence
or prove real odd-sized resource support.

Four controlled and real 8x8 compressed mip chains retain/read four levels
(8x8,4x4,2x2,1x1), producing **sixteen exact CPU/device compressed-byte
agreements**, all S_OK. Lower levels re-encode; they are not authored-byte copies.
For example DXT5 endpoint order 240/16 produces 4x4 block
`a96e766687f6622fe083007ceebbeebb`, then 2x2 block
`947f799ee7799ee70084e07beebbeebb`. Small mip storage remains one full block.

These are bounded encoder examples, **not a sufficiently specified encoder**.
The inspected codec calls D3DXEncodeDXT1/3/5; endpoint fitting, selector tie rules,
degenerate blocks, RGB/alpha coupling and small-block extension have not been
derived and independently distinguished. Matching these few structured blocks
cannot establish exact encoding for arbitrary production content. Replacing it
with another nominally valid DXT compressor would guess output bytes. Gate 5
remains NO. No final kernel, compressor or helper migration was implemented.

### X64.9B non-lockable staging/access

Six scaled/partial owned render-target cases exercise target->system,
system->target and target->itself, BOX/TRIANGLE, source rect (1,2)-(7,7),
destination rect (2,1)-(5,3), with asymmetric real pixels. All S_OK; whole
destinations, including unchanged regions, agree with directly lockable oracle
inputs containing the exact pre-operation bytes. Eight further raw cases cover
back buffers and standalone non-lockable render targets in both directions,
using the same scaled rects/filters: all S_OK and exact whole-destination CPU
agreement. Raw actual interfaces preserve the oracle's standalone access policy.

Eight forwarding-observer cases characterize the owned DEFAULT-texture route
without inventing successful resource access. Successful allocation/CopyRects
are forwarded to the actual HAL device; only designated failures are injected.
The represented texture's true descriptor and real container are used, original
LockRect is rejected, and CopyRects unwraps only that represented original.

| Failure | Observed result / state |
| --- | --- |
| Stage CreateImageSurface returns E_OUTOFMEMORY | Same HRESULT, no CopyRects, destination unchanged. |
| First CopyRects returns E_FAIL | E_FAIL; source readback or destination-preservation failure leaves destination unchanged. |
| Final destination upload (second CopyRects) returns E_FAIL | Helper returns S_OK, original destination unchanged. This ignored error is also code-supported by Unlock. |
| No designated failure | Source read uses one full-resource staging copy; partial destination write uses preservation copy then upload; S_OK. |

Stage allocation uses full 8x8 resource dimensions and the original format,
not merely the scaled rect dimensions. Recording surface/device refs return to
one; represented surfaces remain unlocked; real owner/device AddRef/Release
counts return to their pre-operation values. This confirms cleanup for the
measured success/failure paths, not arbitrary driver internals. Existing outer
CopyRects fallback/lock/unlock/GetSurfaceLevel partial failure assertions remain.

### X64.9B aliasing and RLE disposition

Aliasing **cannot be excluded**: public Stretch_Copy accepts `other==this`,
and independently created SurfaceClass wrappers may retain the same interface.
There is no ownership guard preventing this. Same-shape ordinary Copy uses the
engine CopyRects branch, but that does not exclude Stretch_Copy aliases.

Nine real SYSTEMMEM ARGB8 cases cover full equal rects, overlapping 6x6 source
at (0,0) to destination (2,2), and full 8x8 to destination (1,1)-(5,5), each
NONE/BOX/TRIANGLE. All S_OK. Each complete output equals the corresponding
operation using a separate source snapshot; outside destination rects is
unchanged. Three equal-size cases write no changed pixels, three overlap cases
change 36 pixels, three scaled cases change 16. Owned render-target self-scaling
also matches snapshot-based CPU output above. A later access layer must preserve
these alias results rather than taking two simultaneous unsupported locks.

The type-10 packet-spanning TGA failure remains isolated and is never executed
by ordinary CTest. The source/reachability audit still shows the filename wrapper
as a public/plausible B route, without a demonstrated active type-10 asset or
caller requirement. The separate engine Targa loader's RLE capability does not
prove use of the D3DX filename wrapper. Row-bounded RLE remains a successful
measured case. No broad decoder reverse engineering was undertaken; do not
reproduce the heap corruption. Gate 8 refers to the frozen required caller/file
matrix, not a universal public D3DX image-format promise.

### X64.9B compact references and real cross-check totals

`LegacyResourceSamplingReference.jsonl` adds **94** exact, generated-input rows:
42 threshold, 24 direct/byte-intermediate packed contrasts, four two-pass
contrast rows, eight odd compressed-source decode/filter rows, sixteen compressed
mip rows. The original 58-row file is untouched. All input construction lives
in the test source; no retail assets are copied into the reference data.
Each reference retains exact descriptors/HRESULT/source/output storage and
transcript. Each map is bounded to 128 rows / 16 KiB per line and rejects
duplicates/missing files. The new test requires exactly 94 reference hits.

New ignored `sampling.jsonl`: **334** rows (318 surface operations plus sixteen
compressed mip records). New ignored `sampling-device.jsonl`: **380** rows,
including 294 supported surface agreements, 24 creation limitations, sixteen
compressed mip agreements, nine aliases/nine snapshot comparisons, six staged
results/six pre-operation destination records, eight failure-policy cases and
eight raw scaled-access cases. Device outputs keep actual pitch; CPU references
also retain padding and guards. These counts are evidence coverage, not a reason
to add more bulk dimensions. The resource target now has four passing tests,
with no device skip on this adapter.

### X64.9B final readiness answers

YES entries are limited to the frozen production formats, flags and caller
matrix, and require later exact reference tests; they are not universal D3DX
format/driver promises.

| # | Question | Answer / basis |
| --- | --- | --- |
| 1 | General production BOX specified? | YES for uncompressed sampling: special half-size dispatch/odd omission/native averaging versus triangle fallback, targeted exact fixtures and real agreement. Compressed encoding is separately blocked by 5. |
| 2 | Production TRIANGLE specified? | YES: integrated coefficient generation, coordinate/support/edge branch, float accumulation and final quantization route are code-supported and threshold-tested. Preserve operation/store order. |
| 3 | Mixed-channel 2D rounding specified? | YES: independent float fields and joint coefficient products, final encode rather than a byte-materialized horizontal image; measured mixed inputs. Two-call differences alone are not used as proof. |
| 4 | Packed filtering interaction specified? | YES: native-field normalization/fast averaging and target quantization, with exact counterexamples to ARGB8-intermediate filtering. |
| 5 | Required DXT3/5 specified or excluded? | **NO.** Decode and bounded encoded mip bytes agree, but required exact compressed encoder decision rules remain unbounded; re-encoding is reachable. |
| 6 | Scaled/non-lockable staging specified? | YES for required measured access classes: real owned/default, standalone target and back buffer; scaled/partial pixels, allocation/readback/upload failures and cleanup. No private-layout assumption is proposed for a replacement. |
| 7 | Aliasing characterized/excluded? | YES, characterized rather than excluded: full/overlap/scale equal snapshot output, real SYSTEMMEM and owned-target self-scale. |
| 8 | File decode bounded despite RLE? | YES for the frozen required matrix and quarantined anomaly disposition; not a claim of arbitrary decoder equivalence. |
| 9 | Final helpers implementable without guessed pixel/device semantics? | **NO**, because of required compressed re-encoding in 5. |

**Gate remains OPEN. No commit or push.** Do not start X64.10. Exactly one next
task: **close the required compressed-output contract in this same harness**.
Inspect the pinned D3DXEncodeDXT1/3/5 route actually reached by compressed
SurfaceClass copies/adjacent mips, then add only block fixtures distinguishing
its endpoint/selector/tie/alpha and small-mip extension decisions. Determine
whether that bounded production encoder can be specified exactly; if not, leave
the migration blocked. Do not repeat broad uncompressed sampling, request game
validation, redesign loading or substitute a generic DXT compressor.

### X64.9B final regression and unchanged native/tool state

Full Win32 Release PASS; CTest 20/20 PASS (resource target four tests, no skip);
direct 149/title PASS, six pre-existing disabled/title; focused 75/title PASS.
Legacy math/reference, FVF/reference, texture/reference/device,
fixed-shader/reference/device, error strings, RB1/2/3, producer and D3D11.1
sentence/D3D11.2 UI passed through the accepted CTest/direct sequence.
Independent AMD64 CTest 10/10 PASS. Rendering Python 28/28, performance Python
16/16, PowerShell parsing nine scripts and preservation audit PASS. The audit
checks 391 prior runtime/evidence files, accepted A.2 capture hashes and empty
index. `git diff --check` PASS. No Generals/Zero Hour process was launched.

Native full graph still fails as expected; separate title probes retain exactly
three unique helpers/title: D3DXCreateTextureFromFileExA, D3DXFilterTexture,
D3DXLoadSurfaceFromSurface. Exact diagnostic-line multisets match the saved
baseline: combined LNK2019=6, LNK2001=8, LNK1120=2. Separate tool compiler
multiset remains exactly 30: C2065=8,C2440=6,C2555=4,C2664=8,C2737=4.
No tool ABI repair or native-link progress is claimed.

### X64.9B final combined inventory and disposition

Five modified tracked files: `Tests/Google/Dependencies/CMakeLists.txt`,
`docs/modernization/AGENT_HANDOFF.md`, `RENDERER.md`, `ROADMAP.md`, `X64.md`.
Five untracked intended files: `Tests/Google/Core/Libraries/LegacyResourceCharacterization.h`,
`LegacyResourceCharacterizationTest.cpp`, `LegacyResourceReference.jsonl`,
`LegacyResourceSamplingReference.jsonl`, and
`docs/modernization/X64_9_RESOURCE_CHARACTERIZATION.md`.
All ten remain unstaged. No production source changed. Original captured/runtime
evidence remains local and ignored, including `stage4a2-B-final-2` and the
2,469,396,480-byte `B-cpu.etl`. Generated JSONL, extraction/disassembly, fixtures,
analysis and logs are under ignored build paths. Commit SHA/subject: none;
conditional acceptance failed at 5/9. Push: not attempted for the same reason.

## X64.9C compressed encoder continuation (2026-10-11, review pending)

**Readiness remains OPEN.** This continuation establishes the actual encoder
route, quantization/diffusion stages, transparency, small-mip extension and
float handoff, but does not yet specify an independently reproducible complete
RGB/DXT5 endpoint and selector contract. Reference agreement is not a proof of
an exact encoder. No production helper was replaced; no commit or push was
attempted. Earlier sections retain their historical checkpoint results.

### Preserved baseline and earlier evidence

HEAD and origin/dev/modern-engine remain
`3283218029fea5eb5e1785d6220a661d5d965815`, branch `dev/modern-engine`, upstream
`origin/dev/modern-engine`, 0 ahead/behind. The index is empty. Before extending
the tests, the complete tracked diff and ten-file inventory were inspected and
the four existing resource tests passed without skips. Their evidence streams
were saved under ignored `build/x64-9c-evidence` for byte comparison.

After the final regression, all four streams are byte-identical: 3,010 original
CPU, 371 original device, 334 sampling CPU and 380 sampling device/access rows.
The original 58 compact rows and B's 94 compact rows have unchanged SHA256s:
`8a6e6a0ec00b64348febe6b9b8a7728d38307ac8566d2f6dc53a923d2ca961a6` and
`896ea0963c415c7105dfcf4d33e3b1371908d80619a284ee3137ab71549dfc81`.
The recording-interface header and existing CMake registration are unchanged
from the preserved B checkpoint. No previous assertions were removed or relaxed.

### Production reachability and pinned binary

Re-read `texture.cpp`, `TextureClass::TextureClass(SurfaceClass*, MipCountType)`:
it retains the source format and enables compression for DXT sources. The
surface overload of `DX8Wrapper::_Create_DX8_Texture` obtains that descriptor,
creates the texture through the accepted native texture-creation policy, copies
level zero with `D3DXLoadSurfaceFromSurface(... D3DX_FILTER_BOX, 0)` and null
rectangles/palettes, then calls `D3DXFilterTexture(... D3DX_FILTER_BOX)` unless
one mip was requested. Adjacent compressed levels therefore require encoding.

DXT1, DXT3 and DXT5 can each be retained when `CheckDeviceFormat` accepts the
requested format for the actual adapter/device/display mode, usage 0 and
`D3DRTYPE_TEXTURE`, and resource creation succeeds. They are not unconditionally
converted to ARGB8. Unsupported requests follow the accepted format-rank/table
fallback. Caps constrain dimensions/aspect/power-of-two and mip availability;
compressed top dimensions round to multiples of four. A supported texture's
lower logical mip surfaces still reach 2x2 and 1x1. No arbitrary compressed API,
premultiplied DXT2/4 encoder contract or unsupported odd top-level creation is
added to this scope.

Oracle: `build/win32/_deps/dx8-src/d3dx8.lib`, dependency revision
`7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc`, SHA256
`39a8e21889a7c1f0b966f04a9e7d392de14ddebb3e091dfa1e5ce3e19564fc28`.
The actual `dxtn.obj` and prior codec/blitter objects were inspected with the
VS dumpbin tool; extraction/disassembly stays ignored. Test-only declarations
call the pinned public-in-COFF encoder symbols, not a new replacement routine.

Code-supported call graph:

```text
resource helper / float blitter -> CD3DXCodecDXT::Encode
  -> cached 16 x D3DXCOLOR block -> format-selected encoder pointer
  D3DXEncodeDXT1 -> EncodeDXT1(alpha-test=true)
  D3DXEncodeDXT3 -> alpha nibble diffusion -> EncodeDXT1(false)
  D3DXEncodeDXT5 -> alpha byte diffusion -> EncodeDXT1(false)
                   -> OptimizeAlpha -> alpha selectors/diffusion/packing
  EncodeDXT1 -> RGB565 preconditioning/diffusion -> weighted OptimizeRGB
             -> inverse weighting / Encode565 -> order / color selectors
  OptimizeRGB / OptimizeAlpha -> F2IBegin / F2I / F2IEnd as applicable
```

### Deliberate fixture coverage and compact corpus

There are 48 byte-source distinguishing blocks: constant colors, reversed
asymmetric two-color frequencies, three/four colors, near-equal endpoints,
isolated outlier, weighted axes, channel gradients, checker, gray projection
thresholds, transparency and constant/mixed/asymmetric alpha. Each is encoded
as all three formats directly and through an actual BOX surface copy, with
exact agreement. Direct input records retain all 256 RGBA binary32 input bytes.
Surface records retain exact byte storage, dimensions, descriptors, pitch,
HRESULT, output including guards/padding, filter and access transcript.

Purposeful numerical coverage adds 375 direct float fixtures: immediately
below/at/above every 31 red, 63 green and 31 blue RGB565 rounding boundary.
These exercise the composed encoder, not an isolated exported Encode565.
There are three exact DXT1 alpha-half threshold fixtures and 45 direct float
fixtures below/at/above every DXT3 nibble midpoint. All three formats are recorded
for each, giving 471 general fixture blocks. Additional DXT3-only tests cover
all 256 scalar byte alphas and 32 deterministic mixed validation blocks. These
validation blocks were not used to derive the checker. Eighteen small-mip
cases and nine float/byte boundary contrasts complete the targeted matrix.

New ignored `encoder.jsonl`: **1,899 rows**, comprising 1,710 direct records and
189 surface operations (144 equal-size, eighteen small, nine direct boundary
and eighteen byte-intermediate stages). `encoder-device.jsonl`: **171** supported
real-device agreement records, no creation limitation or skip on this adapter.
The old evidence files remain separate and unchanged.

New `LegacyResourceEncoderReference.jsonl`: **128** deliberately selected exact
rows, SHA256 `c6dca00672df429f78857f1f1de5f1bf5d5202d6d8d99aba94cb93fd02028176`.
Selection: 19 DXT1 color decisions, six shared DXT3/5 color decisions, nine DXT1
alpha decisions, fourteen DXT3 alpha decisions, seventeen DXT5 alpha decisions,
27 representative RGB565 boundary sides, nine alpha midpoint sides, eighteen
small extensions and nine filter-boundary rows. The exhaustive sweeps remain
ignored. Loading rejects missing files, duplicate IDs, overlong lines and more
than 128 rows; the test requires exactly 128 rows and exactly 128 reference
hits. Generated IDs were also checked unique (1,899/1,899). No retail assets.

### RGB endpoint fitting, quantization, order and selectors

**Code-supported:** the encoder first quantizes RGB to normalized 5/6/5 native
fields with raster error diffusion. It does not simply choose the source's two
extreme byte colors. Diffusion uses right 7/16, next-row left 3/16, down 5/16,
next-row right 1/16, without crossing a row edge. Weighted input then enters
`OptimizeRGB`. The actual weight binary32 words (RGBA) are
`3e981530,3f800000,3dce6734,3f800000`; inverse words are
`4057762e,3f800000,411ec1dd,3f800000`.

`OptimizeRGB` computes weighted channel bounds, evaluates four signed axis
orientations about their midpoint, selects an orientation and performs up to
eight projection/refinement iterations using three- or four-step coefficient
tables. Tiny-span exits and component convergence tests exist. Its convergence
epsilon is binary32 `37800000` (1/65536). This supports a weighted bounding-axis
initialization plus iterative refinement, rather than a bare min/max, luminance
extreme or generic principal-axis compressor specification. Exact numerical
orientation ties, derivative updates, x87 intermediate/store order and all
degenerate exits have **not** been independently validated. Naming the algorithm
does not close endpoint fitting.

**Code-supported and threshold-observed:** final Encode565 clamps each endpoint
component to [0,1], applies `component*31+0.5` (red/blue) or `*63+0.5` (green),
stores binary32 and converts under x87 truncate mode. It restores the prior
control word. Replacing this with byte expansion/nearest modern rounding is not
justified. In four-color mode endpoints are ordered descending; equal quantized
endpoints take a special all-zero-selector return. Three-color DXT1 orders them
ascending. Stored endpoint bytes are little endian.

The selector route projects onto the weighted endpoint axis, rounds/clamps the
step index, remaps and diffuses residual error in raster order. Extracted step
maps are `[0,2,3,1]` (four) and `[0,2,1]` (three); selectors pack two bits per
pixel row-major into the final little-endian word. It is not a naive independent
nearest decoded RGB palette decision. Gray equidistance-side fixtures and
reversed-frequency fixtures preserve distinguishing outputs, but exact
projection/store/tie behavior across arbitrary required blocks remains **NO**.
There is no independent RGB encoder/checker yet.

**Observed:** constant opaque gray 127 encodes DXT1 as
`f0830f7caaeebaae`: even a constant source need not produce equal endpoints or
uniform selectors because preconditioning/diffusion precedes optimization.
DXT3 and DXT5 color payloads agree in all **474** paired direct blocks, including
the three filtered-float patterns. Both call the same non-alpha-tested color
routine. Their complete color contracts remain blocked by the same RGB fitting
and selector rules; agreement alone is not independent reproduction.

### DXT1 transparency

**Observed and code-supported:** transparent classification is alpha **< 0.5**.
The immediate lower binary32 neighbor is transparent; exact 0.5 and its upper
neighbor are opaque. Byte alpha 127/128 straddle the boundary. Any transparent
pixel selects the three-color route, uses selector 3 for that pixel, and changes
endpoint ordering. Fully transparent blocks produce exactly
`0000ffffffffffff` (0, ffff endpoints, all selector 3).

Transparent pixels' RGB still participates in fitting. Changing only pixel 5's
RGB from black to white at alpha 127 changes DXT1 from
`00004bc1555d5555` to `4bc1ffff000c0000`. Thus ignoring transparent RGB would
break compatibility. Alpha threshold/mode/sentinel/contribution semantics are
bounded here; exact resulting endpoint arithmetic remains covered by open item 1.

### DXT3 alpha and independent partial checker

**Observed, code-supported and independently checked:** raster alpha error
diffusion is unconditional on this BOX/no-dither-flag route. For each pixel,
add its error, store adjusted binary32, convert a binary32 store of
`(alpha+error)*15+0.5` under truncate rounding, pack the nibble, and distribute
`adjusted - q*(binary32 1/15)` with the same 7/3/5/1 weights. Preserve the
extended arithmetic versus explicit float-store boundaries. Nibbles pack
row-major, pixel zero in the low nibble of byte zero. Exact ties are governed
by those operations, not a blanket byte-alpha midpoint formula.

`CheckDXT3Alpha` is TEST-ONLY and calls no D3DX routine. It restores the caller's
rounding mode and independently reproduces the eight alpha bytes for all
**762** DXT3 direct inputs: 471 fixture blocks, 256 scalar alphas, 32 additional
mixed validation blocks and three filtered-float blocks. Real-resource equality
checks the applicable byte-source subset. Example: constant alpha 127 produces
`8787787887877878`, while 128 produces `7878878778788787`; uniform per-pixel
rounding would be wrong. The contract is restricted to normalized reachable
inputs; no arbitrary HDR/negative/NaN alpha promise is inferred.

This is a partial decision checker, **not** an independent full DXT compressor.
Its success closes only DXT3 alpha. It cannot close RGB or DXT5 readiness.

### DXT5 alpha: measured rules and remaining blocker

**Code-supported:** a first raster pass quantizes alpha to normalized byte
precision using `*255+0.5`, truncate conversion and error diffusion, while
tracking extrema. After color encoding, an all-one minimum has a special ff/ff
and zero-selector return. Otherwise the nominal eight-step path is selected
when extrema involve neither 0 nor 1; the six-step path handles endpoint
involvement. `OptimizeAlpha` initializes interior bounds; the six-step path
excludes 0 and 1 from those bounds. Small spans, iterative projection/derivative
refinement, final [0,1] clamping and byte endpoint quantization matter.

An important pinned-table detail: the eight-step derivative coefficient table
`pD8` ends with binary32 **3f924925 (8/7)**, not the expected 1.0. `pC8` starts
at 1 and ends at 0. The six-step C/D tables use the binary32 multiples of 1/5.
Do not silently correct that table when deriving a compatibility model. Its
complete effect through optimizer iterations is unproven in this continuation.

The nominal six-step branch writes ascending fitted bounds; the nominal
eight-step branch reverses them. This is **not** an unconditional promise that
stored endpoints always imply that nominal mode: degenerate interior bounds
can violate it. Observed all-zero alpha yields `ff00499224499224`, constant
127 yields `7f7f000000000000`, and one 127 with the other fifteen alphas 255
also collapses to `7f7f000000000000`. Do not substitute min/max endpoints or
normalize these surprising bytes. All corresponding real-resource cases agree.

Observed six-step fixture alpha bytes: `34cac6981b636e8c`; eight-step fixture:
`ff14b94b4ab94b4a`. The selector route builds normalized float interpolation
values with binary32 1/5 or 1/7, projects/rounds, handles fixed 0/1 choices and
diffuses residuals. Maps are `[0,2,3,4,5,1]` and `[0,2,3,4,5,6,7,1]`; three-bit
selectors pack row-major in two eight-pixel/three-byte groups. Exact tie,
degenerate, store-order and iterative endpoint behavior is **not independently
reproduced**. Both DXT5 alpha fitting and selector readiness remain NO.

### Small mips, filter boundary and real-device cross-check

**Observed and code-supported:** codec block cache extension is periodic in
logical power-of-two dimensions: a 2x2 mip repeats its 2x2 tile across 4x4;
1x1 repeats its pixel. Eighteen cases cover 1x1, 2x2, 1x4, 4x1, 2x4 and 4x2 in
all three formats. Direct encoder periodic candidates agree 18/18. Clamping
differs in every 2-wide case (2x2, 2x4, 4x2); zero-fill differs in all eighteen.
For 1-wide dimensions clamping and periodic replication coincide, so the
2-wide cases and codec mask-copy code establish the distinction. Small DXT1
sources are opaque to avoid a fully transparent sentinel masking the policy.

The encoder receives cached **float D3DXCOLOR RGBA**, not an ARGB8 intermediate
image. Nine actual 8x8 -> 4x4 BOX cases agree byte-for-byte with direct encoding
of fractional float block averages. Pattern 0's DXT3 and DXT5 disagree with
the explicitly materialized ARGB8 intermediate route; the other seven match.
That equality does not make the byte route equivalent. Existing B's TRIANGLE
coefficient/float-field handoff remains unchanged; codec dispatch receives the
same float representation. The production flag is plain BOX, yet the internal
encoder's diffusion still runs. No new caller-side clamp or byte quantization
stage is present at this handoff. Quantization/clamping inside the encoder is
as described above; out-of-range synthetic float inputs are outside this gate.

Real Win32 HAL validation uses hidden 64x64 DX8 windows with software vertex
processing/FPU preservation, not Generals or Zero Hour. It creates aligned
managed DXT textures and obtains level-two surfaces to test supported 1/2-wide
mips, rather than chasing invalid odd top-level creation. A8R8G8B8 image-surface
sources copy with the same actual BOX helper and exact compressed-byte readback.
All **171/171** cases agree: 144 distinguishing byte blocks, eighteen small
extensions and nine boundary cases. Existing B's sixteen adjacent compressed
mip levels still agree and their evidence is byte-identical. No adapter skip or
limitation was encountered in the new cases; this is evidence for this adapter,
not a universal driver claim.

### X64.9C thirteen-item compressed readiness gate

YES is limited to the frozen reachable input/format/flag contract. Observed
fixtures and partial disassembly do not close missing exact arithmetic.

| # | Required decision | Answer |
| --- | --- | --- |
| 1 | DXT1 RGB endpoint fitting specified? | **NO**: weighted/refined route identified; exact numerical updates/orientation ties lack independent reproduction. |
| 2 | DXT1 endpoint ordering/mode specified? | YES: alpha-tested three/four choice, quantized ordering, equal and all-transparent exits. |
| 3 | DXT1 selectors/ties specified? | **NO**: projection/remap/diffusion identified, exact tie/store decisions not independently reproduced. |
| 4 | DXT1 transparency specified? | YES: <0.5, transparent RGB contributes, selector 3, mode/order and sentinel; endpoint arithmetic tracked separately in 1. |
| 5 | DXT3 color specified? | **NO**: shared RGB optimizer/selectors from 1/3. |
| 6 | DXT3 alpha specified? | YES: exact partial checker, all byte alphas, every midpoint side and additional mixed validation. |
| 7 | DXT5 color specified? | **NO**: shared RGB optimizer/selectors from 1/3. |
| 8 | DXT5 alpha endpoints/mode specified? | **NO**: nominal branch and anomalous/degenerate examples bounded, exact refinement not validated. |
| 9 | DXT5 alpha selectors/ties specified? | **NO**: maps/packing/projection route known, full numerical/tie checker absent. |
| 10 | 4x4/2x2/1x1 extension specified? | YES: periodic cache extension, narrow cases, distinguishing clamp/zero contrasts. |
| 11 | Filter -> encoder boundary specified? | YES: float RGBA handoff, nine direct-float agreements and two byte-intermediate counterexamples; no new clamp/byte stage. |
| 12 | Required real-device cases agree? | YES: 171 new exact agreements plus preserved adjacent-mip evidence. |
| 13 | Required route reproducible without generic compressor/guessing? | **NO**: no exact complete RGB/DXT5 model yet. |

### Final nine-item X64.9 readiness gate

| # | Contract | Answer |
| --- | --- | --- |
| 1 | BOX | YES, preserved B contract; compressed encoding separately blocked in 5. |
| 2 | TRIANGLE | YES, preserved B contract. |
| 3 | Mixed-channel 2D rounding | YES, preserved B contract. |
| 4 | Packed filtering interaction | YES, preserved B contract. |
| 5 | Required exact DXT3/5 compressed output | **NO**, compressed gate above remains open. |
| 6 | Scaled/non-lockable staging | YES, preserved B access/failure contract. |
| 7 | Aliasing | YES, preserved B snapshot-equivalent measured contract. |
| 8 | File decode/RLE disposition | YES, preserved frozen matrix/quarantined anomaly disposition. |
| 9 | Final helper implementability without guessed semantics | **NO**, because of 5. |

### Final regression, native diagnostics and tool ABI

Actually rerun after the final source/reference edits: full Win32 Release PASS;
CTest **20/20** PASS; direct **149/title** PASS (six pre-existing disabled/title);
focused **75/title** PASS. The resource executable now has **six** passing tests
without skips. Accepted LegacyD3DX math/reference, FVF/reference,
texture/reference/device, fixed shader/reference/device, error strings,
resource, RB1/2/3 and D3D11.1 sentence/D3D11.2 UI tests pass through this suite.
Producer/render2D standalone sequence also passed. Independent AMD64 CTest
**10/10** PASS. Rendering Python **28/28**, performance Python **16/16**,
PowerShell parser **9 scripts**, preservation audit and `git diff --check` PASS.
The audit verifies 391 earlier runtime/evidence files, accepted A.2 capture
hashes and empty index. No game process was launched.

Full AMD64 graph still fails (exit 2) as expected. Separate title probes retain
exactly three unique unresolved helpers/title: D3DXCreateTextureFromFileExA,
D3DXFilterTexture, D3DXLoadSurfaceFromSurface. Their diagnostic-line multiset
matches the saved baseline exactly: LNK2019=6, LNK2001=8, LNK1120=2 combined.

The unconstrained parallel full-graph retry reported **34** tool errors, rather
than the prior 30: it reached two additional already-failing Generals
WorldBuilder objects, `EditCondition.cpp` and `EditAction.cpp`, before Ninja's
failure cutoff stopped in-flight work. Their extra diagnostics are two C2440
member-pointer casts and two C2737 message-map initializations. No tool source
or compiler configuration changed. To compare equivalent work rather than
silently discard those lines, a separate fixed-target probe rebuilt the exact
**20 failing object targets** from the saved baseline. It reproduces the entire
accepted **30 diagnostic-line multiset byte-for-byte**: C2065=8, C2440=6,
C2555=4, C2664=8, C2737=4. Both raw logs are retained, including the full-graph
34. Tool ABI remains unresolved and untouched; no native-link progress claimed.
Ignored logs: `x64-9-final-win32.log`, `x64-9-win32-ctest.log`, direct/focused
logs, `x64-9-final-win64.log`, `x64-9-final-link.log`,
`x64-9c-native-tools-fixed.log`, `x64-9-native-ctest.log`, C Python logs and
`x64-9c-evidence/final-audit.log` under `build`.

### Combined inventory, disposition and exactly one next task

Five modified tracked files, all unstaged:

- `Tests/Google/Dependencies/CMakeLists.txt`
- `docs/modernization/AGENT_HANDOFF.md`
- `docs/modernization/RENDERER.md`
- `docs/modernization/ROADMAP.md`
- `docs/modernization/X64.md`

Six untracked intended files:

- `Tests/Google/Core/Libraries/LegacyResourceCharacterization.h`
- `Tests/Google/Core/Libraries/LegacyResourceCharacterizationTest.cpp`
- `Tests/Google/Core/Libraries/LegacyResourceReference.jsonl`
- `Tests/Google/Core/Libraries/LegacyResourceSamplingReference.jsonl`
- `Tests/Google/Core/Libraries/LegacyResourceEncoderReference.jsonl`
- `docs/modernization/X64_9_RESOURCE_CHARACTERIZATION.md`

All **eleven** combined files remain unstaged. No production, tool ABI,
gameplay, timing, networking or save-format source changes. Generated JSONL,
fixtures, extraction/disassembly, audit helpers and logs remain ignored.
`build/performance/stage4a2-B-final-2` and the 2,469,396,480-byte
`build/performance/stage4a1-B-cpu/B-cpu.etl` remain available and unchanged.
Commit SHA/subject: **none**, because compressed readiness items 1/3/5/7/8/9/13
and overall 5/9 remain NO. Push: **not attempted**, for the same failed gate.

Exactly one next task: **finish the numerical RGB/DXT5 encoder decision checker
in this same characterization harness**. Work only on OptimizeRGB's axis ties,
derivative/refinement/store order and downstream color selector ties, then
OptimizeAlpha's exact six/eight coefficient/degenerate updates and alpha
selector ties. Reuse the existing blocks and extracted tables; add a fixture
only to distinguish a specific remaining branch. Require exact independent
bytes against these references and additional validation blocks, including the
pinned 8/7 coefficient anomaly. Do not repeat broad resource sampling, request
game validation, substitute a generic compressor, migrate a helper or start
X64.10. Stop for developer review with the gate OPEN.


## X64.9D final independent numerical decision model (2026-10-11)

### Convergence result and scope

The remaining numerical gate is closed. `LegacyResourceEncoderModel.h` extends
the existing Win32 characterization target; it does not call D3DX, read expected
bytes, hash inputs into cases, or provide a production entry point. The pinned
encoder remains only the independent comparison oracle in the test harness.
The model was completed against the existing C fixtures, then validated with
one fixed 32-block set not used to derive it. No repeated corpus growth occurred.

The contract is the accepted finite, normalized production DXT1/3/5 input and
BOX/TRIANGLE/caller matrix. This is not universal equivalence for arbitrary HDR,
NaN, DXT2/4, all FPU modes or all drivers. The checker inherits caller x87
precision; the validated control word is `0x9001f`, with rounding and precision
preserved across the model/oracle sequence. It is intentionally Win32/MSVC-only.
Future AMD64 implementation must reproduce the specified numerical decisions,
not copy this test-only x87 machinery blindly or assume SSE reassociation safe.

### Independently executable arithmetic and decisions

The `E` primitive stores x87 intermediates in 80-bit storage (MSVC long double
is binary64), with explicit binary32 stores marked by `F()`. Arithmetic order,
current precision and scoped truncation match the pinned instruction findings.
The model saves/restores rounding control and tests caller control preservation.

RGB preconditioning reproduces 5/6/5 normalization and raster diffusion,
channel-specific residual reloads, exact float weights/inverse weights and the
extended blue intermediate. OptimizeRGB reproduces bounds, all four orientation
scores, strict first-best tie behavior (score zero accumulates extended; other
scores store per pixel), green/blue orientation swaps, tiny-span exits, exact
three/four-step coefficients, up to eight refinement iterations, derivatives,
inverse updates and component-square convergence threshold 1/65536. Projection
store points and rounding are explicit. An unexpected out-of-table optimizer
index fails validation instead of inventing a lower clamp absent in pinned code.

Final RGB endpoints reproduce inverse weighting, clamp/565 quantization, endpoint
ordering and equal-endpoint exits. Selector projection uses original weighted
float input plus diffusion errors, decoded normalized endpoints and exact palette
interpolants, R+B+G summation, asymmetric denominator stores, half-up/truncating
rounding and exact selector maps. Transparent DXT1 input takes selector 3 and
skips selector diffusion. Selector residual scaling uses cached alpha one;
blue reloads binary32 for every neighbor, green only on the diagonal, red keeps
its extended residual. These store choices are part of the specification.

OptimizeAlpha fits byte-preconditioned/diffused alpha; selector projection uses
the **original alpha**, not that fitting input. Extrema comparisons retain the
documented extended value versus stored extrema. Nominal eight-step mode needs
both interior endpoints; six-step fitting excludes zero/one extrema. Preserve
all-one/equal exits, span threshold 1/256, eight-iteration bound, exact coefficient
words (including the unusual final eight-step derivative coefficient **8/7**),
input-minus-palette residual, subtractive gradient updates, endpoint swap,
gradient-square convergence 1/64, clamp and byte endpoint rounding. Eight-step
selector projection runs from stored high toward low; six-step projection uses
ascending endpoints and fixed zero/one half-interval choices with exact ties.
Packing and alpha diffusion are reproduced. DXT3 alpha remains the accepted
independent C checker; shared RGB supplies both DXT3 and DXT5 color bytes.

### Independent prediction and distinguishing validation

All 1,710 existing direct records are predicted byte-for-byte (including nine
float-boundary contrasts), plus eighteen small-block extension records. The 471
existing block fixtures independently check every RGB format and DXT5 alpha;
they exercise all four orientations and reach RGB/alpha iteration maxima 4/8.
The 128 existing compact encoder references remain exact and unchanged.

The fixed holdout generator uses seed `0x649d2c11`: eight mixed byte/extrema
blocks, eight asymmetric distributions, eight near-565-midpoint and alpha-half
boundary blocks, and eight fractional normalized filter-like blocks. Input
normalization is asserted. All **96 full DXT1/3/5 encodings match**. Orientation
mask 15 covers all four orientations; alpha mode mask 321 covers the all-one
exit plus six/eight modes; holdout iteration maxima are 7/7. A deliberate
conventional-coefficient mutation (8/7 replaced by 1) changes eighteen blocks.
This negative control demonstrates that the anomaly is executable behavior,
not an incidental comment or agreement claim. Generated holdout JSONL stays
ignored; expected outputs are never consulted by the model.

### Final compressed readiness: 13/13 YES

| Item | Contract | Final basis |
| --- | --- | --- |
| 1 | DXT1 RGB endpoint fitting | YES: independent OptimizeRGB and endpoint arithmetic. |
| 2 | DXT1 ordering/mode | YES: preserved C decisions and full-model prediction. |
| 3 | DXT1 selectors/ties | YES: independent projection, stores, rounding, maps/diffusion. |
| 4 | DXT1 transparency | YES: preserved threshold/sentinel and independent model. |
| 5 | DXT3 color | YES: shared independent RGB model. |
| 6 | DXT3 alpha | YES: accepted independent C checker, reused in full prediction. |
| 7 | DXT5 color | YES: shared independent RGB model. |
| 8 | DXT5 endpoints/mode | YES: independent OptimizeAlpha, 8/7, six/eight/degenerate decisions. |
| 9 | DXT5 selectors/ties | YES: independent original-alpha projection and exact maps. |
| 10 | Small-block extension | YES: preserved periodic extension and eighteen predictions. |
| 11 | Float filter handoff | YES: preserved B/C contract and independent boundary predictions. |
| 12 | Real-device agreement | YES: 171 agreements rerun, earlier adjacent mips preserved. |
| 13 | Reproducible required route | YES: executable numerical model, references plus fixed holdout. |

### Final overall X64.9 readiness: 9/9 YES

| Item | Contract | Final disposition |
| --- | --- | --- |
| 1 | BOX | YES: accepted B dispatch, odd-edge/native averaging contract. |
| 2 | TRIANGLE | YES: accepted B coefficient/edge/accumulation contract. |
| 3 | Mixed-channel 2D rounding | YES: accepted float accumulation/store ordering. |
| 4 | Packed filtering | YES: accepted native fields/quantization; no byte intermediary. |
| 5 | Required DXT3/5 output | YES: all thirteen compressed gates above now closed. |
| 6 | Scaled/non-lockable staging | YES: accepted B access/failure/cleanup contract. |
| 7 | Aliasing | YES: accepted snapshot-equivalent measured contract. |
| 8 | File decode/RLE disposition | YES: frozen required matrix and quarantined anomaly unchanged. |
| 9 | Final helper implementability | YES within the frozen contract; no unresolved numerical guess. |

### Final validation and preservation

Full Win32 Release build PASS; CTest **20/20**; resource executable **8/8**;
direct title tests **149/title**, six existing disabled/title; focused tests
**75/title**; standalone producer/render2D sequence PASS. Native independent
CTest **10/10**, rendering Python **28/28**, performance Python **16/16**,
PowerShell parsing **9 scripts**, preservation audit and whitespace checks PASS.
The audit preserves 391 earlier runtime/evidence files and accepted A.2 hashes.

All six pre-D streams remain byte-identical: recording3010, device371,
sampling334, sampling-device380, encoder1899, encoder-device171. Compact
references remain 58/94/128 with the exact hashes recorded above; shared DXT3/5
color agrees on 474 records. No compact reference or generated expected bytes were rewritten
to make numerical tests pass. Final logs and holdout evidence are under ignored
`build/x64-9d-evidence`; established build/regression logs retain their names.

Native full graph still fails as expected (exit 2). Separate title links retain
the exact baseline diagnostic multiset: LNK2019=6, LNK2001=8, LNK1120=2,
with the same three unique helpers/title. The fixed twenty-object tool probe
reproduces the exact thirty-line baseline multiset: C2065=8, C2440=6, C2555=4,
C2664=8, C2737=4. The full parallel graph reported 34 tool errors, reaching the
two additional unchanged WorldBuilder objects documented in C; its raw log is retained. No
production helper or tool ABI change, native-link progress or game launch claimed.

### Combined commit and exactly one next task

The conditional gate is satisfied. Explicitly include the complete twelve-file
characterization checkpoint: the five tracked paths listed in C, the six
intended new paths listed in C, and the new test-only
`Tests/Google/Core/Libraries/LegacyResourceEncoderModel.h`. Commit subject:
`X64.9: characterize legacy resource conversion and filtering`.
Push only to `origin/dev/modern-engine` as authorized. No generated evidence,
disassembly, validation helpers, build logs or runtime outputs belong in it.
The final A.2 capture and 2,469,396,480-byte B-cpu ETL remain locally preserved.

Exactly one next task for developer review: **X64.10: implement and migrate the
final bounded resource-helper group**, preserving the complete accumulated
contract and numerical decisions. Production implementation is not part of
X64.9D. Do not reopen closed characterization gates absent contradictory new
evidence; native tool ABI repair remains outside this proposed helper task.
