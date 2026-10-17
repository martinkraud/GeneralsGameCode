"""Stage4B.1 reference proof - redesigned in X64.41 as a function-level contract.

Stage4B.1 (docs/modernization/STAGE4B1_PATH_POLICY_HOIST.md) made ONE authoritative pathfinding change: examineNeighboringCells copies
locomotorSet.getValidSurfaces() into the stack-local ExamineCellsStruct once, and the line callback reads the copy instead of the LocomotorSet pointer.
The guard used to prove "nothing else changed" by comparing the whole of AIPathfind.cpp, token for token, with a 2026-10-09 snapshot. That model stopped
holding with the first later (authorized, non-behavioural) change - the X64.35 profiler scopes - and has been red since; it could neither tell authorized
from unauthorized changes nor keep protecting the rest of the file. It is replaced here by a contract that is just as strict about everything not listed:

  1. every definition in AIPathfind.cpp must equal the Stage4B.1 snapshot after measurement code and braces are removed (pathfinder_function_guard.py),
     EXCEPT the definitions in the tables below; the tables name the milestone, the reason and the oracle that protects each authorized change;
  2. definitions added or removed since the snapshot must be listed, and a listed entry that no longer differs is reported as stale (so the table cannot grow silently);
  3. the Stage4B.1 hoist itself is asserted directly (struct member, one setup assignment, two callback reads, no LocomotorSet pointer);
  4. the locomotor getter / mutator files are still the reference (unchanged from the original test);
  5. the guard is mutation-tested on every run: a behavioural edit of a protected function, an unlisted new function and an unlisted removal must each be reported.
"""
from pathlib import Path
import re
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pathfinder_function_guard as guard

ROOT = Path(__file__).resolve().parents[2]
guard.ROOT = ROOT
BASE = '0ff7f9c9bf8dfe188015af36c7db8d5388756eda'
PATH = 'Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp'

# ---- authorized behaviour-bearing changes since the snapshot (name prefix -> milestone / reason / oracle) ----
CHANGED = {
    'PathfindCellInfo::getACellInfo': 'X64.40/41: initialises the open-list key / indexed flag and the line-memo fields of a recycled cell info; OpenListIndexTest, OPEN LIST + LINE verify modes',
    'applyZone': 'X64.39: templated over legacy / min-root merge primitives; ZoneEquivalenceTest, GENERALS_ZONE_VERIFY',
    'applyBlockZone': 'X64.39: templated over legacy / min-root merge primitives; ZoneEquivalenceTest, GENERALS_ZONE_VERIFY',
    'flattenZones': 'X64.39: delegates to ZoneEquivalence::legacyFlatten (verbatim legacy semantics); ZoneEquivalenceTest',
    'PathfindZoneManager::calculateZones': 'X64.39: dispatcher between the legacy and min-root implementation plus the verify mode; GENERALS_ZONE_VERIFY (388 recalculations + 16 maps)',
    'PathfindCell::removeFromOpenList': 'X64.40: maintains the open-list index; OpenListIndexTest, GENERALS_PATH_VERIFY, open-list evolution digest',
    'PathfindCell::forwardInsertionSortRetailCompatible': 'X64.40: insertion point from OpenListIndex, original walk + linking kept; test_stage4b3_reference.py, OpenListIndexTest, GENERALS_PATH_VERIFY',
    'Pathfinder::Pathfinder': 'X64.40: attaches the open-list index (GENERALS_PATH_LEGACY disables it)',
    'Pathfinder::examineNeighboringCells': 'X64.41: initialises the per-walk memo generation / modes and flushes the line counters; GENERALS_LINE_VERIFY, line decision digest',
    'Pathfinder::examineCellsCallback': 'X64.41: per-search passability memo for cells that already passed (80 percent of callbacks); GENERALS_LINE_VERIFY (72 M hits re-verified), line decision digest',
    'block:struct ExamineCellsStruct': 'X64.41: carries the memo generation / mode flags and the walk counters; the Stage4B.1 hoist is asserted separately below',
}
ADDED = {
    'PathfindCellList::reset': 'X64.40/41: restarts the open-list index and advances the search generation',
    'unionZones': 'X64.39 helper', 'PathfindZoneManager::calculateZonesImpl': 'X64.39 implementation templates', 'ZoneBlock::blockCalculateZonesImpl': 'X64.39 implementation templates',
    'ZoneBlock::blockCalculateZones': 'X64.39 dispatcher (the reference definition is reported as removed because its signature gained a flag)',
    'PathfindZoneManager::captureState': 'X64.39 verify mode', 'ZoneBlock::captureState': 'X64.39 verify mode',
    'PathfindZoneManager::verify': 'X64.39 verify statistics', 'currentZoneMode': 'X64.39 mode switch', 'block:enum ZoneMode': 'X64.39 mode switch', 'block:struct ZoneSnapshot': 'X64.39 verify mode',
    'Pathfinder::openList': 'X64.40 statistics', 'currentOpenListMode': 'X64.40 mode switch', 'openListTraceEnabled': 'X64.40 trace', 'openListTraceMix': 'X64.40 trace', 'block:enum OpenListMode': 'X64.40 mode switch',
    'Pathfinder::line': 'X64.41 statistics', 'currentLineMode': 'X64.41 mode switch', 'lineTimingEnabled': 'X64.41 attribution', 'lineTraceEnabled': 'X64.41 trace', 'lineTraceMix': 'X64.41 trace',
    'block:enum LineMode': 'X64.41 mode switch', 'block:struct ExamineLineStats': 'X64.41 attribution',
}
REMOVED = {
    'resolveBlockZones': 'X64.39: moved verbatim to ZoneEquivalence::legacyResolveBlockZones (the oracle)',
    'resolveZones': 'X64.39: moved verbatim to ZoneEquivalence::legacyResolveZones (the oracle)',
    'ZoneBlock::blockCalculateZones': 'X64.39: signature gained the legacy flag (see ADDED)',
}


