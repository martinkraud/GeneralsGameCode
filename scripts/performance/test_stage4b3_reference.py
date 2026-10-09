"""Frozen retail reference and test-only candidate are audited against real source."""
from pathlib import Path
import re, subprocess, unittest
ROOT=Path(__file__).resolve().parents[2]
BASE='0ff7f9c9bf8dfe188015af36c7db8d5388756eda'
SOURCE='Core/GameEngine/Source/GameLogic/AI/AIPathfind.cpp'
TEST=ROOT/'Tests/Google/Core/GameEngine/Common'
def body(s):
    a=s.index('void PathfindCell::forwardInsertionSortRetailCompatible');a=s.index('{',a)
    return s[a:s.index('\n}\n#endif',a)+2]
def tokens(s):
    return re.findall(r'\w+|[^\s]',re.sub(r'/\*[\s\S]*?\*/|//[^\n]*','',s))
class RetailOpenListReference(unittest.TestCase):
    def test_frozen_body_is_exact_accepted_production(self):
        baseline=subprocess.check_output(['git','show',BASE+':'+SOURCE],cwd=ROOT,text=True)
        current=(ROOT/SOURCE).read_text()
        frozen=(TEST/'RetailOpenListReference.inc').read_text().replace('self','this')
        self.assertEqual(tokens(frozen),tokens(body(baseline)))
        self.assertEqual(tokens(body(current)),tokens(body(baseline)))
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
            restored=s.replace('friend struct RetailOpenListTestAccess;','')
            baseline=subprocess.check_output(['git','show',BASE+':'+path],cwd=ROOT,text=True)
            self.assertEqual(tokens(restored),tokens(baseline))
if __name__=='__main__':unittest.main()
