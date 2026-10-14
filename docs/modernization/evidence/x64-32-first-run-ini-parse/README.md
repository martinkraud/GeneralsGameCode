# x64-32-original-experience first native run

Exit 1 at engine init: `TheInGameUI.init fail` -> `Error parsing INI file 'Data\INI\CommandButton.ini'
(Line: 'CommandButton Command_FireParticleUplinkCannonFromShortcut ')`. Dumps CrashFG (380 MB, stage only) and
CrashMG (here) are the dumper firing in `GameEngine::init`'s catch.

Root cause: the overlay generator emitted `Command = SPECIAL_POWER_FROM_SHORTCUT` (Zero Hour's spelling). In a Generals
build (`RTS_GENERALS`, ControlBar.h `TheGuiCommandNames`) that enum value is spelled `SPECIAL_POWER_FROM_COMMAND_CENTER`
(the same spelling the retail Generals shortcut buttons use); the unknown token raises INI_INVALID_DATA. Fixed in
`modern_extensions.py` (`SHORTCUT_COMMAND`); `test_modern_extensions_vocabulary.py` now checks every generated
field name, Options bit and Command token against the engine's own tables.
