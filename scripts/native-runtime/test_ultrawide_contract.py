# SPDX-License-Identifier: GPL-3.0-or-later
# X64.48A (second-PC correctness pass): contracts for the two ultrawide defects found on a 5120x1440 display.
#  * the terrain draw window was the retail 4:3 size (129 cells), so a Hor+ wide view showed black bands on both sides even in the middle of the map;
#  * the shroud (fog of war) overlay batch ignored the object-pass chunk's INDEX limit, so a wide (or zoomed-out) view made the pass refuse it and the
#    fog vanished.
# Source-text contracts only (the behaviour is measured by the automated captures recorded in X64_48A_SECOND_PC_CORRECTNESS.md and by the gtest
# NativeWorldEffects.ShroudBandsFitTheObjectPassChunkAtEveryWidth).
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


class UltrawideContract(unittest.TestCase):
    def test_modern_terrain_draw_window_follows_the_view_and_classic_is_untouched(self):
        view = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp")
        body = view[view.index("bool W3DView::getDesiredTerrainDrawSize"):view.index("void W3DView::updateTerrain()")]
        # the retail sizes are still produced first and Classic returns them unchanged
        self.assertIn("WorldHeightMap::NORMAL_DRAW_WIDTH", body)
        self.assertIn("WorldHeightMap::LOW_ANGLE_DRAW_WIDTH", body)
        self.assertIn("if (DisplayRuntime::modern()", body)
        # the window only ever grows (a resize rebuilds the terrain buffers) and is reset with the view
        self.assertIn("std::max(m_modernDrawWidth", body)
        self.assertIn("m_modernDrawWidth = 0;", view[view.index("void W3DView::reset()"):view.index("void W3DView::reset()") + 1500])
        self.assertIn("std::max(dimensions.x, m_modernDrawWidth)", body)

    def test_shroud_batches_respect_both_chunk_limits(self):
        fx = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldEffects.cpp")
        self.assertIn("Geometry::ShroudBandRows(lx1 - lx0)", fx)
        self.assertNotIn("59000 / (lx1 - lx0 + 1) - 1", fx)
        band = fx[fx.index("int ShroudBandRows("):fx.index("unsigned ShroudTexel(")]
        self.assertIn("kBatchIndices", band)
        self.assertIn("6 * across", band)
        objects = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/NativeWorldObjects.cpp")
        # the band limits are derived from the pass chunk: if the chunk changes this contract must be revisited
        self.assertIn("constexpr unsigned kChunkVertices = 60000;", objects)
        self.assertIn("constexpr unsigned kChunkIndices = 180000;", objects)

    def test_developer_size_override_is_inert_without_its_variable(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        start = host.index("GENERALS_FORCE_CLIENT_SIZE")
        window = host[start - 400:start + 900]
        self.assertIn("StartupDiagnostics::normalLaunch()", window)
        self.assertIn("std::getenv(\"GENERALS_FORCE_CLIENT_SIZE\")", window)
        self.assertIn("forcedW >= 640", window)          # sane bounds only
        script = read("scripts/native-runtime/Invoke-NativeAutoCapture.ps1")
        self.assertIn("$env:GENERALS_FORCE_CLIENT_SIZE = $null", script)   # never leaks into later runs


if __name__ == "__main__":
    unittest.main()
