// SPDX-License-Identifier: GPL-3.0-or-later
// X64.30 radar / minimap: the original W3DRadar raster rules (terrain shading, 2x2 object marker, stealth
// pulse, shroud rectangle and alpha) and the three dynamic UI textures (terrain, overlay, shroud) through
// the real TextureClass -> CPU content -> D3D11 residency path on WARP, with deterministic scenes.
#include <gtest/gtest.h>
#if defined(_WIN64)
#include "Common/Radar.h"
#include "W3DDevice/Common/W3DRadarRaster.h"
#include "WW3D2/Backend/D3D11Backend.h"
#include "WW3D2/Render2DSubmission.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/Texture2DCPUContent.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/texture.h"
#include "WW3D2/shader.h"
#include "WW3D2/ww3d.h"
#include "WWMath/vector2.h"
#include "WWMath/vector3.h"
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
constexpr unsigned N = RADAR_CELL_WIDTH;  // 128: the radar is one texel per cell, drawn 1:1 here
static_assert(RADAR_CELL_WIDTH == RADAR_CELL_HEIGHT, "the 1:1 target below assumes a square radar");

// RadarDummy exposes nothing about the map; this sets what Radar::newMap would from a TerrainLogic extent.
struct TestRadar : RadarDummy
{
    void setExtent(Real width, Real height)
    {
        m_mapExtent.lo.x = m_mapExtent.lo.y = m_mapExtent.lo.z = 0.0f;
        m_mapExtent.hi.x = width; m_mapExtent.hi.y = height; m_mapExtent.hi.z = 100.0f;
        m_xSample = width / RADAR_CELL_WIDTH;
        m_ySample = height / RADAR_CELL_HEIGHT;
    }
    Real xSample() const { return m_xSample; }
    Real ySample() const { return m_ySample; }
    using Radar::findDrawPositions;
};

struct Rig
{
    std::unique_ptr<D3D11Backend> backend;
    ShaderClass shader;
    Rig()
    {
        D3D11Backend::Configuration c;
        c.software = true; c.width = c.height = N;
        backend = D3D11Backend::Create_For_W3D(c);
        WW3D::Set_Render_Backend_For_Tests(backend.get());
        shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE); shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
        shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
        shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA); shader.Set_Fog_Func(ShaderClass::FOG_DISABLE);
        shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE); shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
    }
    ~Rig() { WW3D::Set_Render_Backend_For_Tests(nullptr); }

    // Draw one radar texture over the whole target exactly like drawImage with the radar Image UVs
    // (lo.y = 1, hi.y = 0: texel row y appears at screen row N-1-y).
    void drawImage(TextureClass &texture)
    {
        const float left = -1 - 1.0f / N, right = 1 - 1.0f / N, top = 1 + 1.0f / N, bottom = -1 + 1.0f / N;
        const std::array<Vector2, 4> p = {Vector2(left, top), Vector2(left, bottom), Vector2(right, top), Vector2(right, bottom)};
        const std::array<Vector2, 4> uv = {Vector2(0, 1), Vector2(0, 0), Vector2(1, 1), Vector2(1, 0)};
        const std::array<unsigned long, 4> color = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
        const std::array<unsigned short, 6> index = {0, 1, 2, 2, 1, 3};
        Render2DSubmission s = {&texture, &shader, p.data(), uv.data(), color.data(), index.data(), 4, 6, 0, {0, 0, N, N, 0, 1}, false};
        backend->Submit_Render2D(s);
    }
    std::vector<unsigned char> frame(TextureClass &terrain, TextureClass &overlay, TextureClass &shroud)
    {
        backend->Clear(true, false, Vector3(0, 0, 0));
        backend->Begin_Scene();
        drawImage(terrain);
        drawImage(overlay);
        drawImage(shroud);
        backend->End_Scene(false);
        return backend->Readback_RGBA8();
    }
};

struct Pixel { unsigned r, g, b, a; };
Pixel at(const std::vector<unsigned char> &frame, unsigned cellX, unsigned cellY)
{
    // texel row cellY is shown at screen row N-1-cellY
    const size_t i = (static_cast<size_t>(N - 1 - cellY) * N + cellX) * 4;
    return {frame[i], frame[i + 1], frame[i + 2], frame[i + 3]};
}

