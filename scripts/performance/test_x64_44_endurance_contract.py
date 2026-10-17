#!/usr/bin/env python3
"""X64.44 contract: EnduranceTelemetry is read-only, inert unless GENERALS_ENDURANCE is set, and never flushes to disk on the logic thread.

Structural checks plus mutation tests proving the checks bite. The behaviour of the telemetry on a deterministic replay (fingerprint unchanged with it
on) is verified separately with the headless benchmark.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TELEMETRY = ROOT / "Core" / "GameEngine" / "Source" / "Common" / "EnduranceTelemetry.cpp"
HEADER = ROOT / "Core" / "GameEngine" / "Include" / "Common" / "EnduranceTelemetry.h"
ENGINES = [ROOT / "Generals" / "Code" / "GameEngine" / "Source" / "Common" / "GameEngine.cpp", ROOT / "GeneralsMD" / "Code" / "GameEngine" / "Source" / "Common" / "GameEngine.cpp"]

FORBIDDEN_CALLS = [
    "FlushFileBuffers", "fflush", "_commit", "fsync",                                                        # X64.42: no synchronous durability on the logic thread
    "GameLogicRandomValue", "GameClientRandomValue", "GameLogicRandomValueReal", "newObject", "destroyObject",
    "setPosition", "setStatus", "kill", "attemptDamage", "aiIdle", "aiMoveToPosition", "queueForPath", "setIgnoreObstacleID",
    "setFrame", "setGamePaused", "setTimeFrozen", "setLogicTimeScaleFps", "setFramesPerSecondLimit", "calculateZones",
]


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def telemetry_problems(source):
    found = []
    source = strip_comments(source)
    for name in FORBIDDEN_CALLS:
        if re.search(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"\s*\(", source):
            found.append("telemetry calls " + name)
    if "WriteFile(" not in source:
        found.append("output is not written with WriteFile")
    if 'getenv("GENERALS_ENDURANCE")' not in source:
        found.append("the GENERALS_ENDURANCE switch is missing")
    return found


def engine_problems(source):
    found = []
    m = re.search(r"\{\s*EnduranceTelemetry::LogicTick enduranceTick;\s*TheGameLogic->UPDATE\(\);\s*\}", source)
    if not m:
        found.append("TheGameLogic->UPDATE() is not wrapped by exactly the LogicTick scope")
    if source.count("TheGameLogic->UPDATE();") != 1:
        found.append("unexpected number of TheGameLogic->UPDATE() calls")
    if "EnduranceTelemetry::engineFrame();" not in source:
        found.append("engineFrame() hook missing")
    for line in source.splitlines():
        if "EnduranceTelemetry::" in line and "engineFrame" not in line and "LogicTick" not in line and "#include" not in line:
            found.append("unexpected telemetry use: " + line.strip()[:80])
    return found


class EnduranceContract(unittest.TestCase):
    def setUp(self):
        self.src = TELEMETRY.read_text(encoding="utf-8")

    def test_the_telemetry_is_read_only_and_never_flushes(self):
        self.assertEqual(telemetry_problems(self.src), [])

    def test_the_header_is_off_by_default(self):
        header = HEADER.read_text(encoding="utf-8")
        self.assertIn("inline bool enabled() { return g_state > 0 ? true : (g_state == 0 ? false : initState()); }", header)
        self.assertIn("if (!enabled()", self.src)

    def test_the_engine_hooks_wrap_only_the_logic_update(self):
        for path in ENGINES:
            self.assertEqual(engine_problems(path.read_text(encoding="utf-8")), [], str(path))

    def test_mutation_a_flush_is_detected(self):
        mutated = self.src.replace("WriteFile(h, text.data()", "FlushFileBuffers(h); WriteFile(h, text.data()", 1)
        self.assertNotEqual(mutated, self.src)
        self.assertTrue(telemetry_problems(mutated))

    def test_mutation_an_fflush_is_detected(self):
        mutated = self.src.replace("++s.rows;", "++s.rows; fflush(stdout);", 1)
        self.assertNotEqual(mutated, self.src)
        self.assertTrue(telemetry_problems(mutated))

    def test_mutation_a_random_number_use_is_detected(self):
        mutated = self.src.replace("++s.rows;", "++s.rows; GameLogicRandomValue(0, 1);", 1)
        self.assertTrue(telemetry_problems(mutated))

    def test_mutation_a_state_write_is_detected(self):
        mutated = self.src.replace("++s.rows;", "++s.rows; TheGameLogic->setGamePaused(TRUE); setStatus(0);", 1)
        self.assertTrue(telemetry_problems(mutated))

    def test_mutation_an_unwrapped_update_is_detected(self):
        src = ENGINES[0].read_text(encoding="utf-8")
        mutated = src.replace("EnduranceTelemetry::LogicTick enduranceTick;", "", 1)
        self.assertNotEqual(mutated, src)
        self.assertTrue(engine_problems(mutated))

    def test_mutation_an_extra_update_call_is_detected(self):
        src = ENGINES[1].read_text(encoding="utf-8")
        mutated = src.replace("TheGameLogic->UPDATE();", "TheGameLogic->UPDATE(); TheGameLogic->UPDATE();", 1)
        self.assertTrue(engine_problems(mutated))


if __name__ == "__main__":
    unittest.main()
