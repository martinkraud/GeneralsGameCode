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

TEST(UIProducerTest, OriginalGeneralsFontKeepsLegacyNarrowWidthPolicy)
{
    std::unique_ptr<FontCharsClass,Release> original(new FontCharsClass), arial(new FontCharsClass);
    ASSERT_TRUE(original->Initialize_GDI_Font("Generals",15,false,nullptr));
    ASSERT_TRUE(arial->Initialize_GDI_Font("Arial",15,false,nullptr));
    // Same original GDI producer; the Generals alias also requests lfWidth=8.
    EXPECT_TRUE(original->Is_Font("Generals",15,false));
    EXPECT_GT(original->Get_Char_Width(L'W'),0);
    EXPECT_LT(original->Get_Char_Width(L'W'),arial->Get_Char_Width(L'W'));
}
#if defined(_WIN64)
#include "W3DDevice/GameClient/NativeMenuComposition.h"
namespace {
unsigned unsafeCalls=0;
WindowMsgHandledType UnsafeMenuCallback(GameWindow *,UnsignedInt,WindowMsgData,WindowMsgData) {
    ++unsafeCalls;return MSG_HANDLED;
}
struct MenuRecorder : NativeStaticMenuManager {
    struct Rect { int x,y,endX,endY;const Image *image; };
    std::vector<Rect> draws;
    GameFont *winFindFont(AsciiString,Int,Bool) override {return nullptr;}
    void winDrawImage(const Image *image,Int x,Int y,Int ex,Int ey,Color) override {
        draws.push_back({x,y,ex,ey,image});
    }
};
}
// Exercise the actual legacy filename resolver and the actual diagnostic Load
// entry point. The local-file spy returns null deliberately: no assets, renderer,
// layout callbacks or native title execution are needed to reproduce this gate.
#include "Common/FileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/ArchiveFileSystem.h"
namespace {
struct MenuFileSpy : LocalFileSystem {
    void init() override {}
    void reset() override {}
    void update() override {}
    AsciiString requested;
    File *openFile(const Char *name,Int,size_t) override {requested=name;return nullptr;}
    Bool doesFileExist(const Char *) const override {return FALSE;}
    void getFileListInDirectory(const AsciiString&,const AsciiString&,const AsciiString&,FilenameList&,Bool) const override {}
    Bool getFileInfo(const AsciiString&,FileInfo*) const override {return FALSE;}
    Bool createDirectory(AsciiString) override {return FALSE;}
    AsciiString normalizePath(const AsciiString& p) const override {return p;}
};
struct MenuFileScope {
    FileSystem *file=TheFileSystem;
    LocalFileSystem *local=TheLocalFileSystem;
    ArchiveFileSystem *archive=TheArchiveFileSystem;
    FileSystem system;MenuFileSpy spy;
    MenuFileScope(){TheFileSystem=&system;TheLocalFileSystem=&spy;TheArchiveFileSystem=nullptr;}
    ~MenuFileScope(){TheFileSystem=file;TheLocalFileSystem=local;TheArchiveFileSystem=archive;}
};
}
TEST(UIProducerTest, StaticMenuUsesOriginalMenusArchivePathThroughActualLegacyResolver)
{
    MenuFileScope files;
    NativeMenuComposition menu;
    EXPECT_EQ(TheWindowManager->winCreateFromScript("MainMenu.wnd"),nullptr);
    EXPECT_EQ(files.spy.requested,AsciiString("Window\\MainMenu.wnd"));
    // The corrected production entry point must request the actual BIG key,
    // not the widget-name prefix MainMenu.wnd: used inside the layout.
    EXPECT_THROW(menu.Load(),std::runtime_error);
    EXPECT_EQ(files.spy.requested,AsciiString("Window\\Menus\\MainMenu.wnd"));
}
TEST(UIProducerTest, StaticMenuBlocksApplicationEventsAndRestoresSingletonOwnership)
{
    auto *old=TheWindowManager;auto *transitions=TheTransitionHandler;unsafeCalls=0;
    {
        NativeMenuComposition scope;
        EXPECT_NE(TheWindowManager,old);EXPECT_EQ(TheTransitionHandler,nullptr);
        auto *manager=static_cast<NativeStaticMenuManager*>(TheWindowManager);
        auto *w=manager->allocateNewWindow();
        w->winSetSystemFunc(UnsafeMenuCallback);w->winSetInputFunc(UnsafeMenuCallback);
        EXPECT_EQ(manager->winSendSystemMsg(w,GWM_CREATE,0,0),MSG_IGNORED);
        EXPECT_EQ(manager->winSendInputMsg(w,GWM_SCRIPT_CREATE,0,0),MSG_IGNORED);
        EXPECT_EQ(unsafeCalls,0u);EXPECT_EQ(manager->blockedSystem,1u);EXPECT_EQ(manager->blockedInput,1u);
        w->winSetSystemFunc(GadgetPushButtonSystem);
        EXPECT_EQ(manager->winSendSystemMsg(w,GWM_CREATE,0,0),MSG_HANDLED);
        EXPECT_THROW(manager->winSendSystemMsg(w,GWM_INPUT_FOCUS,0,0),std::runtime_error);
        w->winSetSystemFunc(nullptr);deleteInstance(w);
    }
    EXPECT_EQ(TheWindowManager,old);EXPECT_EQ(TheTransitionHandler,transitions);
}
TEST(UIProducerTest, ActualThreePieceButtonDrawTilesOriginalGeometryAndManagerDestroysTree)
{
    auto *old=TheWindowManager;auto *transitions=TheTransitionHandler;
    {
        MenuRecorder manager;TheWindowManager=&manager;TheTransitionHandler=nullptr;
        WinInstanceData data;data.m_style=GWS_USER_WINDOW;
        auto *root=manager.winCreate(nullptr,WIN_STATUS_ENABLED,0,0,800,600,nullptr,&data);
        ASSERT_NE(root,nullptr);
        // Actual widget construction and image callback, no substitute tiler.
        Image *left=newInstance(Image),*middle=newInstance(Image),*right=newInstance(Image);
        ICoord2D size{8,36};left->setImageSize(&size);right->setImageSize(&size);
        size.x=16;middle->setImageSize(&size);
        data.m_style=GWS_PUSH_BUTTON;
        auto *button=manager.gogoGadgetPushButton(root,WIN_STATUS_ENABLED|WIN_STATUS_IMAGE,540,116,208,36,&data,nullptr,FALSE);
        ASSERT_NE(button,nullptr);
        button->winSetEnabledImage(0,left);button->winSetEnabledImage(5,middle);button->winSetEnabledImage(6,right);
        button->winGetDrawFunc()(button,button->winGetInstanceData());
        ASSERT_FALSE(manager.draws.empty());
        bool foundLeft=false,foundRight=false;
        for(const auto &r:manager.draws){
            EXPECT_GE(r.x,540);EXPECT_LE(r.endX,748);EXPECT_EQ(r.y,116);EXPECT_EQ(r.endY,152);
            if(r.image==left){foundLeft=true;EXPECT_EQ(r.x,540);EXPECT_EQ(r.endX,548);}
            if(r.image==right){foundRight=true;EXPECT_EQ(r.x,740);EXPECT_EQ(r.endX,748);}
        }
        EXPECT_TRUE(foundLeft);EXPECT_TRUE(foundRight);
        manager.winDestroyAll();EXPECT_EQ(manager.winGetWindowList(),nullptr);
        deleteInstance(left);deleteInstance(middle);deleteInstance(right);
    }
    TheWindowManager=old;TheTransitionHandler=transitions;
}
#endif

