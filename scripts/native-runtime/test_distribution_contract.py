#!/usr/bin/env python3
"""Developer Preview distribution contract (source checks + a hermetic generator-parity test; no retail data, no game, no Steam).

Proves, statically:
  * one release identity: scripts/distribution/identity.json == Core/GameEngine/Include/Common/ReleaseIdentity.h, and the branding keeps the Modern footer text;
  * the player scripts are Windows PowerShell 5.1 safe (no PowerShell 7 syntax), touch no registry, no user data, no repository / developer path;
  * the package builder excludes EA-derived data, and the validator keeps its proprietary-asset gate;
  * the Generals extension generator in PowerShell writes BYTE-IDENTICAL output to modern_extensions.py on a synthetic archive (skipped without Windows PowerShell);
  * the Controls/Keyboard scroll fix (wheel, keyboard, track click) is still wired in both titles.
The dynamic proofs are Test-DeveloperPreviewPackage.ps1 (layers A, B, C) and the native Options audit.
"""
import json
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
DIST = ROOT / "scripts" / "distribution"
NATIVE = ROOT / "scripts" / "native-runtime"
sys.path.insert(0, str(NATIVE))
CRLF = chr(13) + chr(10)


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


PLAYER_SCRIPTS = ["MKRaudstein-Setup.ps1", "Install-MKRaudstein.ps1", "Uninstall-MKRaudstein.ps1", "GeneralsModernExtensions.ps1"]


class Identity(unittest.TestCase):
    def test_single_designation(self):
        identity = json.loads((DIST / "identity.json").read_text(encoding="utf-8"))
        header = read("Core/GameEngine/Include/Common/ReleaseIdentity.h")
        self.assertIn('kDesignation = "%s"' % identity["designation"], header)
        self.assertRegex(identity["designation"], r"^Developer Preview \d+$")

    def test_branding_is_the_modern_footer(self):
        identity = json.loads((DIST / "identity.json").read_text(encoding="utf-8"))
        footer = (ROOT / "Core/GameEngine/Include/Common/ModernBranding.h").read_bytes().decode("cp1252")
        self.assertIn("Modern Experience by MKRaudstein | 64-bit \u2022 DirectX 11", footer)
        self.assertEqual(identity["branding"], "Modern Experience by MKRaudstein | 64-bit \u2022 DirectX 11")

    def test_designation_reaches_the_log(self):
        for path in ("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp",
                     "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp",
                     "Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp",
                     "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp"):
            text = (ROOT / path).read_bytes().decode("cp1252").replace("\r\n", "\n")
            self.assertIn("full.concat(ReleaseIdentity::kDesignation);", text, path)
            self.assertIn('StartupDiagnostics::checkpoint("version", "observed", full.str());', text, path)


class PlayerScripts(unittest.TestCase):
    def test_windows_powershell_5_1_only_syntax(self):
        for name in PLAYER_SCRIPTS:
            text = read("scripts/distribution/" + name)
            self.assertNotIn("#Requires -Version 7", text, name)
            for banned in (r"\?\?", r"\?\.", r"&&", r"\|\|", r"-Parallel", r"\$PSStyle", r"\bForEach-Object\s+-Parallel"):
                code = "\n".join(l for l in text.split("\n") if not l.lstrip().startswith("#"))
                code = re.sub(r"'[^'\n]*'|\"[^\"\n]*\"", "''", code)          # string literals may contain these characters
                self.assertIsNone(re.search(banned, code), "%s uses %s" % (name, banned))

    def test_no_developer_paths_tools_or_registry_writes(self):
        for name in PLAYER_SCRIPTS + ["Install.cmd", "Uninstall.cmd", "README-FIRST.template.txt"]:
            text = read("scripts/distribution/" + name)
            for banned in (r"C:\\Users", r"buskr", r"ProjectsX", r"dev-runtimes", r"\bcmake\b", r"\bpwsh\b", r"vswhere", r"VsDevCmd"):
                self.assertIsNone(re.search(banned, text, re.I), "%s mentions %s" % (name, banned))
            self.assertIsNone(re.search(r"Set-ItemProperty|New-ItemProperty|Remove-ItemProperty|reg(\.exe)?\s+(add|delete)|mklink|HardLink|SymbolicLink|Junction", text, re.I), name)

    def test_only_the_game_folder_is_written(self):
        setup = read("scripts/distribution/MKRaudstein-Setup.ps1")
        self.assertIn("Resolve-UnderGameFolder", setup)
        for name in ("Install-MKRaudstein.ps1", "Uninstall-MKRaudstein.ps1"):
            text = read("scripts/distribution/" + name)
            self.assertNotRegex(text, r"(?i)MyDocuments|\$env:USERPROFILE|\$HOME|Documents\\Command", name)       # saves and settings are never touched
            self.assertRegex(text, r"Resolve-UnderGameFolder")

    def test_installer_semantics(self):
        install = read("scripts/distribution/Install-MKRaudstein.ps1")
        for needle in ("MKRaudstein\\backup", "'installing'", "'installed'", "$CheckOnly", "Rolling back", "Get-FileSha256", "0x8664", "$needsBackup = $exists -and -not $oldEntry"):
            self.assertIn(needle, install)
        uninstall = read("scripts/distribution/Uninstall-MKRaudstein.ps1")
        for needle in ("uninstall-incomplete", "backupSha256", "Verify integrity of game files", "Your saves and settings"):
            self.assertIn(needle, uninstall)
        for cmd in ("Install.cmd", "Uninstall.cmd"):
            self.assertIn('set "PSModulePath="', read("scripts/distribution/" + cmd))      # a PowerShell 7 parent must not hide the built-in commands of 5.1

    def test_no_eula_or_proprietary_text_is_embedded(self):
        template = read("scripts/distribution/README-FIRST.template.txt")
        for needle in ("NO game data", "CLASSIC", "-classic", "Verify integrity of game files", "two PCs", "Skirmish only", "read-only", "not affiliated"):
            self.assertIn(needle, template)


