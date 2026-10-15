// SPDX-License-Identifier: GPL-3.0-or-later
// Native world effects (X64.29): geometry builders, line render objects, the decal/planar-shadow manager
// and the stencil-once state, exercised through the D3D11 WARP backend. Hermetic except the retail-model
// shadow test (GENERALS_OFFLINE_ASSETS).
#include <gtest/gtest.h>
#if defined(_WIN64)
#include "Common/GameCommon.h"
#include "Common/GameType.h"
#include "Common/GlobalData.h"
#include "GameClient/Color.h"
#include "GameClient/Shadow.h"
#include "W3DDevice/GameClient/NativeWorldEffects.h"
#include "W3DDevice/GameClient/NativeWorldObjects.h"
#include "WW3D2/Backend/D3D11Backend.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/camera.h"
#include "WW3D2/line3d.h"
#include "WW3D2/segline.h"
#include "WW3D2/shader.h"
#include "WWLib/ffactory.h"
#include "WWMath/matrix3d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace
{
using NativeWorldEffects::Geometry::Mesh;

struct Rig
{
    std::unique_ptr<D3D11Backend> backend;
    CameraClass camera;
    NativeWorldObjects::SceneLighting lights = {};
    unsigned size;

    explicit Rig(unsigned s = 128) : size(s)
    {
        NativeWorldObjects::setRenderTargetSizeForTests(s, s);
        D3D11Backend::Configuration c;
        c.software = true; c.width = c.height = s; c.depth_buffer = true;
        backend = D3D11Backend::Create(c);
        camera.Set_Aspect_Ratio(1.0f);
        camera.Set_View_Plane(DEG_TO_RADF(45.0f), -1);
        camera.Set_Clip_Planes(1.0f, 2000.0f);
        camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
        Matrix3D cam(1);
        cam.Look_At(Vector3(0.0f, -120.0f, 90.0f), Vector3(0.0f, 0.0f, 0.0f), 0.0f);
        camera.Set_Transform(cam);
        lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.5f;
        lights.count = 0;
    }
    ~Rig()
    {
        NativeWorldObjects::releaseAll();
        NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
    }
    std::vector<unsigned char> render(const NativeWorldObjects::ObjectInput *objects, unsigned count)
    {
        backend->Clear(true, true, Vector3(0.2f, 0.25f, 0.3f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        NativeWorldObjects::renderObjects(*backend, camera, lights, lights, objects, count);
        backend->End_Scene(false);
        return backend->Readback_RGBA8();
    }
};

unsigned Differing(const std::vector<unsigned char> &a, const std::vector<unsigned char> &b)
{
    unsigned n = 0;
    for (size_t i = 0; i + 3 < a.size() && i + 3 < b.size(); i += 4)
        if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) ++n;
    return n;
}


bool WriteBmp(const std::string &path, const std::vector<unsigned char> &rgba, unsigned w, unsigned h)
{
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    unsigned char header[54] = {'B', 'M'};
    const unsigned dataSize = w * h * 4, fileSize = 54 + dataSize, offset = 54, dib = 40, planes = 1, bits = 32;
    const int topDown = -static_cast<int>(h);
    std::memcpy(header + 2, &fileSize, 4); std::memcpy(header + 10, &offset, 4); std::memcpy(header + 14, &dib, 4);
    std::memcpy(header + 18, &w, 4); std::memcpy(header + 22, &topDown, 4);
    std::memcpy(header + 26, &planes, 2); std::memcpy(header + 28, &bits, 2); std::memcpy(header + 34, &dataSize, 4);
    std::fwrite(header, 1, 54, f);
    std::vector<unsigned char> bgra(rgba.size());
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) { bgra[i] = rgba[i + 2]; bgra[i + 1] = rgba[i + 1]; bgra[i + 2] = rgba[i]; bgra[i + 3] = 255; }
    std::fwrite(bgra.data(), 1, bgra.size(), f);
    std::fclose(f);
    return true;
}
}

