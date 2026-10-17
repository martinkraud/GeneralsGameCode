"""X64.23 ordinary-menu source contracts; never executes a title."""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "47bd08345"  # accepted X64.22 (ordinary-lifetime presentation host)


def read(path):
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")


def show(path):
    return subprocess.check_output(["git", "show", f"{BASELINE}:{path}"], cwd=ROOT).decode("latin1").replace("\r\n", "\n")


def expected_engine(baseline_source):
    """The accepted engine source with the single X64.29 particle-manager selection change applied."""
    old = "createParticleSystemManager(TheGlobalData->m_headless), nullptr);"
    new = ('createParticleSystemManager(TheGlobalData->m_headless && '
           '!(StartupDiagnostics::ordinaryPlay() && StartupDiagnostics::nativeFeature("particles"))), nullptr);')
    assert baseline_source.count(old) == 1
    import sys, pathlib
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
    import x64_33_deltas, x64_44_deltas
    return x64_44_deltas.with_endurance_hooks(x64_33_deltas.with_modern_caption_hunk(baseline_source.replace(old, new)))


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


HOST = "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp"


class OrdinaryMenuContract(unittest.TestCase):
    def test_real_ui_factories_only_under_menu_profile(self):
        s = read("Generals/Code/GameEngine/Source/GameClient/GameClient.cpp")
        self.assertIn("if (!TheGlobalData->m_headless || StartupDiagnostics::ordinaryMenu())", s)
        self.assertIn("(TheGlobalData->m_headless && !StartupDiagnostics::ordinaryMenu()) ? NEW MouseDummy : createMouse()", s)
        self.assertIn("(TheGlobalData->m_headless && !StartupDiagnostics::ordinaryMenu()) ? NEW GameWindowManagerDummy : createWindowManager()", s)
        # World/audio/radar/particle selection and m_headless are untouched.
        engine = read("Generals/Code/GameEngine/Source/Common/GameEngine.cpp")
        self.assertEqual(engine, expected_engine(show("Generals/Code/GameEngine/Source/Common/GameEngine.cpp")))
        for path in ("Generals/Code/GameEngine/Source/GameClient/GameClient.cpp", HOST,
                     "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp"):
            self.assertNotIn("m_headless = FALSE", read(path))

    def test_menu_loop_is_ui_only_and_bounded(self):
        host = read(HOST)
        loop = code_only(body(host, "int W3DDisplay::runOrdinaryMenuHost()"))
        for forbidden in ("execute(", "GameClient::update", "TheGameClient->UPDATE", "TheGameLogic", "propagateMessages",
                          "TheScriptEngine", "m_shellMapOn", "GameEngine::update", "TheNetwork", "WW3D::Render"):
            self.assertNotIn(forbidden, loop)
        # Only the allowed per-frame calls drive the UI.
        for required in ("TheGameEngine->serviceWindowsOS()", "TheKeyboard->UPDATE()", "TheMouse->UPDATE()",
                         "TheWindowManager->UPDATE()", "TheDisplay->DRAW()", "TheShell->UPDATE()",
                         "TheGameClient->startMenuOnlyShell()", "setPushGuard(MenuPushGuard)", "kTotalMs", "kIdleMs"):
            self.assertIn(required, loop)
        self.assertLess(loop.index("TheWindowManager->UPDATE()"), loop.index("TheShell->UPDATE()"))
        self.assertLess(loop.index("TheShell->UPDATE()"), loop.rindex("DrainMenuStream(translator, c, false)"))
        draw = code_only(body(host, "void W3DDisplay::drawOrdinaryMenuFrame()"))
        for forbidden in ("Begin_Render(true, true", "WW3D::Render(", "drawViews", "TheGameLogic"):
            self.assertNotIn(forbidden, draw)
        self.assertIn("winRepaint()", draw)

    def test_boundary_covers_every_class_c_door(self):
        host = read(HOST)
        for token in ("MSG_NEW_GAME", "MSG_CLEAR_GAME_DATA", "MSG_BEGIN_NETWORK_MESSAGES",
                      "MSG_END_NETWORK_MESSAGES", "shell_push"):
            self.assertIn(token, host)
        guard = body(host, "Bool MenuPushGuard(")
        self.assertIn('"Menus/MainMenu.wnd"', guard)
        self.assertIn("return FALSE;", guard)

    def test_shell_push_guard_defaults_to_legacy_behavior(self):
        old = show("Core/GameEngine/Source/GameClient/GUI/Shell/Shell.cpp")
        new = read("Core/GameEngine/Source/GameClient/GUI/Shell/Shell.cpp")
        added = [line for line in new.splitlines() if line not in old.splitlines()]
        # reviewed deltas: the X64.18 push guard and (X64.37) the per-frame hook of the display Keep/Revert countdown
        self.assertEqual(sorted({x.strip() for x in added if x.strip()}),
                         ['#include "GameClient/ModernDisplayOptions.h"',
                          "ModernDisplayOptions::update();   // X64.37: countdown / automatic revert of an unconfirmed display change",
                          "if( m_pushGuard && !m_pushGuard( filename ) )", "m_pushGuard = nullptr;"])
        self.assertIn("m_pushGuard = nullptr;", new)

    def test_gate_and_host_paths_unchanged_and_selected_in_gamemain(self):
        main = read("Generals/Code/GameEngine/Source/Common/GameMain.cpp")
        self.assertLess(main.index("StartupDiagnostics::ordinaryMenu()) exitcode = TheDisplay->runOrdinaryMenuHost()"),
                        main.index("exitcode = TheDisplay->runOrdinaryHostGate()"))
        # The accepted X64.22 gate function is byte-identical.
        self.assertEqual(body(read(HOST), "int W3DDisplay::runOrdinaryHostGate()"),
                         body(show(HOST), "int W3DDisplay::runOrdinaryHostGate()"))
        self.assertEqual(body(read(HOST), "void W3DDisplay::drawOrdinaryHostFrame()"),
                         body(show(HOST), "void W3DDisplay::drawOrdinaryHostFrame()"))
        zh = read("GeneralsMD/Code/GameEngine/Source/Common/GameMain.cpp") + read("GeneralsMD/Code/Main/WinMain.cpp")
        self.assertNotIn("ordinaryMenu", zh)

    def test_keyboard_failure_is_explicit_only_under_menu(self):
        s = read("Core/GameEngineDevice/Source/Win32Device/GameClient/Win32DIKeyboard.cpp")
        self.assertEqual(s.count("keyboardOpenFailed( hr );"), 5)
        helper = body(s, "static void keyboardOpenFailed(")
        self.assertIn("StartupDiagnostics::ordinaryMenu()", helper)
        self.assertIn("throw std::runtime_error", helper)

    def test_launcher_requires_interaction_evidence(self):
        common = read("scripts/native-runtime/OrdinaryHostCommon.ps1")
        self.assertIn("-nativeOrdinaryMenu", common)
        self.assertIn("No proven mouse movement and click interaction", common)
        launch = read("scripts/native-runtime/Invoke-NativeOrdinaryMenu.ps1")
        for term in ("$exit -eq 0", "$shutdownBytes -eq 0", "$dumps.Count -eq 0", "$evidence.Valid", "$pathsMatch",
                     "-RunReviewedOrdinaryMenu", "do not rerun automatically"):
            self.assertIn(term, launch)


if __name__ == "__main__":
    unittest.main()
