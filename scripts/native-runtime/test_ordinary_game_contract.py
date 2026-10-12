"""X64.24 ordinary-game source contracts; never executes a title."""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "365f42ffc"  # X64.23 implementation + doc commit (accepted)
HOST = "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp"


def read(path):
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")


def show(path):
    return subprocess.check_output(["git", "show", f"{BASELINE}:{path}"], cwd=ROOT).decode("latin1").replace("\r\n", "\n")


def body(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:i + 1]
    raise AssertionError("unbalanced " + signature)


def code_only(source):
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', source)


class OrdinaryGameContract(unittest.TestCase):
    def test_game_loop_is_the_ordinary_engine_loop_not_a_private_one(self):
        loop = code_only(body(read(HOST), "int W3DDisplay::runOrdinaryGameHost()"))
        for required in ("TheGameEngine->update()", "TheFramePacer->update()", "TheGameLogic->exitGame()",
                         "setPushGuard(GamePushGuard)", "kLogicFrameBound", "kTotalMs"):
            self.assertIn(required, loop)
        # No private re-implementation of client/logic stepping, no execute/replay/save entry.
        for forbidden in ("->execute(", "GameLogic->UPDATE", "TheGameClient->UPDATE", "propagateMessages",
                          "simulateReplays", "loadQueuedSaveGame", "TheNetwork", "m_shellMapOn", "m_headless ="):
            self.assertNotIn(forbidden, loop)

    def test_push_allow_list_is_the_audited_skirmish_path_only(self):
        guard = body(read(HOST), "Bool GamePushGuard(")
        self.assertIn('"Menus/MainMenu.wnd"', guard)
        self.assertIn('"Menus/SkirmishGameOptionsMenu.wnd"', guard)
        self.assertIn("return FALSE;", guard)
        self.assertEqual(guard.count("compareNoCase"), 2)

    def test_menu_and_gate_paths_are_untouched(self):
        for signature in ("int W3DDisplay::runOrdinaryMenuHost()", "int W3DDisplay::runOrdinaryHostGate()",
                          "void W3DDisplay::drawOrdinaryHostFrame()", "Bool MenuPushGuard("):
            self.assertEqual(body(read(HOST), signature), body(show(HOST), signature), signature)

    def test_selection_and_world_factories_unchanged(self):
        self.assertEqual(read("Generals/Code/GameEngine/Source/Common/GameEngine.cpp"),
                         show("Generals/Code/GameEngine/Source/Common/GameEngine.cpp"))
        s = read("Generals/Code/GameEngine/Source/GameClient/GameClient.cpp")
        self.assertEqual(s, show("Generals/Code/GameEngine/Source/GameClient/GameClient.cpp"))
        header = read("Core/GameEngine/Include/Common/StartupDiagnostics.h")
        self.assertIn("s.ordinaryHost = s.ordinaryMenu = s.ordinaryGame = true", header)
        self.assertIn('validOrdinaryArguments(argc, argv, "-nativeOrdinaryGame")', header)

    def test_main_selects_game_before_menu_and_zero_hour_untouched(self):
        main = read("Generals/Code/GameEngine/Source/Common/GameMain.cpp")
        self.assertLess(main.index("runOrdinaryGameHost()"), main.index("runOrdinaryMenuHost()"))
        self.assertLess(main.index("runOrdinaryMenuHost()"), main.index("runOrdinaryHostGate()"))
        zh = read("GeneralsMD/Code/GameEngine/Source/Common/GameMain.cpp") + read("GeneralsMD/Code/Main/WinMain.cpp")
        self.assertNotIn("ordinaryGame", zh)

    def test_launcher_requires_live_logic_evidence(self):
        common = read("scripts/native-runtime/OrdinaryHostCommon.ps1")
        for term in ("-nativeOrdinaryGame", "Fewer than 150 logic frames in game", "Shell push boundary reached",
                     "Game did not start with at least two players", "exit=(frame_bound|game_quit)"):
            self.assertIn(term, common)
        launch = read("scripts/native-runtime/Invoke-NativeOrdinaryGame.ps1")
        for term in ("$exit -eq 0", "$shutdownBytes -eq 0", "$dumps.Count -eq 0", "$evidence.Valid", "$pathsMatch",
                     "-RunReviewedOrdinaryGame", "do not rerun automatically"):
            self.assertIn(term, launch)


if __name__ == "__main__":
    unittest.main()