TEST(NativeWorldEffectsTest, StandingWaterPatchMatchesTheLegacyTessellationAndScrolls)
{
    const float corners[4][3] = {{100, 100, 7}, {0, 100, 7}, {0, 0, 7}, {100, 0, 7}};
    Mesh a, b;
    NativeWorldEffects::Geometry::standingWater(corners, 0.0f, 0x80ffffffu, a);
    NativeWorldEffects::Geometry::standingWater(corners, 0.5f, 0x80ffffffu, b);
    ASSERT_FALSE(a.vertices.empty());
    EXPECT_EQ(a.vertices.size(), b.vertices.size());
    EXPECT_EQ(a.indices.size() % 3, 0u);
    // uCount = (100 + 100) / 80 = 2 (+1) -> a 3 x 3 vertex grid, 8 triangles.
    EXPECT_EQ(a.vertices.size(), 9u);
    EXPECT_EQ(a.indices.size(), 24u);
    for (unsigned short i : a.indices) EXPECT_LT(i, a.vertices.size());
    for (const WorldVertexXYZDUV2 &v : a.vertices) EXPECT_FLOAT_EQ(v.z, 7.0f);
    unsigned moved = 0;
    for (size_t i = 0; i < a.vertices.size(); ++i)
        if (std::fabs(a.vertices[i].u0 - b.vertices[i].u0) > 1e-5f || std::fabs(a.vertices[i].v0 - b.vertices[i].v0) > 1e-5f) ++moved;
    EXPECT_GT(moved, 0u);   // the texture coordinates wobble with the scroll phase
}

TEST(NativeWorldEffectsTest, RiverStripWalksInwardAndAppliesTheEdgeAlphaProfile)
{
    // 8 points: outer edge going along +x then the inner edge coming back (river start = 1).
    const float pts[8][3] = {{0, 0, 3}, {40, 0, 3}, {80, 0, 3}, {120, 0, 3}, {120, 20, 3}, {80, 20, 3}, {40, 20, 3}, {0, 20, 3}};
    std::vector<float> flat;
    for (auto &p : pts) { flat.push_back(p[0]); flat.push_back(p[1]); flat.push_back(p[2]); }
    const std::vector<float> edge = {0.0f, 1.0f, 1.0f, 0.0f};
    Mesh mesh;
    ASSERT_TRUE(NativeWorldEffects::Geometry::river(flat.data(), 8, 3, 0.25f, 0xc0808080u, edge, mesh));
    const int rows = 4, across = 7;
    EXPECT_EQ(mesh.vertices.size(), static_cast<size_t>(rows * across));
    EXPECT_EQ(mesh.indices.size(), static_cast<size_t>((rows - 1) * (across - 1) * 6));
    // Edge vertices are transparent, the middle keeps the water alpha.
    EXPECT_EQ(mesh.vertices[0].diffuse >> 24, 0u);
    EXPECT_EQ(mesh.vertices[across - 1].diffuse >> 24, 0u);
    EXPECT_GT(mesh.vertices[across / 2].diffuse >> 24, 0x80u);
    // rgb is preserved
    EXPECT_EQ(mesh.vertices[across / 2].diffuse & 0xffffffu, 0x808080u);
    Mesh bad;
    EXPECT_FALSE(NativeWorldEffects::Geometry::river(flat.data(), 3, 0, 0.0f, 0xffffffffu, {}, bad));
}

TEST(NativeWorldEffectsTest, RiverWithoutCpuProfileKeepsTheLegacyTwoVertexRowsForTheGpuAlphaEdge)
{
    // The retail river pixel shader samples TWAlphaEdge across the strip with the second coordinate set
    // (u2 = 1 inner / 0 outer); the first set is the river texture (HEIGHT_TO_USE 0.5 inner / 0 outer).
    const float pts[8][3] = {{0, 0, 3}, {40, 0, 3}, {80, 0, 3}, {120, 0, 3}, {120, 20, 3}, {80, 20, 3}, {40, 20, 3}, {0, 20, 3}};
    std::vector<float> flat;
    for (auto &p : pts) { flat.push_back(p[0]); flat.push_back(p[1]); flat.push_back(p[2]); }
    Mesh mesh;
    ASSERT_TRUE(NativeWorldEffects::Geometry::river(flat.data(), 8, 3, 0.0f, 0xff808080u, {}, mesh));
    ASSERT_EQ(mesh.vertices.size(), 8u);          // 4 rows x (outer, inner)
    EXPECT_EQ(mesh.indices.size(), 3u * 6u);
    for (size_t row = 0; row < 4; ++row) {
        const WorldVertexXYZDUV2 &outer = mesh.vertices[row * 2], &inner = mesh.vertices[row * 2 + 1];
        EXPECT_FLOAT_EQ(outer.u0, 0.0f); EXPECT_FLOAT_EQ(inner.u0, 0.5f);
        EXPECT_FLOAT_EQ(outer.u1, 0.0f); EXPECT_FLOAT_EQ(inner.u1, 1.0f);
        EXPECT_EQ(outer.diffuse >> 24, 0xffu);    // vertex alpha untouched: it only weights the sparkle term
        EXPECT_FLOAT_EQ(outer.v0, outer.v1);
    }
}

