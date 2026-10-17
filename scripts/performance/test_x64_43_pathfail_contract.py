#!/usr/bin/env python3
"""X64.43 contract: the failing-search diagnostic (PathFailureDiag) is read-only and inert unless GENERALS_SIM_PATHFAIL is set.

Structural checks on the sources (the exactness of the pathfinder itself is covered by the Stage4B.1 function-level guard, which treats these hooks as
instrumentation, and by the unchanged determinism fingerprint) plus mutation tests proving the checks bite.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DIAG = ROOT / "Core" / "GameEngine" / "Source" / "GameLogic" / "AI" / "PathFailureDiag.cpp"
PATHFIND = ROOT / "Core" / "GameEngine" / "Source" / "GameLogic" / "AI" / "AIPathfind.cpp"

# Mutators of search / pathfinder / object state that the diagnostic must never call.
FORBIDDEN_CALLS = [
    "allocateInfo", "releaseInfo", "setCostSoFar", "setTotalCost", "setParentCell", "clearParentCell", "putOnSortedOpenList", "putOnClosedList",
    "removeFromOpenList", "removeFromClosedList", "setBlockedByAlly", "setType", "setPinched", "setZone", "setGoalUnit", "setGoalAircraft", "setPosUnit",
    "setLayer", "cleanOpenAndClosedLists", "queueForPath", "startPathfind", "GameLogicRandomValue", "GameClientRandomValue", "setIgnoreObstacleID",
    "updateGoal", "updatePos", "removeGoal", "removePos", "calculateZones", "clearPassableFlags", "setAllPassable", "newObject", "destroyObject",
]


def diag_problems(source):
    found = []
    for name in FORBIDDEN_CALLS:
        if re.search(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"\s*\(", source):
            found.append("diagnostic calls the mutator " + name)
    if re.search(r"\bpf\s*\.\s*m_\w+(\.\w+)?\s*(=|\+=|-=|\+\+|--)(?!=)", source):
        found.append("diagnostic assigns to a Pathfinder member")
    if "getenv(\"GENERALS_SIM_PATHFAIL\")" not in source:
        found.append("the GENERALS_SIM_PATHFAIL switch is missing")
    return found


def hook_problems(source):
    found = []
    for line in source.splitlines():
        if "PathFailureDiag::" not in line or line.lstrip().startswith("//") or line.lstrip().startswith("#include"):
            continue
        st = line.strip()
        if "noteMapMutation" in st:
            continue            # increments a counter only when the switch is on
        if "PathFailureDiag::Search failDiag" in st:
            continue            # default-constructed, inert
        if "setHierarchical" in st or "noteZoneRecalculation" in st or "failDiag.begin" in st:
            if "PathFailureDiag::enabled()" not in st:
                found.append("unguarded hook: " + st[:90])
            continue
        found.append("unexpected PathFailureDiag use: " + st[:90])
    for line in source.splitlines():
        if "failDiag.failed(" in line and "failDiag.active" not in line:
            found.append("failed() is not guarded by failDiag.active")
    return found


class PathFailDiagContract(unittest.TestCase):
    def setUp(self):
        self.diag = DIAG.read_text(encoding="utf-8")
        self.path = PATHFIND.read_text(encoding="utf-8")

    def test_the_diagnostic_is_read_only(self):
        self.assertEqual(diag_problems(self.diag), [])

    def test_the_hooks_are_guarded(self):
        self.assertEqual(hook_problems(self.path), [])

    def test_the_diagnostic_is_registered_off_by_default(self):
        self.assertIn("g_state = getenv(\"GENERALS_SIM_PATHFAIL\") ? 1 : 0;", self.diag)
        header = (ROOT / "Core" / "GameEngine" / "Include" / "GameLogic" / "Pathfinder" / "PathFailureDiag.h").read_text(encoding="utf-8")
        self.assertIn("inline void noteMapMutation() { if (g_state > 0)", header)

    def test_mutation_a_mutating_call_is_detected(self):
        mutated = self.diag.replace("++r.closed;", "++r.closed; c->setCostSoFar(0);", 1)
        self.assertNotEqual(mutated, self.diag)
        self.assertTrue(diag_problems(mutated))

    def test_mutation_a_pathfinder_member_write_is_detected(self):
        mutated = self.diag.replace("++r.closed;", "++r.closed; pf.m_isTunneling = false;", 1)
        self.assertNotEqual(mutated, self.diag)
        self.assertTrue(diag_problems(mutated))

    def test_mutation_random_number_use_is_detected(self):
        mutated = self.diag.replace("++r.closed;", "++r.closed; GameLogicRandomValue(0, 1);", 1)
        self.assertTrue(diag_problems(mutated))

    def test_mutation_an_unguarded_hook_is_detected(self):
        mutated = self.path.replace("if (PathFailureDiag::enabled()) PathFailureDiag::setHierarchical(hPat ? 1 : 0);", "PathFailureDiag::setHierarchical(hPat ? 1 : 0);", 1)
        self.assertNotEqual(mutated, self.path)
        self.assertTrue(hook_problems(mutated))

    def test_mutation_an_unguarded_failed_call_is_detected(self):
        mutated = self.path.replace("if (failDiag.active) failDiag.failed(cellCount);", "failDiag.failed(cellCount);", 1)
        self.assertNotEqual(mutated, self.path)
        self.assertTrue(hook_problems(mutated))


if __name__ == "__main__":
    unittest.main()
