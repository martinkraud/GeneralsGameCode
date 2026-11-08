# Developer Preview - player guide

*MKRaudstein Modern Engine, Developer Preview 1. Modern Experience by MKRaudstein | 64-bit • DirectX 11.*

This is a **testing build for people who already own Command & Conquer Generals and/or Zero Hour on Steam**. It is not a public release, it is not a finished product and it may have rough edges.
It adds a 64-bit, Direct3D 11 version of the game engine with a modern menu, options and interface on top of **your own** copy of the game.
**The package contains no game data of Electronic Arts** (no archives, music, movies, textures or maps): without a legitimate installation it does nothing.

## Requirements

* Windows 10 or Windows 11, 64-bit, and a graphics card with Direct3D 11 support.
* The Steam edition of **Command & Conquer Generals** and/or **Command & Conquer Generals: Zero Hour** (English), installed and started once.
* Nothing else. You do **not** need Visual Studio, CMake, Python, the Visual C++ redistributable or any developer tool.
* About 50 MB of free space per game for the files plus a backup of the few original files the installer replaces.

## Install

1. Download the package for your game (`MKRaudstein-Generals-DeveloperPreview-….zip` or `MKRaudstein-ZeroHour-DeveloperPreview-….zip`). Check the download against the `.sha256` file next to it if you like (`certutil -hashfile <zip> SHA256`).
2. Close the game. Extract the whole ZIP to any folder (right-click → *Extract All*).
3. Double-click **`Install.cmd`**. It finds your Steam copy, checks that it is the supported game, shows what it will do, and asks before it changes anything. It then backs up every file it replaces, installs, and verifies every file.
   If your game is somewhere unusual it asks for the folder. `Install.cmd -CheckOnly` shows the plan and changes nothing.
4. Start the game from Steam with the normal **PLAY** button.

Windows SmartScreen or your antivirus may question an unsigned download from an unknown publisher. The installer is a plain, readable PowerShell script (`tools\*.ps1`); the package is built from the public sources.

## Starting the game: Modern or Classic

* **Modern Experience** is the default: modern main menu, Options, Skirmish setup with match rules (Starting Cash, AI Behaviour, Superweapons, Game Speed) and the optional Enhanced HUD.
* **Classic** is the original look and behaviour of the game's frontend: in Steam open *Properties → General → Launch Options* and type `-classic`. Remove it to return to Modern.
  The installer never changes a game setting and never forces Modern; both modes use the same saves and settings folder.
* To confirm the 64-bit build is running: Task Manager → *Details* shows `Generals.exe` as x64. After a run the game folder contains `generals64.log`; its `version` line names the preview designation and commit.

## Saves and settings

The game keeps using its normal folder (`Documents\Command and Conquer Generals Data` / `…Zero Hour Data`). The installer and uninstaller **never touch it**. Back it up first if your saves matter to you.

## Updating to a newer Developer Preview

Close the game, extract the new ZIP and run its `Install.cmd`. It recognises the earlier install, keeps the **original** backup of your retail files, replaces the project files, and removes files the new version no longer has. Running the same version again repairs a damaged install. You do not need to uninstall first, and backups do not accumulate.

## Uninstalling

Double-click **`MKRaudstein\Uninstall.cmd`** inside the game folder (or `Uninstall.cmd` of the package). It restores the original files from the backup, deletes the files the installer added (and the `generals64.log` the game wrote), and removes the `MKRaudstein` folder. If an installation was interrupted it undoes what had been done. If something cannot be restored it says so and keeps the record and backup so you can retry.

**Fallback that always works:** Steam → right-click the game → *Properties → Installed Files → Verify integrity of game files*. Steam restores every original game file. Files the project added (they are harmless) can then be deleted by hand.
The uninstaller restores what *this installer* backed up; it cannot repair changes made by other tools.

## What the installer does and does not do

| It does | It does not |
|---|---|
| copy the 64-bit `Generals.exe`, the app-local Visual C++ runtime DLLs and a licence file into the game folder | touch the registry, Steam itself, other games or your saves |
| (Generals) generate the Zero-Hour-style special-power sidebar files **from your own game archives** on your PC | download anything or contact any server |
| back up every file it replaces and record everything in `MKRaudstein\install-record.json` | require administrator rights beyond what writing to the game folder needs |

## Known limitations of this Developer Preview

* **It is a testing build.** Expect bugs. Please report them (below).
* **Multiplayer:** LAN uses the original menus. Real two-PC multiplayer validation has **not** been done yet; do not rely on it. The original online services no longer exist.
* **Zero Hour in-game HUD (Developer Preview 1):** the faction-specific Enhanced HUD is not active for Zero Hour in this build (the original control bar is used). It is implemented in later development builds, not in Developer Preview 1.
* **Game Speed** (Slow / Normal / Fast / Very Fast) works in **Skirmish only**; it is not available in multiplayer. There is no Modern LAN screen yet.
* **Key bindings** are shown read-only in *Options → Keyboard*; they cannot be changed in the game (the original engine has no rebinding; edit `CommandMap.ini` as before).
* **Movies** (Bink) are skipped in the 64-bit build: intro and briefing videos do not play.
* Only the Steam release in English has been tested. Campaign progression is not fully validated.
* The project is tested on a small set of PCs; other graphics cards and drivers are unverified.

## Reporting problems

Open an issue at <https://github.com/martinkraud/GeneralsGameCode/issues> and attach `generals64.log` from the game folder, your Windows version, graphics card, the resolution and whether you used Modern or `-classic`.

## Legal

This is an independent modernization project based on the open [GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode) sources and the source code released by Electronic Arts under the GPL-3.0 (see `MKRaudstein\LICENSE.md` after installing). It is **not affiliated with or endorsed by Electronic Arts**. "Command & Conquer", "Generals" and "Zero Hour" are trademarks of Electronic Arts Inc. You need your own legitimate copy of the game; this project does not distribute game assets.
