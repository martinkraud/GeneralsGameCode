# X64.6: reference-tested DX8 texture creation

2026-10-11. Baseline `84e5d2ec1503027d4c1ee81722a4dc2dad36a271`,
`X64.5: replace external D3DX FVF sizing with native integer compatibility`.
Before edits: HEAD, dev/modern-engine, origin/dev/modern-engine, clean status
and zero ahead/behind verified. Full bounded native graph and separate title
link probes reproduced the accepted baseline.

## Result and scope

The ten audited calls to D3DXCreateTexture/CubeTexture/VolumeTexture now use
engine-owned LegacyD3DX creation functions. They retain real DX8 COM resource
creation, the measured requirement policy and the existing wrapper's retries,
ownership and failure handling. Native game probes naturally remove all three
helpers: **8 -> 5 names per title**. Neither native game links yet.

The header-only boundary includes d3d8.h declarations and a small measured
integer format-preference table. It does not link D3DX, allocate replacement
resource objects or use D3D11. The only modified production .cpp is
dx8wrapper.cpp: one include and ten identifier substitutions. No engine
normalization, other resource route, backend selection or gameplay changed.
DX8 remains default. No game was launched.

## Caller inventory and frozen wrapper behavior

All calls are in Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp, shared by
both games. Counts include explicit retries, not just distinct functions.

| Helper / wrapper | Calls | Parameters and preserved handling |
|---|---:|---|
| CreateTexture / _Create_DX8_Texture(width,height,format,mips,pool,rendertarget) | 4 | Width/height, MipCountType and pool forwarded; WW3DFormat_To_D3DFormat; usage 0 for ordinary or RENDERTARGET. Initial and OOM retry for each route. |
| CreateCubeTexture / _Create_DX8_Cube_Texture | 4 | Existing width==height assertion; width becomes edge; same format/mips/pool and ordinary/RT usage split; initial and OOM retry for each. |
| CreateVolumeTexture / _Create_DX8_Volume_Texture | 2 | Explicit width/height/depth, mips, converted format, pool, usage 0; initial and OOM retry. No volume RT wrapper route. |

Each wrapper initializes its output to nullptr. RT 2D/cube NOTAVAILABLE logs
and returns nullptr before retry. OUTOFVIDEOMEMORY invalidates textures unused
for 5000 ms, invalidates the mesh cache, then retries once with the same input
parameters. A second RT OOM logs and returns nullptr; the normal route retains
its original DX8_ErrorCode handling. Other errors follow the original error
macros. Ordinary volume uses the same single invalidation/retry pattern.
The helper itself never retries or invalidates anything.

Successful resource ownership passes unchanged to the caller/TextureClass
Poke_Texture path; helpers introduce no resource AddRef/Release. The temporary
IDirect3D8 interface acquired during negotiation is balanced. Default-pool
tracking/reacquisition, resource release and wrapper thread assertions remain
unchanged. The separate _Create_DX8_ZTexture direct device route and its existing
AddRef are untouched. No file loader, filter, surface-copy or shader call changed.

## Existing engine policy, before D3DX

- TextureLoader::Validate_Texture_Size rounds dimensions/depth to powers of two,
  caps width/height/volume extent and increases the smaller dimension for aspect
  constraints. Loader reduction paths compare these normalized values with
  reduced file dimensions. Procedural constructors can forward explicit inputs
  directly, so this normalization is not universal.
- Get_Valid_Texture_Format applies compression support/policy (including the
  existing DXT1->DXT2 workaround), RGB24->XRGB32, 16-bit texture preference and
  the existing supported-format fallback chain. It primarily uses ordinary
  texture support, not the full resource-type/usage negotiation done by D3DX.
- Loader mip reduction respects file levels and removed leading levels;
  MIP_LEVELS_ALL requests all levels, while explicit counts remain possible.
- Render-target factories precheck format/depth support and normalize RT size;
  procedural TextureClass/cube/volume constructors map DEFAULT/MANAGED/SYSTEMMEM
  pools and preserve tracker ownership. The wrappers still request a closest
  supported format from D3DX if needed.

