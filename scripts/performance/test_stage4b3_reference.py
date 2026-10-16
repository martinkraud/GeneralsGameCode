"""Frozen retail reference and test-only candidate are audited against real source.

X64.40 deliberately changed how the retail open list finds its insertion point (OpenListIndex, exact by construction, differential-tested). The guard keeps its
original purpose - the retail algorithm is protected - in these ways:
  * the frozen reference (RetailOpenListReference.inc) must still equal the accepted baseline body, token for token;
  * the production function must still contain the baseline's dangling-pointer repair and its linking block VERBATIM, and must obtain the insertion point from
    openListInsertionPredecessor, whose fallback walk must still be the baseline's loop condition (head first, <= cost, capped at PATHFIND_CELLS_PER_FRAME);
  * the differential tests that compare the index with the verbatim walk must still exist;
  * the representation may differ from the baseline ONLY by the exact, listed X64.40 additions - anything else still fails.
"""
from pathlib import Path
import re, subprocess, unittest
ROOT=Path(__file__).resolve().parents[2]
BASE='0ff7f9c9bf8dfe188015af36c7db8d5388756eda'
SOURCE='Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp'
TEST=ROOT/'Tests/Google/Core/GameEngine/Common'
INDEX_HEADER='Core/GameEngine/Include/GameLogic/Pathfinder/OpenListIndex.h'
def body(s):
    a=s.index('void PathfindCell::forwardInsertionSortRetailCompatible');a=s.index('{',a)
    return s[a:s.index('\n}\n#endif',a)+2]
def tokens(s):
    return re.findall(r'\w+|[^\s]',re.sub(r'/\*[\s\S]*?\*/|//[^\n]*','',s))
def norm(s):
    return ' '.join(tokens(s))
def contains(haystack, needle):
    return norm(needle) in norm(haystack)
# the exact X64.40 additions to the representation: (new text, text it replaces in the baseline)
ALLOWED={
    'PathfindCell.h':[('friend class PathfindCellList;',''),
                      ('inline Bool hasLineVerdict(UnsignedInt generation, Int layer) const { return m_info && m_info->m_lineGeneration == generation && m_info->m_lineLayer == layer; } inline void setLineVerdict(UnsignedInt generation, Int layer) { if (m_info) { m_info->m_lineGeneration = generation; m_info->m_lineLayer = static_cast<UnsignedByte>(layer); } }','')],
    'PathfindCellInfo.h':[('friend class PathfindCellList;',''),('UnsignedShort m_openKey;',''),('UnsignedInt m_indexed:1;',''),('UnsignedInt m_lineGeneration;',''),('UnsignedByte m_lineLayer;','')],
    'PathfindCellList.h':[('#include "GameLogic/Pathfinder/OpenListIndex.h"',''),
                          ('PathfindCellList() : m_head(nullptr), m_tail(nullptr), m_index(nullptr), m_generation(1) {}','PathfindCellList() : m_head(nullptr), m_tail(nullptr) {}'),
                          ('void reset(PathfindCell* newHead = nullptr); void attachIndex(OpenListIndex<PathfindCell>* index) { m_index = index; }','void reset(PathfindCell* newHead = nullptr) { m_head = newHead; m_tail = nullptr; }'),
                          ('void reset() { m_head = nullptr; m_tail = nullptr; ++m_generation; }','void reset() { m_head = nullptr; m_tail = nullptr; }'),
                          ('UnsignedInt searchGeneration() const { return m_generation; }',''),
                          ('UnsignedInt m_generation;',''),
                          ('OpenListIndex<PathfindCell>* m_index;','')],
}
class RetailOpenListReference(unittest.TestCase):
    def test_frozen_body_is_exact_accepted_production(self):
        baseline=subprocess.check_output(['git','show',BASE+':'+SOURCE],cwd=ROOT,text=True)
        current=(ROOT/SOURCE).read_text()
        frozen=(TEST/'RetailOpenListReference.inc').read_text().replace('self','this')
        # the frozen reference is still exactly the accepted baseline
        self.assertEqual(tokens(frozen),tokens(body(baseline)))
        base_body=body(baseline); cur_body=body(current)
        # the baseline's linking block and dangling-pointer repair survive verbatim in production
        linking=base_body[base_body.index('if (currentCell)\n\t{\n\t\t// insert just before'):base_body.rindex('}')]
        self.assertTrue(contains(cur_body,linking.replace('previousCell->m_info->m_nextOpen = this->m_info;','previousCell->m_info->m_nextOpen = this->m_info;')))
        repair=base_body[base_body.index('if (currentCell->m_info->m_nextOpen &&'):base_body.index('cellCount++;')]
        repair=repair.replace('currentCell->','c->').rstrip().rstrip('}')   # production adds `return true;` after the two assignments, inside the same braces
        self.assertTrue(contains(current,repair))
        # production takes the insertion point from the shared decision, whose fallback is the baseline loop condition
        self.assertIn('openListInsertionPredecessor',cur_body)
        header=(ROOT/INDEX_HEADER).read_text()
        self.assertTrue(contains(header,'while (current && count < cap && access.cost(current) <= cost)'))
        # the retail cap is still the per-frame cell budget constant
        self.assertIn('PATHFIND_CELLS_PER_FRAME',cur_body)
        # and the differential oracle against the verbatim walk exists
        oracle=(TEST/'OpenListIndexTest.cpp').read_text()
        for name in ('DifferentialRandomClusteredAndUniformCosts','DifferentialManyTiesAndWideRange','DifferentialWithTheCapReachedManyTimes','DifferentialLongListsAtTheRealCap'):
            self.assertIn(name,oracle)
    def test_two_step_candidate_only_duplicates_original_loop_iteration(self):
        s=(TEST/'RetailOpenListReference.inc').read_text();s=s[s.index('{'):]
        a=s.index('\twhile (');b=s.index('\n\t}\n\tPerformanceProfile::pathCount',a)+len('\n\t}')
        condition=s[s.index('(',a)+1:s.index('\n\t{',a)-1]
        inner=s[s.index('\n\t{',a)+len('\n\t{'):b-len('\n\t}')]
        block='\twhile ('+condition+')\n\t{'+inner+'\n\t\tif (!('+condition+')) break;'+inner+'\n\t}'
        expected=s[:a]+block+s[b:]
        self.assertEqual(tokens((TEST/'RetailOpenListUnrolledCandidate.inc').read_text()),tokens(expected))
    def test_access_declarations_do_not_change_representation(self):
        for name in ('PathfindCell.h','PathfindCellInfo.h','PathfindCellList.h'):
            path='Core/GameEngine/Include/GameLogic/Pathfinder/'+name
            s=(ROOT/path).read_text()
            self.assertEqual(s.count('friend struct RetailOpenListTestAccess;'),1)
            restored=norm(s.replace('friend struct RetailOpenListTestAccess;',''))
            for new,old in ALLOWED[name]:
                self.assertIn(norm(new),restored,name+': expected X64.40 addition missing: '+new)
                restored=' '.join(restored.replace(norm(new),norm(old),1).split())
            baseline=subprocess.check_output(['git','show',BASE+':'+path],cwd=ROOT,text=True)
            self.assertEqual(restored,norm(baseline),name+': the representation differs from the baseline by more than the listed X64.40 additions')
if __name__=='__main__':unittest.main()
