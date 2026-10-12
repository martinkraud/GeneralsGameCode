// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/Backend/D3D11Backend.h"
#include "WW3D2/UITextureContent.h"
#include "WW3D2/Render2DSubmission.h"
#include "WW3D2/shader.h"
#include "WWMath/vector2.h"
#include "WWMath/vector3.h"
#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <cstring>

namespace {
UITextureBytes Read(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return {UITextureStatus::Ready, {std::istreambuf_iterator<char>(file), {}}};
}
std::vector<unsigned char> TGA(unsigned bytes = 4, unsigned origin = 32, bool rle = false)
{
    std::vector<unsigned char> b(18); b[2] = rle ? 10 : 2;
    b[12] = b[14] = 4; b[16] = bytes * 8; b[17] = origin;
    if (rle) b.push_back(15); // one literal packet
    for (unsigned i = 0; i < 16; ++i) {
        b.push_back(i * 9); b.push_back(i * 5); b.push_back(i * 3);
        if (bytes == 4) b.push_back(i * 17);
    }
    return b;
}
std::vector<unsigned char> DDS(char type = '1')
{
    std::vector<unsigned char> b(type == '1' ? 136 : 144);
    std::memcpy(b.data(), "DDS ", 4);
    b[4] = 124; b[12] = b[16] = 4; b[76] = 32; b[80] = 4;
    std::memcpy(&b[84], "DXT1", 4); b[87] = type;
    return b;
}
UITextureResult Decode(const std::vector<unsigned char> &b, bool dds = false)
{
    return Load_UI_Texture_Content("ui.tga", {}, [&](const std::string &name) {
        if ((name == "ui.dds") != dds) return UITextureBytes{};
        return UITextureBytes{UITextureStatus::Ready, b};
    });
}
ShaderClass Shader()
{
    ShaderClass s;
    s.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE); s.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
    s.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
    s.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA); s.Set_Fog_Func(ShaderClass::FOG_DISABLE);
    s.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE); s.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
    return s;
}
Texture2DSampler UISampler()
{
    using F = TextureFilterClass;
    return {F::FILTER_TYPE_DEFAULT, F::FILTER_TYPE_DEFAULT, F::FILTER_TYPE_NONE,
            F::TEXTURE_ADDRESS_REPEAT, F::TEXTURE_ADDRESS_REPEAT};
}
struct Quad {
    ShaderClass shader = Shader();
    std::array<Vector2, 4> p, uv{{{0, 0}, {0, 1}, {1, 0}, {1, 1}}};
    std::array<unsigned long, 4> color{{0x8040a0e0, 0x8040a0e0, 0x8040a0e0, 0x8040a0e0}};
    std::array<unsigned short, 6> index{{0, 1, 2, 2, 1, 3}};
    Render2DSubmission s{};
    Quad(unsigned w, unsigned h) {
        float left = -1 - 1.0f / w, right = 1 - 1.0f / w;
        float top = 1 + 1.0f / h, bottom = -1 + 1.0f / h;
        p = {Vector2(left, top), Vector2(left, bottom), Vector2(right, top), Vector2(right, bottom)};
        s = {nullptr, &shader, p.data(), uv.data(), color.data(), index.data(), 4, 6, 0, {0, 0, w, h, 0, 1}, false};
    }
};
}

TEST(UITextureTest, TargaRawRLEOriginsAndOpaqueFormat)
{
    for (unsigned bytes : {3u, 4u}) for (unsigned origin : {0u, 16u, 32u, 48u}) for (bool rle : {false, true}) {
        auto result = Decode(TGA(bytes, origin, rle)); ASSERT_EQ(result.status, UITextureStatus::Ready);
        EXPECT_EQ(result.content->Describe().format, bytes == 4 ? WW3D_FORMAT_A8R8G8B8 : WW3D_FORMAT_X8R8G8B8);
        EXPECT_EQ(result.content->Row_Pitch(), 16);
        for (unsigned y = 0; y < 4; ++y) for (unsigned x = 0; x < 4; ++x) {
            unsigned source = ((origin & 32) ? y : 3 - y) * 4 + ((origin & 16) ? 3 - x : x);
            auto *p = result.content->Pixels() + y * 16 + x * 4;
            EXPECT_EQ(p[0], source * 9); EXPECT_EQ(p[1], source * 5); EXPECT_EQ(p[2], source * 3);
            EXPECT_EQ(p[3], bytes == 4 ? source * 17 : 255);
        }
    }
    auto b = TGA(4, 32, true); b.resize(23); b[18] = 143; // repeated packet, 16 pixels
    auto result = Decode(b); ASSERT_EQ(result.status, UITextureStatus::Ready);
    EXPECT_EQ(result.content->Pixels()[63], 0);
}

