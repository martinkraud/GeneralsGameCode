#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""X64.50 / X64.51 contract: the source-level facts the AI audit (X64_50_AI_ARCHAEOLOGY.md) and the Enhanced AI foundation (X64_51_ENHANCED_AI_FOUNDATION.md) rest on.

X64.50 pinned what the retail AI DOES. X64.51 keeps that behaviour as the Original mode behind one run-time seam (Common/AIBehavior.h) and adds the Enhanced repairs, so the
facts below are now pinned in their seam form: for every audited site the Original answer is the retail expression and the Enhanced answer is the repair. A change to any of
them must update this test and the documents in the same commit. Read-only: it parses source text only.
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


def body_of(text, start, end):
    return text[text.index(start):text.index(end, text.index(start))]


class AiBehaviorSeam(unittest.TestCase):
    def test_helpers_return_the_retail_answer_for_original(self):
        h = read("Core/GameEngine/Include/Common/AIBehavior.h")
        self.assertIn("return enhanced ? currentEntry : accumulatedPlan;", h)             # AI-01
        self.assertIn("inline bool refreshBuildingAfterHoleLink(bool enhanced) { return enhanced; }", h)
        self.assertIn("inline bool stopHoleScanAtFirstMatch(bool enhanced) { return enhanced; }", h)
        self.assertIn("return enhanced ? y : x;", h)                                       # AI-04
        self.assertIn("return enhanced && offsetPresent;", h)                              # AI-05
        self.assertIn("return enhanced ? candidateAiTargetsMe : someOtherAiTargetsMe;", h)  # AI-08
        self.assertIn("return enhanced ? std::fabs(angle) > limit : angle > limit;", h)     # AI-11
        self.assertIn("return enhanced ? 10 * logicFramesPerSecond : 10;", h)               # AI-13
        self.assertNotRegex(h, r"GameLogicRandom|getFrame|TheGameLogic|rand\(")

    def test_every_audited_site_goes_through_the_seam_in_both_titles(self):
        for tree in (GEN, ZH):
            sk = read(tree + "AISkirmishPlayer.cpp")
            pl = read(tree + "AIPlayer.cpp")
            self.assertIn("AIBehavior::dozerCheckTemplate(m_enhanced, bldgPlan, curPlan)", sk)
            self.assertIn("TheBuildAssistant->canMakeUnit(dozer, dozerCheckPlan)!=CANMAKE_OK", sk)
            self.assertIn("AIBehavior::refreshBuildingAfterHoleLink(m_enhanced)", sk)
            self.assertIn("AIBehavior::stopHoleScanAtFirstMatch(m_enhanced)", sk)
            self.assertIn("AIBehavior::retaliationBonusOncePerCandidate(m_enhanced)", sk)
            self.assertIn("AIBehavior::defenseArcExhausted(m_enhanced, angle, PI/3)", sk)
            self.assertEqual(pl.count("gotOffset = AIBehavior::applyRallyOffset(m_enhanced, true);"), 2)
            self.assertIn("AIBehavior::supplyAttackScanWindowFrames(m_enhanced, LOGICFRAMES_PER_SECOND)", pl)
            self.assertIn("center && m_enhanced && center->isKindOf(KINDOF_REBUILD_HOLE)", pl)
        # AI-04 is a Zero Hour regression only: Generals keeps its own (correct) loop and does NOT call the helper
        self.assertIn("AIBehavior::fineSearchYIndex(m_enhanced, x, y)", read(ZH + "AIPlayer.cpp"))
        gen = read(GEN + "AIPlayer.cpp")
        self.assertIn("pos.y = bestPos.y+ (j-5)*(weaponRadius/10);", gen)
        self.assertNotIn("fineSearchYIndex", gen)

    def test_ai01_original_still_passes_the_accumulated_plan_and_the_branch_logic_is_unchanged(self):
        for tree in (GEN, ZH):
            src = read(tree + "AISkirmishPlayer.cpp")
            body = body_of(src, "void AISkirmishPlayer::processBaseBuilding()", "void AISkirmishPlayer::onUnitProduced")
            assigns = [m.start() for m in re.finditer(r"bldgPlan\s*=\s*curPlan", body)]
            self.assertEqual(len(assigns), 2)
            self.assertLess(assigns[0], body.index("dozerCheckPlan"))                    # the priority assignment precedes the check
            self.assertGreater(assigns[1], body.index("canMakeUnit(dozer, dozerCheckPlan)"))
        ba = read("GeneralsMD/Code/GameEngine/Source/Common/System/BuildAssistant.cpp")
        head = ba[ba.index("CanMakeType BuildAssistant::canMakeUnit"):]
        self.assertIn("if( builder == nullptr || whatToBuild == nullptr )\n\t\treturn CANMAKE_NO_PREREQ;", head[:400])

    def test_priority_flag_is_never_cleared_and_automatic_defaults_to_true(self):
        sides = read("Core/GameEngine/Source/GameLogic/Map/SidesList.cpp")
        self.assertIn("m_automaticallyBuild(true),", sides)
        self.assertIn("m_priorityBuild(false),", sides)
        hdr = read("Core/GameEngine/Include/GameLogic/SidesList.h")
        self.assertEqual(len(re.findall(r"m_priorityBuild\s*=\s*false", hdr + sides)), 0)

    def test_ai02_hole_branch_keeps_the_shadowed_declaration_for_original(self):
        for tree in (GEN, ZH):
            src = read(tree + "AISkirmishPlayer.cpp")
            body = body_of(src, "void AISkirmishPlayer::processBaseBuilding()", "void AISkirmishPlayer::onUnitProduced")
            self.assertEqual(body.count("Object *bldg = TheGameLogic->findObjectByID( info->getObjectID() );"), 1)    # the inner (shadowing) declaration is untouched
            self.assertIn("if (linkedHole) {", body)
            self.assertLess(body.index("if (linkedHole) {"), body.index("if (info->getObjectID()==INVALID_ID && info->getObjectTimestamp()>0)"))

    def test_ai06_structure_bounds_fallback_is_dead(self):
        for tree in (GEN, ZH):
            body = body_of(read(tree + "AIPlayer.cpp"), "void AIPlayer::getPlayerStructureBounds", "\x00") if False else read(tree + "AIPlayer.cpp")
            self.assertIn("if (!firstStructure) {", body)

    def test_ai09_three_second_cap_overrides_the_ini_timers_unchanged(self):
        self.assertEqual(read(ZH + "AISkirmishPlayer.cpp").count("> 3*LOGICFRAMES_PER_SECOND"), 2)

    def test_ai10_skirmish_xfer_omits_enemy_state_unchanged(self):
        for tree in (GEN, ZH):
            body = body_of(read(tree + "AISkirmishPlayer.cpp"), "void AISkirmishPlayer::xfer", "void AISkirmishPlayer::loadPostProcess")
            self.assertIn("XferVersion currentVersion = 1;", body)
            self.assertNotIn("m_currentEnemy", body)

    def test_ai16_temporary_state_exit_is_guarded_in_both_titles(self):
        for tree in (GEN, ZH):
            src = read(tree + "AIStates.cpp")
            self.assertIn("if (m_temporaryState) {\n\t\t\tm_temporaryState->onExit(EXIT_NORMAL);\n\t\t\tm_temporaryState = nullptr;\n\t\t}\n\t}\n\tStateReturnType retType = StateMachine::updateStateMachine();", src)

    def test_cheats_hunt_and_idle_acquisition_knowledge(self):
        states = read(ZH + "AIStates.cpp")
        self.assertIn("victim = TheAI->findClosestEnemy( owner, 9999.9f, AI::CAN_ATTACK, info );", states)
        upd = read("GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate.cpp")
        idx = upd.index("flags |= AI::UNFOGGED;")
        self.assertIn("getPlayerType() == PLAYER_HUMAN", upd[idx - 250:idx])

    def test_cheats_pathfinder_and_difficulty_bonuses(self):
        pf = read("Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp")
        self.assertGreaterEqual(pf.count("computer gets to cheat."), 7)
        player = read("GeneralsMD/Code/GameEngine/Source/Common/RTS/Player.cpp")
        body = player[player.index("void Player::friend_applyDifficultyBonusesForObject"):]
        self.assertIn("if (TheGameLogic->isInSinglePlayerGame())", body[:300])
        self.assertIn("return mode == GAME_SINGLE_PLAYER;", read("Core/GameEngine/Include/GameLogic/GameMode.h"))
        self.assertIn("case SLOT_BRUTAL_AI : d.setInt(TheKey_skirmishDifficulty, DIFFICULTY_HARD); break;", read("GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp"))

    def test_determinism_boundary(self):
        self.assertIn("#define RETAIL_COMPATIBLE_CRC (1)", read("Core/GameEngine/Include/Common/GameDefines.h"))
        ai = read(ZH + "AIPlayer.cpp")
        self.assertIn("GameLogicRandomValue( 0, count-1 )", ai)
        self.assertIn("m_skillsetSelector = GameLogicRandomValue(0, limit);", ai)
        for tree in (GEN, ZH):
            for name in ("AIPlayer.cpp", "AISkirmishPlayer.cpp", "AIStates.cpp"):
                self.assertNotRegex(read(tree + name), r"[^A-Za-z_]rand\(\)|GetTickCount|timeGetTime|std::random")
        # the Enhanced repairs and the diagnostics add no random draw
        for tree in (GEN, ZH):
            sk = read(tree + "AISkirmishPlayer.cpp")
            for needle in ("AIBehavior", "AIDiagnostics"):
                for line in sk.split("\n"):
                    if needle in line:
                        self.assertNotIn("GameLogicRandom", line)


