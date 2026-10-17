#!/usr/bin/env python3
"""X64.44: analyse an EnduranceTelemetry capture (GENERALS_ENDURANCE) and print a timeline.

    python -I Analyze-EnduranceTelemetry.py <directory-or-file-prefix> [--window-minutes N] [--json out.json]

Inputs (written by the engine next to each other): <stem>-meta.txt, <stem>-intervals.csv, <stem>-ticks.csv, optional <stem>-pathfail.csv.
Everything printed is measured data or an explicitly labelled heuristic; the script never claims a unit limit. Pure standard library.
"""
import csv
import glob
import json
import math
import os
import sys

PERIOD_MS = 1000.0 / 30.0
INTERVAL_S = 10.0


def find_stem(arg):
    if os.path.isdir(arg):
        metas = sorted(glob.glob(os.path.join(arg, "endurance-*-meta.txt")))
        if not metas:
            raise SystemExit("no endurance-*-meta.txt in " + arg)
        return metas[-1][:-len("-meta.txt")]
    for suffix in ("-meta.txt", "-intervals.csv", "-ticks.csv"):
        if arg.endswith(suffix):
            return arg[:-len(suffix)]
    return arg


def read_meta(stem):
    meta = {}
    try:
        for line in open(stem + "-meta.txt", encoding="utf-8", errors="replace"):
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                meta[k] = v
    except FileNotFoundError:
        pass
    return meta


def read_csv(path):
    try:
        with open(path, newline="", encoding="utf-8", errors="replace") as f:
            return list(csv.DictReader(f))
    except FileNotFoundError:
        return []


def num(row, key, default=0.0):
    try:
        return float(row[key])
    except (KeyError, ValueError, TypeError):
        return default


def pct(sorted_values, p):
    if not sorted_values:
        return 0.0
    rank = max(1, min(len(sorted_values), int(math.ceil(p / 100.0 * len(sorted_values)))))
    return sorted_values[rank - 1]


def realtime_model(durations):
    """Back-to-back model of SimulationTickStats: ticks run as soon as they are due; returns (missed, catchup, longest_run, max_backlog_ms, per-tick backlog)."""
    finish = 0.0
    missed = catchup = run = longest = 0
    max_backlog = 0.0
    backlog_series = []
    for i, d in enumerate(durations):
        sched = i * PERIOD_MS
        start = max(sched, finish)
        backlog = start - sched
        backlog_series.append(backlog)
        if backlog > 1e-9:
            catchup += 1
            run += 1
            longest = max(longest, run)
        else:
            run = 0
        max_backlog = max(max_backlog, backlog)
        finish = start + d
        if finish > sched + PERIOD_MS:
            missed += 1
    return missed, catchup, longest, max_backlog, backlog_series


