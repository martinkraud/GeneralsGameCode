# X64.4: native CPU compatibility for external D3DX math

Started 2026-10-11 from clean synchronized
`0f6704ce8ed8bde06b0374645a5960f4bd051b91`, `dev/modern-engine` tracking
`origin/dev/modern-engine`, zero ahead/behind. DX8 remains the default.
No game launch, renderer migration, gameplay algorithm change, layout change,
or performance optimization is part of this stage.

## Reference inventory and actual callers

Fresh bounded native full-graph and g_generals/z_generals link probes were run
before edits (`build/x64-4-baseline-*`). Each title had 17 unresolved D3DX helper
names; aggregate diagnostics were LNK2019=34, LNK2001=24, LNK1120=2.
The full graph had the same 30 tool ABI C++ errors as D3D11.2.

| External math helper | Direct calls in the eight migrated production files |
|---|---:|
| D3DXMatrixInverse | 20 |
| D3DXMatrixMultiply | 6, plus SDK matrix operators |
| D3DXMatrixRotationZ | 1 |
| D3DXMatrixScaling | 13 |
| D3DXMatrixTranslation | 11 |
| D3DXMatrixTranspose | 2 |
| D3DXVec3Transform | 1 |
| D3DXVec4Transform | 4 |

The pinned SDK declarations are in d3dx8math.h. Matrix `operator*`/`operator*=`
in d3dx8math.inl also call external MatrixMultiply. Merely renaming direct
calls would leave that dependency. Those audited expressions now use the
engine boundary while retaining parentheses, multiplication order and aliases.
Inline Identity and Vec4Dot remain unchanged; they are not external link blockers.

| Production file | Use and deterministic-risk classification |
|---|---|
| Common/Bezier/BezierSegment.cpp | **Simulation-authoritative/shared**: evaluate and subdivision; approximate length can select projectile flight tick count |
| Common/Bezier/BezFwdIterator.cpp | **Simulation-authoritative/shared**: generates projectile trajectory samples |
| GameClient/Water/W3DWater.cpp | Presentation: water UV/view/projection/reflection and shader constants |
| GameClient/W3DTreeBuffer.cpp | Presentation: composite/transposed shader matrix; output aliases rhs then input |
| GameClient/W3DShaderManager.cpp | Presentation: shroud, cloud, terrain, mask and road texture matrices |
| GameClient/TerrainTex.cpp | Presentation: lightmap/cloud texture transforms, including in-place multiply |
| WW3D2/pointgr.cpp | Presentation: non-billboard orientation; angle derived from byte /255 *2 *PI |
| WW3D2/sortingrenderer.cpp | Presentation: world/view composition, transformed sphere and draw sorting |

Both titles' DumbProjectileBehavior::calcFlightPath constructs BezierSegment,
calls getApproximateLength and getSegmentPoints, then consumes m_flightPath in
update. Flight segment count/speed/start/end are transferred in snapshot code;
loadPostProcess regenerates the curve. Thus floating-point changes can affect
flight duration, collision timing, restored trajectories and lockstep. These
callers and serialization source/layouts were not changed. Presentation files
have no newly serialized matrix state in this migration.

Other helpers were classified and deliberately retained:

- Resource/format/filter/copy: D3DXCreateTexture, D3DXCreateCubeTexture,
  D3DXCreateVolumeTexture, D3DXCreateTextureFromFileExA, D3DXFilterTexture,
  D3DXLoadSurfaceFromSurface.
- Shader compilation: D3DXAssembleShader (water and profiler capture).
- CPU vertex layout and diagnostics, outside this math scope:
  D3DXGetFVFVertexSize and D3DXGetErrorStringA.
- WorldBuilder also references D3DXCreateFont; that is a tool-only reference,
  not a newly exposed game-link helper in these probes.

## Compatibility boundary and matrix conventions

`WWMath/LegacyD3DXMath.h/.cpp` provides eight engine-owned operations in the
LegacyD3DX namespace and a value-returning Multiply convenience for the old SDK
operators. A separate static target links **headers only**, with no D3DX/D3D8
binary, renderer library or D3D11 dependency. Existing D3DXMATRIX/VECTOR layouts
are retained to avoid retyping/casting surrounding game code. This removes
external math implementation linkage, not the SDK declaration dependency.

