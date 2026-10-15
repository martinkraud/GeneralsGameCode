"""X64.35 baseline2 source contracts: the diagnostic instrumentation must stay measurement-only.

* GPU timestamp queries are off by default and are only polled with DONOTFLUSH (never a blocking/spinning read);
* the dev frame-rate override is explicit, developer-only and defaults to the historical 120;
* the receipt keeps every earlier field and the creation label is cleared after use.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def text(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


BACKEND = "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.cpp"


class Baseline2Contracts(unittest.TestCase):
    def test_gpu_timing_is_opt_in_and_nonblocking(self):
        s = text(BACKEND)
        self.assertIn("bool gpuEnabled = false", s)
        for call in re.findall(r"GetData\([^;]*;", s):
            self.assertIn("D3D11_ASYNC_GETDATA_DONOTFLUSH", call)
        self.assertNotIn("while (context->GetData", s)          # no spin on a query
        host = text("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn("Set_Gpu_Timing(true)", host)
        self.assertIn("Set_Gpu_Timing(false)", host)

    def test_present_and_submit_are_only_timed_not_changed(self):
        s = text(BACKEND)
        self.assertIn("swapchain->Present(impl->config.vsync ? 1 : 0, 0)", s)   # arguments unchanged
        self.assertIn("ScopedTicks timing(impl->stats.present_ticks)", s)
        self.assertIn("ScopedTicks timing(impl->stats.submit_ticks)", s)

    def test_dev_fps_override_default_off(self):
        s = text("Core/GameEngine/Source/Common/DeveloperInteractive.cpp")
        self.assertIn("int fpsOverride=0;", s)
        self.assertIn("return fpsOverride?fpsOverride:120;", s)
        self.assertNotIn("setFramesPerSecondLimit(120)", s)      # the literal is now scenarioFpsLimit()
        cl = text("Core/GameEngine/Source/Common/CommandLine.cpp")
        self.assertIn('"-devFps"', cl)

    def test_receipt_keeps_earlier_fields_and_adds_new(self):
        s = text("Core/GameEngine/Source/Common/PerformanceProfile.cpp")
        for field in ("avg_draw_calls", "avg_draws_world", "avg_requested", "avg_texture_binds", "hardware: ", "render_context: ",
                      "fps_cap_override: ", "timing_ms_per_frame", "gpu: frames_measured", "textures: created", "memory: page_faults"):
            self.assertIn(field, s)

    def test_texture_creation_label_is_cleared(self):
        s = text("Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11TextureBridge.cpp")
        self.assertIn("texture.Get_Full_Path().str()", s)
        self.assertIn("Get_Diagnostic_Label()", s)
        self.assertIn("Set_Creation_Label(nullptr)", s)

    def test_attribution_scopes_exist_and_zone_calc_behavior_untouched(self):
        for rel, scope in (
            ("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp", "NativeEndRender"),
            ("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp", "NativeUi"),
            ("Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp", "NativeTerrain"),
            ("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldObjects.cpp", "NativeObjProcess"),
            ("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldEffects.cpp", "NativeFxParticles"),
            ("Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp", "NativeDevOverlay"),
            ("GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp", "NativeDevOverlay"),
        ):
            self.assertIn(scope, text(rel))
        ai = text("Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp")
        self.assertIn("PERFORMANCE_PROFILE_SCOPE(ZoneCalc)", ai)
        # the original statement sequence is unchanged: calculate, then return
        self.assertRegex(ai, r"calculateZones\(m_map, m_layers, m_extent\);\s*\}\s*return;")

    def test_shadow_attribution_scopes_and_cache_reverted(self):
        fx = text("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldEffects.cpp")
        for scope in ("NativeShadowGather", "NativeShadowProject", "NativeShadowEmit", "NativeShadowDecal"):
            self.assertIn("PerformanceProfile::Category::" + scope + ")" if scope == "NativeShadowProject" else scope, fx)
        # the rejected shadow-volume cache (C-S1: no benefit) is gone
        self.assertNotIn("g_volumeCache", fx)
        self.assertNotIn("NativeShadowVolumeCache", fx)
        import pathlib
        self.assertFalse((ROOT / "Core/GameEngineDevice/Include/W3DDevice/GameClient/NativeShadowVolumeCache.h").exists())


if __name__ == "__main__":
    unittest.main()
