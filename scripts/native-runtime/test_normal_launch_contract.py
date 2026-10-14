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
        self.assertIn("if (speedLabel)", c)

    def test_crash_dumps_are_preserved_and_still_fail_the_run(self):
        inv = read("scripts/native-runtime/Invoke-NativeNormalLaunch.ps1")
        self.assertIn("Restore-NativeRuntimeCrashDumps", inv)
        self.assertIn("if ($crashDumps.Count) { throw", inv)
        common = read("scripts/native-runtime/Common.ps1")
        # only exact MiniDumper names qualify; the strict stage check is never relaxed globally
        self.assertIn("Crash(FG|MG)-", common)
        self.assertIn("[switch]$AllowCrashDumps", common)


if __name__ == "__main__":
    unittest.main()
