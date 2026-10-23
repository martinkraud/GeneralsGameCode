#!/usr/bin/env python3
"""X64.52A: builds the GENERALS_AI_DIAG_SW scenario of one Superweapons rule value for Invoke-AiBaseExperiments.ps1 -SwAid.

  make_sw_aids.py <rule 1|3|5|D|0> [--human 2] [--ai 3]

The human slot (player index 2 in the benchmark replays, an America player in both titles) is given a dozer and the Strategy Center (the Particle Cannon's prerequisite), then:
  * fills it up to one below the cap with given Particle Cannons (a finished structure, as a completed construction leaves it),
  * probes BuildAssistant::canMakeUnit (what the build button and DozerAIUpdate::construct ask) and calls DozerAIUpdate::construct (what a human's construct command ends in),
  * a construction that the rule allows must create a site, the one at the cap must be refused, however it is asked (twice in a row: the second sees the first one's site),
  * an enemy superweapon is captured (Object::setTeam): nothing is destroyed, the owner count rises above the cap and construction stays refused,
  * structures are destroyed one by one: construction is allowed again exactly when the owner count is below the cap,
  * Unlimited builds everything, Disabled refuses everything.
Prints the aid string (semicolon separated "<frame>:<verb>:<Template>@<player>[><from>]").
"""
import argparse
import sys

PC = "AmericaParticleCannonUplink"
DOZER = "AmericaVehicleDozer"
PREREQ = "AmericaStrategyCenter"
SCUD = "GLAScudStorm"


def make(rule, human, ai):
    aids = []
    frame = [300]

    def add(verb, template, player=human, source=None):
        aids.append("%d:%s:%s@%d%s" % (frame[0], verb, template, player, (">%d" % source) if source is not None else ""))
        frame[0] += 120

    add("give", DOZER)
    add("give", PREREQ)
    if rule == "0":
        for _ in range(6):
            add("give", PC)
        add("probe", PC)
        add("build", PC)          # Unlimited: a site is created although the player already owns six
        return ";".join(aids)
    if rule == "D":
        add("probe", PC)          # no structure owned and still refused
        add("build", PC)
        return ";".join(aids)
    cap = int(rule)
    for _ in range(cap - 1):
        add("give", PC)
    add("probe", PC)              # below the cap: allowed
    add("build", PC)              # allowed: a site is created (owned == cap)
    add("probe", PC)              # at the cap: refused
    add("build", PC)              # refused
    add("give", SCUD, ai)         # an enemy superweapon ...
    add("capture", SCUD, human, ai)   # ... is captured: owned == cap + 1
    add("probe", PC)              # still refused, nothing was destroyed
    add("kill", PC)               # owned == cap
    add("probe", PC)              # at the cap: refused
    add("kill", SCUD if cap == 1 else PC)   # owned == cap - 1
    add("probe", PC)              # below the cap: allowed again
    add("build", PC)              # a site is created again
    return ";".join(aids)


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("rule", choices=["0", "1", "3", "5", "D"])
    parser.add_argument("--human", type=int, default=2)
    parser.add_argument("--ai", type=int, default=3)
    args = parser.parse_args(argv)
    print(make(args.rule, args.human, args.ai))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
