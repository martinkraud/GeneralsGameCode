#!/usr/bin/env python3
"""Seeded graphical benchmark tooling (Summarize-GraphicalBenchmark.py, Analyze-PathFailures.py, Summarize-TickWindow.py): digest, divergence detection, windows (synthetic data only)."""
import csv
import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
PERF = ROOT / "scripts/performance"
FIXTURE = PERF / "fixtures/zh-twilight4H-E50000-1x.fingerprint.txt"


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, PERF / file)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class GraphicalBenchmark(unittest.TestCase):
    def test_digest_matches_the_engines_known_headless_digest(self):
        # the X64.pf guard digest of the Zero Hour twilight4H 1x scenario, computed by the engine (DeterminismFingerprint::digest)
        m = load("sgb", "Summarize-GraphicalBenchmark.py")
        self.assertEqual(m.fnv_digest(m.read_lines(str(FIXTURE))), "1E5982E5B3A1B86B")

    def run_summary(self, graphical, headless):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            (d / "fingerprint.txt").write_text("\n".join(graphical) + "\n")
            (d / "headless_fingerprint.txt").write_text("\n".join(headless) + "\n")
            p = subprocess.run([sys.executable, "-I", str(PERF / "Summarize-GraphicalBenchmark.py"), str(d), "--headless-fingerprint", str(d / "headless_fingerprint.txt")], capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stderr)
            return json.loads(p.stdout)

    def test_equal_fingerprints_report_no_divergence(self):
        lines = ["frame=30 crc=0F4F6F0A objects=226", "frame=60 crc=E0DBC5DF objects=226", "frame=90 crc=CD468ABB objects=227"]
        j = self.run_summary(lines, lines + ["frame=120 crc=3F5CBDC3 objects=227"])
        self.assertEqual(j["fingerprint"]["vs_headless"]["first_divergence_index"], -1)
        self.assertEqual(j["fingerprint"]["vs_headless"]["compared_checkpoints"], 3)   # the common prefix only

    def test_first_divergence_is_located(self):
        a = ["frame=30 crc=00000001 objects=1", "frame=60 crc=00000002 objects=1", "frame=90 crc=00000003 objects=1"]
        b = ["frame=30 crc=00000001 objects=1", "frame=60 crc=000000FF objects=1", "frame=90 crc=00000003 objects=1"]
        self.assertEqual(self.run_summary(a, b)["fingerprint"]["vs_headless"]["first_divergence_index"], 1)

    def test_path_failure_classifier(self):
        m = load("apf", "Analyze-PathFailures.py")
        header = "frame,object,aistate,ms,goalX,goalY,repeat,cellsAllocated,pops,blockerUnits,goalType"
        rows = ["1,5,HUNT,10.0,1,2,0,21000,100,3,clear", "2,5,HUNT,10.0,1,2,1,21000,100,0,clear", "3,6,MOVE_TO,5.0,3,4,0,100,10,0,clear"]
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / "pf.csv"
            p.write_text(header + "\n" + "\n".join(rows) + "\n")
            out = subprocess.run([sys.executable, "-I", str(PERF / "Analyze-PathFailures.py"), "t=" + str(p)], capture_output=True, text=True).stdout
        self.assertIn("3 analysed failing searches", out)
        self.assertIn("repeats of an earlier failing signature: 1", out)
        self.assertIn("failures that repeat a pair: 1", out)
        self.assertIn("pool pressure): 2", out)

    def test_tick_window_statistics(self):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            with open(d / "x_ticks.csv", "w", newline="") as f:
                w = csv.writer(f)
                w.writerow(["tick", "frame", "ms", "path_search"])
                for i in range(1, 101):
                    w.writerow([i, i, float(i), 1.0])
            (d / "x_fingerprint.txt").write_text("frame=30 crc=00000001 objects=7\n")
            out = subprocess.run([sys.executable, "-I", str(PERF / "Summarize-TickWindow.py"), "--from-frame", "51", "--phases", "path_search", "r=" + str(d)], capture_output=True, text=True).stdout
        row = [l for l in out.splitlines() if l.startswith("r ")][0].split()
        self.assertEqual(int(row[1]), 50)          # ticks in the window
        self.assertAlmostEqual(float(row[2]), 75.5)   # mean of 51..100


if __name__ == "__main__":
    unittest.main()
