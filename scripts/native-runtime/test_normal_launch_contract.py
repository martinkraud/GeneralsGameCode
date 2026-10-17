"""X64.31 normal-launch contract: source pins, never executes a title."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")


DIAG = "Core/GameEngine/Include/Common/StartupDiagnostics.h"


class NormalLaunchContract(unittest.TestCase):
    def test_normal_launch_is_amd64_only_and_token_free(self):
        d = read(DIAG)
        self.assertIn("inline bool normalLaunch()", d)
        # Normal launch is entered only when no diagnostic token was requested, and only on the 64 bit build.
        cfg = d[d.index("if (!s.requested)"):]
        self.assertIn("_WIN64", d[d.index("if (!s.requested)") - 400:d.index("if (!s.requested)") + 400])
        self.assertIn("s.normal = true;", cfg[:1200])
        self.assertIn("generals64.log", d)
        # Test hosts that include the header must never be classified as a normal launch.
        self.assertIn('_strnicmp(base, "generalsv", 9)', d)

    def test_every_ordinary_predicate_is_true_for_normal_launch(self):
        d = read(DIAG)
        for name in ("ordinaryHost", "ordinaryMenu", "ordinaryGame", "ordinaryTerrain", "ordinaryPlay"):
            m = re.search(r"inline bool %s\(\)[^\n]*\n?[^\n]*" % name, d)
            self.assertIsNotNone(m, name)
            self.assertIn("state().normal", m.group(0), name)

    def test_normal_log_is_bounded_and_never_fails_the_game(self):
        d = read(DIAG)
        self.assertIn("4096", d)
        self.assertIn("normalLog", d)

    def test_command_line_engages_headless_overrides_only_for_normal_launch(self):
        c = read("Core/GameEngine/Source/Common/CommandLine.cpp")
        self.assertIn("StartupDiagnostics::normalLaunch()", c)
        self.assertIn("if(ordinaryGame() && !normalLaunch())", c.replace("StartupDiagnostics::", ""))

    def test_edge_scroll_default_includes_windowed_on_amd64(self):
        s = read("Core/GameEngine/Include/GameClient/LookAtXlat.h")
        self.assertIn("ScreenEdgeScrollMode_EnabledInFullscreenApp | ScreenEdgeScrollMode_EnabledInWindowedApp", s)
        self.assertIn("_WIN64", s)

    def test_audio_runs_on_native_ail_not_dummy(self):
        loader = read("Dependencies/Miles/MilesLoader.cpp")
        self.assertIn("bindNative()", loader)
        self.assertIn("NativeMiles::AIL_startup", loader)
        engine = read("Core/GameEngineDevice/Include/Win32Device/Common/Win32GameEngine.h")
        self.assertIn("dummy && !StartupDiagnostics::normalLaunch()", engine)
        cmake = read("Dependencies/Miles/CMakeLists.txt")
        self.assertIn("NativeMiles.cpp", cmake)
        # Every AIL function the loader forwards, except the waveOut pair, is bound natively.
        resolved = set(re.findall(r"MILES_RESOLVE\((AIL_\w+),", loader))
        bound = set(re.findall(r"(\w+)Ptr = &NativeMiles::\1;", loader))
        self.assertEqual(resolved - bound, {"AIL_waveOutOpen", "AIL_waveOutClose"})

    def test_display_modes_and_screenshot_are_native(self):
        w = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp")
        self.assertIn("saveNativeScreenshot", w)
        self.assertIn("xres == 800 && yres == 600", w.replace("static_cast<int>(", "").replace("(Int)", ""))

    def test_launcher_preserves_log_and_reverifies_strictly(self):
        inv = read("scripts/native-runtime/Invoke-NativeNormalLaunch.ps1")
        self.assertIn("Restore-NativeRuntimeGeneratedFiles", inv)
        # strict re-read (no allowance) after cleanup
        self.assertIn("Read-NativeOrdinaryPlayStage $Title $Name }", inv)
        common = read("scripts/native-runtime/Common.ps1")
        self.assertIn("$script:NativeNormalLaunchGenerated=@('generals64.log')", common)
        # default Read-NativeStage still allows nothing
        self.assertIn("[string[]]$AllowAdded=@()", common)


    # ---- X64.31 fix2 (first native review crash) ----
    def test_file_texture_bridge_accepts_mip_chains_and_never_throws_for_missing_files(self):
        b = read("Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11TextureBridge.cpp")
        self.assertNotIn("texture.MipLevelCount != MIP_LEVELS_1", b)
        self.assertNotIn("legacy MissingTexture GPU fallback is not migrated", b)
        self.assertIn("Note_Missing_Texture", b)
        self.assertIn("0x7FFF00FF", b)  # legacy MissingTexture pixels
        t = read("Core/Libraries/Source/WWVegas/WW3D2/texture.cpp")
        self.assertIn("level == 0 && Render2DCPUContent && !Peek_D3D_Texture()", t)

    def test_pooled_sample_handles_do_not_keep_the_previous_clips_playback_rate(self):
        m = read("Dependencies/Miles/NativeMiles.cpp")
        self.assertIn("v->playbackRate = 0;", m[m.index("bool LoadSampleData"):])

    def test_skirmish_game_speed_label_is_hidden_with_its_slider_by_name(self):
        c = read("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp")
        self.assertIn("StaticTextBattleHonors1", c)
        self.assertIn("if (speedLabel && !comboBoxStartingCash)", c)  # label hidden by name unless Starting Cash took the slot

    def test_crash_dumps_are_preserved_and_still_fail_the_run(self):
        inv = read("scripts/native-runtime/Invoke-NativeNormalLaunch.ps1")
        self.assertIn("Restore-NativeRuntimeCrashDumps", inv)
        self.assertIn("if ($crashDumps.Count) { throw", inv)
        common = read("scripts/native-runtime/Common.ps1")
        # only exact MiniDumper names qualify; the strict stage check is never relaxed globally
        self.assertIn("Crash(FG|MG)-", common)
        self.assertIn("[switch]$AllowCrashDumps", common)


    # ---- X64.31 fix3 ----
    def test_audio_completion_handles_are_pointer_sized(self):
        # Root cause of SFX/voices dying over a long session: completion callbacks truncated the 64 bit sample
        # handle to 32 bits, findPlayingAudioFrom never matched, and the pooled handles were never recycled.
        m = read("Core/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp")
        for cast in ("(UnsignedInt) sampleCompleted", "(UnsignedInt) sample3DCompleted", "(UnsignedInt) streamCompleted"):
            self.assertNotIn(cast, m)
        self.assertIn("notifyOfAudioCompletion((UnsignedPtr) sampleCompleted, PAT_Sample)", m)
        self.assertIn("PlayingAudio *MilesAudioManager::findPlayingAudioFrom( UnsignedPtr handle", m)
        self.assertIn("notifyOfAudioCompletion( UnsignedPtr audioCompleted", read("Core/GameEngine/Include/Common/GameAudio.h"))
        self.assertIn("typedef uintptr_t UnsignedPtr;", read("Core/Libraries/Include/Precompiled/Precompiled/BaseTypes.h"))

    def test_native_audio_completion_is_off_the_mixer_thread_and_counted(self):
        n = read("Dependencies/Miles/NativeMiles.cpp")
        self.assertIn("EosWorker", n)
        self.assertIn("++g_stats.starved", n)
        self.assertIn("THREAD_PRIORITY_HIGHEST", n)
        # sample decode happens before the mixer lock is taken
        body = n[n.index("bool LoadSampleData"):n.index("bool LoadSampleData") + 900]
        self.assertLess(body.index("DecodeWavToPcm"), body.index("Lock lock;"))
        # past the maximum distance a sound is clamped, never silenced
        self.assertNotIn("if (dist > v.maxDist) attenuation = 0.0f;", n)
        self.assertIn("audio_stats", read("Core/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp"))

    def test_starting_cash_extension_wiring(self):
        c = read("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp")
        self.assertIn("static Bool startingCashExtensionEnabled()", c)
        self.assertIn("{ 5000, 10000, 20000, 50000 }", c)                   # Zero Hour choices
        self.assertIn("TheGlobalData->m_defaultStartingCash", c)            # retail amount is the default
        # every menu entry resets to the retail amount before a stored (validated) choice is applied
        self.assertLess(c.index("TheSkirmishGameInfo->setStartingCash(TheGlobalData->m_defaultStartingCash);"),
                        c.index("isStartingCashChoice((Int)stored.countMoney())"))
        self.assertIn("setStartingCash( TheSkirmishGameInfo->getStartingCash() );", c)  # persisted
        self.assertIn("handleStartingCashSelection();", c)
        # the retail multiplayer-settings default is zero for Generals: never used as the default
        self.assertNotIn("return TheMultiplayerSettings->getDefaultStartingMoney();", c)
        # campaign: only a skirmish game binds TheGameInfo to the skirmish info that carries the amount
        g = read("Generals/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp")
        self.assertIn("else if(m_gameMode == GAME_SKIRMISH)", g)
        p = read("Generals/Code/GameEngine/Source/Common/RTS/Player.cpp")
        self.assertIn("TheGameInfo->getStartingCash().countMoney()", p)
        # GameData.ini is never rewritten by the selection
        self.assertNotIn("m_defaultStartingCash =", c)


    def test_proven_review_paths_stay_wired(self):
        # Proven by the x64-31-fix2 native review: load/score screens, return to the shell, movie entry points and
        # busy state key on normal launch rather than m_headless alone.
        nh = "TheGlobalData->m_headless && !StartupDiagnostics::normalLaunch()"
        for path in ("Generals/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp",
                     "Core/GameEngine/Source/GameLogic/System/GameLogicDispatch.cpp",
                     "Generals/Code/GameEngine/Source/GameClient/InGameUI.cpp",
                     "Core/GameEngine/Source/GameClient/Display.cpp"):
            self.assertIn(nh, read(path), path)
        # texture bridge: placeholder instead of exceptions (laser/mip-chain crash)
        self.assertIn("MissingTextureContent()", read("Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11TextureBridge.cpp"))
        # real audio manager on normal launch
        self.assertIn("dummy && !StartupDiagnostics::normalLaunch()",
                      read("Core/GameEngineDevice/Include/Win32Device/Common/Win32GameEngine.h"))


    def test_starting_cash_layout_follows_the_setup_grid_and_hides_every_game_speed_remnant(self):
        c = read("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp")
        # parented to the header's own parent (the inset sub-panel); the fix3 control landed ~41 px off
        self.assertIn("GameWindow *parent = caption->winGetParent();", c)
        self.assertNotIn("createStartingCashControl(parentSkirmishGameOptions", c)
        # X64.33 FIX1: header-aligned Match Options rows, one 24 px grid row tall, flush under the header; the
        # geometry now comes from MatchOptionsLayout (header left edge, grid rhythm) instead of fixed pixels
        self.assertIn("matchOptionsLayoutFor(caption, appearance, FALSE)", c)
        self.assertIn("layout.control[0].x", c)
        self.assertIn("layout.control[0].y", c)
        self.assertIn("createStartingCashControl(sliderGameSpeed, speedLabel, comboBoxTeam[0], comboBoxPlayer[1])", c)
        # remnants are identified geometrically (unnamed static texts), never by localized text
        self.assertIn("static Int hideGameSpeedRemnants(", c)
        self.assertIn("hideGameSpeedRemnants(parentSkirmishGameOptions, screenLayout, speedLabel, sliderGameSpeed, nullptr)", c)
        self.assertNotIn("text == slower", c)
        self.assertIn('checkpoint("skirmish_ui", "layout"', c)

    def test_listener_orientation_convention_is_documented_and_used(self):
        n = read("Dependencies/Miles/NativeMiles.cpp")
        self.assertIn("right = up x forward", n)
        self.assertIn("float r[3] = {u[1] * f[2] - u[2] * f[1], u[2] * f[0] - u[0] * f[2], u[0] * f[1] - u[1] * f[0]};", n)
        m = read("Core/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp")
        self.assertIn("m_listenerOrientation.z, 0, 0, -1)", m)  # engine passes up = (0,0,-1)

    def test_delivery_is_traced_for_diagnosis(self):
        d = read("Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/DeliverPayloadAIUpdate.cpp")
        self.assertIn('checkpoint("delivery", "trace"', d)
        self.assertIn('checkpoint("delivery", "start"', d)
        self.assertIn("traced < 600", d)  # fix5: per-aircraft budget of 40 inside a 600 record ceiling
        a = read("Generals/Code/GameEngine/Source/GameLogic/AI/AIGroup.cpp")
        self.assertIn('checkpoint("special_power", "dispatch"', a)


    # ---- X64.32 ----
    def test_modern_experience_gate_is_single_and_used(self):
        h = read("Core/GameEngine/Include/Common/ModernExperience.h")
        self.assertIn("enum Feature", h)
        self.assertIn("FEATURE_STARTING_CASH", h)
        c = read("Core/GameEngine/Source/Common/CommandLine.cpp")
        self.assertIn('{ "-classic", parseClassic }', c)
        m = read("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp")
        self.assertIn("ModernExperience::enabled(ModernExperience::FEATURE_STARTING_CASH)", m)

    def test_zero_hour_save_load_fixes_are_ported(self):
        s = read("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/PopupSaveLoad.cpp")
        # ESC must kill the delete confirmation before the dialog closes (retail crash)
        i = s.index("deleteConfirm->winHide( TRUE );")
        self.assertLess(i, s.index("TheWindowManager->winSendSystemMsg( window, GBM_SELECTED,", i - 200))
        self.assertIn("listboxGames->winEnable( TRUE );", s[i:i + 200])
        self.assertIn("defaultDesc.truncateBy(4);", s)

    def test_staging_applies_the_overlay_unless_classic(self):
        a = read("scripts/native-runtime/Stage-NativeOrdinaryPlay.ps1")
        # X64.45: the shortcut-sidebar overlay adapts Generals to Zero Hour's design; Zero Hour already ships it and never gets one.
        self.assertIn("-ModernExtensions:(($Title -eq 'Generals') -and !$Classic)", a)
        b = read("scripts/native-runtime/Stage-NativeInitOnly.ps1")
        self.assertIn("modern_extensions.py", b)
        # the overlay is written before the manifest inventory is taken (so the integrity gate covers it)
        self.assertLess(b.index("modern_extensions.py"), b.index("Inventory=@(Get-ZHInventory $game)"))


if __name__ == "__main__":
    unittest.main()
