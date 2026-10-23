#!/usr/bin/env python3
"""X64.38: generate a deterministic HEAVY simulation-benchmark replay without any human play and without committing game content.

A Generals replay stores only the match setup (map, seed, slots) and the commands the HUMAN players issued. Computer-controlled slots are
simulated by the engine itself during playback. A replay that has an idle human slot and several brutal AI slots is therefore a
self-playing AI-versus-AI battle of any length: playback is the benchmark workload, and the same file always produces the same simulation
on the same build (the determinism oracle compares two runs of it).

Nothing proprietary is written: the output is the header of a replay YOU recorded (any skirmish replay of this build) with a new setup
string, plus a single end marker. The map is referenced by name from your own installation.

  python New-SimulationBenchmarkReplay.py --template "Last Replay.rep" --out bench.rep \
      --map "Wasteland Warlords" --map-size 305428 --ai 7 --level H --minutes 15 --seed 12345

Then:  set GENERALS_SIM_BENCH=<out base> & set GENERALS_SIM_IGNORE_CRC=1 & generalsv.exe -headless -replay bench.rep
"""
import argparse
import struct
import sys


def cstr_end(data, offset, wide):
    if wide:
        end = offset
        while data[end:end + 2] != b"\0\0" or (end - offset) % 2:
            end += 1
        return end + 2
    return data.index(b"\0", offset) + 1


def parse_header(data):
    """Returns (offsets, game_info, local_index_text, end_of_header_fields)."""
    if data[:6] != b"GENREP":
        raise SystemExit("not a GENREP replay")
    o = 6 + 4 + 4 + 4          # start time, end time, frame count
    o += 2 + 8                 # desync, quit early, player disconnect flags (MAX_SLOTS = 8)
    o = cstr_end(data, o, True)   # replay name
    o += 16                    # SYSTEMTIME
    o = cstr_end(data, o, True)   # version string
    o = cstr_end(data, o, True)   # version time string
    o += 12                    # version number, exe CRC, ini CRC
    gi_start = o
    o = cstr_end(data, o, False)
    gi_end = o
    o = cstr_end(data, o, False)   # local player index text
    local_end = o
    tail_start = o             # difficulty, original game mode, rank points, max FPS follow (16 bytes)
    return gi_start, gi_end, local_end, tail_start


def build_setup(args):
    # Every random choice is resolved here (the lobby resolves them before a replay is written; an unresolved -1 would leave a player
    # without a faction and its AI idle). Templates cycle USA / China / GLA (PlayerTemplate.ini order: Civilian 0, Observer 1,
    # America 2, China 3, GLA 4), start positions are the slot numbers, teams are free-for-all.
    slots = []
    # slot 0: an idle human (never issues a command) so the replay has a local player; the rest are AI
    slots.append("HBenchmark,0,0,TT,0,2,0,-1,1:")
    level = {"E": "E", "M": "M", "H": "H"}[args.level]
    factions = [int(f) for f in args.factions.split(",")] if args.factions else None
    for i in range(1, args.ai + 1):
        template = factions[(i - 1) % len(factions)] if factions else 2 + (i % 3)   # X64.51: --factions picks PlayerTemplate indices (Zero Hour generals)
        slots.append("C%s,%d,%d,%d,-1:" % (level, i, template, i))
    while len(slots) < 8:
        slots.append("X:")
    # X64.51: --ai-mode E writes the AI Behaviour rule (AI=E2: Enhanced revision 2) into the setup; Original (the default) writes nothing, so Original setups stay byte-identical.
    behavior = "AI=E2;" if args.ai_mode == "E" else ""
    return "M=03maps/%s;MC=%X;MS=%d;SD=%d;C=%d;%sS=%s;" % (
        args.map.lower(), args.map_crc, args.map_size, args.seed, args.crc_interval, behavior, "".join(slots))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--template", required=True, help="a replay recorded by this build (only its header is used)")
    p.add_argument("--out", required=True)
    p.add_argument("--map", required=True, help="map name as in the installation, e.g. 'Wasteland Warlords'")
    p.add_argument("--map-size", type=int, required=True, help="MS= value: the size of the .map file in bytes")
    p.add_argument("--map-crc", type=lambda v: int(v, 16), default=0, help="MC= value in hex (0 = unchecked in headless playback)")
    p.add_argument("--ai", type=int, default=7, help="number of AI slots (1..7)")
    p.add_argument("--level", choices="EMH", default="H", help="AI level: E easy, M medium, H brutal")
    p.add_argument("--minutes", type=float, default=10.0, help="length of the simulated game")
    p.add_argument("--seed", type=int, default=12345)
    p.add_argument("--factions", default="", help="comma separated PlayerTemplate indices for the AI slots (default cycles America, China, GLA = 2,3,4)")
    p.add_argument("--ai-mode", choices="OE", default="O", help="AI Behaviour rule: O original (default, nothing written), E enhanced (AI=E2)")
    p.add_argument("--crc-interval", type=int, default=100)
    args = p.parse_args()
    if not 1 <= args.ai <= 7:
        raise SystemExit("--ai must be 1..7")

    data = open(args.template, "rb").read()
    gi_start, gi_end, local_end, tail_start = parse_header(data)
    frames = int(args.minutes * 60 * 30)

    head = bytearray(data[:gi_start])
    struct.pack_into("<I", head, 6 + 8, frames)               # frame count field
    setup = build_setup(args).encode("ascii") + b"\0"
    local = b"0\0"
    tail = bytearray(data[local_end:local_end + 16])          # difficulty, game mode, rank, max fps (copied from the template)
    # one end marker: type 1000 (MSG_BEGIN_NETWORK_MESSAGES, a CRC message the reader discards), local player, no arguments
    marker = struct.pack("<iiiB", frames, 1000, 0, 0)
    out = bytes(head) + setup + local + bytes(tail) + marker
    open(args.out, "wb").write(out)
    print("wrote %s: %d bytes, %d frames (%.1f min), %d AI slots, level %s, map %s" % (
        args.out, len(out), frames, args.minutes, args.ai, args.level, args.map))


if __name__ == "__main__":
    sys.exit(main())
