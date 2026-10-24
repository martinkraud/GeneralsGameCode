#!/usr/bin/env python3
"""UI.1: offline census of the retail frontend / HUD window layouts (WND) of an installed title.

  ui_archaeology.py <game-dir> <title-label> <output-json>

Reads the BIG archives of the installation READ-ONLY (never executes the game, never copies an archive entry out of the process: only counts and names are written).
For every Window/**/*.wnd of the effective file system (later archives, Patch* last, override earlier ones) it records the number of windows per gadget type, the creation
resolutions, the callbacks, the fonts, the images referenced and the extent of the layout. The output is the evidence behind docs/modernization/MODERN_UI_ARCHAEOLOGY.md.
"""
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path


def read_big(path):
    entries = {}
    with open(path, "rb") as f:
        head = f.read(16)
        if head[:4] != b"BIGF":
            return entries
        count = struct.unpack(">I", head[8:12])[0]
        for _ in range(count):
            off, size = struct.unpack(">II", f.read(8))
            name = bytearray()
            while True:
                c = f.read(1)
                if c in (b"\0", b""):
                    break
                name += c
            entries[name.decode("latin1").replace("\\", "/").lower()] = (path, off, size)
    return entries


def effective_filesystem(game_dir):
    bigs = sorted(Path(game_dir).glob("*.big"), key=lambda p: (("patch" in p.name.lower()), p.name.lower()))
    fs = {}
    for big in bigs:
        fs.update(read_big(big))
    return fs


def fetch(entry):
    path, off, size = entry
    with open(path, "rb") as f:
        f.seek(off)
        return f.read(size)


GADGET = re.compile(r"WINDOWTYPE\s*=\s*([A-Z_]+)\s*;")
RECT = re.compile(r"SCREENRECT\s*=\s*UPPERLEFT:\s*(-?\d+)\s+(-?\d+)\s*,\s*BOTTOMRIGHT:\s*(-?\d+)\s+(-?\d+)\s*,\s*CREATIONRESOLUTION:\s*(\d+)\s+(\d+)")
LAYOUT = re.compile(r"(LAYOUTINIT|LAYOUTUPDATE|LAYOUTSHUTDOWN)\s*=\s*([A-Za-z0-9_\[\]]+)\s*;")
CALLBACK = re.compile(r"(SYSTEMCALLBACK|INPUTCALLBACK|TOOLTIPCALLBACK|DRAWCALLBACK)\s*=\s*\"([^\"]*)\"")
FONT = re.compile(r"FONT\s*=\s*NAME:\s*\"([^\"]*)\"\s*,\s*SIZE:\s*(\d+)")
IMAGE = re.compile(r"IMAGE:\s*([A-Za-z0-9_]+)")
STATUS = re.compile(r"STATUS\s*=\s*([A-Z_+ ]+);")


def census_file(text):
    kinds = Counter(GADGET.findall(text))
    rects = [tuple(int(v) for v in m) for m in RECT.findall(text)]
    res = Counter((r[4], r[5]) for r in rects)
    ext = [0, 0]
    for r in rects:
        ext[0] = max(ext[0], r[2] * 800 // max(r[4], 1))
        ext[1] = max(ext[1], r[3] * 600 // max(r[5], 1))
    callbacks = Counter(name for _, name in CALLBACK.findall(text))
    fonts = Counter("%s %s" % f for f in FONT.findall(text))
    statuses = Counter()
    for s in STATUS.findall(text):
        for token in s.replace(" ", "").split("+"):
            statuses[token] += 1
    return {
        "windows": sum(kinds.values()),
        "layout_callbacks": dict(LAYOUT.findall(text)),
        "gadgets": dict(kinds),
        "creation_resolutions": {"%dx%d" % k: v for k, v in res.items()},
        "extent_on_800x600": ext,
        "callbacks": sorted(callbacks),
        "fonts": dict(fonts),
        "images_referenced": len(set(IMAGE.findall(text))),
        "status_flags": dict(statuses),
    }


def main(argv):
    if len(argv) != 4:
        print(__doc__)
        return 2
    game_dir, label, out = argv[1], argv[2], argv[3]
    fs = effective_filesystem(game_dir)
    files = {}
    for name, entry in sorted(fs.items()):
        if name.startswith("window/") and name.endswith(".wnd"):
            files[name] = census_file(fetch(entry).decode("latin1"))
    ini = {}
    for name in ("data/ini/shellmenuscheme.ini", "data/ini/windowtransitions.ini", "data/ini/controlbarscheme.ini", "data/ini/mouse.ini", "data/ini/mappedimage.ini"):
        entry = fs.get(name) or next((v for k, v in fs.items() if k.endswith("/" + name.split("/")[-1])), None)
        if entry:
            text = fetch(entry).decode("latin1")
            ini[name.split("/")[-1]] = {"bytes": len(text), "lines": text.count("\n"), "top_level_blocks": len(re.findall(r"^\s*(?:ShellMenuScheme|Transition|ControlBarScheme|MappedImage|Mouse)\b", text, re.M | re.I))}
    totals = Counter()
    for f in files.values():
        totals.update(f["gadgets"])
    data = {"title": label, "wnd_files": len(files), "total_windows": sum(totals.values()), "gadget_totals": dict(totals), "ini": ini, "files": files}
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_text(json.dumps(data, indent=1, sort_keys=True))
    print("%s: %d WND files, %d windows, gadgets %s" % (label, len(files), sum(totals.values()), dict(totals.most_common(8))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