TEST(UITextureTest, DDSWinsFallbackAndSingleTopMipBytes)
{
    for (char type : {'1', '3', '5'}) {
        auto b = DDS(type); b[28] = 4;
        for (size_t i = 128; i < b.size(); ++i) b[i] = static_cast<unsigned char>(i);
        const auto top = b; b.resize(b.size() + 64, 99); // lower mip bytes must not enter content
        auto result = Decode(b, true); ASSERT_EQ(result.status, UITextureStatus::Ready);
        EXPECT_EQ(result.content->Describe().mip_count, 1u);
        EXPECT_EQ(result.content->Storage_Rows(), 1u);
        EXPECT_EQ(std::memcmp(result.content->Pixels(), top.data() + 128, top.size() - 128), 0);
        EXPECT_EQ(result.content->Map(), nullptr);
        UITextureContent snapshot(result.content->Snapshot());
        EXPECT_EQ(snapshot->Describe().format, result.content->Describe().format);
        EXPECT_EQ(std::memcmp(snapshot->Pixels(), result.content->Pixels(), top.size() - 128), 0);
    }
    std::vector<std::string> calls;
    auto reader = [&](const std::string &name) { calls.push_back(name); return UITextureBytes{UITextureStatus::Ready, name == "ui.dds" ? DDS() : TGA()}; };
    ASSERT_EQ(Load_UI_Texture_Content("ui.tga", {}, reader).status, UITextureStatus::Ready);
    EXPECT_EQ(calls, (std::vector<std::string>{"ui.dds"}));
    calls.clear(); UITexturePolicy policy; policy.allow_compression = false;
    ASSERT_EQ(Load_UI_Texture_Content("ui.tga", policy, reader).status, UITextureStatus::Ready);
    EXPECT_EQ(calls, (std::vector<std::string>{"ui.tga"}));
}

TEST(UITextureTest, AbsentInvalidAndUnsupportedStayDistinct)
{
    EXPECT_EQ(Load_UI_Texture_Content("absent.tga", {}, [](const std::string &) { return UITextureBytes{}; }).status, UITextureStatus::Absent);
    auto b = TGA(); b.pop_back(); EXPECT_EQ(Decode(b).status, UITextureStatus::Invalid);
    b = TGA(); b[12] = 3; EXPECT_EQ(Decode(b).status, UITextureStatus::Unsupported);
    b = TGA(); b[16] = 16; EXPECT_EQ(Decode(b).status, UITextureStatus::Unsupported);
    b = DDS('2'); EXPECT_EQ(Decode(b, true).status, UITextureStatus::Unsupported);
    b = DDS(); b[112] = 2; EXPECT_EQ(Decode(b, true).status, UITextureStatus::Unsupported);
    b = DDS(); b.pop_back(); EXPECT_EQ(Decode(b, true).status, UITextureStatus::Invalid);
    b = DDS(); b[28] = 4; EXPECT_EQ(Decode(b, true).status, UITextureStatus::Invalid);
    b = TGA(4, 32, true); b[18] = 255; EXPECT_EQ(Decode(b).status, UITextureStatus::Invalid);
    UITexturePolicy policy; policy.texture_bits = 8;
    unsigned reads = 0; auto reader = [&](const std::string &) { ++reads; return UITextureBytes{}; };
    EXPECT_EQ(Load_UI_Texture_Content("ui.tga", policy, reader).status, UITextureStatus::Unsupported);
    policy.texture_bits = 32; policy.hsv_shift = true;
    EXPECT_EQ(Load_UI_Texture_Content("ui.tga", policy, reader).status, UITextureStatus::Unsupported);
    EXPECT_EQ(reads, 0u);
}

