# Distribution runtime audit (project-history level)

Permanent invariant: **every completed runtime change -> the canonical staged runtime -> the ZIP -> a clean Steam install -> the same complete MKRaudstein game**, for
**both** titles. This document audits the whole history `adac468d7` (modernization baseline) .. `6df387db2` and the upstream TheSuperHackers distribution, and records
the proof. Reproduce the data with `scripts/native-runtime/audit_*.py` (see "How to reproduce"). Nothing here is inferred from ZIP size or file count.

## 1. History range and what was inspected

| Item | Value |
|---|---|
| Range | `adac468d7..6df387db2` on `dev/modern-engine` (HEAD at audit start) |
| Commits | 221 |
| Files changed | 705 (every one classified; CSV in `evidence/distribution-audit/changed-files.csv`) |
| Compiled-into-exe test | the **real Win64 Release ninja link graph** (sources + header deps of `generalsv.exe` / `generalszh.exe`), not name heuristics |

| Category | Files | Linked into Generals exe | Linked into Zero Hour exe |
|---|---:|---:|---:|
| 1 compiled into the player exe | 314 | 267 | 261 |
| 2 compiled into another runtime binary/DLL | 0 (none ships in the player package; see section 5) | - | - |
| 3 runtime data as a loose repository file | 0 | - | - |
| 4 generated runtime data (generator: `modern_extensions.py`) | 1 | - | - |
| 5 shader / renderer resource | 0 (legacy pixel-shader bytecode is pinned *inside* the exe, X64.7) | - | - |
| 6 WND / UI layout resource in the repository | 0 (the Generals sidebar WNDs are *generated* by category 4) | - | - |
| 7 INI / game-data resource in the repository | 0 (the Generals INI overrides are *generated* by category 4) | - | - |
| 8 dev / test / build / docs only | 388 (109 scripts, 81 tests, 184 docs/evidence, 14 build config) | - | - |
| 9 superseded / not linked | 2 (`SimulationWorkerPlan.h`: X64.38 design header with no includer; `debug_dlg.cpp`: debug-DLL helper, not part of any player binary) | - | - |
| 10 unchanged proprietary EA content | 0 repository files; at runtime supplied by the Steam install (BIG archives, `Data/**`) and **never redistributed** (validator proves it by path and SHA-256) | - | - |

The branch changes **no** loose runtime resource in the repository. All completed features are therefore either compiled code (-> `Generals.exe` in the ZIP) or the
data the stager generates (Generals only: 6 files). Search of the source tree for runtime resources (.ini .wnd shader files, DLL configs, renderer resources,
generated data, title-specific assets) found exactly these, each decided:

| Resource | Decision | Evidence |
|---|---|---|
| Legacy pixel-shader bytecode | **embedded** in the exe (X64.7 pinned tables) | no shader file loaded at runtime; D3D11 path uses compiled-in HLSL via `d3dcompiler_47` (a Windows system DLL) |
| `d3dcompiler_47.dll`, `d3d11.dll`, `dxgi.dll`, `d3d8.dll` string | **system** DLLs of Windows (imports proven to resolve with a Windows-only PATH by the validator); the `d3d8.dll` string is the legacy wrapper name, unused on the native path | validator "every imported DLL resolves" |
| Microsoft VC++ runtime (4 DLLs) | **copied** app-local into the ZIP, derived from the PE import table | package builder |
| Generals modern-extension `CommandButton.ini`, `CommandSet.ini`, `PlayerTemplate.ini`, `GenPowersShortcutBar{US,China,GLA}.wnd` | **generated + copied** (stage -> ZIP, 6 files) | X64.48B; feature_probe parity |
| `Dazzle.INI`, shatter tables/planes, `w3d_missing_texture.tga`, projector gradients (`RequiredAssets` in the texture-project tool) | **intentionally unused at runtime**: these are optional files of the *tool*/legacy loader; the exe contains only the names; not present in the Steam BIGs; the native game runs (human PASS x2 PCs) without them; no completed feature loads them | strings scan of both exes + BIG scan + human validation |
| `BrowserEngine.dll` (tracked in `Generals/Run`, `GeneralsMD/Run`) | **intentionally unused**: optional `LoadLibrary` (in-game web browser), not required; same as upstream (not in upstream ZIPs) | source + upstream workflow |
| `DebugWindow.dll`, `ParticleEditor.dll` | **intentionally excluded** from the player package: loaded only with `m_scriptDebug` / `m_particleEdit` developer options | `ScriptEngine.cpp` |
| Bink / Miles | no Bink in 64-bit (movies skipped, documented limitation); Miles AIL mixer replaced by the native AMD64 mixer compiled into the exe (X64.31) | README limits |