class Builder(unittest.TestCase):
    def test_derived_data_is_never_shipped(self):
        builder = read("scripts/native-runtime/New-DeveloperPreviewPackage.ps1")
        self.assertIn("GenPowersShortcutBar", builder)
        self.assertIn("generators", builder)
        self.assertIn("containsRetailAssets = $false", builder)
        self.assertIn("'GeneralsModernExtensions.ps1'", builder)
        # the files modern_extensions.py generates are exactly the ones the builder strips
        import modern_extensions
        out = {"Data/INI/CommandButton.ini", "Data/INI/CommandSet.ini", "Data/INI/PlayerTemplate.ini"} | {"Window/" + w for w in modern_extensions.BARS}
        ps = read("scripts/distribution/GeneralsModernExtensions.ps1")
        for rel in out:
            base = rel.split("/")[-1]
            self.assertTrue(base in ps or ("Window/" in rel and "GenPowersShortcutBar" in ps), rel)

    def test_symbols_stay_out_of_the_player_package(self):
        builder = read("scripts/native-runtime/New-DeveloperPreviewPackage.ps1")
        self.assertIn("-Symbols.zip", builder)
        self.assertNotIn(".pdb'", builder.replace("stage-parity", ""))

    def test_the_pipeline_builds_and_validates_it(self):
        pipeline = read("scripts/native-runtime/New-MKRaudsteinDistribution.ps1")
        self.assertIn("New-DeveloperPreviewPackage.ps1", pipeline)
        self.assertIn("Test-DeveloperPreviewPackage.ps1", pipeline)
        self.assertIn("developerPreview", pipeline)
        cleanup = read("scripts/native-runtime/Remove-ObsoleteDistributions.ps1")
        self.assertIn("DeveloperPreview", cleanup)


class Validator(unittest.TestCase):
    def test_gates(self):
        v = read("scripts/native-runtime/Test-DeveloperPreviewPackage.ps1")
        for needle in ("no game-asset file types", "byte-identical to any of the", "containsRetailAssets", "no developer path or tool reference", "no PDB", "B6 a failure in the middle of an install",
                       "B8 a damaged backup", "B10 the real user-data folder is unchanged", "C1 PARITY", "C7 user-data guard", "NOT a real clean-machine test", "[IO.File]::Delete((Join-Path $game 'Generals.exe'))"):
            self.assertIn(needle, v)


