# X64.47B - Generals Steam overlay ZIP feasibility (PREVIEW package)

Status: **OPEN - package built and automatically validated on a disposable layout; the real Steam test on a second PC is NOT done.** The Generals distribution model is **not validated** until the human performs the checklist in section 9. Companion of `X64_47A_ZERO_HOUR_STEAM_OVERLAY_ZIP.md` (same builder, manifest, validator, safety rules). Not a feature milestone; no performance or visual/options work was started.
Start point: HEAD `5d2144522` = `origin/dev/modern-engine`. The real Steam directory was only read (the launch configuration in `appcache`, a listing); nothing was written or executed there.

## 1. The package
| | |
| --- | --- |
| File | `MKRaudstein-Generals-x64-Preview-afe92caa2.zip` |
| Path | `build\dist\MKRaudstein-Generals-x64-Preview-afe92caa2.zip` (git-ignored; rebuild: `New-ZeroHourOverlayPackage.ps1 -Title Generals`, deterministic) |
| Size | 3,982,983 bytes |
| SHA-256 | `B3CC4BB19D802F36C8705312C103EC969F3DCAE652C1655F35D0FA22002F1F3D` |
| Built from | commit `afe92caa24a9d301c7777e854dd0ed6120589518` (later commits: validator fix and documentation only) |
| Companions | `.sha256`, `.manifest.json`, optional `-Symbols.zip` (the `.pdb`, developers only); evidence copy in `docs/modernization/evidence/x64-47b-generals-overlay/` |
Both preview ZIPs now exist: Zero Hour `...ZeroHour-x64-Preview-951547ec0.zip` (SHA-256 `C9D6C17E...5D04`) and Generals (above). They are **separate archives**; no unified launcher.

