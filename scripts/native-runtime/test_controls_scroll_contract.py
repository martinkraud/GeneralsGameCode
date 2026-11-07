#!/usr/bin/env python3
"""Controls / Keyboard scroll fix: source contract (the native Options audit is the dynamic proof; see docs/modernization/MODERN_SETTINGS_ARCHITECTURE.md)."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace(chr(13) + chr(10), chr(10))


class ControlsScrollFix(unittest.TestCase):
    """Owner report (Settings / UX acceptance): the Controls / Keyboard static binding list could not be scrolled."""
    TABS = "Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp"

    def test_rows_receive_mouse_events(self):
        tabs = read(self.TABS)
        make = tabs[tabs.index("GameWindow *makeStatic("):][:900]
        self.assertNotIn("WIN_STATUS_NO_INPUT", make.split("gogoGadgetStaticText")[1].split(";")[0])      # a NO_INPUT top window makes the window manager drop the event

    def test_a_combo_box_passes_the_wheel_to_its_parent(self):
        combo = read("Core/GameEngine/Source/GameClient/GUI/Gadget/GadgetComboBox.cpp")
        wheel = combo[combo.index("case GWM_WHEEL_DOWN:"):][:420]
        self.assertIn("return MSG_IGNORED;", wheel)

    def test_keyboard_and_track_click_scroll(self):
        tabs = read(self.TABS)
        for needle in ("case KEY_DOWN: if (s_layout.scrollable", "case KEY_UP: if (s_layout.scrollable", "bool onClick(int x, int y)", "scrollTo(((y - t.y) * maxFirst + t.h / 2) / t.h);"):
            self.assertIn(needle, tabs)
        self.assertIn("s_trackHit", tabs)                       # an enabled window over the track: the hosting panels are NO_INPUT, a click there would reach no window
        for tree in ("Generals", "GeneralsMD"):
            options = read("%s/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp" % tree)
            self.assertIn("case GWM_LEFT_DOWN:", options)
            self.assertIn("OptionsTabs::onClick(", options)
            self.assertIn("OptionsTabs::onWheel(", options)

    def test_the_layout_root_covers_the_whole_display(self):
        # the retail root is the centred 4:3 area; a top-level window only receives mouse events inside its own rectangle, so the panel margin (the scroll track) was a dead zone on wider displays
        tabs = read(self.TABS)
        self.assertIn("s_root->winSetSize(TheDisplay->getWidth(), TheDisplay->getHeight());", tabs)
        self.assertIn("child->winSetPosition(childX + rootX, childY + rootY);", tabs)       # the direct children keep their screen positions
        self.assertIn('byName("OptionsMenuParentOld")', tabs)                                # the hit window lives under the frame that owns the input callback

    def test_the_native_audit_covers_it(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        for needle in ("the mouse wheel over a row scrolls the page", "the Down arrow scrolls one row", "End scrolls to the last row", "Home scrolls to the first row", "a click on the scroll track scrolls the page"):
            self.assertIn(needle, host)


if __name__ == "__main__":
    unittest.main()
