#!/usr/bin/env python3
"""Stage asset store (AssetStore.ps1, Optimize-StageStorage.ps1): sharing identical immutable assets by hard links, pruning, and the safety refusals.
Everything runs on a disposable fake dev-runtimes tree under the system temp directory; the real build tree is never touched."""
import hashlib
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL = ROOT / "scripts/native-runtime/Optimize-StageStorage.ps1"
LIB = ROOT / "scripts/native-runtime/AssetStore.ps1"
PWSH = shutil.which("pwsh")
BIG = 1100 * 1024


def payload(seed):
    return hashlib.sha256(seed.encode()).digest() * (BIG // 32 + 1)


def run(*args):
    p = subprocess.run([PWSH, "-NoProfile", "-File", str(TOOL), *args], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def same_file(a, b):
    return os.path.samefile(a, b)


@unittest.skipUnless(PWSH, "pwsh not available")
class AssetStore(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp(prefix="x64store-"))
        self.dev = self.tmp / "dev-runtimes"
        self.stages = []
        for title, name in (("Generals", "a"), ("Generals", "b"), ("ZeroHour", "c")):
            game = self.dev / "native" / title / name / "game"
            game.mkdir(parents=True)
            (game / "Textures.big").write_bytes(payload("textures"))
            (game / "Movie.bik").write_bytes(payload("movie-" + title))
            (game / "generalsv.exe").write_bytes(payload("exe-" + name)[:2 * 1024 * 1024])   # large but an executable: must never be linked
            (game / "small.big").write_bytes(b"tiny")                                       # below the size floor
            self.stages.append(game)
        (self.stages[2] / "Textures.big").write_bytes(payload("different"))   # same name and size, other content

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_dry_run_changes_nothing(self):
        code, out = run("-DevRuntimes", str(self.dev))
        self.assertEqual(code, 0, out)
        self.assertIn("DRY RUN", out)
        self.assertFalse((self.dev / "asset-store").exists())
        self.assertFalse(same_file(self.stages[0] / "Textures.big", self.stages[1] / "Textures.big"))

    def test_execute_shares_identical_content_only(self):
        code, out = run("-DevRuntimes", str(self.dev), "-Execute")
        self.assertEqual(code, 0, out)
        a, b, c = (s / "Textures.big" for s in self.stages)
        self.assertTrue(same_file(a, b))
        self.assertFalse(same_file(a, c), "different content must never be shared")
        self.assertEqual(c.read_bytes(), payload("different"))
        self.assertEqual(a.read_bytes(), payload("textures"))
        self.assertTrue(same_file(self.stages[0] / "Movie.bik", self.stages[1] / "Movie.bik"))
        self.assertFalse(same_file(self.stages[0] / "Movie.bik", self.stages[2] / "Movie.bik"), "movies differ per title in this fixture")
        self.assertFalse(same_file(self.stages[0] / "generalsv.exe", self.stages[1] / "generalsv.exe"))
        self.assertFalse(same_file(self.stages[0] / "small.big", self.stages[1] / "small.big"))
        entries = list((self.dev / "asset-store").glob("*.asset"))
        self.assertTrue(all(len(e.stem) == 64 for e in entries))
        # the stage file hashes exactly as its content
        entry = self.dev / "asset-store" / (hashlib.sha256(payload("textures")).hexdigest().upper() + ".asset")
        self.assertTrue(entry.exists())
        self.assertTrue(same_file(entry, a))

    def test_idempotent_and_no_leftovers(self):
        run("-DevRuntimes", str(self.dev), "-Execute")
        code, out = run("-DevRuntimes", str(self.dev), "-Execute")
        self.assertEqual(code, 0, out)
        self.assertEqual([p for p in self.dev.rglob("*") if ".dedupe-" in p.name or ".tmp-" in p.name], [])

    def test_prune_removes_only_unused_entries(self):
        run("-DevRuntimes", str(self.dev), "-Execute")
        store = self.dev / "asset-store"
        count = len(list(store.glob("*.asset")))
        code, out = run("-DevRuntimes", str(self.dev), "-PruneStore", "-Execute")
        self.assertEqual(code, 0, out)
        self.assertEqual(len(list(store.glob("*.asset"))), count, "entries still linked by stages must stay")
        shutil.rmtree(self.dev / "native" / "Generals" / "a")
        run("-DevRuntimes", str(self.dev), "-PruneStore", "-Execute")
        self.assertEqual(len(list(store.glob("*.asset"))), count, "stage b still uses them")
        shutil.rmtree(self.dev / "native" / "Generals" / "b")
        code, out = run("-DevRuntimes", str(self.dev), "-PruneStore")
        self.assertIn("would prune", out)
        self.assertEqual(len(list(store.glob("*.asset"))), count, "dry run prunes nothing")
        run("-DevRuntimes", str(self.dev), "-PruneStore", "-Execute")
        remaining = {e.stem for e in store.glob("*.asset")}
        self.assertNotIn(hashlib.sha256(payload("textures")).hexdigest().upper(), remaining)
        self.assertEqual(self.stages[2].joinpath("Textures.big").read_bytes(), payload("different"))

    def test_refusals(self):
        code, out = run("-DevRuntimes", str(self.tmp))   # not named dev-runtimes
        self.assertNotEqual(code, 0)
        steam = self.tmp / "Steam" / "dev-runtimes"
        (steam / "native").mkdir(parents=True)
        code, out = run("-DevRuntimes", str(steam))
        self.assertNotEqual(code, 0)
        self.assertIn("Steam", out)
        code, out = run("-DevRuntimes", str(self.tmp / "missing" / "dev-runtimes"))
        self.assertNotEqual(code, 0)

    def test_stager_helper_links_and_falls_back(self):
        script = (
            ". '%s'\n"
            "$src='%s'; $dst='%s'; $dev='%s'\n"
            "$h=(Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash\n"
            "$a=New-StoredAssetLink $src $dst $h $dev\n"
            "$small='%s'; $b=New-StoredAssetLink $small '%s' (Get-FileHash -LiteralPath $small).Hash $dev\n"
            "Write-Output \"linked=$a small=$b\"\n"
        ) % (LIB, self.stages[0] / "Textures.big", self.tmp / "stagedcopy.big", self.dev, self.stages[0] / "small.big", self.tmp / "smallcopy.big")
        p = subprocess.run([PWSH, "-NoProfile", "-Command", script], capture_output=True, text=True)
        self.assertIn("linked=True small=False", p.stdout + p.stderr)
        self.assertEqual((self.tmp / "stagedcopy.big").read_bytes(), payload("textures"))
        self.assertTrue(same_file(self.tmp / "stagedcopy.big", self.stages[0] / "Textures.big") is False)   # linked to the store entry (a copy of the source), not to the source
        self.assertFalse((self.tmp / "smallcopy.big").exists(), "ineligible files are left to the caller's copy")

    def test_free_space_guard(self):
        script = ". '%s'\ntry { Assert-FreeDiskSpace '%s' 99999999 'the test'; 'no-throw' } catch { 'threw: ' + $_.Exception.Message }\nAssert-FreeDiskSpace '%s' 0.001 'the test'; 'ok'\n" % (LIB, self.tmp, self.tmp)
        p = subprocess.run([PWSH, "-NoProfile", "-Command", script], capture_output=True, text=True)
        self.assertIn("threw: Not enough free disk space for the test", p.stdout)
        self.assertIn("ok", p.stdout)


if __name__ == "__main__":
    unittest.main()