These policies are preserved byte-for-byte after reversing the mechanical
wrapper edits. They are not assumed equivalent to D3DX: the latter clamps in
a different order, can reduce aspect dimensions, negotiates per usage/type,
and supplies mip/default/scratch/compression rules of its own.

## Pinned characterization and recording methodology

Reference: actual Win32 d3dx8.lib from min-dx8-sdk commit
`7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc`, build/win32/_deps/dx8-src.
The legacy-only recorder was executed before the replacement was written.
Audited engine source plus pinned disassembly of the three entry points,
CheckTextureRequirements, D3DXFindClosestDeviceFormat and D3DXScoreFormat
establish the contract; modern DirectX documentation was not used as an oracle.

Typed IDirect3D8/IDirect3DDevice8 test interfaces record ordered queries and
device-creation arguments. Unused COM methods abort, rather than succeeding
permissively. Format support defaults to rejection and is explicitly configured
per input. Real typed test resource interfaces detect unexpected resource
operations and track output identity/refcounts, including the unchanged caller
object and caller release. These test doubles measure CPU policy, not hardware
capability or native graphics availability.

For non-scratch successful negotiation the exact sequence is:

1. GetDirect3D (with AddRef), GetDeviceCaps, GetDisplayMode.
2. CheckDeviceFormat in the pinned 43-format order, with caps AdapterOrdinal /
   DeviceType, display format, requested usage and resource type.
3. Stop immediately on a supported exact requested format; otherwise score all
   supported candidates. Release the temporary IDirect3D8 interface.
4. If no eligible format, return NOTAVAILABLE without device creation or output
   modification. Otherwise GetDeviceCaps again for dimension/mip negotiation.
5. Call exactly one matching device Create operation with negotiated arguments.

The pinned format score depends on fixed format metadata; ties use byte size,
then original table order. The recorder measures each requested format's total
preference order by repeatedly removing the selected supported format. The
production table records 44 rows (UNKNOWN plus all 43 pinned formats), each
with 43 candidate ranks. Rank zero is ineligible as a fallback. A supported
exact request still wins immediately, including formats ineligible as a
conversion fallback. Unrecognized format words use the measured UNKNOWN row.
This declarative table avoids implementing unrelated pixel conversion code.
Candidate query order remains independent of preference rank.

Independent evidence checks that reduction: every singleton and pair support
set, reject-all and 64 seeded larger support sets, for every requested format
and all three resource types. The pinned disassembly confirms the score and
tie ordering is independent of the support set. The full live comparison
compares each complete result/event vector, not only a digest.

### Reference coverage

**145,755 cases** cover all engine formats and all pinned candidates; unknown /
invalid format words; ordinary/RT/depth/write-only/dynamic/default/invalid usage;
all four pools; zero/default/explicit dimensions, oversize requests and mip
counts; POW2/SQUAREONLY/aspect/conditional-NPOT/mip/cube/volume capability
combinations; unequal/non-power-of-two limits; fallback support sets; creation
success/S_FALSE/INVALIDCALL/NOTAVAILABLE/OUTOFVIDEOMEMORY/E_FAIL; device failure
leaving or clearing output; null device/output and invalid pool.

90 additional examples store complete literal HRESULT/output/query/creation/
ownership transcripts. Native tests compare those fields exactly. The broader
sweep uses legacy-only fingerprints of groups of up to 1024 complete results
as supplementary regression evidence. They are not the sole correctness proof:
the live Win32 test compares all 145,755 transcripts directly, and native tests
also compare the 90 complete literals. Fingerprints are not collision-free proof.

The CMake-built legacy-only recorder exactly reproduces all three intentional
policy/oracle .inc files. It never includes the replacement header. The raw
recorder and the CMake recorder use different compilation setup and agree.

## Exact measured creation semantics

