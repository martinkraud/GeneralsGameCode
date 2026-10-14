"""Tests for the shortcut-sidebar modern extension overlay (modern_extensions.py)."""
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import modern_extensions as mx  # noqa: E402

BUTTONS = """
CommandButton Command_FireParticleUplinkCannon
  Command           = SPECIAL_POWER
  SpecialPower      = SuperweaponParticleUplinkCannon
  Options           = NEED_SPECIAL_POWER_SCIENCE NEED_TARGET_POS CONTEXTMODE_COMMAND
  TextLabel         = CONTROLBAR:FireParticleUplinkCannon
End

CommandButton Command_CIAIntelligence
  Command           = SPECIAL_POWER
  SpecialPower      = SuperweaponCIAIntelligence
;  Options           =  ; Commented out to show lack of Options needed
  TextLabel         = CONTROLBAR:CIAIntelligence
End

CommandButton Command_NeutronMissile
  Command           = SPECIAL_POWER
  SpecialPower      = SuperweaponNeutronMissile
  Options           = NEED_TARGET_POS CONTEXTMODE_COMMAND
End

CommandButton Command_ScudStorm
  Command                 = SPECIAL_POWER
  SpecialPower            = SuperweaponScudStorm
  Options                 = NEED_TARGET_POS CONTEXTMODE_COMMAND
End

CommandButton Command_RadarVanScan
  Command           = SPECIAL_POWER
  SpecialPower      = SpecialPowerRadarVanScan
  Options           = NEED_TARGET_POS CONTEXTMODE_COMMAND NEED_UPGRADE OK_FOR_MULTI_SELECT
  Upgrade           = Upgrade_GLARadarVanScan
End
"""

SETS = """
CommandSet SpecialPowerShortcutUSA
  3 = Command_A10ThunderboltMissileStrikeFromCommandCenter
  1 = Command_SpyDroneFromCommandCenter
  5 = Command_DaisyCutterFromCommandCenter
END

CommandSet SpecialPowerShortcutChina
  1 = Command_ClusterMinesFromCommandCenter
  5 = Command_EMPPulseFromCommandCenter
END

CommandSet SpecialPowerShortcutGLA
  1 =Command_AmbushFromCommandCenter
  3 =Command_AnthraxBombFromCommandCenter
END
"""


def wnd(slots):
    out = ['FILE_VERSION = 2;', '', 'WINDOW', '  WINDOWTYPE = USER;',
           '  SCREENRECT = UPPERLEFT: 740 159,', '               BOTTOMRIGHT: 800 429,',
           '               CREATIONRESOLUTION: 800 600;',
           '  NAME = "GenPowersShortcutBarUS.wnd:GenPowersShortcutBarParent";']
    for k in range(1, slots + 1):
        y = 159 + 49 * (k - 1)
        out += ['  CHILD', '  WINDOW', '    WINDOWTYPE = USER;', '    SCREENRECT = UPPERLEFT: 740 %d,' % y,
                '                 BOTTOMRIGHT: 799 %d,' % (y + 56), '                 CREATIONRESOLUTION: 800 600;',
                '    NAME = "GenPowersShortcutBarUS.wnd:ButtonParent%d";' % k,
                '    CHILD', '    WINDOW', '      WINDOWTYPE = PUSHBUTTON;',
                '      SCREENRECT = UPPERLEFT: 743 %d,' % (y + 9), '                   BOTTOMRIGHT: 788 %d,' % (y + 47),
                '                   CREATIONRESOLUTION: 800 600;',
                '      NAME = "GenPowersShortcutBarUS.wnd:ButtonCommand%d";' % k,
                '    END', '    ENDALLCHILDREN', '  END']
    out += ['  ENDALLCHILDREN', 'END', '']
    return "\n".join(out)