TEST(UITextureTest, PaddedContentSnapshotMapAndCopyPreserveOwnedBytes)
{
    UITextureContent content(new Texture2DCPUContent(4, 4, WW3D_FORMAT_A8R8G8B8, 24));
    int pitch; void *storage = content->Lock(&pitch); std::memset(storage, 73, pitch * 4); content->Unlock();
    UITextureContent snapshot(content->Snapshot());
    std::array<unsigned char, 80> copied{}; snapshot->Copy_Rows(copied.data(), 20);
    EXPECT_EQ(copied[15], 73); EXPECT_EQ(copied[16], 0); EXPECT_EQ(copied[20], 73);
    Texture2DRegion region{1, 1, 3, 3};
    std::unique_ptr<Texture2DContent> map(content->Map(&region)); ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->Mapping().row_pitch, 24);
    *static_cast<unsigned char *>(map->Mapping().data) = 99; map.reset();
    EXPECT_EQ(content->Pixels()[28], 99); EXPECT_EQ(snapshot->Pixels()[28], 73);
}

TEST(UITextureTest, RealRepositoryUIFileDecodeAndNormalAlphaGPUReadback)
{
    const char *external = std::getenv("D3D11_UI_TEST_ASSET");
    const std::string path = external ? external : D3D11_UI_ASSET;
    const auto file = Read(path); ASSERT_EQ(file.status, UITextureStatus::Ready);
    auto loaded = Load_UI_Texture_Content(path, {}, Read); ASSERT_EQ(loaded.status, UITextureStatus::Ready);
    auto &content = *loaded.content; const auto d = content.Describe();
    ASSERT_EQ(d.format, WW3D_FORMAT_A8R8G8B8);
    // This real file proof uses an uncompressed 32-bit TGA; validate decoding
    // independently against the raw source, not only against decoder output.
    ASSERT_EQ(file.bytes[2], 2); ASSERT_EQ(file.bytes[16], 32);
    for (unsigned y = 0; y < d.height; ++y) for (unsigned x = 0; x < d.width; ++x) {
        unsigned sy = (file.bytes[17] & 32) ? y : d.height - 1 - y;
        unsigned sx = (file.bytes[17] & 16) ? d.width - 1 - x : x;
        EXPECT_EQ(std::memcmp(content.Pixels() + y * content.Row_Pitch() + x * 4,
            file.bytes.data() + 18 + file.bytes[0] + (sy * d.width + sx) * 4, 4), 0);
    }
    D3D11Backend::Configuration config; config.software = true; config.width = d.width; config.height = d.height;
    auto backend = D3D11Backend::Create(config); Quad q(d.width, d.height);
    Texture2DResidencyOwner owner;
    auto &r = owner.Get_Or_Create([&] { return backend->Create_Content_Residency(content); });
    backend->Clear(true, false, Vector3(.08f, .12f, .16f), .25f);
    auto background = backend->Readback_RGBA8();
    auto draw = [&] { backend->Begin_Scene(); backend->Submit_Render2D_Content(q.s, &r, UISampler()); backend->End_Scene(false); };
    draw(); const auto pixels = backend->Readback_RGBA8(); unsigned visible = 0, partial = 0;
    for (unsigned y = 0; y < d.height; ++y) for (unsigned x = 0; x < d.width; ++x) {
        const auto *p = content.Pixels() + y * content.Row_Pitch() + x * 4;
        size_t at = (size_t(y) * d.width + x) * 4;
        double alpha = (p[3] / 255.0) * (128 / 255.0);
        const double source[] = {p[2] * (64 / 255.0), p[1] * (160 / 255.0), p[0] * (224 / 255.0), alpha * 255};
        for (unsigned c = 0; c < 4; ++c)
            EXPECT_NEAR(pixels[at + c], source[c] * alpha + background[at + c] * (1 - alpha), 2) << x << ',' << y << ',' << c;
        if (p[3]) ++visible; if (p[3] && p[3] < 255) ++partial;
    }
    EXPECT_GT(visible, 20u);
    backend->Clear(true, false, Vector3(.08f, .12f, .16f), .25f); draw();
    EXPECT_EQ(backend->Readback_RGBA8(), pixels);
    EXPECT_EQ(backend->Texture_Creations(), 1u); EXPECT_EQ(backend->Buffer_Creations(), 2u);
    owner.Reset();
    auto &replacement = owner.Get_Or_Create([&] { return backend->Create_Content_Residency(content); });
    backend->Begin_Scene(); backend->Submit_Render2D_Content(q.s, &replacement, UISampler()); backend->End_Scene(false);
    EXPECT_EQ(backend->Texture_Creations(), 2u); EXPECT_EQ(backend->Buffer_Creations(), 2u);
    RecordProperty("asset", path); RecordProperty("visible", visible); RecordProperty("partial_alpha", partial);
}

