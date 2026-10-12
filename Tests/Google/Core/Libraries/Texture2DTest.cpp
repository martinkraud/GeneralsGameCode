// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <array>
#include <string>
#include <vector>
#include "WW3D2/Texture2D.h"

namespace
{
using Filter = TextureFilterClass;
struct Policy
{
    Texture2DSampler value{Filter::FILTER_TYPE_DEFAULT, Filter::FILTER_TYPE_FAST,
                          Filter::FILTER_TYPE_NONE, Filter::TEXTURE_ADDRESS_CLAMP, Filter::TEXTURE_ADDRESS_REPEAT};
    Filter::FilterType Get_Min_Filter() const { return value.min; }
    Filter::FilterType Get_Mag_Filter() const { return value.mag; }
    Filter::FilterType Get_Mip_Mapping() const { return value.mip; }
    Filter::TxtAddrMode Get_U_Addr_Mode() const { return value.u; }
    Filter::TxtAddrMode Get_V_Addr_Mode() const { return value.v; }
};
struct Texture
{
    Policy filter;
    bool initialized = false;
    unsigned loads = 0;
    std::vector<std::string> *events;
    void Prepare_Render2D_Texture()
    {
        events->push_back("prepare");
        if (!initialized) { initialized = true; ++loads; events->push_back("load"); }
    }
    Policy &Get_Filter() { return filter; }
};
struct Resource : Texture2DResidency
{
    // Simulates replacement/invalidation of backend storage, deliberately with
    // padded rows and byte values that exercise A4R4G4B4 alpha/content identity.
    std::array<unsigned char, 24> pixels{};
    bool available = true, missing = false, locked = false;
    struct Content : Texture2DContent
    {
        Resource &r;
        void *data;
        Content(Resource &resource, void *start) : r(resource), data(start) { r.locked = true; }
        ~Content() override { r.locked = false; }
        Texture2DMapping Mapping() const override { return {data, 12}; }
    };
    bool Describe(Texture2DDescription &out) override
    {
        if (!available) return false;
        out = {4, 2, 1, WW3D_FORMAT_A4R4G4B4, missing}; return true;
    }
    Texture2DContent *Map(unsigned mip, const Texture2DRegion *region) override
    {
        if (!available || mip != 0) return nullptr;
        return new Content(*this, pixels.data() + (region ? region->top * 12 + region->left * 2 : 0));
    }
};
struct Recorder
{
    std::vector<std::string> events;
    Resource resource;
    Texture2DResidency *bound = nullptr;
    unsigned stage = 999, ensures = 0;
    bool enabled = true;
    bool Texturing_Enabled() { return enabled; }
    Texture2DSampler sampler{};
    void Record(Texture &) { events.push_back("record"); }
    Texture2DResidency &Ensure_Residency(Texture &) { ++ensures; events.push_back("ensure"); return resource; }
    void Bind(unsigned s, Texture2DResidency &r) { stage = s; bound = &r; events.push_back("bind"); }
    void Bind_Null(unsigned s) { stage = s; bound = nullptr; events.push_back("null"); }
    void Apply_Sampler(unsigned s, const Texture2DSampler &intent)
    { stage = s; sampler = intent; events.push_back("sampler"); }
};
}

TEST(Texture2DTest, FirstUseAndRepeatedApplicationRetainIdentityWithoutDuplicateLoad)
{
    Recorder r;
    Texture t{}; t.events = &r.events;
    Execute_Texture2D(r, &t, 0);
    EXPECT_EQ(r.events, (std::vector<std::string>{"prepare", "load", "record", "ensure", "bind", "sampler"}));
    auto *identity = r.bound;
    r.events.clear();
    Execute_Texture2D(r, &t, 0);
    EXPECT_EQ(r.events, (std::vector<std::string>{"prepare", "record", "ensure", "bind", "sampler"}));
    EXPECT_EQ(t.loads, 1u);
    EXPECT_EQ(r.bound, identity);
}

