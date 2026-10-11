# X64.10: engine-owned legacy resource compatibility

Completed 2026-10-11 from accepted baseline
`9afa8cfac0ae78d4ec3d3838e9574b172b68e0c2` on `dev/modern-engine`.
The accumulated [X64.9 specification](X64_9_RESOURCE_CHARACTERIZATION.md),
including its 13/13 compressed and 9/9 overall gates, remains authoritative.

## Result and implementation boundary

The final three external resource helpers are replaced in both titles:
`D3DXCreateTextureFromFileExA`, `D3DXFilterTexture`, and
`D3DXLoadSurfaceFromSurface`. Their production callers now use the corresponding
`LegacyD3DX` functions. The external helper count is **3 -> 0 per title**.
Both actual native AMD64 title executables now link successfully.

`WW3D2/LegacyResource.cpp` separates file decoding, CPU conversion/filtering,
resource access/staging, and helper-specific ownership/failure policy.
`LegacyResourceEncoding.h` implements the accepted numerical encoder;
`LegacyResource.h` exposes the bounded helper interfaces. The `legacy_resource`
static target compiles with `/fp:strict` and links no external D3DX implementation.
Five caller files change only includes and helper names: `dx8wrapper.cpp`,
`surfaceclass.cpp`, `missingtexture.cpp`, `TerrainTex.cpp`, `W3DTreeBuffer.cpp`.
The dormant terrain call is migrated too. No gameplay or renderer selection changes.

The file helper reuses X64.6's `LegacyTextureCreation` capability/format/allocation
negotiation. Its ANSI disk loading and TGA/DDS decoding remain separate from the
UI CPU loader, whose bounded format and failure policies are not interchangeable.
Supported file cases retain orientation, indexed/grayscale/truecolor handling,
row-bounded RLE, authored DDS mips and generated mip policy. The quarantined
packet-spanning RLE anomaly is rejected safely, not reproduced as heap corruption.
Failure preserves the caller's output pointer; success does not release its prior
contents. Partial resources unwind through scoped COM ownership.

Surface conversion retains native packed fields, BOX half/odd-edge dispatch,
TRIANGLE coefficient construction/folded edges and accumulation order, and
float-to-codec handoff without an intermediate ARGB8 quantization. DXT1/3/5
encoding retains the pinned RGB refinement, error diffusion, selector projection,
ties/degenerate cases, alpha six/eight-step fitting and unusual 8/7 coefficient.
Small compressed mips use periodic extension and authored sub-block mips are
decoded/re-encoded as specified. This is not a generic resampler/compressor.

Access preserves the observed outer CopyRects attempt, lock flags/order,
DEFAULT-pool staging/readback, partial destination preservation, cleanup and
ignored final-upload HRESULT. It uses public COM interfaces, not D3DX's private
32-bit layouts. CPU alias handling snapshots the source. FilterTexture preserves
the adjacent-level walk, release order and partial failure behavior.

The APIs intentionally reject palettes, nonzero color keys and unsupported flags
outside the frozen caller contract; FilterTexture supports the production 2D,
source-level-zero, BOX route. This does not claim all-purpose D3DX API coverage.
Allocation exceptions return E_OUTOFMEMORY at allocating helper boundaries.

One old prose description needs clarification: for NONE with unequal extents,
the accepted corpus zero-fills uncovered output before conversion; it does not
leave those pixels untouched. The implementation follows the existing reference
bytes, and neither reference data nor the accepted historical report is rewritten.

## Numerical precision and tests

AMD64 uses explicitly ordered binary64 operations and legacy binary32 stores,
matching the accepted X64.9 PC_53 numerical corpus. Win32 uses explicit x87
operations preserving the caller's precision mode. Inspection of both titles'
existing GameLogic::setFPMode callers found PC_24 selection. Reusing the existing
471 encoder fixtures and fixed 32-block holdout, the production encoder matches
the pinned encoder for DXT1/3/5 under both default PC_53 and PC_24, and preserves
the caller precision. A decode comparison also reuses the existing fixtures.
These are distinguishing compatibility checks, not replacement reference bytes.

The original pinned Win32 target is retained. A second target runs the same
characterization harness with the three helper calls redirected to the candidate.
Direct Win32 encoder entry points remain oracle calls; explicit production encoder
tests compare candidate bytes independently. AMD64 direct encodings use the
candidate and compare against frozen references. No production lookup of fixture
names, IDs or expected outputs exists.

All complete evidence streams agree byte-for-byte, including events, HRESULTs,
pixels and metadata (not merely selected compact references):

| Stream | Rows | Win32 replacement vs accepted | AMD64 vs accepted |
| --- | ---: | --- | --- |
| recording | 3,010 | identical | identical |
| sampling | 334 | identical | identical |
| encoder | 1,899 | identical | identical |
| device | 371 | identical | not available |
| sampling-device | 380 | identical | not available |
| encoder-device | 171 | identical | not available |

