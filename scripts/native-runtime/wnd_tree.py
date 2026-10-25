#!/usr/bin/env python3
"""Read-only dump of the window tree of one retail WND file: wnd_tree.py <game-dir> <Window/Menus/File.wnd>
Prints depth, name, type, rectangle (800x600 reference), status flags and the text label key for every window (used to design Modern screen skins)."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import ui_archaeology as u


def main(argv):
    fs = u.effective_filesystem(argv[1])
    text = u.fetch(fs[argv[2].lower()]).decode("latin1").replace("\r", "")
    depth = -1
    name = typ = rect = status = label = None
    out = []
    for line in text.split("\n"):
        s = line.strip()
        if s == "WINDOW":
            depth += 1
            name = typ = rect = status = label = None
            cur = {"depth": depth}
            out.append(cur)
        elif s == "END" and depth >= 0 and out:
            depth -= 1
        elif out:
            cur = out[-1]
            m = re.match(r'WINDOWTYPE = (\w+)', s)
            if m: cur["type"] = m.group(1)
            m = re.match(r'NAME = "([^"]+)"', s)
            if m: cur["name"] = m.group(1).split(":")[-1]
            m = re.match(r'UPPERLEFT: (\d+) (\d+),', s) or re.search(r'SCREENRECT = UPPERLEFT: (\d+) (\d+),', s)
            if m and "ul" not in cur: cur["ul"] = (int(m.group(1)), int(m.group(2)))
            m = re.match(r'BOTTOMRIGHT: (\d+) (\d+),', s)
            if m and "br" not in cur: cur["br"] = (int(m.group(1)), int(m.group(2)))
            m = re.match(r'STATUS = ([^;]*);', s)
            if m: cur["status"] = m.group(1)
            m = re.match(r'TEXT = "([^"]*)"', s)
            if m and "text" not in cur: cur["text"] = m.group(1)
    for w in out:
        ul, br = w.get("ul"), w.get("br")
        r = "%dx%d@%d,%d" % (br[0] - ul[0], br[1] - ul[1], ul[0], ul[1]) if ul and br else "?"
        st = w.get("status", "")
        print("%s%s %s %s %s%s" % ("  " * w["depth"], w.get("name"), w.get("type"), r, w.get("text", ""), " HIDDEN" if "HIDDEN" in st else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