class ClonedButtons(unittest.TestCase):
    def test_plain_special_powers_become_shortcut_buttons_with_the_science_gate(self):
        out = mx.patch_command_buttons(BUTTONS)
        for new, _src in mx.NEW_BUTTONS:
            m = re.search(r"CommandButton " + new + r"\n([\s\S]*?)\nEnd", out)
            self.assertIsNotNone(m, new)
            self.assertIn("SPECIAL_POWER_FROM_SHORTCUT", m.group(1))
            self.assertIn("NEED_SPECIAL_POWER_SCIENCE", m.group(1))
        # retail buttons are preserved verbatim
        self.assertIn("CommandButton Command_FireParticleUplinkCannon\n  Command           = SPECIAL_POWER\n", out)

    def test_options_line_is_added_when_the_retail_button_has_none(self):
        out = mx.clone_button(BUTTONS, "Command_CIAIntelligence", "Command_CIAIntelligenceFromShortcut")
        self.assertIn("\n  Options           = NEED_SPECIAL_POWER_SCIENCE\n", out)

    def test_existing_options_keep_their_flags(self):
        out = mx.clone_button(BUTTONS, "Command_RadarVanScan", "X")
        self.assertIn("NEED_SPECIAL_POWER_SCIENCE NEED_TARGET_POS CONTEXTMODE_COMMAND NEED_UPGRADE OK_FOR_MULTI_SELECT", out)
        self.assertIn("Upgrade           = Upgrade_GLARadarVanScan", out)

    def test_duplicate_definitions_are_refused(self):
        with self.assertRaises(ValueError):
            mx.patch_command_buttons(BUTTONS + "\nCommandButton Command_ScudStormFromShortcut\nEnd\n")


class CommandSets(unittest.TestCase):
    def test_additions_follow_the_retail_entries(self):
        out = mx.patch_command_sets(SETS)
        usa = re.search(r"CommandSet SpecialPowerShortcutUSA\n([\s\S]*?)\nEND", out).group(1)
        for slot, button in mx.SET_ADDITIONS["SpecialPowerShortcutUSA"]:
            self.assertIn("%d = %s" % (slot, button), usa)
        self.assertIn("3 = Command_A10ThunderboltMissileStrikeFromCommandCenter", usa)  # retail untouched

    def test_slot_collisions_are_refused(self):
        broken = SETS.replace("  5 = Command_DaisyCutterFromCommandCenter", "  6 = Command_X")
        with self.assertRaises(ValueError):
            mx.patch_command_sets(broken)

    def test_every_added_slot_fits_the_bar_slot_count(self):
        counts = {"SpecialPowerShortcutUSA": 8, "SpecialPowerShortcutChina": 6, "SpecialPowerShortcutGLA": 5}
        for set_name, adds in mx.SET_ADDITIONS.items():
            self.assertEqual(max(s for s, _ in adds), counts[set_name])
        self.assertEqual({v[1] for v in mx.BARS.values()}, {8, 6, 5})


class PlayerTemplate(unittest.TestCase):
    def test_button_counts_follow_the_side(self):
        text = ("PlayerTemplate FactionAmerica\n  Side = America\n  SpecialPowerShortcutButtonCount = 5\nEnd\n"
                "PlayerTemplate FactionChina\n  Side = China\n  SpecialPowerShortcutButtonCount = 5\nEnd\n"
                "PlayerTemplate FactionGLA\n  Side = GLA\n  SpecialPowerShortcutButtonCount = 3\nEnd\n")
        out = mx.patch_player_template(text)
        counts = re.findall(r"SpecialPowerShortcutButtonCount = (\d+)", out)
        self.assertEqual(counts, ["8", "6", "5"])