TEST(UITextureTest, CompressedTopMipActuallyRendersWithoutReinterpretation)
{
    for (char type : {'1', '3', '5'}) {
        auto b = DDS(type); unsigned offset = type == '1' ? 128 : 136;
        if (type == '3') std::memset(b.data() + 128, 0x88, 8);
        if (type == '5') b[128] = 128;
        b[offset] = 0; b[offset + 1] = 0xf8; // red 565 endpoint, all selectors zero
        auto result = Decode(b, true); ASSERT_EQ(result.status, UITextureStatus::Ready);
        D3D11Backend::Configuration c; c.software = true; c.width = c.height = 4;
        auto backend = D3D11Backend::Create(c); Quad q(4, 4); q.color.fill(0xffffffff);
        std::unique_ptr<Texture2DResidency> r(backend->Create_Content_Residency(*result.content));
        backend->Clear(true, false, Vector3(0, 0, 0)); backend->Begin_Scene();
        backend->Submit_Render2D_Content(q.s, r.get(), UISampler()); backend->End_Scene(false);
        const auto pixels = backend->Readback_RGBA8();
        const double alpha = type == '1' ? 1.0 : (type == '3' ? 136.0 : 128.0) / 255;
        EXPECT_NEAR(pixels[0], 255 * alpha, 1); EXPECT_EQ(pixels[1], 0);
        EXPECT_NEAR(pixels[3], 255 * alpha * alpha, 1);
    }
}

TEST(UITextureTest, UIRepeatAndGlobalPointLinearIntentArePreserved)
{
    auto b = TGA(3); auto result = Decode(b); ASSERT_EQ(result.status, UITextureStatus::Ready);
    D3D11Backend::Configuration c; c.software = true; c.width = c.height = 4;
    auto backend = D3D11Backend::Create(c); Quad q(4, 4); q.color.fill(0xffffffff);
    std::unique_ptr<Texture2DResidency> r(backend->Create_Content_Residency(*result.content));
    for (auto mode : {TextureFilterClass::TEXTURE_FILTER_POINT, TextureFilterClass::TEXTURE_FILTER_BILINEAR,
                      TextureFilterClass::TEXTURE_FILTER_TRILINEAR}) {
        backend->Set_UI_Filter_Policy(mode, 2);
        std::vector<unsigned char> first;
        for (float uv : {.25f, 1.25f}) {
            q.uv.fill(Vector2(uv, uv));
            backend->Clear(true, false, Vector3(0, 0, 0)); backend->Begin_Scene();
            backend->Submit_Render2D_Content(q.s, r.get(), UISampler()); backend->End_Scene(false);
            const auto pixels = backend->Readback_RGBA8();
            EXPECT_NEAR(pixels[0], mode == TextureFilterClass::TEXTURE_FILTER_POINT ? 15 : 7.5, 1);
            EXPECT_NEAR(pixels[1], mode == TextureFilterClass::TEXTURE_FILTER_POINT ? 25 : 12.5, 1);
            EXPECT_NEAR(pixels[2], mode == TextureFilterClass::TEXTURE_FILTER_POINT ? 45 : 22.5, 1);
            EXPECT_EQ(pixels[3], 255);
            if (first.empty()) first = pixels; else EXPECT_EQ(first, pixels);
        }
    }
    EXPECT_EQ(backend->Texture_Creations(), 1u); EXPECT_EQ(backend->Buffer_Creations(), 2u);
}

