#!/usr/bin/env python3
"""X64.52B: the camera real-input gate (analyze_camera_real.py) must pass the accepted behaviour and FAIL each defect class of the X64.52A human test.

Synthetic traces of the CSV the game writes in real-input mode; plus source contracts that keep the real-input path and the edge rule wired.
"""
import pathlib
import random
import unittest

import analyze_camera_real as gate

ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def trace(fps=120, seconds=1.5, speed=1000.0, kind="smooth", jitter=0.0, seed=1):
    """D rows of one phase: camera x per drawn frame. kinds: smooth | dead | tick30 (camera moves only on 30 Hz boundaries) | ripple (step ignores the interval)."""
    rnd = random.Random(seed)
    rows, t, x, last_tick, nominal = [], 1000.0, 0.0, 0, 1000.0 / fps
    frame = 0
    while t < 1000.0 + seconds * 1000.0:
        dt = nominal * (1.0 + rnd.uniform(-jitter, jitter))
        if kind == "smooth":
            x += speed * dt / 1000.0
        elif kind == "tick30":
            tick = int((t - 1000.0) / (1000.0 / 30.0))
            if tick != last_tick:
                x += speed / 30.0
                last_tick = tick
        elif kind == "ripple":
            x += speed * nominal / 1000.0          # constant step regardless of the real interval
        t += dt
        rows.append({"kind": "D", "phase": "2", "ms": "%.4f" % t, "logic_frame": str(frame // 4), "a": "0", "b": "0", "c": "0", "d": "%.5f" % x, "e": "0"})
        frame += 1
    return rows


def with_phase(rows, phase):
    return [dict(r, phase=str(phase)) for r in rows]


class Gate(unittest.TestCase):
    def test_smooth_camera_passes_at_every_render_rate(self):
        for fps in (30, 60, 120, 240):
            for jitter in (0.0, 0.25):
                res = gate.analyze(with_phase(trace(fps, jitter=jitter), 2) + with_phase(trace(fps, jitter=jitter), 4) + with_phase(trace(fps, jitter=jitter), 6))
                for name in ("key", "edge", "rmb"):
                    self.assertTrue(res[name]["ok"], (fps, jitter, name, res[name]))

    def test_dead_screen_edge_fails(self):
        # the X64.52A defect: the cursor reached the border, nothing scrolled
        res = gate.analyze(with_phase(trace(), 2) + with_phase(trace(kind="dead"), 4) + with_phase(trace(), 6))
        self.assertTrue(res["key"]["ok"])
        self.assertFalse(res["edge"]["ok"])
        self.assertIn("does not scroll", res["edge"]["problems"][0])

    def test_camera_on_the_simulation_tick_fails(self):
        res = gate.analyze(with_phase(trace(120, kind="tick30"), 2) + with_phase(trace(), 4) + with_phase(trace(), 6))
        self.assertFalse(res["key"]["ok"])
        self.assertTrue(any("stale" in p for p in res["key"]["problems"]))

    def test_speed_ripple_with_irregular_frames_fails(self):
        res = gate.analyze(with_phase(trace(240, kind="ripple", jitter=0.6), 2) + with_phase(trace(), 4) + with_phase(trace(), 6))
        self.assertFalse(res["key"]["ok"])
        self.assertTrue(any("ripples" in p for p in res["key"]["problems"]))

    def test_missing_phase_fails(self):
        res = gate.analyze(with_phase(trace(), 2))
        self.assertFalse(res["edge"]["ok"])
        self.assertFalse(res["rmb"]["ok"])

    def test_camera_stopping_at_the_map_border_is_not_a_failure(self):
        rows = trace(120, seconds=1.5)
        for r in rows[int(len(rows) * 0.7):]:          # the camera hit the border: constant position to the end
            r["d"] = rows[int(len(rows) * 0.7)]["d"]
        res = gate.analyze(with_phase(rows, 2) + with_phase(trace(), 4) + with_phase(trace(), 6))
        self.assertTrue(res["key"]["ok"], res["key"])


class Wiring(unittest.TestCase):
    def test_real_input_mode_injects_nothing_and_logs_the_translator_state(self):
        xlat = read("Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp")
        self.assertIn("if (CameraProbe::state().realInput)", xlat)
        self.assertIn("isCursorCaptured()", xlat[xlat.index("realInput"):])
        probe = read("Core/GameEngine/Include/Common/CameraProbe.h")
        self.assertIn("GENERALS_CAMERA_PROBE_REAL", probe)
        driver = read("scripts/native-runtime/Invoke-CameraRealInput.ps1")
        self.assertIn("SendInput", driver)
        self.assertIn("analyze_camera_real", read("docs/modernization/X64_52B_CAMERA_INPUT_REGRESSION.md"))

    def test_edge_rule_is_the_tested_one(self):
        xlat = read("Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp")
        self.assertIn("ScreenEdgeScroll::allowed(", xlat)
        self.assertEqual(xlat.count("ScreenEdgeScroll::hit("), 3)   # start, stop, direction: one rule
        self.assertNotIn("edgeScrollSize", xlat)
        self.assertIn("Tests/Google/Core/GameEngine/Common/ScreenEdgeScrollTest.cpp".split("Tests/Google/Core/")[1], read("Tests/Google/Core/CMakeLists.txt"))

    def test_view_uses_the_tested_integrator(self):
        view = read("Core/GameEngine/Source/GameClient/View.cpp")
        self.assertIn("m_userScroll.advance(", view)
        self.assertIn("m_userScroll.set(", view)


if __name__ == "__main__":
    unittest.main()
