#!/usr/bin/env python3
"""UI.3: the Modern Frontend system (static contract; the behavioural proofs are the gtests ModernComponents.* / ModernScreenLayout.* / ModernMatchSetup.* / ModernDialogLayout.* / ModernBrowserLayout.*
and the native audit flows driven by Invoke-UiAudit.ps1).

What must stay true:
  * Classic (`-classic`) never builds a Modern screen: every entry point is gated, and the pure layout headers stay engine-free;
  * the design system does not borrow retail button art and does not name the retail "Generals" font family (it is not installed);
  * every migrated screen is hooked in BOTH titles, and a screen that is not migrated is listed in the milestone document;
  * the Match Rules are generated from a spec list; the audit flows exist for every migrated screen.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TITLES = ("Generals", "GeneralsMD")
MENUS = "Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def code(text):
    return re.sub(r"//[^\n]*", "", text)


class PureHeaders(unittest.TestCase):
    PURE = ("Core/GameEngine/Include/Common/ModernTheme.h", "Core/GameEngine/Include/Common/ModernComponents.h", "Core/GameEngine/Include/Common/ModernScreenLayout.h",
            "Core/GameEngine/Include/Common/ModernMatchSetupLayout.h")

    def test_headers_are_engine_free(self):
        allowed = {"cstdint", "type_traits", "cmath", "Common/ModernTheme.h", "Common/ModernScreenLayout.h"}
        for path in self.PURE:
            includes = set(re.findall(r'#include\s+[<"]([^>"]+)[>"]', code(read(path))))
            self.assertLessEqual(includes, allowed, path)
            text = code(read(path))
            for forbidden in ("GameWindow", "TheDisplay", "DisplayRuntime", "std::vector", "std::string", "malloc"):
                self.assertNotIn(forbidden, text, "%s uses %s" % (path, forbidden))

    def test_gtests_are_registered_and_cover_the_matrix(self):
        cmake = read("Tests/Google/Core/CMakeLists.txt")
        for test in ("ModernComponentsTest.cpp", "ModernScreenLayoutTest.cpp", "ModernMatchSetupLayoutTest.cpp"):
            self.assertIn("GameEngine/Common/" + test, cmake)
        for test in ("ModernScreenLayoutTest.cpp", "ModernMatchSetupLayoutTest.cpp"):
            t = read("Tests/Google/Core/GameEngine/Common/" + test)
            for surface in ("1920, 1080", "2560, 1440", "3440, 1440", "3840, 2160", "5120, 1440"):
                self.assertIn(surface, t)
            self.assertRegex(t, r"kScales\[\] = \{ 75, 100, 150, 200, 300 \}")


class DesignSystem(unittest.TestCase):
    def test_typography_names_only_installed_families(self):
        theme = read("Core/GameEngine/Include/Common/ModernTheme.h")
        families = set()
        for triple in re.findall(r'\{ \{ "([^"]+)", "([^"]+)", "([^"]+)" \}', code(theme)):
            families.update(triple)
        self.assertNotIn("Generals", families)          # the retail family is not installed anywhere
        self.assertIn("Arial", families)                # the last candidate is a font every Windows has
        self.assertIn("Bahnschrift", families)

    def test_no_retail_button_art_is_borrowed(self):
        for path in ("Core/GameEngine/Source/GameClient/GUI/ModernDraw.cpp", "Core/GameEngine/Source/GameClient/GUI/ModernMenuShell.cpp", "Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp",
                     "Core/GameEngine/Source/GameClient/GUI/ModernMatchSetup.cpp", "Core/GameEngine/Source/GameClient/GUI/ModernBrowser.cpp", "Core/GameEngine/Source/GameClient/GUI/ModernDialog.cpp"):
            text = code(read(path))
            self.assertNotIn("copyAppearance", text, path)
            self.assertNotIn("winSetEnabledImage(0, image", text.replace("g.logo->winSetEnabledImage(0, image);", ""), path)   # only the title logo is an image

    def test_one_component_style_table_drives_every_screen(self):
        draw = read("Core/GameEngine/Source/GameClient/GUI/ModernDraw.cpp")
        self.assertIn("ModernComponents::style(", draw)
        comps = read("Core/GameEngine/Include/Common/ModernComponents.h")
        for kind in ("Nav", "NavMinor", "NavDanger", "Primary", "Secondary", "Danger", "Tab", "Field", "Row"):
            self.assertRegex(comps, r"\b%s\b," % kind if kind != "Row" else r"\bRow\b")
        for state in ("Normal", "Hover", "Focus", "Pressed", "Disabled", "Selected"):
            self.assertIn(state, comps)


class Gating(unittest.TestCase):
    def test_every_screen_entry_point_is_gated(self):
        gates = {
            "Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp": "!DisplayRuntime::modern() || ModernExperience::classic()",
            "Core/GameEngine/Source/GameClient/GUI/ModernMatchSetup.cpp": "!DisplayRuntime::modern() || ModernExperience::classic()",
            "Core/GameEngine/Source/GameClient/GUI/ModernBrowser.cpp": "!DisplayRuntime::modern() || ModernExperience::classic()",
            "Core/GameEngine/Source/GameClient/GUI/ModernDialog.cpp": "!active()",
            "Core/GameEngine/Source/GameClient/GUI/ModernCredits.cpp": "!ModernMenuShell::active()",
        }
        for path, gate in gates.items():
            self.assertIn(gate, read(path), path)
        shell = read("Core/GameEngine/Source/GameClient/GUI/ModernMenuShell.cpp")
        self.assertIn("ModernExperience::enabled(ModernExperience::FEATURE_MODERN_FRONTEND)", shell)
        self.assertIn("GENERALS_MODERN_FRONTEND", shell)

    def test_audit_flows_check_that_classic_builds_nothing(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        for marker in ("classic frontend: no Modern Options backdrop exists", "classic frontend: no Modern Match Setup backdrop exists", "classic frontend: no Modern browser backdrop exists",
                       "classic frontend: the Modern shell was not instantiated"):
            self.assertIn(marker, host)
        self.assertIn("band == ModernMenuShell::active()", host)    # the credits band exists exactly when the Modern shell does


class Hooks(unittest.TestCase):
    def test_every_migrated_screen_is_hooked_in_both_titles(self):
        for title in TITLES:
            options = read("%s/%sOptionsMenu.cpp" % (title, MENUS))
            for call in ("OptionsTabs::requestLeave( leaveWithoutSaving )", "OptionsTabs::update()", "OptionsTabs::onKey(", "OptionsTabs::onWheel("):
                self.assertIn(call, options, "%s %s" % (title, call))
            skirmish = read("%s/%sSkirmishGameOptionsMenu.cpp" % (title, MENUS))
            for call in ("ModernMatchSetupScreen::create(", "ModernMatchSetupScreen::update()", "ModernMatchSetupScreen::forget()"):
                self.assertIn(call, skirmish, "%s %s" % (title, call))
            replay = read("%s/%sReplayMenu.cpp" % (title, MENUS))
            saveload = read("%s/%sPopupSaveLoad.cpp" % (title, MENUS))
            self.assertIn("ModernBrowser::create( ModernBrowser::replaySpec() )", replay)
            self.assertEqual(saveload.count("ModernBrowser::create("), 2)       # the popup and the full-screen Save / Load
            for text in (replay, saveload):
                self.assertIn("ModernBrowser::update()", text)
                self.assertIn("ModernBrowser::forget()", text)
            credits = read("%s/%sCreditsMenu.cpp" % (title, MENUS))
            for call in ("ModernCredits::attach(", "ModernCredits::update()", "ModernCredits::forget()"):
                self.assertIn(call, credits, "%s %s" % (title, call))
        self.assertIn("ModernDialog::apply(", read("Core/GameEngine/Source/GameClient/GUI/GameWindowManager.cpp"))

    def test_the_discard_confirmation_only_asks_when_something_changed(self):
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        self.assertRegex(tabs, r"if \(s_active < 0 \|\| !computeDirty\(\)\)\s*return true;")


class RuleSpecs(unittest.TestCase):
    def test_match_rules_are_a_spec_list(self):
        cpp = read("Core/GameEngine/Source/GameClient/GUI/ModernMatchSetup.cpp")
        specs = re.search(r"kRules\[\] = \{(.*?)\n\};", cpp, re.S).group(1)
        for rule in ("StartingCash", "AIBehavior", "Superweapons"):
            self.assertIn('"%s"' % rule, specs)
        # the layout takes only a count: no rule is named in the pure geometry
        layout = read("Core/GameEngine/Include/Common/ModernMatchSetupLayout.h")
        for name in ("StartingCash", "AIBehavior", "Superweapons"):
            self.assertNotIn(name, layout)

    def test_options_pages_come_from_the_page_tables(self):
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        self.assertIn("pageItems(", tabs)
        tables = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        for category in ("Display", "Graphics", "Audio", "Controls", "Gameplay", "Modern", "Network"):
            self.assertRegex(tables, r"\b%s\b" % category)


class AuditTooling(unittest.TestCase):
    def test_audit_flows_exist_and_the_capture_script_accepts_them(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        for flow in ("mainmenu", "options", "matchsetup", "replay", "loadgame", "credits"):
            self.assertIn('"%s"' % flow, host, flow)
        capture = read("scripts/native-runtime/Invoke-NativeAutoCapture.ps1")
        for flow in ("MainMenu", "Options", "MatchSetup", "Replay", "LoadGame", "Credits"):
            self.assertIn("'%s'" % flow, capture, flow)
        self.assertIn("cost", host)

    def test_matrix_script_covers_the_required_surfaces_and_scales(self):
        script = read("scripts/native-runtime/Invoke-FrontendMatrix.ps1")
        for surface in ("1920x1080", "2560x1440", "3440x1440", "3840x2160", "5120x1440"):
            self.assertIn(surface, script)
        for scale in ("75", "100", "150", "200", "300"):
            self.assertRegex(script, r"\b%s\b" % scale)
        for flow in ("MainMenu", "Options", "MatchSetup", "Replay", "LoadGame"):
            self.assertIn(flow, script)


if __name__ == "__main__":
    unittest.main()