struct Plane
{
    TextureClass &texture;
    SurfaceClass *surface;
    void *bits;
    int pitch;
    WW3DFormat format;
    unsigned bytes;
    explicit Plane(TextureClass &t) : texture(t), surface(t.Get_Surface_Level()), bits(nullptr), pitch(0)
    {
        SurfaceClass::SurfaceDescription d;
        surface->Get_Description(d);
        format = d.Format;
        bytes = Get_Bytes_Per_Pixel(format);
        bits = surface->Lock(&pitch);
    }
    ~Plane() { surface->Unlock(); REF_PTR_RELEASE(surface); }
    void put(Int x, Int y, unsigned argb)
    {
        surface->Draw_Pixel(x, y, ARGB_Color_To_WW3D_Color(format, argb), bytes, bits, pitch);
    }
};

constexpr unsigned argb(unsigned a, unsigned r, unsigned g, unsigned b) { return (a << 24) | (r << 16) | (g << 8) | b; }
}

TEST(NativeRadarTest, DynamicTexturesAreCpuBackedAndSurfaceLocksFollowTheLegacyContract)
{
    Rig rig;
    TextureClass terrain(N, N, WW3D_FORMAT_X8R8G8B8, MIP_LEVELS_1);
    ASSERT_TRUE(terrain.Is_Render2D_Dynamic());
    EXPECT_FALSE(terrain.Peek_D3D_Base_Texture());   // no DX8 storage at all
    SurfaceClass *surface = terrain.Get_Surface_Level();
    ASSERT_NE(surface, nullptr);
    SurfaceClass::SurfaceDescription d;
    surface->Get_Description(d);
    EXPECT_EQ(d.Width, N); EXPECT_EQ(d.Height, N); EXPECT_EQ(d.Format, WW3D_FORMAT_X8R8G8B8);
    EXPECT_EQ(surface->Get_Bytes_Per_Pixel(), 4u);
    const unsigned revision = terrain.Peek_Render2D_Dynamic_Content()->Revision();
    int pitch = 0;
    unsigned char *bits = static_cast<unsigned char *>(surface->Lock(&pitch));
    ASSERT_NE(bits, nullptr);
    EXPECT_GE(pitch, static_cast<int>(N * 4));
    surface->Draw_H_Line(3, 0, N - 1, argb(255, 10, 20, 30), 4, bits, pitch);
    surface->Unlock();
    EXPECT_GT(terrain.Peek_Render2D_Dynamic_Content()->Revision(), revision);
    // Clear zaps the surface (legacy reset()).
    surface->Clear();
    int p2 = 0;
    const unsigned char *check = static_cast<const unsigned char *>(surface->Lock(&p2));
    for (unsigned i = 0; i < 16; ++i) EXPECT_EQ(check[3 * p2 + i], 0);
    surface->Unlock();
    REF_PTR_RELEASE(surface);
}

