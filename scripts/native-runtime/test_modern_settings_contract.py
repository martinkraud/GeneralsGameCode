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


if __name__ == "__main__":
    unittest.main()
