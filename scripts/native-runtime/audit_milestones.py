#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Distribution-completeness audit: group the modernization commits into milestones and report, per milestone, which kinds of runtime artifact it produced.

Reads the per-file classification written by audit_history_files.py (changed-files.csv) and the git history; prints one JSON row per milestone with the number of commits,
files, files compiled into the Generals / Zero Hour player executable, files that are loose runtime data (category 3-7, expected 0 apart from the generated
modern-extension data), and the first commit subject. Nothing is inferred from names: a milestone's runtime artifacts are the classified files its commits touched.
"""
import argparse
import csv
import json
import re
import subprocess
from collections import OrderedDict


def run(*cmd):
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", check=True).stdout


def milestone(subject):
    m = re.match(r"^(X64\.\d+[A-Z]?)\b", subject)
    if m:
        return m.group(1)
    for pattern, name in ((r"^Stage 4B", "Stage4B"), (r"^Stage 4A", "Stage4A"), (r"^Developer workflow", "Stage4A"),
                          (r"^RendererBoundary\.(\d)", None), (r"^D3D11\.(\d)", None), (r"^R1b/R2a", "X64.24"), (r"^Make CPU-backed legacy geometry", "X64.24"),
                          (r"^Close X64\.(\d+)", None)):
        mm = re.match(pattern, subject)
        if mm:
            if name:
                return name
            if pattern.startswith("^Close"):
                return "X64." + mm.group(1)
            return subject.split(":")[0]
    keywords = (("simulation rate", "Stage1-3 timing"), ("render FPS", "Stage1-3 timing"), ("interpolation", "Stage1-3 timing"), ("presentation timing", "Stage1-3 timing"),
                ("ground", "Stage1-3 timing"), ("Zero Hour development runtime", "Stage1-3 timing"))
    for needle, name in keywords:
        if needle.lower() in subject.lower():
            return name
    return "housekeeping (docs / README / merge / cleanup)"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="adac468d7")
    ap.add_argument("--csv", default="build/audit-tmp/changed-files.csv")
    args = ap.parse_args()
    classified = {r["path"]: r for r in csv.DictReader(open(args.csv, encoding="utf-8"))}
    log = run("git", "log", "--reverse", "--name-only", "--format=@@%H\t%s", f"{args.base}..HEAD")
    rows = OrderedDict()
    current = None
    for line in log.split("\n"):
        if line.startswith("@@"):
            sha, subject = line[2:].split("\t", 1)
            current = milestone(subject)
            row = rows.setdefault(current, {"milestone": current, "commits": 0, "first": subject[:90], "files": set()})
            row["commits"] += 1
        elif line.strip() and current:
            rows[current]["files"].add(line.strip())
    out = []
    for name, row in rows.items():
        files = [classified[f] for f in row["files"] if f in classified]
        compiled_g = sum(1 for f in files if f["category"] == "1" and f["in_generals_exe"] == "1")
        compiled_z = sum(1 for f in files if f["category"] == "1" and f["in_zerohour_exe"] == "1")
        compiled_any = sum(1 for f in files if f["category"] == "1")
        data = sum(1 for f in files if f["category"] in ("3", "4", "5", "6", "7"))
        unlinked = sum(1 for f in files if f["category"] == "9")
        out.append({"milestone": name, "commits": row["commits"], "files": len(files), "compiled": compiled_any, "compiled_generals": compiled_g, "compiled_zerohour": compiled_z,
                    "runtime_data_or_generator": data, "not_linked": unlinked, "first_commit": row["first"]})
    json.dump(out, open("build/audit-tmp/milestones.json", "w"), indent=1)
    for r in out:
        print("%-44s commits=%-3d files=%-4d compiled=%-4d G=%-4d Z=%-4d data=%d unlinked=%d" % (r["milestone"], r["commits"], r["files"], r["compiled"], r["compiled_generals"], r["compiled_zerohour"], r["runtime_data_or_generator"], r["not_linked"]))


if __name__ == "__main__":
    main()
