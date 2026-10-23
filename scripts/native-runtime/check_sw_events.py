#!/usr/bin/env python3
"""X64.52A: checks the sw.* events of a GENERALS_AI_DIAG_SW run against the rule's semantics.

  check_sw_events.py <rule 1|3|5|D|0> <run>-diag_ai.txt

Every probe must say MAXED_OUT_FOR_PLAYER exactly when the rule forbids another structure for the owner count it printed; every construct call must create a site exactly when
it was allowed; a capture must raise the count by one (nothing destroyed); every kill must find its target. Exit code 0 = all checks pass; prints one line per checked event.
"""
import re
import sys


def blocks(rule, owned):
    if rule == "0":
        return False
    if rule == "D":
        return True
    return owned >= int(rule)


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    rule, path = argv[1], argv[2]
    failures = checks = 0
    last_probe = None
    last_owned = None
    for line in open(path):
        m = re.match(r"event frame=(\d+) sw\.probe player=(\d+) template=(\S+) owned=(\d+) limit=(\d+) canMakeUnit=(\d+) \((\w+)\)", line)
        if m:
            frame, _, template, owned, limit, code, name = m.groups()
            owned = int(owned)
            expect_blocked = blocks(rule, owned)
            ok = (name == "MAXED_OUT_FOR_PLAYER") == expect_blocked
            checks += 1
            failures += not ok
            print("%s frame %-5s probe owned=%d -> %-22s expected %s" % ("ok  " if ok else "FAIL", frame, owned, name, "MAXED" if expect_blocked else "not MAXED"))
            last_probe = (frame, owned, expect_blocked)
            last_owned = owned
            continue
        m = re.match(r"event frame=(\d+) sw\.build player=(\d+) template=(\S+) site_created=(\d) owned_after=(\d+)", line)
        if m:
            frame, _, template, created, owned_after = m.groups()
            created, owned_after = int(created), int(owned_after)
            if last_probe and last_probe[0] == frame:
                expect_created = not last_probe[2]
                ok = bool(created) == expect_created and owned_after == last_probe[1] + (1 if created else 0)
                checks += 1
                failures += not ok
                print("%s frame %-5s construct (owned before %d) -> site_created=%d owned_after=%d expected site_created=%d" %
                      ("ok  " if ok else "FAIL", frame, last_probe[1], created, owned_after, int(expect_created)))
                last_owned = owned_after
            continue
        m = re.match(r"event frame=(\d+) sw\.capture player=(\d+) template=(\S+) from=(\d+) captured=(\d) owned_after=(\d+)", line)
        if m:
            frame, _, template, source, captured, owned_after = m.groups()
            ok = captured == "1" and last_owned is not None and int(owned_after) == last_owned + 1
            checks += 1
            failures += not ok
            print("%s frame %-5s capture -> captured=%s owned_after=%s (before %s): nothing destroyed, count +1" % ("ok  " if ok else "FAIL", frame, captured, owned_after, last_owned))
            last_owned = int(owned_after)
            continue
        m = re.match(r"event frame=(\d+) sw\.kill player=(\d+) template=(\S+) killed=(\d)", line)
        if m:
            frame, _, template, killed = m.groups()
            ok = killed == "1"
            checks += 1
            failures += not ok
            print("%s frame %-5s kill %s -> killed=%s" % ("ok  " if ok else "FAIL", frame, template, killed))
    print("%d checks, %d failures" % (checks, failures))
    return 1 if failures or checks == 0 else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
