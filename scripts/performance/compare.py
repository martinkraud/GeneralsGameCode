"""Bounded Stage 4A.3 benchmark validation/comparison. Python standard library only."""
import argparse
import csv
import json
import math
import statistics
import re
import struct
from pathlib import Path

MAX_BYTES = 32 * 1024 * 1024
MATCH_FIELDS = ("schema", "scenario", "version", "title", "map", "map_crc", "seed",
                "configuration", "warmup_ticks", "capture_ticks", "order_period", "spawned_units",
                "interpolation", "path_detail", "path_phase", "overlay", "actual_seed", "requested_fps", "effective_fps")
CORRECTNESS = ("start_tick", "end_tick", "start_crc", "end_crc", "rng_start", "rng_end", "path_fingerprint")


def read_text(path):
    if path.stat().st_size > MAX_BYTES:
        raise ValueError(f"oversized report: {path.name}")
    return path.read_text(encoding="utf-8-sig")


def rows(path, limit):
    import io
    result = []
    for row in csv.DictReader(io.StringIO(read_text(path))):
        if len(result) >= limit:
            raise ValueError(f"too many rows: {path.name}")
        result.append(row)
    return result


def require(condition, reason):
    if not condition:
        raise ValueError(reason)



def integer(value, label="counter"):
    # JSON integers and CSV decimal integers only; never truncate a counter.
    require(type(value) is int or isinstance(value, str) and re.fullmatch(r"-?[0-9]+", value),
            "invalid integer " + label)
    return int(value)


def measurement(value, label="measurement"):
    require(type(value) in (str, int, float), "invalid numeric " + label)
    result = float(value)  # Python numeric parsing is independent of the OS locale.
    require(math.isfinite(result), "nonfinite " + label)
    return result


def coordinate_word(value):
    # PathSample coordinates are float32 world units, NOT integer counters.
    # The existing engine digest passes them to hashWord(uint64_t): truncation
    # toward zero is part of that digest contract, not of coordinate parsing.
    parsed = measurement(value, "coordinate")
    require(0 <= parsed < 2**64, "coordinate outside defined uint64 fingerprint range")
    coordinate = struct.unpack("f", struct.pack("f", parsed))[0]
    require(math.isfinite(coordinate) and 0 <= coordinate < 2**64,
            "coordinate outside defined uint64 fingerprint range")
    return math.trunc(coordinate)


def numeric_rows(records):
    text = {"kind", "request_context", "outcome", "category", "time_kind", "ranking"}
    for row in records:
        for key, value in row.items():
            if key in text:
                continue
            if key.endswith("_ms") or key in ("from_x", "from_y", "to_x", "to_y"):
                measurement(value, key)
            else:
                number = integer(value, key)
                if key not in {"queue_before", "queue_after", "layer", "radius", "human", "crusher", "closest_allowed"}:
                    require(number >= 0, "negative counter " + key)

def percentile(values, percent):
    return sorted(values)[max(0, math.ceil(len(values) * percent / 100) - 1)]


def semantic_hash(paths, start):
    kinds = "request internal ground hierarchical closest attack safe patch move_away queue dispatch reconstruction cleanup zones zone_flags obstacle".split()
    outcomes = "observed returned_path returned_closest null_before_work null_after_work".split()
    work = "open_head_pops info_attempts info_new info_failed open_inserts forward_hops reverse_hops cleaned_cells block_zone_queries hierarchy_fallbacks".split()
    value = 14695981039346656037
    for p in paths:
        words = [int(p["id"]), int(p["parent_id"]), kinds.index(p["kind"]),
                 kinds.index(p["request_context"]) if p["request_context"] != "none" else len(kinds),
                 int(p["logic_before"]) - start, int(p["object_id"]), outcomes.index(p["outcome"])]
        words += [coordinate_word(p[key]) for key in ("from_x", "from_y", "to_x", "to_y")]
        words += [int(p[key + "_inclusive_count"]) for key in work]
        for word in words:
            for shift in range(0, 64, 8):
                value = ((value ^ ((word >> shift) & 255)) * 1099511628211) & ((1 << 64) - 1)
    return str(value)