// Both architectures exercise the actual producer, virtual manager forwarding,
// callback ABI and UnicodeString copy that failed in the native menu parser.
#include "GameClient/GameWindowManager.h"
#include "W3DDevice/GameClient/W3DGameWindowManager.h"
#include "GameClient/GadgetPushButton.h"
#include <type_traits>
namespace {
UnicodeString receivedButtonLabel;
uintptr_t receivedLabelAddress=0;
WindowMsgHandledType ReceiveButtonLabel(GameWindow *,UnsignedInt msg,WindowMsgData a,WindowMsgData b) {
    if(msg!=GGM_SET_LABEL)return MSG_IGNORED;
    receivedLabelAddress=static_cast<uintptr_t>(a);
    receivedButtonLabel=*reinterpret_cast<const UnicodeString*>(a);
    EXPECT_EQ(b,0u);return MSG_HANDLED;
}
}
TEST(UIProducerTest, ButtonLabelMessagePreservesPointerAcrossActualCallbackABI)
{
#if defined(_WIN64)
    static_assert(sizeof(WindowMsgData)==sizeof(void*),"AMD64 UI pointer payload");
#else
    static_assert(std::is_same<WindowMsgData,UnsignedInt>::value,"Exact Win32 legacy payload");
#endif
    auto *old=TheWindowManager;
    W3DGameWindowManager manager;TheWindowManager=&manager;
    auto *window=manager.allocateNewWindow();window->winSetSystemFunc(ReceiveButtonLabel);
    UnicodeString expected;expected.set(L"Original menu label");
    GadgetButtonSetText(window,expected);
    EXPECT_EQ(receivedButtonLabel,expected);
#if defined(_WIN64)
    EXPECT_GT(receivedLabelAddress,static_cast<uintptr_t>(UINT32_MAX));
    EXPECT_NE(receivedLabelAddress,static_cast<uintptr_t>(static_cast<UnsignedInt>(receivedLabelAddress)));
#endif
    receivedButtonLabel.clear();receivedLabelAddress=0;
    window->winSetSystemFunc(nullptr);deleteInstance(window);TheWindowManager=old;
}
