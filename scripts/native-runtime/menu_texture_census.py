"""Offline census of the textures the original MainMenu can draw.

Reads only BIG archives in a staged (copied) game directory; never executes a
title. For every image referenced by the menu layouts reachable from the
ordinary-menu profile it resolves the mapped-image atlas file and classifies the
file against the D3D11 UI texture contract (raw/RLE 24/32-bit TGA, single-mip
DXT1/3/5 DDS). Anything else would fail explicitly at draw time.

usage: menu_texture_census.py <game-dir> [layout ...]
"""
import re
import struct
import sys
from pathlib import Path

LAYOUTS = ["Window/Menus/MainMenu.wnd", "Window/Menus/BlankWindow.wnd",
           "Window/Menus/QuitMessageBox.wnd", "Window/Menus/IMECandidateWindow.wnd",
           "Window/IMECandidateWindow.wnd", "Window/Menus/MessageBox.wnd"]


def read_big(path):
    entries = {}
    with open(path, "rb") as f:
        head = f.read(16)
        if head[:4] != b"BIGF":
            raise ValueError(f"{path} is not BIG")
        count = struct.unpack(">I", head[8:12])[0]
        for _ in range(count):
            off, size = struct.unpack(">II", f.read(8))
            name = bytearray()
            while True:
                c = f.read(1)
                if c == b"\0":
                    break
                name += c
            entries[name.decode("latin1").replace("\\", "/").lower()] = (path, off, size)
    return entries


def fetch(entry, limit=None):
    path, off, size = entry
    with open(path, "rb") as f:
        f.seek(off)
        return f.read(size if limit is None else min(size, limit))


def classify_texture(data):
    if data[:4] == b"DDS ":
        fourcc = data[84:88]
        mips = struct.unpack("<I", data[28:32])[0]
        if fourcc in (b"DXT1", b"DXT3", b"DXT5"):
            return ("ok", f"DDS {fourcc.decode()} mips={mips}")
        return ("UNSUPPORTED", f"DDS fourcc={fourcc!r}")
    if len(data) < 18:
        return ("INVALID", "short")
    image_type, bits = data[2], data[16]
    if image_type in (2, 10) and bits in (24, 32):
        return ("ok", f"TGA type={image_type} bpp={bits}")
    return ("UNSUPPORTED", f"TGA type={image_type} bpp={bits}")


def main():
    game = Path(sys.argv[1])
    layouts = sys.argv[2:] or LAYOUTS
    archives = {}
    for big in sorted(game.glob("*.big")):
        try:
            for k, v in read_big(big).items():
                archives.setdefault(k, v)
        except Exception as error:  # report but continue
            print(f"skip {big.name}: {error}")
    # Mapped-image definitions: name -> texture file.
    mapped = {}
    for key, entry in archives.items():
        if "mappedimages" in key and key.endswith(".ini"):
            text = fetch(entry).decode("latin1")
            for m in re.finditer(r"MappedImage\s+(\S+)(.*?)\bEnd\b", text, re.I | re.S):
                t = re.search(r"Texture\s*=\s*(\S+)", m.group(2), re.I)
                if t:
                    mapped[m.group(1).lower()] = t.group(1)
    print(f"mapped images: {len(mapped)}")
    names = set()
    for layout in layouts:
        entry = archives.get(layout.lower()) or archives.get(layout.lower().replace("window/", "window/menus/"))
        if not entry:
            print(f"layout missing: {layout}")
            continue
        text = fetch(entry).decode("latin1")
        found = {m.group(1) for m in re.finditer(r"IMAGE:\s*([A-Za-z0-9_\-]+)", text)}
        found.discard("NoImage")
        print(f"{layout}: {len(found)} images")
        names |= found
    textures = {}
    for name in sorted(names):
        file = mapped.get(name.lower())
        if not file:
            print(f"  image {name}: no mapped-image definition")
            continue
        textures.setdefault(file, []).append(name)
    bad = 0
    for file, users in sorted(textures.items()):
        stem, dot, ext = file.rpartition(".")
        # Original mapped images name an atlas stem; the shipped file carries the
        # "userinterface" suffix for part of the set.
        candidates = [file, f"{stem}userinterface{dot}{ext}"]
        entry = next((archives[k.lower()] for c in candidates for k in ("art/textures/" + c, c)
                      if k.lower() in archives), None)
        if not entry:
            print(f"  {file}: not in archives (users: {', '.join(users[:4])})")
            bad += 1
            continue
        status, detail = classify_texture(fetch(entry, 256))
        flag = "" if status == "ok" else "  <== " + status
        bad += status != "ok"
        print(f"  {file}: {detail}{flag} ({len(users)} images)")
    print(f"textures: {len(textures)} unsupported_or_missing: {bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
