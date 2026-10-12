// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WW3D2/Backend/D3D11Backend.h"
#include "WW3D2/FontChars.h"
#include "WW3D2/Texture2DCPUContent.h"
#include "WW3D2/Render2DSubmission.h"
#include "WW3D2/shader.h"
#include "WWMath/vector2.h"
#include "WWMath/vector3.h"
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
struct Release { void operator()(const RefCountClass *p) const { p->Release_Ref(); } };
using Content = std::unique_ptr<Texture2DCPUContent, Release>;
using Font = std::unique_ptr<FontCharsClass, Release>;
Texture2DSampler Sampler()
{
    using F = TextureFilterClass;
    return {F::FILTER_TYPE_NONE, F::FILTER_TYPE_NONE, F::FILTER_TYPE_NONE,
            F::TEXTURE_ADDRESS_CLAMP, F::TEXTURE_ADDRESS_CLAMP};
}
ShaderClass Shader()
{
    ShaderClass shader;
    shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
    shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
    shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
    shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA);
    shader.Set_Fog_Func(ShaderClass::FOG_DISABLE);
    shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE);
    shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
    return shader;
}
std::unique_ptr<D3D11Backend> Device()
{ D3D11Backend::Configuration c; c.software = true; return D3D11Backend::Create(c); }
struct Quad {
    ShaderClass shader = Shader();
    // Actual Render2D order: top-left, bottom-left, top-right, bottom-right.
    std::array<Vector2, 4> positions;
    std::array<Vector2, 4> uvs{{{0, 0}, {0, 1}, {1, 0}, {1, 1}}};
    std::array<unsigned long, 4> colors{{0x8040a0e0, 0x8040a0e0, 0x8040a0e0, 0x8040a0e0}};
    std::array<unsigned short, 6> indices{{0, 1, 2, 2, 1, 3}};
    Render2DSubmission s{};
    Quad()
    {
        const float left = 23 - 0.5f, top = 11 - 0.5f;
        auto clip = [](float x, float y) { return Vector2(x * 2 / 128 - 1, 1 - y * 2 / 96); };
        positions = {clip(left, top), clip(left, top + 64), clip(left + 64, top), clip(left + 64, top + 64)};
        s = {nullptr, &shader, positions.data(), uvs.data(), colors.data(), indices.data(),
             4, 6, 0, {0, 0, 128, 96, 0, 1}, false};
    }
};
Content Glyph()
{
    Font font(new FontCharsClass);
    if (!font->Initialize_GDI_Font("Arial", 12, false, nullptr)) throw std::runtime_error("Real GDI font initialization failed");
    Content content(new Texture2DCPUContent(64, 64, 136));
    int pitch; auto *pixels = content->Lock(&pitch);
    std::memset(pixels, 0, pitch * 64);
    font->Blit_Char(L'W', static_cast<uint16 *>(pixels), pitch, 2, 2);
    content->Unlock();
    return content;
}
}

TEST(D3D11SentenceTest, AllARGB4444ValuesConvertExactlyWithoutPremultiplication)
{
    Content content(new Texture2DCPUContent(256, 256, 520));
    int pitch; auto *data = static_cast<unsigned char *>(content->Lock(&pitch));
    for (unsigned v = 0; v < 65536; ++v) {
        const unsigned short value = static_cast<unsigned short>(v);
        std::memcpy(data + (v / 256) * pitch + (v % 256) * 2, &value, 2);
    }
    content->Unlock();
    const auto out = D3D11Backend::Convert_Sentence_RGBA8(*content);
    for (unsigned v = 0; v < 65536; ++v) {
        EXPECT_EQ(out[v * 4], ((v >> 8) & 15) * 17);
        EXPECT_EQ(out[v * 4 + 1], ((v >> 4) & 15) * 17);
        EXPECT_EQ(out[v * 4 + 2], (v & 15) * 17);
        EXPECT_EQ(out[v * 4 + 3], ((v >> 12) & 15) * 17);
    }
}

