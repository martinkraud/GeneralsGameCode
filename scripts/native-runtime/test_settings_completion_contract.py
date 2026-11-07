#!/usr/bin/env python3
"""Settings / UX completion source contract: Accept / Cancel semantics, generated Network rows, the read-only Keyboard reference, per-setting help.

Static text checks (no engine). The dynamic proofs are the native Options audit (every category in both titles, 4:3 - 32:9, the Cancel-restores-the-anchor transaction step) and the unit tests
(SettingHelpTest, HotkeyTextTest, ModernSettingsModelTest, ModernScreenLayoutTest). See docs/modernization/MODERN_SETTINGS_ARCHITECTURE.md.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TREES = ("Generals", "GeneralsMD")
OPTIONS = "%s/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp"
TABS = "Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


class AcceptCancel(unittest.TestCase):
    def test_the_commit_button_is_called_accept_and_the_hint_is_true(self):
        tabs = read(TABS)
        self.assertIn('FETCH_OR_SUBSTITUTE("GUI:Accept", L"Accept")', tabs)
        self.assertNotIn('L"Apply"', tabs)
        self.assertIn("Accept saves your changes and closes Options; Cancel discards them.", tabs)
        self.assertNotIn("apply as you move them", tabs)       # volume and scroll speed apply on Accept (retail has no live preview)

    def test_cancel_restores_what_applies_at_the_click(self):
        for tree in TREES:
            text = read(OPTIONS % tree)
            leave = text[text.index("static void leaveWithoutSaving()"):][:200]
            self.assertIn("restoreOpenState();", leave)
            restore = text[text.index("static void restoreOpenState()"):][:600]
            self.assertIn("if (OptionsTabs::activeCategory() < 0) return;", restore)       # Classic keeps the retail behaviour
            for needle in ("setDrawRMBScrollAnchor", "setMoveRMBScrollAnchor", "m_useCameraInReplay", "m_saveCameraInReplay"):
                self.assertIn(needle, restore)
            self.assertLess(text.index("OptionsTabs::create();"), text.index("rememberOpenState();", text.index("OptionsTabs::create();")))


class Network(unittest.TestCase):
    def test_network_is_a_generated_page_not_a_kept_block(self):
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        self.assertIn("constexpr PageItem kNetworkItems[]", layout)
        for name in ("ComboBoxIP", "ComboBoxOnlineIP", "TextEntryFirewallPortOverride", "ButtonFirewallRefresh"):
            self.assertIn('"%s"' % name, layout)
        tabs = read(TABS)
        self.assertNotIn("inNetwork", tabs)
        self.assertNotIn("the retail Network block is kept whole", tabs)
        self.assertIn('"NetworkParent" };', tabs)                # hosted like the other panels

    def test_no_networking_behaviour_changed(self):
        for tree in TREES:
            text = read(OPTIONS % tree)
            self.assertIn("setLANIPAddress", text)
            self.assertIn("FirewallPortOverride", text)


class Keyboard(unittest.TestCase):
    def test_keyboard_page_is_a_read_only_reference(self):
        tabs = read(TABS)
        self.assertIn("buildKeyboardRows", tabs)
        self.assertIn("TheMetaMap->getFirstMetaMapRec()", tabs)
        for write in ("->m_key =", "->m_modState =", "->m_transition =", "->m_usableIn ="):
            self.assertNotIn(write, tabs)                         # the retail engine has no rebinding; Modern does not pretend
        self.assertIn("CATEGORY_DEBUG", read("Core/GameEngine/Include/GameClient/MetaEvent.h"))
        self.assertNotIn("CATEGORY_DEBUG", tabs)                  # debug commands are not listed
        self.assertIn("Keyboard, Gameplay", read("Core/GameEngine/Include/Common/OptionsPageLayout.h"))

    def test_classic_never_gets_the_page(self):
        self.assertIn("if (!DisplayRuntime::modern() || ModernExperience::classic())", read(TABS))


class Help(unittest.TestCase):
    def test_help_strip_exists_in_the_layout_and_the_screen(self):
        layout = read("Core/GameEngine/Include/Common/ModernScreenLayout.h")
        self.assertIn("Rect help;", layout)
        tabs = read(TABS)
        self.assertIn("SettingHelp::find(", tabs)
        self.assertIn("help.wrap = L.help.w;", tabs)
        self.assertIn("WIN_STATE_HILITED", tabs)                  # hover
        self.assertIn("winGetFocus()", tabs)                      # keyboard focus

    def test_match_rules_are_not_global_settings(self):
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        for name in ("GameSpeed", "AIBehavior", "Superweapons", "StartingCash"):
            self.assertNotIn(name, layout)


if __name__ == "__main__":
    unittest.main()
