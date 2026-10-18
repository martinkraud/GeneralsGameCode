#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Distribution-completeness audit helper: which repository files are compiled into which player executable.

Reads the Ninja build graph and dependency log of a Win64 Release build tree (default build/win64) and prints, as JSON, for the two player executables
(generalsv.exe = Generals, generalszh.exe = Zero Hour) the set of repository-relative source files (.cpp) and headers that are linked into them, by walking
the real graph: executable <- static libraries <- objects <- sources, and objects <- headers (the msvc dependency log). Nothing is guessed from paths.

Usage: audit_compiled_files.py [--build build/win64] [--ninja <path to ninja.exe>] > compiled.json
"""
import argparse
import json
import os
import re
import subprocess
import sys

TARGETS = {"Generals": "Generals/Release/generalsv.exe", "ZeroHour": "GeneralsMD/Release/generalszh.exe"}


def unescape(token):
    return token.replace("$:", ":").replace("$ ", " ").replace("$$", "$").replace("\\", "/")


def parse_graph(path):
    edges = {}   # output -> (rule, [inputs])
    with open(path, encoding="utf-8", errors="replace") as handle:
        text = handle.read().replace("$\n    ", "").replace("$\n", "")
    for line in text.split("\n"):
        if not line.startswith("build "):
            continue
        head, _, rest = line[6:].partition(": ")
        outputs = [unescape(o) for o in re.split(r"(?<!\$) ", head.split(" | ")[0]) if o]
        rest = rest.strip()
        rule, _, inputs = rest.partition(" ")
        inputs = inputs.split(" || ")[0].replace(" | ", " ")
        ins = [unescape(i) for i in re.split(r"(?<!\$) ", inputs) if i]
        if " | " in rest and not inputs:
            pass
        for out in outputs:
            edges[out] = (rule, ins)
    return edges


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="build/win64")
    ap.add_argument("--ninja", default="C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe")
    args = ap.parse_args()
    root = os.path.abspath(args.build)
    repo = os.path.abspath(os.path.join(root, "..", ".."))
    edges = parse_graph(os.path.join(root, "CMakeFiles", "impl-Release.ninja"))
    deps_text = subprocess.run([args.ninja, "-f", "build-Release.ninja", "-t", "deps"], cwd=root, capture_output=True, text=True, errors="replace").stdout
    obj_headers = {}
    current = None
    for line in deps_text.split("\n"):
        if line and not line.startswith(" "):
            current = line.split(": #deps")[0].replace("\\", "/")
            if current.endswith(".ddi"):
                current = current[:-4]
            obj_headers.setdefault(current, [])
        elif line.strip() and current:
            obj_headers[current].append(line.strip())

    def rel(p):
        full = os.path.normpath(p if os.path.isabs(p) else os.path.join(root, p))
        try:
            return os.path.relpath(full, repo).replace("\\", "/")
        except ValueError:
            return full.replace("\\", "/")

    result = {}
    for title, exe in TARGETS.items():
        seen, objs = set(), set()
        stack = [exe]
        while stack:
            node = stack.pop()
            if node in seen:
                continue
            seen.add(node)
            if node.endswith(".obj"):
                objs.add(node)
                continue
            if node in edges:
                stack.extend(edges[node][1])
        sources, headers = set(), set()
        for obj in objs:
            rule, ins = edges[obj]
            for i in ins:
                if i.endswith((".cpp", ".c")):
                    sources.add(rel(i))
            for h in obj_headers.get(obj, []):
                headers.add(rel(h))
        libs = sorted(n for n in seen if n.endswith(".lib") or n.endswith(".dll"))
        result[title] = {"executable": exe, "sources": sorted(sources), "headers": sorted(headers), "libraries": libs, "objects": len(objs)}
    json.dump(result, sys.stdout, indent=1)


if __name__ == "__main__":
    main()
