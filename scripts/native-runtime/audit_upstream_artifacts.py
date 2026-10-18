#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Upstream distribution baseline audit.

TheSuperHackers' weekly release ZIPs (generals-weekly-<date>.zip / generalszh-weekly-<date>.zip) are produced by .github/workflows/weekly-release.yml:
the vc6-weekly preset is configured with the Core, Generals / Zero Hour tools ON and extras OFF, and reusable-build-toolchain.yml "Collect Artifact" moves every
.exe, .dll and .pdb that lies directly in <build>/Core and <build>/<Game> into the ZIP (zip -jr: no directories). The artifact set is therefore exactly the set of
executables / DLLs the CMake graph links into those two directories, plus their .pdb files. This script reads that set from the real Release link graph of a
build tree and, for each artifact, walks the graph (executable <- libraries <- objects <- sources) to report how many of the repository files changed by the
MKRaudstein modernization branch (git diff <base>..HEAD) are linked into it.

Usage: audit_upstream_artifacts.py [--build build/win64] [--base adac468d7] > upstream-artifacts.json
"""
import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
from audit_compiled_files import parse_graph   # noqa: E402


def run(*cmd):
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", check=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="build/win64")
    ap.add_argument("--base", default="adac468d7")
    ap.add_argument("--ninja", default="C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe")
    args = ap.parse_args()
    root = os.path.abspath(args.build)
    repo = os.path.abspath(os.path.join(root, "..", ".."))
    edges = parse_graph(os.path.join(root, "CMakeFiles", "impl-Release.ninja"))
    changed = set(l.split("\t")[-1] for l in run("git", "diff", "--name-status", f"{args.base}..HEAD").strip().split("\n"))
    deps_text = subprocess.run([args.ninja, "-f", "build-Release.ninja", "-t", "deps"], cwd=root, capture_output=True, text=True, errors="replace").stdout
    obj_headers, current = {}, None
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
        return os.path.relpath(full, repo).replace("\\", "/")

    artifacts = []
    for out, (rule, ins) in edges.items():
        kind = ("executable" if "EXECUTABLE_LINKER" in rule else "DLL" if ("SHARED_LIBRARY_LINKER" in rule or "MODULE_LIBRARY_LINKER" in rule) else None)
        if not kind or not (out.startswith("Core/") or out.startswith("Generals/") or out.startswith("GeneralsMD/")):
            continue
        seen, objs, stack = set(), set(), [out]
        while stack:
            node = stack.pop()
            if node in seen:
                continue
            seen.add(node)
            if node.endswith(".obj"):
                objs.add(node)
            elif node in edges:
                stack.extend(edges[node][1])
        linked = set()
        for obj in objs:
            for i in edges[obj][1]:
                if i.endswith((".cpp", ".c")):
                    linked.add(rel(i))
            for h in obj_headers.get(obj, []):
                linked.add(rel(h))
        mine = sorted(linked & changed)
        artifacts.append({"output": out, "kind": kind, "objects": len(objs), "linked_files": len(linked), "changed_files_linked": len(mine), "changed_sample": mine[:6]})
    json.dump(sorted(artifacts, key=lambda a: a["output"]), sys.stdout, indent=1)


if __name__ == "__main__":
    main()
