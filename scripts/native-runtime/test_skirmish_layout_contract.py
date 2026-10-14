"""Retail Skirmish layout facts the Starting Cash placement relies on (opt-in: needs GENERALS_GAME_DIR).

Reads SkirmishGameOptionsMenu.wnd from Window.big and checks, from the actual WND geometry, that
  * the Game Speed block's remnant filter rectangle contains exactly the unnamed Slower/Faster captions, and
  * none of the controls that must stay visible falls inside it, and
  * the Starting Cash control (header x, header width, 24 px, flush under the header) stays clear of the Battle
    Honors rows below.
"""
import os
import re
import unittest
from pathlib import Path


def load_wnd():
    game = os.environ.get("GENERALS_GAME_DIR")
    if not game:
        return None
    big = Path(game) / "Window.big"
    if not big.exists():
        return None
    data = big.read_bytes()
    # BIG directory: entries are  offset(4,BE) size(4,BE) name\0  starting at byte 16.
    count = int.from_bytes(data[8:12], "big")
    pos = 16
    for _ in range(count):
        off = int.from_bytes(data[pos:pos + 4], "big")
        size = int.from_bytes(data[pos + 4:pos + 8], "big")
        end = data.index(b"\0", pos + 8)
        name = data[pos + 8:end].decode("latin1").lower().replace("/", "\\")
        pos = end + 1
        if name.endswith("menus\\skirmishgameoptionsmenu.wnd"):
            return data[off:off + size].decode("latin1")
    return None


RECT = re.compile(r"UPPERLEFT: (\d+) (\d+),\s*BOTTOMRIGHT: (\d+) (\d+)")


def windows(wnd):
    out = []
    for block in re.split(r"\n\s*(?:CHILD\s*\n\s*)?WINDOW\s*\n", wnd)[1:]:
        r = RECT.search(block)
        if not r:
            continue
        name = re.search(r'NAME = "([^"]*)"', block)
        text = re.search(r'\n\s*TEXT = "([^"]*)"', block)
        out.append({
            "name": name.group(1).split(":")[-1] if name else "",
            "text": text.group(1) if text else "",
            "rect": tuple(map(int, r.groups())),
        })
    return out


class SkirmishLayoutContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.wnd = load_wnd()

    def setUp(self):
        if not self.wnd:
            self.skipTest("GENERALS_GAME_DIR with Window.big not available")
        self.items = windows(self.wnd)

    def one(self, **kw):
        found = [w for w in self.items if all(w[k] == v for k, v in kw.items())]
        self.assertEqual(len(found), 1, kw)
        return found[0]["rect"]

    def inside(self, r):
        header = self.one(name="StaticTextBattleHonors1")
        slider = self.one(name="SliderGameSpeed")
        left, right = header[0] - 4, header[2] + 140
        top, bottom = slider[1], slider[3] + 14
        return r[0] >= left and r[2] <= right and r[1] >= top and r[3] <= bottom

    def test_filter_rectangle_contains_exactly_the_slower_and_faster_captions(self):
        self.assertTrue(self.inside(self.one(text="GUI:Slower")))
        self.assertTrue(self.inside(self.one(text="GUI:Faster")))
        for w in self.items:
            if w["text"] in ("GUI:Slower", "GUI:Faster") or w["name"] in ("StaticTextBattleHonors1", "SliderGameSpeed",
                                                                          "StaticTextGameSpeed"):
                continue
            self.assertFalse(self.inside(w["rect"]), (w["name"], w["text"], w["rect"]))

    def test_starting_cash_row_clears_the_battle_honors_block(self):
        header = self.one(name="StaticTextBattleHonors1")
        combo_bottom = header[3] + 24                 # flush under the header, one 24 px grid row
        honors_top = min(w["rect"][1] for w in self.items if w["name"].startswith("StaticText") and
                         w["name"] in ("StaticTextBestStreak", "StaticTextBestStreakValue"))
        self.assertLess(combo_bottom, honors_top)
        self.assertGreaterEqual(honors_top - combo_bottom, 12)  # visible breathing room


if __name__ == "__main__":
    unittest.main()
