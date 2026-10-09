"""Deterministic synthetic fixtures; no game or real capture is modified."""
import copy
import csv
import json
import tempfile
import subprocess
import sys
import unittest
from pathlib import Path
import compare


def trial():
    data = {key: 1 for key in compare.MATCH_FIELDS + compare.CORRECTNESS}
    data.update(status="complete", reason="", source="fixture", build="synthetic", dirty=0,
                scenario="pathfinding-heavy", title="fixture", map="fixture", configuration="fixture",
                warmup_ticks=90, capture_ticks=300, spawned_units=96, start_tick=90, end_tick=390,
                errors=0, dropped=0, internal_searches=1, internal_over40ms=0, objects_end=312, queue_max=96, elapsed_seconds=10.5, overlay=False, records=1, outer_frames=300,
                completed_logic_ticks=300, requested_fps=120, effective_fps=120, report_stem="test",
                coverage={"internal_over40ms": 3, "internal_pops_max": 10000},
                representativeness={"representative": True})
    data["caps"] = (120, 120)
    data["metrics"] = {"logic_p95_ms": 100, "logic_max_ms": 110}
    return data


def write_fixture(folder):
    data = trial()
    frames = [{"outer_index": i, "completed_logic_ticks": 1, "requested_fps": 120, "effective_fps": 120,
               "game_logic_update_inclusive_ms": 10} for i in range(300)]
    paths = [dict(id=1, parent_id=0, outer_index=0, logic_before=90, object_id=5, kind="internal",
                  request_context="request", outcome="returned_path", from_x=1090.5, from_y=1030.28, to_x=3870.5, to_y=1100.5,
                  exclusive_ms=11, inclusive_ms=11, phase_errors=0, phase_iterations=64, phase_selected=1,
                  line_sample_calls=1, neighbor_sample_calls=1, line_sample_inclusive_ms=.1, line_sample_insertion_ms=.01,
                  neighbor_sample_inclusive_ms=.1, neighbor_sample_insertion_ms=.01)]
    for key in "open_head_pops info_attempts info_new info_failed open_inserts forward_hops reverse_hops cleaned_cells block_zone_queries hierarchy_fallbacks".split():
        paths[0][key + "_inclusive_count"] = 64 if key == "open_head_pops" else 0
    data["path_fingerprint"] = compare.semantic_hash(paths, 90)
    (folder / "test-benchmark.json").write_text(json.dumps(data))
    summary = dict(mode="scenario_ticks", path_detail_enabled=1, path_phase_enabled=1, path_phase_stride=64,
                   path_phase_errors=0, stack_or_clock_errors=0, path_detail_dropped=0, outer_frames=300,
                   path_detail_records=1, ground_interpolation=1, git="fixture", dirty=0, completed_logic_ticks=300)
    (folder / "test-summary.txt").write_text("\n".join(f"{k}={v}" for k, v in summary.items()))
    for name, rows in (("frames", frames), ("paths", paths), ("categories", [{"category": "fixture"}]), ("slow", [{"ranking": "fixture"}])):
        with (folder / ("test-" + name + ".csv")).open("w", newline="") as stream:
            writer = csv.DictWriter(stream, rows[0].keys()); writer.writeheader(); writer.writerows(rows)


