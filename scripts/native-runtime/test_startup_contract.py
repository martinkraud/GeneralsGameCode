"""Source boundary/preservation tests; never execute a title."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "4a36a0cd2c2e3b2bc7fa6e579dd206aeaa7619d3"

def read(path):
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")

class StartupContract(unittest.TestCase):
    def test_late_observer_is_armed_at_return_and_launcher_requires_real_success(self):
        for title in ("Generals", "GeneralsMD"):
            s = read(f"{title}/Code/Main/WinMain.cpp")
            shutdown = s.rindex('checkpoint("minidumper_shutdown", "complete")')
            arm = s.index("NativeShutdownDiagnostics::arm()", shutdown)
            complete = s.index('checkpoint("WinMain", exitcode', arm)
            self.assertLess(shutdown, arm)
            self.assertLess(arm, complete)
            self.assertIn("if (!NativeShutdownDiagnostics::armed()) StartupDiagnostics::close();", s[complete:])
        launch = read("scripts/native-runtime/Invoke-NativeInitOnly.ps1")
        self.assertIn("$exit -eq 0", launch)
        self.assertIn("$shutdownBytes -eq 0", launch)
        self.assertIn('!$log.Contains("native_shutdown`texception")', launch)

    def test_both_init_boundaries_precede_execute_and_replay(self):
        for title in ("Generals", "GeneralsMD"):
            s = read(f"{title}/Code/GameEngine/Source/Common/GameMain.cpp")
            init = s.index("TheGameEngine->init();")
            gate = s.index("if (StartupDiagnostics::enabled())", init)
            replay = s.index("else if (!TheGlobalData->m_simulateReplays.empty())", gate)
            execute = s.index("TheGameEngine->execute();", replay)
            cleanup = s.index("delete TheGameEngine;", execute)
            self.assertLess(init, gate)
            self.assertNotIn("execute();", s[gate:replay])
            self.assertNotIn("simulateReplays(", s[gate:replay])
            self.assertNotIn("return ", s[gate:replay])
            self.assertGreater(cleanup, execute)

    def test_win32_fp_sequence_is_preserved(self):
        for title in ("Generals", "GeneralsMD"):
            path = f"{title}/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp"
            old = subprocess.check_output(["git", "show", f"{BASELINE}:{path}"], cwd=ROOT).decode("latin1").replace("\r\n", "\n")
            def body(s):
                start = s.index("void setFPMode()\n{")
                return s[start:s.index("\n}", start)]
            preserved = body(read(path)).split("#else\n", 1)[1].rsplit("\n#endif", 1)[0]
            self.assertEqual(preserved, body(old).split("{\n", 1)[1])

    def test_configured_before_globaldata_and_reasserted_after_ini(self):
        s = read("Core/GameEngine/Source/Common/CommandLine.cpp")
        start = s.index("void CommandLine::parseCommandLineForStartup()")
        self.assertLess(s.index("StartupDiagnostics::configure", start), s.index("createGlobalData();", start))
        self.assertEqual(s.count("if(StartupDiagnostics::enabled()) { parseHeadless(nullptr, 0);"), 2)

    def test_userdata_overrides_precede_write(self):
        for title, factory in (("Generals", "BuildUserDataPathFromIni"), ("GeneralsMD", "BuildUserDataPathFromRegistry")):
            s = read(f"{title}/Code/GameEngine/Source/Common/GlobalData.cpp")
            expression = f"StartupDiagnostics::userData() ? AsciiString(StartupDiagnostics::userData()) : {factory}();"
            self.assertIn(expression, s)
            at = s.index(expression)
            self.assertIn("CreateDirectory", s[at:at+300])

    def test_crash_filter_retained_and_failure_catches_logged(self):
        for title in ("Generals", "GeneralsMD"):
            s = read(f"{title}/Code/Main/WinMain.cpp")
            self.assertIn("SetUnhandledExceptionFilter( UnHandledExceptionFilter );", s)
            self.assertIn("MiniDumper::initMiniDumper(TheGlobalData->getPath_UserData());", s)
            self.assertIn('checkpoint("WinMain", "fail", "caught exception")', s)
            self.assertIn("MiniDumper::shutdownMiniDumper();", s)

    def test_factory_unmodified(self):
        path = "Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.cpp"
        old = subprocess.check_output(["git", "show", f"{BASELINE}:{path}"], cwd=ROOT)
        self.assertEqual((ROOT/path).read_bytes().replace(b"\r\n",b"\n"), old.replace(b"\r\n",b"\n"))

    def test_base_assets_route_is_scoped_to_diagnostic(self):
        s = read("Core/GameEngineDevice/Source/Win32Device/Common/Win32BIGFileSystem.cpp")
        self.assertIn('if (StartupDiagnostics::enabled()) installPath = "ZH_Generals\\\\";', s)
        self.assertIn('else GetStringFromGeneralsRegistry("", "InstallPath", installPath );', s)
        self.assertIn('loadBigFilesFromDirectory(installPath, "*.big")', s)

if __name__ == "__main__":
    unittest.main()
