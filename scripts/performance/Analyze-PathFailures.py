#!/usr/bin/env python3
"""X64.aq: classify the failing A* searches of one or more headless runs (the *_pathfail.csv written with GENERALS_SIM_PATHFAIL=1) so runs can be compared.

  python Analyze-PathFailures.py label=<file> [label=<file> ...]

Per run: failures and their total time; by requesting AI state; repeats (same search signature as an earlier failure = the same request again on an unchanged map); same hunter + same goal again;
failures that ended by exhausting the info pool / per-frame budget (cells allocated at or near the pool size) versus exhausting the open list; failures with blocking units on the search frontier;
and the frontier composition (cliff / water / rubble / structure / ally / enemy / off-map cells).
"""
import collections
import csv
import sys


def f(r, k, d=0.0):
    try:
        return float(r[k])
    except (KeyError, ValueError, TypeError):
        return d


def main():
    runs = [a.split("=", 1) for a in sys.argv[1:]]
    if not runs:
        print(__doc__)
        return
    for label, path in runs:
        rows = list(csv.DictReader(open(path, newline="")))
        n = len(rows)
        ms = sum(f(r, "ms") for r in rows)
        print("=== %s: %d analysed failing searches, %.0f ms (mean %.1f ms)" % (label, n, ms, ms / n if n else 0))
        if not n:
            continue
        by = collections.defaultdict(lambda: [0, 0.0])
        for r in rows:
            b = by[r["aistate"]]
            b[0] += 1
            b[1] += f(r, "ms")
        print("  by requesting state: " + "; ".join("%s %d (%.0f ms, %.0f%%)" % (k, v[0], v[1], 100 * v[1] / ms) for k, v in sorted(by.items(), key=lambda x: -x[1][1])[:4]))
        rep = [r for r in rows if r.get("repeat") not in ("0", "", None)]
        print("  repeats of an earlier failing signature: %d (%.0f%%) costing %.0f ms (%.0f%%)" % (len(rep), 100 * len(rep) / n, sum(f(r, "ms") for r in rep), 100 * sum(f(r, "ms") for r in rep) / ms))
        pairs = collections.defaultdict(list)
        for r in rows:
            pairs[(r["object"], r["goalX"], r["goalY"])].append(r)
        again = sum(len(v) - 1 for v in pairs.values())
        print("  distinct (requester, goal) pairs %d; failures that repeat a pair: %d (%.0f%%), %.0f ms" % (len(pairs), again, 100 * again / n, sum(sum(f(x, "ms") for x in v[1:]) for v in pairs.values())))
        goals = collections.Counter((r["goalX"], r["goalY"]) for r in rows)
        top = goals.most_common(3)
        print("  distinct goal cells %d; most failed goals: %s" % (len(goals), ", ".join("%s,%s x%d" % (g[0], g[1], c) for g, c in top)))
        alloc = [f(r, "cellsAllocated") for r in rows]
        pops = [f(r, "pops") for r in rows]
        big = [a for a in alloc if a >= 20000]
        print("  cells allocated: mean %.0f max %.0f; failures with >= 20000 cells allocated (pool pressure): %d (%.0f%%); pops mean %.0f" % (sum(alloc) / n, max(alloc), len(big), 100 * len(big) / n, sum(pops) / n))
        blk = [r for r in rows if f(r, "blockerUnits") > 0]
        print("  with blocking units on the frontier: %d (%.0f%%), %.0f ms" % (len(blk), 100 * len(blk) / n, sum(f(r, "ms") for r in blk)))
        tot = {k: sum(f(r, k) for r in rows) for k in ("fCliff", "fWater", "fRubble", "fStruct", "fOther", "fOffMap", "fAllyNoAI", "fAllyBusy", "fEnemyFixed", "fPasses")}
        s = sum(tot.values()) or 1
        print("  frontier mix: " + " ".join("%s %.0f%%" % (k[1:], 100 * v / s) for k, v in tot.items() if v))
        gt = collections.Counter(r.get("goalType", "") for r in rows)
        print("  goal cell type: " + ", ".join("%s %d" % (k, v) for k, v in gt.most_common(4)))


if __name__ == "__main__":
    main()
