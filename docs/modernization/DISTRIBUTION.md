# Developer Preview distribution - architecture record

Status: Developer Preview Distribution milestone, 2026-11-07, start point `99ed8d038`. Player-facing text: [docs/DEVELOPER_PREVIEW.md](../DEVELOPER_PREVIEW.md) and `README-FIRST.txt` inside the package.
This page is for maintainers: what the package is made of, why, how it is installed and removed, how it is validated, and what is **not** proven.

## 1. Rules (unchanged and new)

1. **No proprietary asset is ever distributed.** Not an archive, not media, and - the case that needed work - **not a modified copy of an Electronic Arts data file**. The validator fails the package otherwise (section 7).
2. **Boring and reversible.** An overlay on a legitimate Steam install, installed by a readable script, with a backup of everything it replaces and an uninstall that uses that backup. Steam *Verify integrity of game files* is the documented fallback; nothing here claims a stronger guarantee.
3. **The distribution rule from X64.47/48 still holds:** *validated staged runtime == clean retail installation + package*. The package is derived from the validated overlay ZIP, and the native validation proves the equality through the installer (section 7, C1).
4. Nothing on the player's machine needs Visual Studio, CMake, Python, the repository, a build tree or PowerShell 7.

## 2. Artifacts

`pwsh scripts/native-runtime/New-MKRaudsteinDistribution.ps1` is still the one command. Per title (Generals, Zero Hour) it now produces, in `build/dist`:

| File | Audience | Content |
|---|---|---|
| `MKRaudstein-<Title>-DeveloperPreview-<commit>.zip` (+ `.zip.sha256`) | **players** | the Developer Preview package (section 3) |
| `MKRaudstein-<Title>-DeveloperPreview-<commit>-Symbols.zip` | developers | the `.pdb` (never in the player package) |
| `MKRaudstein-<Title>-DeveloperPreview-<commit>.package-test.json` | maintainers | validation report (layers A / B / C, section 7) |
| `MKRaudstein-<Title>-x64-Preview-<commit>.zip` + `.manifest.json`, `.stage-parity.json`, `.overlay-test.json` | maintainers | the **overlay ZIP** (the pre-existing developer artifact; the player package is derived from it) |
| `MKRaudstein-x64-Preview-<commit>.SHA256SUMS`, `.distribution.json` | maintainers | one record for both titles, hashes of every ZIP |

Retention is unchanged: the current and the previous validated distribution are kept; `Remove-ObsoleteDistributions.ps1` knows the new file names. The package builders refuse a dirty tree (the package records its commit).

## 3. Package layout

```
MKRaudstein-<Title>-DeveloperPreview-<commit>/
  Install.cmd  Uninstall.cmd  README-FIRST.txt  package-manifest.json
  tools/  MKRaudstein-Setup.ps1  Install-MKRaudstein.ps1  Uninstall-MKRaudstein.ps1  [GeneralsModernExtensions.ps1]
  payload/  Generals.exe  vcruntime140.dll  vcruntime140_1.dll  msvcp140.dll  msvcp140_atomic_wait.dll  MKRaudstein/LICENSE.md  MKRaudstein/manifest.json
```

* `Generals.exe` is the 64-bit runtime under the name Steam's PLAY starts (X64.47A); the original 32-bit launcher is the file the installer backs up.
* The Visual C++ runtime DLLs are shipped app-local, so a clean PC needs no redistributable.
* `package-manifest.json` (schema `mkraudstein-package/1`): identity, target (Steam folder names, required retail archives with their expected sizes, user-data folder), generated-artifact list (`generals64.log`), the SHA-256/size of every payload file, the source overlay ZIP's hash, `containsRetailAssets: false`, and (Generals) the generator list plus the expected hashes of the generated files.
* Text files are CRLF; the ZIP is deterministic (sorted entries, fixed timestamps), so the same inputs give the same bytes.

### 3.1 The Generals sidebar extension is generated, not shipped

The Generals special-power sidebar extension (X64.32: superweapons, CIA intelligence and radar scan get shortcut slots) consists of three INI files and three WND files. They are **the player's retail `INI.big` / `Window.big` entries with additions**, i.e. modified copies of EA data. The overlay ZIP used to carry them; the **player package does not**. The installer regenerates them on the player's machine from the player's own archives: `GeneralsModernExtensions.ps1` is a Windows PowerShell 5.1 port of `scripts/native-runtime/modern_extensions.py` (the developer tool). Output is proven **byte-identical** to the Python tool by `test_distribution_contract.py` on a synthetic archive (every branch) and by the native validation on the real archives (C1 parity). If the archives differ from the tested release the installer still generates and warns; if generation fails it installs without the extension (the game then shows the original sidebar) and says so. Zero Hour needs no generation.