def workload_qualification(paths):
    """Workload-class check, separate from correctness and timing comparisons."""
    internal = [p for p in paths if p["kind"] == "internal"]
    closest = [p for p in paths if p["kind"] == "closest"]
    # Accepted severe operations: final phase capture min 27,722 pops / 643,559
    # info attempts; B2/B3 minima ~19,950/26,295 and 528,774/579,871.
    heavy = [p for p in internal if integer(p["open_head_pops_inclusive_count"]) >= 20000
             and integer(p["info_attempts_inclusive_count"]) >= 500000]
    sampled = [p for p in heavy if integer(p["line_sample_calls"]) > 0 and integer(p["neighbor_sample_calls"]) > 0]
    own = sum(measurement(p["exclusive_ms"]) for p in internal)
    fallback = sum(measurement(p["exclusive_ms"]) for p in closest)
    share = own / (own + fallback) if own + fallback else 0
    internal_pops = sum(integer(p["open_head_pops_inclusive_count"]) for p in internal)
    closest_pops = sum(integer(p["open_head_pops_inclusive_count"]) for p in closest)
    work_share = internal_pops / (internal_pops + closest_pops) if internal_pops + closest_pops else 0
    reasons = []
    if len(internal) < 96: reasons.append("fewer than 96 Internal searches (one army)")
    if len(heavy) < 3: reasons.append("fewer than 3 large Internal searches (20000 pops and 500000 info attempts)")
    if len(sampled) < 3: reasons.append("fewer than 3 large Internal searches with both sampled checking phases")
    # Manual Internal head-pop shares: final 82.33%, B2 86.18%, B3 82.87%;
    # old automation 10.87%. Use work, not wall time, for the actual gate.
    if work_share < .75: reasons.append("Internal head-pop share below 75% of Internal plus Closest work")
    return {"representative": not reasons, "reasons": reasons,
            "internal_searches": len(internal), "large_internal_searches": len(heavy),
            "large_internal_with_checking_samples": len(sampled),
            "internal_self_ms": own, "closest_self_ms": fallback, "internal_self_share": share,
            "internal_head_pops": internal_pops, "closest_head_pops": closest_pops, "internal_head_pop_share": work_share,
            "timing_note": "self-time share is diagnostic only; no duration or timing fingerprint qualification threshold"}

