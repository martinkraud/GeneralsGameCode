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
    text = (ROOT / path).read_text()
    if path.endswith("WW3D2/surfaceclass.cpp"):
        # engine-completion milestone, upstream #3459: one explicit pointer-width cast in the alpha-lock address arithmetic (no behaviour change)
        text = text.replace("(uintptr_t)lock_rect.pBits", "(unsigned int)lock_rect.pBits")
    if path.endswith("WW3D2/assetmgr.cpp"):
        # upstream #3459: the name length is a pointer difference, no longer taken through two truncating (int) casts (same value)
        text = text.replace("(int)(mesh_name - name) + 1", "((int)mesh_name) - ((int)name) + 1")
    return text


# ---------------------------------------------------------------------------
# Reviewed changes made AFTER the references of these tests were frozen.
#
# The reference commits (BASE of each test module) pin the legacy DX8 behaviour.
# Later milestones added native-backend-only code to a few pinned files.  Every
# addition below is purely additive and gated on the native backend; the legacy
# path is token-for-token what the reference has.  Instead of weakening the
# freeze, the tests remove exactly these known additions (asserting each is
# present exactly once) or re-pin the file to the reviewed commit, and compare
# the rest with the original.  Any further, unreviewed change still fails.
#
# * X64.35 (10b4503e9): render2dsentence.{h,cpp} remember the first characters
#   of a sentence (DiagnosticText) to label its texture - measurement only.
# * X64.30 / X64.31 fix2 (a8781fccd, 5e470ff9c): texture.cpp serves procedural
#   (radar) and native file UI textures from CPU content.
# * R1b/R2a (4dcee8c18, 00fb1ce2c): dx8vertexbuffer.cpp CPU-backed geometry,
#   opt-in under the native D3D11 presentation (re-pinned).
# * X64.22 .. X64.49: W3DDisplay.cpp native presentation hooks (screenshots,
#   present path, render scale, camera cadence); no existing line removed
#   (re-pinned to c74890a7e).
# * X64.45 (fb4381382): Zero Hour render2d.cpp carries the Generals viewport
#   (Render2DClass::ScreenResolution, the Modern Display UI scale).
# ---------------------------------------------------------------------------
REVIEWED_PIN = {
    "Core/Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.cpp": "00fb1ce2c",
    "Core/Libraries/Source/WWVegas/WW3D2/dx8indexbuffer.cpp": "00fb1ce2c",
    "GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp": "fb4381382",
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp": "65fe3976c",   # re-pinned: frame-delivery phase marks (_WIN64 only)
}


def reviewed_reference(path, base):
    """Reference text of a file: the reviewed commit if re-pinned, else BASE."""
    return subprocess.check_output(
        ["git", "show", f"{REVIEWED_PIN.get(path, base)}:{path}"], cwd=ROOT).decode()


def assert_additive_only(test, new_text, reference_text, label):
    """The reference's tokens must all survive, in order: only insertions allowed.

    Used for files that gained reviewed, native-only code (texture.h members,
    SurfaceClass CPU content) but whose legacy text must stay token-for-token.
    """
    import difflib
    a, b = tokens(reference_text), tokens(new_text)
    matcher = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag not in ("equal", "insert"):
            test.fail(f"{label}: legacy tokens changed near {' '.join(a[i1:i1 + 12])!r} -> "
                      f"{' '.join(b[j1:j1 + 12])!r}")


def _remove_once(text, pattern):
    text, count = re.subn(pattern, "", text, flags=re.S)
    assert count == 1, (pattern, count)
    return text


def strip_x64_35_sentence_labels(text, header=False):
    """Remove the X64.35 DiagnosticText measurement labels (render2dsentence.*)."""
    if header:
        return _remove_once(text, r"[ \t]*char[ \t]+DiagnosticText\[48\];[^\n]*\n")
    text = _remove_once(text, r"\tDiagnosticText\[0\] = 0;\n")
    text = _remove_once(text, r"\t\tnew_texture->Set_Diagnostic_Label\(DiagnosticText\);[^\n]*\n")
    return _remove_once(text, r"\t\{\t// X64\.35 attribution:.*?DiagnosticText\[n\] = 0;\n\t\}\n\n")