## 4. Version identity (one mechanism)

* `scripts/distribution/identity.json` is the single source for the package builder: **designation** (`Developer Preview 1`), branding string, architecture, graphics API. The commit and commit date come from `git` (the *commit* date, so the build date is reproducible, not "now").
* `Core/GameEngine/Include/Common/ReleaseIdentity.h` carries the same designation for the engine; a contract test keeps the two equal. It is appended to the existing `version` log checkpoint (`generals64.log`) next to the game's own product/version/hash string. The visible footer is unchanged: *Modern Experience by MKRaudstein | 64-bit • DirectX 11*.
* The package name contains title, `DeveloperPreview` and the short commit; `package-manifest.json`, the install record and `README-FIRST.txt` carry the full identity (designation, title, short and full commit, commit date, architecture).
* Bumping a Preview number is one edit in each of the two files above; the contract test fails if they differ.

## 5. Installer

`Install.cmd` → Windows PowerShell 5.1 (`powershell.exe -NoProfile -ExecutionPolicy Bypass`, `PSModulePath` cleared so a PowerShell 7 parent cannot hide the built-in commands). Steps:

1. **Locate** the game: `-GameFolder`, else Steam libraries (`libraryfolders.vdf`, registry read-only), else ask.
2. **Check**: required retail archives present (a size difference is a warning: another game version), `Generals.exe` is the retail 32-bit PE (machine `0x14C`) *or* carries an MKRaudstein install record; a foreign 64-bit `Generals.exe` is refused (restore with Steam first); a running game is refused. Anything unsupported ends with exit 2 and **nothing changed**.
3. **Verify the package**: every payload file's size and SHA-256 against `package-manifest.json`, free disk space; generate the Generals extension files into a temp folder.
4. **Plan** and ask (`-Yes` skips the question; `-CheckOnly` stops here).
5. **Install**: back up each existing file the record has not seen (`MKRaudstein\backup\<same path>`, hash-verified), write `install-record.json` with status `installing`, copy each file via `*.mkr-new` + move (hash-verified), remove files an earlier version had that this one lacks, copy the uninstaller next to the game. **Any failure rolls back** every file written (exit 3) and leaves the record as `installing`.
6. **Verify** every installed file and that `Generals.exe` is x64; set the record to `installed`.

Only the game folder is written (`Resolve-UnderGameFolder` rejects absolute paths, `..` and escapes). No registry write, no user-data access, no network, no Steam change.

## 6. Uninstaller, upgrade and reinstall policy

* **Uninstall** (`MKRaudstein\Uninstall.cmd`, or the one in the package): reads the record; for each file the installer replaced restores the **hash-verified** backup, deletes the files it added and the generated `generals64.log`, removes now-empty directories it created, removes the `MKRaudstein` folder. Interrupted installs (`installing`) work the same. A missing or damaged backup is reported, the other files are still restored, the record and backup are **kept** (status `uninstall-incomplete`, exit 4) so a second attempt or Steam *Verify integrity* is possible. The command runs from a temp copy so it can delete its own folder. Saves and settings are never touched.
* **Reinstall / upgrade policy (smallest robust one):** install over the old version, no uninstall first. The record is read; the **original** backup is kept (a file already in the record is never "backed up" again, so the backup can never become our own file); changed files are replaced; files the new version no longer has are removed; the record gets the new identity. Same version again = repair. Backups do not accumulate (one backup per replaced retail file). A downgrade is the same operation. Installing the other title's package into a record of the first is refused.
* **Not guaranteed:** if the player (or another tool) altered the replaced files outside the installer, or deleted `MKRaudstein`, restoration falls back to Steam *Verify integrity of game files*, which is documented in every player document.

## 7. Validation: three layers, reported separately

`scripts/native-runtime/Test-DeveloperPreviewPackage.ps1`. The pipeline runs layers A and B for both titles and fails the distribution if either fails; layer C (`-NativeInstallTest`) is the slow one.

