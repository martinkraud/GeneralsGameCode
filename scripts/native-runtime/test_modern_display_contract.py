"""X64.33 Modern Experience display contracts (source level; never runs the game).

These pin the properties that make 4:3 / 16:9 / 21:9 / 32:9 ordinary configurations without weakening Classic:
the modern path is reachable only through ModernExperience::FEATURE_MODERN_DISPLAY in a normal launch, Classic and
the diagnostic profiles keep the accepted fixed 800x600 logical display, there is exactly one frame limiter, and
Zero Hour / the Win32 reference are not touched.
"""
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
X64_32_ACCEPTED = "a2b9f0a04"      # first X64.33 commit parent: X64.32 closeout


def read(path):
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")


def function_body(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        depth += text[i] == "{"
        depth -= text[i] == "}"
        if depth == 0:
            return text[start:i + 1]
    raise AssertionError("unbalanced: " + signature)


HOST = "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp"


class ModernDisplayContracts(unittest.TestCase):
    def test_feature_gate_is_the_only_switch_and_classic_keeps_800x600(self):
        header = read("Core/GameEngine/Include/Common/ModernExperience.h")
        self.assertIn("FEATURE_MODERN_DISPLAY", header)
        self.assertIn("FEATURE_SMOOTH_MOTION", header)
        host = read(HOST)
        self.assertIn("StartupDiagnostics::normalLaunch() && ModernExperience::enabled(ModernExperience::FEATURE_MODERN_DISPLAY)", host)
        winmain = read("Generals/Code/Main/WinMain.cpp")
        self.assertIn("StartupDiagnostics::normalLaunch() && ModernExperience::enabled(ModernExperience::FEATURE_MODERN_DISPLAY)", winmain)
        init = function_body(host, "void W3DDisplay::initOrdinaryHostPresentation()")
        # The classic normal-launch coercion and the exact-800x600 gate remain, reachable when not modern.
        self.assertIn("TheWritableGlobalData->m_xResolution = 800;", init)
        self.assertIn("if (!modern && (!TheGlobalData->m_windowed", init)
        # The accepted receipts keep their classic text.
        self.assertIn('"windowed=1 %dx%dx32 ApplicationHWnd"', init)

    def test_classic_defaults_and_modern_defaults(self):
        header = read("Core/GameEngine/Include/Common/DisplaySettings.h")
        classic = header[header.index("inline Settings classicDefaults()"):]
        classic = classic[:classic.index("}\n")]
        self.assertIn("kReferenceSize", classic)
        self.assertIn("uiScalePercent = 100", classic)
        load = function_body(header, "inline Settings load(const KeyValues &kv, bool classic)")
        self.assertLess(load.index("if (classic)"), load.index("DisplayVersion"))

    def test_runtime_hooks_are_registered_only_in_the_modern_branch(self):
        host = read(HOST)
        branch = function_body(host, "void W3DDisplay::initOrdinaryHostPresentation()")
        modern = branch[branch.index("if (modern) {"):branch.index("} else if (StartupDiagnostics::normalLaunch())")]
        self.assertIn("DisplayRuntime::applyModeHook()", modern)
        self.assertIn("DisplayRuntime::vsyncHook()", modern)
        self.assertNotIn("DisplayRuntime::applyModeHook()", branch.replace(modern, ""))

    def test_parse_screen_rect_scaling_is_uniform_and_gated(self):
        text = read("Core/GameEngine/Source/GameClient/GUI/GameWindowManagerScript.cpp")
        body = function_body(text, "static Bool parseScreenRect(")
        self.assertIn("DisplayRuntime::modern()", body)
        self.assertIn("DisplayGeometry::anchorFor", body)
        self.assertIn("layoutKindForFile", body)
        self.assertIn("referenceToPhysical", body)
        # The original anisotropic stretch is still the else branch.
        self.assertIn("Real xScale = (Real)TheDisplay->getWidth() / (Real)createRes.x;", body)
        self.assertLess(body.index("DisplayRuntime::modern()"), body.index("Real xScale"))

    def test_camera_uses_hor_plus_only_when_modern(self):
        text = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp")
        body = function_body(text, "void W3DView::setWidth(Int width)")
        self.assertIn("if (DisplayRuntime::modern()", body)
        self.assertIn("horizontalFovForAspect", body)
        self.assertIn("DEG_TO_RADF(50.0f)", body)

    def test_one_frame_limiter_and_vsync_is_a_present_interval(self):
        backend = read("Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.cpp")
        self.assertIn("Present(impl->config.vsync ? 1 : 0, 0)", backend)
        popup = read("Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp")
        for forbidden in ("FrameRateLimit", "Sleep(", "QueryPerformanceCounter", "setFramesPerSecondLimit"):
            self.assertNotIn(forbidden, popup)
        # FPS limit stays on the existing FrameRateOptions / FramePacer path.
        fr = read("Core/GameEngine/Source/GameClient/GUI/FrameRateOptions.cpp")
        self.assertIn("TheFramePacer->setFramesPerSecondLimit(fps);", fr)

    def test_mouse_is_identity_when_modern(self):
        # Modern: window client == logical display == swap chain (no review scale), so Win32Mouse needs no mapping.
        host = read("Core/GameEngine/Include/Common/NativeOrdinaryHost.h")
        self.assertIn("DisplayRuntime::expectedClient()", host)
        winmain = read("Generals/Code/Main/WinMain.cpp")
        self.assertIn("StartupDiagnostics::ordinaryPlay() && !modernDisplay", winmain)

    def test_zero_hour_and_win32_reference_are_untouched(self):
        diff = subprocess.run(["git", "diff", "--stat", X64_32_ACCEPTED, "--", "GeneralsMD",
                               "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayPresentation.cpp",
                               "Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.cpp"],
                              cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(diff.returncode, 0, diff.stderr)
        self.assertEqual(diff.stdout.strip(), "", diff.stdout)

    def test_options_are_runtime_gadgets_not_a_new_toolkit(self):
        for path in ("Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp",
                     "Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp"):
            text = read(path)
            for forbidden in ("imgui", "ImGui", "Qt", "CreateWindowEx", "MessageBox"):
                self.assertNotIn(forbidden, text)
        display = read("Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp")
        for token in ("gogoGadgetComboBox", "gogoGadgetStaticText", "copyAppearance"):
            self.assertIn(token, display)
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        for token in ("gogoGadgetPushButton", "tab->winSetDrawFunc(tabDraw);", "GadgetButtonSetText"):
            self.assertIn(token, tabs)
        cmake = read("Core/GameEngine/CMakeLists.txt")
        self.assertIn("ModernDisplayOptions.cpp", cmake)
        self.assertIn("OptionsTabs.cpp", cmake)

    def test_deltas_module_lists_only_reviewed_files(self):
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import x64_33_deltas
        self.assertEqual(sorted(x64_33_deltas.DELTAS), [
            "Core/Libraries/Source/WWVegas/WW3D2/PresentationLifecycle.cpp",
            "Core/Libraries/Source/WWVegas/WW3D2/PresentationLifecycle.h",
            "Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp",
        ])

class FixOneContracts(unittest.TestCase):
    """x64-33-fix1-ui / fix2 native review findings."""

    def test_runtime_gadgets_are_placed_in_the_draw_order_not_left_at_the_front(self):
        layering = read("Core/GameEngine/Include/GameClient/GadgetLayering.h")
        self.assertIn("insertWindowAheadOf", layering)
        self.assertIn("HEAD is the front", layering)
        rate = read("Core/GameEngine/Source/GameClient/GUI/FrameRateOptions.cpp")
        self.assertIn("GadgetLayering::placeBehind(combo, detail);", rate)       # retail split (Classic, Zero Hour)
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        # pages: upper rows are brought to the front last, so a dropdown opens over the rows below it
        self.assertIn("for (size_t i = frontToBack.size(); i-- > 0;)", tabs)
        self.assertIn("frontToBack[i]->winBringToTop();", tabs)

    def test_options_pages_geometry_is_scale_derived(self):
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        self.assertIn("scaled(44, scale)", layout)
        self.assertIn("kContent", layout)
        display = read("Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp")
        for stale in ("rowY0", "pitch = 52", "const Int pad = 24", "popup"):
            self.assertNotIn(stale, display)

    def test_hud_is_a_centred_core_not_a_stretched_band(self):
        scheme = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp")
        self.assertIn("HudComposition::compute(", scheme)
        self.assertIn("drawFillRect", scheme)
        for gone in ("GameWinBlockInput", "extensionSlices", "HudExtensionLayout", "updateExtensionBlockers"):
            self.assertNotIn(gone, scheme)       # the world beside the core is the world: no input wall, no stretch
        bar = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp")
        self.assertIn("TheTacticalView->setHeight(TheDisplay->getHeight());", bar)
        layout = read("Core/GameEngine/Include/Common/HudComposition.h")
        for text in (scheme, layout, read("Core/GameEngine/Include/Common/DisplayRuntime.h"),
                     read("Core/GameEngine/Include/Common/DisplayGeometry.h")):
            for literal in ("5120", "3440", "2560", "3840", "1920", "1440"):
                self.assertNotIn(literal, text)       # no monitor-specific branch anywhere in the layout code

    def test_static_text_margins_follow_the_ui_scale(self):
        text = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DStaticText.cpp")
        self.assertIn("DisplayRuntime::uiScale()", text)
        self.assertIn("wordWrap = size.x - (Int)(10 * marginScale + 0.5f);", text)

    def test_timers_use_the_hud_anchor_helpers(self):
        ui = read("Generals/Code/GameEngine/Source/GameClient/InGameUI.cpp")
        self.assertIn("DisplayRuntime::hudX(m_superweaponPosition.x, TheDisplay->getWidth())", ui)
        self.assertIn("DisplayRuntime::hudX(m_namedTimerPosition.x, TheDisplay->getWidth())", ui)
        self.assertNotIn("m_superweaponPosition.x * TheDisplay->getWidth()", ui)

    def test_options_categories_hook_up_only_in_modern(self):
        menu = read("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp")
        self.assertIn("createFrameRateOptions(comboBoxDetail, *pref, DisplayRuntime::modern())", menu)
        self.assertIn("OptionsTabs::create();", menu)
        self.assertIn("OptionsTabs::onButton(controlID)", menu)
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        self.assertIn("if (!DisplayRuntime::modern() || ModernExperience::classic())", tabs)
        pages = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        for category in ("kDisplayItems", "kGraphicsItems", "kAudioItems", "kControlsItems", "kGameplayItems"):
            self.assertIn(category, pages)
        self.assertIn("NetworkParent", tabs)
        # no fake controls: every listed control is a retail control or a gadget ModernDisplayOptions/FrameRateOptions creates
        known = read("Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp") +             read("Core/GameEngine/Source/GameClient/GUI/FrameRateOptions.cpp")
        for control in ("ModernMode", "ModernMonitor", "ModernUiScale", "ModernVsync", "ModernMotion", "ComboBoxFrameRateLimit"):
            self.assertIn(control, known)
        for fake in ("RefreshRate", "Exclusive", "RenderScale", "DLSS", "RayTrac"):
            self.assertNotIn(fake, tabs)


class FixThreeContracts(unittest.TestCase):
    """x64-33-fix3-ui-polish: correctness of the accepted FIX2 design."""

    def test_page_visibility_is_decided_for_the_whole_layout_tree(self):
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        self.assertIn("bool apply(GameWindow *w, const Visibility &v, bool inNetwork)", tabs)
        self.assertIn("apply(s_root, v, false);", tabs)
        self.assertIn("while (s_root->winGetParent())", tabs)        # the root of the layout, not a known panel list
        # gadgets are never recursed into (their combo/slider children are not page items)
        self.assertIn("if (isContainer(w) || w == s_root)", tabs)
        # the retail Advanced Display panel manages itself
        self.assertIn("if (w == s_popup)", tabs)
        # the FIX2 per-panel child loop (the cause of the stale slider art) is gone
        self.assertNotIn("not an item of this page", tabs)

    def test_page_ownership_table_is_pure_and_exclusive(self):
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        self.assertIn("inline int ownerOf(const char *name)", layout)
        self.assertIn("enum class ControlKind", layout)
        for slider in ("SliderGamma", "SliderSFXVolume", "SliderMusicVolume", "SliderVoiceVolume", "SliderScrollSpeed"):
            self.assertEqual(layout.count('"%s"' % slider), 1)         # each slider listed on exactly one page

    def test_tab_strip_is_measured_and_drawn_from_primitives(self):
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        self.assertIn("inline TabStrip computeTabStrip(", layout)
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        self.assertIn("computeTabStrip(measured,", tabs)
        self.assertIn("tab->winSetDrawFunc(tabDraw);", tabs)
        self.assertIn("drawFillRect(x, y, w, h, fill)", tabs)          # opaque dark fill: nothing shows through a tab
        self.assertNotIn("copyAppearance(tab, donor)", tabs)           # no more large retail button art

    def test_options_pages_are_built_after_every_retail_show_hide_decision(self):
        menu = read("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp")
        create = menu.index("OptionsTabs::create();")
        self.assertGreater(create, menu.index("textEntryHTTPProxy->winHide(TRUE);"))
        self.assertLess(create, menu.index("layout->hide( FALSE );", create - 200))

    def test_hud_backing_is_derived_from_the_band(self):
        layout = read("Core/GameEngine/Include/Common/HudComposition.h")
        self.assertIn("const int backTop = bandY + bandH - backH;", layout)
        self.assertIn("inline bool backingVisible(", layout)
        self.assertNotIn("surfaceH - backH", layout)                   # FIX2 anchored it to the surface (black flash)
        scheme = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp")
        self.assertIn("composition.backing", scheme)


if __name__ == "__main__":
    unittest.main()