TEST(D3D11SentenceTest, RealGlyphDrawPlacementColorAlphaAndRepeatedResourceReuse)
{
    auto backend = Device(); auto content = Glyph(); Quad q;
    EXPECT_GE(backend->Feature_Level(), 0xa000u);
    Texture2DResidencyOwner owner;
    auto &residency = owner.Get_Or_Create([&] { return backend->Create_Content_Residency(*content); });
    Texture2DDescription description{};
    ASSERT_TRUE(residency.Describe(description));
    EXPECT_EQ(description.width, 64u); EXPECT_EQ(description.format, WW3D_FORMAT_A4R4G4B4);
    EXPECT_EQ(description.mip_count, 1u); EXPECT_FALSE(description.missing);
    EXPECT_EQ(residency.Map(0), nullptr);
    backend->Clear(true, false, Vector3(0.08f, 0.12f, 0.16f), 0.25f);
    const auto background = backend->Readback_RGBA8();
    backend->Begin_Scene();
    backend->Submit_Render2D_Content(q.s, &residency, Sampler());
    backend->End_Scene(false);
    const auto image = backend->Readback_RGBA8();
    const auto rgba = D3D11Backend::Convert_Sentence_RGBA8(*content);
    unsigned visible = 0;
    for (unsigned y = 0; y < 96; ++y) for (unsigned x = 0; x < 128; ++x) {
        const auto pos = (y * 128 + x) * 4;
        const bool inside = x >= 23 && x < 87 && y >= 11 && y < 75;
        if (!inside) {
            for (int c = 0; c < 4; ++c) EXPECT_EQ(image[pos + c], background[pos + c]);
            continue;
        }
        const unsigned a = rgba[((y - 11) * 64 + x - 23) * 4 + 3];
        const double alpha = (a / 255.0) * (128 / 255.0);
        const double source[] = {64.0, 160.0, 224.0, alpha * 255};
        for (int c = 0; c < 4; ++c) {
            const double expected = source[c] * alpha + background[pos + c] * (1 - alpha);
            EXPECT_NEAR(image[pos + c], expected, 2.0) << "x=" << x << " y=" << y << " channel=" << c;
        }
        if (a) ++visible;
    }
    EXPECT_GT(visible, 20u);
    // GDI may legitimately return binary coverage on the current desktop.
    // Partial vertex alpha is still checked above; all 16 texture alpha values
    // are exercised independently below, without a font-smoothing assumption.
    backend->Clear(true, false, Vector3(0.08f, 0.12f, 0.16f), 0.25f);
    backend->Begin_Scene();
    auto &same = owner.Get_Or_Create([&] { return backend->Create_Content_Residency(*content); });
    EXPECT_EQ(&same, &residency);
    backend->Submit_Render2D_Content(q.s, &same, Sampler());
    backend->End_Scene(false);
    EXPECT_EQ(backend->Readback_RGBA8(), image);
    EXPECT_EQ(backend->Texture_Creations(), 1u);
    EXPECT_EQ(backend->Buffer_Creations(), 2u);
    EXPECT_EQ(backend->Draw_Count(), 2u);
    owner.Reset();
    auto &recreated = owner.Get_Or_Create([&] { return backend->Create_Content_Residency(*content); });
    backend->Begin_Scene(); backend->Submit_Render2D_Content(q.s, &recreated, Sampler()); backend->End_Scene(false);
    EXPECT_EQ(backend->Texture_Creations(), 2u);
    EXPECT_EQ(backend->Buffer_Creations(), 2u);
}

