#!/usr/bin/env python3
"""Static contract for the X64.28 ordinary-play profile (native world objects + interactive input).

The play token is the fifth strict profile. It implies the accepted terrain profile and adds only:
native world-object rendering, native picking, the in-game UI draw, the full Control Bar update and a
user-driven loop. These checks prove each addition is reachable only through the play token and that
the accepted terrain/game/menu/host profiles and the Win32 reference keep their behavior.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


class OrdinaryPlayContract(unittest.TestCase):
    def test_distinct_strict_token_implies_terrain_stack(self):
        grammar = read("Core/GameEngine/Include/Common/StartupDiagnostics.h")
        self.assertIn('validOrdinaryArguments(argc, argv, "-nativeOrdinaryPlay")', grammar)
        self.assertIn('"-nativeOrdinaryPlay")) { s.requested = true; s.ordinaryHost = s.ordinaryMenu = s.ordinaryGame = s.ordinaryTerrain = s.ordinaryPlay = true; }', grammar)
        # The terrain token never implies play.
        self.assertIn('"-nativeOrdinaryTerrain")) { s.requested = true; s.ordinaryHost = s.ordinaryMenu = s.ordinaryGame = s.ordinaryTerrain = true; }', grammar)
        self.assertIn("s.ordinaryPlay ? validOrdinaryPlayArguments(argc, argv) : s.ordinaryTerrain ?", grammar)
        # Zero Hour and Win32 keep rejecting every ordinary token.
        self.assertIn("#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR", grammar)
        self.assertIn("#if !defined(_WIN64)\n    return false; // Native diagnostic option must never change the Win32 route.", grammar)

    def test_object_pass_and_hooks_are_play_only(self):
        objects = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldObjects.cpp")
        self.assertIn("return g_forceActive || StartupDiagnostics::ordinaryPlay();", objects)
        # add() is a no-op unless active, so headless replays and Win32 never track render objects.
        self.assertRegex(objects, r"void add\(RenderObjClass \*object, int hexColor\)\n\{\n    if \(!object \|\| !active\(\)\) return;")
        header = read("Core/GameEngineDevice/Include/W3DDevice/GameClient/NativeWorldObjects.h")
        self.assertIn("inline void add(RenderObjClass *, int = 0) {}", header)  # non-Win64 no-op
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn("if (draws && StartupDiagnostics::ordinaryPlay()) {", host)
        for hook in ("Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp",
                     "Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.cpp"):
            text = read(hook)
            # The legacy scene branch is untouched; the native registry is only the else branch.
            self.assertRegex(text, r"if \(W3DDisplay::m_3DScene != nullptr\)\n\s+W3DDisplay::m_3DScene->Add_Render_Object\(m_renderObject\);\n\s+else\n\s+NativeWorldObjects::add\(")
            self.assertRegex(text, r"if \(W3DDisplay::m_3DScene != nullptr\)\n\s+W3DDisplay::m_3DScene->Remove_Render_Object\(m_renderObject\);\n\s+else\n\s+NativeWorldObjects::remove\(")

    def test_view_and_control_bar_changes_are_play_only(self):
        view = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp")
        self.assertIn("if (TheGlobalData->m_headless && !StartupDiagnostics::ordinaryPlay())", view)
        self.assertIn("if( W3DDisplay::m_3DScene == nullptr && !StartupDiagnostics::ordinaryPlay() )", view)
        # The Win32 (and every non-_WIN64) branch keeps the original unconditional headless return.
        self.assertIn("#else\n\tif (TheGlobalData->m_headless)\n\t\treturn;\n#endif", view)
        bar = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp")
        self.assertIn("if (!StartupDiagnostics::ordinaryPlay())\n\t\t{\n\t\t\tif (StartupDiagnostics::ordinaryTerrain())\n\t\t\t\tupdateNativePresentation();\n\t\t\treturn;\n\t\t}", bar)
        ui = read("Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp")
        # Hint objects use the legacy scene whenever it exists.
        self.assertIn("if( W3DDisplay::m_3DScene )\n\t\tW3DDisplay::m_3DScene->Add_Render_Object( object );", ui)
        self.assertNotIn("W3DDisplay::m_3DScene->Add_Render_Object( m_", ui)

    def test_play_loop_is_user_driven_and_bounded(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn("const unsigned kTotalMs = play ? 3600000 : 900000;", host)
        self.assertIn("if (!play && frame - startFrame >= kLogicFrameBound) {", host)
        self.assertIn("if (StartupDiagnostics::ordinaryTerrain() && !play && inGame && !exiting && TheTacticalView && TheTerrainLogic)", host)
        self.assertIn("exitReason = \"game_ended\";", host)
        # The in-game UI draw replaces the plain window repaint only for play with a drawn world.
        self.assertIn("if (StartupDiagnostics::ordinaryPlay() && terrainDraws && TheInGameUI) {", host)
        self.assertIn("view->drawNativeWorldOverlays();", host)
        self.assertIn("TheInGameUI->DRAW();", host)

    def test_play_receipts_are_bounded(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn("playReceipts < 120", host)
        self.assertIn("playImages < 3", host)
        # Capacity: the 2048-record log fails closed; the play receipts must stay far below it.
        header = read("Core/GameEngine/Include/Common/StartupDiagnostics.h")
        self.assertIn("s.records >= 2048", header)

    def test_message_tap_only_observes(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        body = host.split("class PlayTap : public GameMessageTranslator {", 1)[1].split("PlayTap g_playTap;", 1)[0]
        self.assertIn("return KEEP_MESSAGE;", body)
        self.assertNotIn("DESTROY_MESSAGE", body)
        self.assertIn("attachTranslator(&g_playTap, 998000000)", host)
        self.assertIn("removeTranslator(playTap)", host)

    def test_zero_hour_sources_do_not_mention_play(self):
        zh = read("GeneralsMD/Code/GameEngine/Source/Common/GameMain.cpp") + read("GeneralsMD/Code/Main/WinMain.cpp")
        self.assertNotIn("ordinaryPlay", zh)

    def test_script_surface_is_complete(self):
        common = read("scripts/native-runtime/OrdinaryHostCommon.ps1")
        for term in ("-nativeOrdinaryPlay", "Get-NativeOrdinaryPlayMode", "Get-NativeOrdinaryPlayArguments",
                     "Read-NativeOrdinaryPlayStage", "Test-NativeOrdinaryPlayEvidence"):
            self.assertIn(term, common)
        invoke = read("scripts/native-runtime/Invoke-NativeOrdinaryPlay.ps1")
        self.assertIn("-RunReviewedOrdinaryPlay", invoke)
        self.assertIn("requires explicit developer review", invoke)
        self.assertIn("ValidateSet('Generals')", invoke)
        # Evidence receipts are written next to the log, never into the staged game directory.
        self.assertIn("runs/", invoke)

    def test_every_play_summary_format_fits_its_destination_buffer(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        # play_progress and play_summary are the new long formats; all of them use the 640-byte detail buffer.
        self.assertIn("char detail[640];", host)
        for name in ("play_progress", "play_summary", "world_objects_summary", "world_objects_first_frame"):
            self.assertIn(f'"{name}"', host)
        longest = 0
        for literal in re.findall(r'sprintf_s\(detail, "((?:[^"\\]|\\.)*)"((?:\s*"(?:[^"\\]|\\.)*")*)', host):
            text = literal[0] + "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', literal[1]))
            expanded = re.sub(r"%\.?\d*f", "-1234567.123", text)
            expanded = re.sub(r"%0?\d*[uXd]", "4294967295", expanded)
            expanded = re.sub(r"%\.\d+s", "S" * 60, expanded)
            expanded = re.sub(r"%s", "S" * 60, expanded)
            longest = max(longest, len(expanded))
        self.assertLess(longest, 640 - 1)


if __name__ == "__main__":
    unittest.main()
