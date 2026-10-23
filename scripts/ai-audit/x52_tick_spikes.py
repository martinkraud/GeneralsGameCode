#!/usr/bin/env python3
"""X64.52: the slowest AI ticks of a benchmark run and the AI events that coincide with them.

  x52_tick_spikes.py <run>_ticks.csv <run>-diag_ai.txt [count]

ai_player_strategy is the per-tick cost of AIPlayer::update for all players. Prints (cost ms, frame, events at that frame).
"""
import csv
import re
import sys

def main(argv):
    if len(argv) < 3:
        print(__doc__); return 2
    count = int(argv[3]) if len(argv) > 3 else 8
    rows = list(csv.DictReader(open(argv[1])))
    top = sorted(((float(r["ai_player_strategy"]), int(r["frame"])) for r in rows), reverse=True)[:count]
    events = open(argv[2]).read().splitlines()
    for cost, frame in top:
        near = [e[:150] for e in events if e.startswith("event") and re.search(r"frame=%d " % frame, e) and "layout.slot" not in e]
        print("%7.1f ms  frame %-6d %s" % (cost, frame, near))
    total = sum(float(r["ai_player_strategy"]) for r in rows)
    print("AIPlayer::update total %.0f ms over %d ticks (mean %.3f ms/tick)" % (total, len(rows), total / max(1, len(rows))))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
