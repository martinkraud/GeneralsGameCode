# X64.7 — pinned fixed legacy pixel-shader bytecode

Date: 2026-10-11. Acceptance: all scoped source/reference/build gates pass.
No Generals or Zero Hour process was launched. DX8 remains default.

## Baseline and native progression

Started clean and synchronized on dev/modern-engine, tracking
origin/dev/modern-engine, 0 ahead/behind, at
68956fd653fb3d03a4f371b16ba61890421081cf
(X64.6: replace external D3DX texture creation helpers).
The bounded full native graph and separate title-link probes reproduced the
baseline before any production edit. Both titles really compiled the legacy
renderer; no name shim, fake assembler, linker suppression or excluded backend
was used to remove the dependency.

| Native title-link evidence | Before | After |
| --- | ---: | ---: |
| Unique D3DX helpers per title | 5 | 4 |
| Combined LNK2019 | 10 | 8 |
| Combined LNK2001 | 8 | 8 |
| Combined LNK1120 | 2 | 2 |

Exactly the two D3DXAssembleShader LNK2019 occurrences disappear, one per title
from gameenginedevice.lib(W3DWater.cpp.obj), ReAcquireResources. All other
unresolved occurrences/counts match. No new dependency or game compiler blocker
appeared. Neither native game links yet. The bounded full graph still fails on
these links and the separate exact 30 tool ABI compiler errors (W3DView,
GUIEdit, WorldBuilder): C2065=8, C2440=6, C2555=4, C2664=8, C2737=4.
The complete compiler diagnostic-line multiset matches the baseline.

## Production inventory and original contract

There were exactly four direct production calls, in shared Core code (not
four additional copies per title). Each source was a pointer to immutable
string-literal storage with static lifetime. Water reassigns the local pointer,
not the source bytes. No generated input, macro expansion or runtime editing.

| Original route | Model | Source bytes excluding NUL | Code bytes / DWORDs | Handle |
| --- | --- | ---: | ---: | --- |
| W3DWater.cpp, ReAcquireResources, first | ps.1.1 | 282 | 188 / 47 | m_riverWaterPixelShader |
| Same, second | ps.1.1 | 229 | 124 / 31 | m_waterPixelShader |
| Same, third | ps.1.1 | 341 | 132 / 33 | m_trapezoidWaterPixelShader |
| W3DProfilerFrameCapture.cpp, Capture | ps.1.4 | 136 | 152 / 38 | m_swizzleShader |

All original calls used strlen(source), flags=0, ppConstants=nullptr,
ppCompiledShader=&compiledShader, ppCompilationErrors=nullptr. The actual
DX8 signature has a **constants-output** fourth argument; it has no modern
macro/include arguments. No preprocessor or include resolver is involved.
Water used hr==0 before device creation; the profiler returned on FAILED(hr).
Successful assembly buffers were used synchronously for CreatePixelShader and
released immediately. No error buffer was requested. Water did not branch on
device creation failures; the profiler did return on FAILED(device HRESULT).

Water creation remains chipset-gated at DC_GENERIC_PIXEL_SHADER_1_1, in original
river/water/trapezoid order. ReleaseResources still deletes and clears the same
handles; ReAcquireResources, shader selection, wave-file routes, constants,
water math, render states, textures, effects and blending are unchanged.
The profiler still creates lazily when m_swizzleShader==0, returns on device
failure, and deletes the shader in its destructor. Its entire production TU,
class and new swizzle data remain guarded by PROFILER_ENABLED.

## Pinned characterization before replacement

Oracle: actual Win32 d3dx8.lib from min-dx8-sdk pinned at
7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc, populated at
build/win32/_deps/dx8-src. No modern compiler or inferred opcode generation.
An ignored standalone Win32 recorder extracted the four original literal
expressions before editing; MSVC performed their original backslash-newline
splicing. It assembled each original byte sequence and saved source/token
binaries, complete token text, length and HRESULT under build/x64-7-*.
This retains the original embedded tabs/leading spaces and final-newline
differences, rather than normalizing shader whitespace.

All four returned exactly S_OK (0), a nonnull code buffer and no error buffer
when a diagnostic output pointer was additionally supplied. The checked-in
live reference test repeats the exact assembly against independently frozen
original inputs and compares every output byte to production code. Production's
null error-output parameter is intentionally not turned into a runtime error
buffer dependency. No shader declares assembler constant output.

Every output is DWORD-aligned with no trailing padding beyond END. It begins
with ffff0101 (water) or ffff0104 (profiler), followed by a nine-DWORD COMMENT
containing the pinned assembler identity "D3DX8 Shader Assembler Version 0.91".
That comment is retained byte-for-byte, along with co-issue/instruction/operand
words and the final 0000ffff END. There are no extra terminal words. Full
original sources and token streams appear below; exact independent recorded
data is intentionally versioned in LegacyFixedPixelShaderReference.inc.

