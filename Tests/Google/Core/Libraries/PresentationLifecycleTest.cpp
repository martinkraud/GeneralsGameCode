// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/PresentationLifecycle.h"
#include "WW3D2/Backend/D3D11Backend.h"
#include "WWMath/vector3.h"
#include <stdexcept>
#include <string>
#include <vector>
namespace {
// Deliberately rejects every unsupported operation. Only the neutral lifecycle
// methods are implemented; no simulated game/legacy graphics device is used.
struct Backend final : IRenderBackend {
    std::vector<std::string> &calls; bool failBegin=false,failEnd=false;
    explicit Backend(std::vector<std::string>&value):calls(value){}
    ~Backend(){calls.push_back("destroy");}
    void Set_Viewport(const RenderBackendViewport &v) override {EXPECT_EQ(v.width,800u); EXPECT_EQ(v.height,600u); calls.push_back("viewport");}
    void Clear(bool c,bool z,const Vector3 &,float,float,unsigned) override {EXPECT_TRUE(c);EXPECT_FALSE(z);calls.push_back("clear");}
    void Begin_Scene() override {calls.push_back("begin");if(failBegin)throw std::runtime_error("begin");}
    void End_Scene(bool p) override {EXPECT_TRUE(p);calls.push_back("end");if(failEnd)throw std::runtime_error("present");}
    void Invalidate_Cached_Render_States() override {calls.push_back("invalidate");}
    void Unsupported(){throw std::runtime_error("unsupported");}
    void Set_Gamma(float,float,float,bool,bool) override {Unsupported();}
    void Flip_To_Primary() override {Unsupported();}
    void Set_Ambient(const Vector3&) override {Unsupported();}
    void Set_Light_Environment(LightEnvironmentClass*) override {Unsupported();}
    void Submit_Render2D(const Render2DSubmission&) override {Unsupported();}
    Texture2DResidency *Create_Render2D_Residency(TextureClass&) override {Unsupported();return nullptr;}
    void Apply_Render2D_Texture(TextureClass&,unsigned) override {Unsupported();}
    void Copy_Render2D_Surface(SurfaceClass&,SurfaceClass&) override {Unsupported();}
};
PresentationStatus Status(IRenderBackend&){return {0x1234,0x5678,true};}
}
TEST(PresentationLifecycle, OrderedPairingStatusAndOwnership) {
    std::vector<std::string> calls;
    {
        PresentationLifecycle life(std::make_unique<Backend>(calls),{800,600},Status);
        EXPECT_EQ(life.Size().width,800u); EXPECT_EQ(life.Size().height,600u);
        EXPECT_EQ(life.Status().result,0x1234u); EXPECT_EQ(life.Status().removed_reason,0x5678u); EXPECT_TRUE(life.Status().occluded);
        EXPECT_THROW(life.End(true),std::runtime_error);
        EXPECT_THROW(life.Begin(true,true,Vector3(0,0,0),1),std::runtime_error);
        EXPECT_TRUE(calls.empty());
        for(unsigned i=0;i<2;++i){life.Begin(true,false,Vector3(0,0,0),1);EXPECT_THROW(life.Begin(true,false,Vector3(0,0,0),1),std::runtime_error);life.End(true);}
        EXPECT_EQ(life.Frames(),2u);
    }
    EXPECT_EQ(calls,(std::vector<std::string>{"viewport","clear","begin","end","invalidate","viewport","clear","begin","end","invalidate","destroy"}));
}
TEST(PresentationLifecycle, FailuresDoNotCountFramesAndReleaseOwnedBackend) {
    std::vector<std::string> calls;
    {
        auto value=std::make_unique<Backend>(calls);auto *backend=value.get();backend->failBegin=true;
        PresentationLifecycle life(std::move(value),{800,600},Status);
        EXPECT_THROW(life.Begin(true,false,Vector3(0,0,0),1),std::runtime_error);
        EXPECT_THROW(life.End(true),std::runtime_error);
        backend->failBegin=false;life.Begin(true,false,Vector3(0,0,0),1);backend->failEnd=true;
        EXPECT_THROW(life.End(true),std::runtime_error);EXPECT_EQ(life.Frames(),0u);
        EXPECT_THROW(life.End(true),std::runtime_error);
    }
    EXPECT_EQ(calls.back(),"destroy");
}
TEST(PresentationLifecycle, InvalidConfigurationStillReleasesOwnership) {
    std::vector<std::string> calls;
    EXPECT_THROW((PresentationLifecycle(std::make_unique<Backend>(calls),{0,600},Status)),std::runtime_error);
    EXPECT_EQ(calls,(std::vector<std::string>{"destroy"}));
    EXPECT_THROW((PresentationLifecycle(nullptr,{800,600},Status)),std::runtime_error);
    EXPECT_THROW((PresentationLifecycle(std::make_unique<Backend>(calls),{800,600},nullptr)),std::runtime_error);
}
TEST(PresentationLifecycle, ActualWARPColorReadbackAcrossNeutralFrame) {
    D3D11Backend::Configuration config;config.software=true;config.width=16;config.height=8;
    auto value=D3D11Backend::Create(config);auto *backend=value.get();
    PresentationLifecycle life(std::move(value),{16,8},[](IRenderBackend&b){return static_cast<D3D11Backend&>(b).Last_Presentation();});
    life.Begin(true,false,Vector3(0.08f,0.32f,0.55f),1);life.End(false);
    auto pixels=backend->Readback_RGBA8(); ASSERT_EQ(pixels.size(),16u*8*4);
    for(size_t i=0;i<pixels.size();i+=4){EXPECT_NEAR(pixels[i],20,1);EXPECT_NEAR(pixels[i+1],82,1);EXPECT_NEAR(pixels[i+2],140,1);EXPECT_EQ(pixels[i+3],255);}
    EXPECT_EQ(life.Frames(),1u);
}
