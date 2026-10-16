#!/usr/bin/env python3
"""X64.41 R1: find the first frame, component and object at which a live recording and its re-simulation differ.

Input: two GENERALS_CRC_TRACE files (see docs/modernization/X64_41_PATH_LINE_PHASE_AND_R1.md): the one written while the game was recorded
(live) and the one written while the resulting replay was re-simulated (headless runner or native playback). Both are produced at the same point of
GameLogic::update, so equal states give byte-identical lines.

  python Compare-CrcTraces.py live.trace resim.trace

Prints the first differing frame, which components differ (objects / rng / partition / players / ai), and - when both traces were written with
GENERALS_CRC_TRACE_OBJECTS=1 - the objects whose snapshot CRC differs or that exist on one side only. Exit code 0 = identical on all common frames,
1 = a divergence was found, 2 = usage / input problem.
"""
import sys
import re

def load(path):
    frames = {}      # frame -> dict(components)
    objects = {}     # frame -> {id: (template, crc)}
    session_start = 0
    last_frame = -1
    lines = open(path, errors='replace').read().splitlines()
    # a trace file is appended to: keep only the LAST session (the frame number restarts at the new "# crc trace started")
    for i, line in enumerate(lines):
        if line.startswith('# crc trace started'):
            session_start = i
    for line in lines[session_start:]:
        if line.startswith('F '):
            m = re.match(r'F (\d+) objects=(\w+) count=(\d+) rng=(\w+) partition=(\w+) players=(\w+) ai=(\w+)', line)
            if not m: continue
            frame = int(m.group(1))
            frames[frame] = dict(objects=m.group(2), count=int(m.group(3)), rng=m.group(4), partition=m.group(5), players=m.group(6), ai=m.group(7))
        elif line.startswith('O '):
            parts = line.split()
            if len(parts) != 5: continue
            objects.setdefault(int(parts[1]), {})[int(parts[2])] = (parts[3], parts[4])
    return frames, objects

def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    live_frames, live_objects = load(argv[1])
    resim_frames, resim_objects = load(argv[2])
    common = sorted(set(live_frames) & set(resim_frames))
    if not common:
        print('no common frames (live %d frames, resim %d frames)' % (len(live_frames), len(resim_frames)))
        return 2
    print('live: %d frames, resim: %d frames, %d common (frames %d..%d)' % (len(live_frames), len(resim_frames), len(common), common[0], common[-1]))
    for frame in common:
        a, b = live_frames[frame], resim_frames[frame]
        if a == b:
            continue
        differing = [k for k in ('objects', 'count', 'rng', 'partition', 'players', 'ai') if a[k] != b[k]]
        print('FIRST DIVERGENT FRAME: %d (the state at the start of that logic frame, after the commands of earlier frames)' % frame)
        for k in differing:
            print('  component %-9s live=%s resim=%s' % (k, a[k], b[k]))
        lo, ro = live_objects.get(frame), resim_objects.get(frame)
        if lo is not None and ro is not None:
            only_live = sorted(set(lo) - set(ro))
            only_resim = sorted(set(ro) - set(lo))
            changed = sorted(i for i in set(lo) & set(ro) if lo[i] != ro[i])
            print('  objects only in the live game  : %d %s' % (len(only_live), ['%d(%s)' % (i, lo[i][0]) for i in only_live[:12]]))
            print('  objects only in the resimulation: %d %s' % (len(only_resim), ['%d(%s)' % (i, ro[i][0]) for i in only_resim[:12]]))
            print('  objects present in both with a different snapshot CRC: %d' % len(changed))
            for i in changed[:25]:
                print('    ObjectID %-6d %-32s live=%s resim=%s' % (i, lo[i][0], lo[i][1], ro[i][1]))
            if frame - 1 in live_objects and frame - 1 in resim_objects:
                print('  (the previous frame %d was identical in every object)' % (frame - 1))
        else:
            print('  (re-run both with GENERALS_CRC_TRACE_OBJECTS=1 to name the objects)')
        return 1
    print('NO DIVERGENCE on the %d common frames' % len(common))
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