## Boundary and source identity

LegacyFixedPixelShader.h owns only four named constexpr char arrays and their
corresponding constexpr unsigned long token arrays, under
LegacyD3DX::FixedPixelShader. Windows DWORD is unsigned long on Win32/AMD64;
sizeof==4 and code byte sizes are asserted. Natural DWORD alignment is tested.
It needs only cstddef/cstdint, with no graphics SDK or implementation library.
There is no runtime assembler, parser, allocation, COM buffer or GPU ownership.
Existing device CreatePixelShader receives each correctly mapped const token
pointer and the original handle-output address directly.

Source identity has three independent layers:

- Compile-time original lengths and recorded 32-bit FNV-1a fingerprints
  (RiverWater=80cea9fe, Water=8ea2cc07, TrapezoidWater=52b919be,
  ProfilerSwizzle=22158680) reject accidental source edits with clear messages.
- C++ tests compare the actual production sources, including NUL, and every
  code byte against independent recorded literals; hashes are not the oracle.
- Python tests compare actual production source constants to the original Git
  baseline source expressions and pin mapping/order plus all source outside
  the narrowly replaced assembly blocks, including failure and cleanup paths.

A deliberate same-length source edit was compiled separately and failed the
expected static assertion. Changing a shader requires explicit regeneration
with the pinned library and review of the source, assertions and independent
reference; changing only source cannot silently reuse old bytecode.

## Intentional runtime contract change and preserved failures

Runtime assembly, its transient ID3DXBuffer ownership and its theoretical
assembly/allocation failure are removed. The four inputs are prevalidated;
there is no runtime assembly HRESULT to fabricate or suppress. The former
water hr==0 gates and profiler assembly-failure return are therefore retired.
Device CreatePixelShader calls still occur in the same successful-assembly
order, store HRESULTs and write the original handles. Water continues without
a new failure branch, as before; profiler device failure still returns and can
retry on a later capture with a zero handle. Device failure is not converted
into success, and no handle is invented. Device reset/release behavior is intact.

## Validation actually run

| Gate | Result |
| --- | --- |
| Full Win32 Release graph | PASS |
| Win32 CTest | 17/17 PASS |
| Direct title Google tests | 149/title PASS; existing 6 disabled/title |
| Focused renderer/path/profiler/developer/Bezier tests | 75/title PASS |
| CPU fixed bytecode tests | 2 PASS on Win32 and AMD64 |
| Win32 independent pinned-assembler comparison | 3 PASS; all four sources and complete token streams |
| Separate real Win32 DX8 fixed-shader check | PASS, no skip; all four paired creations S_OK, handles deleted |
| Prior texture reference/real-device, FVF and math comparisons | PASS in CTest |
| RB1/RB2/RB3, producer and Bezier coverage | PASS |
| D3D11.1 sentence / D3D11.2 UI, real WARP | PASS on Win32/AMD64 |
| Bounded full native graph and separate game links | Expected remaining failures; exact progression above |
| Independent native CTest | 9/9 PASS, all eight prior targets retained |
| Rendering Python | 28/28 PASS |
| Performance Python | 16/16 PASS |
| PowerShell parser | 9 files PASS |
| Preservation audit | PASS; 391 prior runtime/evidence files and accepted A.2 capture hashes |
| git diff --check | PASS |

The real-device test loads system Win32 d3d8.dll, creates a hidden 64x64 HAL
device with software vertex processing and FPU_PRESERVE, verifies ps.1.4
capability, and compares CreatePixelShader using live pinned output versus
fixed tokens for all four programs. It cleans up both shader handles, device,
window and DLL. Other hosts explicitly skip on missing device/version; this
host ran all four with S_OK on both routes. Creation is not a rendered visual
equivalence test, and no native AMD64 DX8 shader execution is claimed.

The default game graph compiles with profiler disabled. Separate bounded
compile probes compile the **actual** profiler TU for both titles on Win32 and
AMD64 with PROFILER_ENABLED and live Capture branches. Existing PCH include
content/settings are reused without the PCH binary; a probe-only forced header
sets connection=true and capture dimensions/interval to 256/500 while leaving
image publication a no-op. Symbols confirm real Capture and pinned swizzle
data are present, with no assembler dependency. This is compile coverage, not
a Tracy connection/runtime test. A separate default-header probe succeeds;
trying to reference swizzle data without PROFILER_ENABLED correctly fails.

