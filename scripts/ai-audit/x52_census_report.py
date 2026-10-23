#!/usr/bin/env python3
"""X64.52: reports over the read-only AI diagnostics output (GENERALS_AI_DIAG_CENSUS / _DUMP) of Invoke-AiBaseExperiments.ps1.

  structures  <census files...>              structures per player and template at every census frame
  supply      <census files...>              supply centers / stashes per player over time (expansion)
  defense     <census> <player> <angle>...   fraction of base defences within 60 degrees of the given attack angle
  layout      <ai file O> <ai file E>...     per-slot shift of the Enhanced build list against the Original one (needs GENERALS_AI_DIAG_DUMP)
  peaks       <census files...>              the largest number of any single structure type per player (runaway check)

Only reads text files; no engine state. Python 3.
"""
import collections
import math
import re
import sys

CENSUS = re.compile(r"census frame=(\d+) player=(\d+) side=(\S+) template=(\S+) x=(-?\d+) y=(-?\d+) dx=(-?\d+) dy=(-?\d+) (\S+)")
MONEY = re.compile(r"census frame=(\d+) player=(\d+) money=(-?\d+) base=(-?\d+),(-?\d+) radius=(\d+)")
DUMP = re.compile(r"event frame=\d+ buildlist player=(\d+) entry=(\S+) automatic=\d priority=\d rebuilds=\S+ dx=(-?\d+) dy=(-?\d+)")
DEFENSE_WORDS = ("Gattling", "Bunker", "Patriot", "Stinger", "FireBase", "Tower", "Cannon", "DemoTrap", "Defense")


def load_census(path):
    rows, money = [], {}
    with open(path) as handle:
        for line in handle:
            m = CENSUS.match(line)
            if m:
                frame, player, side, template, x, y, dx, dy, state = m.groups()
                rows.append(dict(frame=int(frame), player=int(player), side=side, template=template, dx=int(dx), dy=int(dy), state=state))
                continue
            m = MONEY.match(line)
            if m:
                money[(int(m.group(1)), int(m.group(2)))] = int(m.group(3))
    return rows, money


def structures(paths):
    for path in paths:
        rows, money = load_census(path)
        print("==", path)
        table = collections.defaultdict(collections.Counter)
        side = {}
        for r in rows:
            if r["state"] != "hole":
                table[(r["frame"], r["player"])][r["template"]] += 1
                side[r["player"]] = r["side"]
        for (frame, player), counts in sorted(table.items(), key=lambda k: (k[0][1], k[0][0])):
            print("p%d %-22s f%-6d n=%3d money=%-6s %s" % (player, side[player], frame, sum(counts.values()), money.get((frame, player), "?"),
                  " ".join("%sx%d" % (t, n) for t, n in sorted(counts.items()))))


def supply(paths):
    for path in paths:
        rows, _ = load_census(path)
        by = collections.defaultdict(collections.Counter)
        for r in rows:
            if r["state"] != "hole" and ("SupplyCenter" in r["template"] or "SupplyStash" in r["template"]):
                by[r["frame"]][r["player"]] += 1
        print("==", path)
        for frame in sorted(by):
            print("  f%-6d %s" % (frame, " ".join("p%d:%d" % (p, n) for p, n in sorted(by[frame].items()))))


def defense(path, player, angles):
    rows, _ = load_census(path)
    found = [(r["template"], (math.degrees(math.atan2(r["dy"], r["dx"])) + 360) % 360) for r in rows
             if r["player"] == player and r["state"] != "hole" and any(w in r["template"] for w in DEFENSE_WORDS)]
    for angle in angles:
        near = sum(1 for _, a in found if min(abs(a - angle), 360 - abs(a - angle)) <= 60)
        print("attack from %3d deg: %2d of %2d defences within 60 degrees" % (angle, near, len(found)))


def layout(paths):
    def read(path):
        out = collections.defaultdict(list)
        with open(path) as handle:
            for line in handle:
                m = DUMP.match(line)
                if m:
                    out[int(m.group(1))].append((m.group(2), int(m.group(3)), int(m.group(4))))
        return out
    base = read(paths[0])
    for path in paths[1:]:
        other = read(path)
        for player in sorted(base):
            if player not in other or len(other[player]) != len(base[player]):
                continue
            d = [math.hypot(a[1] - b[1], a[2] - b[2]) for a, b in zip(base[player], other[player])]
            print("%s player %d: %d slots, mean shift %.1f, max %.1f" % (path, player, len(d), sum(d) / len(d), max(d)))


def peaks(paths):
    for path in paths:
        rows, _ = load_census(path)
        per = collections.defaultdict(collections.Counter)
        for r in rows:
            if r["state"] != "hole":
                per[(r["frame"], r["player"], r["template"])][0] += 1
        peak = collections.defaultdict(lambda: ("", 0))
        for (frame, player, template), c in per.items():
            if c[0] > peak[player][1]:
                peak[player] = (template, c[0])
        print("==", path)
        for player, (template, n) in sorted(peak.items()):
            print("  p%d most numerous: %s x%d" % (player, template, n))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    command, rest = argv[1], argv[2:]
    if command == "structures": structures(rest)
    elif command == "supply": supply(rest)
    elif command == "defense": defense(rest[0], int(rest[1]), [int(a) for a in rest[2:]])
    elif command == "layout": layout(rest)
    elif command == "peaks": peaks(rest)
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