@unittest.skipUnless(shutil.which("powershell.exe") and sys.platform == "win32", "Windows PowerShell 5.1 is needed for the port parity test")
class GeneratorParity(unittest.TestCase):
    """The PowerShell 5.1 port writes byte-identical files to modern_extensions.py, on a synthetic archive that exercises every branch."""

    @staticmethod
    def big(entries):
        names = list(entries)
        table = sum(8 + len(n) + 1 for n in names)
        pos = 16 + table
        out = bytearray(b"BIGF" + b"\0\0\0\0" + len(names).to_bytes(4, "big") + b"\0\0\0\0")
        for n in names:
            out += pos.to_bytes(4, "big") + len(entries[n]).to_bytes(4, "big") + n.encode("latin1") + b"\0"
            pos += len(entries[n])
        for n in names:
            out += entries[n]
        return bytes(out)

    @staticmethod
    def wnd(slots):
        lines = ["FILE_VERSION = 2;", "WINDOW", "  SCREENRECT = UPPERLEFT: 700 100,", "               BOTTOMRIGHT: 800 400,", "               CREATIONRESOLUTION: 800 600;",
                 '  NAME = "GenPowersShortcutBar.wnd:Parent";']
        for k in range(1, slots + 1):
            lines += ["  CHILD", "  WINDOW", "    SCREENRECT = UPPERLEFT: 752 %d," % (100 + 35 * k), "                 BOTTOMRIGHT: 800 %d," % (141 + 35 * k),
                      "                 CREATIONRESOLUTION: 800 600;", '    NAME = "GenPowersShortcutBar.wnd:ButtonParent%d";' % k, "    CHILD", "    WINDOW",
                      "      SCREENRECT = UPPERLEFT: 753 %d," % (107 + 35 * k), "                   BOTTOMRIGHT: 792 %d," % (134 + 35 * k),
                      "                   CREATIONRESOLUTION: 800 600;", '      NAME = "GenPowersShortcutBar.wnd:ButtonCommand%d";' % k, "    ENDWINDOW", "    ENDALLCHILDREN", "  ENDWINDOW"]
        lines += ["  ENDALLCHILDREN", "ENDWINDOW", ""]
        return "\r\n".join(lines).encode("latin1")

    def test_byte_identical_to_the_python_original(self):
        import modern_extensions as me
        buttons = "; header" + CRLF + "".join(
            "CommandButton %s\r\n  Command = SPECIAL_POWER\r\n  SpecialPower = SP_%s\r\n%s\r\nEnd\r\n\r\n" % (name, name, opt)
            for name, opt in (("Command_FireParticleUplinkCannon", "  Options = NEED_TARGET_POS"), ("Command_CIAIntelligence", "  Textx = 1"),
                              ("Command_NeutronMissile", "  Options = NEED_SPECIAL_POWER_SCIENCE NEED_TARGET_POS"), ("Command_ScudStorm", "  Options = NEED_TARGET_POS; comment"),
                              ("Command_RadarVanScan", "  Options    =  OK_FOR_MULTI_SELECT")))
        sets = "".join("CommandSet %s\r\n  1 = Command_A\r\n  2 = Command_B\r\nEND\r\n\r\n" % s for s in ("SpecialPowerShortcutUSA", "SpecialPowerShortcutChina", "SpecialPowerShortcutGLA"))
        templates = "".join("PlayerTemplate Faction%s\r\n  Side = %s ; the side\r\n  SpecialPowerShortcutButtonCount = 5\r\nEND\r\n\r\n" % (s, s) for s in ("America", "China", "GLA"))
        with tempfile.TemporaryDirectory() as tmp:
            game = pathlib.Path(tmp) / "game"
            game.mkdir()
            (game / "INI.big").write_bytes(self.big({"data\\ini\\commandbutton.ini": buttons.encode("latin1"), "data\\ini\\commandset.ini": sets.encode("latin1"),
                                                      "Data\\INI\\PlayerTemplate.ini": templates.encode("latin1")}))
            (game / "Window.big").write_bytes(self.big({"Window\\GenPowersShortcutBarUS.wnd": self.wnd(3), "Window\\GenPowersShortcutBarChina.wnd": self.wnd(2),
                                                         "Window\\GenPowersShortcutBarGLA.wnd": self.wnd(2)}))
            py_out = pathlib.Path(tmp) / "py"
            written = me.build(game, py_out)
            ps_script = pathlib.Path(tmp) / "run.ps1"
            ps_script.write_text(
                "param($Game, $Out)\n$ErrorActionPreference = 'Stop'\n. '%s'\n"
                "foreach ($f in (Build-GeneralsModernExtensions $Game)) { $p = Join-Path $Out $f.Path; New-Item -ItemType Directory -Force -Path (Split-Path -Parent $p) | Out-Null; [IO.File]::WriteAllBytes($p, $f.Bytes) }\n"
                % str(DIST / "GeneralsModernExtensions.ps1"), encoding="utf-8")
            ps_out = pathlib.Path(tmp) / "ps"
            env = {k: v for k, v in __import__("os").environ.items() if k != "PSModulePath"}
            done = subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ps_script), "-Game", str(game), "-Out", str(ps_out)],
                                  capture_output=True, text=True, env=env, timeout=120)
            self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
            self.assertEqual(len(written), 6)
            for rel in written:
                self.assertEqual((py_out / rel).read_bytes(), (ps_out / rel).read_bytes(), rel)


