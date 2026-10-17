# SPDX-License-Identifier: GPL-3.0-or-later
# X64.48A: contract for the Skirmish slot 7/8 defect found on the second PC (Generals): the "Starting Cash" label of the Generals Skirmish screen
# lies over the lower part of the drop-down lists of player slots 7 and 8; the mouse path found that label first (it takes no input and was
# discarded), so clicks on the lower list rows were lost. Fix: the open combo box ("lone window") owns its list rectangle in findWindowUnderMouse.
# The behaviour is measured by Test-NativeSkirmishSlots.ps1 (runtime, disposable stage); these are the source contracts that keep the fix and the probe.
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


class SkirmishSlotsContract(unittest.TestCase):
    def test_open_combo_box_is_tried_first_by_the_mouse_path(self):
        manager = read("Core/GameEngine/Source/GameClient/GUI/GameWindowManager.cpp")
        start = manager.index("GameWindow* GameWindowManager::findWindowUnderMouse(")
        body = manager[start:manager.index("//----", start)]
        lone = body.index("m_loneWindow")
        scan = body.index("for (GameWindow* window = m_windowList")
        self.assertLess(lone, scan, "the open combo box must be consulted before the top-level scan")
        for needle in ("GWS_COMBO_BOX", "WIN_STATUS_HIDDEN", "WIN_STATUS_ENABLED", "winPointInWindow", "winPointInChild"):
            self.assertIn(needle, body[:scan])
        # the retail NO_INPUT rule in winProcessMouseEvent is unchanged
        self.assertIn("WIN_STATUS_NO_INPUT", manager[manager.index("WinInputReturnCode GameWindowManager::winProcessMouseEvent"):])

    def test_probe_flow_and_script_exist_and_use_the_real_mouse_path(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn('!_stricmp(text, "slots")', host)
        probe = host[host.index("SLOT_PROBE"):]
        self.assertIn("findWindowUnderMouse", probe)           # the same lookup the mouse event uses, not a private geometry test
        self.assertIn("WIN_STATUS_NO_INPUT", probe)             # including the discard of windows that take no input
        self.assertIn('"slotprobe"', probe)
        script = read("scripts/native-runtime/Test-NativeSkirmishSlots.ps1")
        for size in ("1920x1080", "2560x1440", "5120x1440"):
            self.assertIn(size, script)
        self.assertIn("-Flow Slots", script)
        capture = read("scripts/native-runtime/Invoke-NativeAutoCapture.ps1")
        self.assertIn("'Slots'", capture)


if __name__ == "__main__":
    unittest.main()