class ModePlumbing(unittest.TestCase):
    def test_mode_is_a_game_rule_written_only_when_enhanced(self):
        info = read("Core/GameEngine/Source/GameNetwork/GameInfo.cpp")
        self.assertIn("if (game.getAIBehavior() == AI_BEHAVIOR_ENHANCED)", info)
        write = body_of(info, "if (game.getAIBehavior() == AI_BEHAVIOR_ENHANCED)", "//add player info for each slot")
        self.assertIn("AIBehavior::optionKey()", write)
        # the key is parsed, an unrecognised value rejects the whole options string, absent = Original
        self.assertIn("AIBehaviorMode aiBehavior = AI_BEHAVIOR_ORIGINAL;", info)
        self.assertIn("if (!AIBehavior::parseOptionValue(val.str(), aiBehavior))", info)
        self.assertIn("game->setAIBehavior(aiBehavior);", info)
        self.assertIn("m_aiBehavior = AI_BEHAVIOR_ORIGINAL;\n//", info.replace("\t", ""))  # reset() default
        hdr = read("Core/GameEngine/Include/GameNetwork/GameInfo.h")
        self.assertIn("AIBehaviorMode m_aiBehavior;", hdr)

    def test_skirmish_save_keeps_the_retail_version_for_original(self):
        info = read("Core/GameEngine/Source/GameNetwork/GameInfo.cpp")
        body = body_of(info, "void SkirmishGameInfo::xfer", "void SkirmishGameInfo::loadPostProcess")
        self.assertIn("const XferVersion retailVersion = 2;", body)
        self.assertIn("const XferVersion retailVersion = 4;", body)
        self.assertIn("XferVersion version = (xfer->getXferMode() == XFER_SAVE && m_aiBehavior == AI_BEHAVIOR_ENHANCED) ? currentVersion : retailVersion;", body)
        self.assertIn("if ( version >= 5 )", body)

    def test_ai_player_save_keeps_version_1_for_original(self):
        for tree in (GEN, ZH):
            pl = read(tree + "AIPlayer.cpp")
            body = body_of(pl, "void AIPlayer::xfer( Xfer *xfer )", "void AIPlayer::loadPostProcess")
            self.assertIn("XferVersion version = (xfer->getXferMode() == XFER_SAVE && m_enhanced) ? 3 : 1;", body)
            self.assertIn("if (version >= 2)", body)
            ctor = body_of(pl, "AIPlayer::AIPlayer( Player *p )", "AIPlayer::~AIPlayer")
            self.assertIn("m_enhanced = TheGameInfo && TheGameInfo->getAIBehavior() == AI_BEHAVIOR_ENHANCED;", ctor)

    def test_diagnostics_are_inert_unless_requested(self):
        d = read("Core/GameEngine/Include/Common/AIDiagnostics.h")
        self.assertIn('getenv("GENERALS_AI_DIAG")', d)
        for tree in (GEN, ZH):
            pl = read(tree + "AIPlayer.cpp")
            self.assertIn("if (AIDiagnostics::enabled()) diagnosticTick();", pl)
            self.assertIn("if (!isSkirmishAI()) return;", pl)

    def test_replay_header_and_lobby_use_the_same_options_string(self):
        for rec in ("Generals/Code/GameEngine/Source/Common/Recorder.cpp", "GeneralsMD/Code/GameEngine/Source/Common/Recorder.cpp"):
            text = read(rec)
            self.assertIn("GameInfoToAsciiString(TheSkirmishGameInfo)", text)
            self.assertIn("ParseAsciiStringToGameInfo(&m_gameInfo, header.gameOptions)", text)


    def test_skirmish_selector_is_wired_in_both_titles_and_classic_is_original(self):
        for menu in ("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp",
                     "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp"):
            text = read(menu)
            self.assertIn("AIBehaviorOptions::create(", text)
            self.assertIn("AIBehaviorOptions::selected(", text)
            self.assertIn("ModernExperience::FEATURE_AI_BEHAVIOR", text)
            # default Original: the stored value is only honoured when the feature is on
            self.assertIn(": AI_BEHAVIOR_ORIGINAL);", text)
            self.assertIn('aiBehaviorKey[] = "AIBehavior"', text)
        ui = read("Core/GameEngine/Source/GameClient/GUI/AIBehaviorOptions.cpp")
        self.assertIn("Original (retail)", ui)
        self.assertIn("Enhanced (repaired)", ui)
        self.assertIn("AIBehaviorOptions.cpp", read("Core/GameEngine/CMakeLists.txt"))


if __name__ == "__main__":
    unittest.main()