TEST(UITextureTest, EightBitAlphaAndPaddedUploadRetainStraightAlpha)
{
    auto b = TGA(); auto result = Decode(b); ASSERT_EQ(result.status, UITextureStatus::Ready);
    UITextureContent padded(new Texture2DCPUContent(4, 4, WW3D_FORMAT_A8R8G8B8, 24));
    int pitch; auto *storage = padded->Lock(&pitch); result.content->Copy_Rows(storage, pitch); padded->Unlock();
    D3D11Backend::Configuration c; c.software = true; c.width = c.height = 4;
    auto backend = D3D11Backend::Create(c); Quad q(4, 4); q.color.fill(0xffffffff);
    std::unique_ptr<Texture2DResidency> r(backend->Create_Content_Residency(*padded));
    backend->Clear(true, false, Vector3(0, 0, 0)); backend->Begin_Scene();
    backend->Submit_Render2D_Content(q.s, r.get(), UISampler()); backend->End_Scene(false);
    const auto pixels = backend->Readback_RGBA8();
    for (unsigned i = 0; i < 16; ++i) {
        double alpha = i / 15.0;
        EXPECT_NEAR(pixels[i * 4], i * 3 * alpha, 1);
        EXPECT_NEAR(pixels[i * 4 + 1], i * 5 * alpha, 1);
        EXPECT_NEAR(pixels[i * 4 + 2], i * 9 * alpha, 1);
        EXPECT_NEAR(pixels[i * 4 + 3], 255 * alpha * alpha, 1);
    }
}

TEST(UITextureTest, ActualSixteenBitReferencePolicyQuantizesBeforeUpload)
{
    for (unsigned bytes : {3u, 4u}) {
        const auto b = TGA(bytes); UITexturePolicy policy; policy.texture_bits = 16; policy.allow_compression = false;
        auto result = Load_UI_Texture_Content("ui.tga", policy, [&](const std::string &) { return UITextureBytes{UITextureStatus::Ready, b}; });
        ASSERT_EQ(result.status, UITextureStatus::Ready);
        EXPECT_EQ(result.content->Describe().format, bytes == 4 ? WW3D_FORMAT_A4R4G4B4 : WW3D_FORMAT_R5G6B5);
        for (unsigned i = 0; i < 16; ++i) {
            const auto *s = b.data() + 18 + i * bytes; unsigned short value;
            std::memcpy(&value, result.content->Pixels() + i * 2, 2);
            const unsigned expected = bytes == 4 ? ((s[3] & 0xf0) << 8) | ((s[2] & 0xf0) << 4) | (s[1] & 0xf0) | (s[0] >> 4) :
                                                  ((s[2] >> 3) << 11) | ((s[1] >> 2) << 5) | (s[0] >> 3);
            EXPECT_EQ(value, expected);
        }
        D3D11Backend::Configuration c; c.software = true; c.width = c.height = 4;
        auto backend = D3D11Backend::Create(c); Quad q(4, 4); q.color.fill(0xffffffff);
        std::unique_ptr<Texture2DResidency> r(backend->Create_Content_Residency(*result.content));
        backend->Clear(true, false, Vector3(0, 0, 0)); backend->Begin_Scene();
        backend->Submit_Render2D_Content(q.s, r.get(), UISampler()); backend->End_Scene(false);
        const auto pixels = backend->Readback_RGBA8();
        // Bottom-right source pixel has R=45 G=75 B=135, A=255. The reference
        // truncates before sampling; RGBA8 upload preserves the quantized intent.
        if (bytes == 4) { EXPECT_EQ(pixels[60], 34); EXPECT_EQ(pixels[61], 68); EXPECT_EQ(pixels[62], 136); }
        else { EXPECT_NEAR(pixels[60], 41, 1); EXPECT_NEAR(pixels[61], 73, 1); EXPECT_NEAR(pixels[62], 132, 1); }
        EXPECT_EQ(pixels[63], 255);
    }
}