The compact reference corpora (58/94/128) remain unchanged, with SHA-256:

- Resource: `8a6e6a0ec00b64348febe6b9b8a7728d38307ac8566d2f6dc53a923d2ca961a6`
- Sampling: `896ea0963c415c7105dfcf4d33e3b1371908d80619a284ee3137ab71549dfc81`
- Encoder: `c6dca00672df429f78857f1f1de5f1bf5d5202d6d8d99aba94cb93fd02028176`

## Final validation actually run

- Full Win32 Release build PASS; CTest **21/21**.
- Pinned resource target **10/10**; replacement resource target **10/10**,
  including real Win32 D3D8 device tests. No game process was launched.
- Direct game tests **149/title**, six existing disabled/title; focused **75/title**.
- Standalone sentence producer **5/5** and render2D/texture sequence **15/15**.
- AMD64 independent CTest **11/11**, using
  `ctest --test-dir build/win64 -C Release -E "^[zg]_googletest$" --output-on-failure`.
  The native resource executable has **3 CPU tests passed, 3 device tests skipped**:
  native system D3D8 is unavailable. This is not native device/runtime validation.
- Rendering Python **28/28**, performance Python **16/16**, PowerShell parse
  **9 scripts / 0 errors**, preservation audit and diff whitespace checks PASS.
  The preservation audit retains 391 prior runtime/evidence files and final A.2
  capture hashes. Source-preservation tests reverse only explicitly counted
  resource-helper substitutions before comparing all other tokens.

An initial unfiltered native CTest invocation registered thirteen targets and
reported the two unbuilt title GoogleTest executables as Not Run; its other
eleven targets passed. The established independent native suite was then run
explicitly with the exclusion above and passed. Full native title GoogleTests
are not claimed as validated.

Generated evidence stays under ignored `build/x64-10-resource-4-evidence` and
`build/x64-10-resource-8-evidence`; comparison scripts and all logs stay ignored
under `build/x64-10-*`. Accepted capture files and runtime copies are preserved.

## Exact native link result and remaining limits

In an AMD64 VS developer environment, actually ran:

```text
cmake --build --preset win64 --target z_generals g_generals --parallel 4 -- -k20
```

Exit **0**. Both `build/win64/Generals/Release/generalsv.exe` and
`build/win64/GeneralsMD/Release/generalszh.exe` linked. A final incremental rebuild
of both titles plus `legacy_resource_comparison` also exited **0**. Dumpbin
reports **8664 machine (x64)** for both executables; their import tables contain
no D3DX dependency. Production source contains no remaining calls to the three
external resource helpers. There are **no new native title linker blockers**.

The full native graph still exits **2** in existing tool ABI code, with **34**
compiler diagnostics, as in the accepted earlier broad probe. The fixed
twenty-object probe reproduces the exact previous **30-line diagnostic multiset**:
C2065=8, C2440=6, C2555=4, C2664=8, C2737=4. Specifically:

- W3DView ColorBar/ColorPicker: obsolete GWL_WNDPROC; DirectoryDialog hook,
  GraphicView timer callback, and DeviceSelectionDialog::DoModal return widths.
- Both GUIEdit titles: WinMain and ColorDialog DLGPROC return signatures.
- Zero Hour WorldBuilder: EditAction/EditCondition/MainFrm/WbView3d UINT vs
  UINT_PTR message-map signatures and their consequent initialization errors.
- The broad probe additionally reaches the already documented Generals
  WorldBuilder EditAction/EditCondition pair (four additional diagnostics).

No tool ABI repair was made. Exact logs: `build/x64-10-native-link.log`,
`build/x64-10-native-final-build.log`, `build/x64-10-native-full.log`,
`build/x64-10-native-tools-fixed.log`, `build/x64-10-tools-compare.log`;
PE reports are `build/x64-10-native-{g,z}-{headers,imports}.log`.

Successful title linkage does not establish a playable native runtime. Native
D3D8 device tests cannot run here; D3D11 coverage remains bounded to prior UI/
sentence work, and existing audio/video fallback limitations remain. The game's
PC_24 startup calls also require an AMD64 policy review; native CPU corpus
agreement is not proof of cross-architecture simulation/lockstep equivalence.
No Generals/Zero Hour launch or runtime candidate deployment occurred.

## Checkpoint and exactly one next task

One coherent X64.10 commit contains only the resource implementation, five caller
migrations, CMake/test integration, exact source-preservation allowances and
modernization documentation. No corpora, generated logs, captures, analysis
helpers, binaries or runtime outputs belong in it. Push destination is only
`origin/dev/modern-engine`, as authorized.

Exactly one recommended next task: **X64.11 native pre-launch readiness audit**.
Bound the actual renderer/dependency startup route and floating-point setup of
both now-linking AMD64 titles, identify concrete blockers and propose the smallest
safe first-runtime-validation scope. Keep this separate from tool ABI repairs
and do not treat successful linkage as permission to launch either game.
