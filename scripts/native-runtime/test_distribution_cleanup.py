#!/usr/bin/env python3
"""Distribution cleanup (Remove-ObsoleteDistributions.ps1): retention, provenance and safety, run against a disposable fake build tree."""
import json
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/native-runtime/Remove-ObsoleteDistributions.ps1"
MAIN = (ROOT / "scripts/native-runtime/New-MKRaudsteinDistribution.ps1").read_text(encoding="utf-8")
PWSH = shutil.which("pwsh")


def make_dist(dist, stage, ver, record=True, mtime=None):
    names = []
    for t, s in (("Generals", "gen"), ("ZeroHour", "zh")):
        p = "MKRaudstein-%s-x64-Preview-%s" % (t, ver)
        names += [p + ".zip", p + ".zip.sha256", p + "-Symbols.zip", p + ".manifest.json", p + ".overlay-test.json", p + ".stage-parity.json"]
        d = stage / t / ("dist-%s-%s" % (ver, s))
        d.mkdir(parents=True)
        (d / "ordinaryplay.json").write_text("{}")
    names.append("MKRaudstein-x64-Preview-%s.SHA256SUMS" % ver)
    if record:
        rec = {"version": ver, "titles": [{"title": "Generals", "candidate": "dist-%s-gen" % ver, "validated": True},
                                           {"title": "ZeroHour", "candidate": "dist-%s-zh" % ver, "validated": True}]}
        (dist / ("MKRaudstein-x64-Preview-%s.distribution.json" % ver)).write_text(json.dumps(rec))
    for n in names:
        (dist / n).write_text("x")


