#!/usr/bin/env python3
"""Zero Hour Modern HUD parity: source / documentation contracts (the geometry itself is covered by the EnhancedHudZeroHour gtests, the native captures by the milestone evidence)."""
import pathlib
import re
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


LAYOUT = "Core/GameEngine/Include/Common/EnhancedHudLayout.h"
DOC = "docs/modernization/ZERO_HOUR_MODERN_HUD.md"
GTEST = "Tests/Google/Core/GameEngine/Common/EnhancedHudLayoutTest.cpp"


class ZeroHourHudContracts(unittest.TestCase):
    def test_the_layout_headers_know_a_grid_profile_but_never_a_title(self):
        # a title is a data choice made once in ControlBarScheme::init; the shared headers stay title and faction free
        for path in (LAYOUT, "Core/GameEngine/Include/Common/EnhancedHudWindows.h", "Core/GameEngine/Include/Common/EnhancedHud.h"):
            text = read(path)
            self.assertNotIn("RTS_ZEROHOUR", text, path)
            self.assertNotIn("RTS_GENERALS", text, path)
        layout = read(LAYOUT)
        self.assertIn("enum class CommandGrid { Standard, Wide };", layout)
        self.assertIn("constexpr int kCommandSlotsStandard = 12;", layout)
        self.assertIn("constexpr int kCommandSlotsWide = 14;", layout)

    def test_the_accepted_generals_grid_is_untouched(self):
        layout = read(LAYOUT)
        self.assertIn("constexpr int kCommandScalePermille = 1200;", layout)
        standard = layout[layout.index("static const GroupWindow table[] = {\n\t\t{\"CommandWindow\""):]
        standard = standard[:standard.index("count = static_cast")]
        self.assertEqual(len(re.findall(r'\{"ButtonCommand\d\d"', standard)), 12)
        self.assertIn('{"ButtonCommand12",           {546, 546,  53,  45}},', standard)
        self.assertNotIn("ButtonCommand13", standard)
        # the Generals mapping is the first branch's fall-through: origin 246,493 at 1.2 x
        self.assertIn("return {409 + ((src.x - 246) * k) / 1000, 44 + ((src.y - 493) * k) / 1000, (src.w * k) / 1000, (src.h * k) / 1000};", layout)

    def test_the_wide_grid_is_the_retail_zero_hour_grid(self):
        layout = read(LAYOUT)
        wide = layout[layout.index("static const GroupWindow wide[] = {"):]
        wide = wide[:wide.index("count = static_cast")]
        buttons = re.findall(r'\{"ButtonCommand(\d\d)",\s+\{(\d+), (\d+),\s+(\d+),\s+(\d+)\}\}', wide)
        self.assertEqual([b[0] for b in buttons], ["%02d" % i for i in range(1, 15)])
        # ControlBar.wnd of the Zero Hour archives (800 x 600): 7 columns on a 55 pitch from x 223, rows at y 494 / 545, 50 x 44 buttons
        for index, (_, x, y, w, h) in enumerate(buttons):
            self.assertEqual((int(x), int(y), int(w), int(h)), (223 + 55 * (index // 2), 494 if index % 2 == 0 else 545, 50, 44), index + 1)

    def test_the_scheme_selects_the_grid_by_title_and_offers_every_side(self):
        scheme = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp")
        self.assertIn("EnhancedHudLayout::CommandGrid::Wide", scheme)
        self.assertNotIn("enhancedSide", scheme)

    def test_the_documented_coverage_matrix_equals_the_sides_the_unit_test_covers(self):
        doc = read(DOC)
        matrix = doc[doc.index("## 2. Coverage matrix"):doc.index("## 3. Architecture")]
        documented = re.findall(r"^\| (\w+)(?: \(\w+\))? \| (?:USA|China|GLA|-) \|", matrix, re.M)
        test = read(GTEST)
        covered = re.findall(r'"(\w+)"', test[test.index("kZeroHourSides[] = {"):test.index("}", test.index("kZeroHourSides[] = {"))])
        playable = [s for s in documented if s not in ("Observer", "Civilian")]
        self.assertEqual(sorted(s if s != "Boss" else "Boss" for s in playable), sorted(covered))
        self.assertEqual(len(covered), 13)                       # 3 base + 9 generals + Boss
        self.assertIn("Observer", documented)
        self.assertIn("Civilian", documented)

    def test_the_dev_harness_override_never_reaches_gameplay_code(self):
        found = subprocess.run(["git", "grep", "-l", "GENERALS_AUTOFLOW_SIDE", "--", "*.cpp", "*.h"], cwd=ROOT, capture_output=True, text=True)
        files = [pathlib.PurePosixPath(line).name for line in found.stdout.split() if line]
        self.assertEqual(files, ["W3DDisplayOrdinaryHost.cpp"])    # the autoflow harness only, never a gameplay or HUD file


if __name__ == "__main__":
    unittest.main()
