"""Pin actual sentence/glyph semantics to RB2, allowing only storage/window seams."""
import subprocess
import re
import unittest
from test_render2d_reference import ROOT, CORE, current, tokens, body

BASE = "4a412d1104763a40a5732f0b7830dc0a56bfbdb9"
MARKER = "////////////////////////////////////////////////////////////////////////////////////\n//\n//\tFontCharsClass\n"


def old(name):
    return subprocess.check_output(["git", "show", f"{BASE}:{CORE}{name}"], cwd=ROOT).decode()


def restored_sentence():
    new = current(CORE + "render2dsentence.cpp")
    new = new[:new.index("bool FontCharsClass::Initialize_GDI_Font(")]
    new = new.replace('"Texture2DCPUContent.h"', '"surfaceclass.h"')
    new = new.replace('#include "ww3d.h"', '#include "IRenderBackend.h"\n#include "ww3d.h"')
    new = new.replace("Texture2DCPUContent", "SurfaceClass")
    new = new.replace("TextureClass (*curr_surface);", "TextureClass (desc.Width, desc.Width, WW3D_FORMAT_A4R4G4B4, MIP_LEVELS_1);\n\t\tSurfaceClass *texture_surface = new_texture->Get_Surface_Level ();")
    new = new.replace("// The constructor snapshots CPU bytes; residency upload occurs at first use.", "WW3D::Get_Render_Backend()->Copy_Render2D_Surface(*curr_surface, *texture_surface);\n\t\tREF_PTR_RELEASE (texture_surface);")
    new = new.replace("(CurrTextureSize, CurrTextureSize)", "(CurrTextureSize, CurrTextureSize, WW3D_FORMAT_A4R4G4B4)")
    glyph = current(CORE + "FontChars.cpp")
    glyph = glyph[glyph.index(MARKER):].replace("std::min", "min").replace("std::max", "max")
    glyph = glyph.replace("Create_GDI_Font (const char *font_name, void *window)", "Create_GDI_Font (const char *font_name)")
    glyph = glyph.replace("(HWND)window", "(HWND)WW3D::Get_Window()")
    glyph = glyph.replace("Initialize_GDI_Font (const char *font_name, int point_size, bool is_bold, void *window)", "Initialize_GDI_Font (const char *font_name, int point_size, bool is_bold)")
    glyph = glyph.replace("Create_GDI_Font (font_name, window)", "Create_GDI_Font (font_name)")
    return new + glyph


class SentenceContentReference(unittest.TestCase):
    def test_all_sentence_and_real_glyph_tokens_preserved(self):
        self.assertEqual(tokens(restored_sentence()), tokens(old("render2dsentence.cpp")))

    def test_gpu_free_actual_producer_and_backing(self):
        for name in ("FontChars.h", "FontChars.cpp", "Texture2DCPUContent.h",
                     "Texture2DCPUContent.cpp", "render2dsentence.cpp"):
            code = " ".join(tokens(current(CORE + name)))
            self.assertIsNone(re.search(r"\b(?:IDirect3D\w*|D3D\w*|D3DX\w*|DX8Wrapper|SurfaceClass)\b", code), name)

    def test_backend_upload_uses_original_copy_and_single_owner(self):
        code = body(current(CORE + "Backend/DX8Texture2D.cpp"),
                    "Texture2DResidency *DX8Backend::Create_Render2D_Residency")
        order = ["content->Describe()", "_Create_DX8_Texture", "TextureBaseClass::Set_D3D_Base_Texture",
                 "native->Release()", "new SurfaceClass", "source->Lock", "content->Copy_Rows",
                 "source->Unlock()", "texture.Get_Surface_Level()", "Copy_Render2D_Surface",
                 "destination->Release_Ref()", "source->Release_Ref()", "new DX8Texture2DResidency"]
        positions = [code.index(value) for value in order]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("D3DPOOL_MANAGED", code)
        self.assertIn("MIP_LEVELS_1", code)
        self.assertIn("Render2DCPUContent = content.Snapshot()", current(CORE + "Texture2D.cpp"))

    def test_independent_target_links_no_engine_or_graphics_sdk(self):
        text = current("Tests/Google/Dependencies/CMakeLists.txt")
        target = text[text.index("add_executable(sentence_content_test"):]
        for forbidden in ("legacy_dx8", "core_wwcommon", "dx8-src", "d3d8.lib", "d3dx8"):
            self.assertNotIn(forbidden, target)
        self.assertIn("WW3D2/FontChars.cpp", target)
        self.assertIn("WW3D2/Texture2DCPUContent.cpp", target)


if __name__ == "__main__":
    unittest.main()
