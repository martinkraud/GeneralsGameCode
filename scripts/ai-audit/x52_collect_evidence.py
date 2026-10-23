#!/usr/bin/env python3
"""X64.52: condense the (large, disposable) experiment output of Invoke-AiBaseExperiments.ps1 / Invoke-AiBehaviorGate.ps1 into the small committed evidence files.

  x52_collect_evidence.py <experiment-root> <gate-gen-dir> <gate-zh-dir> <perf-gen-dir> <perf-zh-dir> <out-dir>

experiment-root holds the directories written by build/x64-52-evidence.sh (layout, long, threat-g<angle>, threat-z<angle>, rebuild).
Writes: evidence-summary.json (everything below), gate-*.json, perf-*.json, and a plain-text report.
"""
import collections
import json
import math
import pathlib
import re
import shutil
import sys

CENSUS = re.compile(r"census frame=(\d+) player=(\d+) side=(\S+) template=(\S+) x=(-?\d+) y=(-?\d+) dx=(-?\d+) dy=(-?\d+) (\S+)")
DUMP = re.compile(r"event frame=\d+ buildlist player=(\d+) entry=(\S+) automatic=\d priority=\d rebuilds=\S+ dx=(-?\d+) dy=(-?\d+)")
DEFENSE_WORDS = ("Gattling", "Bunker", "Patriot", "Stinger", "FireBase", "Tower", "Cannon", "DemoTrap", "Defense")


def counters(path):
    out = {}
    for line in open(path):
        m = re.match(r"counter (\S+) = (-?\d+)", line)
        if m:
            out[m.group(1)] = int(m.group(2))
    return out


def census(path):
    rows = []
    for line in open(path):
        m = CENSUS.match(line)
        if m:
            f, p, side, t, x, y, dx, dy, st = m.groups()
            rows.append(dict(frame=int(f), player=int(p), template=t, dx=int(dx), dy=int(dy), state=st))
    return rows


def digest(path):
    for line in open(path):
        if "digest" in line:
            return line.strip().split("digest ")[-1]
    return None


def layout(root):
    result = {}
    for scenario, player in (("G-desert3H", 3), ("Z-desert3H-china", 3)):
        files = sorted(root.glob("layout/%s-s*-E-diag_ai.txt" % scenario))
        orig = sorted(root.glob("layout/%s-s*-O-diag_ai.txt" % scenario))
        if not files or not orig:
            continue

        def slots(path):
            rows = []
            for line in open(path):
                m = DUMP.match(line)
                if m and int(m.group(1)) == player:
                    rows.append((int(m.group(3)), int(m.group(4))))
            return rows
        base = slots(orig[0])
        entry = []
        for f in files:
            cur = slots(f)
            shifts = [math.hypot(a[0] - b[0], a[1] - b[1]) for a, b in zip(base, cur)]
            variant = re.search(r"variant=(\d) offset_deg=(-?\d+)", open(f).read())
            seed = re.search(r"-s(\d+)-E", f.name).group(1)
            entry.append(dict(seed=seed, variant=int(variant.group(1)), offset_deg=int(variant.group(2)), slots=len(cur),
                              mean_shift=round(sum(shifts) / len(shifts), 1), max_shift=round(max(shifts), 1),
                              digest=digest(str(f).replace("-diag_ai.txt", "_summary.txt"))))
        result[scenario] = entry
    return result


def threat(root):
    result = {}
    for title, prefix, scenario in (("Generals", "g", "G-desert3H-s51002"), ("ZeroHour", "z", "Z-desert3H-china-s51102")):
        rows = []
        for angle in (0, 90, 180, 270):
            row = dict(attack_deg=angle)
            for mode in "OE":
                path = root / ("threat-%s%d" % (prefix, angle)) / ("%s-%s-diag_census.txt" % (scenario, mode))
                if not path.exists():
                    continue
                found = [(r["template"], (math.degrees(math.atan2(r["dy"], r["dx"])) + 360) % 360)
                         for r in census(path) if r["player"] == 3 and r["state"] != "hole" and any(w in r["template"] for w in DEFENSE_WORDS)]
                near = sum(1 for _, a in found if min(abs(a - angle), 360 - abs(a - angle)) <= 60)
                row[mode] = dict(defences=len(found), within_60_deg=near)
            rows.append(row)
        result[title] = rows
    return result


def rebuild(root):
    result = {}
    for title, scenario in (("Generals", "G-desert3H-s51002"), ("ZeroHour", "Z-desert3H-china-s51102")):
        entry = {}
        for mode in "OE":
            path = root / "rebuild" / ("%s-%s-diag_ai.txt" % (scenario, mode))
            if not path.exists():
                continue
            c = counters(path)
            entry[mode] = {k: v for k, v in c.items() if k.startswith(("build.started.China", "build.started.Tank")) and any(w in k for w in ("CommandCenter", "Barracks", "SupplyCenter", "WarFactory", "Airfield"))}
            entry[mode]["kills_landed"] = c.get("events.scenario.kill", 0)
            entry[mode]["recovery_grants"] = c.get("x52.recovery.rebuild_granted", 0)
        result[title] = entry
    return result


def long_runs(root):
    result = collections.defaultdict(list)
    for path in sorted(root.glob("long/*-diag_ai.txt")):
        m = re.match(r"(.+)-s(\d+)-([OE])-diag_ai\.txt", path.name)
        if not m:
            continue
        scenario, seed, mode = m.groups()
        c = counters(path)
        rows = census(pathlib.Path(str(path).replace("-diag_ai.txt", "-diag_census.txt")))
        last = max((r["frame"] for r in rows), default=0)
        per = collections.Counter()
        peak = collections.Counter()
        for r in rows:
            if r["state"] == "hole":
                continue
            per[(r["frame"], r["player"], r["template"])] += 1
        for (f, p, t), n in per.items():
            peak[(p, t)] = max(peak[(p, t)], n)
        supply = collections.defaultdict(int)
        for r in rows:
            if r["frame"] == last and r["state"] != "hole" and ("SupplyCenter" in r["template"] or "SupplyStash" in r["template"]):
                supply[r["player"]] += 1
        result[scenario].append(dict(seed=seed, mode=mode, digest=digest(str(path).replace("-diag_ai.txt", "_summary.txt")),
                                     structures_last_census=sum(1 for r in rows if r["frame"] == last and r["state"] != "hole"),
                                     largest_single_type=max(peak.values()) if peak else 0,
                                     supply_centers_last_census=sum(supply.values()),
                                     x52={k: v for k, v in c.items() if k.startswith("x52.") and "sector" not in k}))
    return result


def main(argv):
    if len(argv) != 7:
        print(__doc__)
        return 2
    root = pathlib.Path(argv[1])
    out = pathlib.Path(argv[6])
    out.mkdir(parents=True, exist_ok=True)
    summary = dict(layout=layout(root), threat=threat(root), rebuild=rebuild(root), long=long_runs(root))
    (out / "evidence-summary.json").write_text(json.dumps(summary, indent=1))
    for src, name in ((argv[2], "gate-Generals.json"), (argv[3], "gate-ZeroHour.json"), (argv[4], "perf-Generals.json"), (argv[5], "perf-ZeroHour.json")):
        for candidate in pathlib.Path(src).glob("gate-*.json"):
            shutil.copy(candidate, out / name)
    print("wrote", out / "evidence-summary.json")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