| Rule | Preserved behavior |
|---|---|
| Invalid arguments | Null device/output, invalid pool or usage outside mask 0x20b returns INVALIDCALL before queries. Existing output is untouched. |
| Default usage | 0xffffffff becomes zero before negotiation and creation. |
| Format | Non-scratch negotiates against device type/usage support; no eligible format returns NOTAVAILABLE. SCRATCH bypasses all capability/format queries, preserves explicit formats and maps UNKNOWN to A8R8G8B8. |
| Width/height defaults | Both 0xffffffff -> 256x256; one default copies the other dimension. Zero becomes one. |
| Depth default | Zero/default -> one; applies to volume. |
| 2D limits | Clamp width/height separately; SQUAREONLY makes aspect ratio one, otherwise MaxTextureAspectRatio. Reduce dimensions in width-then-height order. |
| Cube limits | Single edge enters aliased width/height negotiation. MaxTextureWidth/Height can differ; final height store wins. Mip computation still considers both negotiated dimensions. Preserve this legacy quirk. |
| Volume limits | All three extents clamp to MaxVolumeExtent, independent of MaxTextureWidth/Height. |
| POW2 | Resource-specific POW2 bit rounds up after limits/aspect. This can exceed a synthetic non-power-of-two cap; do not silently re-clamp. |
| DXT1..5 | Width/height round up to multiples of four, even for SCRATCH. Depth does not. |
| Mips | Missing type-specific mip capability forces one level. Conditional NPOT also forces one if any relevant dimension is non-power-of-two. Otherwise zero/default/excess requests clamp to the full chain from the largest dimension. SCRATCH ignores caps. |
| Cube/volume support flags | No extra helper-level rejection solely for missing CUBEMAP/VOLUMEMAP bits; CheckDeviceFormat and actual device creation decide availability. |
| Creation failure | Return exact negative HRESULT and preserve whatever the actual device wrote to output. Do not clear/release an existing caller object preemptively. |
| Creation success | Any nonnegative device creation HRESULT becomes S_OK; return the actual resource with its existing ownership. |

The boundary deliberately keeps 2D/cube/volume differences explicit. It uses
integer policy and actual device calls, with no generic modern texture system,
external-name shim, linker suppression, dummy resource or D3D11 reinterpretation.

## Real Win32 resource validation

A separate test loads system d3d8.dll explicitly from the system directory,
creates a hidden 64x64 window and a real HAL DX8 device with software vertex
processing and FPU_PRESERVE. It does not start the game or Present a scene.
It compares the pinned helper and replacement on 162 parameter combinations
per resource type: three sizes, three mip requests, three formats, three pools
and ordinary/RT usage, including unsupported combinations.

On this host it **ran, without a skip**, and matched:

| Resource | Successful matched pairs |
|---|---:|
| 2D | 108 |
| Cube | 108 |
| Volume | 81 |

All 486 paired attempts match HRESULTs; the other 189 pairs fail consistently.
Successful resources match type, full mip count and every level's dimensions /
extents, format, usage, pool, size and surface multisample value where applicable.
Resources/device/window/module are released in order. The test explicitly skips
with the failure reason on hosts without a usable system DX8 device. One host's
driver coverage is not certification of every adapter or driver.

## AMD64 and regression validation

Native creation-policy tests compile/run as AMD64 (dumpbin machine 8664,
PE32+). The link edge contains GoogleTest and Windows system libraries; no
x86 D3DX archive, D3D8 implementation, D3D11 or DXGI library. Imports are
Windows/CRT only, without graphics DLLs. This proves native CPU policy and COM
declaration compatibility. It does not prove a real AMD64 DX8 device/resource
route: real resource validation here is Win32, and neither native game links yet.

Actually executed with VS18/MSVC 19.51 / SDK 10.0.26100 / Release:

- Full Win32 graph passes; CTest **14/14** passes.
- Direct Google tests **149/title**; focused renderer/path/profiler/developer/
  Bezier tests **75/title**, all pass. Six pre-existing disabled tests/title.
- Existing math/FVF independent and live reference tests, RB1/RB2/RB3 producer
  tests, D3D11.1 sentence and D3D11.2 UI real WARP tests all pass.
