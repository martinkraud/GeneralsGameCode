"""Pin fixed shader inputs and preserve the surrounding water/capture routes."""
import ast
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASE = "68956fd653fb3d03a4f371b16ba61890421081cf"
CLIENT = "Core/GameEngineDevice/Source/W3DDevice/GameClient/"
HEADER = "Core/Libraries/Source/WWVegas/WW3D2/LegacyFixedPixelShader.h"


def original(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode().replace("\r\n", "\n")


def current(path):
    return (ROOT / path).read_text()


def literal(text):
    text = text.replace("\\\n", "")
    return "".join(ast.literal_eval(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', text, re.S))


class FixedShaderReference(unittest.TestCase):
    def test_all_four_sources_are_the_exact_original_production_bytes(self):
        water = original(CLIENT + "Water/W3DWater.cpp")
        profiler = original(CLIENT + "W3DProfilerFrameCapture.cpp")
        sources = re.findall(r'(?:const char \*shader|\bshader) =\s*(".*?");\s*hr = D3DXAssembleShader', water, re.S)
        sources += re.findall(r'const char \*shader =\s*(".*?");\s*HRESULT hr = D3DXAssembleShader', profiler, re.S)
        self.assertEqual(len(sources), 4)
        header = current(HEADER)
        for name, old in zip(("RiverWater", "Water", "TrapezoidWater", "ProfilerSwizzle"), sources):
            # Shader comments contain semicolons: delimit by the next static_assert.
            new = re.search(rf'char {name}Source\[\] =\s*(.*?)\s*static_assert', header, re.S).group(1)
            self.assertEqual(literal(new), literal(old), name)

    def test_water_outside_the_fixed_assembly_block_is_unchanged(self):
        path = CLIENT + "Water/W3DWater.cpp"
        old, new = original(path), current(path)
        new = new.replace('#include "WW3D2/LegacyFixedPixelShader.h"\n', "")
        a, b = old.index("\t\tID3DXBuffer *compiledShader;"), new.index("\t\thr = DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(\n")
        old_end, new_end = old.index("\n\t}", a), new.index("\n\t}", b)
        self.assertEqual(old[:a] + old[old_end:], new[:b] + new[new_end:])
        self.assertEqual(re.findall(r'FixedPixelShader::(\w+)Code, &(\w+)', new[b:new_end]),
                         [("RiverWater", "m_riverWaterPixelShader"), ("Water", "m_waterPixelShader"),
                          ("TrapezoidWater", "m_trapezoidWaterPixelShader")])

    def test_profiler_gate_device_failure_and_cleanup_are_preserved(self):
        path = CLIENT + "W3DProfilerFrameCapture.cpp"
        old, new = original(path), current(path)
        new = new.replace('#include "WW3D2/LegacyFixedPixelShader.h"', '#include <d3dx8core.h>')
        a, b = old.index("\t\tID3DXBuffer *compiledShader"), new.index("\t\tHRESULT hr = DX8Wrapper")
        end = "\n\t\tif (FAILED(hr))"
        old_end = old.index(end, old.index("compiledShader->Release();", a))
        new_end = new.index(end, b)
        self.assertEqual(old[:a] + old[old_end:], new[:b] + new[new_end:])
        self.assertIn("FixedPixelShader::ProfilerSwizzleCode, &m_swizzleShader", new[b:new_end])
        header = current(HEADER)
        self.assertGreater(header.index("ProfilerSwizzleSource"), header.index("#ifdef PROFILER_ENABLED"))
        self.assertLess(header.index("ProfilerSwizzleCode"), header.index("#endif // PROFILER_ENABLED"))


if __name__ == "__main__":
    unittest.main()
