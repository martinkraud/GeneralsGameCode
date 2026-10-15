// SPDX-License-Identifier: GPL-3.0-or-later
// See NativeWorldEffects.h. Producers: line render objects and particle systems (this file section);
// terrain decals, planar shadows, water and shroud follow in the same translation unit.
#if defined(_WIN64)
#include "W3DDevice/GameClient/NativeWorldEffects.h"
#include "Common/PerformanceProfile.h"
#include "W3DDevice/GameClient/W3DTerrainTracks.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/NativeWorldObjects.h"
#include "Common/GlobalData.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Shadow.h"
#include "GameClient/Water.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include <WW3D2/hlod.h>
#include <WW3D2/mesh.h>
#include <WW3D2/meshmdl.h>
#include <WW3D2/matinfo.h>
#include <cstdio>
#include <cstring>
#include <deque>
#include <WW3D2/camera.h>
#include <WW3D2/line3d.h>
#include <WW3D2/segline.h>
#include <WW3D2/shader.h>
#include <WW3D2/texture.h>
#include <WW3D2/ww3d.h>
#include <WW3D2/Backend/D3D11Backend.h>
#include <WW3D2/WorldSubmission.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace NativeWorldEffects
{
namespace
{
Stats g_stats = {};

unsigned PackColor(float r, float g, float b, float a)
{
    auto c = [](float v) { return static_cast<unsigned>(std::max(0.0f, std::min(255.0f, v * 255.0f + 0.5f))); };
    return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
}

Vector3 Normalized(const Vector3 &v, const Vector3 &fallback)
{
    const float len = std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
    if (len < 1e-6f) return fallback;
    return Vector3(v.X / len, v.Y / len, v.Z / len);
}

NativeWorldObjects::EffectBatch BaseBatch(const NativeWorldObjects::EffectState &state, float distance, int layer)
{
    NativeWorldObjects::EffectBatch b = {};
    b.blend = state.blend;
    b.depthWrite = state.depthWrite;
    b.doubleSided = state.doubleSided;
    b.noDiffuse = state.noDiffuse;
    b.alphaTest = state.alphaTest;
    b.distance = distance;
    b.layer = layer;
    return b;
}

// Appends a camera-facing ribbon through `points` (world space). Per-point half width, color and
// V coordinate; U is 0 on the left edge and 1 on the right edge (the legacy segmented-line layout).
void AppendRibbon(std::vector<WorldVertexXYZDUV2> &vertices, std::vector<unsigned short> &indices,
                  const std::vector<Vector3> &points, const std::vector<float> &halfWidth,
                  const std::vector<unsigned> &colors, const std::vector<float> &v, const Vector3 &camera,
                  float uLeft, float uRight, float uOffset, float vOffset)
{
    const size_t n = points.size();
    if (n < 2) return;
    const unsigned short base = static_cast<unsigned short>(vertices.size());
    for (size_t i = 0; i < n; ++i) {
        const Vector3 prev = points[i > 0 ? i - 1 : 0];
        const Vector3 next = points[i + 1 < n ? i + 1 : n - 1];
        const Vector3 tangent = Normalized(next - prev, Vector3(1, 0, 0));
        const Vector3 toCamera = Normalized(camera - points[i], Vector3(0, 0, 1));
        Vector3 side;
        Vector3::Cross_Product(tangent, toCamera, &side);
        side = Normalized(side, Vector3(0, 1, 0));
        side *= halfWidth[i];
        WorldVertexXYZDUV2 l, r;
        l.x = points[i].X + side.X; l.y = points[i].Y + side.Y; l.z = points[i].Z + side.Z;
        r.x = points[i].X - side.X; r.y = points[i].Y - side.Y; r.z = points[i].Z - side.Z;
        l.diffuse = r.diffuse = colors[i];
        l.u0 = l.u1 = uLeft + uOffset;  l.v0 = l.v1 = v[i] + vOffset;
        r.u0 = r.u1 = uRight + uOffset; r.v0 = r.v1 = v[i] + vOffset;
        vertices.push_back(l);
        vertices.push_back(r);
    }
    for (size_t i = 0; i + 1 < n; ++i) {
        const unsigned short a = static_cast<unsigned short>(base + i * 2), b = static_cast<unsigned short>(a + 1);
        const unsigned short c = static_cast<unsigned short>(a + 2), d = static_cast<unsigned short>(a + 3);
        indices.push_back(a); indices.push_back(b); indices.push_back(c);
        indices.push_back(b); indices.push_back(d); indices.push_back(c);
    }
}

} // namespace

Stats stats()
{
    return g_stats;
}

void resetStatsForTests()
{
    g_stats = Stats{};
}

bool emitLine3D(D3D11Backend &backend, Line3DClass &line, float distanceSquared)
{
    if (!StartupDiagnostics::nativeFeature("lines")) return false;
    const Matrix3D &tm = line.Get_Transform();
    const Vector3 *box = line.Native_Vertices();
    const Vector4 &color = line.Native_Color();
    WorldVertexXYZDUV2 vertices[8];
    const unsigned packed = PackColor(color.X, color.Y, color.Z, color.W);
    for (int i = 0; i < 8; ++i) {
        Vector3 world;
        Matrix3D::Transform_Vector(tm, box[i], &world);
        vertices[i].x = world.X; vertices[i].y = world.Y; vertices[i].z = world.Z;
        vertices[i].diffuse = packed;
        vertices[i].u0 = vertices[i].v0 = vertices[i].u1 = vertices[i].v1 = 0.0f;
    }
    const NativeWorldObjects::EffectState state =
        NativeWorldObjects::EffectStateFromShader(line.Native_Shader().Get_Bits(), false);
    NativeWorldObjects::EffectBatch b = BaseBatch(state, distanceSquared, NativeWorldObjects::EffectLayerTranslucent);
    b.vertices = vertices;
    b.vertexCount = 8;
    b.indices = Line3DClass::Native_Indices();
    b.indexCount = 36;
    b.texture = nullptr;
    b.noDiffuse = false;   // PRELIT_DIFFUSE material: the vertex color is the color
    if (!NativeWorldObjects::emitEffect(backend, b)) { ++g_stats.dropped; return false; }
    ++g_stats.lineBoxes;
    return true;
}

namespace
{
// Everything a segmented line needs to become a ribbon, independent of the render object.
struct SegLineData
{
    std::vector<Vector3> points;       // world space
    Vector3 rgb;
    float opacity;
    float width;
    SegLineRendererClass::TextureMapMode mode;
    float tile;
    Vector2 uvRate;
    std::string texture;               // empty = untextured
    unsigned shaderBits;
};
std::vector<SegLineData> g_groundLines;   // snapshots queued by queueGroundLine

bool EmitSegLineData(D3D11Backend &backend, const SegLineData &data, const Vector3 &cameraPosition,
                     float distanceSquared, int layer, bool depthAlways);
}

bool emitSegLine(D3D11Backend &backend, SegmentedLineClass &line, const Vector3 &cameraPosition, float distanceSquared)
{
    if (!StartupDiagnostics::nativeFeature("lines")) return false;
    const int count = line.Get_Num_Points();
    if (count < 2 || count > 512) return false;
    const Matrix3D &tm = line.Get_Transform();
    SegLineData data;
    data.points.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Vector3 local;
        line.Get_Point_Location(static_cast<unsigned>(i), local);
        Matrix3D::Transform_Vector(tm, local, &data.points[static_cast<size_t>(i)]);
    }
    line.Get_Color(data.rgb);
    data.opacity = line.Get_Opacity();
    data.width = line.Get_Width();
    data.mode = line.Get_Texture_Mapping_Mode();
    data.tile = line.Get_Texture_Tile_Factor();
    data.uvRate = line.Get_UV_Offset_Rate();
    data.shaderBits = line.Get_Shader().Get_Bits();
    if (TextureClass *texture = line.Get_Texture()) {
        data.texture = texture->Get_Texture_Name().str();
        texture->Release_Ref();
    }
    return EmitSegLineData(backend, data, cameraPosition, distanceSquared, NativeWorldObjects::EffectLayerTranslucent, false);
}

void queueGroundLine(SegmentedLineClass &line)
{
    if (!StartupDiagnostics::nativeFeature("waypoints")) return;
    const int count = line.Get_Num_Points();
    if (count < 2 || count > 512 || g_groundLines.size() >= 64) return;
    const Matrix3D &tm = line.Get_Transform();
    SegLineData data;
    data.points.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Vector3 local;
        line.Get_Point_Location(static_cast<unsigned>(i), local);
        Matrix3D::Transform_Vector(tm, local, &data.points[static_cast<size_t>(i)]);
    }
    line.Get_Color(data.rgb);
    data.opacity = line.Get_Opacity();
    data.width = line.Get_Width();
    data.mode = line.Get_Texture_Mapping_Mode();
    data.tile = line.Get_Texture_Tile_Factor();
    data.uvRate = line.Get_UV_Offset_Rate();
    data.shaderBits = line.Get_Shader().Get_Bits();
    if (TextureClass *texture = line.Get_Texture()) {
        data.texture = texture->Get_Texture_Name().str();
        texture->Release_Ref();
    }
    g_groundLines.push_back(std::move(data));
}

namespace
{
bool EmitSegLineData(D3D11Backend &backend, const SegLineData &data, const Vector3 &cameraPosition,
                     float distanceSquared, int layer, bool depthAlways)
{
    const std::vector<Vector3> &points = data.points;
    const unsigned packed = PackColor(data.rgb.X, data.rgb.Y, data.rgb.Z, data.opacity);
    const float half = data.width * 0.5f;
    const SegLineRendererClass::TextureMapMode mode = data.mode;
    const float tile = data.tile;

    std::vector<float> halfWidth(points.size(), half), v(points.size(), 0.0f);
    std::vector<unsigned> colors(points.size(), packed);
    float uLeft = 0.0f, uRight = 1.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        switch (mode) {
        case SegLineRendererClass::UNIFORM_WIDTH_TEXTURE_MAP: v[i] = 0.0f; uLeft = 0.0f; uRight = 1.0f; break;
        case SegLineRendererClass::UNIFORM_LENGTH_TEXTURE_MAP: v[i] = static_cast<float>(i) * tile; uLeft = uRight = 0.0f; break;
        default: v[i] = static_cast<float>(i) * tile; uLeft = 0.0f; uRight = 1.0f; break;
        }
    }
    // Scrolling texture (energy beams): rate per second of logic time, wrapped.
    const Vector2 rate = data.uvRate;
    const float seconds = static_cast<float>(WW3D::Get_Logic_Time_Milliseconds()) * 0.001f;
    float uo = rate.X * seconds, vo = rate.Y * seconds;
    uo -= std::floor(uo); vo -= std::floor(vo);

    std::vector<WorldVertexXYZDUV2> vertices;
    std::vector<unsigned short> indices;
    AppendRibbon(vertices, indices, points, halfWidth, colors, v, cameraPosition, uLeft, uRight, uo, vo);
    if (vertices.empty()) return false;

