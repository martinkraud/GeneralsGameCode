MKRaudstein {{SHORT}} - {{DESIGNATION}}
{{BRANDING}}
================================================================================
Title        : {{TITLE}}
Designation  : {{DESIGNATION}}   (a DEVELOPER PREVIEW for testers - not a public release)
Source commit: {{COMMITFULL}}
Commit date  : {{BUILDDATE}}
Architecture : {{ARCH}}, Direct3D 11

WHAT THIS IS
A 64-bit, Direct3D 11 version of the {{SHORT}} engine with a modern menu, options and interface,
built from the open GeneralsGameCode sources (GPL-3.0). It is an ADD-ON for a copy of the game
you already own on Steam. This package contains NO game data of Electronic Arts: no archives,
music, movies, textures or maps. Without your own legitimate installation it does nothing.

WHAT YOU NEED
 - Windows 10 or 11, 64-bit, and a DirectX 11 graphics card.
 - {{TITLE}} installed from Steam (the English release), started at least once.
 - Nothing else: no Visual Studio, no Python, no Visual C++ redistributable, no administrator
   rights beyond what Windows needs to write to the game folder.

HOW TO INSTALL (about one minute)
 1. Close the game. Extract this whole ZIP to any folder (right-click > Extract All).
 2. Double-click  Install.cmd . It finds your Steam copy ("{{FOLDER}}"),
    checks it, backs up every file it replaces, installs, and verifies. It asks before it changes
    anything. If your game is somewhere else, it asks you for the folder.
    (To see what it would do without changing anything: run  Install.cmd -CheckOnly .)
 3. Start the game with the normal PLAY button in Steam. Steam starts the 64-bit runtime.

CLASSIC OR MODERN
 The Modern interface is the default. For the original look and behaviour, add  -classic  to the game's
 launch options in Steam (Properties > General > Launch Options). Remove it to return to Modern.
 Settings and saves are shared: they use your normal "{{DATA}}" folder
 in Documents. The installer never touches that folder. Back it up first if you care about your saves.

UPDATING TO A NEWER PREVIEW
 Close the game, extract the new ZIP, double-click its Install.cmd. It recognises the earlier install,
 keeps the ORIGINAL backup of your retail files, replaces the project files and removes ones the new
 version no longer has. Running the installer again on the same version simply repairs it.

UNINSTALLING
 Double-click  MKRaudstein\Uninstall.cmd  inside the game folder (or Uninstall.cmd of this package).
 It puts back the original files from the backup, deletes the files the installer added and the
 MKRaudstein folder. Your saves and settings are not touched.
 If anything ever looks wrong: Steam > right-click the game > Properties > Installed Files >
 "Verify integrity of game files". That is the fallback that always restores the original game files.

KNOWN LIMITATIONS OF THIS DEVELOPER PREVIEW
 - Testing build. Multiplayer (LAN) uses the original menus and has NOT yet been validated between two PCs.
   The online services of the original game no longer exist.
 - Game Speed (Slow / Normal / Fast / Very Fast) works in Skirmish only; it is not available in multiplayer.
 - No Modern LAN screens (LAN uses the original ones).
 - Key bindings are shown read-only in Options > Keyboard; they cannot be changed in the game.
   (Edit CommandMap.ini as before if you need other keys.)
 - Movies (Bink) are skipped in the 64-bit build: intro and briefing videos do not play.
 - Only the Steam release in English has been tested.

HOW TO CHECK IT IS THE 64-BIT BUILD
 Task Manager > Details: "Generals.exe" shows architecture x64. After a run the game folder has
 generals64.log; its "version" line names the preview designation and the commit. Please attach it to
 bug reports.

PROJECT AND LEGAL
 Sources and issues: https://github.com/martinkraud/GeneralsGameCode (branch dev/modern-engine).
 Based on https://github.com/TheSuperHackers/GeneralsGameCode and the source code released by
 Electronic Arts under the GPL-3.0 (see MKRaudstein\LICENSE.md after installing).
 "Command & Conquer", "Generals" and "Zero Hour" are trademarks of Electronic Arts Inc.
 This project is not affiliated with or endorsed by Electronic Arts.
