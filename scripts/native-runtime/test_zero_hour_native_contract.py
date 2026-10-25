#!/usr/bin/env python3
"""X64.45 contract: the Zero Hour normal launch carries the same reviewed native hooks as Generals, and nothing else.

Before X64.45 the AMD64 normal launch (a game executable started without a diagnostic option) selected the headless world for
Zero Hour as well (Core CommandLine.cpp), but only Generals had the hooks that turn that selection into the real application:
the ordinary-host window, the real mouse / window manager / shell, native particles and ghosts, the native road / bridge / bib /
waypoint buffers, the Modern Experience options screen, footer and HUD placement. A generalszh.exe started that way would stop at
display initialisation (no ApplicationHWnd) or run a window-less shell. These tests pin the port:

* every hook line the fork added to a Generals file also exists in its Zero Hour twin, except an explicit, justified allow-list
  (diagnostic-only receipts, the Zero Hour specific road traversal);
* Zero Hour's GameMain does not dispatch the Generals-only diagnostic hosts, and its WinMain does not know the menu / game /
  terrain diagnostic profiles;
* the Zero Hour ordinary-host window code is byte-identical to the Generals code it was copied from;
* the diagnostic profile grammar and hosts still refuse Zero Hour.
"""
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "adac468d7"   # upstream merge-base of the fork: the files as they were before any native hook existed


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def added_lines(generals_path):
    """Lines the fork added to a Generals file (working tree against the upstream baseline), whitespace-insensitive."""
    out = subprocess.run(["git", "diff", "-w", "--ignore-cr-at-eol", "-U0", BASELINE, "--", generals_path], cwd=ROOT,
                         capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
    return [l[1:].strip() for l in out.splitlines() if l.startswith("+") and not l.startswith("+++") and l[1:].strip()]


def missing_in_zero_hour(generals_path):
    twin = "GeneralsMD/" + generals_path[len("Generals/"):]
    present = {l.strip() for l in read(twin).split("\n")}
    return [l for l in added_lines(generals_path) if l not in present]


G = "Generals/Code/"
GD = G + "GameEngineDevice/"
# file -> (substrings whose lines are allowed to differ, why)
PARITY = {
    GD + "Include/W3DDevice/GameClient/W3DBibBuffer.h": (),
    GD + "Include/W3DDevice/GameClient/W3DBridgeBuffer.h": (),
    GD + "Include/W3DDevice/GameClient/W3DInGameUI.h": (),
    GD + "Include/W3DDevice/GameClient/W3DRoadBuffer.h": (),
    GD + "Include/W3DDevice/GameClient/W3DWaypointBuffer.h": (),
    GD + "Source/W3DDevice/GameClient/W3DBibBuffer.cpp": (),
    GD + "Source/W3DDevice/GameClient/W3DBridgeBuffer.cpp": (),
    GD + "Source/W3DDevice/GameClient/W3DInGameUI.cpp": (),
    GD + "Source/W3DDevice/GameClient/W3dWaypointBuffer.cpp": (),
    GD + "Source/W3DDevice/GameLogic/W3DGhostObject.cpp": (),
    G + "GameEngine/Source/Common/GameEngine.cpp": (),
    G + "GameEngine/Source/GameClient/InGameUI.cpp": (),
    G + "GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp": (),
    G + "GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp": ("Generals has no Challenge menu", "ModernTheme::Title::Generals"),   # UI.2: the two title-specific lines of the Modern main menu glue
    # Zero Hour's road buffer culls with per-segment visibility (visibilityChanged), so its native traversal is its own function
    # (the bounds / loadRoadsInVertexAndIndexBuffers lines are Generals' drawRoads shape); the rest is identical.
    GD + "Source/W3DDevice/GameClient/W3DRoadBuffer.cpp": (
        "/** Mirrors the traversal of drawRoads", "D3D11 path, which draws from the CPU-backed buffers", "m_minX = minX*MAP_XY_FACTOR;",
        "m_maxX = maxX*MAP_XY_FACTOR;", "m_minY = minY*MAP_XY_FACTOR;", "m_maxY = maxY*MAP_XY_FACTOR;",
        "loadRoadsInVertexAndIndexBuffers();"),
    # Diagnostic-only: the setFPMode receipts feed the Generals diagnostic summaries (nativeFloatingPointModeCalls is Generals-only).
    G + "GameEngine/Source/GameLogic/System/GameLogic.cpp": (
        "Number of setFPMode() calls", "Every call is still verified", "s_fpModeCalls", "nativeFloatingPointModeCalls", "RepeatGate gate", "lastControl", "_mm_getcsr", "gate.calls", "gate.admit",
        "fpState, \"control="),
    # Diagnostic-only: the menu-only profile entry point (Generals-only host).
    G + "GameEngine/Source/GameClient/GameClient.cpp": (
        "Ordinary-menu profile", "movies (unavailable on AMD64)", "off by profile", "startMenuOnlyShell"),
    # Window code is pinned byte-for-byte below; only the begin-checkpoint text and one comment differ by design.
    G + "Main/WinMain.cpp": ("StartupDiagnostics::checkpoint(\"WinMain\", \"begin\"", "// Ordinary-host profile: checked real application window."),
}


class ZeroHourNativeContract(unittest.TestCase):
    def test_every_native_hook_of_generals_exists_in_zero_hour(self):
        for generals_path, allowed in PARITY.items():
            with self.subTest(file=generals_path):
                missing = [l for l in missing_in_zero_hour(generals_path) if not any(a in l for a in allowed)]
                self.assertEqual(missing, [], generals_path)

    def test_the_parity_check_bites(self):
        # The comparison must find something when a hook really is absent: Player.cpp carries a Generals-only shortcut fix that Zero
        # Hour intentionally does not have (see the X64.45 document, section on deferred items).
        self.assertTrue(missing_in_zero_hour(G + "GameEngine/Source/Common/RTS/Player.cpp"))

    def test_the_ordinary_host_window_is_byte_identical_to_generals(self):
        def block(path):
            s = read(path)
            a = s.index("#if defined(_WIN64)\n// initializeOrdinaryHostWindow")
            b = s.index("// Necessary to allow memory managers and such")
            return s[a:b]
        self.assertEqual(block("GeneralsMD/Code/Main/WinMain.cpp"), block("Generals/Code/Main/WinMain.cpp"))

    def test_winmain_selects_and_releases_the_host_window_only_through_the_reviewed_calls(self):
        s = read("GeneralsMD/Code/Main/WinMain.cpp")
        self.assertEqual(s.count("initializeOrdinaryHostWindow(hInstance, nCmdShow);"), 1)
        self.assertEqual(s.count("releaseOrdinaryHostWindow(hInstance);"), 1)
        self.assertLess(s.index("initializeOrdinaryHostWindow(hInstance, nCmdShow);"), s.index("initializeAppWindows(hInstance, nCmdShow"))
        self.assertLess(s.index("exitcode = GameMain();"), s.index("releaseOrdinaryHostWindow(hInstance);"))
        for token in ("ordinaryMenu", "ordinaryGame", "ordinaryTerrain"):
            self.assertNotIn(token, s, "the diagnostic profiles stay Generals-only")

    def test_gamemain_does_not_dispatch_the_generals_only_diagnostic_hosts(self):
        s = read("GeneralsMD/Code/GameEngine/Source/Common/GameMain.cpp")
        for token in ("ordinaryHost", "ordinaryMenu", "ordinaryGame", "ordinaryTerrain", "ordinaryPlay",
                      "runOrdinaryHostGate", "runOrdinaryMenuHost", "runOrdinaryGameHost"):
            self.assertNotIn(token, s)

    def test_the_diagnostic_grammar_and_hosts_still_refuse_zero_hour(self):
        grammar = read("Core/GameEngine/Include/Common/StartupDiagnostics.h")
        self.assertRegex(grammar, r"#if defined\(RTS_ZEROHOUR\) && RTS_ZEROHOUR\n\s*\(void\)argc; \(void\)argv; \(void\)mode; return false;")
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn('"menu_host", "fail", "Generals only"', host)
        self.assertIn('"game_host", "fail", "Generals only"', host)

    def test_native_terrain_features_are_not_title_gated_any_more(self):
        heightmap = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp")
        self.assertNotIn("RTS_GENERALS", heightmap)
        for term in ("forEachNativeBatch", "forEachNativeBridge", "isNativeInitialized", "setNative(TRUE)"):
            self.assertIn(term, heightmap)
        base = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp")
        self.assertNotIn("RTS_GENERALS && defined(_WIN64)", base)
        self.assertNotIn("RTS_GENERALS", read("Core/GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h"))
        effects = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldEffects.cpp")
        self.assertNotIn("the bib buffer has no native form yet", effects)

    def test_the_zero_hour_road_traversal_mirrors_its_own_draw_roads(self):
        s = read("GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DRoadBuffer.cpp")
        native = s[s.index("Int W3DRoadBuffer::forEachNativeBatch"):s.index("// W3DRoadBuffer::drawRoads")]
        legacy = s[s.index("void W3DRoadBuffer::drawRoads"):]
        # same visibility refresh and buffer reload rule as the legacy draw
        for term in ("if (m_updateBuffers)", "visibilityChanged(bounds)", "m_updateBuffers = false;", "if (loadBuffers) loadRoadsInVertexAndIndexBuffers();",
                     "m_roadTypes[i].getStacking()", "getNumIndices() == 0"):
            self.assertIn(term, native)
            self.assertIn(term, legacy)
        self.assertIn("Is_CPU_Backed", native)

    def test_staging_and_launching_accept_zero_hour_without_the_generals_overlay(self):
        stage = read("scripts/native-runtime/Stage-NativeOrdinaryPlay.ps1")
        self.assertIn("ValidateSet('Generals','ZeroHour')", stage)
        self.assertIn("-ModernExtensions:(($Title -eq 'Generals') -and !$Classic)", stage)
        launch = read("scripts/native-runtime/Invoke-NativeNormalLaunch.ps1")
        self.assertIn("ValidateSet('Generals','ZeroHour')", launch)
        common = read("scripts/native-runtime/OrdinaryHostCommon.ps1")
        self.assertIn("if($Title -notin 'Generals','ZeroHour'){throw 'The ordinary play stage validates Generals and Zero Hour only'}", common)
        # the other profiles' readers stay Generals-only
        self.assertEqual(len(re.findall(r"validates Generals only", common)), 4)
        init = read("scripts/native-runtime/Stage-NativeInitOnly.ps1")
        self.assertIn("Modern extensions are Generals-only", init)

    def test_enhanced_hud_is_not_offered_to_zero_hour_sides(self):
        # The Enhanced HUD was reviewed against the Generals control bar only: Zero Hour keeps the Original bar (the option is inert).
        scheme = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp")
        zh = scheme.index("#if RTS_ZEROHOUR")
        gla = scheme.index("#else", zh)
        self.assertIn("const char *const enhancedSide = nullptr;", scheme[zh:gla])
        self.assertIn("const char *const enhancedSide = m_side.str();", scheme[gla:scheme.index("#endif", gla)])
        self.assertIn("ModernExperience::classic(), enhancedSide,", scheme)
        self.assertNotIn("classic(), m_side.str()", scheme)

    def test_the_terrain_atlas_version_is_unique_across_maps(self):
        # X64.45 first native run: black strips across the Zero Hour terrain. The terrain render object outlives its maps and keeps the uploaded
        # atlas keyed by WorldHeightMap::m_nativeAtlasVersion; a per-map counter is 1 for every freshly built map, so the map after the shell map
        # (same draw size, no buffer reallocation) kept the shell map's atlas and sampled it with the new map's layout.
        world = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/WorldHeightMap.cpp")
        self.assertNotIn("++m_nativeAtlasVersion", world)
        self.assertIn("static unsigned s_nativeAtlasSerial = 0;", world)
        self.assertIn("m_nativeAtlasVersion = ++s_nativeAtlasSerial;", world)
        heightmap = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp")
        self.assertIn("n.atlasVersion != atlasVersion", heightmap)   # the comparison the unique serial feeds

    def test_the_capture_aids_are_inert_without_their_environment_switch(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        body = host[host.index("void NativeAutoShot()"):]
        body = body[:body.index(chr(10) + "}" + chr(10))]
        head = body[:body.index("if (!backend) return;")]
        self.assertIn('std::getenv("GENERALS_AUTOSHOT")', head)
        self.assertIn("!directory || !*directory || !StartupDiagnostics::normalLaunch()", head)
        draw = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp")
        self.assertIn("NativeAutoShot();", draw)
        self.assertEqual(draw.count(chr(9) + "NativeAutoShot();"), 1)   # one call (the extern declaration is not indented)
        # the flow driver only exists behind its own switch and only ever uses the player's screens
        self.assertIn('std::getenv("GENERALS_AUTOFLOW")', body)
        self.assertIn('TheShell->push(step.a);', body)
        self.assertIn('"Menus/SkirmishGameOptionsMenu.wnd"', body)
        # X64.46: Campaign and Challenge go through the real MainMenu / ChallengeMenu buttons (GBM_SELECTED), never through world state
        for name in ("MainMenu.wnd:ButtonSinglePlayer", "MainMenu.wnd:ButtonUSA", "MainMenu.wnd:ButtonChallenge", "MainMenu.wnd:ButtonEasy",
                     "ChallengeMenu.wnd:GeneralPosition", "ChallengeMenu.wnd:ButtonPlay",
                     "QuitMenu.wnd:ButtonSaveLoad", "PopupSaveLoad.wnd:ButtonSave", "SaveLoad.wnd:ButtonLoad"):
            self.assertIn(name, body)
        self.assertIn("GBM_SELECTED", body)
        self.assertNotIn("MSG_NEW_GAME", body)
        self.assertNotIn("startNewGame", body)
        self.assertNotIn("TheGameLogic->setFrame", body)
        script = read("scripts/native-runtime/Invoke-NativeAutoCapture.ps1")
        self.assertIn("$env:GENERALS_AUTOFLOW = $Flow.ToLowerInvariant()", script)
        self.assertIn("'Skirmish','Campaign','Challenge','SaveLoad','Slots','MatchOptions','MainMenu','QuickGame'", script)

    def test_the_native_screenshot_path_keeps_its_separator(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn("%s" + chr(92) + chr(92) + "Generals64_%04u.bmp", host)   # a lone backslash-G is not an escape: the separator used to be dropped

    def test_launcher_default_candidate_is_the_documented_one(self):
        # The .cmd double-click passes no -Name: the default must be the candidate the handoff documents (a stale default once pointed at v1).
        ps1 = read("scripts/native-runtime/Run-ZeroHourCandidate.ps1")
        default = re.search(r"\[string\]\$Name = '([^']+)'", ps1).group(1)
        doc = read("docs/modernization/MODERN_UI_2_MAIN_MENU.md")   # the milestone that stages the current candidate
        self.assertIn("ZeroHour\\" + default + "\game\generalszh.exe", doc)
        self.assertIn("-Name " + default, doc)
        self.assertEqual(sorted(set(re.findall(r"(?:x64-\d+[a-z]?-zh-(?:[a-z]+-)*v\d+|dist-[0-9a-f]+-zh)", ps1))), [default])

    def test_the_candidate_launcher_can_start_classic_without_a_diagnostic_token(self):
        inv = read("scripts/native-runtime/Invoke-NativeNormalLaunch.ps1")
        self.assertIn("if ($Classic) { $info.ArgumentList.Add('-classic') }", inv)
        self.assertEqual(inv.count("ArgumentList.Add("), 1)
        self.assertNotIn("-native", inv.replace("-nativeOrdinary", "", 0).split("ArgumentList.Add(")[1].split(")")[0])

    def test_the_zero_hour_candidate_launcher_never_touches_the_steam_installation(self):
        ps1 = read("scripts/native-runtime/Run-ZeroHourCandidate.ps1")
        self.assertIn("Invoke-NativeNormalLaunch.ps1", ps1)
        self.assertIn("-Title ZeroHour", ps1)
        for forbidden in ("Copy-Item", "Remove-Item", "Move-Item", "Set-Content", "Out-File"):
            if forbidden in ps1:
                # only the settings snapshot may copy, and only into build\dev-runtimes\userdata-backups
                self.assertEqual(forbidden, "Copy-Item")
                self.assertIn("userdata-backups", ps1)
        self.assertNotIn("Program Files", ps1)
        self.assertNotIn("steamapps", ps1)


    def test_unavailable_movies_are_skipped_not_dereferenced(self):
        # X64.46: with no decoder TheVideoPlayer->open() returns null. ChallengeLoadScreen (Zero Hour only) used to read the
        # stream's size unconditionally (access violation on Win64); it must skip the movie and keep the static challenge presentation.
        load = read("Core/GameEngine/Source/GameClient/GUI/LoadScreen.cpp")
        start = load.index("void ChallengeLoadScreen::init(")
        body = load[start:load.index("void ChallengeLoadScreen::reset()")]
        self.assertIn("const Bool movieUnavailable = (m_videoStream == nullptr);", body)
        self.assertIn("!movieUnavailable && (m_videoBuffer == nullptr", body)
        self.assertIn("activatePiecesMinSpec(generalPlayer, generalOpponent);", body)
        # every movie that cannot be opened is recorded once in generals64.log
        bink = read("Core/GameEngineDevice/Source/VideoDevice/Bink/BinkVideoPlayer.cpp")
        self.assertIn('StartupDiagnostics::checkpoint("movie", "skipped", movieTitle.str());', bink)


if __name__ == "__main__":
    unittest.main()
