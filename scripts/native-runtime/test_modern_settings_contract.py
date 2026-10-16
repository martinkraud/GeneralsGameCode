#!/usr/bin/env python3
"""X64.37 contracts: the Modern display transaction, persistence rules and the control audit (source-level, no native run)."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
NL = chr(10)


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace").replace(chr(13) + chr(10), NL)


OPTIONS = "Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp"
MENU = "Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp"
HOST = "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp"


class ModernSettingsContracts(unittest.TestCase):
    def test_transaction_is_pure(self):
        tx = read("Core/GameEngine/Include/Common/DisplayTransaction.h")
        for forbidden in ("windows.h", "GameWindow", "TheDisplay", "TheShell", "GetTickCount", "SetWindowPos", "D3D11"):
            self.assertNotIn(forbidden, tx)
        for token in ("class Transaction", "Outcome::SafeFallback", "safeFallback()", "bool keep(", "Outcome revert(", "bool tick("):
            self.assertIn(token, tx)

    def test_risky_fields_persist_only_after_keep(self):
        src = read(OPTIONS)
        # a pending change writes the IMMEDIATE plan (risky fields unchanged); only the keep callback writes afterKeep
        self.assertIn("writeInto(preferences, plan.immediate);", src)
        keep = src[src.index("void keepCallback()"):src.index("void revertCallback()")]
        self.assertIn("persist(accepted);", keep)
        self.assertIn("s_plan.afterKeep", keep)
        revert = src[src.index("void revertCallback()"):src.index("namespace ModernDisplayOptions")]
        self.assertNotIn("persist(", revert)
        self.assertIn("s_tx.revert(applyConfig)", revert)
        # the timeout revert happens in update(), driven by the shell, and closes the box before rebuilding the layouts
        upd = src[src.index("void update()"):src.index("void forget()")]
        self.assertIn("s_tx.tick(now, applyConfig, &outcome)", upd)
        self.assertLess(upd.index("closeBox();"), upd.index("relayoutAll();"))
        shell = read("Core/GameEngine/Source/GameClient/GUI/Shell/Shell.cpp")
        self.assertIn("ModernDisplayOptions::update();", shell)

    def test_one_transaction_covers_size_mode_monitor_and_scale(self):
        src = read(OPTIONS)
        self.assertIn("s_tx.begin(good, want, applyConfig, nowMs())", src)
        for field in ("want.monitorIndex", "want.monitorName", "want.mode", "want.client", "want.uiScalePercent"):
            self.assertIn(field, src)
        menu = read(MENU)
        # the retail resolution path (and its dialog) is Classic only; Modern goes through the transaction
        self.assertIn("if (!DisplayRuntime::modern() && comboBoxResolution && comboBoxResolution->winGetEnabled()", menu)
        self.assertIn("ModernDisplayOptions::confirmationPending()", menu)
        self.assertIn("ModernDisplayOptions::populateResolution(comboBoxResolution);", menu)
        self.assertIn("ModernDisplayOptions::onSelectionChanged(control);", menu)

    def test_original_and_classic_are_untouched(self):
        src = read(OPTIONS)
        for fn in ("void populateResolution(GameWindow *combo)", "bool apply(OptionPreferences &preferences, bool layoutsAlreadyRebuilt)"):
            body = src[src.index(fn):]
            self.assertIn("!DisplayRuntime::modern()", body[:400])
        upd = src[src.index("void update()"):]
        self.assertIn("if (s_tx.phase() != Phase::AwaitingConfirmation)", upd[:200])

    def test_live_change_updates_the_logical_display(self):
        host = read(HOST)
        apply = host[host.index("bool NativeApplyDisplayMode("):host.index("// X64.34: per-frame deltas")]
        self.assertIn("WW3D::Resize_Native_Presentation(", apply)
        self.assertIn("TheDisplay->Display::setDisplayMode(", apply)
        self.assertLess(apply.index("Resize_Native_Presentation"), apply.index("Display::setDisplayMode"))
        # borderless keeps the remembered windowed size
        self.assertIn("if (!borderless)", apply)
        self.assertIn("wanted.resolution", apply)
        # the swap chain is resized, never recreated (the device and its resources stay)
        backend = read("Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.cpp")
        self.assertIn("ResizeBuffers(", backend)

    def test_monitor_identity_and_overrides(self):
        settings = read("Core/GameEngine/Include/Common/DisplaySettings.h")
        for token in ("DisplayMonitorName", "inline std::string monitorId(", "inline int chooseMonitor(", "struct SessionOverrides",
                      "inline PersistPlan planPersistence("):
            self.assertIn(token, settings)
        cl = read("Core/GameEngine/Source/Common/CommandLine.cpp")
        for token in ("overrides.windowed = true;", "overrides.fullscreen = true;", "overrides.resolution.width", "overrides.resolution.height"):
            self.assertIn(token, cl)
        host = read(HOST)
        self.assertIn("DisplayConfig::applyOverrides(stored, DisplayRuntime::state().overrides)", host)
        self.assertIn("state.stored = stored;", host)

    def test_no_control_without_a_backend(self):
        # every visible Modern page control has a disposition in the audit; removed / deferred ones are named
        audit = read("docs/modernization/SETTINGS_COMPLETION_AUDIT.md")
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        names = re.findall(r'\{(?:nullptr|"[A-Za-z]+"), "([A-Za-z]+)", ControlKind', layout)
        self.assertGreaterEqual(len(names), 18)
        label_for = {"ComboBoxResolution": "Resolution", "ModernMode": "Display Mode", "ModernMonitor": "Monitor", "ModernUiScale": "UI Scale",
                     "ModernVsync": "VSync", "ModernRenderScale": "Render Scale", "ComboBoxFrameRateLimit": "Frame Rate Limit", "ComboBoxDetail": "Detail",
                     "SliderSFXVolume": "SFX / Music / Voice volume", "SliderMusicVolume": "SFX / Music / Voice volume",
                     "SliderVoiceVolume": "SFX / Music / Voice volume", "CheckAlternateMouse": "Alternate Mouse",
                     "SliderScrollSpeed": "Scroll Speed", "CheckBoxDrawAnchor": "Draw scroll anchor", "CheckBoxMoveAnchor": "Move scroll anchor",
                     "ModernMotion": "Unit Motion", "ModernInterface": "Interface", "CheckLanguageFilter": "Language filter",
                     "CheckBoxUseCamera": "Use camera", "CheckBoxSaveCamera": "Save camera"}
        for name in names:
            self.assertIn(name, label_for, "page control without an audit entry: " + name)
            self.assertIn(label_for[name], audit, name)
        for removed in ("Gamma", "Anti-aliasing", "Keyboard Options", "Refresh rate"):
            self.assertIn(removed, audit)
        for klass in ("**A**", "**B**", "**C**", "**D**", "**E**", "**F**"):
            self.assertIn(klass, audit)
        self.assertNotIn('"SliderGamma"', layout)

    def test_exclusive_fullscreen_is_not_pretended(self):
        settings = read("Core/GameEngine/Include/Common/DisplaySettings.h")
        self.assertIn("exclusive fullscreen is deferred; using borderless", settings)
        options = read(OPTIONS)
        self.assertNotIn("Exclusive", options.replace("WindowMode::Exclusive", ""))
        self.assertIn("Borderless Fullscreen", options)


class ModernOptionsVocabulary(unittest.TestCase):
    """X64.37 native findings: no `MISSING:` text, the Interface dropdown reachable, the Advanced panel inside the Modern panel."""

    def test_language_filter_is_not_offered(self):
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        items = layout[layout.index("constexpr PageItem kDisplayItems"):layout.index("struct PageTable")]
        self.assertNotIn('"CheckLanguageFilter"', items)      # GUI:LanguageFilter is not Generals vocabulary and has no consumer
        self.assertIn("GUI:LanguageFilter", layout)             # ... and the reason is written down next to the table

    def test_runtime_captions_always_have_a_fallback(self):
        for path in (OPTIONS, "Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp"):
            src = read(path)
            self.assertNotIn("TheGameText->fetch(", src)        # fetch() prints MISSING: for an unknown token
            self.assertNotIn("TheGameText->FETCH(", src)
            for match in re.finditer(r"TheGameText->(\w+)\(", src):
                self.assertIn(match.group(1), ("FETCH_OR_SUBSTITUTE", "fetchOrSubstitute"), path)

    def _generals_dir(self):
        import os
        candidates = [os.environ.get("GENERALS_GAME_DIR", ""),
                      r"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour\ZH_Generals"]
        for c in candidates:
            if c and pathlib.Path(c, "English.big").exists() and pathlib.Path(c, "Window.big").exists():
                return pathlib.Path(c)
        return None

    @staticmethod
    def _big_entry(path, suffix):
        import struct
        with open(path, "rb") as f:
            head = f.read(16)
            count = struct.unpack(">I", head[8:12])[0]
            entries = []
            for _ in range(count):
                off, size = struct.unpack(">II", f.read(8))
                name = b""
                while True:
                    ch = f.read(1)
                    if ch in (b"\0", b""):
                        break
                    name += ch
                entries.append((off, size, name.decode("latin1").replace(chr(92), "/").lower()))
            for off, size, name in entries:
                if name.endswith(suffix):
                    f.seek(off)
                    return f.read(size)
        return None

    @staticmethod
    def _csf_labels(data):
        import struct
        assert data[:4] == b" FSC"
        count = struct.unpack("<I", data[8:12])[0]
        pos = 24
        labels = set()
        for _ in range(count):
            n, ln = struct.unpack("<II", data[pos + 4:pos + 12])
            pos += 12
            labels.add(data[pos:pos + ln].decode("latin1").lower())
            pos += ln
            for _ in range(n):
                tag = data[pos:pos + 4]
                length = struct.unpack("<I", data[pos + 4:pos + 8])[0]
                pos += 8 + length * 2
                if tag == b"WRTS":
                    pos += 4 + struct.unpack("<I", data[pos:pos + 4])[0]
        return labels

    def test_every_retail_label_on_a_modern_page_resolves_in_base_generals(self):
        game = self._generals_dir()
        if game is None:
            self.skipTest("no Generals installation (set GENERALS_GAME_DIR) - structural checks above still run")
        labels = self._csf_labels(self._big_entry(game / "English.big", "generals.csf"))
        wnd = self._big_entry(game / "Window.big", "menus/optionsmenu.wnd").decode("latin1").replace(chr(13), "")
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        names = set(re.findall(r'\{(?:nullptr|"[A-Za-z]+"), "([A-Za-z]+)", ControlKind', layout))
        names |= set(re.findall(r'\{"([A-Za-z]+)", "[A-Za-z]+", ControlKind', layout))
        names |= {"ButtonBack", "ButtonAccept", "ButtonDefaults", "LabelVersion"}
        blocks = re.split(r"\n\s*WINDOWTYPE", wnd)
        checked = 0
        for block in blocks:
            m = re.search(r'NAME = "OptionsMenu.wnd:([^"]*)"', block)
            if not m or m.group(1) not in names:
                continue
            for token in re.findall(r'(?:TEXT|TOOLTIPTEXT) = "([^"]+)"', block):
                if ":" in token:
                    checked += 1
                    self.assertIn(token.lower(), labels, "MISSING token for %s: %s" % (m.group(1), token))
        self.assertGreater(checked, 15)

    def test_advanced_panel_is_a_sub_screen_of_the_modern_panel(self):
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        body = tabs[tabs.index("void setAdvancedOpen(bool open)"):tabs.index("bool onButton(int controlId)")]
        self.assertIn("advancedPopupRect(s_geom, pw, ph, s_scale)", body)
        self.assertIn("s_tabs[i]->winHide(TRUE);", body)          # the tab strip is hidden while the panel is open ...
        self.assertIn("s_tabs[i]->winHide(FALSE);", body)         # ... and restored afterwards
        self.assertIn("showCategory(s_active);", body)
        menu = read(MENU)
        self.assertIn("OptionsTabs::setAdvancedOpen(true);", menu)
        self.assertEqual(menu.count("OptionsTabs::setAdvancedOpen(FALSE);"), 2)   # accept and cancel

    def test_restored_checkboxes_share_their_pages_parent_panel(self):
        tabs = read("Core/GameEngine/Source/GameClient/GUI/OptionsTabs.cpp")
        self.assertIn("OptionsPageLayout::kReparented", tabs)
        self.assertIn("control->winSetParent(host);", tabs)

    def test_borderless_resolution_is_read_only_with_a_reason(self):
        src = read(OPTIONS)
        self.assertIn("OptionsPageLayout::resolutionControl(pendingBorderless(), s_inMatch)", src)
        self.assertIn("s_resolution->winEnable(state.enabled ? TRUE : FALSE);", src)
        self.assertIn("GUI:ResolutionDesktopSize", src)
        self.assertIn("GUI:OptionsMainMenuOnly", src)
        # the in-match gating comes from the one shared table (disabled AND labelled)
        self.assertIn("OptionsPageLayout::mainMenuOnly(item.control)", src)


class RenderScaleAndLiveChanges(unittest.TestCase):
    """X64.37 v3: Render Scale lifecycle, live HUD switching, the compact Global Powers dock."""

    BACKEND = "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D11Backend.cpp"

    def test_render_scale_is_world_only_and_off_by_default(self):
        src = read(self.BACKEND)
        sw = src[src.index("void D3D11Backend::Submit_World("):src.index("impl->context->DrawIndexed(s.index_count, s.first_index, 0);")]
        # 100 % is exactly the previous path; only a scale below 100 takes the intermediate target
        self.assertIn("const bool scaled = WorldRenderScale::active(impl->worldScale);", sw)
        self.assertIn("if (!scaled)" + NL + "        Set_Viewport(s.viewport);", sw)
        self.assertIn("drawRtv = impl->lowRtv.Get();", sw)
        self.assertIn("OMSetRenderTargets(1, &rtv, drawDsv);", sw)
        # the 2D path, text, UI and the back buffer are never scaled: the composite runs BEFORE any 2D drawing, the present and a readback
        draw2d = src[src.index("void D3D11Backend::Draw(const Render2DSubmission &s)"):src.index("impl->Buffer(impl->vertices")]
        self.assertIn("impl->FlushWorld();", draw2d)
        self.assertLess(draw2d.index("impl->FlushWorld();"), draw2d.index("Set_Viewport(s.viewport);"))
        self.assertIn("impl->FlushWorld();" + NL + "    impl->pass.onPresented();", src)               # Flip_To_Primary
        self.assertIn("impl->FlushWorld();     // a screenshot is the presented picture", src)    # Readback_RGBA8
        # resize drops the intermediate target (recreated at the new size)
        resize = src[src.index("void D3D11Backend::Resize("):src.index("void D3D11Backend::Set_World_Render_Scale")]
        self.assertIn("impl->ReleaseLow();", resize)
        # the arithmetic is the pure header that the tests cover
        self.assertIn("WorldRenderScale::internalSize(config.width, config.height, worldScale)", src)
        # ordinary scaling only: no vendor upscaler, no temporal data
        for word in ("DLSS", "FSR", "XeSS", "FidelityFX", "frame generation", "TAA"):
            self.assertNotIn(word, src)

    def test_composite_triangle_is_not_culled(self):
        # X64.37 v3 native failure: a black world. The UI raster state is CULL_BACK + FrontCounterClockwise = TRUE; the full-screen composite
        # triangle was wound clockwise, so it was culled completely. Evaluate the vertex shader's own formula, derive the winding, and
        # require that the composite either cannot be culled or is front facing under the backend's convention.
        src = read(self.BACKEND)
        hlsl = src[src.index("static const char blitShaderSource[]"):src.index("ComPtr<ID3DBlob> CompileBlit")]
        uv_formula = re.search(r"float2 uv = float2\(\(id << 1\) & 2, id & 2\);", hlsl)
        pos_formula = re.search(r"o\.pos = float4\(uv \* float2\(([-\d.]+), ([-\d.]+)\) \+ float2\(([-\d.]+), ([-\d.]+)\), 0\.0, 1\.0\);", hlsl)
        self.assertTrue(uv_formula and pos_formula, "composite vertex formula not recognised - update this contract with the shader")
        sx, sy, ox, oy = (float(v) for v in pos_formula.groups())
        pts = []
        for vid in range(3):
            u, v = float((vid << 1) & 2), float(vid & 2)
            pts.append((u * sx + ox, v * sy + oy))
        area = sum(pts[i][0] * pts[(i + 1) % 3][1] - pts[(i + 1) % 3][0] * pts[i][1] for i in range(3)) / 2.0   # >0: counter-clockwise (y up)
        self.assertNotEqual(area, 0.0)
        # the triangle covers the whole clip square (so the viewport rectangle is what limits it)
        for corner in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
            (x0, y0), (x1, y1), (x2, y2) = pts
            d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
            a = ((y1 - y2) * (corner[0] - x2) + (x2 - x1) * (corner[1] - y2)) / d
            b = ((y2 - y0) * (corner[0] - x2) + (x0 - x2) * (corner[1] - y2)) / d
            self.assertTrue(a >= -1e-9 and b >= -1e-9 and 1 - a - b >= -1e-9, "corner %s not covered" % (corner,))
        # the raster state the composite binds: its own, and it must not cull
        flush = src[src.index("void FlushWorld()"):src.index("void EnsureWorld()")]
        self.assertIn("RSSetState(blitRaster.Get());", flush)
        self.assertNotIn("RSSetState(raster.Get());", flush)
        raster = src[src.index("Create blit raster state") - 400:src.index("Create blit raster state")]
        self.assertIn("D3D11_CULL_NONE", raster)
        # the UI raster state is the one that culls this winding: keeping the lesson machine-checkable
        ui = re.search(r"CullMode = D3D11_CULL_BACK;\s*r\.FrontCounterClockwise = TRUE;", src)
        if ui and area < 0:
            self.assertIn("D3D11_CULL_NONE", raster)

    def test_the_world_phase_has_an_explicit_end_before_the_interface(self):
        disp = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp")
        body = disp[disp.index("void W3DDisplay::draw()"):]
        self.assertLess(body.index("drawViews();"), body.index("NativeEndWorldPhase();"))
        self.assertLess(body.index("NativeEndWorldPhase();"), body.index("TheInGameUI->DRAW();"))
        self.assertIn("backend->End_World_Phase();", read(HOST))
        src = read(self.BACKEND)
        # defence in depth: the first 2D submission, the end of the scene, the present and a readback composite as well
        self.assertIn("impl->pass.onBeginScene();", src)                      # no state survives into a new picture (scene transitions)
        self.assertIn("impl->pass.onColorClear();", src)
        self.assertIn("impl->pass.onPresented();", src)
        self.assertIn("impl->pass.onWorldDraw(", src)
        self.assertIn("pass.takeComposite(area)", src)

    def test_one_hundred_percent_is_the_reference_lifecycle(self):
        src = read(self.BACKEND)
        flush = src[src.index("void FlushWorld()"):src.index("void EnsureWorld()")]
        self.assertIn("if (worldScale >= 100 || !lowSrv || !rtv) return;", flush)
        sw = src[src.index("void D3D11Backend::Submit_World("):src.index("impl->context->DrawIndexed(s.index_count, s.first_index, 0);")]
        # the intermediate target, its clear and its viewport live only in the scaled branch
        branch = sw[sw.index("const bool scaled"):sw.index("float wvp[16];")]
        self.assertIn("if (!scaled)", branch)
        self.assertEqual(branch.count("EnsureLow()"), 1)
        before = branch[:branch.index("else")]
        self.assertNotIn("lowRtv", before)
        # setting 100 releases the target
        self.assertIn("impl->ReleaseLow();", src[src.index("void D3D11Backend::Set_World_Render_Scale"):src.index("unsigned D3D11Backend::World_Render_Scale")])
        # and the triage log names what happened (first scaled world draw and first composite)
        self.assertIn('"render_scale", "world_target"', src)
        self.assertIn('"render_scale", "composite"', src)

    def test_render_scale_is_a_live_setting_everywhere(self):
        opt = read(OPTIONS)
        self.assertIn("DisplayRuntime::renderScaleHook()", read(HOST))
        self.assertIn("requested.renderScalePercent = WorldRenderScale::kChoices[renderScaleIndex];", opt)
        self.assertIn("scale(requested.renderScalePercent);", opt)
        # not inside the main-menu-only branch and not a layout change
        layout = read("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        self.assertNotIn('"ModernRenderScale"', layout[layout.index("constexpr const char *kMainMenuOnly"):layout.index("inline bool mainMenuOnly")])
        self.assertNotIn('"ModernInterface"', layout[layout.index("constexpr const char *kMainMenuOnly"):layout.index("inline bool mainMenuOnly")])

    def test_live_hud_switch_rebinds_the_scheme_instead_of_rebuilding_the_bar(self):
        opt = read(OPTIONS)
        fn = opt[opt.index("void refreshHudPresentation()"):opt.index("bool reallyInMatch()")]
        self.assertIn("setControlBarSchemeByPlayer(viewed)", fn)
        self.assertIn("markUIDirty()", fn)
        self.assertNotIn("recreateControlBar", fn)
        ap = opt[opt.index("bool apply(OptionPreferences &preferences"):opt.index("bool confirmationPending()")]
        self.assertIn("if (reallyInMatch())" + NL + chr(9) * 3 + "refreshHudPresentation();", ap)
        # the scheme init is the one place that chooses Enhanced or Original for the side
        scheme = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp")
        init = scheme[scheme.index("void ControlBarScheme::init()"):scheme.index("// Add an animation to the animation list")]
        self.assertIn("EnhancedHud::update(", init)
        self.assertIn("TheControlBar->applyEnhancedLayout();", init)
        self.assertIn("TheControlBar->restoreOriginalLayout();", init)

    def test_compact_global_powers_dock(self):
        eng = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarEnhanced.cpp")
        self.assertIn("EnhancedHudPanels::compactDock(", eng)
        self.assertIn("parent->winSetSize( dock.row.w, dock.row.h );", eng)
        cb = read("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp")
        pop = cb[cb.index("void ControlBar::populateSpecialPowerShortcut"):cb.index("Bool ControlBar::hasAnyShortcutSelection()")]
        self.assertIn("applyEnhancedPowerShortcutLayout();", pop)          # re-packed whenever the visible buttons change
        self.assertLess(pop.index("applyEnhancedPowerShortcutLayout();"), pop.index("showSpecialPowerShortcut();"))
        # the frozen layout is untouched (hash-pinned by test_framework_v1_shared_headers_are_frozen)


if __name__ == "__main__":
    unittest.main()
