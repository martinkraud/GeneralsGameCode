#!/usr/bin/env python3
"""X64.52A source contract: the Skirmish "Superweapons" match rule.

Static text checks (no engine). The dynamic proofs are the headless scenarios of Invoke-AiBaseExperiments.ps1 (-Sw, -SwAid) and the golden Original gate.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TREES = ("Generals", "GeneralsMD")


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def tree_file(tree, rest):
    return read("%s/Code/GameEngine/%s" % (tree, rest))


class Identification(unittest.TestCase):
    def test_superweapons_are_identified_from_metadata(self):
        cpp = read("Core/GameEngine/Source/Common/Thing/ThingTemplate.cpp")
        self.assertIn("Bool ThingTemplate::isSuperweaponStructure() const", cpp)
        self.assertIn("if (m_maxSimultaneousDeterminedBySuperweaponRestriction)", cpp)          # the Zero Hour data marker (incl. the Scud Storm rebuild holes)
        self.assertIn("isKindOf(KINDOF_STRUCTURE)", cpp)                                       # General Powers are not structures
        self.assertIn("data->isSpecialPowerModuleData()", cpp)
        for power in ("SPECIAL_PARTICLE_UPLINK_CANNON", "SPECIAL_NEUTRON_MISSILE", "SPECIAL_SCUD_STORM"):
            self.assertIn("case %s:" % power, cpp)
        for power in ("SUPW_SPECIAL_PARTICLE_UPLINK_CANNON", "LAZR_SPECIAL_PARTICLE_UPLINK_CANNON", "NUKE_SPECIAL_NEUTRON_MISSILE", "SUPW_SPECIAL_NEUTRON_MISSILE"):
            self.assertIn("case %s:" % power, cpp)
        # the title-specific enumerators are compiled only where they exist
        self.assertIn("#if !RTS_GENERALS", cpp)
        # no fragile object-name lists
        self.assertNotIn("ParticleCannonUplink", cpp)
        self.assertNotIn("ChinaNuclearMissileLauncher", cpp)

    def test_every_tree_marks_its_special_power_module_data(self):
        for tree in TREES:
            self.assertIn("virtual Bool isSpecialPowerModuleData() const { return false; }", tree_file(tree, "Include/Common/Module.h"))
            self.assertIn("virtual Bool isSpecialPowerModuleData() const override { return true; }", tree_file(tree, "Include/GameLogic/Module/SpecialPowerModule.h"))


class Enforcement(unittest.TestCase):
    def test_counting_is_exact_and_not_a_lifetime_quota(self):
        cpp = read("Core/GameEngine/Source/GameLogic/System/SuperweaponRule.cpp")
        self.assertIn("if (obj->isEffectivelyDead())", cpp)                  # a destroyed structure frees its capacity
        self.assertIn("KINDOF_REBUILD_HOLE", cpp)                            # a standing hole holds the capacity of the structure it rebuilds
        self.assertIn("player->iterateObjects(countSuperweapon", cpp)        # owned objects, incl. under construction and captured ones
        self.assertNotIn("destroyObject", cpp)                               # capture never destroys anything
        self.assertNotIn("kill(", cpp)
        self.assertIn("if (!SuperweaponLimit::isActive(limit) || !what->isSuperweaponStructure())", cpp)   # Unlimited: nothing checked

    def test_human_construction_goes_through_the_authoritative_check(self):
        self.assertIn("SuperweaponRule::blocksStart( this, whatToBuild )", tree_file("GeneralsMD", "Source/Common/RTS/Player.cpp"))
        gen = tree_file("Generals", "Source/Common/System/BuildAssistant.cpp")
        self.assertIn("SuperweaponRule::blocksStart( player, whatToBuild )", gen)
        self.assertLess(gen.index("SuperweaponRule::blocksStart"), gen.index("isPossibleToMakeUnit(builder, whatToBuild)"))
        for tree in TREES:
            dozer = tree_file(tree, "Source/GameLogic/Object/Update/AIUpdate/DozerAIUpdate.cpp")
            self.assertIn("if( TheBuildAssistant->canMakeUnit( getObject(), what ) != CANMAKE_OK)", dozer)   # the human path re-checks on the simulation side

    def test_ai_obeys_the_rule_in_both_modes_without_blocking_its_queue(self):
        for tree in TREES:
            skirmish = tree_file(tree, "Source/GameLogic/AI/AISkirmishPlayer.cpp")
            self.assertIn("if (SuperweaponRule::blocksStart(m_player, curPlan)) {\n\t\t\t\tAIDiagnostics::count(\"sw.order_skipped_at_limit\");\n\t\t\t\tcontinue;\n\t\t\t}", skirmish)
            # not behind m_enhanced: Original AI obeys the match rule too
            window = skirmish[skirmish.index("SuperweaponRule::blocksStart(m_player, curPlan)") - 120:skirmish.index("SuperweaponRule::blocksStart(m_player, curPlan)")]
            self.assertNotIn("m_enhanced", window)
            self.assertIn("if (SuperweaponRule::blocksStart(m_player, bldgPlan)) {", tree_file(tree, "Source/GameLogic/AI/AIPlayer.cpp"))

    def test_campaign_and_challenge_keep_their_rules(self):
        for tree in TREES:
            logic = tree_file(tree, "Source/GameLogic/System/GameLogic.cpp")
            self.assertIn("return TheGameInfo ? TheGameInfo->getSuperweaponLimit() : 0;", logic)
        # General's Challenge game info is never given a rule: nothing in the Challenge / Campaign code writes the limit
        for path in ("Core/GameEngine/Source/Common/System/ChallengeGameInfo.cpp",):
            f = ROOT / path
            if f.exists():
                self.assertNotIn("setSuperweaponLimit", f.read_text(encoding="utf-8", errors="replace"))


class MatchState(unittest.TestCase):
    def test_default_writes_nothing_and_unknown_values_are_refused(self):
        info = read("Core/GameEngine/Source/GameNetwork/GameInfo.cpp")
        self.assertIn("if (SuperweaponLimit::isActive(game.getSuperweaponLimit()))", info)
        self.assertIn("if (!SuperweaponLimit::parseOptionValue(val.str(), superweaponLimit))", info)
        self.assertIn("optionsOk = FALSE;", info[info.index("parseOptionValue(val.str(), superweaponLimit)"):][:300])
        self.assertIn("game->setSuperweaponLimit(superweaponLimit);", info)

    def test_save_version_6_only_when_not_default(self):
        info = read("Core/GameEngine/Source/GameNetwork/GameInfo.cpp")
        self.assertIn("const XferVersion currentVersion = 6;", info)
        self.assertIn("SuperweaponLimit::isActive(m_superweaponLimit))) ? currentVersion : retailVersion;", info)
        self.assertIn("SuperweaponLimit::xferLimit( xfer, m_superweaponLimit, version );", info)
        self.assertIn("if (version >= 6)", read("Core/GameEngine/Include/Common/SuperweaponLimitXfer.h"))

    def test_registered_in_the_build(self):
        cmake = read("Core/GameEngine/CMakeLists.txt")
        for entry in ("Include/Common/SuperweaponLimit.h", "Include/GameLogic/SuperweaponRule.h", "Source/GameLogic/System/SuperweaponRule.cpp"):
            self.assertIn(entry, cmake)
        self.assertIn("GameEngine/Common/SuperweaponLimitTest.cpp", read("Tests/Google/Core/CMakeLists.txt"))

    def test_no_balance_change(self):
        for path in ("Core/GameEngine/Source/GameLogic/System/SuperweaponRule.cpp", "Core/GameEngine/Include/Common/SuperweaponLimit.h"):
            text = re.sub(r"//[^\n]*", "", read(path))
            for forbidden in ("deposit(", "withdraw(", "setHealth(", "BuildCost", "calcCostToBuild", "setBuildTime"):
                self.assertNotIn(forbidden, text)


if __name__ == "__main__":
    unittest.main()
