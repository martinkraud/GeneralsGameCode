#!/usr/bin/env python3
"""Summarise frame-delivery telemetry (GENERALS_FRAMEDELIVERY=<dir>) of one or more runs.

    python scripts/performance/Summarize-FrameDelivery.py <dir-or-windows.csv> [<dir> ...] [--skip 2]

Per run: mode (off = offline, net = lock-step network), windows, render interval percentiles (the worst window, not an average), tick rate, tick spacing,
presentation validity (share of render iterations that interpolate between two authoritative states), history resets, stalls, and - for network runs - the
negotiated logic frame rate / run-ahead, self-slug and schedule-reset counters. Per-frame files (GENERALS_FRAMEDELIVERY_FRAMES=1) add exact figures over the whole run.
The first windows (map load) are skipped with --skip (default 2).
"""
import argparse
import csv
import glob
import os
import sys


def find(path, suffix):
    if os.path.isdir(path):
        hits = sorted(glob.glob(os.path.join(path, "*" + suffix)))
        return hits[0] if hits else None
    return path if path.endswith(suffix) else None


def pct(values, q):
    if not values:
        return 0.0
    values = sorted(values)
    rank = max(1, min(len(values), int(-(-q * len(values) // 1))))
    return values[rank - 1]


def summarize(path, skip):
    windows_file = find(path, "-windows.csv")
    if not windows_file:
        print("%s: no *-windows.csv" % path)
        return
    rows = list(csv.DictReader(open(windows_file)))[skip:]
    if not rows:
        print("%s: no windows after skipping %d" % (path, skip))
        return
    f = lambda k: [float(r[k]) for r in rows]
    frames = sum(int(r["frames"]) for r in rows)
    ticks_frames = sum(int(r["frames_1tick"]) + int(r["frames_2plus"]) for r in rows)
    valid = sum(float(r["alpha_valid_pct"]) * int(r["frames"]) for r in rows) / max(1, frames)
    print("== %s" % os.path.basename(os.path.normpath(path)))
    print("   mode %s | windows %d | frames %d | ticks %d" % (rows[0]["mode"], len(rows), frames, ticks_frames))
    print("   render fps (mean of windows) %.1f | interval ms: p50 %.2f  worst-window p95 %.2f  p99 %.2f  max %.2f | hitches (>1.5x median) %d" % (
        sum(f("render_fps")) / len(rows), pct(f("int_p50_ms"), 0.5), max(f("int_p95_ms")), max(f("int_p99_ms")), max(f("int_max_ms")), sum(int(r["hitches"]) for r in rows)))
    print("   tick rate (min/mean/max of windows) %.2f / %.2f / %.2f | spacing ms: p50 %.2f  worst p95 %.2f  max %.2f | tick duration ms: worst p99 %.2f" % (
        min(f("tick_rate")), sum(f("tick_rate")) / len(rows), max(f("tick_rate")), pct(f("spacing_p50_ms"), 0.5), max(f("spacing_p95_ms")), max(f("spacing_max_ms")), max(f("tickdur_p99_ms"))))
    print("   presentation: interpolating %.1f%% of iterations | frozen (alpha pinned/invalid) %d | history resets %d | alpha regressions %d | stalled iterations %d" % (
        valid, sum(int(r["frozen_frames"]) for r in rows), sum(int(r["epoch_changes"]) for r in rows), sum(int(r["alpha_regressions"]) for r in rows), sum(int(r["stalled_frames"]) for r in rows)))
    if rows[0]["mode"] == "net":
        print("   network: logic frame rate %s | run-ahead %s..%s | self-slugs %d | schedule resets %d | ticks released %d" % (
            sorted(set(r["net_framerate"] for r in rows)), min(int(r["net_runahead"]) for r in rows), max(int(r["net_runahead"]) for r in rows),
            sum(int(r["net_selfslugs"]) for r in rows), sum(int(r["net_resets"]) for r in rows), sum(int(r["net_released"]) for r in rows)))
    frames_file = find(path, "-frames.csv")
    if frames_file:
        fr = list(csv.DictReader(open(frames_file)))
        iv = [float(r["interval_ms"]) for r in fr]
        valid_n = sum(int(r["valid"]) for r in fr)
        slow = sum(1 for v in iv if v > 50.0)
        print("   per-frame: %d iterations | interval p50 %.2f p95 %.2f p99 %.2f p99.9 %.2f max %.2f | >50 ms: %d | interpolating %.1f%%" % (
            len(fr), pct(iv, 0.5), pct(iv, 0.95), pct(iv, 0.99), pct(iv, 0.999), max(iv), slow, 100.0 * valid_n / max(1, len(fr))))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--skip", type=int, default=2)
    args = ap.parse_args()
    for p in args.paths:
        summarize(p, args.skip)
    return 0


if __name__ == "__main__":
    sys.exit(main())
