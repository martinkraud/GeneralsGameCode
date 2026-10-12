// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/render2d.h"
#include "WW3D2/render2dsentence.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/texture.h"
#include <memory>

namespace {
struct UIState {
    RectClass screen = Render2DClass::Get_Screen_Resolution();
    bool bias = WW3D::Is_Screen_UV_Biased();
    UIState() { Render2DClass::Set_Screen_Resolution(RectClass(0,0,800,600)); }
    ~UIState() { Render2DClass::Set_Screen_Resolution(screen); WW3D::Set_Screen_UV_Bias(bias); }
};
// Expose actual producer output only in tests. No replacement coordinate model.
struct QuadProbe : Render2DClass {
    const Vector2 &Position(int i) const { return Vertices[i]; }
    const Vector2 &UV(int i) const { return UVCoordinates[i]; }
};
struct Release { void operator()(const FontCharsClass *p) const { p->Release_Ref(); } };
}

TEST(UIProducerTest, OriginalLogoActualQuadPreservesExtentUVsWithBothBiasPolicies)
{
    UIState state;
    const RectClass uv(1.0f/512,1.0f/512,371.0f/512,121.0f/512);
    for(bool biased : {false,true}) {
        WW3D::Set_Screen_UV_Bias(biased);
        QuadProbe quad;
        quad.Set_Coordinate_Range(RectClass(0,0,800,600));
        quad.Add_Quad(RectClass(504,16,791,110),uv);
        const float shift = biased ? -0.5f : 0;
        const float x[4] = {504,504,791,791};
        const float y[4] = {16,110,16,110};
        for(int i=0;i<4;++i) {
            // D3D11's existing correction adds half a viewport pixel.
            const float px=(quad.Position(i).X+1)*400+0.5f;
            const float py=(1-quad.Position(i).Y)*300+0.5f;
            EXPECT_NEAR(px,x[i]+shift+0.5f,0.0001f);
            EXPECT_NEAR(py,y[i]+shift+0.5f,0.0001f);
            EXPECT_FLOAT_EQ(quad.UV(i).X,i<2 ? uv.Left : uv.Right);
            EXPECT_FLOAT_EQ(quad.UV(i).Y,i%2==0 ? uv.Top : uv.Bottom);
        }
        EXPECT_NEAR((quad.Position(2).X-quad.Position(0).X)*400,287,0.0001f);
        EXPECT_NEAR((quad.Position(0).Y-quad.Position(1).Y)*300,94,0.0001f);
    }
}

TEST(UIProducerTest, ActualSentenceBuildDrawClipResetAndProductionPools)
{
    UIState state;
    WW3D::Set_Screen_UV_Bias(true);
    std::unique_ptr<FontCharsClass,Release> font(new FontCharsClass);
    ASSERT_TRUE(font->Initialize_GDI_Font("Arial",14,false,nullptr));
    for(unsigned i=0;i<40;++i) {
        Render2DSentenceClass sentence;
        sentence.Set_Font(font.get());
        sentence.Set_Clipping_Rect(RectClass(540,116,756,152));
        sentence.Build_Sentence(L"Select Difficulty",nullptr,nullptr);
        const Vector2 size=sentence.Get_Text_Extents(L"Select Difficulty");
        ASSERT_GT(size.X,0); ASSERT_GT(size.Y,0);
        sentence.Set_Location(Vector2(540,116));
        sentence.Draw_Sentence(0xffffbe01);
        const RectClass extents=sentence.Get_Draw_Extents();
        EXPECT_GE(extents.Left,540); EXPECT_GE(extents.Top,116);
        EXPECT_LE(extents.Right,756); EXPECT_LE(extents.Bottom,152);
        EXPECT_GT(extents.Width(),0); EXPECT_GT(extents.Height(),0);
        sentence.Reset_Polys();
        sentence.Set_Location(Vector2(542,118));
        sentence.Draw_Sentence();
        EXPECT_GT(sentence.Get_Draw_Extents().Width(),0);
        sentence.Reset();
    }
    // The sentence retains/releases the font; our owner is still usable.
    EXPECT_GT(font->Get_Char_Width(L'W'),0);
}
TEST(UIProducerTest, ActualPanelProducerUsesExactDiffuseAlphaShaderAndGeometry)
{
    UIState state;WW3D::Set_Screen_UV_Bias(true);QuadProbe q;
    q.Set_Coordinate_Range(RectClass(0,0,800,600));
    q.Enable_Texturing(false);q.Enable_Additive(false);q.Enable_Alpha(true);
    ShaderClass expected=Render2DClass::Get_Default_Shader();
    expected.Set_Texturing(ShaderClass::TEXTURING_DISABLE);
    EXPECT_EQ(q.Get_Shader()->Get_Bits(),expected.Get_Bits());
    EXPECT_EQ(q.Get_Shader()->Get_Src_Blend_Func(),ShaderClass::SRCBLEND_SRC_ALPHA);
    EXPECT_EQ(q.Get_Shader()->Get_Dst_Blend_Func(),ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
    q.Add_Quad(RectClass(533,109,755,359),0x7e000000);
    EXPECT_NEAR((q.Position(0).X+1)*400+0.5f,533,0.0001f);
    EXPECT_NEAR((1-q.Position(0).Y)*300+0.5f,109,0.0001f);
    EXPECT_NEAR((q.Position(2).X-q.Position(0).X)*400,222,0.0001f);
    EXPECT_NEAR((q.Position(0).Y-q.Position(1).Y)*300,250,0.0001f);
}

TEST(UIProducerTest, ResetAndDisableTexturingRetainActualLogoTextureOwnership)
{
    UIState state;QuadProbe q;
    TextureClass *texture=new TextureClass("unused-retained-logo.tga",nullptr,MIP_LEVELS_1);
    EXPECT_EQ(texture->Num_Refs(),1);
    q.Set_Texture(texture);EXPECT_EQ(texture->Num_Refs(),2);
    q.Enable_Texturing(true);q.Add_Quad(RectClass(504,16,791,110));
    q.Reset();q.Enable_Texturing(false);q.Enable_Additive(false);q.Enable_Alpha(true);
    q.Add_Rect(RectClass(533,109,755,359),0,0,0x7e000000);
    EXPECT_EQ(q.Peek_Texture(),texture);EXPECT_EQ(texture->Num_Refs(),2);
    EXPECT_EQ(q.Get_Shader()->Get_Texturing(),ShaderClass::TEXTURING_DISABLE);
    q.Reset();EXPECT_EQ(q.Peek_Texture(),texture);EXPECT_EQ(texture->Num_Refs(),2);
    q.Enable_Texturing(true);EXPECT_EQ(q.Peek_Texture(),texture);
    q.Set_Texture(static_cast<TextureClass *>(nullptr));EXPECT_EQ(texture->Num_Refs(),1);texture->Release_Ref();
}