Native fixed-shader test is machine 8664 / PE32+. Imports are Windows/CRT only;
its Ninja link edge uses GoogleTest and system libraries, no x86 D3DX archive,
D3D8 implementation, D3D11, DXGI or D3DCompiler. Existing renderer/D3D11 sources
are untouched; the CPU boundary does not depend on them. The default DX8
backend remains compiled in the native graph. Gameplay, timing, pathfinding,
network/save formats and tool ABI code are untouched.

Ignored evidence: build/x64-7-baseline-*, x64-7-final-*, x64-7-characteriz*,
x64-7-*-source.bin, x64-7-*-tokens.bin, x64-7-profiler-*, x64-7-native-*,
x64-7-win32-ctest.log, x64-7-direct-*, x64-7-focused-* and probe scripts/drivers.
No generated runtime copy, capture, analysis helper or log belongs in the commit.
The accepted stage4a2-B-final-2 capture remains ignored/local; B-cpu.etl remains
ignored/local at 2,469,396,480 bytes. No prior capture is overwritten.

## Remaining helpers and next task gate

The final linker inventory is exactly the following four names. Counts are
shared production source calls, verified again after migration.

| Helper | Calls and exact routes | Scope / boundary / risk |
| --- | --- | --- |
| D3DXCreateTextureFromFileExA | 1, dx8wrapper.cpp filename _Create_DX8_Texture | File decode, format conversion, BOX filtering/mips, managed DX8 upload. Default dimensions/UNKNOWN; missing fallback and P8 rejection. X64.6 covers allocation only; bounded D3D11 UI top-mip decoding is not an equivalent general loader. High risk. |
| D3DXFilterTexture | 6: DX8Wrapper surface _Create_DX8_Texture (1), W3DTreeBuffer::W3DTreeTextureClass::update (1), TerrainTextureClass::update (2 compile-time variants), TerrainTextureClass::updateFlat (1), AlphaEdgeTextureClass::update (1) | BOX mip generation from source level 0, no palette. Shares pixel conversion/filter policy and real-device access with other resource routes. High risk. |
| D3DXLoadSurfaceFromSurface | 4: dx8wrapper surface _Create_DX8_Texture (1), MissingTexture::_Init (1), SurfaceClass::Copy and Stretch_Copy (2) | Full/rect conversion, NONE/BOX/TRIANGLE scale/filter, null palettes, zero color key. Existing CopyRects fast route retained. High risk until measured. |
| D3DXGetErrorStringA | 2: dx8wrapper Log_DX8_ErrorCode and Non_Fatal_Log_DX8_ErrorCode | CPU diagnostics into initialized char[256], log only on D3D_OK; fatal wrapper still asserts. String/HRESULT/truncation/fallback policy needs pinned characterization; no resource or GPU ownership. Lower risk. |

The three resource helpers are a coherent group, not three arbitrary isolated
tasks. However, allocation negotiation from X64.6 does **not** characterize
image decoding, pixel conversion/packing, compressed formats, sampling/rounding,
rect edges, mips, locking, failure behavior or format coverage. A grouped
implementation now would still require substantial characterization first.
Removing three names numerically is insufficient evidence for safe migration.

**Exactly one recommended next task: X64.8 — pinned-reference CPU diagnostic
error-string compatibility for D3DXGetErrorStringA.** Characterize actual pinned
text, known/unknown HRESULTs, output buffers/truncation/termination and returned
HRESULTs first, then preserve the two existing logging/assert routes with a
small native CPU boundary and full reference/regression/link probes. This
independent low-risk dependency has no resource-policy prerequisites and is
the shorter safe next executable-unblocking step on present evidence. Expected
natural progression is 4 -> 3 helpers/title, subject to measurement. The larger
resource group will need characterization before a later coherent migration;
this is a limitation, not authorization to begin it. No next task is started.

## Exact production input bytes and recorded DWORD streams

The following C++ source literals explicitly encode every original tab/newline
and space. Concatenation yields exactly the assembler input without the NUL;
the header owns these production constants. Tokens are in complete order,
including the pinned comment and END, with no substitutions.

### RiverWater

```cpp
static constexpr char RiverWaterSource[] =
    "ps.1.1\n"
    " \t\t\ttex t0 \n"
    "\t\t\ttex t1\t\n"
    "\t\t\ttex t2\t\n"
    "\t\t\ttex t3\n"
    "\t\t\tmul r0.rgb, v0, t0 ; blend vertex color into t0. \n"
    "\t\t\tmov r0.a, t0 ; keep vertex alpha from fading the base water. \n"
    "\t\t\tmul r1, t1, t2 ; mul\n"
    "\t\t\tadd r1.rgb, r1, t3\n"
    "\t\t\tmul r1.rgb, r1, v0.a\n"
    "\t\t\t+mul r0.a, r0, t3\n"
    "\t\t\tadd r0.rgb, r0, r1\n"
    "";
```