def reference_source():
    return guard.git_show(BASE, PATH)


def current_source():
    return (ROOT / PATH).read_text(encoding='utf-8')


class Stage4B1Reference(unittest.TestCase):
    def test_every_definition_equals_the_snapshot_unless_authorized(self):
        unauthorized, stale = guard.compare(reference_source(), current_source(), list(CHANGED), list(ADDED), list(REMOVED))
        self.assertEqual(unauthorized, [], 'definitions changed / added / removed without an entry in the authorization tables')
        self.assertEqual(stale, [], 'authorization table entries that no longer correspond to a change - remove them')

    def test_stage4b1_hoist_is_still_in_place(self):
        cur = current_source()
        a = cur.index('struct ExamineCellsStruct')
        b = cur.index('Int Pathfinder::examineNeighboringCells(')
        callback = cur[a:b]
        self.assertEqual(callback.count('LocomotorSurfaceTypeMask acceptableSurfaces;'), 1)
        self.assertEqual(callback.count('d->acceptableSurfaces'), 2)
        self.assertNotIn('theLoco', callback)
        neighbours = cur[b:b + 6000]
        self.assertEqual(neighbours.count('info.acceptableSurfaces = locomotorSet.getValidSurfaces();'), 1)
        self.assertNotIn('theLoco', neighbours)

    def test_surface_getter_and_all_locomotor_mutators_remain_reference(self):
        for title in ('Generals', 'GeneralsMD'):
            for suffix in ('Include/GameLogic/LocomotorSet.h', 'Source/GameLogic/Object/Locomotor.cpp'):
                path = title + '/Code/GameEngine/' + suffix
                current = (ROOT / path).read_text()
                baseline = subprocess.check_output(['git', 'show', BASE + ':' + path], cwd=ROOT, text=True)
                self.assertEqual(guard.TOKEN.findall(guard.strip_comments(current)), guard.TOKEN.findall(guard.strip_comments(baseline)), path)

    # ---- the guard guards itself ----
    def _compare(self, source):
        return guard.compare(reference_source(), source, list(CHANGED), list(ADDED), list(REMOVED))[0]

    def test_mutation_behavioural_edit_of_a_protected_function_is_reported(self):
        cur = current_source()
        mutated = cur.replace('info.allyFixedCount++;', 'info.allyFixedCount += 2;', 1)
        self.assertNotEqual(mutated, cur)
        found = self._compare(mutated)
        self.assertTrue(any('checkForMovement' in x for x in found), found)

    def test_mutation_cost_constant_change_is_reported(self):
        cur = current_source()
        mutated = cur.replace('if (dx>dy) {', 'if (dx>=dy) {', 1)
        self.assertNotEqual(mutated, cur)
        self.assertTrue(any('costToGoal' in x for x in self._compare(mutated)))

    def test_mutation_unlisted_new_function_is_reported(self):
        mutated = current_source() + '\nstatic int x64_41_unauthorised_helper(int a) { return a + 1; }\n'
        self.assertTrue(any('x64_41_unauthorised_helper' in x for x in self._compare(mutated)))

    def test_mutation_removed_function_is_reported(self):
        cur = current_source()
        start = cur.index('UnsignedInt PathfindCell::costToHierGoal')
        end = cur.index('{', start)
        depth = 0
        k = end
        while True:
            if cur[k] == '{': depth += 1
            elif cur[k] == '}':
                depth -= 1
                if depth == 0: break
            k += 1
        mutated = cur[:start] + cur[k + 1:]
        self.assertTrue(any('costToHierGoal' in x for x in self._compare(mutated)))

    def test_instrumentation_only_changes_are_not_behavioural(self):
        cur = current_source()
        mutated = cur.replace('PerformanceProfile::pathCount(PerformanceProfile::PathWork::CleanedCells,count);', 'PerformanceProfile::pathCount(PerformanceProfile::PathWork::CleanedCells,count*2);PERFORMANCE_PROFILE_SCOPE(PathQueue);', 1)
        self.assertNotEqual(mutated, cur)
        self.assertEqual(self._compare(mutated), [])


    def test_x64_43_diagnostic_hooks_are_not_behavioural(self):
        cur = current_source()
        mutated = cur.replace('failDiag.pop();', 'failDiag.pop(); PathFailureDiag::noteMapMutation(); if (PathFailureDiag::enabled()) PathFailureDiag::setHierarchical(7);', 1)
        self.assertNotEqual(mutated, cur)
        self.assertEqual(self._compare(mutated), [])

    def test_mutation_behavioural_edit_next_to_x64_43_hooks_is_reported(self):
        cur = current_source()
        anchor = 'if (parentCell == goalCell)\n\t\t{\n\t\t\tfailDiag.succeeded();'
        self.assertEqual(cur.count(anchor), 1)
        mutated = cur.replace(anchor, 'if (parentCell != goalCell)\n\t\t{\n\t\t\tfailDiag.succeeded();', 1)
        self.assertTrue(any('internalFindPath' in x for x in self._compare(mutated)), self._compare(mutated))


if __name__ == '__main__':
    unittest.main()
