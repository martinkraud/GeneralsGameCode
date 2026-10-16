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
                     "ModernVsync": "VSync", "ComboBoxFrameRateLimit": "Frame Rate Limit", "ComboBoxDetail": "Detail",
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


if __name__ == "__main__":
    unittest.main()
