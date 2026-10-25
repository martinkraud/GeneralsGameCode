#!/usr/bin/env python3
"""UI.1: the Classic / Modern contract and the rules of the Modern UI foundation (static; the behavioural proofs are the gtests ModernUiLayout.* and the native audits).

Classic (`-classic`) is the preservation path: every Modern UI entry point must be gated by `ModernExperience` / `DisplayRuntime::modern()`, and the shared
layout core must stay a pure, allocation-free, engine-free header so it can never leak engine state into Classic.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


class FoundationIsPure(unittest.TestCase):
    def test_layout_core_has_no_engine_dependency_and_no_heap(self):
        h = read("Core/GameEngine/Include/Common/ModernUiLayout.h")
        code = re.sub(r"//[^\n]*", "", h)
        includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', code)
        self.assertEqual(sorted(includes), ["cstdint", "type_traits"])
        for forbidden in ("std::vector", "std::string", "std::map", "malloc", "new ", "delete ", "static ", "TheDisplay", "GameWindow", "DisplayRuntime"):
            self.assertNotIn(forbidden, code.replace("static_assert", ""), forbidden)
        self.assertIn("static_assert(std::is_trivially_copyable<Node>", code)

    def test_gtest_registered_with_the_resolution_matrix(self):
        cmake = read("Tests/Google/Core/CMakeLists.txt")
        self.assertIn("GameEngine/Common/ModernUiLayoutTest.cpp", cmake)
        t = read("Tests/Google/Core/GameEngine/Common/ModernUiLayoutTest.cpp")
        for surface in ("1920, 1080", "2560, 1440", "3440, 1440", "3840, 2160", "5120, 1440"):
            self.assertIn(surface, t)
        for scale in ("75", "100", "125", "150", "200", "300"):
            self.assertRegex(t, r"kScales\[\] = \{[^}]*\b%s\b" % scale)
        for name in ("MatchRulesPanelHasNoDefectAtTheResolutionAndScaleMatrix", "UltrawideKeepsTheContentWidthInsteadOfStretching",
                     "AddingARuleNeverMovesTheRulesBeforeIt", "SolvingAFullMatchPanelIsCheap"):
            self.assertIn(name, t)


class ClassicContract(unittest.TestCase):
    def test_classic_flag_comes_from_the_command_line_only(self):
        self.assertIn("ModernExperience::setClassic(true);", read("Core/GameEngine/Source/Common/CommandLine.cpp"))
        me = read("Core/GameEngine/Include/Common/ModernExperience.h")
        self.assertIn("inline bool enabled(Feature feature) { return feature >= 0 && feature < FEATURE_COUNT && !classicFlag(); }", me)

    def test_every_modern_match_option_is_gated_by_its_feature_in_both_titles(self):
        for tree in ("Generals", "GeneralsMD"):
            menu = read("%s/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp" % tree)
            for feature in ("FEATURE_AI_BEHAVIOR", "FEATURE_SUPERWEAPON_LIMIT"):
                self.assertIn("ModernExperience::enabled(ModernExperience::%s)" % feature, menu, (tree, feature))
            # the stored value is honoured only when the feature is on: Classic is always the retail value
            self.assertIn("? storedAIBehavior(prefs) : AI_BEHAVIOR_ORIGINAL", menu)

    def test_modern_options_and_hud_are_gated(self):
        self.assertIn("if (!DisplayRuntime::modern() || ModernExperience::classic())", read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp"))
        self.assertIn("ModernExperience::classic()", read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp"))
        eh = read("Core/GameEngine/Include/Common/EnhancedHud.h")
        self.assertIn("bool classic", eh)
        script = read("Core/GameEngine/Source/GameClient/GUI/GameWindowManagerScript.cpp")
        self.assertIn("if( DisplayRuntime::modern() && createRes.x > 0 && createRes.y > 0 )", script)

    def test_the_retail_wnd_scaling_remains_the_else_branch(self):
        script = read("Core/GameEngine/Source/GameClient/GUI/GameWindowManagerScript.cpp")
        i = script.index("if( DisplayRuntime::modern() && createRes.x > 0")
        self.assertIn("Real xScale = (Real)TheDisplay->getWidth() / (Real)createRes.x;", script[i:i + 3500])


MENUS = ("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp", "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp")


class ModernMainMenuShell(unittest.TestCase):
    """UI.2: the Modern main menu shell must stay isolated from Classic and wired to the retail behaviour."""

    def test_pure_layers_are_engine_free(self):
        allowed = ("cstdint", "type_traits", "Common/ModernFocus.h", "Common/ModernTheme.h", "Common/ModernUiLayout.h")
        for name in ("ModernFocus.h", "ModernTheme.h", "ModernMainMenuModel.h"):
            code = re.sub(r"//[^\n]*", "", read("Core/GameEngine/Include/Common/" + name))
            for inc in re.findall(r'#include\s+[<"]([^>"]+)[>"]', code):
                self.assertIn(inc, allowed, (name, inc))
            for forbidden in ("GameWindow", "TheDisplay", "TheWindowManager", "std::vector", "malloc", "DisplayRuntime"):
                self.assertNotIn(forbidden, code, (name, forbidden))

    def test_shell_is_registered_and_gated_by_a_feature(self):
        self.assertIn("Source/GameClient/GUI/ModernMenuShell.cpp", read("Core/GameEngine/CMakeLists.txt"))
        self.assertIn("FEATURE_MODERN_FRONTEND", read("Core/GameEngine/Include/Common/ModernExperience.h"))
        cpp = read("Core/GameEngine/Source/GameClient/GUI/ModernMenuShell.cpp")
        self.assertIn("DisplayRuntime::modern() && ModernExperience::enabled(ModernExperience::FEATURE_MODERN_FRONTEND)", cpp)
        self.assertIn("GENERALS_MODERN_FRONTEND", cpp)

    def test_every_shell_call_in_both_titles_is_behind_the_gate(self):
        pattern = re.compile(r"ModernMenuShell::(init|reveal|update|onKey|onSelected|onHover)\(")
        for path in MENUS:
            lines = read(path).split("\n")
            calls = [i for i, l in enumerate(lines) if pattern.search(l)]
            self.assertGreaterEqual(len(calls), 6, path)
            for i in calls:
                window = "\n".join(lines[max(0, i - 8):i + 1])
                self.assertTrue(re.search(r"ModernMenuShell::(active|built)\(\)|modernMenu|revealMainMenu", window), (path, i + 1, lines[i].strip()))

    def test_retail_flow_is_untouched_behind_the_else_branches(self):
        guard = "if(buttonPushed)\n\t\t\t\tbreak;"
        for path in MENUS:
            text = read(path)
            self.assertIn('TheTransitionHandler->setGroup("MainMenuDefaultMenu");', text)
            reveal = text[text.index("static void revealMainMenu()"):][:700]
            self.assertIn("else", reveal)
            self.assertIn(guard, text)
            self.assertLess(text.index("ModernMenuShell::onSelected( control )"), text.index(guard))

    def test_title_specific_actions(self):
        gen, zh = (read(p) for p in MENUS)
        self.assertIn("launchChallengeMenu = TRUE;", zh)
        self.assertIn("ModernTheme::Title::ZeroHour", zh)
        self.assertNotIn("launchChallengeMenu", gen)
        self.assertIn("ModernTheme::Title::Generals", gen)
        for text in (gen, zh):
            for action in ("Skirmish", "Network", "Online", "LoadGame", "LoadReplay", "Credits", "Options", "Exit"):
                self.assertIn("case Action::%s:" % action, text, action)
            self.assertIn('"Menus/SkirmishGameOptionsMenu.wnd"', text)
            self.assertIn("QuitMessageBoxYesNo(", text[text.index("static Bool modernPerform"):])

    def test_shell_creates_only_its_own_windows_and_is_idempotent(self):
        cpp = read("Core/GameEngine/Source/GameClient/GUI/ModernMenuShell.cpp")
        self.assertNotIn("winDestroy", cpp)
        self.assertIn("ModernDraw::createBackdrop(g.parent,", cpp)      # finds its band by name before creating it
        self.assertIn("TheWindowManager->winGetWindowFromId(g.panel, ", cpp)   # buttons and logo likewise
        self.assertNotIn("copyAppearance", cpp)                          # no retail button art is borrowed any more

    def test_audit_tooling_and_automation_hook(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn('"mainmenu"', host)
        self.assertIn("ModernMenuShell::performRetail(button)", host)
        self.assertIn("classic frontend: the Modern shell was not instantiated", host)
        self.assertIn("$Classic", read("scripts/native-runtime/Invoke-UiAudit.ps1"))
        self.assertIn("MainMenu", read("scripts/native-runtime/Invoke-NativeAutoCapture.ps1"))


class Documentation(unittest.TestCase):
    def test_modern_ui_documents_exist_and_cover_the_required_sections(self):
        arch = read("docs/modernization/MODERN_UI_ARCHITECTURE.md")
        for heading in ("Classic / Modern contract", "Information architecture", "Match Setup", "Animated background", "Modern HUD", "Test strategy",
                        "Performance policy", "Phase plan", "UI.2"):
            self.assertIn(heading, arch, heading)
        arch_doc = read("docs/modernization/MODERN_UI_ARCHAEOLOGY.md")
        for heading in ("800x600", "WND", "Shell", "Gadget", "Zero Hour", "Bink"):
            self.assertIn(heading, arch_doc, heading)
        self.assertIn("MODERN_UI_ARCHITECTURE.md", read("docs/modernization/AGENT_HANDOFF.md"))
        status = read("docs/modernization/PROJECT_STATUS.md")
        for part in ("Completed", "In progress", "Planned", "Known issues"):
            self.assertIn(part, status, part)

    def test_readme_does_not_claim_unfinished_work(self):
        readme = read("README.md")
        self.assertIn("MKRaudstein", readme)
        self.assertIn("PROJECT_STATUS.md", readme)
        for claim in ("Modern UI is complete", "animated menu background is implemented"):
            self.assertNotIn(claim, readme)


if __name__ == "__main__":
    unittest.main()
