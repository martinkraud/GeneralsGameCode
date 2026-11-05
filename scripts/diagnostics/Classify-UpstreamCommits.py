#!/usr/bin/env python3
"""Classify upstream commits against the working tree with `git apply --check` (read-only: nothing is modified).

    python scripts/diagnostics/Classify-UpstreamCommits.py <commit> [<commit> ...]

For each commit the code part of its patch (tools, tests, docs and CI paths excluded) is checked in both directions:
  APPLIES      forward check succeeds      -> the change is not in the tree yet (the defect, if it is one, is still present; "applicable as-is" at file level)
  IN-TREE      reverse check succeeds      -> the change is already in the tree
  DIVERGED     neither applies             -> the touched code differs here: manual adaptation or obsolete (our architecture replaced it)
  NOCODE       nothing left after the exclusions
The classification is mechanical; whether a change is *wanted* (gameplay / determinism / save-format effects) is a separate, human decision.
"""
import subprocess
import sys

EXCLUDE = [":(exclude)*Tools*", ":(exclude)*.md", ":(exclude).github", ":(exclude)*Test*", ":(exclude)*test*", ":(exclude)*.yml", ":(exclude)Benchmarks*"]


def run(args, data=None):
    return subprocess.run(args, input=data, capture_output=True)


def classify(commit):
    patch = run(["git", "show", commit, "--format=", "--", "."] + EXCLUDE).stdout
    subject = run(["git", "log", "-1", "--format=%s", commit]).stdout.decode(errors="replace").strip()
    if not patch.strip():
        return "NOCODE", subject
    if run(["git", "apply", "--check", "-"], patch).returncode == 0:
        return "APPLIES", subject
    if run(["git", "apply", "--reverse", "--check", "-"], patch).returncode == 0:
        return "IN-TREE", subject
    return "DIVERGED", subject


def main():
    for commit in sys.argv[1:]:
        kind, subject = classify(commit)
        print("%-9s %s %s" % (kind, commit[:9], subject[:100]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
