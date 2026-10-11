# X64.8 — pinned-reference native CPU error strings

Date: 2026-10-11. Scoped implementation/reference/build gates pass.
No game was launched. DX8 remains default. No resource helper was changed.

## Baseline and native link progression

Verified HEAD=6cb8b0d341cbfbb4cc5afc84f8888f0a64268e35,
X64.7: pin reference-tested legacy pixel shader bytecode, on dev/modern-engine,
tracking origin/dev/modern-engine, clean, 0 ahead/behind. Reproduced the bounded
full native graph and separate title links before editing production code.

| Native evidence | Before | After |
| --- | ---: | ---: |
| Unique external D3DX helpers per title | 4 | 3 |
| Combined LNK2019 | 8 | 6 |
| Combined LNK2001 | 8 | 8 |
| Combined LNK1120 | 2 | 2 |

Exactly D3DXGetErrorStringA disappears from both title linker diagnostics,
previously ww3d2.lib(dx8wrapper.cpp.obj), Log_DX8_ErrorCode. The entire remaining
unresolved occurrence/count multiset matches. No new game compiler dependency
or unresolved symbol. Neither native game links yet. The bounded full graph
retains the separate exact 30 tool ABI compiler errors (W3DView/GUIEdit/
WorldBuilder): C2065=8, C2440=6, C2555=4, C2664=8, C2737=4. Exact compiler
diagnostic lines/counts match baseline; no tool fix is included.

## Production callers and required contract

Exactly two production calls, both shared dx8wrapper.cpp:

| Route | Input / buffer / arguments | Existing result handling |
| --- | --- | --- |
| Log_DX8_ErrorCode | unsigned res (HRESULT bits), char tmp[256]="", res/tmp/sizeof(tmp) | On D3D_OK helper return, WWDEBUG_SAY((tmp)); then unconditional WWASSERT(0) |
| Non_Fatal_Log_DX8_ErrorCode | same HRESULT input and initialized stack buffer | On D3D_OK helper return, WWDEBUG_SAY(("DX8 Error: %s, File: %s, Line: %d",tmp,file,line)); no fatal assert |

The inline DX8_ErrorCode entry point returns immediately for D3D_OK; that gate
is unchanged. Calling either logger directly with D3D_OK still formats S_OK,
and the fatal logger retains its assert. Unknown HRESULTs are also formatted
and reported because the legacy helper returns S_OK, not the original error.
Actual debug/log/assert macro configuration remains untouched.

The two callers always supply 256 bytes, ample for every pinned name (maximum
32 characters) or fallback (30 characters). They have no truncation handling
and rely on a terminated string. There is no heap or persistent output owner.
Preserve exact strings, surrounding log text, success return, fatal distinction,
buffer contents/lifetime and all wrapper behavior. No broader logging redesign.

## Actual pinned Win32 characterization, before implementation

Oracle: min-dx8-sdk commit 7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc,
actual build/win32/_deps/dx8-src/d3dx8.lib. An isolated ignored Win32 recorder
called the pinned function before production replacement. It recorded full
output bytes, returned HRESULT and canary preservation for all 32 names in
the pinned implementation plus 14 unknown/common HRESULT values. The original
binary disassembly independently confirms the finite lookup, fixed hexadecimal
fallback and buffer guards. No modern DX error database or documentation oracle.

All 1,264 recorded cases return exactly S_OK, including null output and size 0.
Unknown input does not return an error; e.g. E_INVALIDARG (80070057), S_FALSE (1),
E_NOINTERFACE, E_POINTER, E_ACCESSDENIED and E_UNEXPECTED use fallback text.
Fallback is exactly `Unrecognized error: 0x` plus **eight lowercase zero-padded
hex digits**, with no newline or trailing punctuation. For example:

```text
0x00000000 -> S_OK
0x00000001 -> Unrecognized error: 0x00000001
0x80070057 -> Unrecognized error: 0x80070057
0xffffffff -> Unrecognized error: 0xffffffff
```

For text length L excluding NUL, measured buffer behavior is:

| Output / size | Bytes changed | Return |
| --- | --- | --- |
| nullptr, any tested size | none; no dereference | S_OK |
| nonnull, 0 | none | S_OK |
| 0 < size < L | first size-1 text bytes, then NUL | S_OK |
| size == L | all L text bytes, **no NUL** | S_OK |
| size > L | all L text bytes and NUL; tail unchanged | S_OK |

In particular size 1 writes NUL for these nonempty outputs. The exact-fit
unterminated case is a legacy _snprintf quirk, not silently repaired. Tests
compare every byte of a 520-byte canary buffer, including NUL position and
untouched tails, at sizes 0/1/2/3/4/8/16/32/64/256/512 and L-1/L/L+1 (deduplicated),
both null and nonnull. Valid storage must of course be supplied for the
advertised nonnull capacity; malformed pointers are outside the contract.

