#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""X64.49: robust per-render-rate statistics of camera-probe.csv runs written by Invoke-CameraProbe.ps1.

Usage: analyze_camera_probe.py [build/diag] [Label ...]     (default labels: before after; titles Generals ZeroHour)
Per title / input path (key = arrow-key state, edge = screen-edge state) and render cap it prints the frame-interval distribution (between handed-over
frames), the camera step per drawn frame (its coefficient of variation) and the world speed the camera has between drawn frames (camera step / time between the
frames' draw starts: mean and coefficient of variation). Frames in which the camera does not move (map boundary) and the first / last six frames are excluded.
"""
import csv
import math
import os
import statistics as st
import sys


def load(path, phase):
    rows = [r for r in csv.DictReader(open(path)) if r["phase"] == str(phase)]
    draws = [(float(r["ms"]), float(r["d"]), float(r["e"])) for r in rows if r["kind"] == "D"]
    presents = [float(r["ms"]) for r in rows if r["kind"] == "P"]
    return draws, presents


def cv(v):
    return st.pstdev(v) / st.mean(v) if v and st.mean(v) > 0 else 0.0


def pct(v, p):
    s = sorted(v)
    return s[int(p * (len(s) - 1))]


def analyse(path, phase):
    draws, presents = load(path, phase)
    n = min(len(draws), len(presents))
    draws, presents = draws[6:n - 6], presents[6:n - 6]
    steps = [math.hypot(draws[i][1] - draws[i - 1][1], draws[i][2] - draws[i - 1][2]) for i in range(1, len(draws))]
    med = st.median(steps)
    keep = [i for i in range(1, len(draws)) if steps[i - 1] > 0.3 * med]
    frame = [presents[i] - presents[i - 1] for i in keep]
    step = [steps[i - 1] for i in keep]
    speed = [steps[i - 1] / (draws[i][0] - draws[i - 1][0]) * 1000.0 for i in keep]
    return {"n": len(keep), "frame_mean": st.mean(frame), "frame_p95": pct(frame, .95), "frame_p99": pct(frame, .99), "frame_max": max(frame),
            "frame_std": st.pstdev(frame), "step_mean": st.mean(step), "step_cv": cv(step), "speed_mean": st.mean(speed), "speed_cv": cv(speed)}


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "build/diag"
    labels = sys.argv[2:] or ["before", "after"]
    for title in ("Generals", "ZeroHour"):
        for phase, name in ((2, "key"), (4, "edge")):
            print("== %s %s" % (title, name))
            print("cap mode      n  frame_ms mean/p95/p99/max/std  | step cv | camera speed units/s (draw interval) mean  cv")
            for cap in (30, 60, 120, 144, 240):
                for mode in labels:
                    path = os.path.join(root, "camera-probe-%s-%s" % (title, mode), "fps%d" % cap, "camera-probe.csv")
                    if not os.path.exists(path):
                        continue
                    a = analyse(path, phase)
                    print("%3d %-6s %4d  %5.2f/%5.2f/%5.2f/%5.1f/%4.2f | %.3f | %7.1f %.3f" % (cap, mode, a["n"], a["frame_mean"], a["frame_p95"], a["frame_p99"],
                          a["frame_max"], a["frame_std"], a["step_cv"], a["speed_mean"], a["speed_cv"]))


if __name__ == "__main__":
    main()