def strip_native_texture_branches(text):
    """Remove the reviewed native-only branches of texture.cpp (see above)."""
    text = _remove_once(text, r'#include "Texture2DCPUContent\.h"\n')
    text = _remove_once(text, r"\t// Native backends keep procedural single-mip UI textures.*?\t\treturn;\n\t\}\n\n")
    return _remove_once(
        text, r"\tif \(Render2DDynamic && Render2DCPUContent\)\n\t\{.*?\n\t\}\n"
              r"\tif \(level == 0 && Render2DCPUContent && !Peek_D3D_Texture\(\)\)\n\t\{.*?\n\t\}\n")


def restore_resource_calls(test, text, helpers):
    """Reverse only X64.10's explicitly counted compatibility substitutions."""
    include = '#include "WW3D2/LegacyResource.h"\n'
    test.assertEqual(text.count(include), 1)
    text = text.replace(include, "")
    for helper, count in helpers:
        replacement = "LegacyD3DX::" + helper + "("
        test.assertEqual(text.count(replacement), count)
        text = text.replace(replacement, "D3DX" + helper + "(")
    return text


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
                     "texturefilter.cpp", "dx8fvf.cpp"):
            text = current(CORE + name)
            if name == "dx8wrapper.cpp":
                text = restore_resource_calls(self, text, (
                    ("CreateTextureFromFileExA", 1), ("LoadSurfaceFromSurface", 1),
                    ("FilterTexture", 1)))
                # X64.8 permits only two CPU diagnostic substitutions.
                include = '#include "LegacyErrorString.h"\n'
                self.assertEqual(text.count(include), 1)
                self.assertEqual(text.count("LegacyD3DX::GetErrorStringA("), 2)
                text = text.replace(include, "").replace(
                    "LegacyD3DX::GetErrorStringA(", "D3DXGetErrorStringA(")
                # X64.6 permits only the measured creation-helper substitutions.
                include = '#include "LegacyTextureCreation.h"\n'
                self.assertEqual(text.count(include), 1)
                text = text.replace(include, "")
                for helper, count in (("CreateTexture", 4), ("CreateCubeTexture", 4),
                                      ("CreateVolumeTexture", 2)):
                    replacement = "LegacyD3DX::" + helper
                    self.assertEqual(text.count(replacement), count)
                    text = text.replace(replacement, "D3DX" + helper)
            if name == "dx8fvf.cpp":
                # X64.5 permits only this reference-tested integer substitution.
                include = '#include "LegacyFVFStride.h"\n'
                self.assertEqual(text.count(include), 1)
                self.assertEqual(text.count("LegacyD3DX::FVFVertexSize("), 1)
                text = text.replace(include, "").replace(
                    "LegacyD3DX::FVFVertexSize(", "D3DXGetFVFVertexSize(")
            self.assertEqual(tokens(text), tokens(reviewed_reference(CORE + name, BASE)), name)
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
            # X64.45 (fb4381382): Zero Hour carries the Generals viewport, the
            # Render2DClass::ScreenResolution rectangle (Modern Display UI scale);
            # the device-resolution viewport is gone from both titles.
            for expression in ("ScreenResolution.Left", "ScreenResolution.Top",
                               "ScreenResolution.Width()", "ScreenResolution.Height()"):
                self.assertIn(expression, render, title)
            self.assertNotIn("Get_Device_Resolution", render, title)

    def test_contract_has_no_sdk_declarations(self):
        for name in ("IRenderBackend.h", "Render2DSubmission.h", "Render2DSubmission.cpp"):
            source = " ".join(tokens(current(CORE + name)))
            self.assertNotRegex(source, r"\b(?:IDirect3D\w*|D3D\w*|D3DX\w*|DXGI\w*)\b", name)


if __name__ == "__main__":
    unittest.main()
