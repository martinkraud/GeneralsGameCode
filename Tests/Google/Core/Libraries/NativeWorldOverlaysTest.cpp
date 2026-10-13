// SPDX-License-Identifier: GPL-3.0-or-later
// X64.30 world overlays: terrain tracks keep the legacy edge bookkeeping and are presented natively.
#include <gtest/gtest.h>
#if defined(_WIN64)
#include "Common/GameCommon.h"
#include "Common/GlobalData.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/NativeWorldEffects.h"
#include "W3DDevice/GameClient/NativeWorldObjects.h"
#include "W3DDevice/GameClient/W3DTerrainTracks.h"
#include "WW3D2/segline.h"
#include "WW3D2/shader.h"
#if RTS_GENERALS
#include "W3DDevice/GameClient/W3DBibBuffer.h"
#endif
#include "WW3D2/Backend/D3D11Backend.h"
#include "WW3D2/camera.h"
#include "WWMath/matrix3d.h"
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace
{
// Flat ground with an up-pointing normal (the base TerrainLogic returns a zero normal, which has no width).
class FlatTerrainLogic : public TerrainLogic
{
public:
    Real getGroundHeight(Real, Real, Coord3D *normal = nullptr) const override
    {
        if (normal) { normal->x = 0; normal->y = 0; normal->z = 1; }
        return 0.0f;
    }
    Real getLayerHeight(Real, Real, PathfindLayerEnum, Coord3D *normal = nullptr, Bool = true) const override
    {
        if (normal) { normal->x = 0; normal->y = 0; normal->z = 1; }
        return 0.0f;
    }
};

struct Globals
{
    bool madeData = false;
    TerrainLogic *previousTerrain = nullptr;
    Bool oldMarks = FALSE;
    Int oldTracks = 0, oldEdges = 0, oldOpaque = 0, oldFade = 0;
    Globals()
    {
        if (!TheWritableGlobalData) { TheWritableGlobalData = new GlobalData; madeData = true; }
        oldMarks = TheWritableGlobalData->m_makeTrackMarks;
        oldTracks = TheWritableGlobalData->m_maxTerrainTracks;
        oldEdges = TheWritableGlobalData->m_maxTankTrackEdges;
        oldOpaque = TheWritableGlobalData->m_maxTankTrackOpaqueEdges;
        oldFade = TheWritableGlobalData->m_maxTankTrackFadeDelay;
        TheWritableGlobalData->m_makeTrackMarks = TRUE;
        TheWritableGlobalData->m_maxTerrainTracks = 4;
        TheWritableGlobalData->m_maxTankTrackEdges = 40;
        TheWritableGlobalData->m_maxTankTrackOpaqueEdges = 10;
        TheWritableGlobalData->m_maxTankTrackFadeDelay = 3000;
        TheWritableGlobalData->m_terrainAmbient[0].red = 0.4f;
        TheWritableGlobalData->m_terrainAmbient[0].green = 0.4f;
        TheWritableGlobalData->m_terrainAmbient[0].blue = 0.4f;
        TheWritableGlobalData->m_terrainDiffuse[0].red = 0.5f;
        TheWritableGlobalData->m_terrainDiffuse[0].green = 0.5f;
        TheWritableGlobalData->m_terrainDiffuse[0].blue = 0.5f;
        // Always our own flat ground: other tests leave a base TerrainLogic (zero normals) installed.
        previousTerrain = TheTerrainLogic;
        TheTerrainLogic = new FlatTerrainLogic;
    }
    ~Globals()
    {
        delete TheTerrainLogic;
        TheTerrainLogic = previousTerrain;
        TheWritableGlobalData->m_makeTrackMarks = oldMarks;
        TheWritableGlobalData->m_maxTerrainTracks = oldTracks;
        TheWritableGlobalData->m_maxTankTrackEdges = oldEdges;
        TheWritableGlobalData->m_maxTankTrackOpaqueEdges = oldOpaque;
        TheWritableGlobalData->m_maxTankTrackFadeDelay = oldFade;
        if (madeData) { delete TheWritableGlobalData; TheWritableGlobalData = nullptr; }
    }
};

struct Strip
{
    std::string texture;
    std::vector<TerrainTracksRenderObjClassSystem::NativeTrackVertex> vertices;
    std::vector<unsigned short> indices;
};
std::vector<Strip> Collect(TerrainTracksRenderObjClassSystem &system)
{
    std::vector<Strip> strips;
    system.forEachNativeTrack([](void *user, const char *texture, const TerrainTracksRenderObjClassSystem::NativeTrackVertex *v,
                                 unsigned vertexCount, const unsigned short *i, unsigned indexCount) {
        Strip s;
        s.texture = texture;
        s.vertices.assign(v, v + vertexCount);
        s.indices.assign(i, i + indexCount);
        static_cast<std::vector<Strip> *>(user)->push_back(std::move(s));
    }, &strips);
    return strips;
}
unsigned Alpha(const TerrainTracksRenderObjClassSystem::NativeTrackVertex &v) { return v.diffuse >> 24; }
unsigned Differing(const std::vector<unsigned char> &a, const std::vector<unsigned char> &b)
{
    unsigned n = 0;
    for (size_t i = 0; i + 3 < a.size() && i + 3 < b.size(); i += 4)
        if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) ++n;
    return n;
}
}