TEST(NativeWorldEffectsTest, StandingWaterCarriesTheSparkleCoordinatesInTheSecondSet)
{
    const float corners[4][3] = {{100, 100, 7}, {0, 100, 7}, {0, 0, 7}, {100, 0, 7}};
    Mesh mesh;
    NativeWorldEffects::Geometry::standingWater(corners, 0.0f, 0x80ffffffu, mesh);
    ASSERT_FALSE(mesh.vertices.empty());
    for (const WorldVertexXYZDUV2 &v : mesh.vertices) {
        EXPECT_NEAR(v.u1, v.x / 50.0f, 1e-5f);                       // BUMP_SIZE
        EXPECT_NEAR(v.v1, (v.y + 0.3f * v.x) / 50.0f, 1e-5f);
    }
}

float latticeHeight(int x, int y) { return 2.0f + 0.1f * static_cast<float>(x) + 0.2f * static_cast<float>(y); }
bool latticeFlip(int x, int y) { return ((x + y) & 1) != 0; }

TEST(NativeWorldEffectsTest, ShroudOverlayLiesInTheTerrainSurfaceWithTheTerrainDiagonals)
{
    // 4x4 shroud cells of 40 units (a 16x16 terrain lattice); all clear (255) except cell (1,1) shrouded (0)
    // and cell (2,1) fogged (127).
    std::vector<unsigned char> levels(16, 255);
    levels[1 * 4 + 1] = 0;
    levels[1 * 4 + 2] = 127;
    const float white[3] = {1.0f, 1.0f, 1.0f};
    Mesh smooth, blocky;
    ASSERT_TRUE(NativeWorldEffects::Geometry::shroud(levels.data(), 4, 4, 40.0f, 0, true, 0, 0, 16, 16, &latticeHeight, &latticeFlip, white, smooth));
    ASSERT_TRUE(NativeWorldEffects::Geometry::shroud(levels.data(), 4, 4, 40.0f, 0, false, 0, 0, 16, 16, &latticeHeight, &latticeFlip, white, blocky));
    const int vx = 17;
    auto at = [&](const Mesh &m, int i, int j) { return m.vertices[static_cast<size_t>(j) * vx + i]; };
    ASSERT_EQ(smooth.vertices.size(), static_cast<size_t>(17 * 17));
    ASSERT_EQ(smooth.indices.size(), static_cast<size_t>(16 * 16 * 6));
    // Every vertex is a terrain lattice vertex lifted by a constant (so the overlay is in the surface).
    for (int j = 0; j < 17; j += 5)
        for (int i = 0; i < 17; i += 3) {
            EXPECT_FLOAT_EQ(at(smooth, i, j).x, static_cast<float>(i) * 10.0f);
            EXPECT_FLOAT_EQ(at(smooth, i, j).y, static_cast<float>(j) * 10.0f);
            EXPECT_NEAR(at(smooth, i, j).z, latticeHeight(i, j) + 0.35f, 1e-4f);
        }
    // Cell (0,0) is not flipped, cell (1,0) is flipped: the diagonals follow the terrain's.
    auto cellTriangles = [&](int i, int j) {
        const size_t first = (static_cast<size_t>(j) * 16 + i) * 6;
        return std::vector<unsigned short>(smooth.indices.begin() + first, smooth.indices.begin() + first + 6);
    };
    const unsigned short a0 = 0, a1 = 1, c0 = 17, c1 = 18;
    EXPECT_EQ(cellTriangles(0, 0), (std::vector<unsigned short>{a0, c1, c0, a0, a1, c1}));
    const unsigned short b0 = 1, b1 = 2, d0 = 18, d1 = 19;
    EXPECT_EQ(cellTriangles(1, 0), (std::vector<unsigned short>{b1, d0, b0, b1, d1, d0}));
    // Piecewise-constant sampling: inside the shrouded cell black, inside a clear cell white, fog about half.
    EXPECT_EQ(at(blocky, 5, 5).diffuse & 0xffffffu, 0x000000u);      // world (50,50): cell (1,1)
    EXPECT_EQ(at(blocky, 0, 0).diffuse & 0xffffffu, 0xffffffu);
    EXPECT_NEAR(static_cast<float>(at(blocky, 9, 5).diffuse & 0xff), 127.0f, 1.0f);   // world (90,50): cell (2,1)
    // Bilinear between cell centers (20 + 40 k): at the shrouded cell's own center the level is 0, at the
    // boundary to a clear cell it is halfway.
    EXPECT_EQ(at(smooth, 6, 6).diffuse & 0xff, 0u);                  // world (60,60): center of cell (1,1)
    const unsigned boundary = at(smooth, 8, 6).diffuse & 0xff;       // world (80,60): between (1,1)=0 and (2,1)=127
    EXPECT_NEAR(static_cast<float>(boundary), 63.5f, 1.5f);
    // A tinted shroud color multiplies the level.
    const float tint[3] = {1.0f, 0.5f, 0.0f};
    Mesh tinted;
    ASSERT_TRUE(NativeWorldEffects::Geometry::shroud(levels.data(), 4, 4, 40.0f, 0, false, 0, 0, 16, 16, &latticeHeight, &latticeFlip, tint, tinted));
    const unsigned fogged = at(tinted, 9, 5).diffuse;
    EXPECT_NEAR(static_cast<float>((fogged >> 16) & 0xff), 127.0f, 1.0f);
    EXPECT_NEAR(static_cast<float>((fogged >> 8) & 0xff), 64.0f, 1.0f);
    EXPECT_EQ(fogged & 0xff, 0u);
    // Outside the shroud grid the border level applies.
    Mesh outside;
    ASSERT_TRUE(NativeWorldEffects::Geometry::shroud(levels.data(), 4, 4, 40.0f, 77, false, -4, -4, 0, 0, &latticeHeight, nullptr, white, outside));
    EXPECT_EQ(outside.vertices[0].diffuse & 0xff, 77u);
}