    Texture2DResidency *residency = nullptr;
    if (!data.texture.empty())
        residency = NativeWorldObjects::effectTexture(backend, data.texture.c_str());
    const NativeWorldObjects::EffectState state = NativeWorldObjects::EffectStateFromShader(data.shaderBits, true);
    NativeWorldObjects::EffectBatch b = BaseBatch(state, distanceSquared, layer);
    b.vertices = vertices.data();
    b.vertexCount = static_cast<unsigned>(vertices.size());
    b.indices = indices.data();
    b.indexCount = static_cast<unsigned>(indices.size());
    b.texture = residency;
    b.doubleSided = true;
    b.noDiffuse = false;   // the renderer enables the gradient: the ribbon color/opacity is the vertex color
    b.depthAlways = depthAlways;
    if (!NativeWorldObjects::emitEffect(backend, b)) { ++g_stats.dropped; return false; }
    ++g_stats.segLines;
    g_stats.segQuads += static_cast<unsigned>(points.size() - 1);
    return true;
}
}

namespace
{
// ---------------------------------------------------------------------------------------------
// Particle systems (W3DParticleSystemManager::doParticles equivalent).
// ---------------------------------------------------------------------------------------------
unsigned ShaderBitsFor(ParticleSystemInfo::ParticleShaderType type)
{
    switch (type) {
    case ParticleSystemInfo::ADDITIVE: return ShaderClass::_PresetAdditiveSpriteShader.Get_Bits();
    case ParticleSystemInfo::ALPHA_TEST: return ShaderClass::_PresetATestSpriteShader.Get_Bits();
    case ParticleSystemInfo::MULTIPLY: return ShaderClass::_PresetMultiplicativeSpriteShader.Get_Bits();
    case ParticleSystemInfo::ALPHA:
    default: return ShaderClass::_PresetAlphaSpriteShader.Get_Bits();
    }
}

struct ParticleQuadBuilder
{
    Geometry::Mesh mesh;
    std::vector<WorldVertexXYZDUV2> &vertices = mesh.vertices;
    std::vector<unsigned short> &indices = mesh.indices;
    Vector3 right, up;
    Vector3 cameraPosition;

    void Reset() { vertices.clear(); indices.clear(); }

    // billboard: corners (-.5,.5)(-.5,-.5)(.5,-.5)(.5,.5) rotated by `angle` in the camera plane
    void AddBillboard(const Vector3 &p, float size, float angle, unsigned color)
    {
        const float c[3] = {p.X, p.Y, p.Z}, r[3] = {right.X, right.Y, right.Z}, u[3] = {up.X, up.Y, up.Z};
        Geometry::billboard(c, r, u, size, angle, color, mesh);
    }

    // ground aligned (XY planar): the legacy orientation matrix rotates the X/Y axes by -angle
    void AddGround(const Vector3 &p, float size, float angle, unsigned color)
    {
        static const float uv[4][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
        const float c = std::cos(angle), s = std::sin(angle);
        const float ox = c, oy = -s;      // rotated X axis
        const float px = s, py = c;       // rotated Y axis
        const float cx[4] = {(ox + px) * size, (ox - px) * size, -(ox + px) * size, (-ox + px) * size};
        const float cy[4] = {(oy + py) * size, (oy - py) * size, -(oy + py) * size, (-oy + py) * size};
        const unsigned short base = static_cast<unsigned short>(vertices.size());
        for (int i = 0; i < 4; ++i) {
            WorldVertexXYZDUV2 vtx;
            vtx.x = p.X + cx[i]; vtx.y = p.Y + cy[i]; vtx.z = p.Z;
            vtx.diffuse = color;
            vtx.u0 = vtx.u1 = uv[i][0]; vtx.v0 = vtx.v1 = uv[i][1];
            vertices.push_back(vtx);
        }
        indices.push_back(base); indices.push_back(static_cast<unsigned short>(base + 1)); indices.push_back(static_cast<unsigned short>(base + 2));
        indices.push_back(base); indices.push_back(static_cast<unsigned short>(base + 2)); indices.push_back(static_cast<unsigned short>(base + 3));
    }
};

void CollectParticles(D3D11Backend &backend, CameraClass &camera)
{
    if (!TheParticleSystemManager || TheParticleSystemManager->isDummy()) return;
    // Render-rate integration of the logic frame (W3DDisplay::draw does this before the scene render):
    // particles advance smoothly between logic updates instead of in 30 Hz steps.
    TheParticleSystemManager->DRAW();
    const ParticleSystemManager::PipelineStats &pipeline = TheParticleSystemManager->getPipelineStats();
    g_stats.pipelineRequested = pipeline.systemsRequested;
    g_stats.pipelineNullTemplate = pipeline.systemsNullTemplate;
    g_stats.pipelineCreated = pipeline.systemsCreated;
    g_stats.pipelineRemoved = pipeline.systemsRemoved;
    g_stats.pipelineParticlesAdded = pipeline.particlesAdded;
    g_stats.pipelineUpdates = pipeline.updateCalls;
    g_stats.pipelineDraws = pipeline.drawCalls;
    g_stats.pipelinePeakSystems = pipeline.peakSystems;
    g_stats.pipelinePeakParticles = pipeline.peakParticles;
    g_stats.systemsAlive = TheParticleSystemManager->getParticleSystemCount();
    g_stats.particlesAlive = TheParticleSystemManager->getParticleCount();
    if (g_stats.particlesAlive > g_stats.particlesAliveMax) g_stats.particlesAliveMax = g_stats.particlesAlive;
    const Matrix3D &ct = camera.Get_Transform();
    ParticleQuadBuilder builder;
    Vector3 right, up;
    ct.Get_X_Vector(&right);
    ct.Get_Y_Vector(&up);
    builder.right = right;
    builder.up = up;
    builder.cameraPosition = camera.Get_Position();
    const Vector3 cameraPosition = builder.cameraPosition;
    const float kHalfPi2 = 2.0f * 3.14159265f;

    ParticleSystemManager::ParticleSystemList &systems = TheParticleSystemManager->getAllParticleSystems();
    for (ParticleSystemManager::ParticleSystemListIt it = systems.begin(); it != systems.end(); ++it) {
        ParticleSystem *sys = *it;
        if (!sys || sys->isUsingDrawables()) continue;   // drawable particles are normal objects
        if (sys->isUsingSmudge()) { ++g_stats.smudgeSystems; continue; }

        // Cull by the view frustum with the particle size as radius.
        struct Visible { const Particle *p; Vector3 pos; };
        std::vector<Visible> visible;
        for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext) {
            const Coord3D *pos = p->getPosition();
            const float size = p->getSize();
            const SphereClass sphere(Vector3(pos->x, pos->y, pos->z), size > 0.01f ? size : 0.01f);
            if (camera.Cull_Sphere(sphere)) { ++g_stats.particlesCulled; continue; }
            visible.push_back(Visible{p, Vector3(pos->x, pos->y, pos->z)});
        }
        if (visible.empty()) continue;

        Texture2DResidency *texture = nullptr;
        const AsciiString &name = sys->getParticleTypeName();
        if (name.isNotEmpty()) texture = NativeWorldObjects::effectTexture(backend, name.str());
        const bool billboard = sys->getParticleAlignment() == ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD;
        const NativeWorldObjects::EffectState state =
            NativeWorldObjects::EffectStateFromShader(ShaderBitsFor(sys->getShaderType()), true);

        float meanDistance = 0.0f;
        for (const Visible &v : visible) {
            const Vector3 d = v.pos - cameraPosition;
            meanDistance += d.X * d.X + d.Y * d.Y + d.Z * d.Z;
        }
        meanDistance /= static_cast<float>(visible.size());

        auto flush = [&]() {
            if (builder.vertices.empty()) return;
            NativeWorldObjects::EffectBatch b = BaseBatch(state, meanDistance, NativeWorldObjects::EffectLayerParticle);
            b.vertices = builder.vertices.data();
            b.vertexCount = static_cast<unsigned>(builder.vertices.size());
            b.indices = builder.indices.data();
            b.indexCount = static_cast<unsigned>(builder.indices.size());
            b.texture = texture;
            b.doubleSided = true;
            b.noDiffuse = false;
            if (NativeWorldObjects::emitEffect(backend, b)) ++g_stats.particleBatches;
            else ++g_stats.dropped;
            builder.Reset();
        };

        if (sys->isUsingStreak() && visible.size() >= 2) {
            // Ribbon through the particles in system order; the trailing end fades out as in the legacy streak.
            std::vector<Vector3> pts;
            std::vector<float> half, v;
            std::vector<unsigned> cols;
            for (size_t i = 0; i < visible.size(); ++i) {
                const Particle *p = visible[i].p;
                const RGBColor *c = const_cast<Particle *>(p)->getColor();
                pts.push_back(visible[i].pos);
                half.push_back(const_cast<Particle *>(p)->getSize() * 0.5f);
                v.push_back(static_cast<float>(i) / static_cast<float>(visible.size() - 1));
                cols.push_back(i == 0 ? 0u : PackColor(c->red, c->green, c->blue, const_cast<Particle *>(p)->getAlpha()));
            }
            AppendRibbon(builder.vertices, builder.indices, pts, half, cols, v, cameraPosition, 0.0f, 1.0f, 0.0f, 0.0f);
            flush();
            ++g_stats.streaks;
            g_stats.particlesDrawn += static_cast<unsigned>(visible.size());
            ++g_stats.particleSystems;
            continue;
        }

        // Volume particles are drawn as several billboards shifted toward the camera.
        unsigned layers = 1;
        if (sys->isUsingVolumeParticles() && sys->getVolumeParticleDepth() > DEFAULT_VOLUME_PARTICLE_DEPTH)
            layers = std::min<unsigned>(sys->getVolumeParticleDepth(), 8u);

        for (const Visible &v : visible) {
            Particle *p = const_cast<Particle *>(v.p);
            const RGBColor *c = p->getColor();
            const unsigned color = PackColor(c->red, c->green, c->blue, p->getAlpha());
            // uint8 angle quantization of the legacy orientation table
            const float angle = static_cast<float>(static_cast<unsigned char>(p->getAngle() * 255.0f / kHalfPi2)) / 256.0f * kHalfPi2;
            for (unsigned layer = 0; layer < layers; ++layer) {
                Vector3 pos = v.pos;
                if (layers > 1) {
                    const Vector3 toCamera = Normalized(cameraPosition - pos, Vector3(0, 0, 1));
                    pos += toCamera * (static_cast<float>(layer) * p->getSize() * (0.1f / static_cast<float>(layers)));
                    if (layer) ++g_stats.volumeLayers;
                }
                if (billboard) builder.AddBillboard(pos, p->getSize(), angle, color);
                else builder.AddGround(pos, p->getSize(), angle, color);
                ++g_stats.particlesDrawn;
            }
            if (builder.vertices.size() > 60000 - 8 * 4) flush();
        }
        flush();
        ++g_stats.particleSystems;
    }
}
} // namespace