class ComparisonTest(unittest.TestCase):
    def test_valid_fixture_reads_all_reports(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory); write_fixture(folder)
            data = compare.load(folder)
            self.assertEqual(data["metrics"]["logic_p95_ms"], 10)
            self.assertEqual(data["coverage"]["internal"], 1)

    def test_fractional_coordinates_and_strict_neighbor_schema(self):
        self.assertEqual(compare.coordinate_word("1090.5"), 1090)
        # Six significant digits would print 1024, crossing the digest boundary.
        self.assertEqual(compare.coordinate_word("1023.99994"), 1023)
        for value in ("1090.5", "1e3", True, 1.5):
            with self.assertRaises(ValueError): compare.integer(value)
        compare.numeric_rows([dict(from_x="1090.5", inclusive_ms="0.125", queue_before="-1", phase_selected="17")])
        for key in ("open_head_pops_inclusive_count", "phase_selected", "line_sample_inserts", "queue_before", "object_id"):
            with self.assertRaises(ValueError): compare.numeric_rows([{key: "1090.5"}])
        for value in ("nan", "inf", "1,5"):
            with self.assertRaises(ValueError): compare.measurement(value)
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory); write_fixture(folder)
            compare.load(folder)  # Real coordinate-style values reach the unchanged fingerprint gate.
            file = folder / "test-benchmark.json"
            original = json.loads(file.read_text())
            for key in ("records", "outer_frames", "start_crc", "rng_end", "queue_max"):
                broken = dict(original); broken[key] = 1090.5; file.write_text(json.dumps(broken))
                with self.assertRaisesRegex(ValueError, "integer metadata"): compare.load(folder)

    def test_lossy_coordinate_report_still_fails_fingerprint(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory); write_fixture(folder)
            file = folder / "test-paths.csv"
            with file.open() as stream: records = list(csv.DictReader(stream))
            records[0]["from_x"] = "1023.99994"
            meta = folder / "test-benchmark.json"; data = json.loads(meta.read_text())
            data["path_fingerprint"] = compare.semantic_hash(records, 90); meta.write_text(json.dumps(data))
            def write():
                with file.open("w", newline="") as stream:
                    writer = csv.DictWriter(stream, records[0].keys()); writer.writeheader(); writer.writerows(records)
            write(); compare.load(folder)
            records[0]["from_x"] = "1024"; write()
            with self.assertRaisesRegex(ValueError, "fingerprint mismatch"): compare.load(folder)

    def test_incomplete_and_fingerprint_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory); write_fixture(folder)
            path = folder / "test-benchmark.json"
            data = json.loads(path.read_text()); data["path_fingerprint"] = "tampered"; path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "fingerprint"):
                compare.load(folder)
            (folder / "benchmark-failure.json").write_text("{}")
            with self.assertRaisesRegex(ValueError, "failure"):
                compare.load(folder)

    def test_failure_reason_reaches_cli_without_terminal_control_characters(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory)
            (folder/"benchmark-failure.json").write_text(json.dumps(dict(reason="workload_goal_changed_or_unreachable",
                failure_check="goal_relocated_after_warmup",logic_tick=91,orders_issued=76,failed_unit_index=76)))
            command=[sys.executable,"-B",str(Path(compare.__file__)),"--validate",directory]
            failed=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(failed.returncode,3)
            reason=json.loads(failed.stdout)["reason"]
            self.assertIn("goal_relocated_after_warmup",reason);self.assertIn("91",reason);self.assertIn("76",reason)
            (folder/"benchmark-failure.json").write_text(json.dumps(dict(reason="bad\x1b[31m")))
            with self.assertRaises(ValueError) as error:compare.load(folder)
            self.assertNotIn("\x1b",str(error.exception))

    def test_missing_reports_and_oversized_storage_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory); write_fixture(folder); (folder / "test-slow.csv").unlink()
            with self.assertRaises(OSError): compare.load(folder)
            path = folder / "large"
            with path.open("wb") as stream: stream.truncate(compare.MAX_BYTES + 1)
            with self.assertRaisesRegex(ValueError, "oversized"): compare.read_text(path)

    def test_config_crc_and_workload_qualification(self):
        a = trial(); b = copy.deepcopy(a); b["end_crc"] += 1
        self.assertEqual(compare.compare([a], [b])["verdict"], "not_comparable")
        b = copy.deepcopy(a); b["caps"] = (60, 60)
        self.assertEqual(compare.compare([a], [b])["verdict"], "not_comparable")
        a["representativeness"]["representative"] = False
        self.assertEqual(compare.compare([a], [a])["verdict"], "not_comparable")

    def test_repeated_trials_descriptive_verdicts(self):
        a = trial(); b = copy.deepcopy(a)
        self.assertEqual(compare.compare([a], [b])["verdict"], "unchanged_within_noise")
        b["metrics"]["logic_p95_ms"] = 80
        result = compare.compare([a, a, a], [b, b, b]); self.assertEqual(result["verdict"], "improvement")
        self.assertEqual(result["statistical_significance"], "not_established")
        b["metrics"]["logic_max_ms"] = 200
        self.assertEqual(compare.compare([a], [b])["verdict"], "regression")

    def test_representativeness_is_separate_and_rejects_closest_dominance(self):
        internal = dict(kind="internal", open_head_pops_inclusive_count="27722", info_attempts_inclusive_count="643559",
                        line_sample_calls="400", neighbor_sample_calls="400", exclusive_ms="40")
        representative = [dict(internal) for _ in range(96)]
        self.assertTrue(compare.workload_qualification(representative)["representative"])
        # A legitimate speedup/preemption cannot fail structural qualification.
        representative += [dict(kind="closest",exclusive_ms="10000",open_head_pops_inclusive_count="1")]
        self.assertTrue(compare.workload_qualification(representative)["representative"])
        # Wall durations may change substantially without changing work or qualification.
        for p in representative: p["exclusive_ms"] = "4"
        self.assertTrue(compare.workload_qualification(representative)["representative"])
        for p in representative[:94]: p["info_attempts_inclusive_count"] = "100"
        self.assertFalse(compare.workload_qualification(representative)["representative"])
        old = [dict(internal, exclusive_ms=str(375.9172/307),
                    open_head_pops_inclusive_count=str(357717//307+(i<357717%307)),
                    info_attempts_inclusive_count="391775") for i in range(307)]
        old += [dict(kind="closest", exclusive_ms=str(5929.856/223),
                     open_head_pops_inclusive_count=str(2934492//223+(i<2934492%223))) for i in range(223)]
        result = compare.workload_qualification(old)
        self.assertFalse(result["representative"]); self.assertAlmostEqual(result["internal_self_share"], .05961477)
        self.assertEqual(result["large_internal_searches"], 0)
        self.assertAlmostEqual(result["internal_head_pop_share"], .10865561694)
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);write_fixture(folder)
            loaded=compare.load(folder) # Correctness can pass while qualification fails.
            self.assertFalse(loaded["representativeness"]["representative"])
        phases=[dict(internal,line_sample_calls="0") for _ in range(96)]
        self.assertFalse(compare.workload_qualification(phases)["representative"])

    def test_cli_correctness_and_qualification_have_separate_exit_codes(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);write_fixture(folder)
            command=[sys.executable,"-B",str(Path(compare.__file__))]
            valid=subprocess.run(command+["--validate",directory],capture_output=True,text=True)
            self.assertEqual(valid.returncode,0,valid.stdout)
            qualified=subprocess.run(command+["--qualify",directory],capture_output=True,text=True)
            self.assertEqual(qualified.returncode,2,qualified.stdout)
            self.assertEqual(json.loads(qualified.stdout)["verdict"],"not_representative")
            self.assertIn("representativeness",json.loads(valid.stdout))

    def test_invalid_parent_and_negative_phase_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory); write_fixture(folder)
            file = folder / "test-paths.csv"
            with file.open() as stream: rows = list(csv.DictReader(stream))
            rows[0]["parent_id"] = "99"
            with file.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, rows[0].keys()); writer.writeheader(); writer.writerows(rows)
            with self.assertRaisesRegex(ValueError, "parent"): compare.load(folder)
            rows[0]["parent_id"] = "0"; rows[0]["line_sample_insertion_ms"] = "100"
            with file.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, rows[0].keys()); writer.writeheader(); writer.writerows(rows)
            with self.assertRaisesRegex(ValueError, "phase self"): compare.load(folder)


if __name__ == "__main__":
    unittest.main()
