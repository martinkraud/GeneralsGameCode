// SPDX-License-Identifier: GPL-3.0-or-later
// R1b/R2 foundations: CPU-backed legacy vertex/index buffers under the native
// presentation, and the renderer-neutral terrain atlas build.
#include <gtest/gtest.h>
#if defined(_WIN64)
#include <windows.h>
#include "WW3D2/TerrainAtlas.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/PresentationLifecycle.h"
#include "WW3D2/Backend/D3D11Backend.h"
#include <memory>

TEST(TerrainAtlasTest, TilesAreInvertedQuantizedAndBorderedLikeTheLegacyUpdate)
{
    const int extent = 4, border = 1;           // small stand-ins for 64 / 4
    // Two rows of the tile: row 0 (bottom in legacy data) is red-ish, row 3 blue-ish.
    std::vector<unsigned char> tile(extent * extent * 4);
    for (int r = 0; r < extent; ++r)
        for (int c = 0; c < extent; ++c) {
            unsigned char *p = &tile[(r * extent + c) * 4];
            p[0] = static_cast<unsigned char>(r * 40);        // B
            p[1] = static_cast<unsigned char>(c * 20);        // G
            p[2] = static_cast<unsigned char>(255 - r * 40);  // R
            p[3] = 0;
        }
    TerrainAtlasContent atlas;
    ASSERT_TRUE(BuildTerrainAtlas(16, 16, extent, border, {{4, 4, tile.data()}}, {{4, 4, 1}}, atlas));
    ASSERT_EQ(atlas.mips.size(), 5u); // 16,8,4,2,1
    auto at = [&](int x, int y) { return &atlas.mips[0][(y * 16 + x) * 4]; };
    // Row inversion: atlas row 4 (j=0) comes from legacy row extent-1.
    unsigned char expected[4];
    TerrainAtlas_Quantize(&tile[((extent - 1) * extent + 2) * 4], expected);
    EXPECT_EQ(std::memcmp(at(4 + 2, 4), expected, 4), 0);
    EXPECT_EQ(at(4, 4)[3], 255);                   // A1: always opaque
    // Border: column before = last tile column, column after = first.
    EXPECT_EQ(std::memcmp(at(3, 5), at(4 + extent - 1, 5), 4), 0);
    EXPECT_EQ(std::memcmp(at(4 + extent, 5), at(4, 5), 4), 0);
    // Border rows (including corners) duplicate the opposite edge row.
    EXPECT_EQ(std::memcmp(at(5, 3), at(5, 4 + extent - 1), 4), 0);
    EXPECT_EQ(std::memcmp(at(3, 3), at(4 + extent - 1, 4 + extent - 1), 4), 0);
    EXPECT_EQ(std::memcmp(at(5, 4 + extent), at(5, 4), 4), 0);
    // Untouched texels stay transparent black (legacy surface was never cleared by update).
    EXPECT_EQ(at(0, 0)[0] | at(0, 0)[1] | at(0, 0)[2] | at(0, 0)[3], 0);
}

TEST(TerrainAtlasTest, BoxFilterMipChainAveragesAndRejectsOutOfRangePlacement)
{
    std::vector<unsigned char> tile(2 * 2 * 4, 0);
    for (int i = 0; i < 4; ++i) { tile[i * 4 + 0] = tile[i * 4 + 1] = tile[i * 4 + 2] = static_cast<unsigned char>(i * 80); }
    TerrainAtlasContent atlas;
    ASSERT_TRUE(BuildTerrainAtlas(4, 4, 2, 0, {{1, 1, tile.data()}}, {}, atlas));
    ASSERT_EQ(atlas.mips.size(), 3u);
    // Level 1 texel (0,0) averages atlas texels (0,0),(1,0),(0,1),(1,1); only (1,1) is set.
    unsigned q[4]; unsigned char tmp[4];
    unsigned sum = 0;
    for (int i = 0; i < 4; ++i) { (void)q; (void)tmp; }
    for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) sum += atlas.mips[0][((dy) * 4 + dx) * 4];
    EXPECT_EQ(atlas.mips[1][0], static_cast<unsigned char>((sum + 2) / 4));
    EXPECT_EQ(atlas.mips[2].size(), 4u);
    EXPECT_FALSE(BuildTerrainAtlas(4, 4, 2, 0, {{3, 3, tile.data()}}, {}, atlas)); // exceeds the atlas
}

