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
        self.assertIn('attachTranslator(MSGNEW("PlayTap") PlayTap, 998000000)', host)
        self.assertIn("removeTranslator(playTap)", host)

    def test_play_tap_is_heap_allocated_because_the_stream_owns_translators(self):
        # X64.28 run 20261013T1151501824715Z: a static tap was deleted by MessageStream::removeTranslator.
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertNotIn("PlayTap g_playTap;", host)
        self.assertNotIn("attachTranslator(&", host)
        self.assertIn('attachTranslator(MSGNEW("PlayTap") PlayTap, 998000000)', host)
        stream = read("Core/GameEngine/Include/Common/MessageStream.h")
        self.assertIn("delete m_translator;", stream)

    def test_scene_less_profile_never_dereferences_legacy_shadow_managers(self):
        # X64.28 run 20261013T1155367083324Z: createRadiusDecal used the null TheProjectedShadowManager.
        decal = read("Core/GameEngine/Source/GameClient/RadiusDecal.cpp")
        guard = decal.index("if (TheProjectedShadowManager == nullptr)")
        self.assertLess(decal.index("result.m_empty = false;"), guard)
        self.assertLess(guard, decal.index("TheProjectedShadowManager->addDecal"))
        self.assertLess(guard, decal.index("owningPlayer->getPlayerIndex()"))
        debris = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.cpp")
        self.assertIn("if (t != SHADOW_NONE && (TheW3DShadowManager != nullptr || TheProjectedShadowManager != nullptr))", debris)
        # Every remaining dereference of the shadow managers in the shared draw modules is null-guarded.
        for path in ("Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp",
                     "Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.cpp"):
            lines = read(path).splitlines()
            for number, line in enumerate(lines):
                if "TheW3DShadowManager->addShadow" in line or "TheProjectedShadowManager->addDecal" in line:
                    context = " ".join(lines[max(0, number - 20):number + 1])
                    self.assertTrue("TheW3DShadowManager &&" in context or "TheW3DShadowManager != nullptr" in context
                                    or "TheW3DShadowManager ?" in context or "(TheW3DShadowManager ||" in context
                                    or "if (TheProjectedShadowManager)" in context, f"{path}:{number + 1}")

    def test_world_objects_receipt_follows_terrain_first_frame(self):
        # X64.28 native runs logged world_objects_first_frame one record before terrain_first_frame.
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        terrain = host.index('checkpoint("terrain_first_frame"')
        self.assertEqual(host.count('checkpoint("world_objects_first_frame"'), 1)
        self.assertLess(terrain, host.index('checkpoint("world_objects_first_frame"'))
        common = read("scripts/native-runtime/OrdinaryHostCommon.ps1")
        self.assertIn("'terrain_first_frame','world_objects_first_frame','play_summary'", common)

    def test_last_logic_frame_is_not_overwritten_after_leaving_the_match(self):
        # X64.28 fix1 run 20261013T1216116100741Z: clean Exit Game, but game_summary logic_frames=0.
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        update = host.index("lastFrame = frame;")
        self.assertEqual(host.count("lastFrame = frame;"), 1)
        guard = host.rindex("if (inGame && !exiting", 0, update)
        self.assertIn("TheGameLogic->isInGame()", host[guard:update])
        # The exit detection that follows runs in the same iteration, after the guarded update.
        self.assertLess(update, host.index('exitReason = "game_ended";'))
        common = read("scripts/native-runtime/OrdinaryHostCommon.ps1")
        self.assertIn("-lt 150){$errors.Add('Fewer than 150 logic frames in game')}", common)  # contract unchanged

    def test_x64_29_effects_are_play_only_and_have_a_disable_fallback(self):
        effects = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldEffects.cpp")
        # Every producer is behind its own feature switch (GENERALS_NATIVE_DISABLE) and runs only through collect().
        for feature in ("scorches", "shadows", "water", "shroud", "particles", "lines"):
            self.assertIn('nativeFeature("%s")' % feature, effects)
        header = read("Core/GameEngine/Include/Common/StartupDiagnostics.h")
        self.assertIn("GENERALS_NATIVE_DISABLE", header)
        # The new native hooks in shared sources are play-gated.
        display = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp")
        self.assertIn("#if defined(_WIN64)\n\telse if (StartupDiagnostics::ordinaryPlay())", display)
        engine = read("Generals/Code/GameEngine/Source/Common/GameEngine.cpp")
        self.assertIn("!(StartupDiagnostics::ordinaryPlay() && StartupDiagnostics::nativeFeature(\"particles\"))", engine)
        logic = read("Generals/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp")
        self.assertIn("!(StartupDiagnostics::ordinaryPlay() && StartupDiagnostics::nativeFeature(\"ghosts\"))", logic)
        ghosts = read("Generals/Code/GameEngineDevice/Source/W3DDevice/GameLogic/W3DGhostObject.cpp")
        self.assertIn("TheGlobalData->m_headless && !StartupDiagnostics::ordinaryPlay()", ghosts)
        # Layer order: opaque, decal, translucent (objects/water/lines), shroud overlay, particles.
        objects = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldObjects.cpp")
        self.assertIn("LayerOpaque = 0, LayerDecal = 1, LayerTranslucent = 2, LayerOverlay = 3, LayerParticle = 4", objects)

    def test_x64_29_review_fixes_are_pinned(self):
        objects = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldObjects.cpp")
        # The particle layer was emitted but never submitted: iterate by count, never up to a named layer.
        self.assertIn("for (unsigned layer = 0; layer < LayerCount; ++layer) {", objects)
        self.assertNotIn("layer <= LayerOverlay", objects)
        self.assertIn("static_assert(LayerParticle + 1 == LayerCount", objects)
        effects = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldEffects.cpp")
        # Legacy renderStencilShadows: SRCBLEND = DESTCOLOR, DESTBLEND = ZERO (multiply by the shadow color).
        volume = effects.split("void EmitVolumeShadow(", 1)[1].split("void CollectShadows(", 1)[0]
        self.assertIn("b.blend = WorldBlend::Multiply;", volume)
        self.assertIn("b.stencilOnce = true;", volume)
        # Overlay/scorch geometry lies on the terrain lattice with the terrain's own diagonals.
        self.assertIn("TerrainLatticeFlip", effects)
        self.assertIn("&TerrainLatticeHeight, &TerrainLatticeFlip", effects)
        # Objects fogged/ghosted are drawn through the shroud like the legacy shroud material pass.
        self.assertIn("if (info && !info->m_drawable) input.shrouded = true;", objects)
        self.assertIn("input.shrouded = ss > OBJECTSHROUD_CLEAR;", objects)
        # The review window keeps the accepted app_window receipt and the logical 800x600 display.
        main = read("Generals/Code/Main/WinMain.cpp")
        self.assertIn('"client=800x600 windowed=1 class=Game Window"', main)
        self.assertIn('checkpoint( "review_window", "observed"', main)
        host = read("Core/GameEngine/Include/Common/NativeOrdinaryHost.h")
        self.assertIn("GENERALS_NATIVE_REVIEW_SCALE", host)
        mouse = read("Core/GameEngineDevice/Source/Win32Device/GameClient/Win32Mouse.cpp")
        self.assertIn("NativeOrdinaryHost::clientToLogical", mouse)

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
