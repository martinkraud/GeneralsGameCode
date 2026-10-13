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

float ground(float x, float y) { return 2.0f + 0.01f * x + 0.02f * y; }

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

TEST(NativeWorldEffectsTest, ShroudOverlayFollowsTheTerrainAndTheCellLevels)
{
    // 4x4 cells of 40 units; everything clear (255) except cell (1,1) shrouded (0) and (2,1) fogged (127).
    std::vector<unsigned char> levels(16, 255);
    levels[1 * 4 + 1] = 0;
    levels[1 * 4 + 2] = 127;
    const float white[3] = {1.0f, 1.0f, 1.0f};
    Mesh smooth, blocky;
    ASSERT_TRUE(NativeWorldEffects::Geometry::shroud(levels.data(), 4, 4, 40.0f, 0, true, 0, 0, 4, 4, 2, &ground, white, smooth));
    ASSERT_TRUE(NativeWorldEffects::Geometry::shroud(levels.data(), 4, 4, 40.0f, 0, false, 0, 0, 4, 4, 2, &ground, white, blocky));
    const int vx = 4 * 2 + 1;
    auto at = [&](const Mesh &m, int i, int j) { return m.vertices[static_cast<size_t>(j) * vx + i]; };
    // Heights come from the supplied terrain plus the bias.
    EXPECT_NEAR(at(smooth, 3, 5).z, ground(at(smooth, 3, 5).x, at(smooth, 3, 5).y) + 0.6f, 1e-4f);
    // Blocky: inside the shrouded cell the overlay is black, inside a clear cell white.
    EXPECT_EQ(at(blocky, 2, 2).diffuse & 0xffffffu, 0x000000u);   // world (40,40): cell (1,1) corner -> cell (1,1)
    EXPECT_EQ(at(blocky, 0, 0).diffuse & 0xffffffu, 0xffffffu);
    // Smooth: the same vertex lies between texel centers, so it is a blend (neither black nor white).
    const unsigned s = at(smooth, 2, 2).diffuse & 0xff;
    EXPECT_GT(s, 0u);
    EXPECT_LT(s, 255u);
    // Fogged cell is dimmed to about half.
    const unsigned f = at(blocky, 5, 3).diffuse & 0xff;   // world (100,60): cell (2,1)
    EXPECT_NEAR(static_cast<float>(f), 127.0f, 1.0f);
    // A tinted shroud color multiplies the level.
    const float tint[3] = {1.0f, 0.5f, 0.0f};
    Mesh tinted;
    ASSERT_TRUE(NativeWorldEffects::Geometry::shroud(levels.data(), 4, 4, 40.0f, 0, false, 0, 0, 4, 4, 1, &ground, tint, tinted));
    const unsigned fogged = tinted.vertices[static_cast<size_t>(1) * 5 + 2].diffuse;   // cell (2,1)
    EXPECT_NEAR(static_cast<float>((fogged >> 16) & 0xff), 127.0f, 1.0f);
    EXPECT_NEAR(static_cast<float>((fogged >> 8) & 0xff), 64.0f, 1.0f);
    EXPECT_EQ(fogged & 0xff, 0u);
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
