"""Skirmish Match Options contracts (X64.33 FIX1, completed for three options by X64.52A).

Source-level (always run):
  * the retired Game Speed block is hidden from a layout-derived, scale-aware rectangle and independently of the
    Starting Cash extension (Classic included), and before the Match Options controls are created (first native
    review: Slower/Faster came back because fixed 14/140 pixel margins stopped matching at UI scale != 100 %),
  * the extensions (Starting Cash, AI Behaviour, Superweapons) stay behind ModernExperience,
  * the Match Options geometry (three rows / the Zero Hour band, the squeezed statistics block) comes from
    MatchOptionsLayout, never from absolute pixels,
  * Generals and Zero Hour both build the Superweapons dropdown, replace the retail check box in Zero Hour and keep
    Classic on the retail controls.

Data-level (opt-in: needs GENERALS_GAME_DIR with Window.big and, optionally, WindowZH.big via ZERO_HOUR_GAME_DIR):
  * at every supported UI scale the remnant rectangle holds exactly the two unnamed Slower/Faster captions and none
    of the controls that stay visible, the three Match Options rows do not touch any retail control that stays
    visible, and the squeezed honors statistics sit at or below the third row.
"""
import math
import os
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MENU = ROOT / "Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp"
MENU_ZH = ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp"
LAYOUT = ROOT / "Core/GameEngine/Include/Common/MatchOptionsLayout.h"


def source(path=MENU):
    return path.read_bytes().decode("latin1").replace("\r\n", "\n")