TEST(NativeRadarTest, DeterministicSceneTerrainMarkersAndShroudLandWhereTheOriginalPutsThem)
{
    Rig rig;
    // The radar uses these three formats on the native backend: terrain X8R8G8B8, overlay/shroud A8R8G8B8.
    TextureClass terrain(N, N, WW3D_FORMAT_X8R8G8B8, MIP_LEVELS_1);
    TextureClass overlay(N, N, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1);
    TextureClass shroud(N, N, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1);

    TestRadar radar;
    radar.setExtent(1280.0f, 1280.0f);   // 10 world units per radar cell
    EXPECT_FLOAT_EQ(radar.xSample(), 10.0f);

    // Terrain: a green base, shaded by height (a ramp across x: low on the left, high on the right).
    {
        Plane plane(terrain);
        for (Int y = 0; y < static_cast<Int>(N); ++y)
            for (Int x = 0; x < static_cast<Int>(N); ++x) {
                RGBColor c = {0.2f, 0.6f, 0.2f};
                RadarRaster::shadeForHeight(&c, static_cast<Real>(x), 127.0f, 64.0f, 0.0f);
                plane.put(x, y, argb(255, static_cast<unsigned>(c.red * 255), static_cast<unsigned>(c.green * 255), static_cast<unsigned>(c.blue * 255)));
            }
    }
    // Overlay: one friendly (blue) and one enemy (red) object at known world positions.
    struct Marker { Coord3D pos; unsigned color; };
    const Marker markers[] = {{{300.0f, 500.0f, 0.0f}, argb(255, 0, 0, 255)}, {{1000.0f, 120.0f, 0.0f}, argb(255, 255, 0, 0)}};
    {
        Plane plane(overlay);
        for (const Marker &m : markers) {
            ICoord2D cell;
            radar.worldToRadar(&m.pos, &cell);
            RadarRaster::plotMarker(cell.x, cell.y, [&](Int x, Int y) { plane.put(x, y, m.color); });
        }
    }
    // Shroud: logic cell (2,3) of 40 world units shrouded, (6,6) fogged.
    {
        Plane plane(shroud);
        auto paint = [&](Int sx, Int sy, CellShroudStatus s) {
            const RadarRaster::CellRect r = RadarRaster::shroudCellRect(sx, sy, 40, 40, radar.xSample(), radar.ySample());
            for (Int y = r.minY; y <= r.maxY; ++y)
                for (Int x = r.minX; x <= r.maxX; ++x)
                    plane.put(x, y, argb(static_cast<unsigned>(RadarRaster::shroudAlpha(s)), 0, 0, 0));
        };
        paint(2, 3, CELLSHROUD_SHROUDED);
        paint(6, 6, CELLSHROUD_FOGGED);
    }

    const auto frame = rig.frame(terrain, overlay, shroud);

    // Terrain shading: left lighter than mid? (below midZ darkens; above lightens): monotonic in x on a row
    // away from the markers and the shroud.
    const Pixel low = at(frame, 2, 100), mid = at(frame, 64, 100), high = at(frame, 125, 100);
    EXPECT_LT(low.g, mid.g);
    EXPECT_LT(mid.g, high.g);
    EXPECT_EQ(low.a, 255u);

    // Markers: the 2x2 block (x,y),(x,y+1),(x+1,y+1),(x+1,y) of cell (30,50) and (100,12).
    for (const Marker &m : markers) {
        ICoord2D cell;
        radar.worldToRadar(&m.pos, &cell);
        const Pixel want = {(m.color >> 16) & 0xff, (m.color >> 8) & 0xff, m.color & 0xff, 255};
        for (Int dx = 0; dx < 2; ++dx) for (Int dy = 0; dy < 2; ++dy) {
            const Pixel p = at(frame, cell.x + dx, cell.y + dy);
            EXPECT_EQ(p.r, want.r); EXPECT_EQ(p.g, want.g); EXPECT_EQ(p.b, want.b);
        }
        // neighbours stay terrain
        const Pixel outside = at(frame, cell.x + 3, cell.y);
        EXPECT_NE(outside.r + outside.b * 256u, want.r + want.b * 256u);
    }

    // Shrouded logic cell (2,3): world [80,120) x [120,160) -> radar cells 8..12 x 12..16, opaque black.
    const RadarRaster::CellRect shrouded = RadarRaster::shroudCellRect(2, 3, 40, 40, radar.xSample(), radar.ySample());
    EXPECT_EQ(shrouded.minX, 8); EXPECT_EQ(shrouded.maxX, 12);
    EXPECT_EQ(shrouded.minY, 12); EXPECT_EQ(shrouded.maxY, 16);
    for (Int y = shrouded.minY; y <= shrouded.maxY; ++y)
        for (Int x = shrouded.minX; x <= shrouded.maxX; ++x) {
            const Pixel p = at(frame, x, y);
            EXPECT_EQ(p.r + p.g + p.b, 0u) << x << "," << y;
        }
    // Just outside the shrouded rectangle the terrain is untouched.
    const Pixel beyond = at(frame, shrouded.maxX + 1, shrouded.minY);
    EXPECT_GT(beyond.g, 0u);
    // Fogged cell (6,6): alpha 127 darkens the terrain to about half.
    const RadarRaster::CellRect fogged = RadarRaster::shroudCellRect(6, 6, 40, 40, radar.xSample(), radar.ySample());
    const Pixel fog = at(frame, fogged.minX + 1, fogged.minY + 1);
    const Pixel clearAbove = at(frame, fogged.minX + 1, fogged.maxY + 3);
    EXPECT_GT(fog.g, 0u);
    EXPECT_NEAR(static_cast<double>(fog.g), clearAbove.g * (1.0 - 127.0 / 255.0), clearAbove.g * 0.12 + 3.0);
}

