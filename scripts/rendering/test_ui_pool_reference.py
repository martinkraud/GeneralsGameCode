from pathlib import Path
import re,subprocess,unittest
ROOT=Path(__file__).resolve().parents[2]
BASE='4ddc41d7b856673f4c865b37da68678e178bf63c'
class UIPoolContract(unittest.TestCase):
    def test_all_existing_pool_entries_unchanged(self):
        for title in ('Generals','GeneralsMD'):
            p=f'Core/GameEngine/Source/Common/System/GameMemoryInitPools_{title}.inl'
            old=subprocess.check_output(['git','show',f'{BASE}:{p}'],cwd=ROOT)
            new=(ROOT/p).read_bytes()
            lines=[l for l in new.splitlines(keepends=True) if b'"Texture2DCPUContent"' not in l]
            self.assertEqual(b''.join(lines).replace(b'\r\n',b'\n'),old.replace(b'\r\n',b'\n'))
    def test_cpu_content_named_pool_registered_for_both_titles(self):
        header=(ROOT/'Core/Libraries/Source/WWVegas/WW3D2/Texture2DCPUContent.h').read_text()
        names=re.findall(r'W3DMPO_CODE\((\w+)\)',header)
        self.assertTrue(names)
        for title in ('Generals','GeneralsMD'):
            source=(ROOT/f'Core/GameEngine/Source/Common/System/GameMemoryInitPools_{title}.inl').read_text()
            for name in names:
                entries=re.findall(r'\{\s*"'+name+r'",\s*(\d+),\s*(\d+)\s*\}',source)
                self.assertEqual(len(entries),1)
                self.assertGreater(int(entries[0][0]),0)
                self.assertGreater(int(entries[0][1]),0)
if __name__=='__main__':unittest.main()
