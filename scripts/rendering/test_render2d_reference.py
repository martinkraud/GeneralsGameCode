"""Static DX8 mapping checks against the accepted, pre-migration implementation.

Run with python -B -m unittest discover -s scripts/rendering -p 'test*.py'.
Requires the accepted baseline commit in local Git history; no network or device.
The recording C++ tests cover execution order. These checks independently pin
adapter mappings, retained locks and title-specific behavior to the old source.
"""
from functools import lru_cache
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASE = "91c1e3e12c31f119354eb8ac6d33cb5013dd66f3"
CORE = "Core/Libraries/Source/WWVegas/WW3D2/"


@lru_cache(None)
def original(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode()


def current(path):
    return (ROOT / path).read_text()


def tokens(text):
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return re.findall(r"\w+|[^\s]", text)


def body(text, signature):
    start = text.index("{", text.index(signature))
    depth = 1
    end = start + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start + 1:end - 1]


def calls(text, prefix="DX8Wrapper::"):
    # Balance parentheses rather than using a regex that truncates nested calls.
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    result = []
    for match in re.finditer(re.escape(prefix) + r"\w+\s*\(", text):
        end = match.end()
        depth = 1
        while depth:
            depth += (text[end] == "(") - (text[end] == ")")
            end += 1
        result.append("".join(tokens(text[match.start():end])))
    return result