TEST(UITextureTest, MenuRectangleAtlasCropBlendsOverDiagnosticClear)
{
    // Synthetic atlas only: original Generals MainMenu.wnd logo rectangle.
    // Existing TGA decoder + actual D3D11 UI shader, with CPU readback.
    auto b=TGA();
    for(unsigned i=0;i<16;++i) {
        b[18+i*4]=0; b[19+i*4]=0; b[20+i*4]=255; b[21+i*4]=128;
    }
    auto content=Decode(b); ASSERT_EQ(content.status,UITextureStatus::Ready);
    D3D11Backend::Configuration c; c.software=true; c.width=800; c.height=600;
    auto backend=D3D11Backend::Create(c);
    Quad q(800,600); q.color.fill(0xffffffff);
    const float left=2.f*504/800-1-1.f/800, right=2.f*791/800-1-1.f/800;
    const float top=1-2.f*16/600+1.f/600, bottom=1-2.f*110/600+1.f/600;
    q.p={Vector2(left,top),Vector2(left,bottom),Vector2(right,top),Vector2(right,bottom)};
    q.uv={Vector2(.25f,.25f),Vector2(.25f,.75f),Vector2(.75f,.25f),Vector2(.75f,.75f)};
    std::unique_ptr<Texture2DResidency> r(backend->Create_Content_Residency(*content.content));
    backend->Begin_Scene(); backend->Clear(true,false,Vector3(.08f,.32f,.55f));
    backend->Submit_Render2D_Content(q.s,r.get(),UISampler()); backend->End_Scene(false);
    const auto pixels=backend->Readback_RGBA8();
    const auto outside=(300*800+400)*4, inside=(60*800+650)*4;
    EXPECT_NEAR(pixels[outside],20,1); EXPECT_NEAR(pixels[outside+1],82,1);
    EXPECT_NEAR(pixels[outside+2],140,1);
    EXPECT_NEAR(pixels[inside],138,1); EXPECT_NEAR(pixels[inside+1],41,1);
    EXPECT_NEAR(pixels[inside+2],70,1);
    EXPECT_EQ(backend->Texture_Creations(),1u);
    r.reset(); // residency released while backend is alive
}

TEST(UITextureTest, LiveResidencyCountTracksOwnershipAndSurvivesNoBackendLeak)
{
    // The ordinary-host profile asserts this count is zero before the backend is
    // destroyed; the counter itself must follow create/replace/destroy exactly.
    const unsigned baseline = D3D11Backend::Live_Residencies();
    D3D11Backend::Configuration c; c.software = true; c.width = c.height = 4;
    {
        auto backend = D3D11Backend::Create(c);
        UITextureContent content(new Texture2DCPUContent(4, 4, WW3D_FORMAT_A8R8G8B8, 16));
        std::unique_ptr<Texture2DResidency> first(backend->Create_Content_Residency(*content));
        EXPECT_EQ(D3D11Backend::Live_Residencies(), baseline + 1);
        std::unique_ptr<Texture2DResidency> second(backend->Create_Content_Residency(*content));
        EXPECT_EQ(D3D11Backend::Live_Residencies(), baseline + 2);
        first.reset();
        EXPECT_EQ(D3D11Backend::Live_Residencies(), baseline + 1);
        Texture2DResidencyOwner owner;
        owner.Get_Or_Create([&] { return backend->Create_Content_Residency(*content); });
        EXPECT_EQ(D3D11Backend::Live_Residencies(), baseline + 2);
        owner.Reset();
        second.reset();
        EXPECT_EQ(D3D11Backend::Live_Residencies(), baseline);
    }
    EXPECT_EQ(D3D11Backend::Live_Residencies(), baseline);
}

// Interactive-menu Render2D states: additive, opaque, flat variants and the
// disabled-button grayscale combiner. Exact, not approximate, product contracts.
namespace {
struct StateFixture {
    std::unique_ptr<D3D11Backend> backend;
    std::unique_ptr<Texture2DResidency> residency;
    Quad quad{4, 4};
    StateFixture() {
        auto result = Decode(TGA());
        D3D11Backend::Configuration c; c.software = true; c.width = c.height = 4;
        backend = D3D11Backend::Create(c);
        residency.reset(backend->Create_Content_Residency(*result.content));
        quad.color.fill(0xffffffff);
    }
    std::vector<unsigned char> Textured(const ShaderClass &shader, bool gray = false) {
        quad.shader = shader; quad.s.grayscale = gray;
        backend->Clear(true, false, Vector3(0.2f, 0.4f, 0.6f)); backend->Begin_Scene();
        backend->Submit_Render2D_Content(quad.s, residency.get(), UISampler());
        backend->End_Scene(false);
        return backend->Readback_RGBA8();
    }
    std::vector<unsigned char> Flat(const ShaderClass &shader, unsigned long color) {
        quad.shader = shader; quad.s.grayscale = false; quad.color.fill(color);
        backend->Clear(true, false, Vector3(0.2f, 0.4f, 0.6f)); backend->Begin_Scene();
        backend->Submit_Render2D(quad.s);
        backend->End_Scene(false);
        return backend->Readback_RGBA8();
    }
};
ShaderClass With(ShaderClass base, ShaderClass::SrcBlendFuncType src, ShaderClass::DstBlendFuncType dst) {
    base.Set_Src_Blend_Func(src); base.Set_Dst_Blend_Func(dst); return base;
}
ShaderClass Flat(ShaderClass base) { base.Set_Texturing(ShaderClass::TEXTURING_DISABLE); return base; }
}