def analyse(stem, window_minutes=5):
    meta = read_meta(stem)
    intervals = read_csv(stem + "-intervals.csv")
    ticks = read_csv(stem + "-ticks.csv")
    samples = read_csv(stem + "-pathfail.csv")
    out = {"stem": stem, "meta": meta}
    lines = []

    def emit(s=""):
        lines.append(s)

    if not intervals:
        emit("no interval rows found")
        return "\n".join(lines), out

    durations = [float(r["tick_ms"]) for r in ticks]
    frames = [int(r["frame"]) for r in ticks]
    starts = [float(r["start_ms"]) for r in ticks]
    first_frame = frames[0] if frames else 0
    sorted_d = sorted(durations)
    game_minutes = (frames[-1] - first_frame) / 30.0 / 60.0 if frames else 0.0
    emit("ENDURANCE TELEMETRY ANALYSIS")
    emit("map %s | title %s | git %s | %s" % (meta.get("map", "?"), meta.get("title", "?"), meta.get("git", "?")[:10], meta.get("hardware", "?")))
    for k in sorted(meta):
        if k.startswith("player"):
            emit("  %s: %s" % (k, meta[k]))
    emit("ticks %d | game time %.1f min | wall %.1f min | interval rows %d" % (len(durations), game_minutes, num(intervals[-1], "wall_s") / 60.0, len(intervals)))
    missed, catchup, longest, max_backlog, backlog = realtime_model(durations)
    emit("")
    emit("WHOLE SESSION (exact, from the per-tick file; back-to-back model for deadlines)")
    emit("  tick ms: mean %.3f median %.3f p90 %.3f p95 %.3f p99 %.3f p99.9 %.3f worst %.1f" % (sum(durations) / max(1, len(durations)), pct(sorted_d, 50), pct(sorted_d, 90), pct(sorted_d, 95), pct(sorted_d, 99), pct(sorted_d, 99.9), sorted_d[-1] if sorted_d else 0))
    emit("  ticks > 16.67 / 25 / 33.333 / 50 / 66.67 / 100 ms: " + " / ".join(str(sum(1 for d in durations if d > t)) for t in (16.67, 25, PERIOD_MS, 50, 66.67, 100)))
    emit("  derived missed deadlines %d | catch-up ticks %d | longest catch-up run %d | max backlog %.1f ms" % (missed, catchup, longest, max_backlog))

    # real-time slip from the interval rows: wall time spent beyond the 10 s of game time per interval, ignoring intervals with pauses / frozen time
    # Queue model: a tick cannot run before its slot, so the lag never goes below zero (an uncapped headless run therefore shows 0, a paced run
    # accumulates only real overruns; jitter of either sign cancels).
    slip = 0.0
    slip_series = []
    for r in intervals:
        paused = num(r, "paused_frames") > 0 or num(r, "non_tick_updates") > 0
        if not paused and num(r, "interval_ticks") >= 290:
            slip = max(0.0, slip + num(r, "interval_wall_s") - INTERVAL_S * num(r, "interval_ticks") / 300.0)
        slip_series.append(slip)
    emit("  real-time lag at the end (queue model over un-paused intervals): %.2f s (> 0 means the game ran slower than real time and has not caught up)" % slip)

    # per-window timeline
    window_rows = max(1, int(window_minutes * 6))
    emit("")
    emit("TIMELINE (windows of %d min of game time; intervals averaged; per-tick stats from the tick file)" % window_minutes)
    emit("%7s %7s %7s %7s %6s %7s %7s | %7s %7s %6s | %8s %8s %8s | %7s %6s | %6s %5s" % (
        "min", "objects", "units", "structs", "statNI", "tps", "fps", "p99ms", "maxms", ">33ms", "aiObjMs", "pathMs", "scriptMs", "fail", "fail5", "zoneC", "slip"))
    series = []
    for w in range(0, len(intervals), window_rows):
        chunk = intervals[w:w + window_rows]
        if not chunk:
            continue
        f0, f1 = int(num(chunk[0], "frame")) - 300, int(num(chunk[-1], "frame"))
        wd = [d for d, f in zip(durations, frames) if f0 < f <= f1]
        wd_sorted = sorted(wd)
        n = len(chunk)
        tpsv = [num(r, "achieved_tps") for r in chunk if num(r, "paused_frames") == 0 and num(r, "non_tick_updates") == 0]
        ticks_n = max(1.0, sum(num(r, "interval_ticks") for r in chunk))
        mean = lambda key: sum(num(r, key) for r in chunk) / n
        per_tick = lambda key: sum(num(r, key) for r in chunk) / ticks_n
        row = {
            "minute": f1 / 30.0 / 60.0, "objects": mean("objects_total"), "units": mean("units_total"), "structures": mean("structures_total"), "stationary_nonidle": mean("stationary_nonidle_units"),
            "tps": (sum(tpsv) / len(tpsv)) if tpsv else 0.0, "p99": pct(wd_sorted, 99), "max": wd_sorted[-1] if wd_sorted else 0.0, "over33": sum(1 for d in wd if d > PERIOD_MS),
            "ai_object_ms_per_tick": per_tick("ms_ai_object_base_update"), "path_search_ms_per_tick": per_tick("ms_path_search"), "scripts_ms_per_tick": per_tick("ms_logic_scripts"),
            "object_updates_ms_per_tick": per_tick("ms_object_update_loops"), "zone_ms_per_tick": per_tick("ms_zone_calc"),
            "path_fail": sum(num(r, "path_fail") for r in chunk), "path_fail_ge5": sum(num(r, "path_fail_ge5ms") for r in chunk), "path_fail_ms": sum(num(r, "path_fail_ms") for r in chunk),
            "zone_calls": sum(num(r, "zone_calc_calls") for r in chunk), "slip_s": slip_series[min(len(slip_series) - 1, w + n - 1)], "tick_mean": (sum(wd) / len(wd)) if wd else 0.0,
            "fps": (sum(num(r, "engine_frames") for r in chunk) / max(1e-9, sum(num(r, "interval_wall_s") for r in chunk))),
            "searches": sum(num(r, "path_searches") for r in chunk), "queued_paths": sum(num(r, "queued_paths") for r in chunk), "update_modules_per_tick": per_tick("update_modules"),
        }
        series.append(row)
        emit("%7.1f %7.0f %7.0f %7.0f %6.0f %7.2f %7.1f | %7.2f %7.1f %6d | %8.3f %8.3f %8.3f | %7.0f %6.0f | %6.0f %5.1f" % (
            row["minute"], row["objects"], row["units"], row["structures"], row["stationary_nonidle"], row["tps"], row["fps"], row["p99"], row["max"], row["over33"],
            row["ai_object_ms_per_tick"], row["path_search_ms_per_tick"], row["scripts_ms_per_tick"], row["path_fail"], row["path_fail_ge5"], row["zone_calls"], row["slip_s"]))
    out["timeline"] = series

    # heuristics (labelled)
    emit("")
    emit("HEURISTIC MARKERS (definitions are fixed here, not tuned to the data)")

    def first(pred, rows=series):
        for r in rows:
            if pred(r):
                return r
        return None

    k = first(lambda r: r["tps"] and r["tps"] < 29.5)
    emit("  first window with achieved tick rate < 29.5 TPS: %s" % ("none" if not k else "%.1f min (%.2f TPS, %.0f objects)" % (k["minute"], k["tps"], k["objects"])))
    if k:
        slow = [r for r in series if r["tps"] and r["tps"] < 29.5]
        render_bound = [r for r in slow if 0 < r["fps"] < 30.5]
        emit("    of the %d windows below 29.5 TPS, %d also had a render frame rate below 30.5 FPS (the logic can advance at most one tick per render frame, so these are render/frame-rate limited, not simulation limited); mean logic tick in those windows: %s" % (
            len(slow), len(render_bound), ", ".join("%.2f ms" % r["tick_mean"] for r in slow[:6])))
    k = first(lambda r: r["slip_s"] >= 1.0)
    emit("  first window with accumulated real-time slip >= 1 s: %s" % ("none" if not k else "%.1f min (slip %.1f s, %.0f objects)" % (k["minute"], k["slip_s"], k["objects"])))
    k = first(lambda r: r["p99"] > 16.67)
    emit("  first window with p99 tick > 16.67 ms: %s" % ("none" if not k else "%.1f min (p99 %.1f ms, %.0f objects)" % (k["minute"], k["p99"], k["objects"])))
    k = first(lambda r: r["over33"] >= 5)
    emit("  first window with >= 5 ticks over 33.333 ms: %s" % ("none" if not k else "%.1f min (%d ticks, %.0f objects)" % (k["minute"], k["over33"], k["objects"])))
    persistent = None
    for i in range(len(backlog)):
        if i + 300 <= len(backlog) and all(b > 1e-9 for b in backlog[i:i + 300]):
            persistent = i
            break
    emit("  first persistent model backlog (300 consecutive catch-up ticks): %s" % ("none" if persistent is None else "tick %d (%.1f min game time)" % (persistent, persistent / 30.0 / 60.0)))

    # scaling of phases with the world size
    emit("")
    emit("PHASE SCALING (per-tick cost in each window, first window vs last window; share of the logic tick)")
    if len(series) >= 2:
        a, b = series[0], series[-1]
        for key, label in (("ai_object_ms_per_tick", "ai_object_base_update"), ("object_updates_ms_per_tick", "object_update_loops"), ("path_search_ms_per_tick", "path_search"), ("scripts_ms_per_tick", "logic_scripts"), ("zone_ms_per_tick", "zone_calc")):
            growth = (b[key] / a[key]) if a[key] > 1e-9 else float("inf")
            emit("  %-24s %8.3f -> %8.3f ms/tick (x%s)" % (label, a[key], b[key], "%.1f" % growth if growth != float("inf") else "inf"))
        emit("  objects %.0f -> %.0f | units %.0f -> %.0f | update modules/tick %.0f -> %.0f | tick mean %.3f -> %.3f ms" % (a["objects"], b["objects"], a["units"], b["units"], a["update_modules_per_tick"], b["update_modules_per_tick"], a["tick_mean"], b["tick_mean"]))
        xs = [r["objects"] for r in series]
        ys = [r["tick_mean"] for r in series]
        if len(xs) > 2 and max(xs) > min(xs):
            mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
            cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
            vx = sum((x - mx) ** 2 for x in xs)
            vy = sum((y - my) ** 2 for y in ys)
            emit("  correlation of window mean tick time with live objects: r = %.2f (%d windows)" % (cov / math.sqrt(vx * vy) if vx > 0 and vy > 0 else 0.0, len(xs)))

    # congestion / path failures
    emit("")
    emit("CONGESTION AND PATH FAILURES")
    fails = sum(r["path_fail"] for r in series)
    emit("  searches %.0f, exhausted the open list %.0f (%.1f%%), >= 5 ms: %.0f, failing time %.0f ms" % (sum(r["searches"] for r in series), fails, 100.0 * fails / max(1.0, sum(r["searches"] for r in series)), sum(r["path_fail_ge5"] for r in series), sum(r["path_fail_ms"] for r in series)))
    if len(series) >= 3:
        xs = [r["stationary_nonidle"] for r in series]
        ys = [r["path_fail_ms"] for r in series]
        mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
        cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
        vx = sum((x - mx) ** 2 for x in xs)
        vy = sum((y - my) ** 2 for y in ys)
        emit("  correlation of failing-search time with stationary non-idle units: r = %.2f" % (cov / math.sqrt(vx * vy) if vx > 0 and vy > 0 else 0.0))
    if samples:
        emit("  analysed expensive failures (rate-limited samples): %d" % len(samples))
        by_state = {}
        blocked = 0
        for r in samples:
            by_state[r.get("aistate", "?")] = by_state.get(r.get("aistate", "?"), 0) + 1
            if num(r, "blockerUnits") > 0:
                blocked += 1
        emit("    with blocking units on the frontier: %d | by requester AI state: %s" % (blocked, ", ".join("%s %d" % kv for kv in sorted(by_state.items(), key=lambda kv: -kv[1])[:5])))

    # slowest ticks with context
    emit("")
    emit("SLOWEST 15 TICKS (with the interval they fell into)")
    order = sorted(range(len(durations)), key=lambda i: -durations[i])[:15]
    by_frame_row = {int(num(r, "frame")): r for r in intervals}
    for i in order:
        f = frames[i]
        r = by_frame_row.get(((f // 300) + 1) * 300) or by_frame_row.get((f // 300) * 300)
        ctx = ""
        if r:
            ctx = "objects %.0f units %.0f stat-nonidle %.0f path_fail %.0f (>=5ms %.0f) zone_calls %.0f" % (num(r, "objects_total"), num(r, "units_total"), num(r, "stationary_nonidle_units"), num(r, "path_fail"), num(r, "path_fail_ge5ms"), num(r, "zone_calc_calls"))
        emit("  frame %7d (%.1f min) %8.2f ms | %s" % (f, f / 30.0 / 60.0, durations[i], ctx))

    # decision aid
    emit("")
    emit("DECISION AID (a reading guide, not a verdict)")
    if len(series) >= 2:
        b = series[-1]
        parts = {"ai_object_base_update": b["ai_object_ms_per_tick"], "object_update_loops (exclusive)": b["object_updates_ms_per_tick"], "path_search": b["path_search_ms_per_tick"], "logic_scripts": b["scripts_ms_per_tick"], "zone_calc": b["zone_ms_per_tick"]}
        total = sum(parts.values()) or 1.0
        emit("  last window, share of the named phases: " + ", ".join("%s %.0f%%" % (k, 100.0 * v / total) for k, v in sorted(parts.items(), key=lambda kv: -kv[1])))
        emit("  A: healthy at scale if slip stays ~0 and tps stays ~30; B: throughput-bound if ai_object_base_update dominates while tail stays small; C: congestion-bound if path_fail_ms and stationary non-idle units rise with the tail; D: another phase dominates")
    out["summary"] = {"ticks": len(durations), "missed": missed, "catchup": catchup, "max_backlog_ms": max_backlog, "slip_s": slip}
    return "\n".join(lines), out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    stem = find_stem(argv[1])
    window = 5
    json_path = None
    if "--window-minutes" in argv:
        window = max(1, int(argv[argv.index("--window-minutes") + 1]))
    if "--json" in argv:
        json_path = argv[argv.index("--json") + 1]
    text, data = analyse(stem, window)
    print(text)
    if json_path:
        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
