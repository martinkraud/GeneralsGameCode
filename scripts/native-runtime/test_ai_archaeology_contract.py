#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""X64.50 contract: the source-level facts the AI audit (docs/modernization/X64_50_AI_ARCHAEOLOGY.md) rests on.

This is NOT a statement that the pinned behaviour is desirable: it pins what the audited tree DOES, so that the audit cannot silently go stale. Any milestone that changes
one of these facts on purpose (a future Original-mode preservation fix or the Enhanced AI) must change the matching test and the audit document in the same commit.
Read-only: it parses source text only.
"""
import os
import re
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GEN = "Generals/Code/GameEngine/Source/GameLogic/AI/"
ZH = "GeneralsMD/Code/GameEngine/Source/GameLogic/AI/"


def read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8", errors="replace") as handle:
        return handle.read().replace("\r\n", "\n")


class AiArchaeologyContract(unittest.TestCase):
    def test_ai01_automatic_build_branch_is_unreachable_in_both_titles(self):
        for tree in (GEN, ZH):
            src = read(tree + "AISkirmishPlayer.cpp")
            body = src[src.index("void AISkirmishPlayer::processBaseBuilding()"):src.index("void AISkirmishPlayer::onUnitProduced")]
            self.assertIn("TheBuildAssistant->canMakeUnit(dozer, bldgPlan)!=CANMAKE_OK", body)       # the accumulated plan, not the entry being examined
            self.assertNotIn("canMakeUnit(dozer, curPlan)", body)
            # bldgPlan is assigned only by the priority branch before that call, and (unconditionally) after it
            assigns = [m.start() for m in re.finditer(r"bldgPlan\s*=\s*curPlan", body)]
            self.assertEqual(len(assigns), 2)
            self.assertLess(assigns[0], body.index("canMakeUnit(dozer, bldgPlan)"))                  # the priority assignment
            self.assertGreater(assigns[1], body.index("canMakeUnit(dozer, bldgPlan)"))               # the automatic assignment is only reachable after a passing check
        # ... and a null template can never pass it
        ba = read("GeneralsMD/Code/GameEngine/Source/Common/System/BuildAssistant.cpp")
        head = ba[ba.index("CanMakeType BuildAssistant::canMakeUnit"):]
        self.assertIn("if( builder == nullptr || whatToBuild == nullptr )\n\t\treturn CANMAKE_NO_PREREQ;", head[:400])

    def test_ai01_priority_flag_is_never_cleared_and_automatic_defaults_to_true(self):
        sides = read("Core/GameEngine/Source/GameLogic/Map/SidesList.cpp")
        self.assertIn("m_automaticallyBuild(true),", sides)
        self.assertIn("m_priorityBuild(false),", sides)
        hdr = read("Core/GameEngine/Include/GameLogic/SidesList.h")
        self.assertIn("If true, the ai will build.  If false, script has to enable this.", hdr)
        self.assertEqual(len(re.findall(r"m_priorityBuild\s*=\s*false", hdr + sides)), 0)

    def test_ai02_gla_hole_entry_leaves_the_outer_building_pointer_stale(self):
        for tree in (GEN, ZH):
            src = read(tree + "AISkirmishPlayer.cpp")
            body = src[src.index("void AISkirmishPlayer::processBaseBuilding()"):src.index("void AISkirmishPlayer::onUnitProduced")]
            self.assertEqual(body.count("Object *bldg = TheGameLogic->findObjectByID( info->getObjectID() );"), 1)    # the shadowing inner declaration
            self.assertIn("bldg = TheGameLogic->findObjectByID( info->getObjectID() );", body)                          # the outer assignment, made before the hole scan
            scan = body[body.index("Scan for a GLA hole"):body.index("if (info->getObjectID()==INVALID_ID && info->getObjectTimestamp()>0)")]
            self.assertNotIn("break;", scan)                                                                            # the scan keeps going after a match
            self.assertNotRegex(scan, r"\n\s*bldg\s*=")                                                                 # the outer pointer is not refreshed

    def test_ai03_adjust_build_list_reads_the_head_template(self):
        for tree in (GEN, ZH):
            src = read(tree + "AISkirmishPlayer.cpp")
            body = src[src.index("void AISkirmishPlayer::adjustBuildList"):src.index("void AISkirmishPlayer::newMap")]
            self.assertIn("findTemplate(list->getTemplateName())", body)
            self.assertIn("cur->setAngle(cur->getAngle());", body)

    def test_ai04_zero_hour_only_superweapon_fine_search_typo(self):
        self.assertIn("pos.y = bestPos.y + (x-5)*(weaponRadius/10);", read(ZH + "AIPlayer.cpp"))
        gen = read(GEN + "AIPlayer.cpp")
        self.assertIn("pos.y = bestPos.y+ (j-5)*(weaponRadius/10);", gen)
        self.assertNotIn("(x-5)", gen)

    def test_ai05_rally_offset_statement_has_no_effect(self):
        for tree in (GEN, ZH):
            self.assertEqual(read(tree + "AIPlayer.cpp").count("\t\t\t\tgotOffset;"), 2)

    def test_ai06_structure_bounds_fallback_is_dead(self):
        for tree in (GEN, ZH):
            src = read(tree + "AIPlayer.cpp")
            body = src[src.index("void AIPlayer::getPlayerStructureBounds"):]
            self.assertIn("if (!firstStructure) {", body)

    def test_ai08_retaliation_bonus_tests_the_wrong_player(self):
        for tree in (GEN, ZH):
            src = read(tree + "AISkirmishPlayer.cpp")
            self.assertIn("if (somePlayer->isSkirmishAIPlayer() && (somePlayer->getCurrentEnemy()==m_player)) {", src)

    def test_ai09_three_second_cap_overrides_the_ini_timers(self):
        src = read(ZH + "AISkirmishPlayer.cpp")
        self.assertEqual(src.count("> 3*LOGICFRAMES_PER_SECOND"), 2)

    def test_ai10_skirmish_xfer_omits_enemy_state(self):
        for tree in (GEN, ZH):
            src = read(tree + "AISkirmishPlayer.cpp")
            body = src[src.index("void AISkirmishPlayer::xfer"):src.index("void AISkirmishPlayer::loadPostProcess")]
            self.assertIn("XferVersion currentVersion = 1;", body)
            self.assertNotIn("m_currentEnemy", body)
            self.assertNotIn("m_frameToCheckEnemy", body)

    def test_ai11_defense_angle_limit_only_checks_the_positive_side(self):
        for tree in (GEN, ZH):
            src = read(tree + "AISkirmishPlayer.cpp")
            self.assertIn("if (angle > PI/3) break;", src)
            self.assertNotIn("fabs(angle)", src)

    def test_ai14_loose_harvester_branch_has_no_rebuild_hole_check(self):
        src = read(ZH + "AIPlayer.cpp")
        body = src[src.index("void AIPlayer::queueSupplyTruck()"):src.index("static void deleteQueue")]
        self.assertEqual(body.count("KINDOF_REBUILD_HOLE"), 1)

    def test_cheats_hunt_and_idle_acquisition_knowledge(self):
        states = read(ZH + "AIStates.cpp")
        self.assertIn("victim = TheAI->findClosestEnemy( owner, 9999.9f, AI::CAN_ATTACK, info );", states)       # the whole map, no UNFOGGED / CAN_SEE
        upd = read("GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp")
        self.assertIn("flags |= AI::UNFOGGED;", upd)                                                              # only inside the PLAYER_HUMAN branch
        idx = upd.index("flags |= AI::UNFOGGED;")
        self.assertIn("getPlayerType() == PLAYER_HUMAN", upd[idx - 250:idx])

    def test_cheats_pathfinder_and_difficulty_bonuses(self):
        pf = read("Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp")
        self.assertGreaterEqual(pf.count("computer gets to cheat."), 7)
        player = read("GeneralsMD/Code/GameEngine/Source/Common/RTS/Player.cpp")
        body = player[player.index("void Player::friend_applyDifficultyBonusesForObject"):]
        self.assertIn("if (TheGameLogic->isInSinglePlayerGame())", body[:300])
        mode = read("Core/GameEngine/Include/GameLogic/GameMode.h")
        self.assertIn("return mode == GAME_SINGLE_PLAYER;", mode)
        logic = read("GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp")
        self.assertIn("case SLOT_BRUTAL_AI : d.setInt(TheKey_skirmishDifficulty, DIFFICULTY_HARD); break;", logic)

    def test_determinism_boundary(self):
        defs = read("Core/GameEngine/Include/Common/GameDefines.h")
        self.assertIn("#define RETAIL_COMPATIBLE_CRC (1)", defs)
        ai = read(ZH + "AIPlayer.cpp")
        self.assertIn("GameLogicRandomValue( 0, count-1 )", ai)          # team choice among the highest priority
        self.assertIn("m_skillsetSelector = GameLogicRandomValue(0, limit);", ai)
        for tree in (GEN, ZH):
            for name in ("AIPlayer.cpp", "AISkirmishPlayer.cpp", "AIStates.cpp"):
                self.assertNotRegex(read(tree + name), r"[^A-Za-z_]rand\(\)|GetTickCount|timeGetTime|std::random")


if __name__ == "__main__":
    unittest.main()
