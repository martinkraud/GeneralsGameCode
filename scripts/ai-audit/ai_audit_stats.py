#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""X64.50 AI archaeology: aggregate statistics of the retail skirmish AI data, read (never written) from the user's own Steam installation.

  python scripts/ai-audit/ai_audit_stats.py [--zh "<Zero Hour folder>"] [--out docs/modernization/evidence/x64-50/ai-statistics.json]

What it reads (read-only):
  Data/INI/Default/AIData.ini        (INIZH.big / PatchINI.big for Zero Hour, ZH_Generals/INI.big for Generals): skirmish build lists + AI constants
  Data/Scripts/SkirmishScripts.scb   (loose file): skirmish AI scripts and team templates of both titles
Only AGGREGATES are written (counts, sums, histograms): the retail scripts and team data themselves are EA content and are not copied into the repository.
"""
import argparse
import collections
import json
import os
import re
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scb  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
STEAM_ZH = r'C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour'


def find_in_bigs(bigs, wanted):
    found = None
    for big in bigs:
        if not os.path.exists(big):
            continue
        for name, data in scb.read_big(big):
            if name.replace('\\', '/').lower() == wanted.lower():
                found = data   # later archives override earlier ones
    return found


def build_lists(text):
    out = {}
    side = None
    cur = None
    entries = []

    def flush():
        nonlocal entries, side
        if side is None:
            return
        auto = collections.Counter()
        rebuilds = collections.Counter()
        names = collections.Counter()
        auto_names = collections.Counter()
        for name, d in entries:
            a = d.get('AutomaticallyBuild', '?').split()[0].lower()
            auto[a] += 1
            rebuilds[d.get('Rebuilds', '?')] += 1
            names[name] += 1
            if a == 'yes':
                auto_names[name] += 1
        out[side] = {'entries': len(entries), 'distinct_templates': len(names), 'automatic': dict(auto), 'rebuilds': dict(rebuilds), 'automatic_templates': dict(auto_names),
                     'template_counts': dict(names)}
        entries = []

    for line in text.replace('\r', '').split('\n'):
        s = line.split(';')[0].strip()
        m = re.match(r'SkirmishBuildList\s+(\S+)', s)
        if m:
            flush()
            side = m.group(1)
            continue
        if side is None:
            continue
        m = re.match(r'Structure\s+(\S+)', s, re.I)
        if m:
            cur = (m.group(1), {})
            continue
        if cur is not None:
            if s.upper() == 'END':
                entries.append(cur)
                cur = None
                continue
            kv = re.match(r'(\w+)\s*=\s*(.*)', s)
            if kv:
                cur[1][kv.group(1)] = kv.group(2).strip()
        elif s.upper() == 'END':
            flush()
            side = None
    flush()
    return out


def ai_constants(text):
    wanted = ['StructureSeconds', 'TeamSeconds', 'Wealthy', 'Poor', 'StructuresWealthyRate', 'StructuresPoorRate', 'TeamsWealthyRate', 'TeamsPoorRate', 'TeamResourcesToStart',
              'MaxRecruitRadius', 'SkirmishBaseDefenseExtraDistance', 'RotateSkirmishBases', 'ForceSkirmishAI', 'SupplyCenterSafeRadius', 'RebuildDelayTimeSeconds',
              'AttackUsesLineOfSight', 'AttackIgnoreInsignificantBuildings', 'MinInfantryForGroup', 'MinVehiclesForGroup', 'MinDistanceForGroup', 'DistanceRequiresGroup',
              'GuardInnerModifierAI', 'GuardOuterModifierAI', 'GuardInnerModifierHuman', 'GuardOuterModifierHuman', 'AIDozerBoredRadiusModifier']
    out = {}
    for line in text.replace('\r', '').split('\n'):
        s = line.split(';')[0].strip()
        m = re.match(r'(\w+)\s*=\s*(\S+)', s)
        if m and m.group(1) in wanted and m.group(1) not in out:
            out[m.group(1)] = m.group(2)
    gather = {}
    side = None
    for line in text.replace('\r', '').split('\n'):
        s = line.split(';')[0].strip()
        m = re.match(r'SideInfo\s+(\S+)', s)
        if m:
            side = m.group(1)
        m = re.match(r'ResourceGatherers(Easy|Normal|Hard)\s*=\s*(\d+)', s)
        if m and side:
            gather.setdefault(side, {})[m.group(1)] = int(m.group(2))
    out['ResourceGatherers'] = gather
    return out


def script_statistics(sc, header_text):
    actions_names = scb.enum_names(header_text, 'ScriptActionType')
    cond_names = scb.enum_names(header_text, 'ConditionType')
    lists = sc.script_lists(actions_names, cond_names)
    players = sc.player_names()
    stats = {'script_lists': len(lists), 'players': [p.decode() for p in players]}
    total_actions = collections.Counter()
    total_conditions = collections.Counter()
    per_list = []
    for idx, lst in enumerate(lists):
        scripts = [s for g in lst['groups'] for s in g['scripts']] + lst['scripts']
        give = collections.defaultdict(int)
        give_count = collections.defaultdict(int)
        escal = collections.Counter()
        dynamic = collections.Counter()
        for s in scripts:
            diff = ('E' if s['easy'] else '-') + ('N' if s['normal'] else '-') + ('H' if s['hard'] else '-')
            for name, parms in s['actions'] + s['else']:
                total_actions[name] += 1
                if name == 'PLAYER_GIVE_MONEY':
                    amount = next((int(p.split(':')[1]) for p in parms if p.startswith('INT:')), 0)
                    give[diff] += amount
                    give_count[diff] += 1
                if name in ('SKIRMISH_BUILD_BUILDING', 'SKIRMISH_BUILD_STRUCTURE_FRONT', 'SKIRMISH_BUILD_STRUCTURE_FLANK', 'SKIRMISH_BUILD_BASE_DEFENSE_FRONT',
                            'SKIRMISH_BUILD_BASE_DEFENSE_FLANK', 'AI_PLAYER_BUILD_SUPPLY_CENTER', 'AI_PLAYER_BUILD_TYPE_NEAREST_TEAM', 'AI_PLAYER_BUILD_UPGRADE'):
                    dynamic[name] += 1
                if name == 'INCREMENT_COUNTER' and any(p == 'COUNTER:_ESCALATION' for p in parms):
                    amount = next((int(p.split(':')[1]) for p in parms if p.startswith('INT:')), 0)
                    escal[diff + ':+%d' % amount] += 1
            for orc in s['or']:
                for name, parms in orc:
                    total_conditions[name] += 1
        per_list.append({'index': idx, 'player': players[idx - 1].decode() if 0 < idx <= len(players) else None, 'groups': len(lst['groups']), 'scripts': len(scripts),
                         'by_difficulty': dict(collections.Counter(('E' if s['easy'] else '-') + ('N' if s['normal'] else '-') + ('H' if s['hard'] else '-') for s in scripts)),
                         'give_money_total_by_difficulty': dict(give), 'give_money_actions_by_difficulty': dict(give_count), 'escalation_increments': dict(escal),
                         'placement_and_build_actions': dict(dynamic)})
    stats['lists'] = per_list
    stats['actions'] = dict(total_actions.most_common())
    stats['conditions'] = dict(total_conditions.most_common())
    stats['scripts_total'] = sum(p['scripts'] for p in per_list)
    return stats


def team_statistics(sc):
    ts = sc.teams()
    out = {'templates': len(ts), 'per_owner': dict(collections.Counter(t.get('teamOwner', '') for t in ts))}
    prios = collections.Counter(t.get('teamProductionPriority') for t in ts if 'teamProductionPriority' in t)
    out['production_priority_histogram'] = {str(k): v for k, v in sorted(prios.items())}
    out['max_instances_histogram'] = {str(k): v for k, v in sorted(collections.Counter(t.get('teamMaxInstances') for t in ts if 'teamMaxInstances' in t).items())}
    sizes = [sum(t.get('teamUnitMaxCount%d' % i, 0) for i in range(1, 8)) for t in ts if 'teamUnitMaxCount1' in t]
    out['team_size_by_max_counts'] = {'mean': round(statistics.mean(sizes), 2), 'median': statistics.median(sizes), 'max': max(sizes)} if sizes else {}
    out['priority_success_increase_nonzero'] = sum(1 for t in ts if t.get('teamProductionPrioritySuccessIncrease', 0))
    out['priority_failure_decrease_nonzero'] = sum(1 for t in ts if t.get('teamProductionPriorityFailureDecrease', 0))
    out['auto_reinforce'] = sum(1 for t in ts if t.get('teamAutoReinforce'))
    out['attack_common_target'] = sum(1 for t in ts if t.get('teamAttackCommonTarget'))
    out['ai_recruitable'] = sum(1 for t in ts if t.get('teamIsAIRecruitable'))
    out['with_production_condition'] = sum(1 for t in ts if t.get('teamProductionCondition'))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--zh', default=STEAM_ZH, help='Zero Hour installation folder (read-only); its ZH_Generals sub-folder holds the Generals base game')
    ap.add_argument('--out', default=os.path.join(ROOT, 'docs', 'modernization', 'evidence', 'x64-50', 'ai-statistics.json'))
    args = ap.parse_args()
    gen_dir = os.path.join(args.zh, 'ZH_Generals')
    result = {'note': 'Aggregates only. Source: the installation given by --zh (read-only).'}
    for title, folder, bigs, header in (
            ('Generals', gen_dir, [os.path.join(gen_dir, 'INI.big')], os.path.join(ROOT, 'Generals', 'Code', 'GameEngine', 'Include', 'GameLogic', 'Scripts.h')),
            ('ZeroHour', args.zh, [os.path.join(args.zh, 'INIZH.big'), os.path.join(args.zh, 'PatchINI.big')],
             os.path.join(ROOT, 'GeneralsMD', 'Code', 'GameEngine', 'Include', 'GameLogic', 'Scripts.h'))):
        entry = {}
        ini = find_in_bigs(bigs, 'Data/INI/Default/AIData.ini')
        if ini is not None:
            text = ini.decode('latin1')
            entry['aidata_constants'] = ai_constants(text)
            entry['skirmish_build_lists'] = {k: {kk: vv for kk, vv in v.items() if kk != 'template_counts'} for k, v in build_lists(text).items()}
        path = os.path.join(folder, 'Data', 'Scripts', 'SkirmishScripts.scb')
        if os.path.exists(path):
            sc = scb.Chunked(open(path, 'rb').read())
            header_text = open(header, encoding='latin1').read()
            entry['scripts'] = script_statistics(sc, header_text)
            entry['teams'] = team_statistics(sc)
        result[title] = entry
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, 'w', encoding='utf-8') as f:
        json.dump(result, f, indent=1)
    print('wrote', args.out)


if __name__ == '__main__':
    main()
