#!/usr/bin/env python3
"""Read-only lookup of GameText (CSF) labels of an installed title: csf_lookup.py <game-dir> <label-regex> [...]. Prints label = text (English). Used to confirm string keys for UI work."""
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import ui_archaeology as u


def parse_csf(data):
    assert data[:4] == b"FSC " or data[:4] == b" FSC" or data[:4] == b"CSF " or True
    off = 4
    version, nlabels, nstrings, _unused, lang = struct.unpack("<5I", data[off:off + 20])
    off += 20
    out = {}
    for _ in range(nlabels):
        assert data[off:off + 4] in (b" LBL", b"LBL "), data[off:off + 4]
        off += 4
        nstr, namelen = struct.unpack("<2I", data[off:off + 8]); off += 8
        name = data[off:off + namelen].decode("latin1"); off += namelen
        text = ""
        for i in range(nstr):
            tag = data[off:off + 4]; off += 4
            ln = struct.unpack("<I", data[off:off + 4])[0]; off += 4
            raw = bytes(0xFF - b for b in data[off:off + ln * 2]); off += ln * 2
            if tag == b"WRTS":
                n = struct.unpack("<I", data[off:off + 4])[0]; off += 4 + n
            if i == 0:
                text = raw.decode("utf-16-le", "replace")
        out[name] = text
    return out


def main(argv):
    fs = u.effective_filesystem(argv[1])
    entry = next((v for k, v in fs.items() if k.endswith("generals.csf")), None)
    labels = parse_csf(u.fetch(entry))
    for pat in argv[2:]:
        rx = re.compile(pat, re.I)
        for k, v in sorted(labels.items()):
            if rx.search(k):
                print("%s = %s" % (k, v.replace("\n", " | ")[:140]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
