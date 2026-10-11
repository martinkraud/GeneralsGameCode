"""Freeze real UI/loader policy while reversing only the explicit CPU init seam."""
import subprocess
import unittest
from test_render2d_reference import ROOT, CORE, current, tokens, body

BASE = "0170ad1adc8594f06e953dad12894818dfa1c683"

def old(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode()

class UITextureReference(unittest.TestCase):
    def test_actual_caller_and_entire_legacy_policy_still_frozen(self):
        for path in [CORE + name for name in ("textureloader.cpp", "textureloader.h", "ww3dformat.cpp",
                     "texturefilter.cpp", "missingtexture.cpp", "bitmaphandler.cpp", "bitmaphandler.h", "dx8caps.cpp",
                     "Backend/DX8Texture2D.cpp", "Backend/DX8Backend.cpp")
                     ] + ["Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp",
                          "Core/Libraries/Source/WWVegas/WWLib/TARGA.cpp"] + [
                          f"{title}/Code/Libraries/Source/WWVegas/WW3D2/{name}" for title in ("Generals", "GeneralsMD")
                          for name in ("assetmgr.cpp", "assetmgr.h", "ddsfile.cpp", "ddsfile.h", "render2d.cpp")]:
            self.assertEqual(tokens(current(path)), tokens(old(path)), path)

    def test_exact_init_and_install_seams_only(self):
        source = current(CORE + "texture.cpp").replace('#include "IRenderBackend.h"', '')
        guard = 'if (WW3D::Get_Render_Backend() && WW3D::Get_Render_Backend()->Prepare_File_Texture(*this)) return;'
        self.assertEqual(source.count(guard), 1)
        self.assertEqual(tokens(source.replace(guard, '')), tokens(old(CORE + "texture.cpp")))
        header = current(CORE + "texture.h").replace('void Install_Render2D_File_Content(const Texture2DCPUContent &content);', '')
        self.assertEqual(tokens(header), tokens(old(CORE + "texture.h")))
        interface = current(CORE + "IRenderBackend.h").replace('virtual bool Prepare_File_Texture(TextureClass &) { return false; }', '')
        self.assertEqual(tokens(interface), tokens(old(CORE + "IRenderBackend.h")))
        source = current(CORE + "Texture2D.cpp")
        signature = 'void TextureClass::Install_Render2D_File_Content(const Texture2DCPUContent &content)'
        install = body(source, signature)
        source = source.replace('TextureFormat(content.Describe().format)', 'TextureFormat(WW3D_FORMAT_A4R4G4B4)')
        self.assertEqual(tokens(source.replace(signature + '\n{' + install + '}', '')), tokens(old(CORE + "Texture2D.cpp")))
        order = ["content.Snapshot()", "Release_D3D_Storage()", "Discard_Render2D_CPU_Content()", "Render2DCPUContent = snapshot", "Initialized = true"]
        positions = [install.index(s) for s in order]
        self.assertEqual(positions, sorted(positions))

    def test_cpu_decode_and_content_have_no_graphics_api(self):
        import re
        for name in ("UITextureContent.h", "UITextureContent.cpp", "Texture2DCPUContent.h", "Texture2DCPUContent.cpp"):
            self.assertIsNone(re.search(r'\b(?:ID3D\w*|IDirect3D\w*|IDXGI\w*|D3D\w*|DXGI\w*|DX8Wrapper|ComPtr)\b', ' '.join(tokens(current(CORE + name)))), name)
        source = current(CORE + "Backend/D3D11TextureBridge.cpp")
        self.assertIn("file_auto_ptr file(_TheFileFactory", source)
        self.assertNotIn("Request_Foreground_Loading", source)
        self.assertNotIn("Request_Background_Loading", source)
        self.assertNotIn("_Create_DX8_Texture", source)

    def test_sentence_default_storage_and_pipeline_remain_bounded(self):
        source = current(CORE + "Texture2DCPUContent.cpp")
        self.assertIn(": Texture2DCPUContent(w, h, WW3D_FORMAT_A4R4G4B4, row_pitch)", source)
        shader = current(CORE + "Backend/D3D11Backend.cpp")
        self.assertEqual(tokens(body(shader, "ShaderClass SentenceShader()")), tokens(body(old(CORE + "Backend/D3D11Backend.cpp"), "ShaderClass SentenceShader()")))
        self.assertIn("if (name.size() < 4", current(CORE + "UITextureContent.cpp"))

if __name__ == "__main__": unittest.main()