namespace {
// Native presentation needs a real window and a hardware device; skipped otherwise.
struct NativeScope {
    HWND window = nullptr; bool ok = false;
    NativeScope() {
        WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "NativeTerrainTestWindow"; RegisterClassA(&wc);
        window = CreateWindowA(wc.lpszClassName, "t", WS_OVERLAPPEDWINDOW, 0, 0, 816, 639, nullptr, nullptr, wc.hInstance, nullptr);
        if (!window) return;
        try {
            PresentationConfiguration config; config.window = window;
            WW3D::Init_Native_Presentation(config);
            ok = true;
        } catch (...) { ok = false; }
    }
    ~NativeScope() {
        if (ok) WW3D::Shutdown_Native_Presentation();
        if (window) DestroyWindow(window);
        UnregisterClassA("NativeTerrainTestWindow", GetModuleHandleA(nullptr));
    }
};
}

TEST(NativeBufferTest, LegacyBuffersAreCPUBackedAndFeedBackendGeometryResidency)
{
    NativeScope scope;
    if (!scope.ok) GTEST_SKIP() << "No hardware D3D11 device/window available";
    auto *backend = dynamic_cast<D3D11Backend *>(WW3D::Get_Render_Backend());
    ASSERT_NE(backend, nullptr);
    const unsigned fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2;
    DX8VertexBufferClass *vb = new DX8VertexBufferClass(fvf, 4, DX8VertexBufferClass::USAGE_DEFAULT);
    ASSERT_TRUE(vb->Is_CPU_Backed());
    ASSERT_EQ(vb->FVF_Info().Get_FVF_Size(), sizeof(WorldVertexXYZDUV2));
    EXPECT_EQ(vb->Get_DX8_Vertex_Buffer(), nullptr);
    const unsigned version0 = vb->Get_CPU_Version();
    {
        VertexBufferClass::WriteLockClass lock(vb);
        auto *v = static_cast<WorldVertexXYZDUV2 *>(lock.Get_Vertex_Array());
        v[0] = {-1, 1, 0.5f, 0xffff0000u, 0, 0, 0, 0};
        v[1] = {1, 1, 0.5f, 0xff00ff00u, 1, 0, 1, 0};
        v[2] = {1, -1, 0.5f, 0xff0000ffu, 1, 1, 1, 1};
        v[3] = {-1, -1, 0.5f, 0xffffffffu, 0, 1, 0, 1};
    }
    EXPECT_EQ(vb->Get_CPU_Version(), version0 + 1);
    {
        VertexBufferClass::AppendLockClass lock(vb, 2, 1);   // offset addressing
        auto *v = static_cast<WorldVertexXYZDUV2 *>(lock.Get_Vertex_Array());
        EXPECT_EQ(v->diffuse, 0xff0000ffu);
        v->diffuse = 0xff123456u;
    }
    DX8IndexBufferClass *ib = new DX8IndexBufferClass(6);
    ASSERT_TRUE(ib->Is_CPU_Backed());
    {
        IndexBufferClass::WriteLockClass lock(ib);
        unsigned short *i = lock.Get_Index_Array();
        const unsigned short quad[6] = {0, 1, 2, 0, 2, 3};
        std::memcpy(i, quad, sizeof(quad));
    }
    // The backend consumes exactly this content (no COM aliasing).
    GeometryCPUContent content;
    const auto *src = reinterpret_cast<const WorldVertexXYZDUV2 *>(vb->Get_CPU_Data());
    content.vertices.assign(src, src + 4);
    content.indices.assign(ib->Get_CPU_Data(), ib->Get_CPU_Data() + 6);
    EXPECT_EQ(content.vertices[2].diffuse, 0xff123456u);
    ASSERT_TRUE(content.Valid());
    std::unique_ptr<GeometryResidency> residency(backend->Create_Geometry_Residency(content));
    EXPECT_EQ(residency->Vertex_Count(), 4u);
    EXPECT_EQ(residency->Index_Count(), 6u);
    residency.reset();
    vb->Release_Ref();
    ib->Release_Ref();
}
#else
TEST(NativeBufferTest, Win32UsesTheUnchangedLegacyBuffers) { SUCCEED(); }
#endif