namespace
{
// One opaque-alpha quad on the z=0 plane around the origin, in a single flat color.
void EmitColorQuad(D3D11Backend &backend, unsigned argb, int layer)
{
    WorldVertexXYZDUV2 v[4];
    const float h = 30.0f;
    const float xs[4] = {-h, h, h, -h}, ys[4] = {-h, -h, h, h};
    for (int i = 0; i < 4; ++i) {
        v[i].x = xs[i]; v[i].y = ys[i]; v[i].z = 0.0f; v[i].diffuse = argb;
        v[i].u0 = v[i].v0 = v[i].u1 = v[i].v1 = 0.0f;
    }
    static const unsigned short idx[6] = {0, 2, 1, 0, 3, 2};
    NativeWorldObjects::EffectBatch b = {};
    b.vertices = v; b.vertexCount = 4; b.indices = idx; b.indexCount = 6;
    b.blend = WorldBlend::SrcAlpha;
    b.depthWrite = false; b.doubleSided = true; b.noDiffuse = false;
    b.layer = layer;
    EXPECT_TRUE(NativeWorldObjects::emitEffect(backend, b));
}

std::vector<unsigned char> CenterPixel(const std::vector<unsigned char> &px, unsigned size)
{
    const size_t at = (static_cast<size_t>(size / 2) * size + size / 2) * 4;
    return {px[at], px[at + 1], px[at + 2]};
}
}

TEST(NativeWorldEffectsTest, EveryEffectLayerIsSubmittedAndDrawnInOrder)
{
    // Regression for the X64.29 review (no smoke, flames or explosions): the submission loop stopped one layer
    // short, so the particle layer was emitted, counted, and never drawn. Each layer must reach D3D11, later
    // layers must paint over earlier ones.
    Rig rig;
    ASSERT_TRUE(rig.backend);
    const int layers[] = {NativeWorldObjects::EffectLayerDecal, NativeWorldObjects::EffectLayerTranslucent,
                          NativeWorldObjects::EffectLayerOverlay, NativeWorldObjects::EffectLayerParticle};
    const unsigned colors[] = {0xffff0000u, 0xff00ff00u, 0xff0000ffu, 0xffffff00u};   // red, green, blue, yellow
    const unsigned char expectR[4] = {255, 0, 0, 255}, expectG[4] = {0, 255, 0, 255}, expectB[4] = {0, 0, 255, 0};

    // Each layer alone is visible.
    for (int i = 0; i < 4; ++i) {
        NativeWorldObjects::resetStatsForTests();
        NativeWorldObjects::setFrameEffectHookForTests([&](D3D11Backend &b) { EmitColorQuad(b, colors[i], layers[i]); });
        const std::vector<unsigned char> frame = rig.render(nullptr, 0);
        const std::vector<unsigned char> c = CenterPixel(frame, rig.size);
        EXPECT_EQ(c[0], expectR[i]) << "layer " << layers[i];
        EXPECT_EQ(c[1], expectG[i]) << "layer " << layers[i];
        EXPECT_EQ(c[2], expectB[i]) << "layer " << layers[i];
        EXPECT_GE(NativeWorldObjects::stats().layerDraws[layers[i]], 1u) << "layer " << layers[i];
    }

    // All layers at once, emitted in reverse order: the highest layer wins regardless of emission order.
    NativeWorldObjects::resetStatsForTests();
    NativeWorldObjects::setFrameEffectHookForTests([&](D3D11Backend &b) {
        for (int i = 3; i >= 0; --i) EmitColorQuad(b, colors[i], layers[i]);
    });
    std::vector<unsigned char> c = CenterPixel(rig.render(nullptr, 0), rig.size);
    EXPECT_EQ(c[0], 255); EXPECT_EQ(c[1], 255); EXPECT_EQ(c[2], 0);   // the particle layer (yellow) is on top
    for (int i = 0; i < 4; ++i) EXPECT_GE(NativeWorldObjects::stats().layerDraws[layers[i]], 1u);

    // Without the particle layer the shroud overlay is on top, then translucents, then decals.
    NativeWorldObjects::setFrameEffectHookForTests([&](D3D11Backend &b) {
        for (int i = 0; i < 3; ++i) EmitColorQuad(b, colors[i], layers[i]);
    });
    c = CenterPixel(rig.render(nullptr, 0), rig.size);
    EXPECT_EQ(c[0], 0); EXPECT_EQ(c[1], 0); EXPECT_EQ(c[2], 255);
    NativeWorldObjects::setFrameEffectHookForTests({});
}