TEST(Texture2DTest, NullDoesNotInitializeOrChangeSampler)
{
    Recorder r;
    Execute_Texture2D(r, static_cast<Texture *>(nullptr), 0);
    EXPECT_EQ(r.events, (std::vector<std::string>{"null"}));
    EXPECT_EQ(r.ensures, 0u);
    EXPECT_EQ(r.bound, nullptr);
}

TEST(Texture2DTest, TexturingDisabledStillPreparesRecordsAndAppliesIntent)
{
    Recorder r;
    Texture t{}; t.events = &r.events;
    r.enabled = false;
    Execute_Texture2D(r, &t, 0);
    EXPECT_EQ(r.events, (std::vector<std::string>{"prepare", "load", "record", "ensure", "null", "sampler"}));
    EXPECT_EQ(r.bound, nullptr);
    EXPECT_EQ(r.sampler.min, Filter::FILTER_TYPE_DEFAULT);
    EXPECT_EQ(r.sampler.mag, Filter::FILTER_TYPE_FAST);
    EXPECT_EQ(r.sampler.mip, Filter::FILTER_TYPE_NONE);
    EXPECT_EQ(r.sampler.u, Filter::TEXTURE_ADDRESS_CLAMP);
    EXPECT_EQ(r.sampler.v, Filter::TEXTURE_ADDRESS_REPEAT);
}

TEST(Texture2DTest, ContentLeasePreservesPitchedBytesAndRegionUpdates)
{
    Resource r;
    r.pixels.fill(0xcd);
    Texture2DDescription desc{};
    ASSERT_TRUE(r.Describe(desc));
    EXPECT_EQ(desc.width, 4u); EXPECT_EQ(desc.height, 2u); EXPECT_EQ(desc.mip_count, 1u);
    EXPECT_EQ(desc.format, WW3D_FORMAT_A4R4G4B4);
    const Texture2DRegion region{1, 1, 3, 2};
    auto *lease = r.Map(0, &region);
    ASSERT_NE(lease, nullptr);
    EXPECT_TRUE(r.locked);
    EXPECT_EQ(lease->Mapping().row_pitch, 12);
    auto *bytes = static_cast<unsigned char *>(lease->Mapping().data);
    bytes[0] = 0x7f; bytes[1] = 0xa4; bytes[2] = 0x19; bytes[3] = 0x01;
    delete lease;
    EXPECT_FALSE(r.locked);
    for (unsigned i = 0; i < r.pixels.size(); ++i)
        if (i < 14 || i >= 18) EXPECT_EQ(r.pixels[i], 0xcd);
    lease = r.Map(0, nullptr);
    ASSERT_NE(lease, nullptr);
    EXPECT_EQ(static_cast<unsigned char *>(lease->Mapping().data)[14], 0x7f);
    EXPECT_EQ(static_cast<unsigned char *>(lease->Mapping().data)[15], 0xa4);
    delete lease;
    EXPECT_EQ(r.Map(1, nullptr), nullptr);
}

TEST(Texture2DTest, MissingAndInvalidatedStorageAreDistinctFromNullLogicalBinding)
{
    Recorder r;
    Texture t{}; t.events = &r.events;
    r.resource.missing = true;
    Execute_Texture2D(r, &t, 0);
    Texture2DDescription desc{};
    ASSERT_TRUE(r.bound->Describe(desc)); EXPECT_TRUE(desc.missing);
    r.resource.available = false;
    EXPECT_FALSE(r.bound->Describe(desc));
    EXPECT_EQ(r.bound->Map(0), nullptr);
    t.initialized = false;
    r.resource.available = true; r.resource.missing = false;
    Execute_Texture2D(r, &t, 0);
    EXPECT_EQ(t.loads, 2u);
    ASSERT_TRUE(r.bound->Describe(desc)); EXPECT_FALSE(desc.missing);
}