| Layer | What it proves | What it does **not** prove |
|---|---|---|
| **A. Package audit** (static) | extraction (one top folder, named like the ZIP, `.sha256` matches); no duplicate (case-insensitive), absolute, drive-letter or `..` entry; manifest == files, size + SHA-256; identity complete and equal to `identity.json` / `ReleaseIdentity.h`; **no proprietary asset**: no archive / media / texture / map file type, no INI / WND data file, the payload is *exactly* `Generals.exe` + the app-local VC++ DLLs + licence + overlay manifest, and **no packaged file is byte-identical to any retail asset of the staged candidate** (130 / 278 files); no PDB / object / source / repo file; text scan of all scripts, manifests and docs for the user name, repo / build paths, Visual Studio, CMake, Python, `pwsh`; no hard link / symlink / junction / registry-write / execution-policy change in the scripts; scripts parse; `Generals.exe` is AMD64, imports only package or Windows DLLs, no debug runtime; README claims | that a different machine runs it |
| **B. Synthetic install tests** (no game, no Steam) | the **shipped scripts, run by `powershell.exe` 5.1**, on a stand-in 32-bit launcher and tiny stand-in archives: check-only changes nothing; fresh install; repair of a deleted and a damaged file; upgrade (changed / removed / added file, new identity) keeping the *original* backup; uninstall to a **byte-identical** tree; second uninstall is a no-op; install failure mid-way → exit 3, full rollback, then clean uninstall and clean reinstall; refusals (not the game, foreign x64 exe, missing folder, damaged package) change nothing; damaged backup → exit 4, repair, second uninstall completes; the `Uninstall.cmd` double-click path; the real user-data folder is unchanged | behaviour with the real game files |
| **C. Native** (`-Native`) | the same installer on a **disposable copy of the real game** (staged from the read-only Steam files, project files removed, stand-in retail launcher): parity **retail copy + installer == the validated staged runtime, file for file (incl. generated Generals files)**; real launches of the installed `Generals.exe` with a Windows-only `PATH`: first launch (no arguments, like PLAY), Modern main-menu audit, Modern Options audit, Skirmish to a rendered match, Skirmish save/load at Game Speed Fast, `-classic` main menu and Options; user-data guard (ini / Save / Replays restored); uninstall → tree identical to the clean copy | a clean *machine*; the owner's Steam |

Honest categories (do not blur them): **package isolation validation** = layers A + B (+ C on the developer PC); **real clean-machine validation** = a different Windows machine without the developer tools - **pending**; **real owner/native validation** = the owner's own Steam run - **pending**. Layer C cannot isolate the user-data folder (the game writes `Documents\…Data`); it backs it up and restores it, as in the earlier milestones.

## 8. Classic / Modern

The installer writes no game setting and no launch option. Modern is the default of the 64-bit runtime; `-classic` selects the retail frontend exactly as before (validated in layer C after install). Uninstall removes the 64-bit executable and restores the 32-bit launcher, so nothing Modern-only remains configured; the shared `Options.ini` is the player's own and is never modified by the scripts.

## 9. Known gaps (deliberately left)

* **Real clean-machine validation** and **real two-PC multiplayer validation of `2071ad765`** remain pending.
* **Owner manual acceptance (2026-11-08)** of the real player packages: Zero Hour - Install.cmd found the Steam folder, recognised the original 32-bit game and a fresh install, verified every package hash, showed the plan, backed up the replaced files, kept Documents / userdata, installed and verified the files; Steam PLAY started the 64-bit Modern runtime (PASS); MKRaudstein\Uninstall.cmd recognised Developer Preview 1, restored the replaced files, removed the generated generals64.log and the MKRaudstein files / folder, kept saves and settings (PASS). Generals - installation, start and game operation worked (PASS). This is a manual player-package test, not the automated Layer C. Clean-machine status: the owner's test machine is not established to be an independent Windows machine without the development tools (Visual Studio, CMake, Python, the repository), so the existing definition is not shown to be met and **real clean-machine validation stays pending**.
* `Generals.exe` still embeds its build-time `.pdb` path in the PE debug directory (`/PDBALTPATH` would strip the user name from it): an informational finding of the validator, not a runtime dependency.
* **Known, pre-existing shutdown report (not a regression):** a launch that ends on the live Modern main menu (the Layer C first launch) logs `ui_residency_release fail live_residencies=2` in the shutdown invariant check, because the menu's UI textures are still resident when the check runs; the backend teardown that follows destroys them and the process exits 0. The `2071ad765` build reports the same with 5; the Options, Skirmish and Classic launches report 0. Layer C lists it as `info` (any other failed phase still fails). A fix belongs to a separate UI-texture lifetime task.
* The package is not code-signed (SmartScreen / antivirus may warn); there is no auto-updater (by decision).
* The original EA `Generals.exe` is not available to the validator, so the retail launcher is recognised by its PE machine type (32-bit) and the install record, not by hash. The Steam folders on the development PC already contain the earlier overlay; all tests therefore run on isolated copies.
* Only the English Steam release is targeted; a different game version is warned about, not blocked.
* Layer C uses a stand-in retail `Generals.exe`; the real retail launcher replacement is exercised only by the owner's manual test.
