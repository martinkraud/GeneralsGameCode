#!/usr/bin/env python3
"""Summarise the pointer-truncation compiler warnings (C4311 / C4312 / C4302) of a full MSVC x64 build log.

    python scripts/diagnostics/Summarize-PointerWarnings.py <build.log> [--out unique.txt]

Method of the AMD64 audit: the win64 preset builds at /W3, which reports a pointer cast to a smaller integer (C4311), an integer cast to a larger pointer
(C4312) and a pointer truncation (C4302). A *full* rebuild is needed (a log of an incremental build only covers the files that were compiled).
Prints the number of distinct sites per top-level directory and per warning, and writes one line per distinct site (file:line code message).
"""
import argparse
import collections
import re
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--out", default="")
    args = ap.parse_args()
    text = open(args.log, errors="replace").read()
    root = None
    hits = set()
    for m in re.finditer(r"([A-Za-z]:[^\n(]+?)\((\d+)\): warning (C43(?:11|12|02)): ([^\n\[]+)", text):
        path, line, code, message = m.groups()
        if root is None:
            idx = path.lower().find("generalsgamecode")
            root = path[: idx + len("generalsgamecode")] if idx >= 0 else ""
        rel = path[len(root):].lstrip("\\/") if root else path
        hits.add((rel.replace("\\", "/"), int(line), code, message.strip()[:120]))
    by = collections.Counter()
    for rel, _line, code, _message in hits:
        parts = rel.split("/")
        by[("/".join(parts[:2]), code)] += 1
    print("distinct sites: %d" % len(hits))
    for key, count in by.most_common():
        print("%4d  %-45s %s" % (count, key[0], key[1]))
    if args.out:
        with open(args.out, "w") as f:
            for rel, line, code, message in sorted(hits):
                f.write("%s:%d %s %s\n" % (rel, line, code, message))
    return 0


if __name__ == "__main__":
    sys.exit(main())