namespace
{
// ---------------------------------------------------------------------------------------------
// Terrain decals and shadows. The native profile has no W3DShadowManager (stencil volumes, render-to-
// texture projections, DX8 decal buffers); the Shadow interface is implemented natively instead:
//   SHADOW_DECAL / ALPHA / ADDITIVE : a terrain-conforming textured grid (multiply / alpha / additive)
//   SHADOW_VOLUME                   : the object's opaque triangles projected along the sun direction onto
//                                   the terrain, blended once per pixel with the map shadow color
//                                   (stencil-once), which is the planar equivalent of the stencil volume
//   SHADOW_PROJECTION and friends   : drawn as the same planar silhouette (no render-to-texture blur)
// ---------------------------------------------------------------------------------------------
class NativeShadow : public Shadow
{
public:
    NativeShadow(RenderObjClass *object, const Shadow::ShadowTypeInfo &info)
        : m_object(object), m_info(info)
    {
        m_isEnabled = TRUE;
        m_isInvisibleEnabled = FALSE;
        m_type = info.m_type;
        m_x = m_y = m_z = 0.0f;
        m_decalSizeX = m_decalSizeY = 0.0f;
        m_oowDecalSizeX = m_oowDecalSizeY = 0.0f;
        if (object) object->Add_Ref();
        setSize(info.m_sizeX, info.m_sizeY);
    }
    ~NativeShadow() { if (m_object) m_object->Release_Ref(); }

    void release() override;
#if defined(RTS_DEBUG)
    void getRenderCost(RenderCost &) const override {}
#endif

    RenderObjClass *object() const { return m_object; }
    const Shadow::ShadowTypeInfo &info() const { return m_info; }
    ShadowType shadowType() const { return m_type; }
    unsigned diffuse() const { return static_cast<unsigned>(m_diffuse); }
    float sizeX() const { return m_decalSizeX; }
    float sizeY() const { return m_decalSizeY; }
    float angle() const { return m_localAngle; }
    void position(float &x, float &y, float &z) const { x = m_x; y = m_y; z = m_z; }

private:
    RenderObjClass *m_object;
    Shadow::ShadowTypeInfo m_info;
};

class NativeShadowManager : public ProjectedShadowManager
{
public:
    Shadow *addDecal(RenderObjClass *object, Shadow::ShadowTypeInfo *info) override { return Add(object, info); }
    Shadow *addDecal(Shadow::ShadowTypeInfo *info) override { return Add(nullptr, info); }
    ~NativeShadowManager() override
    {
        // Remaining shadows are owned by their drawables, which release them; drop the registry only.
        shadows.clear();
    }

