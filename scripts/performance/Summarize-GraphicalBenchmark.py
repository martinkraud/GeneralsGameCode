#!/usr/bin/env python3
"""X64.gb: compact summary of ONE seeded graphical benchmark run (a directory written by Invoke-FrameDeliveryRun.ps1 -Replay ... -Endurance).

  python Summarize-GraphicalBenchmark.py <run dir> [--from-frame N] [--headless-fingerprint file] [--out summary.json]

Reports, for the logic frames >= --from-frame: simulation tick mean/p50/p95/p99/p99.9/worst and ticks over 33.333 ms (from the per-tick file), the render interval distribution
(per-window medians and the worst window), hitch attribution by dominant phase, world scale (objects, units, moving units, path requests) from the endurance intervals, memory,
and the authoritative fingerprint (FNV-1a digest in the format of the headless bench) with an optional line-by-line comparison against a headless fingerprint file.
The JSON it writes is the permanent, compact evidence of the run; the raw per-tick / per-frame files may then be deleted.
"""
import argparse
import csv
import glob
import json
import os
import re
import statistics as st


def pct(sorted_values, q):
    if not sorted_values:
        return 0.0
    return sorted_values[min(len(sorted_values) - 1, max(0, int(round(q * (len(sorted_values) - 1)))))]


def fnv_digest(lines):
    """DeterminismFingerprint::digest: FNV-1a over frame, crc, objects as little-endian 32-bit words."""
    h = 14695981039346656037
    for ln in lines:
        m = re.match(r"frame=(\d+) crc=([0-9A-Fa-f]+) objects=(\d+)", ln)
        if not m:
            continue
        for v in (int(m.group(1)), int(m.group(2), 16), int(m.group(3))):
            for i in range(4):
                h ^= (v >> (8 * i)) & 0xFF
                h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return "%016X" % h


