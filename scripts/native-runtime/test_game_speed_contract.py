#!/usr/bin/env python3
"""Game Speed source contract: the Skirmish "Game Speed" match rule is PACING only.

Static text checks (no engine). The dynamic proofs are the headless digests at every level (identical), the graphical replay runs (pace = 30 x percent logic frames per second at 60 / 120 / 240
FPS, fingerprints equal to headless at every checkpoint) and the unit tests (GameSpeedTest.cpp). See docs/modernization/GAME_SPEED.md.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TREES = ("Generals", "GeneralsMD")


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


class Representation(unittest.TestCase):
    def test_one_authoritative_value_in_the_game_info(self):
        info = read("Core/GameEngine/Include/GameNetwork/GameInfo.h")
        self.assertIn("UnsignedShort m_gameSpeed;", info)
        self.assertIn("GameSpeed::sanitize( percent )", info)
        cpp = read("Core/GameEngine/Source/GameNetwork/GameInfo.cpp")
        self.assertIn("m_gameSpeed = GameSpeed::kNormal;", cpp)
        self.assertIn("game->setGameSpeed(gameSpeed);", cpp)

    def test_options_string_written_only_when_not_normal_and_unknown_values_rejected(self):
        cpp = read("Core/GameEngine/Source/GameNetwork/GameInfo.cpp")
        self.assertIn("if (GameSpeed::isActive(game.getGameSpeed()))", cpp)
        self.assertIn("if (!GameSpeed::parseOptionValue(val.str(), gameSpeed))", cpp)
        self.assertIn("optionsOk = FALSE;", cpp[cpp.index("parseOptionValue(val.str(), gameSpeed)"):][:300])

    def test_save_version_7_carries_the_rule(self):
        cpp = read("Core/GameEngine/Source/GameNetwork/GameInfo.cpp")
        self.assertIn("const XferVersion currentVersion = 7;", cpp)
        self.assertIn("GameSpeed::isActive(m_gameSpeed)", cpp)
        self.assertIn("GameSpeed::xferPercent( xfer, m_gameSpeed, version );", cpp)
        self.assertIn("if (version >= 7)", read("Core/GameEngine/Include/Common/GameSpeedXfer.h"))

    def test_pure_header_is_engine_free(self):
        header = read("Core/GameEngine/Include/Common/GameSpeed.h")
        for forbidden in ("FramePacer", "TheGameLogic", "TheNetwork", "#include \"Common/Xfer", "Windows.h"):
            self.assertNotIn(forbidden, header)


class Scheduler(unittest.TestCase):
    def test_game_speed_never_touches_the_render_limit(self):
        for path in ("Core/GameEngine/Source/Common/FramePacer.cpp",):
            text = read(path)
            body = text[text.index("void FramePacer::setGameSpeedPercent"):][:200]
            self.assertNotIn("setFramesPerSecondLimit", body)
            self.assertNotIn("m_maxFPS", body)
        xlat = read("Core/GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp")
        speed = xlat[xlat.index("static bool changeGameSpeed"):xlat.index("bool changeLogicTimeScale")]
        self.assertNotIn("setFramesPerSecondLimit", speed)
        self.assertNotIn("setLogicTimeScaleFps", speed)

    def test_normal_is_bit_identical(self):
        text = read("Core/GameEngine/Source/Common/FramePacer.cpp")
        self.assertIn("m_gameSpeedPercent = 100;", text)
        self.assertIn("(Real)getLogicTimeScaleFps() * ((Real)m_gameSpeedPercent / 100.0f)", text)

    def test_network_games_ignore_the_rule(self):
        text = read("Core/GameEngine/Source/Common/FramePacer.cpp")
        exact = text[text.index("Real FramePacer::getActualLogicTimeScaleFpsExact"):][:900]
        self.assertLess(exact.index("TheNetwork != nullptr"), exact.index("m_gameSpeedPercent"))
        for tree in TREES:
            logic = read("%s/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp" % tree)
            getter = logic[logic.index("UnsignedShort GameLogic::getGameSpeed() const"):][:260]
            self.assertIn("TheNetwork != nullptr", getter)
            self.assertIn("!TheGameInfo", getter)

    def test_both_titles_apply_the_rule_each_frame_and_read_the_exact_rate(self):
        for tree in TREES:
            engine = read("%s/Code/GameEngine/Source/Common/GameEngine.cpp" % tree)
            self.assertIn("TheFramePacer->setGameSpeedPercent(TheGameLogic->getGameSpeed());", engine)
            self.assertIn("const Real logicTimeScaleFps = TheFramePacer->getActualLogicTimeScaleFpsExact(logicTimeQueryFlags);", engine)
            self.assertIn("UnsignedShort getGameSpeed() const;", read("%s/Code/GameEngine/Include/GameLogic/GameLogic.h" % tree))

    def test_no_game_speed_symbol_in_the_simulation(self):
        # pacing only: nothing under GameLogic/ (other than the single getter) may read the rule
        for tree in TREES:
            for path in (ROOT / tree / "Code/GameEngine/Source/GameLogic").rglob("*.cpp"):
                text = path.read_text(encoding="utf-8", errors="replace")
                if path.name == "GameLogic.cpp":
                    start = text.index("UnsignedShort GameLogic::getGameSpeed() const")
                    text = text[:start] + text[text.index("\n}\n", start):]
                self.assertNotIn("getGameSpeed", text, str(path))
        for path in (ROOT / "Core/GameEngine/Source/GameLogic").rglob("*.cpp"):
            self.assertNotIn("getGameSpeed", path.read_text(encoding="utf-8", errors="replace"), str(path))


class Selector(unittest.TestCase):
    def test_classic_never_offers_it(self):
        feature = read("Core/GameEngine/Include/Common/ModernExperience.h")
        self.assertIn("FEATURE_GAME_SPEED", feature)
        for tree in TREES:
            menu = read("%s/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp" % tree)
            self.assertIn("ModernExperience::enabled( ModernExperience::FEATURE_GAME_SPEED ) && DisplayRuntime::modern()", menu)
            self.assertIn("setGameSpeed( gameSpeedSelectorOffered() ? storedGameSpeed( prefs ) : GameSpeed::kNormal )".replace("( ", "(").replace(" )", ")"),
                          menu.replace("( ", "(").replace(" )", ")"))
            self.assertIn("ComboBoxGameSpeed", menu)

    def test_rule_is_a_generated_spec_not_special_layout(self):
        cpp = read("Core/GameEngine/Source/GameClient/GUI/ModernMatchSetup.cpp")
        self.assertIn('{ "GameSpeed", "GUI:GameSpeed", L"Game Speed", "ComboBoxGameSpeed", nullptr }', cpp)
        layout = read("Core/GameEngine/Include/Common/ModernMatchSetupLayout.h")
        self.assertNotIn("GameSpeed", layout)

    def test_runtime_controls_are_skirmish_only_and_discrete(self):
        xlat = read("Core/GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp")
        speed = xlat[xlat.index("static bool changeGameSpeed"):xlat.index("bool changeLogicTimeScale")]
        for needle in ("TheNetwork != nullptr", "isInSkirmishGame()", "isInReplayGame()", "FEATURE_GAME_SPEED", "GameSpeed::step("):
            self.assertIn(needle, speed)
        self.assertIn("if (changeGameSpeed(change))", xlat)


class Tooling(unittest.TestCase):
    def test_replay_generator_writes_the_key_only_for_non_normal(self):
        gen = read("scripts/performance/New-SimulationBenchmarkReplay.py")
        self.assertIn('behavior += "GS=%d;" % args.game_speed', gen)
        self.assertIn("args.game_speed != 100", gen)


if __name__ == "__main__":
    unittest.main()