Staging defects found by the audit: **none beyond the one already fixed in X64.48B** (the 6 generated Generals files, previously omitted). No feature-required
artifact is missing from either staged candidate.

## 2. Milestone -> runtime location (every completed milestone)

Legend: **EXE** = compiled C++ in `Generals.exe` (the AMD64 build shipped under Steam's launch name); **DATA** = loose files in the ZIP; **PASS** = present in
stage **and** ZIP (the ZIP *is* the exe + data). "n/a" = nothing separate to package; the justification is given. Counts are classified files touched by the milestone's commits
(`milestones.json`): G / Z = files linked into the Generals / Zero Hour exe.

| Milestone / feature | Runtime implementation | Runtime file(s) | Generals | Zero Hour | In stage? | In ZIP? | Status |
|---|---|---|---|---|---|---|---|
| Stage 1-3 timing (sim rate, render FPS, interpolation, presentation timing, ground) | EXE (G32 / Z33 files) | `Generals.exe` | yes | yes | yes | yes | PASS |
| Stage 4A/4B profiling + path-policy | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.1-X64.3 Win64 foundation, pointer-width diagnostics, DX8 declaration split | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| RendererBoundary.1-3, D3D11.1-2 (Render2D, sentence content, UI textures) | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.4-X64.10 D3DX replacement (math, FVF, textures, errors, shader bytecode pinning, resource conversion) | EXE (shader bytecode embedded) | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.11-X64.15 startup readiness, FP policy, headless startup, shutdown capture, ObjectPool offset | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.16-X64.24 graphical smoke gates, retained display, menu, skirmish load | EXE (+ stager/test scripts: dev only) | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.25-X64.30 terrain, world objects, multi-pass, radar, effects | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.31 native AMD64 audio mixer | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.32 shortcut sidebar / original-experience QoL | EXE + **DATA (Generals): 6 generated files** | `Generals.exe`, `Data/INI/{CommandButton,CommandSet,PlayerTemplate}.ini`, `Data/Window/GenPowersShortcutBar{US,China,GLA}.wnd` | yes (EXE+DATA) | yes (EXE only: Zero Hour already ships this data) | yes | yes | PASS |
| X64.33 central display geometry / ultrawide | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.34 smooth presentation (aircraft, projectiles, turrets) | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.35 renderer instrumentation | EXE (log checkpoints) | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.36 Enhanced HUD (USA prototype) | EXE | `Generals.exe` | active | inert / deferred (documented; Zero Hour uses the classic control bar) | yes | yes | PASS (Zero Hour: documented limitation in the ZIP README) |
| X64.37 Modern Display / Settings | EXE (settings stored in the user's data folder at runtime) | `Generals.exe` | yes | yes (3 ZH-only options hidden, documented) | yes | yes | PASS |
| X64.38 CPU scalability foundation | EXE (tick stats); `SimulationWorkerPlan.h` unused design header (cat 9) | `Generals.exe` | yes | yes | yes | yes | PASS (cat-9 header justified: no includer, no runtime effect) |
| X64.39-X64.42 ZoneCalc / path-search / passability memo / script-tick flush | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.43 documentation | docs only | - | n/a | n/a | n/a | n/a | PASS (no runtime artifact by definition) |
| X64.44 endurance telemetry (opt-in) | EXE (env-var gated) | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.45 Zero Hour bring-up | EXE (27 files linked in Z) | `Generals.exe` | n/a (shared) | yes | yes | yes | PASS |
| X64.46 ChallengeLoadScreen null movie stream | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.47A/B overlay naming + packaging | EXE (exact `generals.exe` normal-launch rule) + packaging scripts | `Generals.exe`, README, licence, manifest | yes | yes | yes | yes | PASS |
| X64.48A second-PC fixes (terrain draw window, shroud batch, skirmish slots 7/8) | EXE | `Generals.exe` | yes | yes | yes | yes | PASS |
| X64.48B complete runtime parity | packaging + validator | ZIP == stage | yes | yes | yes | yes | PASS |
| Housekeeping (docs, README, merges, cleanup) | none | - | n/a | n/a | n/a | n/a | PASS (no runtime artifact) |

Dev-only aids (autoshot / autoflow / forced client size / endurance / feature probe) are compiled into the same exe, off by default, and are exercised by the validator.

## 3. Upstream TheSuperHackers distribution baseline

Source of truth: `.github/workflows/weekly-release.yml` + `reusable-build-toolchain.yml`. The weekly release builds the `vc6-weekly` preset (Win32, **tools ON, extras OFF**)
and the "Collect Artifact" step does `zip -jr` of every `.exe`, `.dll` and `.pdb` that lies directly in `<build>/Core` and `<build>/<Game>`. Published assets
(GitHub release metadata only; nothing was downloaded): `generals-weekly-2026-10-16.zip` 31,987,764 B, `generalszh-weekly-2026-10-16.zip` 33,838,193 B.
The artifact set was derived from the build graph and cross-checked against the published sizes (see the caveat below). It is exactly: Generals 8 binaries + 8 PDBs = **16 files**; Zero Hour 9 binaries + 9 PDBs = **18 files**.

"Linked changed files" = number of the 705 branch-changed files linked into that artifact (Win64 graph). "Still built" = builds as AMD64 with the project's win64 preset.

| Upstream artifact | Upstream ZIP? | Purpose | Still built by MKRaudstein? | Modified by MKRaudstein? | Required at player runtime? | MKRaudstein destination | Reason |
|---|---|---|---|---|---|---|---|
| `generalsv.exe` (G) / `generalszh.exe` (ZH) | yes | the game | yes (AMD64) | **yes** (267 / 261 changed files linked) | **yes** | player ZIP as `Generals.exe` (name Steam launches) | the product |
| `generalsv.pdb` / `generalszh.pdb` | yes | symbols | yes | yes | no | Symbols ZIP | symbols are not functionality |
| `DebugWindow.dll` (+pdb) | yes | script-debug window | yes (AMD64) | 1 changed file linked | no (loaded only with `m_scriptDebug`) | excluded from the player package; not produced into a tools package in this milestone | developer feature; **UNRESOLVED-low**: not shipped in a Tools package yet |
| `ParticleEditor.dll` (+pdb) | yes | particle editor | yes (AMD64) | 14 changed files linked | no (only with `m_particleEdit`) | same | developer feature; UNRESOLVED-low (no Tools package yet) |
| `W3DViewV.exe` / `W3DViewZH.exe` (+pdb) | yes | W3D model viewer | **no**: fails to compile for AMD64 (MFC/pointer-width code, unmodified upstream sources) | 66 changed shared files linked (not edited for it) | no | none (cannot be produced as AMD64); Win32 tool of upstream remains usable by modders | pre-existing AMD64 porting gap; UNRESOLVED (needs a tools-port milestone) |
| `WorldBuilderV.exe` / `WorldBuilderZH.exe` (+pdb) | yes | map editor | **no** (same reason) | 265 / 259 shared files | no | none | as above; UNRESOLVED |
| `guiedit.exe` (G and ZH) (+pdb) | yes | WND GUI editor | **no** (same reason) | 265 / 259 shared | no | none | as above; UNRESOLVED |
| `imagepacker.exe` (G, ZH) (+pdb) | yes | texture atlas packer | yes (AMD64) | 265 / 259 shared | no | excluded from player package; candidate for a Tools package | modding tool |
| `mapcachebuilder.exe` (G, ZH) (+pdb) | yes | map cache builder | yes (AMD64) | 265 / 259 shared | no | excluded; candidate for a Tools package | modding tool; the game builds its own map cache at runtime |
| `wdump.exe` (+pdb) (ZH only) | yes | W3D dump tool | yes (AMD64) | 66 shared | no | excluded; candidate for a Tools package | modding tool |

**Quantitative accounting.**

| Package | Files |
|---|---:|
| Upstream Generals weekly | 16 (8 binaries + 8 PDB) |
| Upstream Zero Hour weekly | 18 (9 binaries + 9 PDB) |
| MKRaudstein Generals player ZIP | 14 = `Generals.exe` + 4 app-local VC++ DLLs + README + licence + manifest + **6 generated data files** |
| MKRaudstein Zero Hour player ZIP | 8 = `Generals.exe` + 4 app-local VC++ DLLs + README + licence + manifest |
| Symbols ZIPs | 1 PDB each |

Difference per title: of the upstream 16 / 18 files, **2 / 2 are carried** (game exe; its PDB -> Symbols ZIP). The other 14 / 16 are 7 / 8 non-game binaries and their PDBs. Of those, 3 (W3DView, WorldBuilder, guiedit) **cannot be built for AMD64 today** (pre-existing; UNRESOLVED), and 4 / 5 (DebugWindow.dll, ParticleEditor.dll, imagepacker, mapcachebuilder, plus wdump in Zero Hour) build but are intentionally not in the player package (developer/modding tools; no Tools package exists yet). MKRaudstein adds files upstream does not have: 4 CRT DLLs (upstream targets machines with the VC6-era runtime; an AMD64 build needs the
VC++ runtime and is shipped app-local), README, licence, manifest, and for Generals the 6 generated data files. Size: upstream ZIPs are ~32 / ~34 MB because they include ~8 Win32 PDBs with full debug info and tools;
MKRaudstein player ZIPs are ~4.0 / ~4.1 MB because debug info is in the Symbols ZIP and the tools are not shipped. Caveat: re-packing the locally built Win32 artifact set gives a different absolute size
(different toolchain than upstream's VC6) but the Zero Hour / Generals size ratio is 1.047 against the published 1.058, supporting the derived file list. **No unexplained difference remains**; the three UNRESOLVED
unbuildable tools and the missing Tools package are explicit open items, not silent exclusions, and none is required by any completed MKRaudstein player functionality.

## 4. Permanent dual-title workflow

`scripts/native-runtime/New-MKRaudsteinDistribution.ps1` (one invocation, no per-title parameter required):

```
pwsh scripts/native-runtime/New-MKRaudsteinDistribution.ps1            # stage both fresh candidates from the current win64 build, package both, validate both
pwsh scripts/native-runtime/New-MKRaudsteinDistribution.ps1 -Build     # same, rebuilding the two executables first
```

For both titles it stages (or takes `-GeneralsCandidate` / `-ZeroHourCandidate`), builds the deterministic ZIP + Symbols ZIP + per-title manifest + stage-parity file, runs the full validator
(ZIP == manifest, ZIP == canonical stage, clean install + ZIP == stage file-for-file, imports resolve, feature probe + autoflow equality), and writes one combined
`MKRaudstein-x64-Preview-<version>.distribution.json` and `.SHA256SUMS`. Any failure for either title aborts with a non-zero exit. The single-title tools remain for development.
Contract test: `test_overlay_package_contract.py::test_one_distribution_command_covers_both_titles`.

## 5. Package contents, hashes and validation (final)

Produced by **one** invocation of `New-MKRaudsteinDistribution.ps1` (`-GeneralsCandidate x64-48b-gen-v2 -ZeroHourCandidate x64-48b-zh-v2`, source commit `83d6236b1`; the executables
are byte-identical to the ones in the 08d74caaa packages - only the commit recorded in README/manifest changed, hence new hashes). Both titles: **26 of 26 validator checks passed**
(ZIP == manifest; ZIP == canonical stage; no legitimate file redistributed; AMD64; clean install + ZIP == stage file-for-file; imports resolve with app-local VC++ runtime; feature probe + autoflow identical).

| Package | Files | Bytes | SHA-256 |
|---|---:|---:|---|
| `MKRaudstein-Generals-x64-Preview-83d6236b1.zip` | 14 | 4,010,850 | `D2686251E49D35D5D3A3B454952F05A87D36AE2F9BA50DCF8A680AEB599B21B9` |
| `MKRaudstein-ZeroHour-x64-Preview-83d6236b1.zip` | 8 | 4,116,368 | `BC8411B1DE04C75E6F71B847E60E2021795AFE1127649E42D83829733C8851DF` |
| Generals Symbols ZIP | 1 PDB | - | `F347A374ACD3022835DE46E72A093BDD2DC68CB76574DD82D07E57C7C3296218` |
| Zero Hour Symbols ZIP | 1 PDB | - | `0ABA90AC54AA494626A1F0D7F38C38ACE4BD4B47460C19D461C786FD732442E1` |

Generals ZIP: `Generals.exe`, `Data/INI/{CommandButton,CommandSet,PlayerTemplate}.ini`, `Window/GenPowersShortcutBar{US,China,GLA}.wnd`, 4 VC++ DLLs, README, `MKRaudstein/LICENSE.md`, `MKRaudstein/manifest.json`.
Zero Hour ZIP: `Generals.exe`, 4 VC++ DLLs, README, `MKRaudstein/LICENSE.md`, `MKRaudstein/manifest.json`. Evidence: `evidence/distribution-audit/`. This is automated validation, **not** the human Steam test;
the earlier human PASS (X64.47B, second PC) covers the previous equal-binary packages.

## 6. Conclusion

**Generals: COMPLETE RUNTIME DISTRIBUTION PROVEN** - all 705 changed files classified against the real link graph; every completed milestone maps to the exe (all compiled code) or to the 6 generated
data files; both are in the canonical stage and in the ZIP; clean install + ZIP == stage file-for-file and feature-probe identical (26/26).

**Zero Hour: COMPLETE RUNTIME DISTRIBUTION PROVEN** - same evidence; Zero Hour needs no generated data (it already ships the shortcut bar); all 261 linked changed files are in `Generals.exe`.

Scope of the claim: proven for the project's own completed runtime changes against the player-runtime definition. Open, explicitly non-blocking items: (a) W3DView, WorldBuilder and guiedit do not build for AMD64
(pre-existing upstream porting gap, UNRESOLVED); (b) no Tools package yet for the buildable tools/DLLs (imagepacker, mapcachebuilder, wdump, DebugWindow.dll, ParticleEditor.dll); (c) the unused-at-runtime optional
resources (Dazzle.INI, shatter tables, projector gradients, BrowserEngine.dll) are not in the Steam BIGs and the native game runs without them (human PASS), so they are not staged. None is required by a completed player-facing feature.

## How to reproduce the audit

```
python scripts/native-runtime/audit_compiled_files.py   > build/audit-tmp/compiled.json     # Win64 link graph of both player exes
python scripts/native-runtime/audit_history_files.py                                         # classification of all 705 files -> changed-files.csv, summary.json
python scripts/native-runtime/audit_milestones.py                                            # per-milestone view
python scripts/native-runtime/audit_build_outputs.py build/win32 build/win64 > build/audit-tmp/outputs.json
python scripts/native-runtime/audit_upstream_artifacts.py > build/audit-tmp/upstream-artifacts.json
python scripts/native-runtime/audit_upstream_zip_estimate.py build/win32
```
