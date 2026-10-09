"""Stage4B.1 reference proof: exact inverse of the sole authoritative transformation.

This guards all decisions, FP expressions, callbacks, list/pool/queue work and
profiler probes, rather than implementing a second toy pathfinder. It is not a
runtime replay or full-map path-result test. Run from any directory in this repo.
"""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASE = '0ff7f9c9bf8dfe188015af36c7db8d5388756eda'
PATH = 'Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp'


def reference(path):
    return subprocess.check_output(['git', 'show', BASE + ':' + path], cwd=ROOT, text=True)


def tokens(source):
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|\w+|[^\s]'
    return [m[0] for m in re.finditer(pattern, source) if not m[0].startswith(('//', '/*'))]


class Stage4B1Reference(unittest.TestCase):
    def test_exact_inverse_preserves_entire_pathfinder_token_order(self):
        current = (ROOT / PATH).read_text()
        start = current.index('struct ExamineCellsStruct')
        end = current.index('Int cellCount = 0;', start)
        changed = current[start:end]
        self.assertEqual(changed.count('LocomotorSurfaceTypeMask acceptableSurfaces;'), 1)
        self.assertEqual(changed.count('d->acceptableSurfaces'), 2)
        self.assertEqual(changed.count('info.acceptableSurfaces = locomotorSet.getValidSurfaces();'), 1)
        restored = changed.replace('LocomotorSurfaceTypeMask acceptableSurfaces;', 'const LocomotorSet *theLoco;')
        restored = restored.replace('d->acceptableSurfaces', 'd->theLoco->getValidSurfaces()')
        restored = restored.replace('info.acceptableSurfaces = locomotorSet.getValidSurfaces();', 'info.theLoco = &locomotorSet;')
        restored = current[:start] + restored + current[end:]
        self.assertEqual(tokens(restored), tokens(reference(PATH)))

    def test_surface_getter_and_all_locomotor_mutators_remain_reference(self):
        for title in ('Generals', 'GeneralsMD'):
            for suffix in ('Include/GameLogic/LocomotorSet.h', 'Source/GameLogic/Object/Locomotor.cpp'):
                path = title + '/Code/GameEngine/' + suffix
                current = (ROOT / path).read_text()
                self.assertEqual(tokens(current), tokens(reference(path)))
                if suffix.endswith('.h'):
                    self.assertIn('getValidSurfaces() const { return m_validLocomotorSurfaces; }', current)
        # The only line-walk callback context construction must snapshot once,
        # before the unchanged synchronous walker call.
        current = (ROOT / PATH).read_text()
        start = current.index('Int Pathfinder::examineNeighboringCells(')
        end = current.index('Int cellCount = 0;', start)
        setup = current[start:end]
        self.assertLess(setup.index('info.acceptableSurfaces = locomotorSet.getValidSurfaces();'),
                        setup.index('iterateCellsAlongLine(start, end,'))


if __name__ == '__main__':
    unittest.main()
