#!/usr/bin/env python3
"""X64.42 contract: the normal-launch log must never flush to disk from gameplay code.

StartupDiagnostics::checkpoint is called from the simulation (special power activation, payload delivery, group dispatch). A
FlushFileBuffers per record is a synchronous disk flush on the logic thread: 10-85 ms per superweapon fire, the periodic 70-110 ms
"script tick" and the 55 ms update-module spikes of the heavy benchmark. The evidence-harness modes (enabled()) still flush every
record, and GENERALS_NORMAL_LOG_FLUSH=1 restores the flush in a normal launch.

The check is a structural contract on the source plus mutation tests proving that it bites.
"""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "Core" / "GameEngine" / "Include" / "Common" / "StartupDiagnostics.h"


def checkpoint_body(source):
    start = source.index("inline void checkpoint(")
    end = source.index("inline bool configure(", start)
    return source[start:end]


def problems(source):
    found = []
    body = checkpoint_body(source)
    if "GENERALS_NORMAL_LOG_FLUSH" not in body:
        found.append("the A/B / forensic switch GENERALS_NORMAL_LOG_FLUSH is missing from checkpoint()")
    flush = re.search(r"const bool flush\s*=\s*([^;]+);", body)
    if not flush:
        found.append("checkpoint() has no 'flush' decision")
    else:
        expr = flush.group(1)
        for needed in ("enabled()", "!s.normalLog", "flushNormalLog"):
            if needed not in expr:
                found.append("flush decision lacks %s: %s" % (needed, expr.strip()))
    calls = [m.start() for m in re.finditer(r"FlushFileBuffers\(", body)]
    if len(calls) != 1:
        found.append("checkpoint() must flush through exactly one guarded call, found %d" % len(calls))
    else:
        window = body[max(0, calls[0] - 60):calls[0]]
        if "flush &&" not in window:
            found.append("the FlushFileBuffers call is not guarded by 'flush &&'")
    return found


class StartupLogContract(unittest.TestCase):
    def setUp(self):
        self.source = HEADER.read_text(encoding="utf-8")

    def test_current_source_satisfies_the_contract(self):
        self.assertEqual(problems(self.source), [])

    def test_mutation_unconditional_flush_is_detected(self):
        mutated = self.source.replace("(flush && !FlushFileBuffers(s.file))", "!FlushFileBuffers(s.file)")
        self.assertNotEqual(mutated, self.source)
        self.assertTrue(problems(mutated))

    def test_mutation_flush_always_true_is_detected(self):
        mutated = re.sub(r"const bool flush\s*=\s*[^;]+;", "const bool flush = true;", self.source)
        self.assertNotEqual(mutated, self.source)
        self.assertTrue(problems(mutated))

    def test_mutation_harness_modes_no_longer_flush_is_detected(self):
        mutated = self.source.replace("const bool flush = enabled() || ", "const bool flush = ")
        self.assertNotEqual(mutated, self.source)
        self.assertTrue(problems(mutated))

    def test_mutation_missing_switch_is_detected(self):
        mutated = self.source.replace("GENERALS_NORMAL_LOG_FLUSH", "GENERALS_SOMETHING_ELSE")
        self.assertTrue(problems(mutated))


if __name__ == "__main__":
    unittest.main()