TEST(NativeWorldEffectsTest, BillboardQuadIsCameraFacingAndRotates)
{
    const float c[3] = {10, 20, 30}, r[3] = {1, 0, 0}, u[3] = {0, 0, 1};
    Mesh m;
    NativeWorldEffects::Geometry::billboard(c, r, u, 8.0f, 0.0f, 0xff112233u, m);
    ASSERT_EQ(m.vertices.size(), 4u);
    EXPECT_NEAR(m.vertices[0].x, 10 - 4, 1e-4f);  EXPECT_NEAR(m.vertices[0].z, 30 + 4, 1e-4f);   // (-.5, .5)
    EXPECT_NEAR(m.vertices[2].x, 10 + 4, 1e-4f);  EXPECT_NEAR(m.vertices[2].z, 30 - 4, 1e-4f);   // (.5, -.5)
    EXPECT_FLOAT_EQ(m.vertices[0].u0, 0.0f); EXPECT_FLOAT_EQ(m.vertices[0].v0, 0.0f);
    EXPECT_FLOAT_EQ(m.vertices[2].u0, 1.0f); EXPECT_FLOAT_EQ(m.vertices[2].v0, 1.0f);
    EXPECT_EQ(m.indices.size(), 6u);
    Mesh rotated;
    NativeWorldEffects::Geometry::billboard(c, r, u, 8.0f, 3.14159265f * 0.5f, 0xffffffffu, rotated);
    // a quarter turn moves corner 0 (-.5,.5) to (-.5,-.5)
    EXPECT_NEAR(rotated.vertices[0].x, 10 - 4, 1e-3f);
    EXPECT_NEAR(rotated.vertices[0].z, 30 - 4, 1e-3f);
}

TEST(NativeWorldEffectsTest, LineRenderObjectsDrawThroughTheNativePath)
{
    NativeWorldObjects::resetStatsForTests();
    NativeWorldEffects::resetStatsForTests();
    Rig rig;
    ASSERT_TRUE(rig.backend);
    const std::vector<unsigned char> empty = rig.render(nullptr, 0);

    // Tracer/rope style box.
    Line3DClass *line = new Line3DClass(Vector3(-40, 0, 5), Vector3(40, 0, 5), 6.0f, 1.0f, 0.9f, 0.2f, 1.0f);
    NativeWorldObjects::ObjectInput input = {};
    input.object = line;
    input.opacity = 1.0f;
    const std::vector<unsigned char> withLine = rig.render(&input, 1);
    EXPECT_GT(Differing(empty, withLine), 60u);
    EXPECT_GE(NativeWorldEffects::stats().lineBoxes, 1u);
    EXPECT_EQ(NativeWorldObjects::stats().unsupportedClass, 0u);

    // Laser/projectile-stream style ribbon.
    SegmentedLineClass *seg = new SegmentedLineClass;
    Vector3 pts[3] = {Vector3(-40, 0, 20), Vector3(0, 0, 40), Vector3(40, 0, 20)};
    seg->Set_Points(3, pts);
    seg->Set_Width(8.0f);
    seg->Set_Color(Vector3(0.2f, 1.0f, 0.2f));
    seg->Set_Opacity(1.0f);
    seg->Set_Shader(ShaderClass::_PresetOpaqueSolidShader);
    input.object = seg;
    const std::vector<unsigned char> withSeg = rig.render(&input, 1);
    EXPECT_GT(Differing(empty, withSeg), 60u);
    EXPECT_GE(NativeWorldEffects::stats().segLines, 1u);
    EXPECT_GE(NativeWorldEffects::stats().segQuads, 2u);

    line->Release_Ref();
    seg->Release_Ref();
}