TEST(UIStateTest, TexturedOpaqueAndAdditiveMatchLegacyBlendEquations)
{
    StateFixture f;
    const auto opaque = f.Textured(With(Shader(), ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ZERO));
    const auto additive = f.Textured(With(Shader(), ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ONE));
    for (unsigned i = 0; i < 16; ++i) {
        // Opaque: source replaces destination irrespective of texture alpha.
        EXPECT_NEAR(opaque[i * 4], i * 3, 1); EXPECT_NEAR(opaque[i * 4 + 1], i * 5, 1);
        EXPECT_NEAR(opaque[i * 4 + 2], i * 9, 1);
        // Additive: destination + texture*vertex colour, saturated.
        EXPECT_NEAR(additive[i * 4], (std::min)(255u, 51u + i * 3), 1);
        EXPECT_NEAR(additive[i * 4 + 1], (std::min)(255u, 102u + i * 5), 1);
        EXPECT_NEAR(additive[i * 4 + 2], (std::min)(255u, 153u + i * 9), 1);
    }
}

TEST(UIStateTest, GrayscaleIsLegacyLuminanceCombinerOpaqueIgnoringVertexRGB)
{
    StateFixture f;
    f.quad.color.fill(0xff00ff00); // vertex RGB must not tint the legacy DOT3 result
    const auto gray = f.Textured(Shader(), true);
    for (unsigned i = 0; i < 16; ++i) {
        const double g = i * 3 * 0.294118 + i * 5 * 0.584314 + i * 9 * 0.113725;
        for (unsigned c = 0; c < 3; ++c) EXPECT_NEAR(gray[i * 4 + c], g, 1.0) << i;
    }
}

TEST(UIStateTest, FlatAlphaAdditiveAndOpaqueVariants)
{
    StateFixture f;
    const unsigned long color = 0x80102030; // A=0x80 R=0x10 G=0x20 B=0x30
    const auto alpha = f.Flat(Flat(Shader()), color);
    const auto additive = f.Flat(Flat(With(Shader(), ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ONE)), color);
    const auto opaque = f.Flat(Flat(With(Shader(), ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ZERO)), color);
    const double a = 0x80 / 255.0;
    EXPECT_NEAR(alpha[0], 16 * a + 51 * (1 - a), 1.5);
    EXPECT_NEAR(alpha[1], 32 * a + 102 * (1 - a), 1.5);
    EXPECT_NEAR(alpha[2], 48 * a + 153 * (1 - a), 1.5);
    EXPECT_NEAR(additive[0], 51 + 16, 1); EXPECT_NEAR(additive[1], 102 + 32, 1); EXPECT_NEAR(additive[2], 153 + 48, 1);
    EXPECT_NEAR(opaque[0], 16, 1); EXPECT_NEAR(opaque[1], 32, 1); EXPECT_NEAR(opaque[2], 48, 1);
}

TEST(UIStateTest, UnrelatedStatesStillFailExplicitly)
{
    StateFixture f;
    // Premultiplied-style and depth-tested states are not part of the UI set.
    EXPECT_THROW(f.Textured(With(Shader(), ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA)),
                 std::runtime_error);
    ShaderClass depth = Shader(); depth.Set_Depth_Compare(ShaderClass::PASS_LEQUAL);
    EXPECT_THROW(f.Textured(depth), std::runtime_error);
    // Grayscale requires the normal textured Render2D shader.
    EXPECT_THROW(f.Textured(With(Shader(), ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ONE), true), std::runtime_error);
}