TEST(D3D11SentenceTest, RealGPUAllAlphaLevelsAndImmutableReplacement)
{
    auto backend = Device(); Quad q; q.colors.fill(0xffffffff);
    Content content(new Texture2DCPUContent(16, 1, 40));
    int pitch; auto *p = static_cast<uint16 *>(content->Lock(&pitch));
    for (unsigned a = 0; a < 16; ++a) p[a] = static_cast<uint16>((a << 12) | 0x056a);
    content->Unlock();
    std::unique_ptr<Texture2DResidency> original(backend->Create_Content_Residency(*content));
    auto render = [&](Texture2DResidency *r) {
        backend->Clear(true, false, Vector3(0, 0, 0)); backend->Begin_Scene();
        backend->Submit_Render2D_Content(q.s, r, Sampler()); backend->End_Scene(false);
        return backend->Readback_RGBA8();
    };
    const auto first = render(original.get());
    for (unsigned a = 0; a < 16; ++a) {
        const auto pos = (20 * 128 + 23 + a * 4 + 2) * 4;
        const double alpha = a / 15.0;
        EXPECT_NEAR(first[pos], 85 * alpha, 1);
        EXPECT_NEAR(first[pos + 1], 102 * alpha, 1);
        EXPECT_NEAR(first[pos + 2], 170 * alpha, 1);
        EXPECT_NEAR(first[pos + 3], 255 * alpha * alpha, 1);
    }
    p = static_cast<uint16 *>(content->Lock(&pitch));
    for (unsigned a = 0; a < 16; ++a) p[a] = 0xff00;
    content->Unlock();
    EXPECT_EQ(render(original.get()), first); // CPU edits cannot mutate uploaded immutable content.
    std::unique_ptr<Texture2DResidency> replacement(backend->Create_Content_Residency(*content));
    const auto second = render(replacement.get()); const auto pos = (20 * 128 + 30) * 4;
    EXPECT_EQ(second[pos], 255); EXPECT_EQ(second[pos + 1], 0); EXPECT_EQ(second[pos + 2], 0);
    EXPECT_EQ(backend->Texture_Creations(), 2u); EXPECT_EQ(backend->Buffer_Creations(), 2u);
}

TEST(D3D11SentenceTest, OffsetViewportAndHalfPixelTranslation)
{
    auto backend = Device(); Quad q; q.colors.fill(0xffffffff);
    q.s.viewport = {8, 4, 96, 64, 0, 1};
    const float left = -1 - 1.0f / 96, right = 1 - 1.0f / 96;
    const float top = 1 + 1.0f / 64, bottom = -1 + 1.0f / 64;
    q.positions = {Vector2(left, top), Vector2(left, bottom), Vector2(right, top), Vector2(right, bottom)};
    backend->Clear(true, false, Vector3(0, 0, 0)); backend->Begin_Scene();
    backend->Submit_Render2D(q.s); backend->End_Scene(false);
    const auto image = backend->Readback_RGBA8();
    for (unsigned y = 0; y < 96; ++y) for (unsigned x = 0; x < 128; ++x) {
        const bool inside = x >= 8 && x < 104 && y >= 4 && y < 68;
        EXPECT_EQ(image[(y * 128 + x) * 4], inside ? 255 : 0) << x << "," << y;
    }
}

TEST(D3D11SentenceTest, UnsupportedStatesAndForeignResidencyFailClosed)
{
    auto backend = Device(); auto other = Device(); auto content = Glyph(); Quad q;
    std::unique_ptr<Texture2DResidency> residency(other->Create_Content_Residency(*content));
    EXPECT_THROW(backend->Bind_Content(residency.get(), Sampler()), std::runtime_error);
    auto sampler = Sampler(); sampler.u = TextureFilterClass::TEXTURE_ADDRESS_REPEAT;
    EXPECT_THROW(backend->Bind_Content(nullptr, sampler), std::runtime_error);
    backend->Begin_Scene(); q.s.grayscale = true;
    EXPECT_THROW(backend->Submit_Render2D(q.s), std::runtime_error);
    q.s.grayscale = false; q.shader.Set_Alpha_Test(ShaderClass::ALPHATEST_ENABLE);
    EXPECT_THROW(backend->Submit_Render2D(q.s), std::runtime_error);
    q.shader = Shader(); q.indices[0] = 99;
    EXPECT_THROW(backend->Submit_Render2D(q.s), std::runtime_error);
    q.indices[0] = 0; q.s.viewport.width = 129;
    EXPECT_THROW(backend->Submit_Render2D(q.s), std::runtime_error);
    backend->End_Scene(false);
    EXPECT_EQ(backend->Draw_Count(), 0u);
    EXPECT_THROW(backend->Flip_To_Primary(), std::runtime_error);
    EXPECT_THROW(backend->Clear(false, true, Vector3(0, 0, 0)), std::runtime_error);
}

