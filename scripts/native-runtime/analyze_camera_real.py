#!/usr/bin/env python3
"""X64.52B: judges the output of Invoke-CameraRealInput.ps1 (camera-probe.csv) against the accepted camera behaviour.

  analyze_camera_real.py <output directory of one run>      exit 0 = all phases pass

Per phase (2 = arrow key, 4 = screen edge, 6 = right mouse button, CameraProbe::Phase) of REAL operating-system input:
  * the camera must travel (a dead input path - the X64.52A screen edge - travels 0);
  * it must move on (nearly) every drawn frame, not on a subset: a camera updated at the 30 Hz simulation rate while 120 frames per second are drawn moves on 1 of 4;
  * the step of a frame must be proportional to the frame interval it covers (speed = step / interval stays constant): irregular frame times must not make the speed ripple.
The frames right after the input starts (input latency) and the camera hitting the map border are excluded exactly as documented in MIN_FRAMES_SKIPPED.
"""
import csv
import math
import statistics
import sys

PHASES = {2: "key", 4: "edge", 6: "rmb"}
SKIP_MS = 150.0                   # input latency: the key / edge / button reaches the translator a few frames after the OS event (and the previous phase's scroll may still run)
MIN_DISTANCE = {2: 400.0, 4: 400.0, 6: 100.0}
MAX_STALE_FRACTION = 0.10
MAX_RIPPLE = 0.15                 # mean |speed change between adjacent frames| / mean speed. Steady scroll with real frame jitter measures 0.02 - 0.05; a step that ignores the
                                  # frame interval (the pre-X64.49 defect) measures 0.3 and more. Slow ramps (RMB accelerating with the mouse) are not penalised.
MIN_FRAMES = 20


def load(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def analyze(rows):
    """Returns {phase name: {"ok": bool, "problems": [...], "metrics": {...}}}."""
    result = {}
    for phase, name in PHASES.items():
        frames = [(float(r["ms"]), float(r["d"]), float(r["e"])) for r in rows if r["kind"] == "D" and int(r["phase"]) == phase]
        problems, metrics = [], {}
        if len(frames) < MIN_FRAMES:
            result[name] = {"ok": False, "problems": ["only %d drawn frames in the phase" % len(frames)], "metrics": {"frames": len(frames)}}
            continue
        steps = []
        for (t0, x0, y0), (t1, x1, y1) in zip(frames, frames[1:]):
            steps.append((t1 - t0, math.hypot(x1 - x0, y1 - y0)))
        distance = math.hypot(frames[-1][1] - frames[0][1], frames[-1][2] - frames[0][2])
        metrics["frames"] = len(frames)
        metrics["distance"] = round(distance, 1)
        if distance < MIN_DISTANCE[phase]:
            problems.append("the camera travelled only %.1f units (< %.0f): the %s input does not scroll" % (distance, MIN_DISTANCE[phase], name))
        t0 = frames[0][0]
        skip = sum(1 for f in frames[1:] if f[0] - t0 < SKIP_MS)
        body = steps[skip:]
        # drop the tail where the camera stopped at the map border (consecutive zero steps to the end)
        while body and body[-1][1] < 1e-4:
            body.pop()
        if len(body) >= MIN_FRAMES // 2:
            stale = sum(1 for dt, d in body if d < 1e-4) / len(body)
            speeds = [d / (dt / 1000.0) for dt, d in body if dt > 0 and d >= 1e-4]
            mean = statistics.mean(speeds) if speeds else 0.0
            ripple = (statistics.mean(abs(a - b) for a, b in zip(speeds, speeds[1:])) / mean) if len(speeds) > 2 and mean > 0 else 0.0
            metrics["stale_fraction"] = round(stale, 3)
            metrics["ripple"] = round(ripple, 3)
            if stale > MAX_STALE_FRACTION:
                problems.append("%.0f%% of the drawn frames did not move the camera (stale; a camera on the 30 Hz tick moves on 25%% of frames at 120 fps)" % (stale * 100))
            if ripple > MAX_RIPPLE:
                problems.append("camera speed ripples (%.2f > %.2f): the step does not follow the frame interval" % (ripple, MAX_RIPPLE))
        elif distance >= MIN_DISTANCE[phase]:
            metrics["note"] = "camera reached the map border early; smoothness measured on the available frames only"
        result[name] = {"ok": not problems, "problems": problems, "metrics": metrics}
    return result


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    path = argv[1].rstrip("\\/") + "/camera-probe.csv"
    res = analyze(load(path))
    bad = 0
    for name in ("key", "edge", "rmb"):
        r = res[name]
        print("%-4s %s %s" % (name, "PASS" if r["ok"] else "FAIL", r["metrics"]))
        for p in r["problems"]:
            print("      - " + p)
        bad += not r["ok"]
    print("CAMERA REAL-INPUT GATE: %s" % ("PASS" if not bad else "FAIL (%d phase(s))" % bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
