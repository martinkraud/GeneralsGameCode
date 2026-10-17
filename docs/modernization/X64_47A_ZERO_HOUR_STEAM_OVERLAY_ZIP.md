# X64.47A - Zero Hour Steam overlay ZIP feasibility (PREVIEW package)

Status: **OPEN - package built and automatically validated on a disposable layout; the real Steam test on a second PC is NOT done.** The Steam distribution model is **not validated** until the human performs the checklist in section 9. This is not a public release and not the "X64.47 Zero Hour visual and options parity" feature milestone (that stays the next feature milestone, see `FIRST_PLAYABLE_ROADMAP.md`).
Start point: HEAD `8d3db7b08` = `origin/dev/modern-engine` (X64.46 closed). Safety held throughout: the real Steam directory was only read (listing, hashing, the launch configuration in `appcache`), never written, and the candidate was never run on it.

## 1. The package
| | |
| --- | --- |
| File | `MKRaudstein-ZeroHour-x64-Preview-951547ec0.zip` |
| Path | `build\dist\MKRaudstein-ZeroHour-x64-Preview-951547ec0.zip` (git-ignored; rebuild with `scripts\native-runtime\New-ZeroHourOverlayPackage.ps1`, bit-identical for the same inputs) |
| Size | 4,109,125 bytes |
| SHA-256 | `C9D6C17E03037C779B3FC788B623B0C357FF226476EC755C7A2269605BE45D04` |
| Built from | source commit `951547ec0eac65cf3461afe79e07f2f0eda327e3` (later commits only change documentation) |
| Companions | `...zip.sha256`, `...manifest.json` (per-file hashes), optional `...-Symbols.zip` (the `.pdb`, 22 MB, developers only, **not** for players) |
| Evidence copy | `docs/modernization/evidence/x64-47a-overlay/` (manifest, hash, test report, launch log) |

## 2. Steam launch chain (Phase A finding - the important one)
- Steam's launch configuration for app 2732960 (read from `appcache\appinfo.vdf`, read-only) is: executable **`Generals.exe`** (and `WorldBuilder.exe` as a second entry). **It is not `generalszh.exe`.**
- The original `Generals.exe` of the Steam installation is a 32-bit Electronic Arts launcher (4,303,312 bytes, SHA-256 `7b075b9f0baa9df81651c0c9dd7d8c445454ae1b2452b928f4a1d9332e9ccece`, 2026-09-09, depot build 17435311); it starts `Game.dat` (the 32-bit game with the Steam API) through `CreateProcess`.
- So **replacing `generalszh.exe` is NOT sufficient for Steam's Play button.** The overlay therefore ships the AMD64 runtime **under the name `Generals.exe`**, which Steam starts directly. No custom launcher, no compatibility or startup arguments, no registry change.
- This needed one runtime change (commit `d03fb8d1c`, section 8): the AMD64 build decides "normal launch" (modern runtime, `generals64.log`, headless overrides) from its own file name, which was `generalsv*` / `generalszh*` only. The exact name `generals.exe` was added (nothing else; contract test updated). Without it a renamed exe would not have taken the normal-launch path.
- The developer's own Steam folder is **not pristine** (it contains earlier community / test builds: a 32-bit `generalszh.exe` and `d3d8.dll` dated October, `DebugWindow.dll`). It was therefore used only for the facts above (launch configuration, original `Generals.exe` hash), never to define "original". A 64-bit process ignores the 32-bit `d3d8.dll`; the AMD64 runtime does not import it (checked).
- Original Steam executables import only the old Microsoft runtimes (`msvcrt`, `msvcp60`, `msvcp90`), none import `vcruntime140` / `msvcp140`, so the four app-local DLLs below do not disturb them. (A 32-bit community `d3d8.dll` that imports `msvcp140`/`vcruntime140` would pick up the 64-bit copies if a 32-bit process in the same folder loaded it: do not run a 32-bit community build from the same folder at the same time; irrelevant for pristine installations.)
- Not verified (needs the real test): that Steam's overlay / playtime tracking is happy with the replaced `Generals.exe`; that Steam does not re-verify and restore the file on launch (it does not by default, only "Verify integrity" restores).