TEST(D3D11SentenceTest, NullTextureUsesDiffuseAndBackfaceIsCulled)
{
    auto backend = Device(); Quad q;
    q.colors.fill(0xff1234ab);
    backend->Clear(true, false, Vector3(0, 0, 0)); backend->Begin_Scene();
    backend->Submit_Render2D(q.s); backend->End_Scene(false);
    auto image = backend->Readback_RGBA8(); const auto pos = (20 * 128 + 30) * 4;
    EXPECT_EQ(image[pos], 0x12); EXPECT_EQ(image[pos + 1], 0x34); EXPECT_EQ(image[pos + 2], 0xab); EXPECT_EQ(image[pos + 3], 255);
    backend->Clear(true, false, Vector3(0, 0, 0));
    std::swap(q.indices[0], q.indices[1]); std::swap(q.indices[3], q.indices[4]);
    backend->Begin_Scene(); backend->Submit_Render2D(q.s); backend->End_Scene(false);
    image = backend->Readback_RGBA8(); EXPECT_EQ(image[pos], 0); EXPECT_EQ(image[pos + 2], 0);
}

TEST(D3D11SentenceTest, HiddenWindowDXGIPresentResizeAndCleanup)
{
    struct Window {
        HWND handle = CreateWindowExW(0, L"STATIC", L"D3D11 validation", WS_OVERLAPPEDWINDOW,
            0, 0, 192, 128, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        ~Window() { if (handle) DestroyWindow(handle); }
    } window;
    ASSERT_NE(window.handle, nullptr);
    D3D11Backend::Configuration c; c.software = true; c.window = window.handle;
    auto backend = D3D11Backend::Create(c);
    backend->Begin_Scene(); backend->Clear(true, false, Vector3(0.2f, 0.3f, 0.4f), 1);
    backend->End_Scene(true); // real Present; hidden/occluded success is permitted.
    backend->Resize(160, 80);
    backend->Clear(true, false, Vector3(0.2f, 0.3f, 0.4f), 1);
    auto image = backend->Readback_RGBA8(); EXPECT_EQ(image.size(), 160u * 80 * 4);
    EXPECT_NEAR(image[0], 51, 1); EXPECT_NEAR(image[1], 76.5, 1); EXPECT_EQ(image[3], 255);
    backend->Flip_To_Primary();
    EXPECT_THROW(backend->Resize(0, 0), std::runtime_error);
    backend.reset(); // destroy swap chain/device before HWND.
}

TEST(D3D11SentenceTest, DiagnosticBackendReportsDeviceTargetAndSetup) {
    struct Receipt {
        std::vector<std::string> stages;
        static void Log(void *p,const char *stage,const char *status,const char *,unsigned code) {
            EXPECT_EQ(code,0u);
            if(std::string(status)=="complete") static_cast<Receipt *>(p)->stages.emplace_back(stage);
        }
    } receipt;
    D3D11Backend::Configuration c; c.software=true;
    c.diagnostic=Receipt::Log;c.diagnostic_context=&receipt;
    auto backend=D3D11Backend::Create(c);
    EXPECT_EQ(receipt.stages,(std::vector<std::string>{"graphics_device","graphics_target","graphics_setup","graphics_viewport"}));
}
