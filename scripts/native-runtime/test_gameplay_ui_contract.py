#!/usr/bin/env python3
"""Modern Experience gameplay UI polish: source / documentation contracts (the geometry rules are covered by the GameplayTextLayout / EnhancedHudOverview gtests, the native result by the audit evidence)."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


TEXT = "Core/GameEngine/Include/Common/GameplayTextLayout.h"
UI = ["Generals/Code/GameEngine/Source/GameClient/InGameUI.cpp", "GeneralsMD/Code/GameEngine/Source/GameClient/InGameUI.cpp"]
BAR = "Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarEnhanced.cpp"
DOC = "docs/modernization/GAMEPLAY_UI_AUDIT.md"
CLASSES = ("Modernized", "Acceptable retail", "Needs polish", "Needs responsive fix", "Functional defect", "Classic-only", "Out of scope")


class GameplayUiContracts(unittest.TestCase):
    def test_the_text_rules_are_pure(self):
        text = read(TEXT)
        for include in re.findall(r"#include\s+[<\"]([^>\"]+)", text):
            self.assertIn(include, ("algorithm", "cmath"), include)       # no engine header: the rules stay unit testable

    def test_messages_are_modern_only_and_classic_keeps_the_retail_line(self):
        for path in UI:
            text = read(path)
            body = text[text.index("void InGameUI::addMessageText("):]
            body = body[:body.index("\n}\n")]
            self.assertIn("DisplayRuntime::modern() && !ModernExperience::classic()", body, path)
            self.assertIn("GameplayTextLayout::pointSize(", body, path)
            self.assertIn("GameplayTextLayout::wrapWidth(", body, path)
            self.assertIn(": TheGlobalLanguageData->adjustFontSize(m_messagePointSize);", body, path)      # the Classic size is the retail expression
            post = text[text.index("void InGameUI::postDraw()"):]
            self.assertIn("else if (GameFont *font = m_uiMessages[ i ].displayString->getFont())", post[:2200], path)   # Classic keeps the one-line advance

    def test_the_superweapon_list_is_lifted_only_with_the_enhanced_hud(self):
        for path in UI:
            text = read(path)
            block = text[text.index("if( EnhancedHud::active() )\n\t\t{\n\t\t\tconst Int hudTop"):][:600]
            self.assertIn("GameplayTextLayout::superweaponMargin(", block, path)
            self.assertIn("GameplayTextLayout::superweaponStartY(", block, path)

    def test_the_battle_overview_scales_by_height_and_restores_the_retail_geometry(self):
        bar = read(BAR)
        draw = bar[bar.index("void overviewDraw("):][:900]
        self.assertIn("const double u = r.h / (double)EnhancedHudPanels::kOverviewH;", draw)     # 424 wide in Generals, 596 in Zero Hour: the height is the same
        self.assertIn("ts.w = r.w - 2 * ts.x;", bar)
        attach = bar[bar.index("void EnhancedHudOverview::attach("):]
        self.assertIn("overviewFitAboveHud(", attach)
        forget = bar[bar.index("void EnhancedHudOverview::forget("):][:400]
        self.assertIn("restoreOverviewGeometry();", forget)

    def test_the_general_powers_popup_is_placed_by_the_safe_area_rule(self):
        bar = read(BAR)
        self.assertIn("popupPlacementClear( surfaceW, surfaceH, layout, ui )", bar)
        self.assertNotIn("popupPlacement( surfaceW, surfaceH, layout.infoTop, ui )", bar)       # the old rule let a tall popup fall back onto the HUD
        panels = read("Core/GameEngine/Include/Common/EnhancedHudPanels.h")
        self.assertIn("inline PopupPlacement popupPlacementClear(", panels)
        self.assertIn("shellSegments(l, seg)", panels)

    def test_the_selection_ring_is_presentation_only_and_classic_keeps_the_retail_ring(self):
        header = read("Core/GameEngine/Include/Common/SelectionRingStyle.h")
        self.assertEqual([l for l in header.splitlines() if l.startswith("#include")], ["#include <algorithm>"])   # pure: no engine, no simulation
        view = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp")
        self.assertIn("DisplayRuntime::modern() && !ModernExperience::classic()", view[view.index("Selection rings."):])
        self.assertIn("SelectionRing::styleFor(modernRing, SelectionRing::preference()", view)
        self.assertIn("ring.width", view)
        # the ring never takes part in picking, selection, commands, network, replays or saves
        for path in ("Core/GameEngine/Source/GameClient/MessageStream/SelectionXlat.cpp", "Core/GameEngine/Source/GameLogic/System/GameLogicDispatch.cpp"):
            self.assertNotIn("SelectionRing", read(path), path)
        # the preference is a Modern display setting beside Interface: one source of truth, stored in Options.ini only
        settings = read("Core/GameEngine/Include/Common/DisplaySettings.h")
        self.assertIn("bool selectionRing = true;", settings)
        self.assertIn('"SelectionRing"', settings)
        for forbidden in ("Xfer", "GameMessage", "RecorderClass"):
            self.assertNotIn(forbidden, header)
        opts = read("Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp")
        self.assertIn("select(s_ring, 1);        // Selection Ring: On", opts)                  # Defaults restore On
        self.assertIn("SelectionRing::setPreference(requested.selectionRing);", opts)
        self.assertIn("OptionsMenu.wnd:ModernSelectionRing", opts)
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn("SelectionRing::setPreference(settings.selectionRing);", host)


    def test_the_probe_flow_is_a_harness_and_the_capture_script_knows_it(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn('"gameui"', host)
        self.assertIn("GAMEUI_PROBE", host)
        self.assertIn("'GameUi'", read("scripts/native-runtime/Invoke-NativeAutoCapture.ps1"))

    def test_the_audit_inventory_classifies_every_required_surface(self):
        doc = read(DOC)
        rows = [line for line in doc.splitlines() if line.startswith("| ") and any(c in line for c in CLASSES)]
        self.assertGreaterEqual(len(rows), 30)
        text = "\n".join(rows).lower()
        for surface in ("command tooltip", "build queue", "selection", "multi-selection", "veterancy", "general powers", "global powers", "battle overview", "hide / show", "pause",
                        "game speed", "objective", "countdown", "message", "money", "subtitle", "victory", "score", "replay", "observer", "beacon", "chat", "disconnect",
                        "diplomacy", "confirmation", "campaign", "quit menu", "floating"):
            self.assertIn(surface, text, surface)


if __name__ == "__main__":
    unittest.main()