TEST(NativeWorldEffectsTest, DecalShadowsComeFromTheNativeProjectedShadowManager)
{
    NativeWorldObjects::resetStatsForTests();
    NativeWorldEffects::resetStatsForTests();
    Rig rig;
    ASSERT_TRUE(rig.backend);
    ASSERT_EQ(TheProjectedShadowManager, nullptr);
    NativeWorldEffects::install();
    ASSERT_NE(TheProjectedShadowManager, nullptr);

    const std::vector<unsigned char> before = rig.render(nullptr, 0);

    Shadow::ShadowTypeInfo info;
    info.m_type = SHADOW_ALPHA_DECAL;
    info.m_sizeX = info.m_sizeY = 60.0f;
    Shadow *decal = TheProjectedShadowManager->addDecal(&info);   // radius-decal style: not bound to an object
    ASSERT_NE(decal, nullptr);
    decal->setColor(0x00ff2020);
    decal->setOpacity(200);
    decal->setPosition(0.0f, 0.0f, 0.0f);
    decal->setAngle(0.3f);
    const std::vector<unsigned char> during = rig.render(nullptr, 0);
    EXPECT_GT(Differing(before, during), 200u);
    EXPECT_GE(NativeWorldEffects::stats().decals, 1u);

    // Disabled and invisible decals are not drawn.
    decal->enableShadowRender(FALSE);
    EXPECT_EQ(Differing(before, rig.render(nullptr, 0)), 0u);
    decal->enableShadowRender(TRUE);
    decal->enableShadowInvisible(true);
    EXPECT_EQ(Differing(before, rig.render(nullptr, 0)), 0u);
    decal->enableShadowInvisible(false);
    EXPECT_GT(Differing(before, rig.render(nullptr, 0)), 200u);

    decal->release();
    EXPECT_EQ(Differing(before, rig.render(nullptr, 0)), 0u);
    NativeWorldEffects::uninstall();
    EXPECT_EQ(TheProjectedShadowManager, nullptr);
}

TEST(NativeWorldEffectsTest, ShadowManagerSurvivesReleaseAfterUninstall)
{
    Rig rig;
    NativeWorldEffects::install();
    Shadow::ShadowTypeInfo info;
    info.m_type = SHADOW_DECAL;
    Shadow *shadow = TheProjectedShadowManager->addDecal(&info);
    ASSERT_NE(shadow, nullptr);
    NativeWorldEffects::uninstall();   // display detach comes before late drawable teardown in some paths
    shadow->release();                 // must not touch the removed manager
    SUCCEED();
}

