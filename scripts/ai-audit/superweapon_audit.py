#!/usr/bin/env python3
"""X64.52A: audit which buildable objects are superweapon structures in the retail data of both titles (read-only: reads the BIG archives of the Steam installation).

  superweapon_audit.py [--zh <Zero Hour install dir>]

Prints, per title, every object tagged KINDOF FS_SUPERWEAPON with its MaxSimultaneousOfType / MaxSimultaneousLinkKey / Buildable, and every object that uses
"MaxSimultaneousOfType = DeterminedBySuperweaponRestriction" without the tag. No retail content is written anywhere.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scb  # noqa: E402

STEAM_ZH = r"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour"
BS = chr(92)


def object_files(bigs):
    files = {}
    for big in bigs:
        if not os.path.exists(big):
            continue
        for name, data in scb.read_big(big):
            n = name.replace(BS, "/").lower()
            if n.startswith("data/ini/object/") and n.endswith(".ini"):
                files[n] = data
    return files


def parse(files):
    result = {}
    for name, data in sorted(files.items()):
        cur = None
        for line in data.decode("latin-1").splitlines():
            text = line.split(";")[0].strip()
            m = re.match(r"^(Object|ObjectReskin)\s+([^\s=]+)\s*$", line.split(";")[0].rstrip()) if not line[:1].isspace() else None
            if m:
                cur = dict(name=m.group(2), kinds="", det=False, link=None, build=None, file=name, side=None, max=None)
                result[cur["name"]] = cur
                continue
            if cur is None or "=" not in text:
                continue
            key, value = [p.strip() for p in text.split("=", 1)]
            key = key.lower()
            if key == "kindof":
                cur["kinds"] += " " + value
            elif key == "maxsimultaneousoftype":
                cur["max"] = value
                cur["det"] = "DeterminedBySuperweaponRestriction" in value
            elif key == "maxsimultaneouslinkkey":
                cur["link"] = value
            elif key == "buildable":
                cur["build"] = value
            elif key == "side" and cur["side"] is None:
                cur["side"] = value
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--zh", default=STEAM_ZH)
    args = parser.parse_args()
    for title, bigs in (("ZeroHour", [os.path.join(args.zh, "INIZH.big")]), ("Generals", [os.path.join(args.zh, "ZH_Generals", "INI.big")])):
        objs = parse(object_files(bigs))
        print("=====", title, len(objs), "objects")
        tagged = [o for o in objs.values() if "FS_SUPERWEAPON" in o["kinds"]]
        print("tagged FS_SUPERWEAPON:", len(tagged))
        for o in tagged:
            print("  %-34s %-9s structure=%-5s determined=%-5s max=%-34s link=%-22s buildable=%s" % (
                o["name"], o["side"], "STRUCTURE" in o["kinds"], o["det"], o["max"], o["link"], o["build"]))
        stray = [o["name"] for o in objs.values() if o["det"] and "FS_SUPERWEAPON" not in o["kinds"]]
        print("DeterminedBySuperweaponRestriction without the tag:", stray)
    return 0


if __name__ == "__main__":
    sys.exit(main())