- Native independent CTest **8/8** passes, retaining all seven prior tests.
- Rendering Python **25/25**, performance Python **16/16**, PowerShell parser
  **9 files**, preservation audit and git diff --check pass.

The source-preservation test permits only one include and counted 4/4/2 helper
substitutions in dx8wrapper.cpp, reverses them, then retains its original
whole-source token comparison. An additional baseline comparison confirms the
wrapper is identical after reversal and Git newline normalization. Thus wrapper
retry/invalidation/ownership code is preserved independently of the helper tests.
Actual game OOM recovery and device loss were not exercised by launching a game.
No authoritative state, timing, pathfinding, RNG, FP policy, save/network format
or replay logic changed; no multiplayer/save/replay certification is claimed.

## Actual native progression

Before and after: full bounded Win64 graph (-k20), separate native
g_generals/z_generals link probes, then independent native tests.

| Measure | Baseline | X64.6 |
|---|---:|---:|
| Unique external D3DX helpers per title | 8 | 5 |
| Combined LNK2019 occurrences | 16 | 10 |
| Combined LNK2001 occurrences | 8 | 8 |
| LNK1120 occurrences | 2 | 2 |

Each target helper had one LNK2019 occurrence per title in
ww3d2.lib(dx8wrapper.cpp.obj), referring to its corresponding width/height
_Create_DX8_* wrapper. All six target occurrences disappear. Every other
unresolved-symbol occurrence remains unchanged. No new game compiler or linker
dependency appears. Neither game links. Full native graph retains the exact
30 tool ABI diagnostic multiset: C2065=8, C2440=6, C2555=4, C2664=8, C2737=4,
in W3DView/GUIEdit/WorldBuilder, separate from game executable linkage.

## Remaining game-link blockers and grouping

All five helpers below still block both native games. Counts are shared source
calls, including the conditional profiler call, not duplicated per-title counts.

| Helper | Exact production callers | Scope, useful boundary and risk |
|---|---|---|
| D3DXCreateTextureFromFileExA (1) | dx8wrapper.cpp filename _Create_DX8_Texture | Decode/convert/filter/mip plus device upload; default dimensions/format, MANAGED, BOX filters, zero color key, missing fallback and P8 rejection. Existing loader and new creation boundary help; D3D11.2's bounded TGA/DDS top-mip loader is not equivalent. High. |
| D3DXFilterTexture (6) | dx8wrapper.cpp surface _Create_DX8_Texture (1); W3DTreeBuffer.cpp (1); TerrainTex.cpp (4) | BOX mip generation over device resource data. Surface/format helpers help partially. Coherent with surface conversion/filtering, but not implemented by the creation boundary. High. |
| D3DXLoadSurfaceFromSurface (4) | dx8wrapper.cpp surface _Create_DX8_Texture (1); missingtexture.cpp _Init (1); surfaceclass.cpp Copy and Stretch_Copy (2) | Device surface access plus NONE/BOX/TRIANGLE rect conversion/scaling. Shares filter/format machinery with mip filtering and file upload. Existing SurfaceClass and CPU content seams help partially, without proving equivalence. High. |
| D3DXAssembleShader (4) | W3DWater.cpp ReAcquireResources (3); W3DProfilerFrameCapture.cpp Capture (1 under PROFILER_ENABLED) | CPU assembly of three immutable ps.1.1 water strings and one immutable ps.1.4 swizzle string; device consumes tokens. A pinned exact-bytecode path can be bounded without a general assembler. D3D11 HLSL is unrelated. Low-medium. |
| D3DXGetErrorStringA (2) | dx8wrapper.cpp Log_DX8_ErrorCode and Non_Fatal_Log_DX8_ErrorCode | CPU text/HRESULT diagnostics; engine logging exists but exact text/truncation/return behavior needs characterization. Low. No shared conversion dependency. |

The file/filter/surface trio is a coherent larger resource group because it
shares pixel-format conversion, rect/filter sampling and mip rules. Creation
now supplies its allocation prerequisite, but those other contracts remain
uncharacterized and much broader than allocation. Do not substitute D3D11
resources or the restricted UI loader into existing DX8 COM return types.