## 2. Audit: Steam launch chain for original Generals (do not assume it equals Zero Hour)
- Steam app **2229870** "Command & Conquer Generals", `installdir` **`Command and Conquer Generals`** (Zero Hour is app 2732960, `Command & Conquer Generals - Zero Hour`). Launch configuration (parsed read-only from `appinfo.vdf`): entry 0 executable **`Generals.exe`**, entry 1 `WorldBuilder.exe`. **Same executable name as Zero Hour**, in a different folder.
- **Limit of this audit:** the original Generals Steam app is **not installed on this machine** (only Zero Hour's app is; its `ZH_Generals\` subfolder holds the Generals base-game BIG archives and no executable). So the launcher -> `Game.dat` chain, the exact `Generals.exe` of the Generals depot (size / hash) and the folder contents could **not** be inspected, and the Zero Hour chain (32-bit EA launcher `Generals.exe` -> `Game.dat`) is **assumed**, not proven, to be the same. What is proven: Steam starts `Generals.exe`. The overlay replaces exactly that file with the AMD64 Generals runtime; whatever the original launcher does becomes irrelevant. **A real Generals installation (second PC) is therefore required to confirm the file name and absence of other launch requirements.**
- Executable-name detection: the AMD64 normal-launch gate (`StartupDiagnostics.h`) accepts the exact name `generals.exe` since X64.47A (commit `d03fb8d1c`) for **both** titles (the check is in Core and shared); no further change was needed. Contract test `test_normal_launch_contract.py` covers it. The overlay test confirms: the extracted Generals runtime named `Generals.exe` logs `normal_launch`.
- Working directory: Steam starts the game with the install directory as working directory (same as Zero Hour). The Generals runtime finds the base-game archives by the recursive `*.big` scan of the working directory (in a real Generals install they sit directly in the folder, like the contents of `ZH_Generals`, which the validation stage reproduces).
- User data folder: `Documents\Command and Conquer Generals Data` (not the Zero Hour one).

## 3. Dependencies (derived from the import table, not guessed)
Identical set to Zero Hour: 31 imports = Windows system DLLs (`kernel32 user32 gdi32 advapi32 shell32 ole32 oleaut32 winmm ws2_32 imm32 dinput8 avifil32 d3d11 dxgi D3DCOMPILER_47`, 14 `api-ms-win-crt-*`) plus the x64 Visual C++ runtime `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`, `msvcp140_atomic_wait.dll`, shipped app-local from the Visual Studio redistributable (`Microsoft.VC145.CRT`). No project-built DLLs. **No external runtime installation needed.** Generals needs no file the Zero Hour package does not carry.

## 4. Overlay contract (manifest, shipped as `MKRaudstein\manifest.json`)
| File | Size | SHA-256 (start) | Source | Effect |
| --- | --- | --- | --- | --- |
| `Generals.exe` | 8,269,312 | `0D57E3E2B313E6A8...` | target `g_generals`, preset `win64`, Release, `generalsv.exe` | **REPLACES** the EA launcher `Generals.exe` of the Generals Steam installation |
| `vcruntime140.dll` | 178,616 | `D1F4225DF2CD877D...` | VS redistributable x64 | new |
| `vcruntime140_1.dll` | 50,112 | `A7146C08F89FE5B0...` | same | new |
| `msvcp140.dll` | 643,512 | `7C26614E1D733892...` | same | new |
| `msvcp140_atomic_wait.dll` | 57,792 | `3D0CBFAA1BF3EECF...` | same | new |
| `MKRaudstein\LICENSE.md` | 37,990 | `20E84AE14950E349...` | repository | new (GPL-3.0 + EA terms) |
| `MKRaudstein\manifest.json` | 5,210 | generated | builder | new |
| `MKRaudstein-Generals-README.txt` | 3,673 | `350757795DC23316...` | builder | new |
**Replaced in a Generals Steam installation: exactly one file, `Generals.exe`.** Restore: Steam > Properties > Installed Files > Verify integrity (the added files are harmless; the README lists them). No registry change, installer, uninstaller or administrator rights beyond Windows' own. Excluded: all EA assets, `.pdb`, `generalsv.exe` as a copy, sources, build files, scripts, editors/tools, logs, evidence, test binaries. The capture aid (`GENERALS_AUTOSHOT`/`GENERALS_AUTOFLOW`) stays compiled in (inert without its environment variables).
The README identifies the build as PREVIEW / not a public release, requires a legitimate Generals installation, names the Steam target folder (`...\steamapps\common\Command and Conquer Generals`), the replaced file, the restoration, the limitations (no movie playback; no LAN/online; victory/progression not human validated on the preview; not yet tested in a real Steam installation) and the project identity ("Modern Experience by MKRaudstein | 64-bit * DirectX 11", repository and commit, EA trademark notice).

## 5. Builder and validator changes
`New-ZeroHourOverlayPackage.ps1` and `Test-ZeroHourOverlayPackage.ps1` gained `-Title Generals|ZeroHour` (title table: executable, build target, folder, user-data folder, limitations; everything else shared). The Zero Hour output is unchanged apart from the recorded commit (checked by rebuilding it with the same version and diffing the entries). The validator's README-name check had hard-coded the Zero Hour name (found by the first Generals run, fixed).

## 6. Automated disposable-overlay evidence (this is NOT the real Steam test)
`Test-ZeroHourOverlayPackage.ps1 -Title Generals`: **16 of 16 checks passed** (report, launch log in the evidence folder): ZIP == manifest and hashes; no proprietary/developer file types; only executable `Generals.exe`, AMD64; only the four VC++ DLLs; disposable installation built by the stager from the read-only Steam assets (`ZH_Generals`), placeholder `Generals.exe` replaced by extraction, only DLLs + README added; all 31 imports resolve, the VC++ runtime from the extracted folder; the extracted `Generals.exe` started with no arguments and a Windows-only `PATH` logged `normal_launch`, rendered the main menu / shell (`autoshot-shell`), drove the real Skirmish setup screens (`GENERALS_AUTOFLOW=skirmish`) to a match frame, exited with code 0, no failed phase. Preflight: the stager's own preflight passed on the developer stage before extraction; on the overlay the import-resolution check plays that role. The Zero Hour package was re-validated with the same generalized validator: 16 of 16.

## 7. Build-path / privacy finding (affects both packages)
The Generals executable embeds the developer's absolute build path (user name visible) in its PE debug directory, exactly like Zero Hour's (`ProjectsX` / the user profile path appear in the image). Not a runtime dependency; **does not block the internal second-PC test; must be fixed before any public release.** Proposed narrow fix (not applied here): the standard MSVC linker option `/PDBALTPATH:%_PDB%` for the Release configuration (CMake: `target_link_options(<target> PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/PDBALTPATH:%_PDB%>)` on the game executables), which stores only the `.pdb` file name; it changes no code, diagnostics or behaviour (the symbols archive still matches by GUID). To be applied and re-validated in the release-engineering milestone.

## 8. Known limitations of the preview
No Bink movie playback (movies skipped); no LAN/online; mission victory/progression not human validated on the preview; Steam Generals not yet tested in a real installation (not installed on the developer machine); unsigned executable (antivirus reactions unknown); English only.

## 9. Second-PC / real Steam checklist (the human test; NOT done)
Test **both** previews on the same second PC before either distribution model is considered validated.
For Generals: 1. install Command & Conquer Generals from Steam; Play once unmodified; exit. 2. Optional: copy `Generals.exe` to `Generals.exe.original`. 3. Check the ZIP hash (`certutil -hashfile <zip> SHA256`) against `B3CC4BB1...1F3D`. 4. Extract into the Generals folder (Steam > Manage > Browse local files); replace `Generals.exe`. 5. Press Play: no missing-DLL or runtime error. 6. Task Manager > Details: `Generals.exe` is x64; `generals64.log` starts with `normal_launch`. 7. Main menu (move the mouse), Skirmish, Campaign, Save/Load, audio, modern resolution/display options. 8. Exit normally. 9. Optional Verify integrity; the original 32-bit game starts again.
Then the same for Zero Hour (`X64_47A_ZERO_HOUR_STEAM_OVERLAY_ZIP.md` section 10), both installed side by side; extract each ZIP into its own game folder only. **Report especially:** whether the Generals Steam folder really contains/starts `Generals.exe` as assumed, any dialog text, x64 yes/no, which steps fail.

## 10. Blockers
None proven by the automated work. Unconfirmable here: the real Generals depot layout and launcher (section 2), Steam overlay/playtime behaviour with the replaced launcher, antivirus reaction to an unsigned `Generals.exe`.
