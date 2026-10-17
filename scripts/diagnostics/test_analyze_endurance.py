#!/usr/bin/env python3
"""Tests of Analyze-EnduranceTelemetry.py on synthetic telemetry (no game, no real capture)."""
import csv
import importlib.util
import os
import re
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("analyze_endurance", HERE / "Analyze-EnduranceTelemetry.py")
analyze = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyze)

INTERVAL_COLUMNS = ["session_s", "frame", "game_s", "wall_s", "interval_ticks", "interval_wall_s", "achieved_tps", "lag_s", "engine_frames", "paused_frames", "non_tick_updates",
                    "objects_total", "units_total", "structures_total", "stationary_nonidle_units", "ms_ai_object_base_update", "ms_path_search", "ms_logic_scripts", "ms_object_update_loops",
                    "ms_zone_calc", "update_modules", "queued_paths", "path_searches", "path_fail", "path_fail_ge5ms", "path_fail_ms", "zone_calc_calls"]


def write_session(directory, minutes, degrade_minute=None, paused_minute=None):
    """A session whose objects grow linearly; from degrade_minute on every tick takes 40 ms (the game runs slower than real time)."""
    stem = os.path.join(directory, "endurance-20260101T000000Z")
    ticks = []
    intervals = []
    wall = 0.0
    frame = 1
    total_ticks = int(minutes * 60 * 30)
    for t in range(total_ticks):
        minute = t / 30.0 / 60.0
        slow = degrade_minute is not None and minute >= degrade_minute
        dur = 40.0 if slow else 0.7
        ticks.append((frame, dur, wall * 1000.0))
        wall += max(1.0 / 30.0, dur / 1000.0)
        frame += 1
        if frame % 300 == 0:
            interval_ticks = 300
            interval_wall = max(10.0, 300 * dur / 1000.0)
            intervals.append({
                "session_s": wall, "frame": frame, "game_s": (frame - 1) / 30.0, "wall_s": wall, "interval_ticks": interval_ticks, "interval_wall_s": interval_wall,
                "achieved_tps": interval_ticks / interval_wall, "lag_s": 0, "engine_frames": 300, "paused_frames": 300 if (paused_minute is not None and abs(minute - paused_minute) < 0.2) else 0,
                "non_tick_updates": 0, "objects_total": 500 + 10 * minute * 6, "units_total": 100 + 5 * minute * 6, "structures_total": 100, "stationary_nonidle_units": 2 * minute,
                "ms_ai_object_base_update": 30 + minute, "ms_path_search": 5, "ms_logic_scripts": 20, "ms_object_update_loops": 20, "ms_zone_calc": 0, "update_modules": 100000,
                "queued_paths": 50, "path_searches": 50, "path_fail": 1, "path_fail_ge5ms": 0, "path_fail_ms": 3, "zone_calc_calls": 0})
    with open(stem + "-meta.txt", "w") as f:
        f.write("map=synthetic\ntitle=Generals\ngit=0000000000\nhardware=test\nplayer2=side:America;type:human;difficulty:1\n")
    with open(stem + "-ticks.csv", "w", newline="") as f:
        f.write("frame,tick_ms,start_ms\n")
        for fr, d, s in ticks:
            f.write("%d,%.3f,%.3f\n" % (fr, d, s))
    with open(stem + "-intervals.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=INTERVAL_COLUMNS)
        w.writeheader()
        for r in intervals:
            w.writerow({k: r.get(k, 0) for k in INTERVAL_COLUMNS})
    return stem


class AnalyzeEnduranceTests(unittest.TestCase):
    def test_healthy_session_has_no_degradation_markers(self):
        with tempfile.TemporaryDirectory() as d:
            stem = write_session(d, 20)
            text, data = analyze.analyse(stem, 5)
            self.assertIn("first window with achieved tick rate < 29.5 TPS: none", text)
            self.assertIn("first persistent model backlog (300 consecutive catch-up ticks): none", text)
            self.assertEqual(data["summary"]["missed"], 0)
            self.assertLess(data["summary"]["slip_s"], 1e-6)

    def test_degradation_is_located(self):
        with tempfile.TemporaryDirectory() as d:
            stem = write_session(d, 30, degrade_minute=20)
            text, data = analyze.analyse(stem, 5)
            self.assertIn("first window with accumulated real-time slip >= 1 s:", text)
            marker = [l for l in text.splitlines() if "first persistent model backlog" in l][0]
            self.assertNotIn("none", marker)
            minute = float(re.search(r"\(([0-9.]+) min game time\)", marker).group(1))
            self.assertAlmostEqual(minute, 20.0, delta=0.2)
            self.assertGreater(data["summary"]["slip_s"], 100.0)
            self.assertGreater(data["summary"]["max_backlog_ms"], 1000.0)

    def test_paused_intervals_do_not_count_as_slip(self):
        with tempfile.TemporaryDirectory() as d:
            stem = write_session(d, 10, paused_minute=5)
            _, data = analyze.analyse(stem, 5)
            self.assertLess(data["summary"]["slip_s"], 1e-6)

    def test_realtime_model_matches_the_engine_definition(self):
        missed, catchup, longest, max_backlog, _ = analyze.realtime_model([10.0, 50.0, 10.0, 10.0, 10.0])
        # tick 1 (50 ms) finishes after its slot (33.33 ms): missed; tick 2 starts 16.7 ms late: catch-up; tick 3 starts at 83.3 on a slot at 100 -> on time again
        self.assertEqual(missed, 1)
        self.assertEqual(catchup, 1)
        self.assertEqual(longest, 1)
        self.assertAlmostEqual(max_backlog, 50.0 - 33.333, delta=0.01)

    def test_percentiles_use_nearest_rank(self):
        values = sorted(float(i) for i in range(1, 101))
        self.assertEqual(analyze.pct(values, 99), 99.0)
        self.assertEqual(analyze.pct(values, 50), 50.0)


if __name__ == "__main__":
    unittest.main()