## Exactly one recommended next task

**X64.7: pinned-reference fixed legacy pixel-shader bytecode compatibility for
all four production D3DXAssembleShader strings.**

The final native evidence leaves five helpers, with no new dependency. The
four immutable shader sources provide a finite CPU contract: record their exact
tokens with the pinned assembler before migration, verify source identity and
byte-for-byte output on Win32/AMD64, and preserve real CreatePixelShader calls,
conditional profiler coverage, creation failures and reset/release ownership.
Do not build a general assembler or migrate shaders to D3D11. Expected natural
progress is 5 -> 4 helpers/title; require full regression and actual native
probes rather than suppressing errors. This task has not begun.

The three remaining resource helpers could unlock three names together, and
should be assessed as a coherent conversion/filter group rather than arbitrarily
one helper per stage. However, their unmeasured sampling/conversion/mip/format
contract is substantially broader than the four fixed shader inputs; new
allocation compatibility alone does not bound it safely. Fixed shader bytecode
is the next shorter safe link-unblocking step on current evidence. Diagnostics
are smaller still, but remove only one logging dependency and supply no resource
or shader prerequisite. They are not selected merely for numerical ease.

## Limitations and preserved evidence

The policy contract assumes valid DX8 interfaces with initialized successful
capability/display query outputs and meaningful driver caps. The pinned routine
ignores those query HRESULTs and can dereference a failed GetDirect3D result or
loop on nonsensical POW2 limits. These broken-driver conditions are outside
the measured compatibility contract. The boundary returns no eligible format if GetDirect3D supplies
no interface; it does not fabricate a resource. Null public arguments, invalid
usage/pool, unsupported format and actual creation failures are characterized.
No arbitrary malformed COM object or exhaustive 32-bit cap space is claimed.
Internal D3DX debug-mute state is not reproduced; production has no caller of
D3DXDebugMute. This does not change the wrapper's own error handling.

Ignored evidence is under build/x64-6-baseline-*, x64-6-final-*, x64-6-native-*,
x64-6-texture-*, x64-6-win32-ctest.log, x64-6-direct-*, x64-6-focused-*,
x64-6-cmake-* and x64-6-validated-source.json; probe/disassembly/generation/
analysis helpers also stay in build/. All 391 preserved prior runtime/evidence
files and accepted A.2 final capture hashes pass the preservation audit.
stage4a2-B-final-2 remains available and ignored; B-cpu.etl remains local,
ignored and 2,469,396,480 bytes. No runtime candidate/capture was changed.

## Exact changed-file inventory

- Core/Libraries/Source/WWVegas/WW3D2/LegacyTextureCreation.h
- Core/Libraries/Source/WWVegas/WW3D2/LegacyTextureFormatPolicy.inc
- Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp
- Tests/Google/Dependencies/CMakeLists.txt
- Tests/Google/Core/Libraries/LegacyTextureCreationCases.h
- Tests/Google/Core/Libraries/LegacyTextureCreationDeviceTest.cpp
- Tests/Google/Core/Libraries/LegacyTextureCreationExamples.inc
- Tests/Google/Core/Libraries/LegacyTextureCreationRecording.h
- Tests/Google/Core/Libraries/LegacyTextureCreationReference.cpp
- Tests/Google/Core/Libraries/LegacyTextureCreationReference.inc
- Tests/Google/Core/Libraries/LegacyTextureCreationReferenceAPI.h
- Tests/Google/Core/Libraries/LegacyTextureCreationStubs.h
- Tests/Google/Core/Libraries/LegacyTextureCreationTest.cpp
- scripts/rendering/test_render2d_reference.py
- docs/modernization/X64_6_TEXTURE_CREATION.md
- docs/modernization/X64.md
- docs/modernization/RENDERER.md
- docs/modernization/ROADMAP.md
- docs/modernization/AGENT_HANDOFF.md

The three .inc files are intentionally versioned measured compatibility/reference
data. Generated build/performance/runtime/analysis artifacts are excluded.
