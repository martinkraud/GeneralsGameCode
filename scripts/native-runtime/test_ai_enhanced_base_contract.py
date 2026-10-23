#!/usr/bin/env python3
"""X64.52 source contract: Enhanced AI Dynamic Base & Defence is Enhanced-only, deterministic and saved; Original is not touched.

Static text checks over the sources (no engine needed). The dynamic proof of the same properties is the golden gate (Invoke-AiBehaviorGate.ps1) and the experiments
(Invoke-AiBaseExperiments.ps1); see docs/modernization/X64_52_ENHANCED_AI_DYNAMIC_BASE.md.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TREES = ("Generals", "GeneralsMD")


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


HEADER = "Core/GameEngine/Include/Common/AIEnhancedBase.h"
XFER_HEADER = "Core/GameEngine/Include/Common/AIEnhancedBaseXfer.h"
IMPL = "Core/GameEngine/Source/GameLogic/AI/AIEnhancedBase.cpp"


def ai(tree, name):
    return read("%s/Code/GameEngine/Source/GameLogic/AI/%s" % (tree, name))


class Determinism(unittest.TestCase):
    FORBIDDEN = re.compile(r"\b(rand|srand|GetTickCount|timeGetTime|clock|time|QueryPerformanceCounter|random_device|mt19937|uniform_\w+|GameLogicRandomValue\w*|GameClientRandomValue\w*)\s*\(|std::chrono|<random>|<ctime>|<chrono>")

    def test_no_clock_or_rng_in_the_enhanced_base_code(self):
        for path in (HEADER, XFER_HEADER, IMPL):
            text = re.sub(r"//[^\n]*", "", read(path))   # comments may mention the words
            self.assertIsNone(self.FORBIDDEN.search(text), "%s uses a clock or an RNG" % path)

    def test_decisions_come_from_the_match_seed(self):
        header = read(HEADER)
        self.assertIn("uint32_t playerSeed(uint32_t matchSeed, int playerIndex)", header)
        for tree in TREES:
            text = ai(tree, "AIPlayer.cpp")
            self.assertIn("AIEnhancedBase::playerSeed((uint32_t)TheGameInfo->getSeed(), m_player->getPlayerIndex())", text)

    def test_pure_header_has_no_engine_includes(self):
        includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', read(HEADER))
        self.assertEqual(sorted(includes), ["cmath", "cstdint"])


class OriginalIsNotTouched(unittest.TestCase):
    def test_every_entry_point_is_behind_the_enhanced_flag(self):
        for tree in TREES:
            player = ai(tree, "AIPlayer.cpp")
            self.assertIn("if (m_enhanced) enhancedBaseUpdate();", player)
            self.assertIn("if (m_enhanced) m_eb.seed", player.replace("\tif (m_enhanced) m_eb.seed", "if (m_enhanced) m_eb.seed"))
            skirmish = ai(tree, "AISkirmishPlayer.cpp")
            self.assertIn("if (m_enhanced && foundInBuildList) {\n\t\tangle += enhancedLayoutOffset(", skirmish)
            self.assertIn("if (m_enhanced) {\n\t\tenhancedJitterSlots(list);", skirmish)
            self.assertIn("if (m_enhanced && !flank) {\n\t\t\tReal threatAngle;", skirmish)
            self.assertIn("if (m_enhanced) selector += ", skirmish)

    def test_the_diagnostic_aids_are_inert_without_the_environment(self):
        text = read("Core/GameEngine/Include/Common/AIDiagnostics.h")
        self.assertIn("if (!enabled()) return false;\n\tstatic const std::string off", text)
        for tree in TREES:
            self.assertIn("diagnosticAttackAid();", ai(tree, "AIPlayer.cpp"))

    def test_xfer_adds_state_only_for_enhanced(self):
        for tree in TREES:
            text = ai(tree, "AIPlayer.cpp")
            self.assertIn("XferVersion currentVersion = 3;", text)
            self.assertIn("(xfer->getXferMode() == XFER_SAVE && m_enhanced) ? 3 : 1;", text)
            self.assertIn("if (version >= 3)\n\t{\n\t\tenhancedXferState( xfer );", text)

    def test_every_new_decision_function_checks_its_caps(self):
        impl = read(IMPL)
        self.assertIn("m_eb.threatDefenses >= kThreatDefenseCap", impl)
        self.assertIn("m_eb.expansions >= kExpansionCap || m_eb.expansionFailures >= kExpansionFailureCap", impl)
        self.assertIn("recoveryAllowed(", impl)
        self.assertIn("if (frame < kFillInStart", impl)


class Wiring(unittest.TestCase):
    def test_sources_are_registered(self):
        cmake = read("Core/GameEngine/CMakeLists.txt")
        for entry in ("Include/Common/AIEnhancedBase.h", "Include/Common/AIEnhancedBaseXfer.h", "Source/GameLogic/AI/AIEnhancedBase.cpp"):
            self.assertIn(entry, cmake)
        self.assertIn("GameEngine/Common/AIEnhancedBaseTest.cpp", read("Tests/Google/Core/CMakeLists.txt"))

    def test_both_titles_declare_the_same_members(self):
        for tree in TREES:
            header = read("%s/Code/GameEngine/Include/GameLogic/AIPlayer.h" % tree)
            for member in ("void enhancedBaseUpdate();", "Bool enhancedThreatDirection(Real *angle) const;", "AIEnhancedBase::State m_eb;", "void diagnosticAttackAid();"):
                self.assertIn(member, header)
            self.assertIn("Real enhancedLayoutOffset(", read("%s/Code/GameEngine/Include/GameLogic/AISkirmishPlayer.h" % tree))

    def test_no_balance_or_data_change(self):
        # the Enhanced code may call these read-only; it must never write money, health, cost or build time
        impl = re.sub(r"//[^\n]*", "", read(IMPL))
        for forbidden in ("->deposit(", "->withdraw(", "setHealth(", "setInitialHealth", "m_buildTime", "setBuildTime", "calcTimeToBuild"):
            self.assertNotIn(forbidden, impl)


if __name__ == "__main__":
    unittest.main()