@unittest.skipUnless(PWSH, "pwsh not available")
class Cleanup(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp(prefix="x64dist-"))
        self.dist = self.tmp / "dist"
        self.stage = self.tmp / "native"
        self.dist.mkdir()
        import os, time
        for i, ver in enumerate(["aaaaaaa1", "bbbbbbb2", "ccccccc3", "ddddddd4"]):
            make_dist(self.dist, self.stage, ver)
            for f in self.dist.glob("*" + ver + "*"):
                os.utime(f, (time.time() - 1000 + i * 100,) * 2)
        make_dist(self.dist, self.stage, "eeeeeee0", record=False)       # legacy, no record
        (self.dist / "notes-by-hand.txt").write_text("mine")              # unknown file
        (self.dist / "MKRaudstein-Generals-x64-Preview-zzzzzzz9.txt").write_text("mine")   # similar name, not a generated one
        (self.stage / "Generals" / "x64-48b-gen-v2").mkdir()             # not a dist-* stage
        (self.stage / "ZeroHour" / "dist-fffffff5-zh").mkdir()           # dist-looking stage without the stager marker

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_cleanup(self, *extra):
        r = subprocess.run([PWSH, "-NoProfile", "-File", str(SCRIPT), "-OutputDirectory", str(self.dist), "-StageRoot", str(self.stage), *extra],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout

    def versions_left(self):
        return sorted({n.split("Preview-")[1][:8] for n in (p.name for p in self.dist.iterdir()) if "Preview-" in n and n.endswith(("zip", "json", "SUMS", "sha256"))})

    def test_keeps_current_and_previous_only(self):
        self.run_cleanup("-CurrentVersion", "ddddddd4")
        self.assertEqual(self.versions_left(), ["ccccccc3", "ddddddd4"])
        for ver in ("ccccccc3", "ddddddd4"):
            self.assertEqual(len(list(self.dist.glob("*" + ver + "*"))), 14)
            self.assertTrue((self.dist / ("MKRaudstein-ZeroHour-x64-Preview-%s-Symbols.zip" % ver)).exists())
            self.assertTrue((self.dist / ("MKRaudstein-x64-Preview-%s.distribution.json" % ver)).exists())
            self.assertTrue((self.stage / "ZeroHour" / ("dist-%s-zh" % ver)).exists())
        for ver in ("aaaaaaa1", "bbbbbbb2", "eeeeeee0"):
            self.assertFalse(list(self.dist.glob("*" + ver + "*")), ver)
            self.assertFalse((self.stage / "Generals" / ("dist-%s-gen" % ver)).exists())

    def test_unknown_files_and_lookalikes_are_untouched(self):
        self.run_cleanup("-CurrentVersion", "ddddddd4")
        self.assertTrue((self.dist / "notes-by-hand.txt").exists())
        self.assertTrue((self.dist / "MKRaudstein-Generals-x64-Preview-zzzzzzz9.txt").exists())
        self.assertTrue((self.stage / "Generals" / "x64-48b-gen-v2").exists())
        self.assertTrue((self.stage / "ZeroHour" / "dist-fffffff5-zh").exists())

    def test_dry_run_deletes_nothing(self):
        before = sorted(p.name for p in self.dist.iterdir())
        out = self.run_cleanup("-CurrentVersion", "ddddddd4", "-DryRun")
        self.assertIn("would remove", out)
        self.assertEqual(before, sorted(p.name for p in self.dist.iterdir()))

    def test_no_validated_record_for_the_current_version_deletes_nothing(self):
        before = sorted(p.name for p in self.dist.iterdir())
        self.run_cleanup("-CurrentVersion", "9999999")
        self.assertEqual(before, sorted(p.name for p in self.dist.iterdir()))

    def test_an_unvalidated_record_is_not_a_distribution(self):
        rec = self.dist / "MKRaudstein-x64-Preview-ddddddd4.distribution.json"
        j = json.loads(rec.read_text())
        j["titles"][1]["validated"] = False
        rec.write_text(json.dumps(j))
        before = sorted(p.name for p in self.dist.iterdir())
        self.run_cleanup("-CurrentVersion", "ddddddd4")
        self.assertEqual(before, sorted(p.name for p in self.dist.iterdir()))

    def test_cleanup_cannot_escape_the_distribution_area(self):
        import os
        outside = self.tmp / "outside"
        (outside / "keep").mkdir(parents=True)
        (outside / "ordinaryplay.json").write_text("{}")
        (outside / "MKRaudstein-Generals-x64-Preview-eeeeeee0.zip").write_text("precious")
        # a stage that is a junction to somewhere else, and a candidate name in a record that tries to climb out
        link = self.stage / "Generals" / "dist-abcdef1-gen"
        r = subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(outside)], capture_output=True, text=True) if os.name == "nt" else None
        rec = self.dist / "MKRaudstein-x64-Preview-bbbbbbb2.distribution.json"
        j = json.loads(rec.read_text())
        j["titles"][0]["candidate"] = "..\..\outside"
        rec.write_text(json.dumps(j))
        # an output directory spelled with .. still only touches generated names inside it
        spelled = str(self.dist / ".." / "dist")
        r2 = subprocess.run([PWSH, "-NoProfile", "-File", str(SCRIPT), "-OutputDirectory", spelled, "-StageRoot", str(self.stage), "-CurrentVersion", "ddddddd4"],
                            capture_output=True, text=True)
        self.assertEqual(r2.returncode, 0, r2.stdout + r2.stderr)
        self.assertTrue((outside / "keep").exists())
        self.assertTrue((outside / "ordinaryplay.json").exists())
        self.assertTrue((outside / "MKRaudstein-Generals-x64-Preview-eeeeeee0.zip").exists())
        self.assertTrue(self.tmp.exists())
        self.assertTrue((self.dist / "notes-by-hand.txt").exists())


class Wiring(unittest.TestCase):
    def test_cleanup_runs_only_after_the_distribution_is_ready(self):
        self.assertIn("[switch]$NoCleanup", MAIN)
        self.assertIn("[switch]$CleanupDryRun", MAIN)
        ready = MAIN.index('Write-Host "DISTRIBUTION READY for BOTH titles')
        self.assertGreater(MAIN.index("Remove-ObsoleteDistributions.ps1"), ready)
        self.assertIn("elseif ($SkipValidation)", MAIN)
        self.assertEqual(MAIN.count("Remove-ObsoleteDistributions.ps1"), 1)
        # failed build / package / validation: every step throws (ErrorActionPreference Stop) before the cleanup, which sits after the final "not validated" check
        self.assertIn("$ErrorActionPreference = 'Stop'", MAIN)
        self.assertGreater(MAIN.index("Remove-ObsoleteDistributions.ps1"), MAIN.index("throw 'A title was not validated.'"))
        self.assertGreater(MAIN.index("Remove-ObsoleteDistributions.ps1"), MAIN.index("Test-ZeroHourOverlayPackage.ps1"))
        # -NoCleanup is checked first and never reaches the script
        self.assertLess(MAIN.index("if ($NoCleanup)"), MAIN.index("Remove-ObsoleteDistributions.ps1"))
        self.assertIn("if ($NoCleanup) { Write-Host 'cleanup: skipped (-NoCleanup).' }", MAIN)


if __name__ == "__main__":
    unittest.main()
