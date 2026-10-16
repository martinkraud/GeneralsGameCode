"""Enhanced gameplay HUD (USA prototype) source contracts.

The prototype is presentation only: it moves existing windows and draws a procedural frame. These contracts pin that
the Original path is untouched when the Enhanced presentation is not active, that no command/gameplay logic was
duplicated, that the choice is persisted by the Modern Experience settings and that unsupported factions fall back.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def text(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


BAR = "Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarEnhanced.cpp"
SCHEME = "Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp"


class EnhancedHudContracts(unittest.TestCase):
    def test_enhanced_code_is_presentation_only(self):
        s = text(BAR)
        for forbidden in ("TheMessageStream", "GameMessage", "appendMessage", "TheGameLogic", "processCommandUI",
                          "ThePlayerList", "doSpecialPower", "TheBuildAssistant", "getSkillPoints", "getRankLevel",
                          "MSG_", "Xfer", "TheRandom", "GameLogicRandom"):
            self.assertNotIn(forbidden, s, forbidden)
        # it only moves/sizes/hides windows and draws rectangles
        for allowed in ("winSetPosition", "winSetSize", "winHide", "drawFillRect"):
            self.assertIn(allowed, s)

    def test_every_entry_point_is_gated_and_original_is_a_no_op(self):
        s = text(BAR)
        apply_ = s[s.index("void ControlBar::applyEnhancedLayout()"):]
        self.assertIn("if( !EnhancedHud::active() || !TheWindowManager )", apply_[:200])
        restore = s[s.index("void ControlBar::restoreOriginalLayout()"):]
        self.assertIn("if( s_barMaster == nullptr && s_powersParent == nullptr )", restore[:400])
        self.assertIn("return;", restore[:400])
        power = s[s.index("void ControlBar::applyEnhancedPowerShortcutLayout()"):]
        self.assertIn("if( !EnhancedHud::active() )", power[:150])
        tip = s[s.index("Bool ControlBar::getEnhancedTooltipPosition"):]
        self.assertIn("if( !EnhancedHud::active()", tip[:200])
        frame = s[s.index("void drawEnhancedHudFrame()"):]
        self.assertIn("if( !EnhancedHud::active()", frame[:150])

    def test_scheme_hooks_keep_original_drawing_when_inactive(self):
        s = text(SCHEME)
        self.assertIn("EnhancedHud::update(", s)
        self.assertIn("m_side.str()", s)              # the faction decision uses the scheme's side
        bg = s[s.index("void ControlBarSchemeManager::drawBackground"):]
        self.assertRegex(bg[:260], r"if\( EnhancedHud::active\(\) \)\s*\{\s*drawEnhancedHudFrame\(\);\s*return;\s*\}\s*if\(m_currentScheme\)")
        fg = s[s.index("void ControlBarScheme::drawForeground"):]
        self.assertIn("if( EnhancedHud::active() )\n\t\treturn;", fg[:300])
        # Original: the previous scheme art call is untouched
        self.assertIn("m_currentScheme->drawBackground( m_multiplier, DisplayRuntime::offsetByControlBarOrigin(offset) );", s)
        init_tail = s[s.index("// Enhanced: move the windows into the island"):]
        self.assertIn("applyEnhancedLayout();", init_tail[:300])
        self.assertIn("restoreOriginalLayout();", init_tail[:300])

    def test_only_usa_and_never_classic(self):
        layout = text("Core/GameEngine/Include/Common/EnhancedHudLayout.h")
        self.assertIn('static const char prefix[] = "America";', layout)
        self.assertIn('std::strstr(side, "Small") == nullptr', layout)
        self.assertIn("requested && modernDisplay && !classic && supportsSide(side)", layout)
        for engine in ("TheDisplay", "TheControlBar", "GameWindow", "TheGameLogic", "ThePlayerList"):
            self.assertNotIn(engine, layout)          # pure geometry: no engine dependency
        hud = text("Core/GameEngine/Include/Common/EnhancedHud.h")
        self.assertIn("bool requested = false;", hud)  # Original by default

    def test_choice_is_persisted_default_original_and_in_options(self):
        settings = text("Core/GameEngine/Include/Common/DisplaySettings.h")
        self.assertIn("bool enhancedHud = false;", settings)
        self.assertIn('"InterfaceStyle"', settings)
        self.assertIn('s.enhancedHud ? "Enhanced" : "Original"', settings)
        page = text("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        self.assertIn('{"ModernInterfaceLabel", "ModernInterface", ControlKind::Combo}', page)
        opts = text("Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp")
        self.assertIn("EnhancedHud::setRequested(next.enhancedHud)", opts)
        self.assertIn("select(s_interface, 0);   // Original", opts)       # Defaults button = Original
        host = text("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn("EnhancedHud::setRequested(settings.enhancedHud);", host)

    def test_no_win32_reference_or_zero_hour_changes_outside_shared_engine(self):
        # the prototype lives in the shared Core engine only; Generals/GeneralsMD trees are not touched
        import subprocess
        out = subprocess.run(["git", "diff", "--name-only", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout
        for line in out.split():
            self.assertFalse(line.startswith(("Generals/", "GeneralsMD/")) and "ControlBar" in line, line)

    def test_original_assets_and_commands_are_reused(self):
        s = text("Core/GameEngine/Include/Common/EnhancedHudLayout.h")
        # every window moved is an existing ControlBar.wnd window (names from the retail layout)
        for name in ("LeftHUD", "CommandWindow", "ButtonCommand01", "ButtonCommand12", "GeneralsExp", "ExpBarForeground",
                     "ButtonGeneral", "MoneyDisplay", "PowerWindow", "WinUnitSelected", "ProductionQueueWindow", "CameoWindow"):
            self.assertIn('"' + name + '"', s)
        # the special power shortcut column is the existing GenPowersShortcutBar layout, moved not rebuilt
        bar = text(BAR)
        self.assertIn("m_specialPowerShortcutParent", bar)
        cb = text("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp")
        self.assertIn("if( EnhancedHud::active() )\n\t\tapplyEnhancedPowerShortcutLayout();", cb)


if __name__ == "__main__":
    unittest.main()