TEST(NativeWorldOverlaysTest, TerrainTracksKeepTheLegacyEdgeBookkeepingAndProduceTheFlushGeometry)
{
    Globals globals;
    TerrainTracksRenderObjClassSystem *previous = TheTerrainTracksRenderObjClassSystem;
    TerrainTracksRenderObjClassSystem system;
    TheTerrainTracksRenderObjClassSystem = &system;
    system.initNative();
    ASSERT_TRUE(system.isNative());

    // The "vehicle": any render object (the default spacing applies without TREADFX bones).
    TerrainTracksRenderObjClass *vehicle = new TerrainTracksRenderObjClass;
    TerrainTracksRenderObjClass *track = system.bindTrack(vehicle, 10.0f, "ExTrackMarks.tga");
    ASSERT_NE(track, nullptr);

    // Nothing to draw before two edges exist (the first call only anchors).
    EXPECT_TRUE(Collect(system).empty());
    track->addEdgeToTrack(0.0f, 0.0f);
    track->addEdgeToTrack(0.0f, 0.0f);   // too close: ignored
    EXPECT_TRUE(Collect(system).empty());

    // A straight run along +x: one edge per 10 units (the system's length), 6 edges.
    for (int i = 1; i <= 6; ++i) track->addEdgeToTrack(10.0f * i, 0.0f);
    std::vector<Strip> strips = Collect(system);
    ASSERT_EQ(strips.size(), 1u);
    const Strip &s = strips[0];
    EXPECT_EQ(s.texture, "ExTrackMarks.tga");
    ASSERT_EQ(s.vertices.size(), 12u);            // 6 edges x (left, right)
    ASSERT_EQ(s.indices.size(), 5u * 6u);         // (edges - 1) quads of two triangles
    for (unsigned short i : s.indices) EXPECT_LT(i, s.vertices.size());
    // Each edge straddles the path (y = -w/2, +w/2), is raised 0.2 * MAP_XY_FACTOR above the ground and the
    // UVs alternate between edges exactly like the legacy table.
    const float half = std::fabs(s.vertices[0].y);
    EXPECT_GT(half, 0.5f);
    for (unsigned e = 0; e < 6; ++e) {
        const auto &l = s.vertices[e * 2], &r = s.vertices[e * 2 + 1];
        EXPECT_FLOAT_EQ(l.x, 10.0f * (e + 1));
        EXPECT_FLOAT_EQ(r.x, l.x);
        EXPECT_NEAR(l.y, -r.y, 1e-4f);
        EXPECT_NE(l.y, r.y);
        EXPECT_FLOAT_EQ(l.z, 2.0f);
        EXPECT_FLOAT_EQ(l.u, 0.0f); EXPECT_FLOAT_EQ(r.u, 1.0f);
        EXPECT_FLOAT_EQ(l.v, r.v);
        if (e) EXPECT_NE(l.v, s.vertices[(e - 1) * 2].v);
    }
    // Terrain shade: ambient + diffuse/2 = 0.65 of 255 (truncated: 165) in every channel; the first edge starts transparent
    // (track restart smoothing), later edges are opaque while within the opaque run.
    EXPECT_EQ(s.vertices[0].diffuse & 0xffffff, 0xa5a5a5u);
    EXPECT_EQ(Alpha(s.vertices[0]), 0u);
    EXPECT_EQ(Alpha(s.vertices[10]), 255u);

    // Capping (object entering the fog, going airborne...) fades the last edge to nothing.
    track->addCapEdgeToTrack(80.0f, 0.0f);
    strips = Collect(system);
    ASSERT_EQ(strips.size(), 1u);
    EXPECT_EQ(Alpha(strips[0].vertices[strips[0].vertices.size() - 1]), 0u);

    // The ring buffer is bounded by MaxTankTrackEdges: older edges are overwritten.
    for (int i = 9; i < 120; ++i) track->addEdgeToTrack(10.0f * i, 0.0f);
    strips = Collect(system);
    ASSERT_EQ(strips.size(), 1u);
    EXPECT_EQ(strips[0].vertices.size(), 40u * 2u);
    // Older edges of a long run fade with distance behind the vehicle (the opaque edges stay fully visible).
    const auto &v = strips[0].vertices;
    EXPECT_LT(Alpha(v[2]), Alpha(v[v.size() - 2]));
    EXPECT_EQ(Alpha(v[v.size() - 2]), 255u);

    // Disabled track marks produce nothing.
    TheWritableGlobalData->m_makeTrackMarks = FALSE;
    EXPECT_TRUE(Collect(system).empty());
    TheWritableGlobalData->m_makeTrackMarks = TRUE;

    system.unbindTrack(track);
    vehicle->Release_Ref();
    system.Reset();
    EXPECT_TRUE(Collect(system).empty());
    TheTerrainTracksRenderObjClassSystem = previous;
}