The complete measured name mapping is small enough to preserve directly:

| HRESULT word | Exact text |
| --- | --- |
| 00000000 | S_OK |
| 80004001 | E_NOTIMPL |
| 80004005 | E_FAIL |
| 8007000e | E_OUTOFMEMORY |
| 88760818 | D3DERR_WRONGTEXTUREFORMAT |
| 88760819 | D3DERR_UNSUPPORTEDCOLOROPERATION |
| 8876081a | D3DERR_UNSUPPORTEDCOLORARG |
| 8876081b | D3DERR_UNSUPPORTEDALPHAOPERATION |
| 8876081c | D3DERR_UNSUPPORTEDALPHAARG |
| 8876081d | D3DERR_TOOMANYOPERATIONS |
| 8876081e | D3DERR_CONFLICTINGTEXTUREFILTER |
| 8876081f | D3DERR_UNSUPPORTEDFACTORVALUE |
| 88760821 | D3DERR_CONFLICTINGRENDERSTATE |
| 88760822 | D3DERR_UNSUPPORTEDTEXTUREFILTER |
| 88760826 | D3DERR_CONFLICTINGTEXTUREPALETTE |
| 88760827 | D3DERR_DRIVERINTERNALERROR |
| 88760866 | D3DERR_NOTFOUND |
| 88760867 | D3DERR_MOREDATA |
| 88760868 | D3DERR_DEVICELOST |
| 88760869 | D3DERR_DEVICENOTRESET |
| 8876086a | D3DERR_NOTAVAILABLE |
| 8876017c | D3DERR_OUTOFVIDEOMEMORY |
| 8876086b | D3DERR_INVALIDDEVICE |
| 8876086c | D3DERR_INVALIDCALL |
| 8876086d | D3DERR_DRIVERINVALIDCALL |
| 88760b54 | D3DXERR_CANNOTMODIFYINDEXBUFFER |
| 88760b55 | D3DXERR_INVALIDMESH |
| 88760b56 | D3DXERR_CANNOTATTRSORT |
| 88760b57 | D3DXERR_SKINNINGNOTSUPPORTED |
| 88760b58 | D3DXERR_TOOMANYINFLUENCES |
| 88760b59 | D3DXERR_INVALIDDATA |
| 88760b5a | D3DXERR_LOADEDMESHASNODATA |

## Compatibility implementation and migration

Header-only LegacyD3DX::GetErrorStringA in LegacyErrorString.h requires only
cstdint. It accepts exact 32-bit HRESULT bits and capacity, writes a supplied
buffer, and returns a signed 32-bit S_OK. The finite 32-name switch uses the
audited SDK HRESULT bit values and original names; this includes the seven
legacy mesh values so a previously recognized production result cannot turn
into fallback. It is the complete tiny lookup of this pinned function, not a
general DirectX error database. All other values use stack-only, locale-free
eight-nibble formatting and explicit measured copy/termination rules.
No heap, D3DX/D3D8 implementation, D3D11, DXGI, D3DCompiler or external-name shim.

Migration is exactly one include plus two identifier substitutions in
dx8wrapper.cpp. All other bytes match the baseline after reversing these
changes/newline normalization. The existing renderer preservation test now
permits only this exact substitution in addition to prior accepted stages;
all original wrapper token checks remain active. No resource route, log format,
assert, initialization, retry or ownership behavior changed.

## Reference and AMD64 validation

LegacyErrorStringReference.inc is intentional versioned measured oracle data:
1,264 exact records from the original pinned function, not replacement-generated
expectations. It stores input/capacity/null state, HRESULT and every changed
output byte. Native and Win32 tests reconstruct untouched canary tails and
compare the entire buffer. The separate Win32 live-reference target invokes
the actual pinned function independently for every recorded case.

Additional live coverage checks all 65,536 low-word values of the production
D3D HRESULT facility at the actual 256-byte caller capacity, plus 4,096 seeded
32-bit words at every size 0..34 and 512: 212,992 further exact paired calls.
This covers recognized-value neighbors and deterministic fallback/truncation
without defining expected strings from the replacement. Assembly plus measured
finite lookup bounds the unenumerated 32-bit space; no exhaustive 2^32 runtime
test or arbitrary pointer validity claim is made.

Native test binary is PE32+ / machine 8664. Its CPU test target has no graphics
SDK include dependency and no D3D8/D3DX archive in the Ninja link edge. Imports
are Windows/CRT only; no D3D11/DXGI/D3DCompiler. It verifies the same recorded
output bytes and results as Win32. Actual graphics-device availability is
irrelevant to this CPU boundary; no native DX8 runtime proof is claimed.