TEST(NativeRadarTest, TextureUpdatesReachTheGpuOncePerChangeAndOnlyWhenChanged)
{
    Rig rig;
    TextureClass terrain(N, N, WW3D_FORMAT_X8R8G8B8, MIP_LEVELS_1);
    TextureClass overlay(N, N, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1);
    TextureClass shroud(N, N, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1);
    {
        Plane plane(terrain);
        for (Int y = 0; y < static_cast<Int>(N); ++y) for (Int x = 0; x < static_cast<Int>(N); ++x) plane.put(x, y, argb(255, 40, 120, 40));
    }
    const auto first = rig.frame(terrain, overlay, shroud);
    const unsigned uploadsAfterFirst = rig.backend->Dynamic_Uploads();
    // An unchanged radar redraws without re-uploading anything.
    const auto again = rig.frame(terrain, overlay, shroud);
    EXPECT_EQ(again, first);
    EXPECT_EQ(rig.backend->Dynamic_Uploads(), uploadsAfterFirst);

    // The object overlay is refreshed periodically (OVERLAY_REFRESH_RATE): one lock/unlock = one upload.
    {
        Plane plane(overlay);
        plane.put(10, 10, argb(255, 255, 255, 0));
    }
    const auto updated = rig.frame(terrain, overlay, shroud);
    EXPECT_EQ(rig.backend->Dynamic_Uploads(), uploadsAfterFirst + 1);
    EXPECT_NE(updated, first);
    const Pixel p = at(updated, 10, 10);
    EXPECT_EQ(p.r, 255u); EXPECT_EQ(p.g, 255u); EXPECT_EQ(p.b, 0u);

    // A shroud update batch (begin/set/end keeps one lock) is one upload, not one per cell.
    {
        Plane plane(shroud);
        for (Int i = 0; i < 20; ++i) plane.put(i, 60, argb(255, 0, 0, 0));
    }
    rig.frame(terrain, overlay, shroud);
    EXPECT_EQ(rig.backend->Dynamic_Uploads(), uploadsAfterFirst + 2);
}