class Bars(unittest.TestCase):
    def rects(self, text):
        found = re.findall(r"UPPERLEFT: (\d+) (\d+),\s*BOTTOMRIGHT: (\d+) (\d+)", text)
        return [tuple(map(int, r)) for r in found]

    def test_zero_hour_geometry_bottom_anchored(self):
        out = mx.build_wnd(wnd(5), 8, 429)
        rects = self.rects(out)
        self.assertEqual(len(rects), 1 + 8 * 2)
        self.assertEqual(rects[0], (752, 429 - 41 - 35 * 7, 800, 429))
        for k in range(8):
            parent, command = rects[1 + 2 * k], rects[2 + 2 * k]
            self.assertEqual(parent[2] - parent[0], 48)
            self.assertEqual(parent[3] - parent[1], 41)
            self.assertEqual(command, (parent[0] + 1, parent[1] + 7, parent[0] + 40, parent[1] + 34))
            if k:
                self.assertEqual(parent[1] - self.rects(out)[1 + 2 * (k - 1)][1], 35)
        self.assertEqual(rects[-2][3], 429)       # last slot rests on the bar bottom

    def test_names_are_renumbered_and_structure_is_intact(self):
        out = mx.build_wnd(wnd(5), 8, 429)
        self.assertEqual([int(n) for n in re.findall(r"ButtonParent(\d+)\"", out)], list(range(1, 9)))
        self.assertEqual([int(n) for n in re.findall(r"ButtonCommand(\d+)\"", out)], list(range(1, 9)))
        self.assertEqual(out.count("ENDALLCHILDREN"), 8 + 1)

    def test_fewer_slots_than_retail_is_valid(self):
        out = mx.build_wnd(wnd(5), 3, 421)
        self.assertEqual(len(self.rects(out)), 1 + 3 * 2)

    def test_malformed_wnd_is_refused(self):
        with self.assertRaises(ValueError):
            mx.build_wnd("FILE_VERSION = 2;\nWINDOW\nEND\n", 3, 400)


class RetailData(unittest.TestCase):
    """Opt-in: GENERALS_GAME_DIR names the installed Generals directory (INI.big, Window.big)."""

    def setUp(self):
        game = os.environ.get("GENERALS_GAME_DIR")
        if not game or not (Path(game) / "INI.big").exists():
            self.skipTest("GENERALS_GAME_DIR not available")
        self.game = Path(game)

    def test_overlay_builds_from_the_retail_archives(self):
        with tempfile.TemporaryDirectory() as tmp:
            written = mx.build(self.game, tmp)
            self.assertEqual(len(written), 6)
            cs = (Path(tmp) / "Data/INI/CommandSet.ini").read_bytes().decode("latin1")
            self.assertIn("Command_FireParticleUplinkCannonFromShortcut", cs)
            cb = (Path(tmp) / "Data/INI/CommandButton.ini").read_bytes().decode("latin1")
            for new, _ in mx.NEW_BUTTONS:
                self.assertIn("CommandButton " + new, cb)
            # every command set entry names a button that exists (retail or new)
            names = set(re.findall(r"CommandButton (\w+)", cb))
            for set_name, adds in mx.SET_ADDITIONS.items():
                for _slot, button in adds:
                    self.assertIn(button, names)
            for wnd_name, (_side, slots, bottom) in mx.BARS.items():
                text = (Path(tmp) / "Window" / wnd_name).read_bytes().decode("latin1")
                self.assertEqual(len(re.findall(r"ButtonCommand\d+\"", text)), slots)
                top = bottom - 41 - 35 * (slots - 1)
                self.assertGreater(top, 60)           # clear of the superweapon timers at the top right
                self.assertLessEqual(bottom, 430)

    def test_zero_hour_geometry_constants_match_the_zero_hour_archive(self):
        zh = self.game.parent / "WindowZH.big"
        if not zh.exists():
            self.skipTest("Zero Hour archives not installed beside the Generals directory")
        text = mx.to_text(mx.read_big(zh)["window\\genpowersshortcutbarus.wnd"])
        rects = [tuple(map(int, r)) for r in re.findall(r"UPPERLEFT: (\d+) (\d+),\s*BOTTOMRIGHT: (\d+) (\d+)", text)]
        parent, command = rects[1], rects[2]
        self.assertEqual((parent[2] - parent[0], parent[3] - parent[1]), (mx.SLOT_W, mx.SLOT_H))
        self.assertEqual((command[0] - parent[0], command[1] - parent[1], command[2] - command[0], command[3] - command[1]),
                         (mx.CMD_DX, mx.CMD_DY, mx.CMD_W, mx.CMD_H))
        self.assertEqual(rects[3][1] - parent[1], -mx.SLOT_PITCH)   # Zero Hour stacks slots upward by the pitch


if __name__ == "__main__":
    unittest.main()