| Validation actually run | Result |
| --- | --- |
| Full Win32 Release graph | PASS |
| Win32 CTest | 19/19 PASS |
| Direct title Google tests | 149/title PASS; existing 6 disabled/title |
| Focused renderer/path/profiler/developer/Bezier tests | 75/title PASS |
| Error-string recorded CPU test | PASS Win32 and AMD64 |
| Win32 live error-string reference tests | 3 PASS, including broad facility/seeded sweep |
| Legacy math/reference, FVF/reference, Bezier | PASS |
| Texture creation/reference/real-device | PASS; real matched successes 108 2D / 108 cube / 81 volume |
| Fixed shader/independent assembler/real-device | PASS; all four paired device creations S_OK, no skip |
| RB1/RB2/RB3 and producer | PASS |
| D3D11.1 sentence / D3D11.2 UI, real WARP | PASS on Win32 and AMD64 |
| Bounded full Win64 graph and separate title links | Expected remaining failures; exact progression above |
| Independent native CTest | 10/10 PASS, all prior targets retained |
| Rendering Python / performance Python | 28/28 and 16/16 PASS |
| PowerShell parser | 9 files PASS |
| Preservation audit | PASS; 391 prior runtime/evidence files and accepted A.2 capture hashes |
| git diff --check | PASS |

DX8 remains default. No game was launched, and gameplay/timing/pathfinding/
network/save formats, tools and D3D11 implementation are untouched. Generated
reference probes, logs, binary inspection, drivers and analysis stay ignored
under build/x64-8-*. The accepted stage4a2-B-final-2 directory remains local and
ignored; B-cpu.etl remains local/ignored at 2,469,396,480 bytes. No capture or
runtime output is overwritten or staged.

## Remaining three-resource-helper readiness audit

Final native links contain exactly these three helpers; source counts are
shared Core calls, not duplicated per title:

| Helper | Exact callers | Required policy |
| --- | --- | --- |
| D3DXCreateTextureFromFileExA | 1, DX8Wrapper::_Create_DX8_Texture(const char*, MipCountType) | Default width/height, requested mip count, usage 0, UNKNOWN format, MANAGED pool, BOX file/mip filters, color key 0, no info/palette output; missing texture on failed result or returned P8 (release first) |
| D3DXLoadSurfaceFromSurface | 4: DX8Wrapper surface _Create_DX8_Texture; MissingTexture::_Init; SurfaceClass::Copy; SurfaceClass::Stretch_Copy | Null palettes, color key 0; full surfaces with BOX, explicit clipped rects with NONE for Copy fallback, explicit scale rects with TRIANGLE for Stretch_Copy. Copy retains the original same-format/same-size CopyRects fast path |
| D3DXFilterTexture | 6: DX8Wrapper surface _Create_DX8_Texture (1); W3DTreeBuffer::W3DTreeTextureClass::update (1); TerrainTextureClass::update (2 compile-time variants); TerrainTextureClass::updateFlat (1); AlphaEdgeTextureClass::update (1) | 2D texture call sites, null palette, source mip 0, BOX; surface wrapper only filters when mip count !=1 |

### A. File decode

The ordinary engine TextureLoader has concrete DDS-first/TGA paths, DXT1..5,
24/32-bit TGA handling, destination-format policy, reduction and separate
palette/luminance/conversion paths. This is evidence of engine file formats,
not proof that every one passes through the D3DX filename overload. Static
named-call search found no direct filename-overload invocation outside its
definition; its retained exported wrapper still creates the unresolved link.
Do not infer a smaller runtime contract merely from absent direct call sites
or silently remove the route. Resolve reachability and installed asset coverage
before choosing supported decoder inputs.

Tracked repository image inventory is seven TGA and 66 BMP, with no tracked
DDS/JPEG/PNG/PCX found. The TGA headers were inspected directly:

- RequiredAssets/AddProjectorGradient.tga and MultProjectorGradient.tga:
  grayscale type 3, 16x64, 8-bit.
- RequiredAssets/w3d_missing_texture.tga: type 2, 128x128, 24-bit.
- W3DView Res cursor/grab/orbit/zoom.tga: type 2, 32x32, 32-bit.

These are useful real fixtures, not a representative retail texture corpus;
the many BMP files include tool/autorun UI. Missing tracked DDS coverage must
be supplied by controlled fixtures and, where available, a read-only installed
asset manifest. No new asset copies or decode implementation were made here.
Measure native/default dimensions, UNKNOWN format negotiation, mip request
0/1/explicit, both BOX filters, zero color key, invalid/missing files, palette
decode and output P8 rejection/fallback against the pinned function. Preserve
ANSI filename/file access semantics; FileFactory-based UI loading is not
automatically equivalent to D3DX OS filename access.

### B. Surface conversion/copy

