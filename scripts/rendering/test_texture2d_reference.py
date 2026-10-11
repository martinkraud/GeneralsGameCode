"""Pin the bounded texture migration to the accepted RB1 DX8 reference."""
import subprocess
import re
import unittest
from test_render2d_reference import ROOT, CORE, current, tokens, body, calls

BASE = "4911acc4cc52d75a95a3c6702ca29bea385aba12"


def old(name):
    return subprocess.check_output(["git", "show", f"{BASE}:{CORE}{name}"], cwd=ROOT).decode()


class Texture2DReference(unittest.TestCase):
    def test_only_render2d_branch_and_single_owner_release_hooks_changed(self):
        new = current(CORE + "texture.cpp")
        new = new.replace('#include "IRenderBackend.h"', '')
        new = new.replace('if (WW3D::Get_Render_Backend() && WW3D::Get_Render_Backend()->Prepare_File_Texture(*this)) return;', '')
        branch = "if(Render2DUsed){Apply_Render2D_Texture(stage);return;}"
        flat = "".join(tokens(new))
        self.assertEqual(flat.count("Discard_Render2D_CPU_Content();"), 1)
        self.assertEqual(flat.count("if(Get_Render2D_CPU_Content()&&!Peek_D3D_Base_Texture())Get_Render2D_Residency();"), 1)
        flat = flat.replace("Discard_Render2D_CPU_Content();", "")
        flat = flat.replace("if(Get_Render2D_CPU_Content()&&!Peek_D3D_Base_Texture())Get_Render2D_Residency();", "")
        self.assertEqual(flat.count(branch), 1)
        flat = flat.replace(branch, "")
        signature = "void TextureBaseClass::Release_D3D_Storage()"
        release = body(new, signature)
        self.assertEqual(tokens(release), tokens("""
            if (Render2DResidency.Has_Residency()) { Render2DResidency.Reset(); }
            else if (D3DTexture) { D3DTexture->Release(); }
            D3DTexture = nullptr;
        """))
        flat = flat.replace("".join(tokens(signature + "{" + release + "}")), "")
        reference = "".join(tokens(old("texture.cpp")))
        release_block = "if(D3DTexture){D3DTexture->Release();D3DTexture=nullptr;}"
        self.assertEqual(reference.count(release_block), 2)
        reference = reference.replace(release_block, "Release_D3D_Storage();")
        reference = reference.replace("if(D3DTexture!=nullptr){D3DTexture->Release();}", "Release_D3D_Storage();")
        reference = reference.replace("if(D3DTexture)D3DTexture->Release();D3DTexture=nullptr;", "Release_D3D_Storage();")
        reference = reference.replace("IDirect3DBaseTexture8*d3d_tex=Peek_D3D_Base_Texture();if(d3d_tex)d3d_tex->Release();",
                                      "Peek_D3D_Base_Texture();Release_D3D_Storage();", 1)
        self.assertEqual(flat, reference)
        # Loader results, format selection, reduction, missing pixels, font3d
        # conversion, reference counts, and every legacy surface update unchanged.
        for name in ("textureloader.cpp", "textureloader.h", "surfaceclass.cpp",
                     "surfaceclass.h", "missingtexture.cpp", "font3d.cpp", "texturefilter.cpp"):
            self.assertEqual(tokens(current(CORE + name)), tokens(old(name)), name)

    def test_sentence_copy_preserves_entire_producer_and_surface_lifetime(self):
        # RB3 separately pins every sentence and extracted glyph token against
        # RB2, reversing only the explicit CPU backing/window seams.
        from test_sentence_content_reference import restored_sentence
        new = restored_sentence().replace('#include "ww3d.h"', '')
        new = new.replace('#include "IRenderBackend.h"', '#include "dx8wrapper.h"')
        new = new.replace("WW3D::Get_Render_Backend()->Copy_Render2D_Surface(*curr_surface, *texture_surface)",
                          "DX8Wrapper::_Copy_DX8_Rects (curr_surface->Peek_D3D_Surface (), nullptr, 0, texture_surface->Peek_D3D_Surface (), nullptr)")
        self.assertEqual(tokens(new), tokens(old("render2dsentence.cpp")))
        adapter = current(CORE + "Backend/DX8Texture2D.cpp")
        expected = "DX8Wrapper::_Copy_DX8_Rects(source.Peek_D3D_Surface(),nullptr,0,destination.Peek_D3D_Surface(),nullptr)"
        self.assertEqual(calls(body(adapter, "void DX8Backend::Copy_Render2D_Surface")), [expected])

    def test_sampler_translations_and_order_match_reference(self):
        adapter = current(CORE + "Backend/DX8Texture2D.cpp")
        actual = []
        for method in ("Min", "Mag", "Mip", "U", "V"):
            actual += calls(body(adapter, f"void {method}("))
        reference = body(old("texturefilter.cpp"), "void TextureFilterClass::Apply(")
        for member in ("TextureMinFilter", "TextureMagFilter", "MipMapFilter"):
            reference = re.sub(r"\b" + member + r"\b", "value", reference)
        self.assertEqual(actual, calls(reference))
        self.assertEqual(len(actual), 7)
        neutral = current(CORE + "Texture2D.h")
        self.assertEqual(calls(body(neutral, "void Execute_Texture2D_Sampler("), "commands."),
                         [f"commands.{name}(stage,sampler.{field})" for name, field in
                          (("Min", "min"), ("Mag", "mag"), ("Mip", "mip"), ("U", "u"), ("V", "v"))])

    def test_preparation_and_binding_match_deferred_reference(self):
        neutral = current(CORE + "Texture2D.cpp")
        self.assertEqual(tokens(body(neutral, "void TextureClass::Prepare_Render2D_Texture(")),
                         tokens("if (!Initialized) Init(); LastAccessed = WW3D::Get_Sync_Time();"))
        self.assertIn("Texture2DResidencyOwner Render2DResidency;", current(CORE + "texture.h"))
        adapter = current(CORE + "Backend/DX8Texture2D.cpp")
        self.assertIn("DX8_RECORD_TEXTURE(&texture)", adapter)
        self.assertIn("texture.Peek_D3D_Base_Texture()", adapter)
        self.assertNotIn("AddRef", adapter)
        self.assertIn("if (storage) storage->Release();", adapter)
        self.assertIn("storage(logical.Peek_D3D_Texture())", adapter)
        render = body(current(CORE + "Backend/DX8Render2D.cpp"), "void Set_Texture(")
        self.assertEqual(tokens(render), tokens("if (texture) texture->Use_For_Render2D(); DX8Wrapper::Set_Texture(0, texture);"))
        self.assertNotIn("Init(", render)

    def test_content_mapping_uses_actual_current_storage_and_original_lock(self):
        adapter = current(CORE + "Backend/DX8Texture2D.cpp")
        self.assertIn("texture.Get_Surface_Level(mip)", adapter)
        self.assertIn("current->GetLevelCount()", adapter)
        self.assertIn("D3DFormat_To_WW3DFormat(desc.Format)", adapter)
        self.assertIn("texture.Is_Missing_Texture()", adapter)
        self.assertIn("surface->Lock(&mapping.row_pitch)", adapter)
        release = body(adapter, "~DX8Texture2DContent()")
        self.assertLess(release.index("surface->Unlock()"), release.index("surface->Release_Ref()"))

    def test_new_contract_and_producer_source_have_no_sdk_tokens(self):
        import re
        for name in ("Texture2D.h", "Texture2D.cpp", "render2dsentence.cpp"):
            source = " ".join(tokens(current(CORE + name)))
            self.assertIsNone(re.search(r"\b(?:IDirect3D\w*|D3D\w*|D3DX\w*|DXGI\w*|DX8Wrapper)\b", source), name)

    def test_shared_dynamic_ui_and_file_asset_producers_are_unchanged(self):
        paths = ["Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp",
                 "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DVideoBuffer.cpp",
                 "Core/GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp"]
        paths += [f"{title}/Code/Libraries/Source/WWVegas/WW3D2/assetmgr.cpp"
                  for title in ("Generals", "GeneralsMD")]
        for path in paths:
            reference = subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode()
            self.assertEqual(tokens(current(path)), tokens(reference), path)


if __name__ == "__main__":
    unittest.main()
