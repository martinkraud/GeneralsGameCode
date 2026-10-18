# SPDX-License-Identifier: GPL-3.0-or-later
# X64.48B: the distribution rule  "validated staged runtime == clean legitimate installation + overlay ZIP"  as source contracts.
# The builder must DERIVE the package from the canonical staged candidate (no hand-written allowlist, no size/file-count goal) and the validator must
# FAIL when a project runtime file is missing from the ZIP or a completed feature differs after the overlay install. The behaviour itself is
# measured by Test-ZeroHourOverlayPackage.ps1 on a disposable layout (see X64_48B_COMPLETE_RUNTIME_PARITY.md).
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


class OverlayPackageContract(unittest.TestCase):
    def test_builder_derives_the_contents_from_the_staged_candidate(self):
        b = read("scripts/native-runtime/New-ZeroHourOverlayPackage.ps1")
        self.assertIn("[Parameter(Mandatory)][string]$Candidate", b)
        self.assertIn("Read-NativeOrdinaryPlayStage $Title $Candidate", b)       # an intact, preflighted stage
        self.assertIn("Get-ChildItem -LiteralPath $game -Recurse -File", b)       # every file of the stage is classified
        self.assertIn("$legit.ContainsKey($key)", b)                              # unchanged legitimate files are the only exclusion
        # no hand-written list of project files and no size / file-count goal
        for forbidden in ("Data/INI/CommandButton.ini'", "GenPowersShortcutBarUS.wnd'", "maximum files", "minimum", "$ExePath"):
            self.assertNotIn(forbidden, b.replace("Data/INI/(CommandButton|CommandSet|PlayerTemplate)", ""))
        self.assertIn("project-generated / project-modified runtime data", b)
        self.assertIn(".stage-parity.json", b)

    def test_validator_fails_on_missing_project_files_and_feature_differences(self):
        v = read("scripts/native-runtime/Test-ZeroHourOverlayPackage.ps1")
        for needle in ("no project runtime file of the staged candidate is missing from the ZIP", "FILE-FOR-FILE: clean installation + ZIP == the staged candidate",
                       "FEATURE PARITY: the feature probe of the overlay install equals the staged candidate's",
                       "no unchanged legitimate game file is redistributed", "the disposable installation is clean",
                       "a fresh staging of the same build is file-for-file identical to the canonical staged candidate",
                       "if ($failed.Count) { throw"):
            self.assertIn(needle, v)
        self.assertIn("[Parameter(Mandatory)][string]$Candidate", v)

    def test_runtime_reports_the_feature_data_it_runs_with(self):
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        probe = host[host.index('"feature_probe"') - 1500:host.index('"feature_probe"') + 200]
        for needle in ("getSpecialPowerShortcutButtonCount", "getSpecialPowerShortcutCommandSet", "findCommandSet", "isPlayableSide"):
            self.assertIn(needle, probe)

    def test_generals_modern_extension_data_is_part_of_the_runtime_contract(self):
        # the stager generates these loose files for Generals; the package must carry exactly what the stage carries (validated live)
        stage = read("scripts/native-runtime/Stage-NativeInitOnly.ps1")
        self.assertIn("modern_extensions.py", stage)
        ext = read("scripts/native-runtime/modern_extensions.py")
        for name in ("Data/INI/CommandButton.ini", "Data/INI/CommandSet.ini", "Data/INI/PlayerTemplate.ini", "GenPowersShortcutBar"):
            self.assertIn(name, ext)

    def test_one_distribution_command_covers_both_titles(self):
        d = read("scripts/native-runtime/New-MKRaudsteinDistribution.ps1")
        for needle in ("'Generals'", "'ZeroHour'", "New-ZeroHourOverlayPackage.ps1", "Test-ZeroHourOverlayPackage.ps1", "SHA256SUMS", ".distribution.json",
                       "DISTRIBUTION READY for BOTH titles", "A title was not validated"):
            self.assertIn(needle, d)
        # one invocation, no required per-title parameter: the candidates are optional and staged from the current build when omitted
        self.assertNotIn("[Parameter(Mandatory)]", d)
        self.assertIn("Stage-NativeOrdinaryPlay.ps1", d)


if __name__ == "__main__":
    unittest.main()