def load(folder):
    folder = Path(folder)
    marker = folder / "benchmark-failure.json"
    if marker.exists():
        failure = json.loads(read_text(marker))
        require(isinstance(failure, dict), "invalid benchmark failure marker")
        # JSON encoding escapes control characters; never print marker text as terminal control codes.
        details = {key: failure[key] for key in ("reason", "failure_check", "failure_state", "logic_tick", "orders_issued", "failed_unit_index") if key in failure}
        raise ValueError("benchmark failure marker present: " + json.dumps(details, ensure_ascii=True))
    files = list(folder.glob("*-benchmark.json"))
    require(len(files) == 1, "each trial directory must contain exactly one benchmark result")
    data = json.loads(read_text(files[0]))
    integer_fields = "schema version dirty map_crc seed warmup_ticks capture_ticks order_period spawned_units objects_end actual_seed start_tick end_tick start_crc end_crc rng_start rng_end internal_searches internal_over40ms queue_max records dropped errors outer_frames completed_logic_ticks requested_fps effective_fps interpolation path_detail path_phase".split()
    for key in integer_fields:
        require(key in data and type(data[key]) is int and data[key] >= 0, "invalid integer metadata " + key)
    require(type(data.get("overlay")) is bool, "invalid boolean overlay")
    require(type(data.get("elapsed_seconds")) in (int, float) and measurement(data["elapsed_seconds"], "elapsed_seconds") > 0, "invalid elapsed seconds")
    require(isinstance(data.get("path_fingerprint"), str) and re.fullmatch(r"[0-9]+", data["path_fingerprint"]), "invalid fingerprint encoding")
    require(all(k in data for k in MATCH_FIELDS + CORRECTNESS), "missing metadata")
    require(data["schema"] == 1 and data["scenario"] == "pathfinding-heavy" and data["version"] in (1, 2), "unsupported schema/scenario")
    require(data["status"] == "complete" and not data["reason"], "benchmark did not complete")
    require(data["errors"] == data["dropped"] == 0 and data["spawned_units"] == 96, "errors/drops/incomplete army")
    require(data["capture_ticks"] == data["end_tick"] - data["start_tick"] == 300, "wrong tick duration")
    stem = data["report_stem"]
    require(Path(stem).name == stem and "/" not in stem and "\\" not in stem, "unsafe report stem")
    summary = dict(line.split("=", 1) for line in read_text(folder / (stem + "-summary.txt")).splitlines() if "=" in line)
    for key, expected in (("mode", "scenario_ticks"), ("path_detail_enabled", "1"), ("path_phase_enabled", "1"),
                          ("path_phase_stride", "64"), ("path_phase_errors", "0"), ("stack_or_clock_errors", "0"), ("path_detail_dropped", "0")):
        require(summary.get(key) == expected, "invalid summary " + key)
    frames = rows(folder / (stem + "-frames.csv"), 20000)
    paths = rows(folder / (stem + "-paths.csv"), 32768)
    categories = rows(folder / (stem + "-categories.csv"), 1000)
    slow = rows(folder / (stem + "-slow.csv"), 1000)
    require(frames and paths and categories and slow, "empty report")
    for report in (frames, paths, categories, slow):
        numeric_rows(report)
    require(len(frames) == data["outer_frames"] == int(summary["outer_frames"]), "frame count mismatch")
    require(len(paths) == data["records"] == int(summary["path_detail_records"]), "path count mismatch")
    require(sum(int(f["completed_logic_ticks"]) for f in frames) == data["capture_ticks"], "tick count mismatch")
    require(int(summary["completed_logic_ticks"]) == data["completed_logic_ticks"] == data["capture_ticks"], "summary tick count mismatch")
    require(summary["git"] == data["source"] and int(summary["dirty"]) == data["dirty"], "source identity mismatch")
    caps = {(int(f["requested_fps"]), int(f["effective_fps"])) for f in frames}
    require(len(caps) == 1, "FPS cap changed during trial")
    require(int(summary["ground_interpolation"]) == data["interpolation"], "interpolation mismatch")
    data["caps"] = next(iter(caps))
    require(data["caps"] == (data["requested_fps"], data["effective_fps"]), "metadata cap mismatch")
    by_id = {}
    for p in paths:
        number, parent = int(p["id"]), int(p["parent_id"])
        require(number not in by_id and number == len(by_id) + 1, "invalid path ID sequence")
        require(0 <= int(p["outer_index"]) < len(frames), "invalid path frame")
        if parent:
            require(parent in by_id and p["outer_index"] == by_id[parent]["outer_index"] and p["logic_before"] == by_id[parent]["logic_before"], "invalid parent")
        require(int(p["phase_errors"]) == 0, "phase errors")
        duration, self_time = float(p["inclusive_ms"]), float(p["exclusive_ms"])
        require(math.isfinite(duration) and math.isfinite(self_time) and 0 <= self_time <= duration, "invalid search durations")
        require(int(p["phase_selected"]) == int(p["phase_iterations"]) // 64, "phase selection mismatch")
        for phase in ("line", "neighbor"):
            require(float(p[phase + "_sample_inclusive_ms"]) >= float(p[phase + "_sample_insertion_ms"]) >= 0, "invalid phase self")
        by_id[number] = p
    require(semantic_hash(paths, data["start_tick"]) == data["path_fingerprint"], "path fingerprint mismatch")
    internal = [p for p in paths if p["kind"] == "internal"]
    require(len(internal) == data["internal_searches"] > 0, "missing Internal workload")
    logic = [float(f["game_logic_update_inclusive_ms"]) for f in frames if int(f["completed_logic_ticks"])]
    require(all(math.isfinite(v) and v >= 0 for v in logic), "invalid durations")
    data["metrics"] = {"logic_mean_ms": statistics.mean(logic), "logic_p95_ms": percentile(logic, 95),
                       "logic_p99_ms": percentile(logic, 99), "logic_max_ms": max(logic),
                       "internal_self_p95_ms": percentile([float(p["exclusive_ms"]) for p in internal], 95)}
    data["coverage"] = {"internal": len(internal), "internal_over40ms": sum(float(p["inclusive_ms"]) > 40 for p in internal),
                        "internal_pops_max": max(int(p["open_head_pops_inclusive_count"]) for p in internal),
                        "internal_failed": sum(p["outcome"] == "null_after_work" for p in internal)}
    data["phase_sample_ms"] = {key: sum(float(p[key]) for p in internal) for key in
                               ("line_sample_inclusive_ms", "line_sample_insertion_ms", "neighbor_sample_inclusive_ms", "neighbor_sample_insertion_ms")}
    data["representativeness"] = workload_qualification(paths)
    return data


def compare(reference, candidate, noise=5):
    require(reference and candidate, "both reference and candidate trials required")
    all_runs = reference + candidate
    for field in MATCH_FIELDS + CORRECTNESS + ("caps",):
        if any(r[field] != all_runs[0][field] for r in all_runs):
            return {"verdict": "not_comparable", "reason": "configuration/correctness differs: " + field}
    if any(not r.get("representativeness", {}).get("representative", False) for r in reference):
        return {"verdict": "not_comparable", "reason": "reference failed separate Internal workload representativeness gate"}
    metrics = {}
    for key in reference[0]["metrics"]:
        before = statistics.median(r["metrics"][key] for r in reference)
        after = statistics.median(r["metrics"][key] for r in candidate)
        delta = (after / before - 1) * 100 if before else (0 if not after else None)
        metrics[key] = {"reference_median": before, "candidate_median": after, "change_percent": delta}
    values = [v["change_percent"] for v in metrics.values()]
    if any(v is None or v > noise for v in values):
        verdict = "regression"
    elif metrics["logic_p95_ms"]["change_percent"] < -noise:
        verdict = "improvement"
    else:
        verdict = "unchanged_within_noise"
    return {"verdict": verdict, "metrics": metrics, "reference_trials": len(reference), "candidate_trials": len(candidate),
            "threshold_percent": noise, "statistical_significance": "not_established",
            "warning": "descriptive threshold only; runtime workload coverage requires developer review"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", action="append")
    parser.add_argument("--candidate", action="append")
    parser.add_argument("--validate", help="validate one trial without requiring it to qualify as a heavy reference")
    parser.add_argument("--qualify", help="validate correctness, then require the Internal workload class")
    parser.add_argument("--noise-percent", type=float, default=5)
    args = parser.parse_args()
    try:
        require(math.isfinite(args.noise_percent) and 0 < args.noise_percent <= 50, "invalid noise threshold")
        if args.validate or args.qualify:
            require(not (args.validate and args.qualify) and not args.reference and not args.candidate, "validation, qualification and comparison are separate modes")
            data=load(args.validate or args.qualify)
            result={"verdict":"valid", "metrics":data["metrics"], "coverage":data["coverage"],
                    "reference_coverage_qualified":data["representativeness"]["representative"],
                    "representativeness":data["representativeness"],
                    "warning":"valid capture does not establish representative workload or global determinism"}
            if args.qualify and not data["representativeness"]["representative"]:
                result["verdict"] = "not_representative"
        else:
            require(args.reference and args.candidate, "provide --validate or both --reference and --candidate")
            result = compare([load(p) for p in args.reference], [load(p) for p in args.candidate], args.noise_percent)
    except (ValueError, KeyError, OSError, TypeError, csv.Error) as error:
        result = {"verdict": "invalid", "reason": str(error)}
    print(json.dumps(result, indent=2, allow_nan=False))
    return {"valid":0, "improvement": 0, "unchanged_within_noise": 0, "regression": 1, "not_comparable": 2, "not_representative": 2, "invalid": 3}[result["verdict"]]


if __name__ == "__main__":
    raise SystemExit(main())