TEST(NativeWorldOverlaysTest, TerrainTracksReachTheFrameThroughTheDecalLayer)
{
    Globals globals;
    NativeWorldObjects::setRenderTargetSizeForTests(128, 128);
    D3D11Backend::Configuration c;
    c.software = true; c.width = c.height = 128; c.depth_buffer = true;
    auto backend = D3D11Backend::Create(c);
    ASSERT_TRUE(backend);
    CameraClass camera;
    camera.Set_Aspect_Ratio(1.0f);
    camera.Set_View_Plane(DEG_TO_RADF(45.0f), -1);
    camera.Set_Clip_Planes(1.0f, 2000.0f);
    camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
    Matrix3D cam(1);
    cam.Look_At(Vector3(0.0f, -1.0f, 150.0f), Vector3(0.0f, 0.0f, 0.0f), 0.0f);   // looking straight down
    camera.Set_Transform(cam);
    NativeWorldObjects::SceneLighting lights = {};
    lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.5f;

    TerrainTracksRenderObjClassSystem *previous = TheTerrainTracksRenderObjClassSystem;
    TerrainTracksRenderObjClassSystem system;
    TheTerrainTracksRenderObjClassSystem = &system;
    system.initNative();
    TerrainTracksRenderObjClass *vehicle = new TerrainTracksRenderObjClass;
    TerrainTracksRenderObjClass *track = system.bindTrack(vehicle, 10.0f, "ExTrackMarks.tga");
    ASSERT_NE(track, nullptr);

    auto frame = [&]() {
        NativeWorldObjects::resetStatsForTests();
        NativeWorldEffects::resetStatsForTests();
        backend->Clear(true, true, Vector3(0.2f, 0.25f, 0.3f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        NativeWorldObjects::renderObjects(*backend, camera, lights, lights, nullptr, 0);
        backend->End_Scene(false);
        return backend->Readback_RGBA8();
    };
    const std::vector<unsigned char> before = frame();
    EXPECT_EQ(NativeWorldEffects::stats().tracks, 0u);
    for (int i = 0; i <= 8; ++i) track->addEdgeToTrack(-40.0f + 10.0f * i, 0.0f);
    const std::vector<unsigned char> during = frame();
    EXPECT_EQ(NativeWorldEffects::stats().tracks, 1u);
    EXPECT_GE(NativeWorldObjects::stats().layerDraws[NativeWorldObjects::EffectLayerDecal], 1u);
    EXPECT_GT(Differing(before, during), 30u);   // the strip covers a visible patch of the frame

    system.unbindTrack(track);
    vehicle->Release_Ref();
    system.Reset();
    EXPECT_EQ(Differing(before, frame()), 0u);
    NativeWorldObjects::releaseAll();
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
    TheTerrainTracksRenderObjClassSystem = previous;
}
#if RTS_GENERALS
namespace
{
struct Seen { unsigned normal = 0, highlighted = 0; std::vector<float> xs; std::vector<bool> order; };
}

TEST(NativeWorldOverlaysTest, BibListKeepsTheLegacySemanticsAndDrawsNormalBeforeHighlighted)
{
    W3DBibBuffer bibs(TRUE);   // native: CPU list only
    auto corners = [](float x) {
        Vector3 c[4] = {Vector3(x, 0, 5), Vector3(x + 10, 0, 5), Vector3(x + 10, 10, 5), Vector3(x, 10, 5)};
        return std::vector<Vector3>(c, c + 4);
    };
    auto add = [&](float x, int id, bool highlight) { auto c = corners(x); bibs.addBib(c.data(), static_cast<ObjectID>(id), highlight); };
    auto collect = [&]() {
        Seen seen;
        bibs.forEachNativeBib([](void *user, const Vector3 c[4], bool highlight) {
            Seen &s = *static_cast<Seen *>(user);
            s.xs.push_back(c[0].X); s.order.push_back(highlight);
            (highlight ? s.highlighted : s.normal)++;
        }, &seen);
        return seen;
    };
    add(0, 1, true);     // highlighted first, but drawn after the normal ones
    add(100, 2, false);
    add(200, 3, false);
    Seen s = collect();
    EXPECT_EQ(s.normal, 2u); EXPECT_EQ(s.highlighted, 1u);
    ASSERT_EQ(s.xs.size(), 3u);
    EXPECT_FALSE(s.order[0]); EXPECT_FALSE(s.order[1]); EXPECT_TRUE(s.order[2]);
    EXPECT_FLOAT_EQ(s.xs[2], 0.0f);
    // The same object id replaces its bib (placement ghost following the cursor).
    add(50, 1, true);
    s = collect();
    ASSERT_EQ(s.xs.size(), 3u);
    EXPECT_FLOAT_EQ(s.xs[2], 50.0f);
    // Removal frees the slot and removeHighlighting turns every bib normal.
    bibs.removeBib(static_cast<ObjectID>(2));
    EXPECT_EQ(collect().xs.size(), 2u);
    bibs.removeHighlighting();
    s = collect();
    EXPECT_EQ(s.highlighted, 0u);
    bibs.clearAllBibs();
    EXPECT_TRUE(collect().xs.empty());
}
#endif

TEST(NativeWorldOverlaysTest, QueuedGroundLinesAreSnapshottedAndDrawnOnTheGroundLayer)
{
    NativeWorldObjects::setRenderTargetSizeForTests(128, 128);
    D3D11Backend::Configuration c;
    c.software = true; c.width = c.height = 128; c.depth_buffer = true;
    auto backend = D3D11Backend::Create(c);
    ASSERT_TRUE(backend);
    CameraClass camera;
    camera.Set_Aspect_Ratio(1.0f);
    camera.Set_View_Plane(DEG_TO_RADF(45.0f), -1);
    camera.Set_Clip_Planes(1.0f, 2000.0f);
    camera.Set_Viewport(Vector2(0, 0), Vector2(1, 1));
    Matrix3D cam(1);
    cam.Look_At(Vector3(0.0f, -1.0f, 150.0f), Vector3(0.0f, 0.0f, 0.0f), 0.0f);
    camera.Set_Transform(cam);
    NativeWorldObjects::SceneLighting lights = {};
    lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.5f;

    auto frame = [&]() {
        NativeWorldObjects::resetStatsForTests();
        NativeWorldEffects::resetStatsForTests();
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        NativeWorldObjects::renderObjects(*backend, camera, lights, lights, nullptr, 0);
        backend->End_Scene(false);
        return backend->Readback_RGBA8();
    };
    const std::vector<unsigned char> empty = frame();
    EXPECT_EQ(NativeWorldEffects::stats().groundLines, 0u);

    // The waypoint buffer reuses one line object for every path of a frame: the queue must copy it.
    SegmentedLineClass *line = new SegmentedLineClass;
    ShaderClass shader = ShaderClass::_PresetAdditiveShader;
    shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
    line->Set_Shader(shader);
    line->Set_Width(6.0f);
    line->Set_Color(Vector3(0.25f, 0.5f, 1.0f));
    Vector3 first[2] = {Vector3(-40, -20, 0), Vector3(40, -20, 0)};
    line->Set_Points(2, first);
    NativeWorldEffects::queueGroundLine(*line);
    Vector3 second[2] = {Vector3(-40, 20, 0), Vector3(40, 20, 0)};
    line->Set_Points(2, second);
    NativeWorldEffects::queueGroundLine(*line);
    line->Release_Ref();

    const std::vector<unsigned char> drawn = frame();
    EXPECT_EQ(NativeWorldEffects::stats().groundLines, 2u);
    EXPECT_GE(NativeWorldObjects::stats().layerDraws[NativeWorldObjects::EffectLayerGround], 2u);
    unsigned differing = 0;
    for (size_t i = 0; i + 3 < empty.size(); i += 4)
        if (empty[i] != drawn[i] || empty[i + 1] != drawn[i + 1] || empty[i + 2] != drawn[i + 2]) ++differing;
    EXPECT_GT(differing, 100u);                 // two ribbons across the view
    // The queue is consumed: nothing is drawn on the next frame.
    EXPECT_EQ(NativeWorldEffects::stats().groundLines, 2u);
    const std::vector<unsigned char> after = frame();
    EXPECT_EQ(NativeWorldEffects::stats().groundLines, 0u);
    EXPECT_EQ(after, empty);
    NativeWorldObjects::releaseAll();
    NativeWorldObjects::setRenderTargetSizeForTests(0, 0);
}

#endif
