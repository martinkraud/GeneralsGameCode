#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Upstream distribution baseline: estimate what TheSuperHackers' weekly ZIP of a game contains by packing the artifact set the workflow collects
(every .exe/.dll/.pdb directly in <build>/Core and <build>/<Game>, zip -jr) from a LOCAL Win32 Release tree built with the same CMake targets, and compare its
size with the published asset size. A match in order of magnitude supports (does not prove) the derived file list; the published sizes were read from the GitHub
release metadata (generals-weekly-2026-10-16.zip 31,987,764 bytes, generalszh-weekly-2026-10-16.zip 33,838,193 bytes). Nothing is downloaded.
Usage: audit_upstream_zip_estimate.py [build/win32]
"""
import glob
import json
import os
import sys
import zipfile

UPSTREAM = {"Generals": 31987764, "GeneralsMD": 33838193}


def main():
    tree = sys.argv[1] if len(sys.argv) > 1 else "build/win32"
    report = {}
    for game, published in UPSTREAM.items():
        files = []
        for directory in (os.path.join(tree, "Core", "Release"), os.path.join(tree, game, "Release")):
            for path in sorted(glob.glob(os.path.join(directory, "*"))):
                if os.path.isfile(path) and path.lower().endswith((".exe", ".dll", ".pdb")):
                    files.append(path)
        target = os.path.join("build", "audit-tmp", f"upstream-estimate-{game}.zip")
        with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for path in files:
                z.write(path, os.path.basename(path))
        raw = sum(os.path.getsize(p) for p in files)
        report[game] = {"files": [os.path.basename(p) for p in files], "file_count": len(files), "raw_bytes": raw, "estimated_zip_bytes": os.path.getsize(target),
                        "published_zip_bytes": published, "ratio_estimate_to_published": round(os.path.getsize(target) / published, 2)}
    json.dump(report, sys.stdout, indent=1)


if __name__ == "__main__":
    main()
