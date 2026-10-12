"""X64.22 ordinary-host source contracts; never executes a title."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "55bc829890f53118cc7205d4a8c32708c6ecb8b5"  # accepted X64.21 handoff


def read(path):
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")


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
    """Remove string literals so receipt text cannot satisfy or violate code checks."""
    import re
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', source)


PRESENTATION = "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp"
DISPLAY = "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp"


class OrdinaryHostContract(unittest.TestCase):
    def test_world_selection_is_not_globally_flipped(self):
        for path in (PRESENTATION, DISPLAY, "Generals/Code/Main/WinMain.cpp",
                     "Generals/Code/GameEngine/Source/Common/GameMain.cpp"):
            s = read(path)
            self.assertNotIn("m_headless = FALSE", s, path)
            self.assertNotIn("m_headless=FALSE", s, path)
            self.assertNotIn("m_headless = false", s, path)
        # The only headless writers remain the existing parse path.
        cl = read("Core/GameEngine/Source/Common/CommandLine.cpp")
        self.assertEqual(cl.count("m_headless = TRUE"), 1)

    def test_display_init_branch_is_after_legacy_block_and_never_enters_dx8(self):
        s = read(DISPLAY)
        init = body(s, "void W3DDisplay::init()")
        legacy = init.index("WW3D::Init( ApplicationHWnd )")
        ordinary = init.index("initOrdinaryHostPresentation();")
        self.assertLess(legacy, ordinary)
        self.assertLess(ordinary, init.index("initAssets();"))
        self.assertIn("#if defined(_WIN64)\n\telse if (StartupDiagnostics::ordinaryHost())", init)
        raw = body(read(PRESENTATION), "void W3DDisplay::initOrdinaryHostPresentation()")
        fn = code_only(raw)
        self.assertIn("WW3D::Init_Native_Presentation(configuration)", fn)
        for forbidden in ("WW3D::Init(", "Set_Render_Device", "DX8Wrapper", "LoadLibrary",
                          "Create_Render_Backend", "W3DShaderManager::init", "m_headless =",
                          "->update(", "execute(", "TheGameLogic", "showShell"):
            self.assertNotIn(forbidden, fn)
        self.assertEqual(raw.count('"display_bypass"'), 1)  # loop over six receipted decisions
        for token in ("legacy_ww3d_init", "legacy_render_device", "legacy_scenes",
                      "legacy_shader_manager", "legacy_debug_display_browser", "legacy_gamma_lod"):
            self.assertIn(token, raw)
        self.assertLess(fn.index("clientIsExact"), fn.index("Init_Native_Presentation"))
        self.assertLess(fn.index("m_2DRender = NEW Render2DClass;"), fn.index("setWidth(800);"))

    def test_ui_only_draw_has_no_world_or_depth(self):
        presentation = read(PRESENTATION)
        frame = body(presentation, "void W3DDisplay::drawOrdinaryHostFrame()")
        self.assertIn("Begin_Render(true, false,", frame)
        for forbidden in ("Begin_Render(true, true", "WW3D::Render(", "drawViews", "updateViews",
                          "TheGameLogic", "TheTerrain", "TheTacticalView"):
            self.assertNotIn(forbidden, frame)
        draw = body(read(DISPLAY), "void W3DDisplay::draw()")
        # Profile branch precedes the unchanged legacy headless return.
        self.assertLess(draw.index("drawOrdinaryHostFrame();"), draw.index("if (TheGlobalData->m_headless)\n\t\treturn;"))
        self.assertIn("TheGlobalData->m_headless && StartupDiagnostics::ordinaryHost() && m_2DRender", draw)

    def test_gate_loop_never_updates_or_executes(self):
        gate = body(read(PRESENTATION), "int W3DDisplay::runOrdinaryHostGate()")
        for forbidden in ("execute(", "TheGameClient->UPDATE", "TheGameLogic", "->update(", "propagateMessages",
                          "showShell", "TheShell", "m_headless ="):
            self.assertNotIn(forbidden, gate)
        self.assertIn("TheGameEngine->serviceWindowsOS()", gate)
        self.assertIn("TheDisplay->DRAW()", gate)
        self.assertIn("NativeGraphicsSmoke::Evaluate", gate)
        self.assertIn("5000, 300", gate)
        # Strings are released before returning to the engine/display teardown.
        self.assertLess(gate.index("content.reset();"), gate.index("ordinary_host_summary"))

    def test_teardown_order_residency_then_backend_then_window(self):
        dtor = body(read(DISPLAY), "W3DDisplay::~W3DDisplay()")
        self.assertLess(dtor.index("m_assetManager->Free_Assets();"), dtor.index("releaseOrdinaryHostPresentation();"))
        release = body(read(PRESENTATION), "void W3DDisplay::releaseOrdinaryHostPresentation()")
        self.assertLess(release.index("D3D11Backend::Live_Residencies()"), release.index("WW3D::Shutdown_Native_Presentation()"))
        self.assertIn("markFailed()", release)
        win = read("Generals/Code/Main/WinMain.cpp")
        main = body(win, "Int APIENTRY WinMain(")
        self.assertLess(main.index("exitcode = GameMain();"), main.index("releaseOrdinaryHostWindow(hInstance);"))
        rel = body(win, "static void releaseOrdinaryHostWindow(")
        self.assertLess(rel.index("DestroyWindow( window )"), rel.index("UnregisterClass( "))

    def test_checked_window_and_legacy_path_untouched(self):
        win = read("Generals/Code/Main/WinMain.cpp")
        fn = body(win, "static void initializeOrdinaryHostWindow(")
        for term in ("RegisterClass( &wndClass ) == 0", "hWnd == nullptr", "clientIsExact", "GetLastError"):
            self.assertIn(term if term != "GetLastError" else "hostFail", fn)
        self.assertIn("WndProc", fn)
        # Legacy function keeps its unchecked structure verbatim.
        old = subprocess.check_output(["git", "show", f"{BASELINE}:Generals/Code/Main/WinMain.cpp"], cwd=ROOT).decode("latin1").replace("\r\n", "\n")
        self.assertEqual(body(win, "static Bool initializeAppWindows("), body(old, "static Bool initializeAppWindows("))
        self.assertEqual(body(win, "LRESULT CALLBACK WndProc("), body(old, "LRESULT CALLBACK WndProc("))

    def test_main_gate_precedes_replay_and_execute_and_is_generals_only(self):
        s = read("Generals/Code/GameEngine/Source/Common/GameMain.cpp")
        init = s.index("TheGameEngine->init();")
        host = s.index("StartupDiagnostics::ordinaryHost()", init)
        self.assertLess(host, s.index("else if (!TheGlobalData->m_simulateReplays.empty())"))
        self.assertLess(host, s.index("TheGameEngine->execute();"))
        self.assertLess(host, s.index("StartupDiagnostics::displaySmoke()", init))
        zh = read("GeneralsMD/Code/GameEngine/Source/Common/GameMain.cpp") + read("GeneralsMD/Code/Main/WinMain.cpp")
        self.assertNotIn("ordinaryHost", zh)
        grammar = read("Core/GameEngine/Include/Common/StartupDiagnostics.h")
        self.assertIn("#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR", grammar)
        for token in ('"-win", "-xres", "800", "-yres", "600"', '"-nologo", "-noshellmap", "-winCursors", "-nativeOrdinaryHost"',
                      "argc != 11"):
            self.assertIn(token, grammar)

    def test_reference_factory_and_legacy_render_device_untouched(self):
        for path in ("Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.cpp",
                     "Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp"):
            old = subprocess.check_output(["git", "show", f"{BASELINE}:{path}"], cwd=ROOT)
            self.assertEqual((ROOT / path).read_bytes().replace(b"\r\n", b"\n"), old.replace(b"\r\n", b"\n"), path)

    def test_launcher_requires_real_success_and_exact_grammar(self):
        launch = read("scripts/native-runtime/Invoke-NativeOrdinaryHost.ps1")
        for term in ("$exit -eq 0", "$shutdownBytes -eq 0", "$dumps.Count -eq 0", "$evidence.Valid",
                     "$pathsMatch", "-RunReviewedOrdinaryHost", "do not rerun automatically"):
            self.assertIn(term, launch)
        common = read("scripts/native-runtime/OrdinaryHostCommon.ps1")
        self.assertIn("@('-win','-xres','800','-yres','600','-nologo','-noshellmap','-winCursors','-nativeOrdinaryHost',$Root)", common)
        self.assertNotIn("Start-Process", common)


if __name__ == "__main__":
    unittest.main()