The actual pinned structures are sixteen contiguous row-major floats for a
matrix (64 bytes), VECTOR3=12 and VECTOR4=16. Tests and size assertions freeze
that layout. The helper convention is row-vector: `v * M`; Vec3 supplies w=1
without perspective division, Vec4 uses the supplied w. Translation is row 4,
columns 1..3. MatrixMultiply computes conventional `a * b`; a row vector
experiences a then b. Do not infer ordering from the old header's prose, or
change WWMath's separate column-vector interpretation/casts. Transpose swaps
row/column indices. RotationZ has +sin in _12 and -sin in _21; no coordinate
system or renderer handedness conversion is introduced.

All required pointers remain non-null preconditions, as in the audited release
code (no null guards). Inverse's determinant pointer is optional. Successful
pointer-returning operations return the exact output pointer. Value Multiply
does not use SDK `operator*`. Every operation consumes aliased input into
temporary scalar/result storage before committing output.

## FP discipline and inverse characterization

The reference is min-dx8-sdk commit
`7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc`, actual bundled x86 d3dx8.lib.
An isolated CPU probe and object disassembly established the default dispatch
as CPU=2 (SSE2) on this host. No game was used for characterization.
The library's lazy-init wrappers themselves call D3DXCpuOptimizations(TRUE);
the game source contains no dispatch override. The oracle uses that same init.

Important measured distinctions:

- MatrixMultiply accumulates `((p0+p1)+p2)+p3`.
- Vec3/Vec4 transforms accumulate `(p0+p1)+(p2+p3)` (Vec3's p3 is translation).
- Inverse uses the pinned cofactor/determinant operation sequence, not WWMath's
  existing inverse or Gaussian elimination. Its scalar expansion names each
  rounded intermediate; the legacy determinant reduction order is retained.
- Inverse uses an approximate scalar reciprocal followed by the exact legacy
  refinement `2*seed - det*(seed*seed)`. The single `_mm_rcp_ss` preserves that
  existing CPU instruction, rather than substituting division. It is not a
  new vectorized implementation or performance optimization. All cofactor and
  matrix/vector arithmetic is scalar C++, without manual packed SIMD or FMA.
- Zero or unordered determinant returns null and leaves **both** matrix output
  and optional determinant untouched. A diagonal 1e-10 scale succeeds; there
  is no epsilon singular cutoff. Identity inverse can contain 0x3f7fffff rather
  than exact 1 because of the reference reciprocal refinement. This is retained.
- Rotation uses double CRT sin/cos then rounds to float. It matches the reference
  x87 FSINCOS bits for all 256 actual point-orientation angles on both tested
  architectures; arbitrary huge angles are not certified by that bounded proof.

The compatibility target alone uses MSVC `/fp:strict /Od`, overriding Release
`/O2`. This deliberately prevents auto-vectorization/reassociation and FP
contraction; no global compiler policy or caller flags change. No intentional
FMA or fast-math is introduced. The target does not alter rounding/FTZ/DAZ or
x87 controls. Other compilers have explicit contraction/fast-math/vectorization
restrictions but were not validated in this stage.

The contract is the measured modern-x86 SSE2 reference, not an assertion that
all old C/x87/3DNow/SSE dispatch variants or all CPU reciprocal seeds produce
identical bits. Reference recorder/live comparison reject a different dispatch.
Cross-machine, exceptional-value and altered-FP-environment equivalence beyond
the tested cases remains unverified. No multiplayer/replay certification is
claimed from unit tests.

## Reference and production tests

The Win32-only reference recorder links the actual pinned SDK library and never
links the replacement. Its checked-in `.inc` is a **test oracle**, containing
exact IEEE words for 32 cases across all eight operations and the 256 actual
orientation sine/cosine pairs. Inputs use explicit deterministic generation and
include identity, translation, scaling, quarter rotation, composition, transpose,
general/projective matrices, singular, near-singular and Bezier basis cases.
No replacement-generated expected values or broad epsilon tolerances are used.
Fixture whitespace is normalized; all 3,904 recorded IEEE words are unchanged.

`legacy_d3dx_math_test` uses this same recorded oracle on x86 and AMD64. The
separate Win32 live comparison covers 4,096 cases, reference/replacement aliases
and 4,096 Bezier-basis vectors. Alias cases include out==lhs, out==rhs,
out==both, transpose/inverse in place, vector in place, overlapping Vec3/VECTOR4,
optional null determinant, success return pointers and unchanged failure output.

Both title test binaries additionally execute the actual production
BezierSegment evaluation at 4,097 t values and getSegmentPoints at 2/3/17/64/257
steps against the pinned library/legacy forward-difference calculation. These
tests specifically cover the authoritative projectile curve route; runtime
collision, replay, save/load and cross-machine multiplayer remain untested.

## Validation and native progression

Stable-source final validation used VS 18 / MSVC 19.51 (14.51) and SDK
10.0.26100, Release, the established Ninja Multi-Config win32/win64 graphs.
The final source/build/test hash manifest matches all 18 reviewed files.

| Validation | Result |
|---|---|
| Full Win32 Release game/tool/test graph | PASS; both games and tools link |
| Win32 CTest | 9/9 PASS |
| Direct Generals / Zero Hour Google tests | 149/149 PASS per title; six existing disabled per title |
| Focused path/profiler/developer/renderer/Bezier | 75/75 PASS per title |
| RB1/RB2/RB3 submission/texture and sentence producer | 15/15 and 5/5 PASS on x86/AMD64 |
| D3D11.1 real WARP sentence / D3D11.2 UI content | 7/7 and 9/9 PASS on x86/AMD64 |
| Independent AMD64 CTest | 6/6 PASS, including five exact-bit math tests |
| Win32 math oracle / live comparison | 5/5 and 8/8 PASS, all comparisons exact |
| CMake-built legacy-only recorder | All 3,904 words and recorder tokens match; whitespace normalized |
| Rendering / performance Python | 25/25 and 16/16 PASS |
| PowerShell parser | All nine existing scripts PASS |
| Mechanical/protected-source audit | Eight exact call migrations; D3D11 and both projectile behavior sources unchanged |
| Preservation audit | 391 prior files and final A.2 capture hashes PASS |
| git diff --check | PASS, including final staged scope |

Native binary inspection identifies PE machine 8664 (AMD64), Windows/CRT
imports only, and no D3D8/D3DX/D3D11/DXGI import. The math test's native link
does not use the x86 SDK archive. The implementation's actual compile command
has local /fp:strict /Od, no LTCG; disassembly has scalar arithmetic/RCPSs and
no packed mul/add/sub/div or FMA. This is a CPU compatibility proof, not a
playable-native-game or performance claim. The accepted D3D11 implementation,
file texture bridge, selection/default and graphics settings are unchanged.

Final bounded full Win64 graph (`cmake --build --preset win64 --parallel 4 --
-k20`) still fails, as expected. Its exact compiler diagnostic multiset equals
the pre-edit baseline's 30 tool errors: C2065=8, C2440=6, C2555=4, C2664=8,
C2737=4. These are W3DView window-procedure/hook/timer/DoModal ABI, GUIEdit
dialog-procedure ABI and WorldBuilder UINT/UINT_PTR message-handler ABI errors.
They are outside the separate game executable dependency graph and were not
fixed. WorldBuilder's source-only D3DXCreateFont also remains outside the game
helper inventory.

Both separate native game probes reach linkage but neither links. All eight
math names disappear naturally; no external-name shim, fake device/resource
operation, generic replacement inverse, or linker suppression was added.

| Native title-link diagnostic (both titles combined) | Before | After |
|---|---:|---:|
| Unique D3DX helpers per title | 17 | 9 |
| LNK2019 occurrences | 34 | 18 |
| LNK2001 occurrences | 24 | 8 |
| LNK1120 occurrences | 2 | 2 |

The remaining nine names, with unchanged individual diagnostic occurrences:

- D3DXAssembleShader
- D3DXCreateCubeTexture
- D3DXCreateTexture
- D3DXCreateTextureFromFileExA
- D3DXCreateVolumeTexture
- D3DXFilterTexture
- D3DXGetErrorStringA
- D3DXGetFVFVertexSize
- D3DXLoadSurfaceFromSurface

There are no newly exposed unresolved symbols or game compiler errors in the
probes. Native runtime/device/resource behavior remains untested because no
native game executable exists yet. Ignored evidence logs are under
build/x64-4-baseline-*, x64-4-final-*, x64-4-native-*, x64-4-win32-ctest.log,
x64-4-direct-*, x64-4-focused-* and x64-4-validated-source.json. Those logs and
helpers are not source or committed artifacts.

## Next-task gate

Recommend exactly one next task: **X64.5 - reference-tested native CPU FVF
vertex-stride compatibility for D3DXGetFVFVertexSize.**

This is based on the actual final game links: all remaining game failures are
the nine helpers above, rather than tool ABI failures or a newly exposed
non-D3DX dependency. FVF stride is a bounded integer CPU operation with three
audited call sites: WW3D2/dx8fvf.cpp's Get_FVF_Vertex_Size/FVFInfoClass and
GameClient/HeightMap.cpp's two buffer/stride uses. It can remove one further
helper from each game without device/resource emulation or runtime shader
assembler replacement. A tool ABI fix would not remove a current game-link
failure. This is smaller and lower risk than the six resource helpers or
shader assembly; diagnostic formatting alone offers less substantive progress.

Characterize the pinned Win32 results for actual FVF layouts, position/blend,
normal/point-size/color and texture-coordinate dimensions, including meaningful
reserved/invalid cases before defining their contract. Add an engine-owned
integer boundary and native exact-result/layout tests; preserve vertex formats,
offsets, strides and device calls. Require the same full reference regression
suite and natural 9-to-8 native helper reduction, with no unrelated change.
Do not broaden it into a renderer rewrite or start it in X64.4. Further resource,
shader and eventual native runtime gates remain; this is not a first-playable
promise.

## Exact changed-file inventory

Production/build (11):

- Core/GameEngine/Source/Common/Bezier/BezFwdIterator.cpp
- Core/GameEngine/Source/Common/Bezier/BezierSegment.cpp
- Core/GameEngineDevice/Source/W3DDevice/GameClient/TerrainTex.cpp
- Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp
- Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp
- Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp
- Core/Libraries/Source/WWVegas/WW3D2/pointgr.cpp
- Core/Libraries/Source/WWVegas/WW3D2/sortingrenderer.cpp
- Core/Libraries/Source/WWVegas/WWMath/CMakeLists.txt
- Tests/Google/Core/CMakeLists.txt
- Tests/Google/Dependencies/CMakeLists.txt

New compatibility implementation/tests (7):

- Core/Libraries/Source/WWVegas/WWMath/LegacyD3DXMath.h
- Core/Libraries/Source/WWVegas/WWMath/LegacyD3DXMath.cpp
- Tests/Google/Core/GameEngine/Common/BezierCompatibilityTest.cpp
- Tests/Google/Core/Libraries/LegacyD3DXMathCases.h
- Tests/Google/Core/Libraries/LegacyD3DXMathReference.cpp
- Tests/Google/Core/Libraries/LegacyD3DXMathReference.inc
- Tests/Google/Core/Libraries/LegacyD3DXMathTest.cpp

Documentation (5):

- docs/modernization/X64_4_D3DX_MATH.md
- docs/modernization/X64.md
- docs/modernization/RENDERER.md
- docs/modernization/ROADMAP.md
- docs/modernization/AGENT_HANDOFF.md

The `.inc` is the intentionally versioned legacy test oracle. Build logs,
disassemblies, probe/generation/check helpers, executables and analysis output
stay under ignored build/. No runtime candidate or capture is changed. The
preservation audit verifies 391 existing runtime/evidence files and accepted
Stage 4A.2 final-capture hashes; the 2,469,396,480-byte B-cpu.etl remains local
and ignored. No generated runtime/performance artifacts belong in the commit.