All four calls omit palettes and use zero color key. Source/destination types
remain real IDirect3DSurface8 objects. Full-surface transfer and rect clipping,
scaling, format conversion, locks/flags, actual row pitches and failure cleanup
are shared with filter/file pipelines. SurfaceClass uses device image surfaces
and has its own lock paths; do not assume every caller surface is lockable or
tightly packed. Its general format enum includes packed RGB/alpha, palette,
luminance, bump and DXT formats; this is a reachability audit input, not proof
that all conversions succeed.

Prioritize A8R8G8B8/X8R8G8B8, R8G8B8, R5G6B5/A1R5G5B5/A4R4G4B4 and measured
destination fallbacks; determine palette/luminance/bump/DXT edges from actual
callers rather than inventing a universal converter. Measure full and offset
rects, clipping, 1xN/Nx1, odd dimensions, up/down scale, NONE/BOX/TRIANGLE,
source/destination aliasing if reachable, row padding, alpha endpoints and
device access errors. Color key 0/null palettes are fixed production settings;
record unsupported input behavior instead of extending semantics speculatively.

### C. Filter/mip generation

Shared call settings are BOX, source level 0, no palette. Tree texture starts
as A8R8G8B8; terrain constructors request A1R5G5B5 and three/all mip levels.
Other paths can negotiate device formats, so requested format alone cannot
define all filter inputs. MissingTexture separately builds lower levels by
repeated BOX surface copies, providing a cross-helper consistency case.
Measure generated levels and their exact pixels, source-level preservation,
BOX footprint/rounding, odd extents, 1D chains, packed-bit quantization, alpha
and premultiplication behavior, DXT handling/failures and failures midway
through locking/upload. Do not change mip count, fallback format, queue/timing
or ownership to avoid difficult cases.

### Minimum X64.9 evidence plan

1. Freeze the caller-reachable matrix and asset formats first. Preserve the
   current three symbols and all production routes during characterization.
2. Use independent pinned Win32 calls on deterministic CPU-generated fixture
   pixels/files (channel impulses, ramps, alpha endpoints, checkerboards,
   odd extents and packed/compressed edge cases). Record complete output
   descriptors and pixels for every mip/rect, HRESULTs and unchanged regions.
   CPU fixture generation supplies inputs, not inferred expected filter math.
3. Recording COM interfaces should observe GetDesc/levels, locks/unlocks,
   pitches/flags, creation/transfers, ordering, output state/refcounts and
   injected failure cleanup. Keep them fail-fast and separate from real-driver
   proof; a permissive mock must not define conversion semantics.
4. Run bounded real Win32 DX8 comparisons on system-memory/managed/default
   surfaces and textures where actually used. Read back each successful
   operation and distinguish driver/device restrictions from D3DX CPU policy.
   Unsupported format/lock/device cases need explicit results, not fake pixels.
5. Exercise the seven repository TGA fixtures and controlled DDS/palette/mip
   fixtures, plus a read-only available retail asset manifest if needed. Record
   exact output hashes **and full byte references**, not visual approval only.
6. Produce a shared conversion/filter contract and explicit gaps. Decide on
   implementation grouping only after reference/device agreement; stop at
   characterization if any required sampling/format/failure contract is open.

X64.6 already supplies proven allocation/caps/format negotiation and COM
creation; reuse that contract without re-solving it. It does not decode,
convert, resample, generate pixels/mips or establish lockability. D3D11.2
UITextureContent supplies useful CPU TGA/DDS top-mip parsing and fixtures for
its explicitly bounded ordinary single-mip route (16/32-bit policy, rejected
HSV/rescale cases). It is not an equivalent D3DX file loader or mip/filter/
surface implementation. D3D11 resource upload cannot replace DX8 COM ownership.

## Exactly one next task

**X64.9 — characterization-only evidence stage for the coherent file/filter/
surface resource group.** Choose option B, not combined migration yet.
Only three helpers remain, and grouping their eventual pixel pipeline is
coherent. However, allocation work and narrow UI loading do not bound the
remaining conversion, sampling/rounding, compressed-format and device-access
behavior. Characterization first is the shortest SAFE step: it prevents an
apparently faster link fix from silently changing visuals or failures. No
resource implementation, next-stage probe or instrumentation was begun in
X64.8; the plan above is documentation only.

## Exact changed-file inventory

- Core/Libraries/Source/WWVegas/WW3D2/LegacyErrorString.h
- Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp
- Tests/Google/Core/Libraries/LegacyErrorStringTest.cpp
- Tests/Google/Core/Libraries/LegacyErrorStringReference.inc
- Tests/Google/Dependencies/CMakeLists.txt
- scripts/rendering/test_render2d_reference.py
- docs/modernization/X64_8_ERROR_STRING.md
- docs/modernization/X64.md
- docs/modernization/ROADMAP.md
- docs/modernization/AGENT_HANDOFF.md

RENDERER.md is unchanged: this CPU diagnostic step changes no renderer architecture.