TEST(NativeRadarTest, RasterRulesMatchTheLegacyFormulas)
{
    // Stealth pulse: one second ramp between 32 and opaque.
    EXPECT_EQ(RadarRaster::stealthAlpha(0, 30), 32);
    EXPECT_EQ(RadarRaster::stealthAlpha(15, 30), 32);
    EXPECT_GT(RadarRaster::stealthAlpha(29, 30), 230);
    EXPECT_EQ(RadarRaster::stealthAlpha(30, 30), RadarRaster::stealthAlpha(0, 30));   // wraps each second

    // Shroud alpha: shrouded opaque, fogged 127, clear none.
    EXPECT_EQ(RadarRaster::shroudAlpha(CELLSHROUD_SHROUDED), 255);
    EXPECT_EQ(RadarRaster::shroudAlpha(CELLSHROUD_FOGGED), 127);
    EXPECT_EQ(RadarRaster::shroudAlpha(CELLSHROUD_CLEAR), 0);

    // Height shading: mid height unchanged, high toward white (0.95), low toward black (x0.4).
    RGBColor c = {0.5f, 0.25f, 0.75f};
    RadarRaster::shadeForHeight(&c, 50.0f, 100.0f, 50.0f, 0.0f);
    EXPECT_NEAR(c.red, 0.5f, 1e-5); EXPECT_NEAR(c.green, 0.25f, 1e-5); EXPECT_NEAR(c.blue, 0.75f, 1e-5);
    c = {0.5f, 0.25f, 0.75f};
    RadarRaster::shadeForHeight(&c, 100.0f, 100.0f, 50.0f, 0.0f);
    EXPECT_NEAR(c.red, 0.5f + 0.5f * 0.95f, 1e-5);
    c = {0.5f, 0.25f, 0.75f};
    RadarRaster::shadeForHeight(&c, 0.0f, 100.0f, 50.0f, 0.0f);
    EXPECT_NEAR(c.red, 0.5f * 0.4f, 1e-5);
    // Degenerate ranges do not divide by zero.
    c = {0.5f, 0.5f, 0.5f};
    RadarRaster::shadeForHeight(&c, 5.0f, 5.0f, 5.0f, 5.0f);
    EXPECT_FALSE(std::isnan(c.red));

    // Marker clipping at the radar edges: each of the four cells clips separately.
    std::vector<std::pair<Int, Int>> hit;
    RadarRaster::plotMarker(127, 127, [&](Int x, Int y) { hit.push_back({x, y}); });
    ASSERT_EQ(hit.size(), 1u);
    EXPECT_EQ(hit[0], (std::pair<Int, Int>(127, 127)));
    hit.clear();
    RadarRaster::plotMarker(-1, 5, [&](Int x, Int y) { hit.push_back({x, y}); });
    ASSERT_EQ(hit.size(), 2u);   // only x=0 survives: (0,6) and (0,5)
    EXPECT_EQ(hit[0], (std::pair<Int, Int>(0, 6)));
    EXPECT_EQ(hit[1], (std::pair<Int, Int>(0, 5)));
}

TEST(NativeRadarTest, ShroudCellRectsAgreeWithRadarWorldToRadarAcrossMapShapes)
{
    // The helper replaces two worldToRadar calls in W3DRadar::setShroudLevel; it must agree everywhere,
    // including the clamp at the far edge and non-square maps.
    for (const std::array<Real, 2> &extent : {std::array<Real, 2>{1280, 1280}, {2400, 800}, {600, 3000}, {5120, 5120}}) {
        TestRadar radar;
        radar.setExtent(extent[0], extent[1]);
        for (Int cellSize : {10, 40}) {
            const Int cellsX = static_cast<Int>(std::ceil(extent[0] / cellSize));
            const Int cellsY = static_cast<Int>(std::ceil(extent[1] / cellSize));
            for (Int sy = 0; sy < cellsY; sy += 7) for (Int sx = 0; sx < cellsX; sx += 5) {
                const RadarRaster::CellRect r = RadarRaster::shroudCellRect(sx, sy, cellSize, cellSize, radar.xSample(), radar.ySample());
                Coord3D a = {static_cast<Real>(sx * cellSize), static_cast<Real>(sy * cellSize), 0};
                Coord3D b = {static_cast<Real>((sx + 1) * cellSize), static_cast<Real>((sy + 1) * cellSize), 0};
                ICoord2D ra, rb;
                radar.worldToRadar(&a, &ra);
                radar.worldToRadar(&b, &rb);
                EXPECT_EQ(r.minX, ra.x); EXPECT_EQ(r.minY, ra.y);
                EXPECT_EQ(r.maxX, rb.x); EXPECT_EQ(r.maxY, rb.y);
            }
        }
    }
}

TEST(NativeRadarTest, FindDrawPositionsPreservesTheMapAspectRatio)
{
    TestRadar radar;
    radar.setExtent(2400.0f, 800.0f);   // wide map: letterboxed vertically in a square radar window
    ICoord2D ul, lr;
    radar.findDrawPositions(0, 0, 180, 180, &ul, &lr);
    EXPECT_EQ(ul.x, 0); EXPECT_EQ(lr.x, 180);
    EXPECT_NEAR(lr.y - ul.y, 60, 1);
    EXPECT_NEAR(ul.y, 60, 1);
    radar.setExtent(800.0f, 2400.0f);   // tall map
    radar.findDrawPositions(0, 0, 180, 180, &ul, &lr);
    EXPECT_EQ(ul.y, 0); EXPECT_EQ(lr.y, 180);
    EXPECT_NEAR(lr.x - ul.x, 60, 1);
}
#endif
