#!/usr/bin/env python3
"""Clean-DiagnosticOutput.ps1 retention: age rules, protection, KEEP marker, crash-dump safeguard, scope and refusals (disposable fake build tree)."""
import json
import os
import pathlib
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL = ROOT / "scripts/Clean-DiagnosticOutput.ps1"
PWSH = shutil.which("pwsh")
DAY = 86400


def age(path, days):
    t = time.time() - days * DAY
    for p in [path, *path.rglob("*")]:
        os.utime(p, (t, t))


def run(build, policy, *args):
    p = subprocess.run([PWSH, "-NoProfile", "-File", str(TOOL), "-BuildRoot", str(build), "-PolicyFile", str(policy), *args], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


@unittest.skipUnless(PWSH, "pwsh not available")
class DiagCleanup(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp(prefix="x64diag-"))
        self.build = self.tmp / "build"
        self.diag = self.build / "diag"
        self.diag.mkdir(parents=True)
        self.policy = self.tmp / "policy.json"
        self.policy.write_text(json.dumps({"rawCaptureDays": 7, "rawCapturePatterns": ["autocapture-*"], "runOutputDays": 30, "protect": ["baseline.exe"]}))

        def mk(name, days, files=None):
            d = self.diag / name
            d.mkdir()
            for f in files or ["a.txt"]:
                (d / f).write_text("x")
            age(d, days)
            return d
        self.raw_old = mk("autocapture-1", 10)
        self.raw_new = mk("autocapture-2", 2)
        self.run_old = mk("run-old", 40)
        self.run_mid = mk("run-mid", 20)
        self.dump = mk("run-crash", 90, ["a.txt", "CrashFG.dmp"])
        self.marked = mk("run-marked", 90, ["a.txt", "KEEP"])
        self.prot = self.diag / "baseline.exe"
        self.prot.write_text("x")
        age(self.prot, 90)
        (self.build / "evidence-archive").mkdir()
        (self.build / "evidence-archive" / "old.txt").write_text("x")
        age(self.build / "evidence-archive", 400)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_dry_run_removes_nothing(self):
        code, out = run(self.build, self.policy)
        self.assertEqual(code, 0, out)
        self.assertIn("DRY RUN", out)
        for d in (self.raw_old, self.run_old, self.dump):
            self.assertTrue(d.exists())

    def test_execute_follows_the_policy(self):
        code, out = run(self.build, self.policy, "-Execute")
        self.assertEqual(code, 0, out)
        self.assertFalse(self.raw_old.exists(), "raw capture older than 7 days")
        self.assertTrue(self.raw_new.exists(), "raw capture younger than 7 days")
        self.assertFalse(self.run_old.exists(), "run output older than 30 days")
        self.assertTrue(self.run_mid.exists(), "run output younger than 30 days")
        self.assertTrue(self.dump.exists(), "crash dumps are never removed automatically")
        self.assertTrue(self.marked.exists(), "KEEP marker")
        self.assertTrue(self.prot.exists(), "protected name")
        self.assertTrue((self.build / "evidence-archive" / "old.txt").exists(), "outside diag / audit-tmp nothing is touched")

    def test_refusals(self):
        code, out = run(self.tmp, self.policy)   # not named build
        self.assertNotEqual(code, 0)
        steam = self.tmp / "Steam" / "build"
        (steam / "diag").mkdir(parents=True)
        code, out = run(steam, self.policy)
        self.assertNotEqual(code, 0)
        self.assertIn("Steam", out)
        bad = self.tmp / "bad.json"
        bad.write_text(json.dumps({"rawCaptureDays": 0, "rawCapturePatterns": [], "runOutputDays": 30, "protect": []}))
        code, out = run(self.build, bad, "-Execute")
        self.assertNotEqual(code, 0)
        self.assertTrue(self.raw_old.exists())


if __name__ == "__main__":
    unittest.main()