TEST(Texture2DTest, SamplerApplicationOrderAndPolicyAreNotDeviceEnums)
{
    struct Commands
    {
        std::vector<std::string> events;
        void Min(unsigned s, Filter::FilterType v) { EXPECT_EQ(s, 0u); EXPECT_EQ(v, Filter::FILTER_TYPE_DEFAULT); events.push_back("min"); }
        void Mag(unsigned, Filter::FilterType v) { EXPECT_EQ(v, Filter::FILTER_TYPE_FAST); events.push_back("mag"); }
        void Mip(unsigned, Filter::FilterType v) { EXPECT_EQ(v, Filter::FILTER_TYPE_NONE); events.push_back("mip"); }
        void U(unsigned, Filter::TxtAddrMode v) { EXPECT_EQ(v, Filter::TEXTURE_ADDRESS_CLAMP); events.push_back("u"); }
        void V(unsigned, Filter::TxtAddrMode v) { EXPECT_EQ(v, Filter::TEXTURE_ADDRESS_REPEAT); events.push_back("v"); }
    } commands;
    Execute_Texture2D_Sampler(commands, 0, Policy{}.value);
    EXPECT_EQ(commands.events, (std::vector<std::string>{"min", "mag", "mip", "u", "v"}));
}

TEST(Texture2DTest, ActualResidencyOwnerCreatesOneViewAndReleasesExactlyOnce)
{
    struct OwnedResource : Resource
    {
        unsigned &releases;
        explicit OwnedResource(unsigned &count) : releases(count) {}
        ~OwnedResource() override { ++releases; }
    };
    unsigned creates = 0, releases = 0;
    {
        Texture2DResidencyOwner owner;
        EXPECT_FALSE(owner.Has_Residency());
        auto factory = [&]() -> Texture2DResidency * { ++creates; return new OwnedResource(releases); };
        auto *first = &owner.Get_Or_Create(factory);
        EXPECT_EQ(&owner.Get_Or_Create(factory), first);
        EXPECT_EQ(creates, 1u);
        EXPECT_EQ(releases, 0u);
        EXPECT_TRUE(owner.Has_Residency());
        owner.Reset();
        EXPECT_FALSE(owner.Has_Residency());
        EXPECT_EQ(releases, 1u);
        owner.Reset();
        EXPECT_EQ(releases, 1u);
        owner.Get_Or_Create(factory);
        EXPECT_EQ(creates, 2u);
    }
    EXPECT_EQ(releases, 2u);
}

#if !defined(DISABLE_GAMEMEMORY)
// Engine-linked test: unlike isolated GPU tests, the production named memory
// pools are enabled. First allocation previously threw ERROR_OUT_OF_MEMORY.
#include "WW3D2/Texture2DCPUContent.h"
#include "WW3D2/UITextureContent.h"
#include "Common/Errors.h"
TEST(Texture2DTest, ProductionCPUContentPoolAllocatesSnapshotsAndReusesBlocks)
{
    // The same raw32-bit TGA decoder route reached by the original menu atlas.
    std::vector<unsigned char> tga(18+4*4*4,0);
    tga[2]=2; tga[12]=tga[14]=4; tga[16]=32;
    UITexturePolicy policy; policy.allow_compression=false;
    auto decoded=[&]() {
        try {
            return Load_UI_Texture_Content("pool.tga",policy,[&](const std::string &) {
                return UITextureBytes{UITextureStatus::Ready,tga};
            });
        } catch(ErrorCode code) {
            ADD_FAILURE() << "Production TGA allocation threw engine error " << static_cast<unsigned>(code);
            throw;
        }
    }();
    ASSERT_EQ(decoded.status,UITextureStatus::Ready);
    for(unsigned iteration=0;iteration<2;++iteration) {
        std::vector<Texture2DCPUContent *> contents;
        try {
            for(unsigned i=0;i<40;++i) {
                contents.push_back(new Texture2DCPUContent(4,4));
                int pitch; auto *bytes=static_cast<unsigned char *>(contents.back()->Lock(&pitch));
                for(int b=0;b<pitch*4;++b) bytes[b]=0x5a;
                contents.back()->Unlock();
            }
            auto *copy=contents[0]->Snapshot();
            EXPECT_EQ(copy->Describe().width,4u);
            EXPECT_EQ(copy->Pixels()[0],0x5a);
            copy->Release_Ref();
        } catch (...) {
            for(auto *content:contents) content->Release_Ref();
            throw;
        }
        for(auto *content:contents) content->Release_Ref();
    }
}
#endif
