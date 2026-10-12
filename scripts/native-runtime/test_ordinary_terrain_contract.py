#!/usr/bin/env python3
"""Static contract for the X64.25 ordinary-terrain profile (first native terrain frame).

The terrain world pass is a new strict profile token; the accepted X64.22-24 profiles,
the Win32/DX8 reference and Zero Hour must be unchanged. These checks are source-level:
they prove the new behaviour is reachable only through the terrain token.
"""
import pathlib
import re
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
ACCEPTED_X64_24 = "b78388f1e"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def baseline(path):
    return subprocess.check_output(["git", "show", f"{ACCEPTED_X64_24}:{path}"], cwd=ROOT).decode("utf-8", "replace").replace("\r\n", "\n")


def strip_win64_blocks(text):
    """Remove #if defined(_WIN64) ... #endif blocks (native-only additions)."""
    out, depth = [], 0
    for line in text.split("\n"):
        if depth:
            if re.match(r"\s*#\s*if", line):
                depth += 1
            elif re.match(r"\s*#\s*endif", line):
                depth -= 1
            continue
        if re.match(r"\s*#\s*if\s+defined\(_WIN64\)\s*$", line):
            depth = 1
            continue
        out.append(line)
    return "\n".join(out)


class OrdinaryTerrainContract(unittest.TestCase):
    def test_distinct_strict_token_implies_game_stack_only(self):
        grammar = read("Core/GameEngine/Include/Common/StartupDiagnostics.h")
        self.assertIn('validOrdinaryArguments(argc, argv, "-nativeOrdinaryTerrain")', grammar)
        self.assertIn('"-nativeOrdinaryTerrain")) { s.requested = true; s.ordinaryHost = s.ordinaryMenu = s.ordinaryGame = s.ordinaryTerrain = true; }', grammar)
        # The game token never implies terrain.
        self.assertIn('"-nativeOrdinaryGame")) { s.requested = true; s.ordinaryHost = s.ordinaryMenu = s.ordinaryGame = true; }', grammar)
        self.assertIn("argc != 11", grammar)

    def test_terrain_work_is_gated_on_the_profile(self):
        heightmap = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp")
        self.assertIn("const Bool nativeTerrain = StartupDiagnostics::ordinaryTerrain();", heightmap)
        self.assertIn("(m_treeBuffer != nullptr || nativeTerrain)", heightmap)
        self.assertIn("m_treeBuffer == nullptr && StartupDiagnostics::ordinaryTerrain()", heightmap)
        world = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/WorldHeightMap.cpp")
        self.assertIn("if (!StartupDiagnostics::ordinaryTerrain()) return nullptr;", world)
        self.assertIn("if (StartupDiagnostics::ordinaryTerrain()) {", world)
        ingame = read("Generals/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DInGameUI.h")
        self.assertIn("if (dummy && StartupDiagnostics::ordinaryTerrain())", ingame)
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        for opt_in in ("Enable_CPU_Backed_Geometry(true)", "Enable_Depth_Buffer_For_W3D(true)"):
            at = host.index(opt_in)
            self.assertIn("StartupDiagnostics::ordinaryTerrain()", host[max(0, at - 500):at])

    def test_lifecycle_and_legacy_presentation_untouched(self):
        # Depth is requested through the explicit backend opt-in, never the legacy lifecycle,
        # so ww3d.cpp and PresentationLifecycle stay byte-identical to the accepted baseline.
        for path in ("Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp",
                     "Core/Libraries/Source/WWVegas/WW3D2/PresentationLifecycle.cpp",
                     "Core/Libraries/Source/WWVegas/WW3D2/PresentationLifecycle.h",
                     "Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.cpp"):
            self.assertEqual(read(path), baseline(path), path)

    def test_win32_reference_and_zero_hour_have_no_terrain_route(self):
        for path in ("Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp",
                     "Core/GameEngineDevice/Source/W3DDevice/GameClient/WorldHeightMap.cpp",
                     "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp",
                     "Core/GameEngineDevice/Include/W3DDevice/GameClient/HeightMap.h",
                     "Core/GameEngineDevice/Include/W3DDevice/GameClient/WorldHeightMap.h"):
            # With the _WIN64 blocks removed the reference code is byte-identical to the baseline,
            # except for the explicitly listed gate lines asserted separately above.
            now = [l for l in strip_win64_blocks(read(path)).split("\n") if l.strip()]
            old = [l for l in strip_win64_blocks(baseline(path)).split("\n") if l.strip()]
            if path.endswith("HeightMap.cpp") and "WorldHeightMap" not in path:
                # The only unconditional edit is the allocation condition, which also admits the
                # profile flag; every other line is identical to the baseline.
                now = [l.replace("(m_treeBuffer != nullptr || nativeTerrain)", "m_treeBuffer != nullptr") for l in now]
            self.assertEqual(now, old, path)
        zh = read("GeneralsMD/Code/GameEngine/Source/Common/GameMain.cpp") + read("GeneralsMD/Code/Main/WinMain.cpp")
        self.assertNotIn("ordinaryTerrain", zh)
        grammar = read("Core/GameEngine/Include/Common/StartupDiagnostics.h")
        self.assertIn("#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR", grammar)

    def test_world_view_changes_do_not_affect_win32_semantics(self):
        view = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp")
        # Unconditional edits are headless-only or null-scene-only (never true on Win32 W3DView).
        self.assertIn("W3DDisplay::m_3DScene ? W3DDisplay::m_3DScene->createLightsIterator() : nullptr", view)
        self.assertIn("// The headless world selection (native terrain profile) has no drawable rendering.", view)
        self.assertIn("if (TheGlobalData->m_headless && !StartupDiagnostics::ordinaryTerrain())", view)

    def test_native_pass_is_terrain_only_and_explicit(self):
        heightmap = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp")
        body = heightmap.split("Int HeightMapRenderObjClass::renderNative(CameraClass &camera)", 1)[1].split(
            "// HeightMapRenderObjClass::Render", 1)[0]
        for term in ("WorldMaterial::TerrainBase", "WorldMaterial::TerrainBlend", "Create_Mipped_Rgba8_Residency",
                     "Create_Geometry_Residency", "Update_Geometry_Vertices", "Submit_World",
                     "requires CPU-backed legacy buffers", "requires the D3D11 backend with a depth buffer"):
            self.assertIn(term, body)
        # No silent fallback to legacy draws, DX8 state or other subsystems.
        for term in ("DX8Wrapper", "Draw_Triangles", "m_roadBuffer", "m_bridgeBuffer", "m_propBuffer",
                     "renderShoreLines", "drawScorches"):
            self.assertNotIn(term, body)
        display = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        pass_fn = display.split("static int drawNativeTerrainPass()", 1)[1].split("void W3DDisplay::drawOrdinaryMenuFrame()", 1)[0]
        self.assertIn("Clear(false, true,", pass_fn)  # depth-only clear after the color-only Begin
        self.assertIn("isLoadingMap()", pass_fn)

    def test_launcher_requires_terrain_evidence(self):
        common = read("scripts/native-runtime/OrdinaryHostCommon.ps1")
        for term in ("-nativeOrdinaryTerrain", "Get-NativeOrdinaryTerrainMode", "Test-NativeOrdinaryTerrainEvidence",
                     "Terrain draws are not exactly base+blend per uploaded tile",
                     "Missing first terrain frame with one atlas upload",
                     "Too few terrain frames/draws or no residency uploads"):
            self.assertIn(term, common)
        launch = read("scripts/native-runtime/Invoke-NativeOrdinaryTerrain.ps1")
        for term in ("-RunReviewedOrdinaryTerrain", "$exit -eq 0", "$shutdownBytes -eq 0", "$dumps.Count -eq 0",
                     "$evidence.Valid", "$pathsMatch", "do not rerun automatically"):
            self.assertIn(term, launch)
        self.assertNotIn("Start-Process", common)


if __name__ == "__main__":
    unittest.main()