def read_lines(path):
    with open(path) as f:
        return [ln.strip() for ln in f if ln.strip()]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("--from-frame", type=int, default=0)
    ap.add_argument("--headless-fingerprint", default="")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    d = a.run
    res = {"run": os.path.basename(os.path.abspath(d))}

    ticks = glob.glob(os.path.join(d, "endurance-*-ticks.csv"))
    if ticks:
        vals, allvals = [], []
        for r in csv.DictReader(open(ticks[0], newline="")):
            allvals.append(float(r["tick_ms"]))
            if int(r["frame"]) >= a.from_frame:
                vals.append(float(r["tick_ms"]))
        va = sorted(allvals)
        res["tick_all"] = {"ticks": len(va), "mean": round(st.mean(va), 3) if va else 0, "p50": round(pct(va, .5), 3), "p95": round(pct(va, .95), 3), "p99": round(pct(va, .99), 3),
                           "p99_9": round(pct(va, .999), 3), "worst": round(va[-1], 3) if va else 0, "over_33_3": sum(1 for x in va if x > 33.333)}
        v = sorted(vals)
        res["tick"] = {"ticks": len(v), "mean": round(st.mean(v), 3) if v else 0, "p50": round(pct(v, .5), 3), "p95": round(pct(v, .95), 3), "p99": round(pct(v, .99), 3),
                       "p99_9": round(pct(v, .999), 3), "worst": round(v[-1], 3) if v else 0, "over_33_3": sum(1 for x in v if x > 33.333), "over_50": sum(1 for x in v if x > 50), "over_100": sum(1 for x in v if x > 100)}
    iv = glob.glob(os.path.join(d, "endurance-*-intervals.csv"))
    if iv:
        rows = [r for r in csv.DictReader(open(iv[0], newline="")) if int(r["frame"]) >= a.from_frame]
        if rows:
            def col(name):
                return [float(r[name]) for r in rows if r.get(name) not in (None, "")]
            res["world"] = {"objects_max": int(max(col("objects_total"))), "units_max": int(max(col("units_total"))), "moving_units_max": int(max(col("moving_units"))) if "moving_units" in rows[0] else None,
                            "objects_mean": round(st.mean(col("objects_total")), 1), "units_mean": round(st.mean(col("units_total")), 1)}
            total_ticks = sum(float(r["interval_ticks"]) for r in rows) or 1.0
            for key in ("path_searches", "path_ok", "path_fail", "path_fail_ge5ms", "queued_paths", "update_modules", "queue_cells", "objects_visited"):
                if key in rows[0]:
                    res["world"][key + "_total"] = int(sum(col(key)))
            if "path_fail_ms" in rows[0]:
                res["world"]["path_fail_ms_total"] = round(sum(col("path_fail_ms")), 1)
            res["phase_ms_per_tick"] = {c[3:]: round(sum(col(c)) / total_ticks, 4) for c in rows[0].keys() if c.startswith("ms_") and c != "ms_game_logic_update"}
    win = glob.glob(os.path.join(d, "frame-delivery-*-windows.csv"))
    if win:
        w = list(csv.DictReader(open(win[0], newline="")))
        f = lambda k: [float(r[k]) for r in w if r.get(k) not in (None, "")]
        res["render"] = {"windows": len(w), "frames": sum(int(r["frames"]) for r in w), "fps_mean": round(st.mean(f("render_fps")), 2), "fps_min_window": round(min(f("render_fps")), 2),
                         "interval_p50_median": round(st.median(f("int_p50_ms")), 2), "interval_p95_median": round(st.median(f("int_p95_ms")), 2), "interval_p99_median": round(st.median(f("int_p99_ms")), 2),
                         "interval_p99_worst_window": round(max(f("int_p99_ms")), 2), "interval_max": round(max(f("int_max_ms")), 1), "hitches": sum(int(r["hitches"]) for r in w)}
        res["memory"] = {"working_set_mb_start": round(float(w[0]["ws_mb"]), 0), "working_set_mb_end": round(float(w[-1]["ws_mb"]), 0), "working_set_mb_max": round(max(f("ws_mb")), 0),
                         "private_mb_end": round(float(w[-1]["private_mb"]), 0), "handles_start": int(float(w[0]["handles"])), "handles_end": int(float(w[-1]["handles"]))}
    hit = glob.glob(os.path.join(d, "frame-delivery-*-hitches.csv"))
    if hit:
        cat = {}
        for r in csv.DictReader(open(hit[0], newline="")):
            comps = {k: float(r[k]) for k in ("predraw_ms", "draw_ms", "postdraw_ms", "wait_ms", "tick_ms")}
            k = max(comps, key=comps.get)
            c = cat.setdefault(k, {"n": 0, "max_interval_ms": 0.0})
            c["n"] += 1
            c["max_interval_ms"] = max(c["max_interval_ms"], float(r["interval_ms"]))
        res["hitches_by_dominant_phase"] = cat
        # the worst render intervals with their phase split and resource counters, kept in the compact summary so an extreme outlier can still be attributed after the raw rows are deleted
        allrows = list(csv.DictReader(open(hit[0], newline="")))
        allrows.sort(key=lambda r: -float(r["interval_ms"]))
        res["top_render_hitches"] = [
            {"t_s": round(float(r["t_ms"]) / 1000.0, 1), "interval_ms": round(float(r["interval_ms"]), 1), "predraw_ms": round(float(r["predraw_ms"]), 1), "draw_ms": round(float(r["draw_ms"]), 1),
             "postdraw_ms": round(float(r["postdraw_ms"]), 1), "present_ms": round(float(r["present_ms"]), 1), "tick_ms": round(float(r["tick_ms"]), 1), "ticks": int(r["ticks"]),
             "tex_created": int(r["tex_created"]), "buf_created": int(r["buf_created"]), "draws": int(r["draws"]), "page_faults": int(r["page_faults"]), "working_set_mb": float(r["working_set_mb"])}
            for r in allrows[:8]]
    # the headless run of the same replay (headless_ticks.csv), restricted to the frames the graphical run reached: the headless-versus-graphical comparison
    hticks = os.path.join(d, "headless_ticks.csv")
    if os.path.exists(hticks) and "tick_all" in res:
        last = None
        if ticks:
            for r in csv.DictReader(open(ticks[0], newline="")):
                last = int(r["frame"])
        hv, hsum, hphase = [], 0, {}
        rows_h = list(csv.DictReader(open(hticks, newline="")))
        keep = [r for r in rows_h if int(r["frame"]) <= (last or 10 ** 9) and int(r["frame"]) >= a.from_frame]
        vv = sorted(float(r["ms"]) for r in keep)
        if vv:
            res["headless_same_frames"] = {"ticks": len(vv), "mean": round(st.mean(vv), 3), "p50": round(pct(vv, .5), 3), "p95": round(pct(vv, .95), 3), "p99": round(pct(vv, .99), 3), "worst": round(vv[-1], 3),
                                           "over_33_3": sum(1 for x in vv if x > 33.333)}
            skip = {"tick", "frame", "ms", "game_logic_update", "outer_frame_including_pacer"}
            res["headless_phase_ms_per_tick"] = {c: round(sum(float(r[c]) for r in keep) / len(keep), 4) for c in rows_h[0].keys() if c not in skip}
    fp = os.path.join(d, "fingerprint.txt")
    if os.path.exists(fp):
        lines = read_lines(fp)
        res["fingerprint"] = {"checkpoints": len(lines), "last_frame": int(re.match(r"frame=(\d+)", lines[-1]).group(1)) if lines else 0, "digest_of_all": fnv_digest(lines)}
        if a.headless_fingerprint:
            hl = read_lines(a.headless_fingerprint)
            n = min(len(lines), len(hl))
            diverge = next((i for i in range(n) if lines[i] != hl[i]), -1)
            res["fingerprint"]["vs_headless"] = {"compared_checkpoints": n, "first_divergence_index": diverge, "digest_of_common_prefix": fnv_digest(lines[:n]), "headless_digest_of_common_prefix": fnv_digest(hl[:n])}
    print(json.dumps(res, indent=2))
    if a.out:
        with open(a.out, "w") as o:
            json.dump(res, o, indent=2)


if __name__ == "__main__":
    main()