TEST(NativeWorldEffectsTest, ObjectsDrawnThroughTheShroudAreDimmedLikeTheLegacyShroudPass)
{
    // A fogged ghost (or a partially clear object) is rendered with the shroud material pass: its lit color is
    // multiplied by the shroud level at its position. Live clear objects are untouched.
    const char *assets = std::getenv("GENERALS_OFFLINE_ASSETS");
    if (!assets || !*assets) GTEST_SKIP() << "GENERALS_OFFLINE_ASSETS not set (opt-in asset test)";
    std::string dir = assets;
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
    SimpleFileFactoryClass factory;
    factory.Set_Sub_Directory(dir.c_str());
    FileFactoryClass *previous = _TheFileFactory;
    _TheFileFactory = &factory;
    NativeWorldObjects::setFileReaderForTests([dir](const std::string &path, std::vector<unsigned char> &bytes) {
        std::string leaf = path;
        const size_t slash = leaf.find_last_of("\\/");
        if (slash != std::string::npos) leaf = leaf.substr(slash + 1);
        std::ifstream file(dir + leaf, std::ios::binary);
        if (!file) return false;
        bytes.assign(std::istreambuf_iterator<char>(file), {});
        return true;
    });
    {
        WW3DAssetManager manager;
        manager.Set_WW3D_Load_On_Demand(true);
        RenderObjClass *building = manager.Create_Render_Obj("ABBARRACKS");
        ASSERT_NE(building, nullptr);
        Rig rig(192);
        ASSERT_TRUE(rig.backend);
        rig.lights.count = 1;
        rig.lights.direction[0][0] = 0.3f; rig.lights.direction[0][1] = -0.4f; rig.lights.direction[0][2] = 0.85f;
        rig.lights.diffuse[0][0] = rig.lights.diffuse[0][1] = rig.lights.diffuse[0][2] = 0.8f;
        NativeWorldObjects::ObjectInput input = {};
        input.object = building;
        input.opacity = 1.0f;
        auto brightness = [](const std::vector<unsigned char> &px) {
            double sum = 0; unsigned n = 0;
            for (size_t i = 0; i + 3 < px.size(); i += 4) {
                const unsigned v = px[i] + px[i + 1] + px[i + 2];
                if (px[i] != 51 && v > 0) { sum += v; ++n; }
            }
            return n ? sum / n : 0.0;
        };
        const std::vector<unsigned char> clear = rig.render(&input, 1);
        ASSERT_GT(brightness(clear), 30.0);

        // Every shroud cell fogged (127): a shrouded object is about half as bright, a clear one unchanged.
        std::vector<unsigned char> levels(64 * 64, 127);
        NativeWorldEffects::setShroudForTests(64, 64, 40.0f, levels.data(), 127);
        const std::vector<unsigned char> clearInFog = rig.render(&input, 1);
        input.shrouded = true;
        const std::vector<unsigned char> fogged = rig.render(&input, 1);
        EXPECT_EQ(Differing(clear, clearInFog), 0u) << "a clear object is not shrouded";
        EXPECT_GT(Differing(clear, fogged), 500u);
        const double ratio = brightness(fogged) / std::max(1.0, brightness(clear));
        EXPECT_NEAR(ratio, 127.0 / 255.0, 0.12) << "fog level 127 halves the object's brightness";

        // Fully shrouded cells black the object out; fully clear cells leave it as it was.
        std::fill(levels.begin(), levels.end(), 0);
        NativeWorldEffects::setShroudForTests(64, 64, 40.0f, levels.data(), 0);
        EXPECT_LT(brightness(rig.render(&input, 1)), brightness(clear) * 0.05);
        std::fill(levels.begin(), levels.end(), 255);
        NativeWorldEffects::setShroudForTests(64, 64, 40.0f, levels.data(), 255);
        EXPECT_EQ(Differing(clear, rig.render(&input, 1)), 0u);

        NativeWorldEffects::uninstall();
        NativeWorldObjects::releaseAll();
        building->Release_Ref();
        manager.Free_Assets();
    }
    NativeWorldObjects::setFileReaderForTests({});
    _TheFileFactory = previous;
}

