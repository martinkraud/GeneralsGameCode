#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Distribution audit helper: every executable / DLL / module the Release configuration of a CMake build tree links (the artifacts an upstream
TheSuperHackers weekly ZIP collects: all .exe/.dll/.pdb files directly in <build>/Core and <build>/<Game>, see .github/workflows/reusable-build-toolchain.yml).

Usage: audit_build_outputs.py [build/win32 | build/win64 ...]   -> JSON on stdout: {tree: [{output, kind, directory, target_dir}]}
"""
import json
import re
import sys


def outputs(tree):
    result = []
    path = tree.rstrip("/") + "/CMakeFiles/impl-Release.ninja"
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.startswith("build "):
                continue
            head, _, rest = line[6:].partition(": ")
            rule = rest.split(" ", 1)[0]
            kind = ("executable" if "EXECUTABLE_LINKER" in rule else "shared library (DLL)" if "SHARED_LIBRARY_LINKER" in rule
                    else "module (DLL)" if "MODULE_LIBRARY_LINKER" in rule else None)
            if not kind:
                continue
            out = head.split(" | ")[0].replace("\\", "/")
            result.append({"output": out, "kind": kind, "directory": out.rsplit("/", 1)[0] if "/" in out else ".", "rule": rule})
    return sorted(result, key=lambda r: r["output"])


if __name__ == "__main__":
    trees = sys.argv[1:] or ["build/win32", "build/win64"]
    json.dump({t: outputs(t) for t in trees}, sys.stdout, indent=1)