## 3. Dependency audit (derived, not guessed)
Static: the PE import table of the Win64 Release `generalszh.exe` (31 imports) plus the strings of the image for run-time loaded DLLs.
- **Windows system DLLs (not shipped)**: `kernel32 user32 gdi32 advapi32 shell32 ole32 oleaut32 winmm ws2_32 imm32 dinput8 avifil32 d3d11 dxgi D3DCOMPILER_47` and the Universal CRT forwarders `api-ms-win-crt-*` (14). All are part of Windows 10/11 (UCRT included). Run-time loaded and optional: `dbghelp`, `mfplat`/`mfreadwrite` (Media Foundation), `DebugWindow.dll` / `ParticleEditor.dll` (developer tools, absent in a player install, loading is optional), `d3d8.dll` (legacy path, not used by the normal launch).
- **Microsoft Visual C++ runtime, x64 (not part of Windows)**: `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`, `msvcp140_atomic_wait.dll`. Shipped app-local, taken from the Visual Studio redistributable directory that matches the toolset (`VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT`), so a clean PC needs no Visual C++ Redistributable installation.
- **Project-built DLLs**: none (the runtime is one self-contained exe).
- **Original EA assets the runtime needs**: the BIG archives, `Data\`, etc. - supplied by the legitimate installation, **never** in the ZIP. The runtime finds the base-game archives by a recursive `*.big` scan of the working directory (Steam ships them in `ZH_Generals\`), which is what the human-validated stage reproduced.
- **Working directory / DLL search**: Steam starts the game with the install directory as the working directory; the DLLs sit next to `Generals.exe` (application directory first in the search order). The test below proves resolution with a `PATH` reduced to Windows only.
- **User data**: unchanged location (`Documents\Command and Conquer Generals Zero Hour Data`); `generals64.log` is written next to the exe (best effort: a read-only folder is never fatal).
- **Architecture**: AMD64 PE, verified by header; not a "(32 bit)" process in Task Manager.
- **Embedded build path**: the executable's debug directory names the `.pdb` with the developer's absolute build path (user name visible). Harmless at run time, informational; a `/PDBALTPATH` link option would remove it (deferred, build configuration change).

## 4. Overlay contract (manifest)
Extract directly over the Zero Hour folder, overwrite when asked. The ZIP contains **only** these files (also in `MKRaudstein\manifest.json` with SHA-256):
| File (destination in the Zero Hour folder) | Source / build target | Why | Ownership | Required | Effect on an installation |
| --- | --- | --- | --- | --- | --- |
| `Generals.exe` (8,623,616 B) | target `z_generals`, preset `win64`, Release, `generalszh.exe` | the AMD64 / D3D11 runtime; Steam's Play starts this name | project-owned, GPL-3.0-or-later | required | **REPLACES** the EA launcher `Generals.exe` |
| `vcruntime140.dll` (178,616) | Visual Studio redistributable x64 | imported by the runtime | Microsoft redistributable (app-local) | required | new file |
| `vcruntime140_1.dll` (50,112) | same | imported | same | required | new file |
| `msvcp140.dll` (643,512) | same | imported | same | required | new file |
| `msvcp140_atomic_wait.dll` (57,792) | same | imported | same | required | new file |
| `MKRaudstein\LICENSE.md` | repository `LICENSE.md` | GPL-3.0 + EA terms: required to redistribute the exe | project-owned | required | new file |
| `MKRaudstein\manifest.json` | generated | the machine-readable contract | project-owned | required (documentation) | new file |
| `MKRaudstein-ZeroHour-README.txt` | generated | status, install, limitations, restoration | project-owned | required (documentation) | new file |
**Replaced files in a normal installation: exactly one, `Generals.exe`.** Restoration: Steam > Properties > Installed Files > Verify integrity of game files (restores `Generals.exe`; the added files are harmless and listed in the README for manual deletion). No registry changes, no installer, no uninstaller, no administrator rights beyond Windows' own for the chosen folder.

## 5. Deliberately excluded
`.pdb` (separate optional symbols archive), `generalszh.exe` as a second copy, test executables, WorldBuilder / GUIEdit / tools, evidence, developer scripts (`Stage-*`, `Run-*`, `Invoke-*` are not needed by the player), CMake/build files, sources, logs. The developer capture aid (`GENERALS_AUTOSHOT` / `GENERALS_AUTOFLOW`) stays compiled in: it is inert unless its environment variables are set, and diagnostics were not weakened to shrink the ZIP. Not included at all: any EA data, BIG archive, movie, music, codec (`binkw32`), Miles files, Steam files.

## 6. Builder and validator
- `scripts\native-runtime\New-ZeroHourOverlayPackage.ps1`: builds the ZIP (and the symbols ZIP, `.sha256`, `.manifest.json`) from the build output. Refuses a non-AMD64 exe, a dirty tracked tree (unless `-AllowDirtyTree`), a missing redistributable DLL; derives the DLL list from the import table; deterministic (sorted entries, fixed timestamps, no build paths): a second build gave the identical SHA-256.
- `scripts\native-runtime\Test-ZeroHourOverlayPackage.ps1`: the disposable-overlay test (section 7).
- Naming: `MKRaudstein-ZeroHour-x64-Preview-<short commit>.zip`. Two separate player ZIPs are the eventual model (Generals and Zero Hour); only the Zero Hour preview exists; no unified launcher.

## 7. Automated disposable-overlay evidence (this is NOT the real Steam test)
`Test-ZeroHourOverlayPackage.ps1` on the final ZIP: **15 of 15 checks passed** (report and launch log in `evidence/x64-47a-overlay/`):
1. the ZIP holds exactly the manifest's files and every hash matches;
2. no proprietary / developer file types (no BIG, movie, INI, map, texture, audio, `.pdb`, scripts, sources); the only executable is `Generals.exe`; the only DLLs are the four VC++ runtime DLLs;
3. `Generals.exe` is an AMD64 PE image;
4. disposable "installation" built with the existing stager from the **read-only** Steam directory; the developer runtime removed, a placeholder `Generals.exe` put where Steam's is, the ZIP extracted over it with overwrite: the placeholder was replaced by the packaged runtime and only the DLLs and the README were added;
5. all 31 imports resolve; the four VC++ DLLs resolve from the extracted folder, not from the machine; no repository-relative path is needed;
6. the extracted `Generals.exe`, started **without arguments** (as Steam's Play does) with `PATH` limited to Windows (no Visual Studio / CMake directory), logged `normal_launch`, drove the General's Challenge screens to a rendered match and exited with code 0; no failed phase.
Not covered by the automation: real Steam launch (overlay, playtime), a machine without developer tools, a second GPU/driver, audio device differences, Skirmish/Campaign/Save-Load on that machine (all were human validated on the developer PC through the stage, X64.46).

## 8. Commits
`d03fb8d1c` the `Generals.exe` normal-launch name (`StartupDiagnostics.h` + contract test); `aa2ff2a7f` the package builder; `ada49ea56` the validator; `951547ec0` lower-case runtime DLL names (the package source commit); then the documentation and evidence commit. Pushed only to `origin/dev/modern-engine`.

## 9. Known limitations of the preview (also in the ZIP's README)
Movies absent (no Bink on AMD64); no Enhanced Zero Hour HUD; 3 ZH-only options hidden; animated texture mappers static; victory/progression not human validated; no LAN/online; only the English Steam release tested; the replaced `Generals.exe` means the unmodified game cannot be started from Steam until Verify integrity (or restoring `Generals.exe.original`).

## 10. Second-PC / real Steam checklist (the human test; not done)
1. Install Zero Hour through Steam; start it with Play **unmodified**; reach the main menu; exit.
2. Optional backup: copy `Generals.exe` in the game folder to `Generals.exe.original`.
3. Copy `MKRaudstein-ZeroHour-x64-Preview-951547ec0.zip` to the PC; (optional) check `certutil -hashfile <zip> SHA256` against `C9D6C17E03037C779B3FC788B623B0C357FF226476EC755C7A2269605BE45D04`.
4. Extract it **into the Zero Hour game folder** (Steam: right-click the game > Manage > Browse local files). Choose to replace `Generals.exe` when Windows asks.
5. Press Play in Steam. Expected: no missing-DLL or runtime error dialog; the game starts without Visual Studio/CMake/Visual C++ installed.
6. While it runs, Task Manager > Details: `Generals.exe` must show Architecture **x64**. Afterwards `generals64.log` in the game folder starts with `normal_launch`.
7. Check: main menu (move the mouse to show it); Skirmish starts; Campaign starts; General's Challenge starts; Save then Load works; audio plays (music, effects, speech); the modern display options (resolution, windowed/fullscreen) work.
8. Exit normally (Exit Game). Check Steam shows the game as stopped.
9. Optional: Steam > Properties > Installed Files > Verify integrity; confirm the original game starts again (32-bit "Generals.exe" in Task Manager, "(32 bit)").
Report: anything that fails to start, any dialog text, whether Task Manager says x64, which of step 7 fail, whether Steam complained about the file or restored it on its own.

## 11. Blockers for "extract over the Steam install -> Steam Play"
None found in the automated work. Open questions only the real test can answer: Steam overlay / playtime behaviour with the replaced launcher, the pristine-installation layout on another PC (assumed identical depot), the app-local VC++ DLLs on a machine with a very old or very new Visual C++ redistributable (side by side, app-local wins), and antivirus reactions to an unsigned `Generals.exe` (the package is not code-signed; a signing decision belongs to the later release milestone).
