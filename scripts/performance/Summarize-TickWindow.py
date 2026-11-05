#!/usr/bin/env python3
"""X64.sc: distribution of the simulation tick time (and the mean share of the main phases) over a window of frames of a headless bench run.

  python Summarize-TickWindow.py --from-frame 15000 label=<dir containing *_ticks.csv and *_fingerprint.txt> [label=dir ...]

The window matters for the unit-multiplier rungs (GENERALS_SIM_CLONE=k@frame): only the ticks after the multiplication are comparable between rungs.
"""
import argparse
import csv
import glob
import os
import re


def percentile(sorted_values, q):
    if not sorted_values:
        return 0.0
    k = min(len(sorted_values) - 1, max(0, int(round(q * (len(sorted_values) - 1)))))
    return sorted_values[k]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--from-frame", type=int, default=0)
    p.add_argument("--phases", default="path_search,ai_object_base_update,object_update_loops,path_request_entries,logic_partition,logic_scripts,ai_player_strategy,weapon_fire,locomotor_move")
    p.add_argument("runs", nargs="+", help="label=directory")
    args = p.parse_args()
    phases = [x for x in args.phases.split(",") if x]
    print("%-10s %7s %7s %7s %7s %7s %7s %6s  objects(at end)  | mean ms/tick per phase: %s" % ("run", "ticks", "mean", "p50", "p95", "p99", "worst", ">33", " ".join(phases)))
    for spec in args.runs:
        label, directory = spec.split("=", 1)
        ticks = glob.glob(os.path.join(directory, "*_ticks.csv"))
        fps = glob.glob(os.path.join(directory, "*_fingerprint.txt"))
        if not ticks:
            print("%-10s no ticks.csv in %s" % (label, directory))
            continue
        values = []
        sums = {ph: 0.0 for ph in phases}
        with open(ticks[0], newline="") as f:
            for row in csv.DictReader(f):
                if int(row["frame"]) < args.from_frame:
                    continue
                values.append(float(row["ms"]))
                for ph in phases:
                    sums[ph] += float(row.get(ph, 0) or 0)
        objects = 0
        if fps:
            for line in open(fps[0]):
                m = re.search(r"objects=(\d+)", line)
                if m:
                    objects = int(m.group(1))
        values_sorted = sorted(values)
        n = len(values) or 1
        print("%-10s %7d %7.3f %7.3f %7.3f %7.3f %7.2f %6d  %6d          | %s" % (
            label, len(values), sum(values) / n, percentile(values_sorted, .5), percentile(values_sorted, .95), percentile(values_sorted, .99), values_sorted[-1] if values_sorted else 0,
            sum(1 for v in values if v > 33.333), objects, " ".join("%.3f" % (sums[ph] / n) for ph in phases)))


if __name__ == "__main__":
    main()