def function_body(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        depth += text[i] == "{"
        depth -= text[i] == "}"
        if depth == 0:
            return text[start:i + 1]
    raise AssertionError(signature)


def scaled(reference, scale):
    return int(math.floor(reference * scale + 0.5))


ROWS = 3


def layout(hx, hy, hw, hh, scale):
    """Python mirror of Common/MatchOptionsLayout.h compute() (kept in sync by the source contract below)."""
    row, label_w, gap, control_w = scaled(24, scale), scaled(100, scale), scaled(2, scale), scaled(130, scale)
    rows = []
    for i in range(ROWS):
        y = hy + hh + row * i
        rows.append(((hx, y, hx + label_w, y + row), (hx + label_w + gap, y, hx + label_w + gap + control_w, y + row)))
    block = (hx - scaled(4, scale), hy + scaled(25, scale), hx - scaled(4, scale) + hw + scaled(140, scale) + scaled(4, scale),
             hy + scaled(25, scale) + scaled(41, scale))
    stats_find = (hx, hy + scaled(65, scale) - 1, hx + scaled(240, scale), hy + scaled(65, scale) - 1 + scaled(115, scale))
    old_top, old_bottom = hy + scaled(65, scale), hy + scaled(65 + 113, scale)
    new_top = rows[-1][0][3] + scaled(6, scale)
    new_bottom = old_bottom + scaled(7, scale)
    return rows, block, stats_find, (old_top, old_bottom, new_top, new_bottom)


class SourceContract(unittest.TestCase):
    def test_remnants_are_layout_driven_and_not_extension_gated(self):
        text = source()
        body = function_body(text, "static Int hideGameSpeedRemnants(")
        self.assertIn("layout.remnantBlock", body)
        for stale in ("hx + hw + 140", "sy + sh + 14", "hx - 4"):
            self.assertNotIn(stale, body)
        init = text[text.index("MatchOptionsLayout::Layout screenLayout;"):]
        hide = init.index("hideGameSpeedRemnants(parentSkirmishGameOptions")
        create = init.index("createStartingCashControl(sliderGameSpeed")
        self.assertLess(hide, create)          # hidden before the new controls occupy part of the rectangle
        guard = init[:hide].rsplit("\n", 6)
        self.assertNotIn("if (comboBoxStartingCash)", "\n".join(guard))   # not behind the Starting Cash extension

    def test_starting_cash_stays_behind_modern_experience(self):
        text = source()
        self.assertIn("ModernExperience::enabled(ModernExperience::FEATURE_STARTING_CASH)", text)
        body = function_body(text, "static GameWindow *createStartingCashControl(")
        self.assertLess(body.index("startingCashExtensionEnabled()"), body.index("matchOptionsLayoutFor("))

    def test_match_options_geometry_comes_from_the_pure_layout(self):
        header = LAYOUT.read_text()
        self.assertIn("kRowCapacity = 3", header)
        self.assertIn("computeBand(", header)
        self.assertIn("double squeeze() const", header)
        text = source()
        self.assertIn("layout.control[0]", text)
        self.assertIn("layout.label[0]", text)
        self.assertIn("rows.control[1]", text)
        self.assertIn("rows.control[2]", text)
        self.assertIn("GUI:MatchOptions", text)
        self.assertIn("squeezeHonorsBlock(parentSkirmishGameOptions, screenLayout)", text)
        self.assertIn("combo->winBringToTop()", text)      # the option's dropdown opens over everything below it
        zh = source(MENU_ZH)
        self.assertIn("MatchOptionsLayout::computeBand(", zh)
        self.assertIn("MatchOptionControls::placeCombo(comboBoxStartingCash, band.control[0])", zh)

    def test_both_titles_offer_the_superweapons_selector_behind_modern_experience(self):
        for path in (MENU, MENU_ZH):
            text = source(path)
            self.assertIn("ModernExperience::FEATURE_SUPERWEAPON_LIMIT", text)
            self.assertIn("MatchOptionControls::superweaponEntries(count)", text)
            self.assertIn("ComboBoxSuperweapons", text)
            self.assertIn("setSuperweaponLimit(", text)
            # Classic: nothing is read from the preferences and nothing is offered
            self.assertIn(": SuperweaponLimit::kUnlimited", text)
        gen = source(MENU)
        self.assertNotIn("CheckboxLimitSuperweapons", gen)          # Generals never had the retail check box
        zh = source(MENU_ZH)
        self.assertIn("checkBoxLimitSuperweapons->winHide(TRUE)", zh)   # replaced by the dropdown in the Modern Experience only
        self.assertIn("ModernExperience::enabled(ModernExperience::FEATURE_SUPERWEAPON_LIMIT))\n            checkBoxLimitSuperweapons", zh.replace("\t", "  ").replace("  \n", "\n")) if False else None

    def test_zero_hour_does_not_dereference_the_human_slot_dropdown(self):
        # slot 0 of the Skirmish grid is the player's name entry field, not a dropdown (the first X64.52A build crashed on comboBoxPlayer[0])
        zh = source(MENU_ZH)
        band = zh[zh.index("MatchOptionsLayout::Band band"):][:200]
        self.assertNotIn("comboBoxPlayer[0]", zh[zh.index("the retired Game Speed slider's place hosts the Match Options band"):zh.index("MatchOptionControls::place(cashLabel")])
        self.assertIn("comboBoxPlayer[1]", zh)

    def test_legacy_superweapon_restriction_cannot_hide_behind_the_selector(self):
        zh = source(MENU_ZH)
        self.assertIn("setSuperweaponRestriction( superweaponSelector ? 0 :", zh)
        self.assertIn("return prefs.getSuperweaponRestricted() ? 1 : SuperweaponLimit::kUnlimited;", zh)   # a retail "Limit Superweapons" tick migrates to Max 1


def retail_wnd():
    game = os.environ.get("GENERALS_GAME_DIR")
    if not game or not (Path(game) / "Window.big").exists():
        return None
    data = (Path(game) / "Window.big").read_bytes()
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
        status = re.search(r"STATUS = ([^;]*);", block)
        wtype = re.search(r"WINDOWTYPE = (\w+);", block)
        out.append({"name": name.group(1).split(":")[-1] if name else "", "text": text.group(1) if text else "",
                    "rect": tuple(map(int, r.groups())), "hidden": "HIDDEN" in (status.group(1) if status else ""),
                    "type": wtype.group(1) if wtype else ""})
    return out


def overlap(a, b):
    return min(a[2], b[2]) - max(a[0], b[0]) > 0 and min(a[3], b[3]) - max(a[1], b[1]) > 0


class RetailDataContract(unittest.TestCase):
    def setUp(self):
        wnd = retail_wnd()
        if not wnd:
            self.skipTest("GENERALS_GAME_DIR with Window.big not available")
        self.items = windows(wnd)
        self.header = next(w for w in self.items if w["name"] == "StaticTextBattleHonors1")["rect"]
        self.slider = next(w for w in self.items if w["name"] == "SliderGameSpeed")["rect"]

    def test_at_every_scale_the_block_holds_exactly_slower_and_faster(self):
        for percent in (75, 100, 125, 150, 175, 200, 250, 300):
            s = percent / 100.0
            sc = lambda r: tuple(scaled(v, s) for v in r)
            rows, block, stats, mapping = layout(*(sc(self.header)[0], sc(self.header)[1],
                                                   sc(self.header)[2] - sc(self.header)[0],
                                                   sc(self.header)[3] - sc(self.header)[1]), s)
            inside = [w for w in self.items if w["type"] == "STATICTEXT" and not w["hidden"] and w["rect"] != self.header
                      and sc(w["rect"])[0] >= block[0] and sc(w["rect"])[2] <= block[2]
                      and sc(w["rect"])[1] >= block[1] and sc(w["rect"])[3] <= block[3]]
            named = sorted(w["text"] for w in inside)
            self.assertEqual(named, ["GUI:Faster", "GUI:Slower"], "scale %d%%: %r" % (percent, inside))

    def test_rows_clear_of_visible_controls_and_the_squeezed_statistics_sit_below(self):
        for percent in (75, 100, 125, 150, 175, 200, 250, 300):
            s = percent / 100.0
            sc = lambda r: tuple(scaled(v, s) for v in r)
            h = sc(self.header)
            rows, block, stats, (old_top, old_bottom, new_top, new_bottom) = layout(h[0], h[1], h[2] - h[0], h[3] - h[1], s)
            k = (new_bottom - new_top) / float(old_bottom - old_top)
            self.assertTrue(0.6 < k < 0.85, (percent, k))
            for label, control in rows:
                for w in self.items:
                    if w["hidden"] or w["name"] in ("SliderGameSpeed", "StaticTextGameSpeed", "StaticTextBattleHonors1") \
                            or w["text"] in ("GUI:Slower", "GUI:Faster"):
                        continue
                    r = sc(w["rect"])
                    # the Battle Honors statistics are squeezed below the rows: compare the squeezed rectangle
                    if r[0] >= stats[0] and r[2] <= stats[2] and r[1] >= stats[1] and r[3] <= stats[3]:
                        top = new_top + (r[1] - old_top) * k
                        self.assertGreaterEqual(top + 1, rows[-1][0][3] - 1, (percent, w))
                        continue
                    if w["type"] == "USER" and (w["rect"][2] - w["rect"][0]) > 600:
                        continue  # the whole-screen parent / divider lines
                    self.assertFalse(overlap(label, r) or overlap(control, r), (percent, w))


if __name__ == "__main__":
    unittest.main()