TEST(NativeWorldEffectsTest, PlanarVolumeShadowIsBlendedOncePerPixel)
{
    const char *assets = std::getenv("GENERALS_OFFLINE_ASSETS");
    if (!assets || !*assets) GTEST_SKIP() << "GENERALS_OFFLINE_ASSETS not set (opt-in asset test)";
    std::string dir = assets;
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
    SimpleFileFactoryClass factory;
    factory.Set_Sub_Directory(dir.c_str());
    FileFactoryClass *previous = _TheFileFactory;
    _TheFileFactory = &factory;
    NativeWorldObjects::setFileReaderForTests([dir](const std::string &path, std::vector<unsigned char> &bytes) {
        std::string leaf = path;
        const size_t slash = leaf.find_last_of("\\/");
        if (slash != std::string::npos) leaf = leaf.substr(slash + 1);
        std::ifstream file(dir + leaf, std::ios::binary);
        if (!file) return false;
        bytes.assign(std::istreambuf_iterator<char>(file), {});
        return true;
    });
    {
        WW3DAssetManager manager;
        manager.Set_WW3D_Load_On_Demand(true);
        RenderObjClass *tank = manager.Create_Render_Obj("AVPALADIN");
        ASSERT_NE(tank, nullptr);
        Rig rig(192);
        ASSERT_TRUE(rig.backend);
        NativeWorldEffects::resetStatsForTests();
        NativeWorldEffects::install();

        // The tank is far above the camera's ground so the projected silhouette is visible beside it.
        Matrix3D tm(1);
        tm.Set_Translation(Vector3(0.0f, 0.0f, 12.0f));
        tank->Set_Transform(tm);
        NativeWorldObjects::ObjectInput input = {};
        input.object = tank;
        input.hexColor = 0xff3070ff;
        input.opacity = 1.0f;
        const std::vector<unsigned char> noShadow = rig.render(&input, 1);

        // Other tests in this binary may have created the global data with shadow volumes off.
        const bool hadGlobals = TheWritableGlobalData != nullptr;
        const Bool oldVolumes = hadGlobals ? TheWritableGlobalData->m_useShadowVolumes : FALSE;
        if (hadGlobals) TheWritableGlobalData->m_useShadowVolumes = TRUE;
        Shadow::ShadowTypeInfo info;
        info.m_type = SHADOW_VOLUME;
        Shadow *first = TheProjectedShadowManager->addDecal(tank, &info);
        ASSERT_NE(first, nullptr);
        const std::vector<unsigned char> oneShadow = rig.render(&input, 1);
        const unsigned changed = Differing(noShadow, oneShadow);
        if (const char *out = std::getenv("GENERALS_OFFLINE_OUT")) WriteBmp(std::string(out) + "\\native_shadow.bmp", oneShadow, 192, 192);
        std::printf("shadow stats: seen=%u nogeom=%u culled=%u volume=%u\n", NativeWorldEffects::stats().shadowsSeen,
                    NativeWorldEffects::stats().shadowNoGeometry, NativeWorldEffects::stats().shadowCulled,
                    NativeWorldEffects::stats().volumeShadows);
        EXPECT_GT(changed, 150u);
        EXPECT_GE(NativeWorldEffects::stats().volumeShadows, 1u);
        std::printf("shadow reuse: hits=%u misses=%u uncached=%u radius=%.1f\n", NativeWorldEffects::stats().shadowCacheHits, NativeWorldEffects::stats().shadowCacheMisses, NativeWorldEffects::stats().shadowUncached, tank->Get_Bounding_Sphere().Radius);

        // X64.35 shadow-volume reuse: an unchanged caster is served from the cache on the next frame and the image is
        // bit-identical; moving the caster recomputes the silhouette (the image changes) and a return restores it.
        // (1) The full-size tank has a bounding radius above 30: it keeps the canonical path (counted as uncached).
        const unsigned uncachedBefore = NativeWorldEffects::stats().shadowUncached;
        const std::vector<unsigned char> sameFrame = rig.render(&input, 1);
        EXPECT_GT(NativeWorldEffects::stats().shadowUncached, uncachedBefore);
        EXPECT_EQ(Differing(oneShadow, sameFrame), 0u);
        // (2) Scaled to a planar caster (radius <= 30) the shadow is cached: miss, then exact hits.
        tank->Scale(0.5f);
        ASSERT_LE(tank->Get_Bounding_Sphere().Radius, 30.0f);
        const unsigned hitsBefore = NativeWorldEffects::stats().shadowCacheHits;
        const unsigned missesBefore = NativeWorldEffects::stats().shadowCacheMisses;
        const std::vector<unsigned char> planarFirst = rig.render(&input, 1);
        EXPECT_GT(NativeWorldEffects::stats().shadowCacheMisses, missesBefore);
        const std::vector<unsigned char> planarReused = rig.render(&input, 1);
        EXPECT_GT(NativeWorldEffects::stats().shadowCacheHits, hitsBefore);
        EXPECT_EQ(Differing(planarFirst, planarReused), 0u);   // the reused frame is bit-identical to the fresh one
        EXPECT_GT(Differing(noShadow, planarFirst), 0u);
        // (3) Moving the caster recomputes (new silhouette position); moving it back restores the earlier image.
        Matrix3D moved(1);
        moved.Set_Translation(Vector3(3.0f, 0.0f, 12.0f));
        tank->Set_Transform(moved);
        const unsigned missesBeforeMove = NativeWorldEffects::stats().shadowCacheMisses;
        const std::vector<unsigned char> movedImage = rig.render(&input, 1);
        EXPECT_GT(NativeWorldEffects::stats().shadowCacheMisses, missesBeforeMove);
        EXPECT_GT(Differing(planarFirst, movedImage), 0u);
        tank->Set_Transform(tm);
        const std::vector<unsigned char> restored = rig.render(&input, 1);
        EXPECT_EQ(Differing(planarFirst, restored), 0u);
        tank->Scale(2.0f);   // back to full size for the remaining assertions
        const std::vector<unsigned char> fullAgain = rig.render(&input, 1);
        EXPECT_EQ(Differing(oneShadow, fullAgain), 0u);

        // A second shadow of the same object covers the same pixels; the stencil lets each pixel blend once,
        // so the image is identical to the single-shadow image.
        Shadow *second = TheProjectedShadowManager->addDecal(tank, &info);
        ASSERT_NE(second, nullptr);
        const std::vector<unsigned char> twoShadows = rig.render(&input, 1);
        EXPECT_EQ(Differing(oneShadow, twoShadows), 0u);

        if (hadGlobals) TheWritableGlobalData->m_useShadowVolumes = oldVolumes;
        first->release();
        second->release();
        NativeWorldEffects::uninstall();
        NativeWorldObjects::releaseAll();
        tank->Release_Ref();
        manager.Free_Assets();
    }
    NativeWorldObjects::setFileReaderForTests({});
    _TheFileFactory = previous;
}
#endif
