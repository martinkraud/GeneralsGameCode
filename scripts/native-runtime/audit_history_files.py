#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Distribution-completeness audit: classify EVERY file changed on the modernization branch since its baseline.

Usage: audit_history_files.py [--base adac468d7] [--compiled compiled.json] [--csv out.csv] [--summary out.json]
`compiled.json` is the output of audit_compiled_files.py (the real Win64 Release link graph of generalsv.exe / generalszh.exe).

Categories (the audit brief):
 1 compiled into the player executable          2 compiled into another runtime binary / DLL        3 runtime data that must be a loose distributed file
 4 generated runtime data (or its generator)    5 shader / renderer resource                        6 WND / UI / layout resource
 7 INI / game-data resource                     8 development / test / build / documentation only   9 superseded / dead / not linked into a player binary
10 original proprietary game content not changed by the project (none can be a repository change; reported by the stage audit)
"""
import argparse
import csv
import json
import re
import subprocess
from collections import Counter, defaultdict

CODE = (".cpp", ".c", ".h", ".inl", ".inc", ".hpp")


def run(*cmd):
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", check=True).stdout


def classify(path, compiled):
    low = path.lower()
    gen = path in compiled["Generals"]["files"]
    zh = path in compiled["ZeroHour"]["files"]
    if low.startswith("docs/") or low.endswith((".md",)):
        return 8, "documentation / evidence", gen, zh
    if low.startswith("scripts/"):
        if low == "scripts/native-runtime/modern_extensions.py":
            return 4, "generator of runtime data (Generals modern-extension INI/WND overrides; run by the stager)", False, False
        return 8, "development / staging / validation / packaging script", False, False
    if low.startswith("tests/"):
        return 8, "test code", False, False
    if low.endswith(("cmakelists.txt", ".cmake", "cmakepresets.json", ".gitignore")) or "/cmake/" in low:
        return 8, "build configuration", False, False
    if low.endswith(CODE):
        if "/tools/" in low or low.startswith("core/tools/"):
            return 8, "tool source (not part of the player executables)", False, False
        if gen or zh:
            return 1, "compiled into the player executable", gen, zh
        return 9, "source not linked into either Win64 player executable (Win32-only, header-only user absent, or dead)", False, False
    return 8, "other", False, False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="adac468d7")
    ap.add_argument("--compiled", default="build/audit-tmp/compiled.json")
    ap.add_argument("--csv", default="build/audit-tmp/changed-files.csv")
    ap.add_argument("--summary", default="build/audit-tmp/summary.json")
    args = ap.parse_args()
    raw = json.load(open(args.compiled))
    compiled = {t: {"files": set(v["sources"]) | set(v["headers"])} for t, v in raw.items()}
    status = run("git", "diff", "--name-status", f"{args.base}..HEAD").strip().split("\n")
    commits = run("git", "log", "--format=%H", f"{args.base}..HEAD").strip().split("\n")
    # per file: how many commits touched it
    touched = Counter()
    for line in run("git", "log", "--name-only", "--format=", f"{args.base}..HEAD").split("\n"):
        if line.strip():
            touched[line.strip()] += 1
    rows = []
    for line in status:
        parts = line.split("\t")
        state, path = parts[0], parts[-1]
        cat, note, gen, zh = classify(path, compiled)
        rows.append({"path": path, "state": state, "commits": touched[path], "category": cat, "meaning": note, "in_generals_exe": int(gen), "in_zerohour_exe": int(zh)})
    with open(args.csv, "w", newline="", encoding="utf-8") as handle:
        w = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    by = defaultdict(lambda: {"files": 0, "generals": 0, "zerohour": 0})
    for r in rows:
        k = (r["category"], r["meaning"])
        by[k]["files"] += 1
        by[k]["generals"] += r["in_generals_exe"]
        by[k]["zerohour"] += r["in_zerohour_exe"]
    summary = {"base": args.base, "commits": len(commits), "files": len(rows),
               "categories": [{"category": k[0], "meaning": k[1], **v} for k, v in sorted(by.items())]}
    json.dump(summary, open(args.summary, "w"), indent=1)
    print(json.dumps(summary, indent=1))
    unlinked = [r["path"] for r in rows if r["category"] == 9]
    print("category 9 files:", len(unlinked))
    for p in unlinked:
        print("  ", p)


if __name__ == "__main__":
    main()