class Render2DReference(unittest.TestCase):
    def setUp(self):
        self.adapter = current(CORE + "Backend/DX8Render2D.cpp")
        self.submission = current(CORE + "Render2DSubmission.h")

    def test_grayscale_overrides_match_both_legacy_titles_exactly(self):
        actual = calls(body(self.adapter, "void Set_Grayscale_Combiner"))
        for title in ("Generals", "GeneralsMD"):
            old = body(original(f"{title}/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp"),
                       "void Render2DClass::Render()")
            expected = [c for c in calls(old) if "Set_DX8_" in c]
            self.assertEqual(actual, expected, title)
            self.assertEqual(len(actual), 13)
        self.assertTrue("".join(tokens(body(self.adapter, "void Set_Grayscale_Combiner"))).startswith("if(dot3){"))

    def test_simple_adapter_mappings_match_legacy_calls(self):
        old = body(original("GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp"),
                   "void Render2DClass::Render()")
        substitutions = {"view": "saved.view", "proj": "saved.projection", "Texture": "texture",
                         "Shader": "shader", "vm": "material", "vb": "vertices", "ib": "indices"}
        old = re.sub(r"\b(" + "|".join(substitutions) + r")\b",
                     lambda m: substitutions[m.group()], old)
        legacy = calls(old)
        for name, count in (("Save_Transforms", 2), ("Set_Texture", 1), ("Set_Prelit_Diffuse_Material", 1),
                            ("Set_Identity_Transforms", 3), ("Bind_Vertices", 1), ("Set_Shader", 1),
                            ("Set_Opaque_Shader", 1), ("Apply_State_Changes", 1), ("Restore_Transforms", 2)):
            mapped = calls(body(self.adapter, name + "("))
            self.assertEqual(len(mapped), count, name)
            cursor = 0
            for call in mapped:
                self.assertIn(call, legacy[cursor:], name)
                cursor = legacy.index(call, cursor) + 1
        self.assertEqual(calls(body(self.adapter, "void Set_Viewport(")), ["DX8Wrapper::Set_Viewport(&vp)"])
        self.assertIn("{viewport.x,viewport.y,viewport.width,viewport.height,viewport.min_z,viewport.max_z}",
                      "".join(tokens(body(self.adapter, "void Set_Viewport("))))
        self.assertIn("DX8Wrapper::Get_Current_Caps()->Support_Dot3()", self.adapter)
        self.assertIn("DX8Wrapper::Set_Index_Buffer(indices,base)", calls(self.adapter))
        self.assertIn("commands.Bind_Indices(indices,0)", calls(self.submission, "commands."))
        self.assertIn("DX8Wrapper::Draw_Triangles(start,triangles,minimum,count)", calls(self.adapter))
        self.assertIn("commands.Draw_Triangles(0,static_cast<unsignedshort>(submission.index_count/3),"
                      "0,static_cast<unsignedshort>(submission.vertex_count))", calls(self.submission, "commands."))
        self.assertIn("REF_PTR_RELEASE(material)", "".join(tokens(self.adapter)))
        self.assertIn("VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE)",
                      "".join(tokens(self.adapter)))
        self.assertIn("ShaderClass::Invalidate()", "".join(tokens(self.adapter)))

    def test_base_declarations_moved_without_layout_or_signature_changes(self):
        for kind, signature in (("vertex", "class VertexBufferLockClass"),
                                ("index", "class IndexBufferClass :")):
            old = original(CORE + "dx8" + kind + "buffer.h")
            end_marker = "/**\n** Dynamic vertex" if kind == "vertex" else "// HY 2/14/01"
            declarations = old[old.index(signature):old.index(end_marker, old.index(signature))]
            new = current(CORE + kind + "buffer.h")
            self.assertEqual(tokens(declarations), tokens(new[new.index(signature):]))
            self.assertNotIn("<d3d", new.lower())
            self.assertNotIn('"dx8', new)

    def test_dynamic_allocation_lock_and_wrapper_sources_are_unchanged(self):
        for name in ("dx8vertexbuffer.cpp", "dx8indexbuffer.cpp", "dx8wrapper.cpp",
                     "texture.cpp", "texturefilter.cpp", "dx8fvf.cpp"):
            self.assertEqual(tokens(current(CORE + name)), tokens(original(CORE + name)), name)
        adapter = "".join(tokens(self.adapter))
        self.assertIn("DynamicVBAccessClass(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,count)", adapter)
        self.assertIn("DynamicIBAccessClass(BUFFER_TYPE_DYNAMIC_DX8,count)", adapter)
        for accessor in ("Get_FVF_Size", "Get_Location_Offset", "Get_Diffuse_Offset", "Get_Tex_Offset"):
            self.assertIn("layout." + accessor + "(", adapter)

    def test_title_viewports_and_default_shader_are_preserved(self):
        for title in ("Generals", "GeneralsMD"):
            path = f"{title}/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp"
            text = current(path)
            self.assertEqual(tokens(body(text, "Render2DClass::Get_Default_Shader()")),
                             tokens(body(original(path), "Render2DClass::Get_Default_Shader()")))
            render = body(text, "void Render2DClass::Render()")
            self.assertIn("IsHidden", render)
            self.assertIn("Submit_Render2D(submission)", render)
            self.assertNotIn("DX8Wrapper", text)
            self.assertNotRegex(text, r'#include\s*[<"](?:dx8|d3d)')
            # Everything outside Render()/its retired includes remains intact:
            # coordinate bias, clipping, text and geometry generation included.
            old_source = original(path)
            old_render = body(old_source, "void Render2DClass::Render()")
            old_rest = re.sub(r'^#include[^\n]*$', '', old_source.replace(old_render, ''), flags=re.M)
            new_rest = re.sub(r'^#include[^\n]*$', '', text.replace(render, ''), flags=re.M)
            self.assertEqual(tokens(old_rest), tokens(new_rest), title)
            if title == "Generals":
                for expression in ("ScreenResolution.Left", "ScreenResolution.Top",
                                   "ScreenResolution.Width()", "ScreenResolution.Height()"):
                    self.assertIn(expression, render)
            else:
                self.assertIn("WW3D::Get_Device_Resolution(width, height, bits, windowed)", render)
                self.assertIn("viewport = {0, 0,", render)

    def test_contract_has_no_sdk_declarations(self):
        for name in ("IRenderBackend.h", "Render2DSubmission.h", "Render2DSubmission.cpp"):
            source = " ".join(tokens(current(CORE + name)))
            self.assertNotRegex(source, r"\b(?:IDirect3D\w*|D3D\w*|D3DX\w*|DXGI\w*)\b", name)


if __name__ == "__main__":
    unittest.main()