```text
ffff0101, 0009fffe, 58443344, 68532038, 72656461, 73734120
6c626d65, 56207265, 69737265, 30206e6f, 0031392e, 00000042
b00f0000, 00000042, b00f0001, 00000042, b00f0002, 00000042
b00f0003, 00000005, 80070000, 90e40000, b0e40000, 00000001
80080000, b0e40000, 00000005, 800f0001, b0e40001, b0e40002
00000002, 80070001, 80e40001, b0e40003, 00000005, 80070001
80e40001, 90ff0000, 40000005, 80080000, 80e40000, b0e40003
00000002, 80070000, 80e40000, 80e40001, 0000ffff
```

### Water

```cpp
static constexpr char WaterSource[] =
    "ps.1.1\n"
    " \t\t\ttex t0 \n"
    "\t\t\ttex t1\t\n"
    "\t\t\ttexbem t2, t1 ; use t1 as env map adjustment on t2.\n"
    "\t\t\tmul r0,v0,t0 ; blend vertex color into t0. \n"
    "\t\t\tmul r1.rgb,t2,c0 ; reduce t2 (environment mapped reflection) by constant\n"
    "\t\t\tadd r0.rgb, r0, r1";
```

```text
ffff0101, 0009fffe, 58443344, 68532038, 72656461, 73734120
6c626d65, 56207265, 69737265, 30206e6f, 0031392e, 00000042
b00f0000, 00000042, b00f0001, 00000043, b00f0002, b0e40001
00000005, 800f0000, 90e40000, b0e40000, 00000005, 80070001
b0e40002, a0e40000, 00000002, 80070000, 80e40000, 80e40001
0000ffff
```

### TrapezoidWater

```cpp
static constexpr char TrapezoidWaterSource[] =
    "ps.1.1\n"
    " \t\t\ttex t0 ;get water texture\n"
    "\t\t\ttex t1 ;get white highlights on black background\n"
    "\t\t\ttex t2 ;get white highlights with more tiling\n"
    "\t\t\ttex t3\t; get black shroud \n"
    "\t\t\tmul r0,v0,t0 ; blend vertex color and alpha into base texture. \n"
    "\t\t\tmad r0.rgb, t1, t2, r0\t; blend sparkles and noise \n"
    "\t\t\tmul r0.rgb, r0, t3 ; blend in black shroud \n"
    "\t\t\t;\n"
    "";
```

```text
ffff0101, 0009fffe, 58443344, 68532038, 72656461, 73734120
6c626d65, 56207265, 69737265, 30206e6f, 0031392e, 00000042
b00f0000, 00000042, b00f0001, 00000042, b00f0002, 00000042
b00f0003, 00000005, 800f0000, 90e40000, b0e40000, 00000004
80070000, b0e40001, b0e40002, 80e40000, 00000005, 80070000
80e40000, b0e40003, 0000ffff
```

### ProfilerSwizzle

```cpp
static constexpr char ProfilerSwizzleSource[] =
    "ps.1.4\n"
    "texld r0, t0\n"
    "mov r1.a, r0.r\n"
    "mov r2.a, r0.g\n"
    "mov r3.a, r0.b\n"
    "mul r0.rgb, r3.a, c0\n"
    "mad r0.rgb, r2.a, c1, r0\n"
    "mad r0.rgb, r1.a, c2, r0\n"
    "";
```

```text
ffff0104, 0009fffe, 58443344, 68532038, 72656461, 73734120
6c626d65, 56207265, 69737265, 30206e6f, 0031392e, 00000042
800f0000, b0e40000, 00000001, 80080001, 80000000, 00000001
80080002, 80550000, 00000001, 80080003, 80aa0000, 00000005
80070000, 80ff0003, a0e40000, 00000004, 80070000, 80ff0002
a0e40001, 80e40000, 00000004, 80070000, 80ff0001, a0e40002
80e40000, 0000ffff
```

## Exact changed-file inventory

- Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp
- Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DProfilerFrameCapture.cpp
- Core/Libraries/Source/WWVegas/WW3D2/LegacyFixedPixelShader.h
- Tests/Google/Core/Libraries/LegacyFixedPixelShaderReference.inc
- Tests/Google/Core/Libraries/LegacyFixedPixelShaderTest.cpp
- Tests/Google/Core/Libraries/LegacyFixedPixelShaderDeviceTest.cpp
- Tests/Google/Dependencies/CMakeLists.txt
- scripts/rendering/test_fixed_shader_reference.py
- docs/modernization/X64_7_FIXED_SHADER_BYTECODE.md
- docs/modernization/X64.md
- docs/modernization/RENDERER.md
- docs/modernization/ROADMAP.md
- docs/modernization/AGENT_HANDOFF.md

The reference .inc is intentionally versioned oracle data, not disposable build output.