    Shadow *Add(RenderObjClass *object, Shadow::ShadowTypeInfo *info)
    {
        if (!info) return nullptr;
        NativeShadow *shadow = new NativeShadow(object, *info);
        shadows.push_back(shadow);
        return shadow;
    }
    void Remove(NativeShadow *shadow)
    {
        for (size_t i = 0; i < shadows.size(); ++i)
            if (shadows[i] == shadow) { shadows[i] = shadows.back(); shadows.pop_back(); return; }
    }
    std::vector<NativeShadow *> shadows;
};

NativeShadowManager *g_shadowManager = nullptr;
unsigned g_shadowColor = 0x7fa0a0a0;

void NativeShadow::release()
{
    if (g_shadowManager) g_shadowManager->Remove(this);
    delete this;
}

float GroundHeight(float x, float y)
{
    if (!TheTerrainLogic) return 0.0f;
    return TheTerrainLogic->getGroundHeight(x, y);
}

// The terrain height field itself (what the terrain mesh is built from): lattice index relative to the
// playable origin; the heights of bridges and objects are deliberately not part of it.
float TerrainLatticeHeight(int lx, int ly)
{
    WorldHeightMap *map = TheTerrainRenderObject ? TheTerrainRenderObject->getMap() : nullptr;
    if (!map) return 0.0f;
    const int border = map->getBorderSizeInline();
    const int x = std::max(0, std::min(map->getXExtent() - 1, lx + border));
    const int y = std::max(0, std::min(map->getYExtent() - 1, ly + border));
    return static_cast<float>(map->getDataPtr()[x + y * map->getXExtent()]) * MAP_HEIGHT_SCALE;
}

bool TerrainLatticeFlip(int lx, int ly)
{
    WorldHeightMap *map = TheTerrainRenderObject ? TheTerrainRenderObject->getMap() : nullptr;
    if (!map) return false;
    const int border = map->getBorderSizeInline();
    return map->getFlipState(lx + border, ly + border) != FALSE;
}

// Terrain-conforming textured grid (SHADOW_DECAL, ALPHA_DECAL, ADDITIVE_DECAL and radius decals).
void EmitDecal(D3D11Backend &backend, const NativeShadow &shadow, float cx, float cy, float yaw, float distance)
{
    float sx = shadow.sizeX(), sy = shadow.sizeY();
    if (!(sx > 0.0f) || !(sy > 0.0f)) {
        if (const RenderObjClass *object = shadow.object()) {
            const float r = object->Get_Bounding_Sphere().Radius;
            sx = sy = r * 2.0f;
        }
    }
    if (!(sx > 0.01f) || !(sy > 0.01f) || sx > 4000.0f || sy > 4000.0f) return;
    // 2.5 unit steps (a quarter of the terrain cell) keep the decal within a fraction of a unit of the
    // surface; the lift below covers the remaining fold deviation on steep slopes.
    int nu = static_cast<int>(std::ceil(sx / 2.5f)) + 1, nv = static_cast<int>(std::ceil(sy / 2.5f)) + 1;
    nu = std::max(2, std::min(nu, 33)); nv = std::max(2, std::min(nv, 33));
    const float stepX = sx / static_cast<float>(nu - 1), stepY = sy / static_cast<float>(nv - 1);
    const float c = std::cos(yaw), s = std::sin(yaw);
    static std::vector<WorldVertexXYZDUV2> vertices;
    static std::vector<unsigned short> indices;
    vertices.clear(); indices.clear();
    const unsigned color = shadow.diffuse();
    for (int j = 0; j < nv; ++j) {
        for (int i = 0; i < nu; ++i) {
            const float fu = static_cast<float>(i) / static_cast<float>(nu - 1);
            const float fv = static_cast<float>(j) / static_cast<float>(nv - 1);
            const float lx = (fu - 0.5f) * sx, ly = (fv - 0.5f) * sy;
            WorldVertexXYZDUV2 v;
            v.x = cx + lx * c - ly * s;
            v.y = cy + lx * s + ly * c;
            const float h = GroundHeight(v.x, v.y);
            const float gx = (GroundHeight(v.x + 1.25f, v.y) - h) / 1.25f, gy = (GroundHeight(v.x, v.y + 1.25f) - h) / 1.25f;
            const float slope = std::min(2.0f, std::sqrt(gx * gx + gy * gy));
            v.z = h + 0.3f + slope * 0.5f * std::max(stepX, stepY);
            v.diffuse = color;
            v.u0 = v.u1 = fu; v.v0 = v.v1 = 1.0f - fv;
            vertices.push_back(v);
        }
    }
    for (int j = 0; j + 1 < nv; ++j) {
        for (int i = 0; i + 1 < nu; ++i) {
            const unsigned short a = static_cast<unsigned short>(j * nu + i), b = static_cast<unsigned short>(a + 1);
            const unsigned short d = static_cast<unsigned short>(a + nu), e = static_cast<unsigned short>(d + 1);
            indices.push_back(a); indices.push_back(d); indices.push_back(b);
            indices.push_back(b); indices.push_back(d); indices.push_back(e);
        }
    }
    NativeWorldObjects::EffectBatch b = {};
    b.vertices = vertices.data(); b.vertexCount = static_cast<unsigned>(vertices.size());
    b.indices = indices.data(); b.indexCount = static_cast<unsigned>(indices.size());
    char name[80];
    std::snprintf(name, sizeof(name), "%s", shadow.info().m_ShadowName);
    const size_t length = std::strlen(name);
    if (length && (length < 4 || name[length - 4] != '.')) std::strncat(name, ".tga", sizeof(name) - length - 1);
    b.texture = length ? NativeWorldObjects::effectTexture(backend, name) : nullptr;
    const ShadowType type = shadow.shadowType();
    if (type & SHADOW_ADDITIVE_DECAL) b.blend = WorldBlend::Additive;
    else if (type & SHADOW_ALPHA_DECAL) b.blend = WorldBlend::SrcAlpha;
    else b.blend = WorldBlend::Multiply;
    b.depthWrite = false;
    b.doubleSided = true;
    b.clamp = true;
    b.distance = distance;
    b.layer = NativeWorldObjects::EffectLayerDecal;
    if (NativeWorldObjects::emitEffect(backend, b)) {
        if (shadow.object()) ++g_stats.shadowDecals; else ++g_stats.decals;
    } else ++g_stats.dropped;
}

// Mesh triangles of an object (current LOD), world space; opaque meshes (or those flagged CAST_SHADOW).
void GatherShadowGeometry(RenderObjClass *object, std::vector<Vector3> &positions, std::vector<unsigned> &triangles,
                          unsigned depth)
{
    if (!object || depth > 8 || !object->Is_Not_Hidden_At_All()) return;
    switch (object->Class_ID()) {
    case RenderObjClass::CLASSID_MESH: {
        MeshClass *mesh = static_cast<MeshClass *>(object);
        MeshModelClass *model = mesh->Peek_Model();
        if (!model) return;
        const int vertexCount = model->Get_Vertex_Count(), polyCount = model->Get_Polygon_Count();
        if (vertexCount <= 0 || polyCount <= 0 || vertexCount > 65535) return;
        if ((mesh->Is_Alpha() || mesh->Is_Translucent()) && !model->Get_Flag(MeshGeometryClass::CAST_SHADOW)) return;
        const size_t base = positions.size();
        positions.resize(base + static_cast<size_t>(vertexCount));
        const bool skin = model->Get_Flag(MeshGeometryClass::SKIN) != 0;
        if (skin && mesh->Get_Container() && mesh->Get_Container()->Get_HTree()) {
            std::vector<Vector3> normals(static_cast<size_t>(vertexCount));
            mesh->Get_Deformed_Vertices(&positions[base], normals.data());
        } else {
            const Matrix3D tm = mesh->Get_Transform();
            const Vector3 *src = model->Get_Vertex_Array();
            for (int i = 0; i < vertexCount; ++i) Matrix3D::Transform_Vector(tm, src[i], &positions[base + i]);
        }
        const TriIndex *polys = model->Get_Polygon_Array();
        for (int p = 0; p < polyCount; ++p) {
            triangles.push_back(static_cast<unsigned>(base + polys[p].I));
            triangles.push_back(static_cast<unsigned>(base + polys[p].J));
            triangles.push_back(static_cast<unsigned>(base + polys[p].K));
        }
        return;
    }
    case RenderObjClass::CLASSID_HLOD: {
        HLodClass *hlod = static_cast<HLodClass *>(object);
        const int lod = hlod->Get_LOD_Level();
        const int count = hlod->Get_Lod_Model_Count(lod);
        for (int i = 0; i < count; ++i) {
            RenderObjClass *sub = hlod->Peek_Lod_Model(lod, i);
            if (sub && sub->Class_ID() != RenderObjClass::CLASSID_OBBOX) GatherShadowGeometry(sub, positions, triangles, depth + 1);
        }
        return;
    }
    default:
        return;
    }
}

void EmitVolumeShadow(D3D11Backend &backend, const NativeShadow &shadow, const Vector3 &lightDirection, float distance,
                      unsigned &triangleBudget)
{
    RenderObjClass *object = shadow.object();
    if (!object) return;
    static std::vector<Vector3> positions;
    static std::vector<unsigned> triangles;
    positions.clear(); triangles.clear();
    GatherShadowGeometry(object, positions, triangles, 0);
    if (positions.empty() || triangles.empty()) { ++g_stats.shadowNoGeometry; return; }
    if (triangles.size() / 3 > triangleBudget || positions.size() > 60000) return;
    triangleBudget -= static_cast<unsigned>(triangles.size() / 3);

    static std::vector<WorldVertexXYZDUV2> vertices;
    static std::vector<unsigned short> indices;
    vertices.resize(positions.size());
    // renderStencilShadows: SRCBLEND = DESTCOLOR, DESTBLEND = ZERO with the shadow color: the framebuffer is
    // multiplied by the shadow color (its alpha is not used) wherever the stencil is set.
    const unsigned color = 0xff000000u | (g_shadowColor & 0x00ffffffu);
    const Vector3 center = object->Get_Position();
    // Small objects (units) are projected onto the terrain plane fitted at their position (three height
    // samples); large ones (structures) follow the height field per vertex. Both avoid a height lookup
    // per mesh vertex for the common case, which would dominate the frame with many units on screen.
    const bool planar = object->Get_Bounding_Sphere().Radius <= 30.0f;
    const float g0 = GroundHeight(center.X, center.Y);
    const float gx = planar ? (GroundHeight(center.X + 5.0f, center.Y) - g0) / 5.0f : 0.0f;
    const float gy = planar ? (GroundHeight(center.X, center.Y + 5.0f) - g0) / 5.0f : 0.0f;
    for (size_t i = 0; i < positions.size(); ++i) {
        const Vector3 &p = positions[i];
        float x = p.X, y = p.Y, ground = g0;
        if (planar) {
            // z(x, y) = g0 + gx (x - cx) + gy (y - cy); solve p + d t on that plane.
            const float denom = lightDirection.Z - gx * lightDirection.X - gy * lightDirection.Y;
            float t = std::fabs(denom) > 1e-4f ? (g0 + gx * (p.X - center.X) + gy * (p.Y - center.Y) - p.Z) / denom : 0.0f;
            if (t < 0.0f) t = 0.0f;
            x = p.X + lightDirection.X * t;
            y = p.Y + lightDirection.Y * t;
            ground = g0 + gx * (x - center.X) + gy * (y - center.Y);
        } else {
            for (int pass = 0; pass < 2; ++pass) {      // refine against the terrain under the projected point
                const float t = lightDirection.Z < -1e-3f ? (ground - p.Z) / lightDirection.Z : 0.0f;
                const float tt = t > 0.0f ? t : 0.0f;
                x = p.X + lightDirection.X * tt;
                y = p.Y + lightDirection.Y * tt;
                ground = GroundHeight(x, y);
            }
        }
        vertices[i].x = x; vertices[i].y = y; vertices[i].z = ground + 0.3f;
        vertices[i].diffuse = color;
        vertices[i].u0 = vertices[i].v0 = vertices[i].u1 = vertices[i].v1 = 0.0f;
    }
    indices.resize(triangles.size());
    for (size_t i = 0; i < triangles.size(); ++i) indices[i] = static_cast<unsigned short>(triangles[i]);

    NativeWorldObjects::EffectBatch b = {};
    b.vertices = vertices.data(); b.vertexCount = static_cast<unsigned>(vertices.size());
    b.indices = indices.data(); b.indexCount = static_cast<unsigned>(indices.size());
    b.blend = WorldBlend::Multiply;
    b.depthWrite = false;
    b.doubleSided = true;
    b.stencilOnce = true;
    b.distance = distance;
    b.layer = NativeWorldObjects::EffectLayerDecal;
    if (NativeWorldObjects::emitEffect(backend, b)) ++g_stats.volumeShadows;
    else ++g_stats.dropped;
}

void CollectShadows(D3D11Backend &backend, CameraClass &camera)
{
    if (!g_shadowManager || g_shadowManager->shadows.empty()) return;
    // Sun direction (the direction light travels): the first object light of the current time of day.
    Vector3 light(0.3f, 0.3f, -1.0f);
    if (TheGlobalData) {
        const GlobalData::TerrainLighting &l = TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][0];
        const Vector3 d(l.lightPos.x, l.lightPos.y, l.lightPos.z);
        if (d.Length() > 1e-4f) light = d;
    }
    light = Normalized(light, Vector3(0.3f, 0.3f, -1.0f));
    unsigned triangleBudget = 200000;
    const Vector3 cameraPosition = camera.Get_Position();
    const bool volumes = !TheGlobalData || TheGlobalData->m_useShadowVolumes;
    const bool decals = !TheGlobalData || TheGlobalData->m_useShadowDecals;
    for (NativeShadow *shadow : g_shadowManager->shadows) {
        ++g_stats.shadowsSeen;
        if (!shadow->isRenderEnabled() || shadow->isInvisibleEnabled()) { ++g_stats.shadowCulled; continue; }
        const ShadowType type = shadow->shadowType();
        float x, y, z;
        shadow->position(x, y, z);
        float yaw = shadow->angle();
        RenderObjClass *object = shadow->object();
        if (object) {
            if (!object->Is_Not_Hidden_At_All()) continue;
            if (camera.Cull_Sphere(object->Get_Bounding_Sphere())) continue;
            const Vector3 p = object->Get_Position();
            x = p.X; y = p.Y; z = p.Z;
            yaw = object->Get_Transform().Get_Z_Rotation();
            // The decal offset is in object space.
            const float c = std::cos(yaw), s = std::sin(yaw);
            const Shadow::ShadowTypeInfo &info = shadow->info();
            x += info.m_offsetX * c - info.m_offsetY * s;
            y += info.m_offsetX * s + info.m_offsetY * c;
        }
        const Vector3 d = Vector3(x, y, z) - cameraPosition;
        const float distance = d.X * d.X + d.Y * d.Y + d.Z * d.Z;
        if (type & (SHADOW_DECAL | SHADOW_ALPHA_DECAL | SHADOW_ADDITIVE_DECAL)) {
            if ((type & SHADOW_DECAL) && !(type & (SHADOW_ALPHA_DECAL | SHADOW_ADDITIVE_DECAL)) && !decals) continue;
            EmitDecal(backend, *shadow, x, y, yaw, distance);
        } else if (type & (SHADOW_VOLUME | SHADOW_PROJECTION)) {
            if (!volumes) continue;
            EmitVolumeShadow(backend, *shadow, light, distance, triangleBudget);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Water (WaterRenderObjClass::renderWater, WaterType 0): flat polygons as a lit, scrolling,
// translucent surface; rivers as a strip with the edge alpha profile; soft shoreline through the
// destination alpha the terrain pass left.
// ---------------------------------------------------------------------------------------------
struct EdgeProfile
{
    bool loaded = false;
    std::vector<float> alpha;
};

void WaterShade(float &r, float &g, float &b, unsigned &alphaByte)
{
    const WaterSetting &ws = WaterSettings[TheGlobalData->m_timeOfDay];
    const Real sr = TheWaterTransparency->m_standingWaterColor.red;
    const Real sg = TheWaterTransparency->m_standingWaterColor.green;
    const Real sb = TheWaterTransparency->m_standingWaterColor.blue;
    float shadeR, shadeG, shadeB;
    if (sr == 1.0f && sg == 1.0f && sb == 1.0f) {
        shadeR = TheGlobalData->m_terrainAmbient[0].red;
        shadeG = TheGlobalData->m_terrainAmbient[0].green;
        shadeB = TheGlobalData->m_terrainAmbient[0].blue;
        for (Int i = 0; i < TheGlobalData->m_numGlobalLights; ++i) {
            if (-TheGlobalData->m_terrainLightPos[i].z > 0) {
                shadeR += -TheGlobalData->m_terrainLightPos[i].z * TheGlobalData->m_terrainDiffuse[i].red;
                shadeG += -TheGlobalData->m_terrainLightPos[i].z * TheGlobalData->m_terrainDiffuse[i].green;
                shadeB += -TheGlobalData->m_terrainLightPos[i].z * TheGlobalData->m_terrainDiffuse[i].blue;
            }
        }
        // the legacy code scaled the 8-bit components of the water material color
        shadeR *= static_cast<float>(ws.m_waterDiffuseColor.red) / 255.0f;
        shadeG *= static_cast<float>(ws.m_waterDiffuseColor.green) / 255.0f;
        shadeB *= static_cast<float>(ws.m_waterDiffuseColor.blue) / 255.0f;
    } else {
        shadeR = sr; shadeG = sg; shadeB = sb;
        if (sr == 0 && sg == 0 && sb == 0) shadeR = shadeG = shadeB = 1.0f;
    }
    r = std::min(1.0f, std::max(0.0f, shadeR));
    g = std::min(1.0f, std::max(0.0f, shadeG));
    b = std::min(1.0f, std::max(0.0f, shadeB));
    alphaByte = static_cast<unsigned>(ws.m_waterDiffuseColor.alpha) & 0xff;
}

void CollectWater(D3D11Backend &backend, CameraClass &camera)
{
    if (!TheGlobalData || !TheWaterTransparency || !TheTerrainRenderObject || !TheTerrainRenderObject->getMap()) return;
    if (!PolygonTrigger::getFirstPolygonTrigger()) return;

    float shadeR, shadeG, shadeB;
    unsigned alphaByte;
    WaterShade(shadeR, shadeG, shadeB, alphaByte);
    const unsigned rgb = (static_cast<unsigned>(shadeR * 255.0f + 0.5f) << 16) |
                         (static_cast<unsigned>(shadeG * 255.0f + 0.5f) << 8) |
                         static_cast<unsigned>(shadeB * 255.0f + 0.5f);
    const unsigned diffuse = (alphaByte << 24) | rgb;
    Texture2DResidency *texture = nullptr;
    const AsciiString &textureName = TheWaterTransparency->m_standingWaterTexture;
    if (textureName.isNotEmpty()) texture = NativeWorldObjects::effectTexture(backend, textureName.str());
    const bool additive = TheWaterTransparency->m_additiveBlend;
    const bool softEdge = TheGlobalData->m_showSoftWaterEdge && TheWaterTransparency->m_transparentWaterDepth != 0;
    const WorldBlend flatBlend = additive ? (softEdge ? WorldBlend::DestAlphaAdditive : WorldBlend::Additive)
                                          : (softEdge ? WorldBlend::DestAlpha : WorldBlend::SrcAlpha);
    const WorldBlend riverBlend = additive ? WorldBlend::AdditiveAlpha : WorldBlend::SrcAlpha;
    const float phase = TheGameLogic ? 0.002f * static_cast<float>(TheGameLogic->getFrame()) : 0.0f;
    const Vector3 cameraPosition = camera.Get_Position();

    // River edge alpha profile (TWAlphaEdge): sampled across the width on the CPU.
    static EdgeProfile edge;
    if (!edge.loaded) {
        edge.loaded = true;
        unsigned w = 0, h = 0;
        std::vector<unsigned char> rgba;
        if (NativeWorldObjects::effectImage("TWAlphaEdge.tga", rgba, w, h) && w && h) {
            unsigned minA = 255, maxA = 0;
            for (unsigned x = 0; x < w; ++x) { minA = std::min<unsigned>(minA, rgba[x * 4 + 3]); maxA = std::max<unsigned>(maxA, rgba[x * 4 + 3]); }
            edge.alpha.resize(w);
            for (unsigned x = 0; x < w; ++x)   // alpha if it varies, otherwise the luminance
                edge.alpha[x] = (maxA > minA ? rgba[x * 4 + 3] : (rgba[x * 4] + rgba[x * 4 + 1] + rgba[x * 4 + 2]) / 3) / 255.0f;
        }
    }

    // Retail pixel shaders (sparkles + noise, river alpha edge) need their three textures; when any of them is
    // missing the plain flat water of the accepted baseline is drawn instead (never a white sheet).
    static int shaderAssets = -1;
    if (shaderAssets < 0) {
        std::vector<unsigned char> probe;
        unsigned w = 0, h = 0;
        shaderAssets = NativeWorldObjects::effectImage("WaterSurfaceBubbles.tga", probe, w, h) &&
                       NativeWorldObjects::effectImage("Noise0000.tga", probe, w, h) &&
                       NativeWorldObjects::effectImage("TWAlphaEdge.tga", probe, w, h) ? 1 : 0;
    }
    Texture2DResidency *sparkleTexture = nullptr, *noiseTexture = nullptr, *edgeTexture = nullptr;
    if (shaderAssets) {
        sparkleTexture = NativeWorldObjects::effectTexture(backend, "WaterSurfaceBubbles.tga");
        noiseTexture = NativeWorldObjects::effectTexture(backend, "Noise0000.tga");
        edgeTexture = NativeWorldObjects::effectTexture(backend, "TWAlphaEdge.tga");
    }
    const bool waterShaders = sparkleTexture && noiseTexture && edgeTexture && texture;
    static const std::vector<float> noProfile;

    Geometry::Mesh mesh;
    for (PolygonTrigger *trigger = PolygonTrigger::getFirstPolygonTrigger(); trigger; trigger = trigger->getNext()) {
        if (!trigger->isWaterArea() || trigger->getNumPoints() <= 2) continue;
        const Int count = trigger->getNumPoints();
        std::vector<float> points(static_cast<size_t>(count) * 3);
        Vector3 centroid(0, 0, 0);
        for (Int i = 0; i < count; ++i) {
            const ICoord3D *p = trigger->getPoint(i);
            points[i * 3] = static_cast<float>(p->x); points[i * 3 + 1] = static_cast<float>(p->y);
            points[i * 3 + 2] = static_cast<float>(p->z);
            centroid += Vector3(points[i * 3], points[i * 3 + 1], points[i * 3 + 2]);
        }
        centroid *= 1.0f / static_cast<float>(count);
        float radius = 0.0f;
        for (Int i = 0; i < count; ++i) {
            const Vector3 d(points[i * 3] - centroid.X, points[i * 3 + 1] - centroid.Y, 0.0f);
            radius = std::max(radius, d.Length());
        }
        if (camera.Cull_Sphere(SphereClass(centroid, radius + 10.0f))) continue;

        mesh.vertices.clear(); mesh.indices.clear();
        WorldBlend blend = flatBlend;
        if (trigger->isRiver()) {
            blend = riverBlend;
            if (!Geometry::river(points.data(), count, trigger->getRiverStart(), phase, diffuse,
                                 waterShaders ? noProfile : edge.alpha, mesh)) continue;
        } else {
            for (Int k = 1; k < count - 1; k += 2) {
                const float *p3 = &points[0];
                const float *p2 = &points[k * 3];
                const float *p1 = &points[(k + 1) * 3];
                const float *p0 = k + 2 < count ? &points[(k + 2) * 3] : p1;
                const float corners[4][3] = {{p0[0], p0[1], p0[2]}, {p1[0], p1[1], p1[2]}, {p2[0], p2[1], p2[2]},
                                             {p3[0], p3[1], p3[2]}};
                Geometry::standingWater(corners, phase, diffuse, mesh);
                if (mesh.vertices.size() > 56000) break;
            }
        }
        if (mesh.vertices.empty() || mesh.indices.empty() || mesh.vertices.size() > 60000) continue;
        NativeWorldObjects::EffectBatch b = {};
        b.vertices = mesh.vertices.data(); b.vertexCount = static_cast<unsigned>(mesh.vertices.size());
        b.indices = mesh.indices.data(); b.indexCount = static_cast<unsigned>(mesh.indices.size());
        b.texture = texture;
        b.blend = blend;
        b.depthWrite = false; b.doubleSided = true; b.noDiffuse = false;
        if (waterShaders) {
            b.waterKind = trigger->isRiver() ? 2 : 1;
            b.waterSparkle = sparkleTexture; b.waterNoise = noiseTexture; b.waterEdge = edgeTexture;
            // The noise stage's texgen matrix: camera-space position through the inverse view (= world xy),
            // scaled by NOISE_REPEAT_FACTOR (1/16), translated by the water scroll origin.
            b.waterNoiseOffset[0] = b.waterNoiseOffset[1] = phase;
            b.waterNoiseScale = 1.0f / 16.0f;
            if (b.waterKind == 1) ++g_stats.waterSparkles; else ++g_stats.waterRivers;
        }
        const Vector3 d = centroid - cameraPosition;
        b.distance = d.X * d.X + d.Y * d.Y + d.Z * d.Z;
        b.layer = NativeWorldObjects::EffectLayerTranslucent;
        if (NativeWorldObjects::emitEffect(backend, b)) ++g_stats.waterPatches; else ++g_stats.dropped;
    }
}

// ---------------------------------------------------------------------------------------------
// Shroud / fog of war. W3DDisplay::setShroudLevel feeds the CPU level array (the legacy W3DShroud
// texture contents); the overlay multiplies the terrain with it, bilinearly between cell centers.
// ---------------------------------------------------------------------------------------------
struct ShroudState
{
    int cellsX = 0, cellsY = 0;
    float cell = 0.0f;
    const void *map = nullptr;
    std::vector<unsigned char> levels;
    unsigned char border = 0;
    bool filter = true;
    unsigned updates = 0;
    bool forced = false;     // test hook: levels supplied without a map
};
ShroudState g_shroud;

// True when the level array exists for the current map. The array is created at render time (never from
// inside the logic-side shroud update) and followed by a refresh, like W3DShroud::init.
bool ShroudReady()
{
    if (g_shroud.forced) return !g_shroud.levels.empty();
    if (!TheTerrainRenderObject || !TheGlobalData) return false;
    WorldHeightMap *map = TheTerrainRenderObject->getMap();
    const float cell = TheGlobalData->m_partitionCellSize > 1.0f ? TheGlobalData->m_partitionCellSize : 40.0f;
    return map && g_shroud.map == map && g_shroud.cell == cell && !g_shroud.levels.empty();
}

bool EnsureShroud()
{
    if (!TheTerrainRenderObject || !TheGlobalData) return false;
    WorldHeightMap *map = TheTerrainRenderObject->getMap();
    if (!map) return false;
    const float cell = TheGlobalData->m_partitionCellSize > 1.0f ? TheGlobalData->m_partitionCellSize : 40.0f;
    if (g_shroud.map == map && g_shroud.cell == cell && !g_shroud.levels.empty()) return true;
    g_shroud.map = map;
    g_shroud.cell = cell;
    g_shroud.cellsX = static_cast<int>(std::ceil(static_cast<float>(map->getXExtent() - 1 - map->getBorderSizeInline() * 2) * 10.0f / cell));
    g_shroud.cellsY = static_cast<int>(std::ceil(static_cast<float>(map->getYExtent() - 1 - map->getBorderSizeInline() * 2) * 10.0f / cell));
    if (g_shroud.cellsX <= 0 || g_shroud.cellsY <= 0 || g_shroud.cellsX > 2048 || g_shroud.cellsY > 2048) {
        g_shroud.levels.clear();
        return false;
    }
    // Unknown cells start shrouded (the legacy source surface was zeroed).
    g_shroud.levels.assign(static_cast<size_t>(g_shroud.cellsX) * g_shroud.cellsY, 0);
    g_shroud.border = 0;
    if (ThePartitionManager) ThePartitionManager->refreshShroudForLocalPlayer();   // fill from the logic state
    return true;
}

// ---------------------------------------------------------------------------------------------
// Scorch marks (W3DScorch): the terrain cells under the radius, textured from the 3x3 atlas.
// ---------------------------------------------------------------------------------------------
struct ScorchMark
{
    float x, y, z, radius;
    int type;
};
std::deque<ScorchMark> g_scorches, g_staticScorches;

void CollectScorches(D3D11Backend &backend, CameraClass &camera)
{
    if ((g_scorches.empty() && g_staticScorches.empty()) || !TheGlobalData || !TheTerrainRenderObject ||
        !TheTerrainRenderObject->getMap())
        return;
    WorldHeightMap *map = TheTerrainRenderObject->getMap();
    Texture2DResidency *texture = NativeWorldObjects::effectTexture(backend, "EXScorch01.tga");
    // Shade: the mean of the first terrain light's ambient and diffuse (W3DScorch::updateScorches).
    auto channel = [](float ambient, float diffuse) {
        return std::min(1.0f, std::max(0.0f, (ambient + diffuse) * 0.5f));
    };
    const unsigned r = static_cast<unsigned>(channel(TheGlobalData->m_terrainAmbient[0].red, TheGlobalData->m_terrainDiffuse[0].red) * 255.0f + 0.5f);
    const unsigned g = static_cast<unsigned>(channel(TheGlobalData->m_terrainAmbient[0].green, TheGlobalData->m_terrainDiffuse[0].green) * 255.0f + 0.5f);
    const unsigned b = static_cast<unsigned>(channel(TheGlobalData->m_terrainAmbient[0].blue, TheGlobalData->m_terrainDiffuse[0].blue) * 255.0f + 0.5f);
    const unsigned diffuse = 0xff000000u | (r << 16) | (g << 8) | b;
    const Int border = map->getBorderSizeInline();
    const Vector3 cameraPosition = camera.Get_Position();

    static Geometry::Mesh mesh;
    auto emit = [&](const std::deque<ScorchMark> &list) {
        for (auto it = list.rbegin(); it != list.rend(); ++it) {
            const ScorchMark &sc = *it;
            if (camera.Cull_Sphere(SphereClass(Vector3(sc.x, sc.y, sc.z), sc.radius + 10.0f))) continue;
            mesh.vertices.clear(); mesh.indices.clear();
            int minX = static_cast<int>(std::floor((sc.x - sc.radius) / 10.0f));
            int minY = static_cast<int>(std::floor((sc.y - sc.radius) / 10.0f));
            int maxX = static_cast<int>(std::ceil((sc.x + sc.radius) / 10.0f)) + 1;
            int maxY = static_cast<int>(std::ceil((sc.y + sc.radius) / 10.0f)) + 1;
            minX = std::max(minX, -border); minY = std::max(minY, -border);
            maxX = std::min(maxX, map->getXExtent() - border); maxY = std::min(maxY, map->getYExtent() - border);
            const int nx = maxX - minX, ny = maxY - minY;
            if (nx <= 1 || ny <= 1 || nx > 80 || ny > 80) continue;
            const float uOffset = static_cast<float>(sc.type % 3) * 1.5f;
            const float vOffset = static_cast<float>(sc.type / 3) * 1.5f;
            for (int j = minY; j < maxY; ++j) {
                for (int i = minX; i < maxX; ++i) {
                    const float X = static_cast<float>(i) * 10.0f, Y = static_cast<float>(j) * 10.0f;
                    WorldVertexXYZDUV2 v;
                    v.x = X; v.y = Y;
                    v.z = TerrainLatticeHeight(i, j) + 0.2f;   // MAP_HEIGHT_SCALE / 10 above the terrain
                    v.diffuse = diffuse;
                    v.u0 = v.u1 = (uOffset + 0.5f + (X - sc.x) / (2.0f * sc.radius)) / 4.0f;
                    v.v0 = v.v1 = (vOffset + 0.5f + (Y - sc.y) / (2.0f * sc.radius)) / 4.0f;
                    mesh.vertices.push_back(v);
                }
            }
            for (int j = 0; j + 1 < ny; ++j) {
                for (int i = 0; i + 1 < nx; ++i) {
                    const unsigned short a = static_cast<unsigned short>(j * nx + i), c = static_cast<unsigned short>(a + nx);
                    if (TerrainLatticeFlip(minX + i, minY + j)) {   // the terrain's own diagonal (W3DScorch flip state)
                        mesh.indices.push_back(static_cast<unsigned short>(a + 1)); mesh.indices.push_back(c); mesh.indices.push_back(a);
                        mesh.indices.push_back(static_cast<unsigned short>(a + 1)); mesh.indices.push_back(static_cast<unsigned short>(c + 1)); mesh.indices.push_back(c);
                    } else {
                        mesh.indices.push_back(a); mesh.indices.push_back(static_cast<unsigned short>(c + 1)); mesh.indices.push_back(c);
                        mesh.indices.push_back(a); mesh.indices.push_back(static_cast<unsigned short>(a + 1)); mesh.indices.push_back(static_cast<unsigned short>(c + 1));
                    }
                }
            }
            NativeWorldObjects::EffectBatch e = {};
            e.vertices = mesh.vertices.data(); e.vertexCount = static_cast<unsigned>(mesh.vertices.size());
            e.indices = mesh.indices.data(); e.indexCount = static_cast<unsigned>(mesh.indices.size());
            e.texture = texture;
            e.blend = WorldBlend::SrcAlpha;
            e.depthWrite = false; e.doubleSided = true; e.clamp = true; e.noDiffuse = false;
            const Vector3 d = Vector3(sc.x, sc.y, sc.z) - cameraPosition;
            e.distance = d.X * d.X + d.Y * d.Y + d.Z * d.Z;
            e.layer = NativeWorldObjects::EffectLayerDecal;
            if (NativeWorldObjects::emitEffect(backend, e)) ++g_stats.scorches; else ++g_stats.dropped;
        }
    };
    emit(g_staticScorches);
    emit(g_scorches);
}

// ---------------------------------------------------------------------------------------------
// Terrain tracks (W3DTerrainTracks): the legacy system keeps the edge bookkeeping; this presents the
// strips flush() would have drawn (alpha shader: SRC_ALPHA / INV_SRC_ALPHA, depth test, no depth write).
// Their layer is the decal layer, below the shroud overlay, so fogged tracks darken like the terrain.
// ---------------------------------------------------------------------------------------------
void CollectTracks(D3D11Backend &backend, CameraClass &camera)
{
    if (!TheTerrainTracksRenderObjClassSystem || !TheTerrainTracksRenderObjClassSystem->isNative()) return;
    struct Context { D3D11Backend *backend; Vector3 camera; } context = { &backend, camera.Get_Position() };
    TheTerrainTracksRenderObjClassSystem->forEachNativeTrack(
        [](void *user, const char *texture, const TerrainTracksRenderObjClassSystem::NativeTrackVertex *vertices,
           unsigned vertexCount, const unsigned short *indices, unsigned indexCount) {
            Context &c = *static_cast<Context *>(user);
            static std::vector<WorldVertexXYZDUV2> converted;
            converted.resize(vertexCount);
            float cx = 0.0f, cy = 0.0f, cz = 0.0f;
            for (unsigned i = 0; i < vertexCount; ++i) {
                WorldVertexXYZDUV2 &v = converted[i];
                v.x = vertices[i].x; v.y = vertices[i].y; v.z = vertices[i].z;
                v.diffuse = vertices[i].diffuse;
                v.u0 = v.u1 = vertices[i].u;
                v.v0 = v.v1 = vertices[i].v;
                cx += v.x; cy += v.y; cz += v.z;
            }
            if (!vertexCount) return;
            NativeWorldObjects::EffectBatch e = {};
            e.vertices = converted.data(); e.vertexCount = vertexCount;
            e.indices = indices; e.indexCount = indexCount;
            e.texture = NativeWorldObjects::effectTexture(*c.backend, texture);
            e.blend = WorldBlend::SrcAlpha;
            e.depthWrite = false; e.doubleSided = true; e.clamp = false; e.noDiffuse = false;
            const Vector3 d = Vector3(cx / vertexCount, cy / vertexCount, cz / vertexCount) - c.camera;
            e.distance = d.X * d.X + d.Y * d.Y + d.Z * d.Z;
            e.layer = NativeWorldObjects::EffectLayerDecal;
            if (NativeWorldObjects::emitEffect(*c.backend, e)) ++g_stats.tracks; else ++g_stats.dropped;
        }, &context);
}

// ---------------------------------------------------------------------------------------------
// Terrain-pass overlays: building bibs (W3DBibBuffer: detailAlphaShader = SRC_ALPHA, PASS_ALWAYS, no depth
// write, no culling, clamped, vertex diffuse = terrain light) and the waypoint / rally lines queued by the
// waypoint buffer (additive, PASS_ALWAYS). Both belong to the terrain pass of the legacy renderer, so they
// are submitted on the ground layer, before the objects.
// ---------------------------------------------------------------------------------------------
void CollectBibs(D3D11Backend &backend, CameraClass &camera)
{
#if RTS_GENERALS
    if (!TheTerrainRenderObject) return;
    struct Context { D3D11Backend *backend; Vector3 camera; } context = { &backend, camera.Get_Position() };
    TheTerrainRenderObject->forEachNativeBib(
        [](void *user, const Vector3 corners[4], bool highlight) {
            Context &c = *static_cast<Context *>(user);
            WorldVertexXYZDUV2 v[4];
            static const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
            // W3DBibBuffer::loadBibsInVertexAndIndexBuffers: ambient + diffuse of the first terrain light, clamped.
            auto channel = [](float ambient, float diffuse) {
                return static_cast<unsigned>(std::min(1.0f, ambient + diffuse) * 255.0f);
            };
            const unsigned diffuse = 0xff000000u |
                (channel(TheGlobalData->m_terrainAmbient[0].red, TheGlobalData->m_terrainDiffuse[0].red) << 16) |
                (channel(TheGlobalData->m_terrainAmbient[0].green, TheGlobalData->m_terrainDiffuse[0].green) << 8) |
                channel(TheGlobalData->m_terrainAmbient[0].blue, TheGlobalData->m_terrainDiffuse[0].blue);
            float cx = 0, cy = 0, cz = 0;
            for (int i = 0; i < 4; ++i) {
                v[i].x = corners[i].X; v[i].y = corners[i].Y; v[i].z = corners[i].Z;
                v[i].diffuse = diffuse;
                v[i].u0 = v[i].u1 = uv[i][0]; v[i].v0 = v[i].v1 = uv[i][1];
                cx += corners[i].X; cy += corners[i].Y; cz += corners[i].Z;
            }
            static const unsigned short indices[6] = {0, 1, 2, 0, 2, 3};
            NativeWorldObjects::EffectBatch e = {};
            e.vertices = v; e.vertexCount = 4;
            e.indices = indices; e.indexCount = 6;
            e.texture = NativeWorldObjects::effectTexture(*c.backend, highlight ? "TBRedBib.tga" : "TBBib.tga");
            e.blend = WorldBlend::SrcAlpha;
            e.depthWrite = false; e.depthAlways = true; e.doubleSided = true; e.clamp = true; e.noDiffuse = false;
            const Vector3 d = Vector3(cx * 0.25f, cy * 0.25f, cz * 0.25f) - c.camera;
            e.distance = d.X * d.X + d.Y * d.Y + d.Z * d.Z;
            e.layer = NativeWorldObjects::EffectLayerGround;
            if (NativeWorldObjects::emitEffect(*c.backend, e)) ++g_stats.bibs; else ++g_stats.dropped;
        }, &context);
#else
    (void)backend; (void)camera;   // Zero Hour: the bib buffer has no native form yet
#endif
}

void CollectGroundLines(D3D11Backend &backend, CameraClass &camera)
{
    const Vector3 cameraPosition = camera.Get_Position();
    for (const SegLineData &data : g_groundLines) {
        const Vector3 d = data.points.front() - cameraPosition;
        if (EmitSegLineData(backend, data, cameraPosition, d.X * d.X + d.Y * d.Y + d.Z * d.Z,
                            NativeWorldObjects::EffectLayerGround, true)) ++g_stats.groundLines;
    }
    g_groundLines.clear();
}

void CollectShroud(D3D11Backend &backend, CameraClass &camera)
{
    if (!EnsureShroud()) return;
    AABoxClass visible;
    if (!TheTerrainRenderObject->getMaximumVisibleBox(camera.Get_Frustum(), &visible, TRUE)) return;
    const float cell = g_shroud.cell;
    WorldHeightMap *map = TheTerrainRenderObject->getMap();
    const int border = map->getBorderSizeInline();
    // Terrain lattice rectangle under the visible box (one lattice cell of margin), inside the drawn map.
    int lx0 = static_cast<int>(std::floor((visible.Center.X - visible.Extent.X) / 10.0f)) - 1;
    int lx1 = static_cast<int>(std::ceil((visible.Center.X + visible.Extent.X) / 10.0f)) + 1;
    int ly0 = static_cast<int>(std::floor((visible.Center.Y - visible.Extent.Y) / 10.0f)) - 1;
    int ly1 = static_cast<int>(std::ceil((visible.Center.Y + visible.Extent.Y) / 10.0f)) + 1;
    lx0 = std::max(lx0, -border); ly0 = std::max(ly0, -border);
    lx1 = std::min(lx1, map->getXExtent() - 1 - border); ly1 = std::min(ly1, map->getYExtent() - 1 - border);
    if (lx1 <= lx0 || ly1 <= ly0) return;
    // A zoomed-out view can exceed one batch: split into horizontal bands.
    const int bandRows = std::max(1, 59000 / (lx1 - lx0 + 1) - 1);
    const float color[3] = {TheGlobalData->m_shroudColor.red, TheGlobalData->m_shroudColor.green,
                            TheGlobalData->m_shroudColor.blue};
    static Geometry::Mesh mesh;
    for (int row = ly0; row < ly1; row += bandRows) {
        const int rowEnd = std::min(ly1, row + bandRows);
        mesh.vertices.clear(); mesh.indices.clear();
        if (!Geometry::shroud(g_shroud.levels.data(), g_shroud.cellsX, g_shroud.cellsY, cell, g_shroud.border,
                              g_shroud.filter, lx0, row, lx1, rowEnd, &TerrainLatticeHeight, &TerrainLatticeFlip,
                              color, mesh)) continue;
        NativeWorldObjects::EffectBatch b = {};
        b.vertices = mesh.vertices.data(); b.vertexCount = static_cast<unsigned>(mesh.vertices.size());
        b.indices = mesh.indices.data(); b.indexCount = static_cast<unsigned>(mesh.indices.size());
        b.blend = WorldBlend::Multiply;
        b.depthWrite = false; b.doubleSided = true; b.noDiffuse = false;
        b.layer = NativeWorldObjects::EffectLayerOverlay;
        if (NativeWorldObjects::emitEffect(backend, b))
            g_stats.shroudCells += static_cast<unsigned>((lx1 - lx0) * (rowEnd - row));
        else ++g_stats.dropped;
    }
}
} // namespace

void install()
{
    if (!g_shadowManager && !TheProjectedShadowManager && StartupDiagnostics::nativeFeature("shadows")) {
        g_shadowManager = new NativeShadowManager;
        TheProjectedShadowManager = g_shadowManager;
    }
}

// ---------------------------------------------------------------------------------------------
// Sky box
// ---------------------------------------------------------------------------------------------
namespace
{
RenderObjClass *g_skyBox = nullptr;
std::vector<std::pair<std::string, std::string>> g_skyReplacements;

void ClampSkyTextures(RenderObjClass *object)
{
    // WaterRenderObjClass::init / replaceSkyboxTexture: clamp every sky texture (fewer corner seams).
    if (!object || object->Class_ID() != RenderObjClass::CLASSID_MESH) return;
    MaterialInfoClass *material = static_cast<MeshClass *>(object)->Get_Material_Info();
    if (!material) return;
    for (int i = 0; i < material->Texture_Count(); ++i) {
        if (TextureClass *texture = material->Peek_Texture(i)) {
            texture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
            texture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
        }
    }
    material->Release_Ref();
}

void ReleaseSkyBox()
{
    if (g_skyBox) { g_skyBox->Release_Ref(); g_skyBox = nullptr; }
}
}

void replaceSkyboxTexture(const char *oldTexture, const char *newTexture)
{
    if (!oldTexture || !newTexture || !*oldTexture || !*newTexture) return;
    g_skyReplacements.emplace_back(oldTexture, newTexture);
    if (g_skyBox && W3DDisplay::m_assetManager) {
        W3DDisplay::m_assetManager->replacePrototypeTexture(g_skyBox, oldTexture, newTexture);
        ClampSkyTextures(g_skyBox);
    }
}

void addFrameObjects(CameraClass &camera)
{
    if (!TheGlobalData || !TheGlobalData->m_drawSkyBox || !StartupDiagnostics::nativeFeature("skybox")) return;
    if (!g_skyBox) {
        if (!W3DDisplay::m_assetManager) return;
        static bool attempted = false;
        if (attempted) return;
        attempted = true;   // a missing model is tried once
        g_skyBox = W3DDisplay::m_assetManager->Create_Render_Obj("new_skybox", TheGlobalData->m_skyBoxScale, 0);
        if (!g_skyBox) return;
        g_skyBox->Set_Collision_Type(0);
        for (const auto &pair : g_skyReplacements)
            W3DDisplay::m_assetManager->replacePrototypeTexture(g_skyBox, pair.first.c_str(), pair.second.c_str());
        ClampSkyTextures(g_skyBox);
    }
    Vector3 position = camera.Get_Position();
    position.Z = TheGlobalData->m_skyBoxPositionZ;
    g_skyBox->Set_Position(position);
    NativeWorldObjects::addFrameObject(g_skyBox);
    ++g_stats.skyBoxFrames;
}

void uninstall()
{
    ReleaseSkyBox();
    g_skyReplacements.clear();
    if (g_shadowManager) {
        if (TheProjectedShadowManager == g_shadowManager) TheProjectedShadowManager = nullptr;
        delete g_shadowManager;
        g_shadowManager = nullptr;
    }
    g_shroud = ShroudState{};
    g_scorches.clear();
    g_staticScorches.clear();
}

Shadow *addShadow(RenderObjClass *object, Shadow::ShadowTypeInfo *info)
{
    if (!g_shadowManager) return nullptr;
    return g_shadowManager->Add(object, info);
}

void setShadowColor(unsigned argb)
{
    g_shadowColor = argb;
}

void setShroudLevel(int x, int y, unsigned char level)
{
    if (!ShroudReady()) return;   // the array is created (and refreshed) by the next rendered frame
    if (x < 0 || y < 0 || x >= g_shroud.cellsX || y >= g_shroud.cellsY) return;
    if (TheGlobalData && level < TheGlobalData->m_shroudAlpha) level = TheGlobalData->m_shroudAlpha;
    g_shroud.levels[static_cast<size_t>(y) * g_shroud.cellsX + x] = level;
    ++g_shroud.updates;
}

void setBorderShroudLevel(unsigned char level)
{
    g_shroud.border = level;
}

float shroudCellWorldSize()
{
    if (g_shroud.forced) return g_shroud.cell;
    return TheGlobalData && TheGlobalData->m_partitionCellSize > 1.0f ? TheGlobalData->m_partitionCellSize : 40.0f;
}

void setShroudFilter(bool enable)
{
    g_shroud.filter = enable;
}

void setShroudForTests(int cellsX, int cellsY, float cellSize, const unsigned char *levels, unsigned char border)
{
    g_shroud = ShroudState{};
    g_shroud.forced = true;
    g_shroud.cellsX = cellsX; g_shroud.cellsY = cellsY; g_shroud.cell = cellSize; g_shroud.border = border;
    g_shroud.levels.assign(levels, levels + static_cast<size_t>(cellsX) * cellsY);
}

void shroudMultiplierAt(float x, float y, float rgb[3])
{
    rgb[0] = rgb[1] = rgb[2] = 1.0f;
    if (!ShroudReady()) return;
    auto at = [&](int cx, int cy) -> float {
        if (cx < 0 || cy < 0 || cx >= g_shroud.cellsX || cy >= g_shroud.cellsY) return static_cast<float>(g_shroud.border);
        return static_cast<float>(g_shroud.levels[static_cast<size_t>(cy) * g_shroud.cellsX + cx]);
    };
    float level;
    if (g_shroud.filter) {
        const float cx = x / g_shroud.cell - 0.5f, cy = y / g_shroud.cell - 0.5f;
        const int ix = static_cast<int>(std::floor(cx)), iy = static_cast<int>(std::floor(cy));
        const float tx = cx - static_cast<float>(ix), ty = cy - static_cast<float>(iy);
        level = (at(ix, iy) * (1 - tx) + at(ix + 1, iy) * tx) * (1 - ty) + (at(ix, iy + 1) * (1 - tx) + at(ix + 1, iy + 1) * tx) * ty;
    } else {
        level = at(static_cast<int>(std::floor(x / g_shroud.cell)), static_cast<int>(std::floor(y / g_shroud.cell)));
    }
    const float color[3] = {TheGlobalData ? TheGlobalData->m_shroudColor.red : 1.0f,
                            TheGlobalData ? TheGlobalData->m_shroudColor.green : 1.0f,
                            TheGlobalData ? TheGlobalData->m_shroudColor.blue : 1.0f};
    const unsigned t = Geometry::ShroudTexel(level, color);
    rgb[0] = static_cast<float>((t >> 16) & 0xff) / 255.0f;
    rgb[1] = static_cast<float>((t >> 8) & 0xff) / 255.0f;
    rgb[2] = static_cast<float>(t & 0xff) / 255.0f;
}

void addScorch(float x, float y, float z, float radius, int type, bool permanent)
{
    if (!(radius > 0.01f)) return;
    if (type < 0 || type >= 9) type = 0;
    std::deque<ScorchMark> &list = permanent ? g_staticScorches : g_scorches;
    if (list.size() >= 500) list.pop_front();
    list.push_back(ScorchMark{x, y, z, radius, type});
}

void clearScorches()
{
    g_scorches.clear();
    g_staticScorches.clear();
}

void collect(D3D11Backend &backend, CameraClass &camera)
{
    {
        PERFORMANCE_PROFILE_SCOPE(NativeFxGround);
        if (StartupDiagnostics::nativeFeature("scorches")) CollectScorches(backend, camera);
        if (StartupDiagnostics::nativeFeature("tracks")) CollectTracks(backend, camera);
        if (StartupDiagnostics::nativeFeature("bibs")) CollectBibs(backend, camera);
        CollectGroundLines(backend, camera);
    }
    { PERFORMANCE_PROFILE_SCOPE(NativeFxShadows); if (StartupDiagnostics::nativeFeature("shadows")) CollectShadows(backend, camera); }
    { PERFORMANCE_PROFILE_SCOPE(NativeFxWater); if (StartupDiagnostics::nativeFeature("water")) CollectWater(backend, camera); }
    { PERFORMANCE_PROFILE_SCOPE(NativeFxShroud); if (StartupDiagnostics::nativeFeature("shroud")) CollectShroud(backend, camera); }
    { PERFORMANCE_PROFILE_SCOPE(NativeFxParticles); if (StartupDiagnostics::nativeFeature("particles")) CollectParticles(backend, camera); }
}

void releaseAll()
{
    uninstall();
}

// ---------------------------------------------------------------------------------------------
// Geometry builders (game-state independent; unit-tested offline).
// ---------------------------------------------------------------------------------------------
namespace Geometry
{
void standingWater(const float corners[4][3], float scrollPhase, unsigned diffuse, Mesh &out)
{
    // WaterRenderObjClass::drawTrapezoidWater, flat (un-feathered) form.
    const Vector3 origin(corners[0][0], corners[0][1], corners[0][2]);
    const Vector3 p1(corners[1][0], corners[1][1], corners[1][2]);
    const Vector3 p2(corners[2][0], corners[2][1], corners[2][2]);
    const Vector3 p3(corners[3][0], corners[3][1], corners[3][2]);
    Vector3 uVec1 = p1, vVec1 = p3, uVec2 = p2, vVec2 = p2;
    uVec2 -= vVec1; vVec2 -= uVec1;
    uVec1 -= origin; vVec1 -= origin;
    int uCount = static_cast<int>((uVec1.Length() + uVec2.Length()) / (8.0f * 10.0f));
    int vCount = static_cast<int>((vVec1.Length() + vVec2.Length()) / (8.0f * 10.0f));
    uCount = std::max(1, std::min(uCount, 50)); vCount = std::max(1, std::min(vCount, 50));
    ++uCount; ++vCount;
    const float constA = 0.02f * std::cos(11.0f * scrollPhase), constB = 0.02f * std::cos(5.0f * scrollPhase);
    const float constC = 25.0f * scrollPhase;
    const float constD = 3.14159265f / (4.0f * 10.0f);
    const float ooWaterFactor = 1.0f / 150.0f;
    const unsigned base = static_cast<unsigned>(out.vertices.size());
    for (int j = 0; j < vCount; ++j) {
        const float dv = static_cast<float>(j) / static_cast<float>(vCount - 1);
        for (int i = 0; i < uCount; ++i) {
            const float du = static_cast<float>(i) / static_cast<float>(uCount - 1);
            Vector3 vertex = origin;
            vertex += uVec1 * du;
            vertex += vVec1 * dv;
            vertex += (vVec2 - vVec1) * (dv * du);
            WorldVertexXYZDUV2 v;
            v.x = vertex.X; v.y = vertex.Y; v.z = vertex.Z;
            v.diffuse = diffuse;
            v.u0 = vertex.X * ooWaterFactor + constA * std::sin(constC + vertex.X * constD);
            v.v0 = vertex.Y * ooWaterFactor + constB * std::sin(constC + vertex.Y * constD);
            // Second set (drawTrapezoidWater: vb->u2 / v2): the sparkle texture coordinates, BUMP_SIZE = 50.
            v.u1 = vertex.X / 50.0f;
            v.v1 = (vertex.Y + 0.3f * vertex.X) / 50.0f;
            out.vertices.push_back(v);
        }
    }
    for (int j = 0; j + 1 < vCount; ++j) {
        for (int i = 0; i + 1 < uCount; ++i) {
            const unsigned short a = static_cast<unsigned short>(base + j * uCount + i);
            const unsigned short b = static_cast<unsigned short>(a + 1);
            const unsigned short c = static_cast<unsigned short>(a + uCount), d = static_cast<unsigned short>(c + 1);
            out.indices.push_back(a); out.indices.push_back(d); out.indices.push_back(c);
            out.indices.push_back(a); out.indices.push_back(b); out.indices.push_back(d);
        }
    }
}

bool river(const float *points, int pointCount, int riverStart, float scrollPhase, unsigned diffuse,
           const std::vector<float> &edge, Mesh &out)
{
    // WaterRenderObjClass::drawRiverWater: pairs of points walk inward from `riverStart`.
    if (pointCount < 4 || riverStart < 0 || riverStart >= pointCount - 1) return false;
    const int rows = pointCount / 2;
    const int rectangleCount = rows - 1;
    if (rectangleCount < 1) return false;
    int innerNdx = riverStart, outerNdx = riverStart + 1;
    float endLen = 0.0f, totalLen = 0.0f;
    for (int i = 0; i < pointCount - 1; ++i) {
        const float dx = points[i * 3] - points[(i + 1) * 3], dy = points[i * 3 + 1] - points[(i + 1) * 3 + 1];
        const float len = std::sqrt(dx * dx + dy * dy);
        totalLen += len;
        if (i == innerNdx) endLen = len;
    }
    if (endLen < 1e-3f) return false;
    // No CPU edge profile: the exact legacy layout, two vertices per row (inner u = 0.5 / 1, outer 0 / 0); the
    // alpha-edge texture is sampled on the GPU with the second coordinate set like the river pixel shader.
    const bool gpuEdge = edge.empty();
    const float lengthOfRiver = totalLen / 2.0f - endLen;
    const float repeatCount = lengthOfRiver / endLen;
    const float vScale = repeatCount / static_cast<float>(rectangleCount);
    const float constA = 3.0f * scrollPhase;
    const int kAcross = gpuEdge ? 1 : 6;
    const unsigned base = static_cast<unsigned>(out.vertices.size());
    const float baseAlpha = static_cast<float>((diffuse >> 24) & 0xff) / 255.0f;
    auto edgeAlpha = [&](float t) -> float {
        if (edge.empty()) return 1.0f;
        const float p = std::min(1.0f, std::max(0.0f, t)) * static_cast<float>(edge.size() - 1);
        return edge[static_cast<size_t>(p + 0.5f)];
    };
    for (int i = 0; i < rows; ++i) {
        const float *innerPt = &points[outerNdx * 3];
        const float *outerPt = &points[innerNdx * 3];
        ++outerNdx; --innerNdx;
        if (innerNdx < 0) innerNdx = pointCount - 1;
        if (outerNdx >= pointCount) outerNdx = 0;
        const float wobble = -scrollPhase + vScale * static_cast<float>(i) +
                             std::sin(2.0f * 3.14159265f * (vScale * static_cast<float>(i)) - constA) / 22.0f;
        for (int k = 0; k <= kAcross; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(kAcross);   // 0 outer .. 1 inner
            WorldVertexXYZDUV2 v;
            v.x = outerPt[0] + (innerPt[0] - outerPt[0]) * t;
            v.y = outerPt[1] + (innerPt[1] - outerPt[1]) * t;
            v.z = outerPt[2] + (innerPt[2] - outerPt[2]) * t;
            const float a = baseAlpha * edgeAlpha(t);
            v.diffuse = (static_cast<unsigned>(a * 255.0f + 0.5f) << 24) | (diffuse & 0x00ffffffu);
            v.u0 = 0.5f * t;                 // legacy HEIGHT_TO_USE (0.5) on the inner edge, 0 outside
            v.u1 = t;                        // legacy u2: 1 inner / 0 outer (alpha-edge texture across the river)
            v.v0 = v.v1 = wobble;
            out.vertices.push_back(v);
        }
    }
    const int rowSize = kAcross + 1;
    for (int i = 0; i + 1 < rows; ++i) {
        for (int k = 0; k < kAcross; ++k) {
            const unsigned short a = static_cast<unsigned short>(base + i * rowSize + k), b = static_cast<unsigned short>(a + 1);
            const unsigned short c = static_cast<unsigned short>(a + rowSize), d = static_cast<unsigned short>(c + 1);
            out.indices.push_back(a); out.indices.push_back(b); out.indices.push_back(d);
            out.indices.push_back(a); out.indices.push_back(d); out.indices.push_back(c);
        }
    }
    return true;
}

bool shroud(const unsigned char *levels, int cellsX, int cellsY, float cell, unsigned char border, bool filter,
            int lx0, int ly0, int lx1, int ly1, float (*height)(int, int), bool (*flip)(int, int),
            const float color[3], Mesh &out)
{
    // The overlay is built on the terrain lattice (10 world units) with the terrain's own heights and cell
    // diagonals, then lifted by a constant. Any other tessellation (arbitrary sub-division, interpolated
    // heights) cuts under the surface on steep slopes and leaves unshaded squares there (X64.29 review).
    if (!levels || cellsX <= 0 || cellsY <= 0 || lx1 <= lx0 || ly1 <= ly0 || !height) return false;
    const int nx = lx1 - lx0, ny = ly1 - ly0;
    const int vx = nx + 1, vy = ny + 1;
    if (static_cast<size_t>(vx) * vy > 60000) return false;
    auto at = [&](int x, int y) -> float {
        if (x < 0 || y < 0 || x >= cellsX || y >= cellsY) return static_cast<float>(border);
        return static_cast<float>(levels[static_cast<size_t>(y) * cellsX + x]);
    };
    out.vertices.reserve(out.vertices.size() + static_cast<size_t>(vx) * vy);
    const unsigned base = static_cast<unsigned>(out.vertices.size());
    for (int j = 0; j < vy; ++j) {
        for (int i = 0; i < vx; ++i) {
            const int lx = lx0 + i, ly = ly0 + j;
            const float wx = static_cast<float>(lx) * 10.0f, wy = static_cast<float>(ly) * 10.0f;
            float level;
            if (filter) {
                // bilinear between neighbouring cell centers (texel centers at cell + 0.5)
                const float cx = wx / cell - 0.5f, cy = wy / cell - 0.5f;
                const int ix = static_cast<int>(std::floor(cx)), iy = static_cast<int>(std::floor(cy));
                const float tx = cx - static_cast<float>(ix), ty = cy - static_cast<float>(iy);
                level = (at(ix, iy) * (1 - tx) + at(ix + 1, iy) * tx) * (1 - ty) +
                        (at(ix, iy + 1) * (1 - tx) + at(ix + 1, iy + 1) * tx) * ty;
            } else {
                level = at(static_cast<int>(std::floor(wx / cell)), static_cast<int>(std::floor(wy / cell)));
            }
            WorldVertexXYZDUV2 v;
            v.x = wx; v.y = wy; v.z = height(lx, ly) + 0.35f;
            v.diffuse = ShroudTexel(level, color);
            v.u0 = v.u1 = v.v0 = v.v1 = 0.0f;
            out.vertices.push_back(v);
        }
    }
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            const unsigned short a0 = static_cast<unsigned short>(base + j * vx + i), a1 = static_cast<unsigned short>(a0 + 1);
            const unsigned short c0 = static_cast<unsigned short>(a0 + vx), c1 = static_cast<unsigned short>(c0 + 1);
            if (flip && flip(lx0 + i, ly0 + j)) {
                out.indices.push_back(a1); out.indices.push_back(c0); out.indices.push_back(a0);
                out.indices.push_back(a1); out.indices.push_back(c1); out.indices.push_back(c0);
            } else {
                out.indices.push_back(a0); out.indices.push_back(c1); out.indices.push_back(c0);
                out.indices.push_back(a0); out.indices.push_back(a1); out.indices.push_back(c1);
            }
        }
    }
    return true;
}

unsigned ShroudTexel(float level, const float color[3])
{
    // texel = level * shroudColor, fully lit at 255 (W3DShroud::setShroudLevel)
    const unsigned char l = static_cast<unsigned char>(std::min(255.0f, std::max(0.0f, level)) + 0.5f);
    float r = 255.0f, g = 255.0f, b = 255.0f;
    if (l != 255) { r = l * color[0]; g = l * color[1]; b = l * color[2]; }
    auto q = [](float v) { return static_cast<unsigned>(std::max(0.0f, std::min(255.0f, v + 0.5f))); };
    return 0xff000000u | (q(r) << 16) | (q(g) << 8) | q(b);
}

void billboard(const float center[3], const float right[3], const float up[3], float size, float angle,
               unsigned color, Mesh &out)
{
    // PointGroupClass QUADS: corners (-.5,.5)(-.5,-.5)(.5,-.5)(.5,.5) rotated in the camera plane, UV (0,0)(0,1)(1,1)(1,0).
    static const float corner[4][2] = {{-0.5f, 0.5f}, {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}};
    static const float uv[4][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
    const float c = std::cos(angle), s = std::sin(angle);
    const unsigned short base = static_cast<unsigned short>(out.vertices.size());
    for (int i = 0; i < 4; ++i) {
        const float x = (corner[i][0] * c - corner[i][1] * s) * size;
        const float y = (corner[i][0] * s + corner[i][1] * c) * size;
        WorldVertexXYZDUV2 v;
        v.x = center[0] + right[0] * x + up[0] * y;
        v.y = center[1] + right[1] * x + up[1] * y;
        v.z = center[2] + right[2] * x + up[2] * y;
        v.diffuse = color;
        v.u0 = v.u1 = uv[i][0]; v.v0 = v.v1 = uv[i][1];
        out.vertices.push_back(v);
    }
    out.indices.push_back(base); out.indices.push_back(static_cast<unsigned short>(base + 1)); out.indices.push_back(static_cast<unsigned short>(base + 2));
    out.indices.push_back(base); out.indices.push_back(static_cast<unsigned short>(base + 2)); out.indices.push_back(static_cast<unsigned short>(base + 3));
}
} // namespace Geometry

} // namespace NativeWorldEffects
#endif
