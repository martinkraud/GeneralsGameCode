#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""X64.49 contract: held camera scrolling is integrated in presentation time and nothing about the simulation changes.

Structural pins (the arithmetic itself is covered by the gtest PresentationScrollTest.cpp):
  * the translator hands the scroll VELOCITY to the view instead of applying a step sized by the previous frame interval;
  * the view applies it once per update, before the camera transform of the drawn frame, for the real wall-clock time since the previous update;
  * release / forced stop clear the pending velocity (no overshoot);
  * the classic scroll speed formula (speed factor * SCROLL_AMT * keyboard factor, edge / key / RMB) is untouched;
  * no gameplay, RNG, replay or pathfinding file is touched by the camera change; the 30 TPS logic accumulator is untouched;
  * the measurement probe is inert unless GENERALS_CAMERA_PROBE is set.
"""
import os
import re
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8", errors="replace") as handle:
        return handle.read().replace("\r\n", "\n")


class CameraPresentationContract(unittest.TestCase):
    def test_translator_hands_over_a_velocity(self):
        t = read("Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp")
        self.assertIn("userScrollByPresented( &offset, fpsRatio )", t)
        # the former direct step is only reachable through the measurement probe's legacy switch
        direct = [m.start() for m in re.finditer(r"TheTacticalView->userScrollBy\( &offset \)", t)]
        self.assertEqual(len(direct), 1)
        self.assertIn("legacyScroll", t[direct[0] - 200:direct[0]])
        # release and forced stop clear the pending velocity
        self.assertGreaterEqual(t.count("TheTacticalView->clearPendingUserScroll();"), 2)

    def test_classic_scroll_speed_formula_is_unchanged(self):
        t = read("Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp")
        self.assertEqual(len(re.findall(r"TheGlobalData->m_verticalScrollSpeedFactor \* fpsRatio \* SCROLL_AMT \* TheGlobalData->m_keyboardScrollFactor", t)), 4)
        self.assertEqual(len(re.findall(r"TheGlobalData->m_horizontalScrollSpeedFactor \* fpsRatio \* SCROLL_AMT \* TheGlobalData->m_keyboardScrollFactor", t)), 4)
        self.assertIn("vecLength * vec.x * SCROLL_MULTIPLIER * TheGlobalData->m_keyboardScrollFactor", t)
        self.assertIn("static const Int edgeScrollSize = 3;", t)
        self.assertIn("TheInGameUI->setScrollAmount(offset);", t)

    def test_view_applies_the_velocity_in_presentation_time(self):
        v = read("Core/GameEngine/Source/GameClient/View.cpp")
        self.assertIn("PresentationScroll::stepSeconds(now.QuadPart, m_lastUserScrollTicks, frequency.QuadPart)", v)
        self.assertIn("PresentationScroll::deltaForInterval", v)
        self.assertIn("if (isUserControlLocked())", v[v.index("Bool View::userScrollByPresented"):])
        w = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp")
        body = w[w.index("void W3DView::update()"):]
        self.assertLess(body.index("applyPendingUserScroll();"), body.index("updateCameraTransform();"))
        self.assertLess(body.index("applyPendingUserScroll();"), body.index("m_terrainHeightAtPivot = getHeightAroundPos"))

    def test_scroll_arithmetic_is_pure(self):
        h = read("Core/GameEngine/Include/Common/PresentationScroll.h")
        self.assertIn("MaxStepSeconds = 0.2f", h)
        self.assertIn("BaseFps = 30.0f", h)
        self.assertNotIn("TheGameLogic", h)
        self.assertNotIn("GameLogicRandom", h)

    def test_gtest_is_registered(self):
        c = read("Tests/Google/Core/CMakeLists.txt")
        self.assertIn("PresentationScrollTest.cpp", c)
        t = read("Tests/Google/Core/GameEngine/Common/PresentationScrollTest.cpp")
        for name in ("HeldInputCoversTheSameDistanceAtEveryRenderRate", "SeenSpeedDoesNotFollowFrameTimeJitter", "ReleaseStopsImmediatelyWithoutOvershoot",
                     "FirstUpdateStallAndBackwardsCounterNeverJump"):
            self.assertIn(name, t)
        for fps in ("30", "60", "120", "144", "240"):
            self.assertIn(fps, t.split("HeldInputCoversTheSameDistanceAtEveryRenderRate")[1].split("TEST(")[0])

    def test_simulation_is_untouched(self):
        # the logic accumulator that keeps the simulation at exactly 30 ticks per second
        g = read("Generals/Code/GameEngine/Source/Common/GameEngine.cpp")
        self.assertIn("m_logicTimeAccumulator += min(TheFramePacer->getUpdateTime(), targetFrameTime);", g)
        f = read("Core/GameEngine/Source/Common/FramePacer.cpp")
        self.assertIn("m_logicTimeScaleFPS = LOGICFRAMES_PER_SECOND;", f)
        # the new code never reads or writes game-logic state or random numbers
        for rel in ("Core/GameEngine/Include/Common/PresentationScroll.h", "Core/GameEngine/Include/Common/CameraProbe.h"):
            text = read(rel)
            self.assertNotRegex(text, r"GameLogicRandomValue|TheRandom|getObjectByID|AIUpdate|Pathfind")

    def test_probe_is_inert_unless_requested(self):
        p = read("Core/GameEngine/Include/Common/CameraProbe.h")
        self.assertIn('getenv("GENERALS_CAMERA_PROBE")', p)
        for needle in ("GENERALS_CAMERA_PROBE_FPS", "GENERALS_CAMERA_PROBE_HOLD", "GENERALS_CAMERA_PROBE_LEGACY"):
            self.assertIn(needle, p)
        d = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp")
        self.assertGreaterEqual(d.count("if (CameraProbe::enabled())"), 2)


if __name__ == "__main__":
    unittest.main()
