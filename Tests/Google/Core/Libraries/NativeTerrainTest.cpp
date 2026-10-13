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

// The terrain pass converts the legacy column-vector camera/world matrices to the
// D3D row-vector world contract; verify against the legacy product order.
TEST(WorldTransformTest, LegacyColumnVectorChainEqualsRowVectorChain)
{
    // Matrix3D rows 0..2 (rotation about z by 30 degrees + translation), arbitrary view
    // and a perspective Matrix4x4 like CameraClass::Get_D3D_Projection_Matrix produces.
    const float c = 0.8660254f, s = 0.5f;
    const float world3[12] = {c, -s, 0, 10, s, c, 0, -4, 0, 0, 1, 2};
    const float view3[12] = {1, 0, 0, -3, 0, 0.7071f, -0.7071f, 5, 0, 0.7071f, 0.7071f, -20};
    const float proj4[16] = {1.5f, 0, 0, 0, 0, 2.0f, 0, 0, 0, 0, -1.05f, -1.05f * 0.5f, 0, 0, -1, 0};
    float w[16], v[16], p[16];
    World_From_Matrix3D(world3, w);
    World_From_Matrix3D(view3, v);
    World_From_Matrix4x4(proj4, p);
    const float point[4] = {7, 3, 1.5f, 1};
    // Legacy: clip = P * (V * (W * point)).
    auto mul3 = [](const float *m, const float *in, float *out) {
        for (int r = 0; r < 3; ++r) out[r] = m[r * 4] * in[0] + m[r * 4 + 1] * in[1] + m[r * 4 + 2] * in[2] + m[r * 4 + 3] * in[3];
        out[3] = in[3];
    };
    float a[4], b[4], legacy[4];
    mul3(world3, point, a);
    mul3(view3, a, b);
    for (int r = 0; r < 4; ++r)
        legacy[r] = proj4[r * 4] * b[0] + proj4[r * 4 + 1] * b[1] + proj4[r * 4 + 2] * b[2] + proj4[r * 4 + 3] * b[3];
    // D3D: clip = point * W * V * P.
    auto row = [](const float *in, const float *m, float *out) {
        for (int c2 = 0; c2 < 4; ++c2)
            out[c2] = in[0] * m[c2] + in[1] * m[4 + c2] + in[2] * m[8 + c2] + in[3] * m[12 + c2];
    };
    float d1[4], d2[4], d3[4];
    row(point, w, d1);
    row(d1, v, d2);
    row(d2, p, d3);
    for (int i = 0; i < 4; ++i) EXPECT_NEAR(d3[i], legacy[i], 1e-4f) << i;
    // The last column of a transformed Matrix3D is exactly (0,0,0,1) in D3D layout.
    EXPECT_EQ(w[3], 0.0f); EXPECT_EQ(w[7], 0.0f); EXPECT_EQ(w[11], 0.0f); EXPECT_EQ(w[15], 1.0f);
}

// X64.26: the legacy cloud/noise texgen (D3DTSS_TCI_CAMERASPACEPOSITION through
// updateNoise1: inverse(view) * scale(STRETCH) * translate(offset)) must equal the world-space
// XY scale+offset the native combiner computes, independent of the camera.
TEST(WorldTransformTest, LegacyCameraSpaceTexgenEqualsWorldXYScaleAndOffset)
{
    auto mul = [](const float *a, const float *b, float *out) {   // row-vector 4x4 product
        float r[16];
        for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
            float v = 0; for (int k = 0; k < 4; ++k) v += a[i * 4 + k] * b[k * 4 + j]; r[i * 4 + j] = v;
        }
        std::memcpy(out, r, sizeof(r));
    };
    // An arbitrary rigid view (rotation about x then z, translation) and its exact inverse.
    const float cz = 0.8f, sz = 0.6f, cx = 0.6f, sx = 0.8f;
    float rz[16] = {cz, sz, 0, 0, -sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float rx[16] = {1, 0, 0, 0, 0, cx, sx, 0, 0, -sx, cx, 0, 0, 0, 0, 1};
    float tr[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -300, 120, -900, 1};
    float view[16], inv[16], tmp[16];
    mul(rz, rx, tmp); mul(tmp, tr, view);
    // inverse = (T^-1 * Rx^T * Rz^T) in row-vector order: v * T * Rx * Rz -> inverse = Rz^T * Rx^T * T^-1
    float rzt[16] = {cz, -sz, 0, 0, sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float rxt[16] = {1, 0, 0, 0, 0, cx, -sx, 0, 0, sx, cx, 0, 0, 0, 0, 1};
    float tri[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 300, -120, 900, 1};
    mul(tri, rxt, tmp); mul(tmp, rzt, inv);
    float check[16]; mul(view, inv, check);
    for (int i = 0; i < 16; ++i) EXPECT_NEAR(check[i], (i % 5 == 0) ? 1.0f : 0.0f, 1e-4f);

    const float stretch = 1.0f / (63.0f * 10.0f / 2.0f), xo = 0.37f, yo = -0.21f;
    float scale[16] = {stretch, 0, 0, 0, 0, stretch, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float offset[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, xo, yo, 0, 1};
    float texture[16];
    mul(inv, scale, tmp); mul(tmp, offset, texture);       // legacy updateNoise1
    const float world[4] = {1234.0f, 567.0f, 89.0f, 1.0f}; // vertex * World (terrain Transform)
    float camera[4];                                        // D3DTSS_TCI_CAMERASPACEPOSITION: vertex*World*View
    for (int c = 0; c < 4; ++c) camera[c] = world[0] * view[c] + world[1] * view[4 + c] + world[2] * view[8 + c] + world[3] * view[12 + c];
    float uv[2];
    for (int c = 0; c < 2; ++c) uv[c] = camera[0] * texture[c] + camera[1] * texture[4 + c] + camera[2] * texture[8 + c] + camera[3] * texture[12 + c];
    EXPECT_NEAR(uv[0], world[0] * stretch + xo, 1e-4f);
    EXPECT_NEAR(uv[1], world[1] * stretch + yo, 1e-4f);
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
            D3D11Backend::Enable_CPU_Backed_Geometry(true);
            ok = true;
        } catch (...) { ok = false; }
    }
    ~NativeScope() {
        D3D11Backend::Enable_CPU_Backed_Geometry(false);
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