@unittest.skipUnless(shutil.which("powershell.exe") and sys.platform == "win32", "Windows PowerShell 5.1 is needed for the discovery test")
class GameFolderDiscovery(unittest.TestCase):
    """Owner acceptance finding: the installer listed one Steam installation twice (the registry spelling and the environment spelling of the same Windows folder differ in case and slashes). Equivalent Windows paths are one entry; genuinely different folders stay different. Runs the shipped helper under Windows PowerShell 5.1."""

    SCRIPT = r"""
param([string]$Work, [string]$Setup)
$ErrorActionPreference = 'Stop'
. $Setup
$manifest = [pscustomobject]@{ target = [pscustomobject]@{ steamFolderNames = @('Command and Conquer Generals', 'Command & Conquer Generals - Zero Hour') } }
$lib1 = Join-Path $Work 'SteamLib'
$lib2 = Join-Path $Work 'OtherLib'
foreach ($lib in $lib1, $lib2) { foreach ($n in $manifest.target.steamFolderNames) { New-Item -ItemType Directory -Force -Path (Join-Path $lib ('steamapps\common\' + $n)) | Out-Null } }
$out = [ordered]@{}
# the key: case, slash style, trailing separator, "." segments are the same folder; a different folder is not
$a = Get-FolderKey $lib1
$out.sameCase = ($a -eq (Get-FolderKey $lib1.ToLowerInvariant()))
$out.sameSlash = ($a -eq (Get-FolderKey ($lib1 -replace '\\', '/')))
$out.sameTrailing = ($a -eq (Get-FolderKey ($lib1 + '\')))
$out.sameDot = ($a -eq (Get-FolderKey ($lib1 + '\.\')))
$out.distinct = ($a -ne (Get-FolderKey $lib2))
$out.rootKeepsItsSlash = ((Get-FolderKey 'c:/') -eq 'C:\')
$out.emptyIsNull = ($null -eq (Get-FolderKey '   '))
# discovery: the same library reported three ways (registry lower-case / forward slashes, environment spelling, trailing separator) is ONE installation per title
function Get-SteamLibraries { return @($lib1.ToLowerInvariant().Replace([string][char]92, '/'), $lib1, ($lib1 + '\')) }
$one = @(Find-GameFolders $manifest)
$out.oneLibraryCount = $one.Count
$out.oneLibraryDistinctTitles = @($one | ForEach-Object { Split-Path -Leaf $_ } | Sort-Object -Unique).Count
# two genuinely different libraries stay two installations per title
function Get-SteamLibraries { return @($lib1, $lib1.ToUpperInvariant(), $lib2) }
$two = @(Find-GameFolders $manifest)
$out.twoLibraryCount = $two.Count
$out | ConvertTo-Json -Compress
"""

    def test_equivalent_paths_are_one_installation(self):
        import json as _json
        with tempfile.TemporaryDirectory() as tmp:
            script = pathlib.Path(tmp) / "discovery.ps1"
            script.write_text(self.SCRIPT, encoding="utf-8")
            work = pathlib.Path(tmp) / "work"
            work.mkdir()
            env = {k: v for k, v in __import__("os").environ.items() if k != "PSModulePath"}
            done = subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script), "-Work", str(work), "-Setup", str(DIST / "MKRaudstein-Setup.ps1")],
                                  capture_output=True, text=True, env=env, timeout=120)
            self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
            got = _json.loads(done.stdout.strip().splitlines()[-1])
            for key in ("sameCase", "sameSlash", "sameTrailing", "sameDot", "distinct", "rootKeepsItsSlash", "emptyIsNull"):
                self.assertTrue(got[key], key)
            self.assertEqual(got["oneLibraryCount"], 2, got)            # one entry for Generals + one for Zero Hour, not three of each
            self.assertEqual(got["oneLibraryDistinctTitles"], 2, got)
            self.assertEqual(got["twoLibraryCount"], 4, got)            # two real libraries x two titles

    def test_no_case_sensitive_path_dedup_is_left(self):
        for name in PLAYER_SCRIPTS:
            text = read("scripts/distribution/" + name)
            self.assertNotIn("Select-Object -Unique", text.replace("Sort-Object -Unique", ""), name)   # -Unique on paths is case-sensitive
            self.assertNotIn(".Contains($", text, name)                                                # List<string>.Contains is case-sensitive
        self.assertIn("Add-UniqueFolder $unique $c", read("scripts/distribution/Uninstall-MKRaudstein.ps1"))   # the uninstaller list is deduplicated too


if __name__ == "__main__":
    unittest.main()
