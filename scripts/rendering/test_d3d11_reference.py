"""D3D11 slice policy checks, independently pinned to real DX8/W3D callers."""
import re
import subprocess
import unittest
from test_render2d_reference import ROOT, CORE, current, tokens, body

BASE = "564787154a13e996dd73002c9db78c4db7c0cefa"


class D3D11Reference(unittest.TestCase):
    def test_sentence_shader_matches_both_real_title_defaults(self):
        actual = body(current(CORE + "Backend/D3D11Backend.cpp"), "ShaderClass SentenceShader()")
        for title in ("Generals", "GeneralsMD"):
            source = current(f"{title}/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp")
            expected = body(source, "Render2DClass::Get_Default_Shader()")
            self.assertEqual(tokens(actual), tokens(expected), title)
            shader = current(f"{title}/Code/Libraries/Source/WWVegas/WW3D2/shader.h")
            reference = current("GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/shader.h")
            self.assertEqual(tokens(body(shader, "inline void ShaderClass::Reset()")),
                             tokens(body(reference, "inline void ShaderClass::Reset()")))

    def test_reference_backend_contracts_and_producers_are_unchanged(self):
        names = ("IRenderBackend.h", "Render2DSubmission.h", "Render2DSubmission.cpp",
                 "Texture2D.h", "Texture2D.cpp", "Texture2DCPUContent.h", "Texture2DCPUContent.cpp",
                 "texture.h", "texture.cpp", "FontChars.h", "FontChars.cpp", "render2dsentence.h",
                 "render2dsentence.cpp", "textureloader.cpp", "Backend/DX8Backend.cpp",
                 "Backend/DX8Render2D.cpp", "Backend/DX8Texture2D.cpp")
        for name in names:
            old = subprocess.check_output(["git", "show", f"{BASE}:{CORE}{name}"], cwd=ROOT).decode()
            self.assertEqual(tokens(current(CORE + name)), tokens(old), name)

    def test_sampler_culling_and_vertex_writer_use_explicit_reference_semantics(self):
        source = current(CORE + "Backend/D3D11Backend.cpp")
        reference = current(CORE + "texturefilter.cpp")
        for name in ("Min", "Mag"):
            self.assertIn(f"_{name}TextureFilters[0][FILTER_TYPE_NONE]=D3DTEXF_POINT", "".join(tokens(reference)))
        self.assertIn("_MipMapFilters[0][FILTER_TYPE_NONE]=D3DTEXF_NONE", "".join(tokens(reference)))
        self.assertIn("D3D11_FILTER_MIN_MAG_MIP_POINT", source)
        self.assertIn("s.MinLOD = s.MaxLOD = 0", source)
        self.assertIn("r.FrontCounterClockwise = TRUE", source)
        self.assertIn("_PolygonCullMode = D3DCULL_CW", current("GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/shader.cpp"))
        self.assertIn("Write_Render2D_Vertices(s,", source)
        self.assertIn("p.color = v.color.bgra", source)
        self.assertIn("rt.SrcBlend = rt.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA", source)
        self.assertIn("rt.DestBlend = rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA", source)

    def test_backend_has_no_native_types_in_its_header_or_legacy_sdk_in_core(self):
        header = " ".join(tokens(current(CORE + "Backend/D3D11Backend.h")))
        self.assertIsNone(re.search(r"\b(?:ID3D\w*|IDXGI\w*|DXGI_FORMAT|D3D11_\w*|ComPtr)\b", header))
        source = " ".join(tokens(current(CORE + "Backend/D3D11Backend.cpp")))
        self.assertIsNone(re.search(r"\b(?:D3DX\w*|IDirect3D\w*|DX8Wrapper)\b", source))
        bridge = current(CORE + "Backend/D3D11TextureBridge.cpp")
        self.assertLess(bridge.index("WW3D::Get_Render_Backend() != &backend"), bridge.index("texture.Prepare_Render2D_Texture()"))
        self.assertLess(bridge.index("!texture.Get_Render2D_CPU_Content()"), bridge.index("texture.Prepare_Render2D_Texture()"))
        self.assertLess(bridge.index("texture.Prepare_Render2D_Texture()"), bridge.index("texture.Get_Render2D_Residency()"))


if __name__ == "__main__":
    unittest.main()
