// SPDX-License-Identifier: GPL-3.0-or-later
// Native world-object renderer (see NativeWorldObjects.h). Reads the W3D mesh data model that the
// legacy asset manager loaded (no DX8 object is involved) and draws it through the D3D11 backend's
// TerrainOverlay material:
//   * positions: rigid meshes transformed by the mesh transform; skins deformed by the HTree
//   * lighting: the fixed-function result computed on the CPU per vertex (scene ambient + up to
//     MAX_GLOBAL_LIGHTS directional lights, vertex material emissive/ambient/diffuse, DCG colors,
//     drawable tint/selection, infantry light scale), matching LightEnvironmentClass inputs
//   * state: ShaderClass src/dst blend, alpha test (ref 0x60), depth mask, cull, texturing,
//     gradient; alpha override like DX8TextureCategoryClass
//   * team color: ZHCD palette rows / ZHCA hue shift on the decoded texture, HOUSECOLOR materials
#include "W3DDevice/GameClient/NativeWorldObjects.h"
#include "W3DDevice/GameClient/NativeWorldEffects.h"

#if defined(_WIN64)

#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableInfo.h"
#include "GameLogic/Object.h"
#include "GameLogic/GameLogic.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "GameLogic/PartitionManager.h"
#include "Common/FileSystem.h"
#include "Common/File.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include <WW3D2/animobj.h>
#include <WW3D2/camera.h>
#include <WW3D2/coltest.h>
#include <WW3D2/rinfo.h>
#include <WW3D2/colorspace.h>
#include <WW3D2/hlod.h>
#include <WW3D2/line3d.h>
#include <WW3D2/segline.h>
#include <WW3D2/mapper.h>
#include <WW3D2/matinfo.h>
#include <WW3D2/mesh.h>
#include <WW3D2/meshmdl.h>
#include <WW3D2/rendobj.h>
#include <WW3D2/shader.h>
#include <WW3D2/texture.h>
#include <WW3D2/vertmaterial.h>
#include <WW3D2/ww3d.h>
#include <WW3D2/Backend/D3D11Backend.h>
#include <WW3D2/UITextureContent.h>
#include <WW3D2/WorldSubmission.h>
#include <WW3D2/WorldTextureLoad.h>
#include <WWLib/WWFILE.h>
#include <WWLib/ffactory.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace NativeWorldObjects
{
namespace
{
constexpr unsigned kChunkVertices = 60000;
constexpr unsigned kChunkIndices = 180000;     // multiple of 3
constexpr float kAlphaRef = 96.0f / 255.0f;    // legacy D3DRS_ALPHAREF 0x60, GEQUAL
constexpr unsigned kFailedNames = 12;

struct Entry
{
    RenderObjClass *object;
    int hexColor;
    std::string textureOverride;
    bool foliage;
};

struct Scenery
{
    RenderObjClass *object;
    float x, y, z;
    float radius;
    int kind;
};

struct Chunk
{
    std::unique_ptr<GeometryResidency> geometry;
    std::vector<WorldVertexXYZDUV2> vertices;
    std::vector<unsigned short> indices;
};

enum Layer : unsigned char { LayerOpaque = 0, LayerDecal = 1, LayerTranslucent = 2, LayerOverlay = 3, LayerParticle = 4,
                             LayerGround = 5, LayerCount = 6 };
static_assert(LayerGround + 1 == LayerCount, "submission iterates every layer up to LayerCount");
// Submission order: the ground overlays first (terrain-pass objects), then the layers in numeric order.
constexpr Layer kLayerOrder[LayerCount] = {LayerGround, LayerOpaque, LayerDecal, LayerTranslucent, LayerOverlay, LayerParticle};
std::vector<RenderObjClass *> g_frameObjects;   // addFrameObject, consumed by render()

struct Batch
{
    unsigned chunk;
    unsigned firstIndex;
    unsigned indexCount;
    Texture2DResidency *texture;
    WorldBlend blend;
    bool linear, mipLinear, clamp;
    bool depthWrite, doubleSided, noDiffuse;
    float alphaTest;
    float distance;
    bool translucent;
    unsigned char layer;
    bool stencilOnce;
    bool depthAlways;
    int waterKind;
    Texture2DResidency *waterSparkle, *waterNoise, *waterEdge;
    float waterNoiseOffset[2];
    float waterNoiseScale;
};

struct CachedTexture
{
    std::unique_ptr<Texture2DResidency> residency;
    bool failed = false;
};

using LightSet = SceneLighting;

struct State
{
    std::vector<Entry> entries;
    std::unordered_map<RenderObjClass *, size_t> index;
    std::map<std::pair<int, int>, Scenery> scenery;
    std::map<const RenderObjClass *, int> colorHints;   // snapshot clones inherit their original's team color
    D3D11Backend *backend = nullptr;
    std::vector<Chunk> chunks;
    size_t activeChunk = 0;
    std::map<std::string, CachedTexture> textures;
    std::unique_ptr<Texture2DResidency> white;
    std::vector<Batch> batches;
    Stats stats = {};
    std::string failed[kFailedNames];
    unsigned failedCount = 0;
    std::string unsupportedClasses[kFailedNames];
    unsigned unsupportedClassCount = 0;
};

State &S()
{
    static State s;
    return s;
}

void NoteFailure(const std::string &name)
{
    State &s = S();
    for (unsigned i = 0; i < s.failedCount && i < kFailedNames; ++i)
        if (s.failed[i] == name) return;
    if (s.failedCount < kFailedNames) s.failed[s.failedCount] = name;
    ++s.failedCount;
}

FileReader g_testReader;
unsigned g_testTargetSize[2] = {0, 0};
std::function<void(D3D11Backend &)> g_frameHook;

UITextureBytes ReadFile(const std::string &file)
{
    UITextureBytes bytes;
    if (!g_testReader && TheFileSystem && file.size() > 4 && (file.compare(0, 4, "Art/") == 0 || file.compare(0, 4, "Art\\") == 0)) {
        // Full engine paths (tree textures live under Art/Terrain); other names go through the W3D file factory.
        File *stream = TheFileSystem->openFile(file.c_str(), File::READ | File::BINARY);
        if (!stream) return bytes;
        const Int size = stream->size();
        if (size <= 0 || size > 64 * 1024 * 1024) { stream->close(); bytes.status = UITextureStatus::Invalid; return bytes; }
        bytes.bytes.resize(static_cast<size_t>(size));
        const Int got = stream->read(bytes.bytes.data(), size);
        stream->close();
        bytes.status = got == size ? UITextureStatus::Ready : UITextureStatus::Invalid;
        return bytes;
    }
    if (g_testReader) {
        std::vector<unsigned char> data;
        if (g_testReader(file, data) && !data.empty()) { bytes.status = UITextureStatus::Ready; bytes.bytes = std::move(data); }
        return bytes;
    }
    if (!_TheFileFactory) { bytes.status = UITextureStatus::Invalid; return bytes; }
    file_auto_ptr stream(_TheFileFactory, file.c_str());
    if (!stream || !stream->Is_Available()) return bytes;
    bytes.status = UITextureStatus::Invalid;
    if (!stream->Open()) return bytes;
    const int size = stream->Size();
    if (size <= 0 || size > 64 * 1024 * 1024) return bytes;
    bytes.bytes.resize(size);
    if (stream->Read(bytes.bytes.data(), size) == size) bytes.status = UITextureStatus::Ready;
    return bytes;
}

std::string Lower(const char *text)
{
    std::string s = text ? text : "";
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// ---- team color (legacy W3DAssetManager::Remap_Palette, applied to the decoded RGBA8 level) ----
const unsigned short kHouseColorScale[16] = {255, 239, 223, 211, 195, 174, 167, 151, 135, 123, 107, 91, 79, 63, 47, 35};

void RecolorTeamTexture(WorldTextureImage &image, char kind, int color)
{
    const float r = static_cast<float>((color >> 16) & 0xff), g = static_cast<float>((color >> 8) & 0xff),
                b = static_cast<float>(color & 0xff);
    std::vector<unsigned char> &rgba = image.mips[0];
    if (kind == 'D' || kind == 'd') {
        // Palette-only: the model references the 16 texels of the top row.
        const unsigned count = std::min(16u, image.width);
        for (unsigned i = 0; i < count; ++i) {
            const float scale = static_cast<float>(kHouseColorScale[i]);
            rgba[i * 4 + 0] = static_cast<unsigned char>(std::min(255.0f, scale * r / 255.0f + 0.5f));
            rgba[i * 4 + 1] = static_cast<unsigned char>(std::min(255.0f, scale * g / 255.0f + 0.5f));
            rgba[i * 4 + 2] = static_cast<unsigned char>(std::min(255.0f, scale * b / 255.0f + 0.5f));
            rgba[i * 4 + 3] = 255;
        }
    } else if (kind == 'A' || kind == 'a') {
        // Hue shift of every texel that carries house-color alpha; result is forced opaque.
        Vector3 hsvTeam;
        RGB_To_HSV(hsvTeam, Vector3(r / 255.0f, g / 255.0f, b / 255.0f));
        const size_t texels = static_cast<size_t>(image.width) * image.height;
        for (size_t i = 0; i < texels; ++i) {
            unsigned char *p = &rgba[i * 4];
            if (255 - p[3] != 0) {
                Vector3 hsv, out;
                RGB_To_HSV(hsv, Vector3(p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f));
                hsv.X = hsvTeam.X;
                hsv.Y *= hsvTeam.Y;
                HSV_To_RGB(out, hsv);
                p[0] = static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, out.X * 255.0f)));
                p[1] = static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, out.Y * 255.0f)));
                p[2] = static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, out.Z * 255.0f)));
            }
            p[3] = 255;
        }
    }
}

Texture2DResidency *WhiteTexture(D3D11Backend &backend)
{
    State &s = S();
    if (!s.white) {
        std::vector<std::vector<unsigned char>> mips(1, std::vector<unsigned char>{255, 255, 255, 255});
        s.white.reset(backend.Create_Mipped_Rgba8_Residency(1, 1, mips));
    }
    return s.white.get();
}

// Decoded object texture (dds-first, full mip chain), recolored for ZHC team textures. Failures are
// recorded and answered with the white texture so the geometry is still visible and diagnosable.
Texture2DResidency *ObjectTexture(D3D11Backend &backend, const char *rawName, int hexColor, bool &teamColored,
                                  bool terrainDirFirst = false)
{
    State &s = S();
    teamColored = false;
    if (!rawName || !*rawName || rawName[0] == '!') return WhiteTexture(backend);
    const std::string name = Lower(rawName);
    const bool team = hexColor != 0 && name.size() > 3 && name.compare(0, 3, "zhc") == 0 &&
                      (name[3] == 'd' || name[3] == 'a');
    char key[40];
    std::string cacheKey = name;
    if (team) { sprintf_s(key, "|%08x", static_cast<unsigned>(hexColor)); cacheKey += key; }
    auto found = s.textures.find(cacheKey);
    if (found == s.textures.end()) {
        CachedTexture entry;
        // Tree atlas textures: the legacy buffer looked in Art/Terrain first, then in Art/Textures.
        WorldTextureResult base;
        if (terrainDirFirst && name.find('/') == std::string::npos)
            base = Load_World_Texture_Base("Art/Terrain/" + name, ReadFile);
        if (base.status != UITextureStatus::Ready)
            base = Load_World_Texture_Base(name, ReadFile);
        if (base.status == UITextureStatus::Ready && base.image.width && base.image.height) {
            if (team) RecolorTeamTexture(base.image, name[3], hexColor);
            WorldTexture::BuildMips(base.image, 16);
            entry.residency.reset(backend.Create_Mipped_Rgba8_Residency(base.image.width, base.image.height,
                                                                       base.image.mips));
            ++s.stats.textureLoads;
        } else {
            entry.failed = true;
            ++s.stats.textureFailures;
            NoteFailure(name);
        }
        found = s.textures.emplace(cacheKey, std::move(entry)).first;
    }
    teamColored = team && !found->second.failed;
    if (found->second.failed) return WhiteTexture(backend);
    return found->second.residency.get();
}

Chunk &ChunkWithRoom(D3D11Backend &backend, unsigned vertices, unsigned indices)
{
    State &s = S();
    for (;; ++s.activeChunk) {
        if (s.activeChunk == s.chunks.size()) {
            s.chunks.emplace_back();
            s.chunks.back().geometry.reset(backend.Create_Dynamic_Geometry(kChunkVertices, kChunkIndices));
        }
        Chunk &c = s.chunks[s.activeChunk];
        if (c.vertices.size() + vertices <= kChunkVertices && c.indices.size() + indices <= kChunkIndices)
            return c;
    }
}

// Per-frame working data shared by all meshes.
struct Frame
{
    D3D11Backend *backend;
    CameraClass *camera;
    LightSet lights;
    LightSet infantryLights;
    // per object
    const char *textureOverride;
    bool foliage;
    int hexColor;
    float opacity;
    float tint[3];
    bool infantry;
    bool shrouded;          // drawn through the shroud (legacy shroud material pass: fogged ghosts, partial clear)
    float distance;
    Vector3 cameraPosition;
    Matrix3D view;          // D3D-convention world -> camera transform (texture generation)
    unsigned drawnMeshes;
};

// Legacy shroud material pass for one vertex: the projected shroud texture multiplies the lit color.
unsigned ApplyShroud(unsigned argb, float x, float y)
{
    float m[3];
    NativeWorldEffects::shroudMultiplierAt(x, y, m);
    if (m[0] >= 0.999f && m[1] >= 0.999f && m[2] >= 0.999f) return argb;
    auto scale = [](unsigned c, float k) { return static_cast<unsigned>(std::min(255.0f, static_cast<float>(c) * k + 0.5f)); };
    return (argb & 0xff000000u) | (scale((argb >> 16) & 0xff, m[0]) << 16) | (scale((argb >> 8) & 0xff, m[1]) << 8) |
           scale(argb & 0xff, m[2]);
}

unsigned PackColor(float r, float g, float b, float a)
{
    auto c = [](float v) { return static_cast<unsigned>(std::max(0.0f, std::min(255.0f, v * 255.0f + 0.5f))); };
    return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
}

bool IsHouseColorMesh(const char *meshName)
{
    if (!meshName) return false;
    const char *name = std::strchr(meshName, '.');
    if (name && *(name + 1)) ++name; else name = meshName;
    return _strnicmp(name, "HOUSECOLOR", 10) == 0;
}

struct MaterialLook
{
    bool lighting = true;
    float emissive[3] = {0, 0, 0};
    float ambient[3] = {1, 1, 1};
    float diffuse[3] = {1, 1, 1};
    float opacity = 1.0f;
    bool diffuseFromVertex = false;
    bool ambientFromVertex = false;
    bool emissiveFromVertex = false;
    bool hasMapper = false;
};

MaterialLook LookOf(VertexMaterialClass *vmat, bool houseColor, int hexColor, State &s)
{
    MaterialLook look;
    if (!vmat) { look.lighting = false; return look; }
    Vector3 v;
    look.lighting = vmat->Get_Lighting();
    vmat->Get_Emissive(&v); look.emissive[0] = v.X; look.emissive[1] = v.Y; look.emissive[2] = v.Z;
    vmat->Get_Ambient(&v); look.ambient[0] = v.X; look.ambient[1] = v.Y; look.ambient[2] = v.Z;
    vmat->Get_Diffuse(&v); look.diffuse[0] = v.X; look.diffuse[1] = v.Y; look.diffuse[2] = v.Z;
    look.opacity = vmat->Get_Opacity();
    look.diffuseFromVertex = vmat->Get_Diffuse_Color_Source() == VertexMaterialClass::COLOR1;
    look.ambientFromVertex = vmat->Get_Ambient_Color_Source() == VertexMaterialClass::COLOR1;
    look.emissiveFromVertex = vmat->Get_Emissive_Color_Source() == VertexMaterialClass::COLOR1;
    look.hasMapper = vmat->Peek_Mapper(0) != nullptr;
    if (houseColor && hexColor) {
        // Legacy Recolor_Vertex_Material: ambient and diffuse become the team color.
        const float r = static_cast<float>((hexColor >> 16) & 0xff) / 255.0f;
        const float g = static_cast<float>((hexColor >> 8) & 0xff) / 255.0f;
        const float b = static_cast<float>(hexColor & 0xff) / 255.0f;
        look.ambient[0] = look.diffuse[0] = r;
        look.ambient[1] = look.diffuse[1] = g;
        look.ambient[2] = look.diffuse[2] = b;
        ++s.stats.teamColored;
    }
    return look;
}

// Fixed-function vertex color: emissive + ambient*sceneAmbient + diffuse*sum(max(0,N.L)*light).
unsigned LitColor(const MaterialLook &look, const Frame &f, const LightSet &lights, const Vector3 &normal,
                  unsigned vertexColor, bool hasVertexColor, float alphaScale)
{
    const float vr = hasVertexColor ? static_cast<float>((vertexColor >> 16) & 0xff) / 255.0f : 1.0f;
    const float vg = hasVertexColor ? static_cast<float>((vertexColor >> 8) & 0xff) / 255.0f : 1.0f;
    const float vb = hasVertexColor ? static_cast<float>(vertexColor & 0xff) / 255.0f : 1.0f;
    const float va = hasVertexColor ? static_cast<float>((vertexColor >> 24) & 0xff) / 255.0f : 1.0f;
    float alpha = look.opacity;
    if (look.diffuseFromVertex && hasVertexColor) alpha = va;
    alpha *= alphaScale;
    if (!look.lighting) {
        const float r = look.diffuseFromVertex && hasVertexColor ? vr : look.diffuse[0];
        const float g = look.diffuseFromVertex && hasVertexColor ? vg : look.diffuse[1];
        const float b = look.diffuseFromVertex && hasVertexColor ? vb : look.diffuse[2];
        return PackColor(r, g, b, alpha);
    }
    const float dr = look.diffuseFromVertex && hasVertexColor ? vr : look.diffuse[0];
    const float dg = look.diffuseFromVertex && hasVertexColor ? vg : look.diffuse[1];
    const float db = look.diffuseFromVertex && hasVertexColor ? vb : look.diffuse[2];
    const float ar = look.ambientFromVertex && hasVertexColor ? vr : look.ambient[0];
    const float ag = look.ambientFromVertex && hasVertexColor ? vg : look.ambient[1];
    const float ab = look.ambientFromVertex && hasVertexColor ? vb : look.ambient[2];
    float sr = 0, sg = 0, sb = 0;
    for (unsigned i = 0; i < lights.count; ++i) {
        float shade = normal.X * lights.direction[i][0] + normal.Y * lights.direction[i][1] +
                      normal.Z * lights.direction[i][2];
        if (shade <= 0.0f) continue;
        if (shade > 1.0f) shade = 1.0f;
        sr += shade * lights.diffuse[i][0];
        sg += shade * lights.diffuse[i][1];
        sb += shade * lights.diffuse[i][2];
    }
    const float er = look.emissiveFromVertex && hasVertexColor ? vr : look.emissive[0];
    const float eg = look.emissiveFromVertex && hasVertexColor ? vg : look.emissive[1];
    const float eb = look.emissiveFromVertex && hasVertexColor ? vb : look.emissive[2];
    const float r = er + ar * (lights.ambient[0] + f.tint[0]) + dr * sr;
    const float g = eg + ag * (lights.ambient[1] + f.tint[1]) + dg * sg;
    const float b = eb + ab * (lights.ambient[2] + f.tint[2]) + db * sb;
    return PackColor(r, g, b, alpha);
}

// Legacy ShaderClass state reduced to what the native TerrainOverlay material can express.
struct ShaderMapping
{
    WorldBlend blend = WorldBlend::SrcAlpha;
    bool additive = false;
    bool blendKnown = true;     // false: the pair is outside the supported WorldBlend set (approximated)
    float alphaTest = 0.0f;
    bool depthWrite = true;
    bool doubleSided = false;
    bool noDiffuse = false;
};

ShaderMapping MapShader(const ShaderClass &shader, bool twoSided)
{
    ShaderMapping m;
    const ShaderClass::SrcBlendFuncType src = shader.Get_Src_Blend_Func();
    const ShaderClass::DstBlendFuncType dst = shader.Get_Dst_Blend_Func();
    if (src == ShaderClass::SRCBLEND_ONE && dst == ShaderClass::DSTBLEND_ZERO) m.blend = WorldBlend::Opaque;
    else if (src == ShaderClass::SRCBLEND_SRC_ALPHA && dst == ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA) m.blend = WorldBlend::SrcAlpha;
    else if (src == ShaderClass::SRCBLEND_ONE && dst == ShaderClass::DSTBLEND_ONE) { m.blend = WorldBlend::Additive; m.additive = true; }
    else if (src == ShaderClass::SRCBLEND_SRC_ALPHA && dst == ShaderClass::DSTBLEND_ONE) m.blend = WorldBlend::AdditiveAlpha;
    else if (src == ShaderClass::SRCBLEND_ZERO && dst == ShaderClass::DSTBLEND_SRC_COLOR) m.blend = WorldBlend::Multiply;
    else if (src == ShaderClass::SRCBLEND_ZERO && dst == ShaderClass::DSTBLEND_ONE_MINUS_SRC_COLOR) m.blend = WorldBlend::InvMultiply;
    else if (src == ShaderClass::SRCBLEND_ONE && dst == ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA) m.blend = WorldBlend::Premultiplied;
    else { m.blend = WorldBlend::SrcAlpha; m.blendKnown = false; }
    m.alphaTest = shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_ENABLE ? kAlphaRef : 0.0f;
    m.depthWrite = shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_ENABLE;
    m.doubleSided = twoSided || shader.Get_Cull_Mode() == ShaderClass::CULL_MODE_DISABLE;
    m.noDiffuse = shader.Get_Primary_Gradient() == ShaderClass::GRADIENT_DISABLE;
    return m;
}

struct StateKey
{
    unsigned shaderBits;
    TextureClass *texture;
    VertexMaterialClass *material;
    bool operator==(const StateKey &o) const
    {
        return shaderBits == o.shaderBits && texture == o.texture && material == o.material;
    }
};

struct PendingBatch
{
    StateKey key;
    std::vector<unsigned short> indices;
};

void ProcessMesh(Frame &f, MeshClass *mesh, State &s)
{
    MeshModelClass *model = mesh->Peek_Model();
    if (!model || !mesh->Is_Not_Hidden_At_All()) return;
    const int vertexCount = model->Get_Vertex_Count();
    const int polyCount = model->Get_Polygon_Count();
    if (vertexCount <= 0 || polyCount <= 0 || vertexCount > 65535) return;
    ++s.stats.meshes;

    // --- positions and normals in world space ---
    std::vector<Vector3> position(vertexCount), normal(vertexCount);
    const bool skin = model->Get_Flag(MeshGeometryClass::SKIN) != 0;
    if (skin && mesh->Get_Container() && mesh->Get_Container()->Get_HTree()) {
        mesh->Get_Deformed_Vertices(position.data(), normal.data());
        ++s.stats.skinned;
    } else {
        Matrix3D tm = mesh->Get_Transform();
        if (model->Get_Flag(MeshGeometryClass::ALIGNED)) {
            const Vector3 p = tm.Get_Translation();
            Vector3 z;
            f.camera->Get_Transform().Get_Z_Vector(&z);
            tm.Obj_Look_At(p, p + z, 0.0f);
        } else if (model->Get_Flag(MeshGeometryClass::ORIENTED)) {
            const Vector3 p = tm.Get_Translation();
            tm.Obj_Look_At(p, f.cameraPosition, 0.0f);
        }
        const Vector3 *srcPos = model->Get_Vertex_Array();
        const Vector3 *srcNormal = model->Get_Vertex_Normal_Array();
        for (int i = 0; i < vertexCount; ++i) {
            Matrix3D::Transform_Vector(tm, srcPos[i], &position[i]);
            if (srcNormal) Matrix3D::Rotate_Vector(tm, srcNormal[i], &normal[i]);
            else normal[i] = Vector3(0, 0, 1);
        }
    }
    for (int i = 0; i < vertexCount; ++i) {
        const float len = std::sqrt(normal[i].X * normal[i].X + normal[i].Y * normal[i].Y + normal[i].Z * normal[i].Z);
        if (len > 1e-6f) { normal[i].X /= len; normal[i].Y /= len; normal[i].Z /= len; }
        else normal[i] = Vector3(0, 0, 1);
    }

    const int passCount = std::max(1, std::min(model->Get_Pass_Count(), 4));
    if (passCount > 1) ++s.stats.stateMultiPass;   // informational: every pass is drawn natively
    const bool houseMesh = IsHouseColorMesh(mesh->Get_Name());
    const bool twoSided = model->Get_Flag(MeshGeometryClass::TWO_SIDED) != 0;
    // Tank/truck treads publish their scroll as a Material_Override in the mesh user data (the DX8 path
    // feeds it to the vertex material's linear-offset mapper); the offset is added to the UVs here.
    float uvOffsetU = 0.0f, uvOffsetV = 0.0f;
    if (void *userData = mesh->Get_User_Data()) {
        if (*static_cast<int *>(userData) == RenderObjClass::USER_DATA_MATERIAL_OVERRIDE) {
            const auto *override_ = static_cast<RenderObjClass::Material_Override *>(userData);
            uvOffsetU = override_->customUVOffset.X;
            uvOffsetV = override_->customUVOffset.Y;
        }
    }

    const TriIndex *polys = model->Get_Polygon_Array();
    const LightSet &lights = f.infantry ? f.infantryLights : f.lights;
    const bool opacityOverride = f.opacity < 0.999f;
    bool drewAny = false;

    for (int pass = 0; pass < passCount; ++pass) {
        const Vector2 *uv = model->Get_UV_Array(pass, 0);
        const unsigned *dcg = model->Get_DCG_Array(pass);
        const unsigned *dig = model->Get_DIG_Array(pass);

        // --- group this pass's polygons by (shader, texture, material) ---
        std::vector<PendingBatch> pending;
        for (int p = 0; p < polyCount; ++p) {
            const TriIndex &tri = polys[p];
            if (tri.I >= static_cast<unsigned>(vertexCount) || tri.J >= static_cast<unsigned>(vertexCount) ||
                tri.K >= static_cast<unsigned>(vertexCount)) continue;
            StateKey key;
            key.shaderBits = model->Get_Shader(p, pass).Get_Bits();
            key.texture = model->Peek_Texture(p, pass, 0);
            key.material = model->Peek_Material(tri.I, pass);
            PendingBatch *target = nullptr;
            for (PendingBatch &b : pending)
                if (b.key == key) { target = &b; break; }
            if (!target) { pending.push_back(PendingBatch{key, {}}); target = &pending.back(); }
            target->indices.push_back(static_cast<unsigned short>(tri.I));
            target->indices.push_back(static_cast<unsigned short>(tri.J));
            target->indices.push_back(static_cast<unsigned short>(tri.K));
        }
        if (pending.empty()) continue;

        // Distinct materials (each needs its own lit vertex block).
        std::vector<VertexMaterialClass *> materials;
        for (const PendingBatch &b : pending)
            if (std::find(materials.begin(), materials.end(), b.key.material) == materials.end())
                materials.push_back(b.key.material);

        unsigned totalIndices = 0;
        for (const PendingBatch &b : pending) totalIndices += static_cast<unsigned>(b.indices.size());
        const unsigned totalVertices = static_cast<unsigned>(vertexCount) * static_cast<unsigned>(materials.size());
        if (totalVertices > kChunkVertices || totalIndices > kChunkIndices) {
            ++s.stats.chunkOverflow;
            continue;
        }
        Chunk &chunk = ChunkWithRoom(*f.backend, totalVertices, totalIndices);
        const unsigned chunkIndex = static_cast<unsigned>(s.activeChunk);

        std::vector<unsigned> blockBase(materials.size());
        std::vector<MaterialLook> looks(materials.size());
        for (size_t m = 0; m < materials.size(); ++m) {
            looks[m] = LookOf(materials[m], houseMesh, f.hexColor, s);

            // Texture mapper (environment / scroll / rotate ...): evaluated on the CPU per material block.
            TextureMapperClass::NativeUV mapUV = {};
            bool mapped = false;
            if (looks[m].hasMapper) {
                TextureMapperClass *mapper = materials[m]->Peek_Mapper(0);
                const int id = mapper ? mapper->Mapper_ID() : 0;
                if (mapper && StartupDiagnostics::nativeFeature("mappers") && mapper->Native_Evaluate(mapUV)) {
                    mapped = true;
                    ++s.stats.mapperApplied;
                } else {
                    ++s.stats.unsupportedState; ++s.stats.stateMapper;
                    ++s.stats.mapperKinds[id >= 0 && id < 24 ? id : 0];
                }
            }

            blockBase[m] = static_cast<unsigned>(chunk.vertices.size());
            for (int v = 0; v < vertexCount; ++v) {
                WorldVertexXYZDUV2 out;
                out.x = position[v].X; out.y = position[v].Y; out.z = position[v].Z;
                unsigned vertexColor = dcg ? dcg[v] : 0xffffffffu;
                if (!dcg && dig && looks[m].emissiveFromVertex) vertexColor = dig[v];
                out.diffuse = LitColor(looks[m], f, lights, normal[v], vertexColor,
                                       dcg != nullptr || (dig && looks[m].emissiveFromVertex), 1.0f);
                if (f.shrouded) out.diffuse = ApplyShroud(out.diffuse, position[v].X, position[v].Y);
                float u = uv ? uv[v].X : 0.0f;
                float w = uv ? uv[v].Y : 0.0f;
                if (mapped) {
                    float sx = u, sy = w;
                    if (mapUV.texgen != 0) {
                        Vector3 pc, nc;
                        Matrix3D::Transform_Vector(f.view, position[v], &pc);
                        Matrix3D::Rotate_Vector(f.view, normal[v], &nc);
                        if (mapUV.texgen == 1) {
                            sx = nc.X; sy = nc.Y;                    // camera-space normal
                        } else {
                            // D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR: r = 2(n.u)n - u, u = unit eye->vertex
                            const float len = std::sqrt(pc.X * pc.X + pc.Y * pc.Y + pc.Z * pc.Z);
                            if (len > 1e-6f) {
                                const float ux = pc.X / len, uy = pc.Y / len, uz = pc.Z / len;
                                const float d = nc.X * ux + nc.Y * uy + nc.Z * uz;
                                sx = 2.0f * d * nc.X - ux;
                                sy = 2.0f * d * nc.Y - uy;
                            } else { sx = 0.0f; sy = 0.0f; }
                        }
                    }
                    u = mapUV.m[0] * sx + mapUV.m[1] * sy + mapUV.m[2];
                    w = mapUV.m[3] * sx + mapUV.m[4] * sy + mapUV.m[5];
                }
                out.u0 = u + uvOffsetU;
                out.v0 = w + uvOffsetV;
                out.u1 = out.u0; out.v1 = out.v0;
                chunk.vertices.push_back(out);
            }
        }
        s.stats.vertices += totalVertices;

        for (const PendingBatch &b : pending) {
            const size_t m = std::find(materials.begin(), materials.end(), b.key.material) - materials.begin();
            const ShaderClass shader(b.key.shaderBits);
            Batch out = {};
            out.chunk = chunkIndex;

            const ShaderMapping mapping = MapShader(shader, twoSided);
            out.blend = mapping.blend;
            bool additive = mapping.additive;
            if (!mapping.blendKnown) { ++s.stats.unsupportedState; ++s.stats.stateBlend; }
            out.alphaTest = mapping.alphaTest;
            out.depthWrite = mapping.depthWrite;
            out.doubleSided = mapping.doubleSided;
            out.noDiffuse = mapping.noDiffuse;
            {
                const bool m2x = shader.Get_Primary_Gradient() == ShaderClass::GRADIENT_MODULATE2X;
                const bool sec = shader.Get_Secondary_Gradient() == ShaderClass::SECONDARY_GRADIENT_ENABLE;
                const bool det = shader.Get_Post_Detail_Color_Func() != ShaderClass::DETAILCOLOR_DISABLE;
                const bool dual = b.key.material != nullptr && (model->Has_Texture_Array(pass, 1) || model->Peek_Single_Texture(pass, 1) != nullptr);
                if (m2x) ++s.stats.stateModulate2x;
                if (sec) ++s.stats.stateSecondary;
                if (det) ++s.stats.stateDetail;
                if (dual) ++s.stats.stateDualTexture;
                if (m2x || sec || det || dual) ++s.stats.unsupportedState;
            }

            // alpha override (stealth/fade): legacy forced SRC_ALPHA/INV_SRC_ALPHA, additive dims diffuse
            float alphaScale = 1.0f;
            if (opacityOverride) {
                if (additive) {
                    alphaScale = 1.0f;   // additive dims through the diffuse color below
                } else {
                    out.blend = WorldBlend::SrcAlpha;
                    alphaScale = f.opacity;
                    if (out.alphaTest > 0.0f) out.alphaTest = kAlphaRef * f.opacity;
                }
                // Rewrite this material's block colors with the override applied.
                for (int v = 0; v < vertexCount; ++v) {
                    WorldVertexXYZDUV2 &vtx = chunk.vertices[blockBase[m] + v];
                    unsigned vertexColor = dcg ? dcg[v] : 0xffffffffu;
                    if (!dcg && dig && looks[m].emissiveFromVertex) vertexColor = dig[v];
                    MaterialLook look = looks[m];
                    if (additive) { for (float &c : look.diffuse) c *= f.opacity; for (float &c : look.ambient) c *= f.opacity; }
                    vtx.diffuse = LitColor(look, f, lights, normal[v], vertexColor,
                                           dcg != nullptr || (dig && looks[m].emissiveFromVertex), alphaScale);
                    if (f.shrouded) vtx.diffuse = ApplyShroud(vtx.diffuse, position[v].X, position[v].Y);
                }
            }
            if (out.noDiffuse && opacityOverride && !additive) out.noDiffuse = false;

            // texture
            bool teamColored = false;
            const bool overridden = f.textureOverride != nullptr && *f.textureOverride;
            const bool texturing = overridden ||
                (shader.Get_Texturing() == ShaderClass::TEXTURING_ENABLE && b.key.texture != nullptr);
            if (texturing) {
                const char *textureName = overridden ? f.textureOverride : b.key.texture->Get_Texture_Name().str();
                out.texture = ObjectTexture(*f.backend, textureName, f.hexColor, teamColored, overridden);
                if (teamColored) ++s.stats.teamColored;
                if (b.key.texture) {
                    const TextureFilterClass &filter = b.key.texture->Get_Filter();
                    out.clamp = filter.Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP &&
                                filter.Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP;
                }
            } else {
                out.texture = WhiteTexture(*f.backend);
            }
            out.linear = true;
            out.mipLinear = true;

            if (f.foliage) {
                // W3DTreeBuffer's SC_ALPHA_DETAIL: opaque, alpha tested, double sided, depth writing, modulate.
                out.blend = WorldBlend::Opaque;
                out.alphaTest = kAlphaRef;
                out.doubleSided = true;
                out.depthWrite = true;
                out.noDiffuse = false;
            }

            // emit indices into the shared chunk
            out.firstIndex = static_cast<unsigned>(chunk.indices.size());
            for (unsigned short idx : b.indices)
                chunk.indices.push_back(static_cast<unsigned short>(blockBase[m] + idx));
            out.indexCount = static_cast<unsigned>(b.indices.size());
            out.distance = f.distance;
            out.translucent = (out.blend != WorldBlend::Opaque && !f.foliage) || opacityOverride;
            if (out.alphaTest > 0.0f && out.blend == WorldBlend::Opaque) out.translucent = opacityOverride;
            // Later passes of an opaque base pass (environment/gloss) draw after it even when additive.
            out.layer = out.translucent ? LayerTranslucent : LayerOpaque;
            s.batches.push_back(out);
            s.stats.triangles += out.indexCount / 3;
            drewAny = true;
        }
    }
    if (drewAny) { ++s.stats.meshesDrawn; ++f.drawnMeshes; }
}
void ProcessObject(Frame &f, RenderObjClass *object, State &s, unsigned depth)
{
    if (!object || depth > 8 || !object->Is_Not_Hidden_At_All()) return;
    switch (object->Class_ID()) {
    case RenderObjClass::CLASSID_MESH:
        ProcessMesh(f, static_cast<MeshClass *>(object), s);
        return;
    case RenderObjClass::CLASSID_HLOD: {
        HLodClass *hlod = static_cast<HLodClass *>(object);
        // Advance the animation / recompute sub-object and bone transforms exactly as the first step
        // of HLodClass::Render does, without registering anything with the DX8 mesh renderer.
        {
            RenderInfoClass rinfo(*f.camera);
            static_cast<Animatable3DObjClass *>(hlod)->Animatable3DObjClass::Render(rinfo);
        }
        const int lod = hlod->Get_LOD_Level();
        const int count = hlod->Get_Lod_Model_Count(lod);
        for (int i = 0; i < count; ++i) {
            RenderObjClass *sub = hlod->Peek_Lod_Model(lod, i);
            if (sub && sub->Class_ID() != RenderObjClass::CLASSID_OBBOX) ProcessObject(f, sub, s, depth + 1);
        }
        const int extra = hlod->Get_Additional_Model_Count();
        for (int i = 0; i < extra; ++i) ProcessObject(f, hlod->Peek_Additional_Model(i), s, depth + 1);
        return;
    }
    case RenderObjClass::CLASSID_LINE3D:
        if (NativeWorldEffects::emitLine3D(*f.backend, *static_cast<Line3DClass *>(object), f.distance)) ++f.drawnMeshes;
        return;
    case RenderObjClass::CLASSID_SEGLINE:
        if (NativeWorldEffects::emitSegLine(*f.backend, *static_cast<SegmentedLineClass *>(object), f.cameraPosition,
                                            f.distance)) ++f.drawnMeshes;
        return;
    case RenderObjClass::CLASSID_NULL:
    case RenderObjClass::CLASSID_OBBOX:
    case RenderObjClass::CLASSID_AABOX:
        return;
    default: {
        // Generic containers (collections, dist-LOD): recurse through their sub-objects.
        const int count = object->Get_Num_Sub_Objects();
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                RenderObjClass *sub = object->Get_Sub_Object(i);
                if (sub) { ProcessObject(f, sub, s, depth + 1); sub->Release_Ref(); }
            }
            return;
        }
        ++s.stats.unsupportedClass;
        return;
    }
    }
}

void BuildLights(LightSet &set, bool infantry)
{
    const GlobalData *g = TheGlobalData;
    const int tod = g->m_timeOfDay;
    const GlobalData::TerrainLighting *lighting = g->m_terrainObjectsLighting[tod];
    set.ambient[0] = std::min(1.0f, lighting[0].ambient.red);
    set.ambient[1] = std::min(1.0f, lighting[0].ambient.green);
    set.ambient[2] = std::min(1.0f, lighting[0].ambient.blue);
    float scale = 1.0f;
    if (infantry)
        scale = g->m_scriptOverrideInfantryLightScale != -1.0f ? g->m_scriptOverrideInfantryLightScale
                                                                : g->m_infantryLightScale[tod];
    set.count = static_cast<unsigned>(std::max(0, std::min<int>(g->m_numGlobalLights, MAX_GLOBAL_LIGHTS)));
    for (unsigned i = 0; i < set.count; ++i) {
        Vector3 dir(lighting[i].lightPos.x, lighting[i].lightPos.y, lighting[i].lightPos.z);
        const float len = std::sqrt(dir.X * dir.X + dir.Y * dir.Y + dir.Z * dir.Z);
        if (len > 1e-6f) { dir.X /= len; dir.Y /= len; dir.Z /= len; }
        set.direction[i][0] = -dir.X; set.direction[i][1] = -dir.Y; set.direction[i][2] = -dir.Z;
        set.diffuse[i][0] = lighting[i].diffuse.red * scale;
        set.diffuse[i][1] = lighting[i].diffuse.green * scale;
        set.diffuse[i][2] = lighting[i].diffuse.blue * scale;
    }
}

void FillBase(CameraClass &camera, WorldSubmission &base)
{
    base = WorldSubmission{};
    World_Identity(base.world);
    World_From_Matrix3D(&camera.Get_View_Matrix()[0][0], base.view);
    Matrix4x4 projection;
    camera.Get_D3D_Projection_Matrix(&projection);
    World_From_Matrix4x4(&projection[0][0], base.projection);
    int width = 0, height = 0, bits = 0;
    bool windowed = false;
    if (g_testTargetSize[0]) { width = static_cast<int>(g_testTargetSize[0]); height = static_cast<int>(g_testTargetSize[1]); }
    else WW3D::Get_Render_Target_Resolution(width, height, bits, windowed);
    const ViewportClass &viewport = camera.Get_Viewport();
    float zMin = 0.0f, zMax = 1.0f;
    camera.Get_Zbuffer_Range(zMin, zMax);
    base.viewport.x = static_cast<unsigned>(viewport.Min.X * static_cast<float>(width));
    base.viewport.y = static_cast<unsigned>(viewport.Min.Y * static_cast<float>(height));
    base.viewport.width = static_cast<unsigned>((viewport.Max.X - viewport.Min.X) * static_cast<float>(width));
    base.viewport.height = static_cast<unsigned>((viewport.Max.Y - viewport.Min.Y) * static_cast<float>(height));
    base.viewport.min_z = zMin;
    base.viewport.max_z = zMax;
    base.material = WorldMaterial::TerrainOverlay;
}
} // namespace

namespace { bool g_forceActive = false; }

bool active()
{
    return g_forceActive || StartupDiagnostics::ordinaryPlay();
}

void setActiveForTests(bool on)
{
    g_forceActive = on;
}

int colorOf(const RenderObjClass *object)
{
    State &s = S();
    auto found = s.index.find(const_cast<RenderObjClass *>(object));
    if (found != s.index.end()) return s.entries[found->second].hexColor;
    auto hint = s.colorHints.find(object);
    return hint != s.colorHints.end() ? hint->second : 0;
}

void inheritColor(const RenderObjClass *to, const RenderObjClass *from)
{
    if (!to || !from || !active()) return;
    const int color = colorOf(from);
    if (color) S().colorHints[to] = color;
}

void add(RenderObjClass *object, int hexColor)
{
    if (!object || !active()) return;
    State &s = S();
    if (hexColor == 0) {
        auto hint = s.colorHints.find(object);
        if (hint != s.colorHints.end()) hexColor = hint->second;
    }
    if (s.index.find(object) != s.index.end()) { s.entries[s.index[object]].hexColor = hexColor; return; }
    object->Add_Ref();
    s.index[object] = s.entries.size();
    s.entries.push_back(Entry{object, hexColor, std::string(), false});
}

void remove(RenderObjClass *object)
{
    if (!object) return;
    State &s = S();
    auto found = s.index.find(object);
    if (found == s.index.end()) return;
    const size_t at = found->second;
    s.index.erase(found);
    RenderObjClass *held = s.entries[at].object;
    if (at + 1 != s.entries.size()) {
        s.entries[at] = s.entries.back();
        s.index[s.entries[at].object] = at;
    }
    s.entries.pop_back();
    s.colorHints.erase(held);
    held->Release_Ref();
}

unsigned registered()
{
    return static_cast<unsigned>(S().entries.size());
}

bool contains(const RenderObjClass *object)
{
    return object && S().index.find(const_cast<RenderObjClass *>(object)) != S().index.end();
}

bool castRay(RayCollisionTestClass &raytest, bool testAll, int collisionType)
{
    // Mirrors RTS3DScene::castRay: ray-sphere reject, then the object's own geometry test, shortening
    // the ray to the nearest hit so later candidates must be closer.
    State &s = S();
    CastResultStruct result;
    RayCollisionTestClass temp(raytest.Ray, &result);
    temp.CollisionType = COLL_TYPE_ALL;
    temp.CheckTranslucent = true;
    bool hit = false;
    for (const Entry &entry : s.entries) {
        RenderObjClass *robj = entry.object;
        if (!(robj->Get_Collision_Type() & collisionType)) continue;
        if (!testAll && !robj->Is_Really_Visible()) continue;
        const SphereClass &sphere = robj->Get_Bounding_Sphere();
        const Vector3 toCenter(sphere.Center - temp.Ray.Get_P0());
        const float alpha = Vector3::Dot_Product(toCenter, temp.Ray.Get_Dir());
        const float beta = sphere.Radius * sphere.Radius - (Vector3::Dot_Product(toCenter, toCenter) - alpha * alpha);
        if (beta < 0.0f) continue;
        if (robj->Cast_Ray(temp)) {
            raytest.CollidedRenderObj = robj;
            hit = true;
            Vector3 newEnd;
            temp.Ray.Compute_Point(temp.Result->Fraction, &newEnd);
            temp.Ray.Set(raytest.Ray.Get_P0(), newEnd);
            temp.Result->Fraction = 1.0f;
        }
    }
    raytest.Ray = temp.Ray;
    return hit;
}

EffectState EffectStateFromShader(unsigned shaderBits, bool twoSided)
{
    const ShaderMapping m = MapShader(ShaderClass(shaderBits), twoSided);
    return EffectState{m.blend, m.depthWrite, m.doubleSided, m.noDiffuse, m.alphaTest, m.blendKnown};
}

Texture2DResidency *effectTexture(D3D11Backend &backend, const char *name, int hexColor)
{
    bool team = false;
    // Texture names without a 3-letter extension (INI shadow/particle names) are TGA files.
    std::string fixed = name ? name : "";
    if (!fixed.empty() && (fixed.size() < 4 || fixed[fixed.size() - 4] != '.')) fixed += ".tga";
    return ObjectTexture(backend, fixed.c_str(), hexColor, team);
}

bool effectImage(const char *name, std::vector<unsigned char> &rgba, unsigned &width, unsigned &height)
{
    if (!name || !*name) return false;
    std::string fixed = Lower(name);
    if (fixed.size() < 4 || fixed[fixed.size() - 4] != '.') fixed += ".tga";
    WorldTextureResult base = Load_World_Texture_Base(fixed, ReadFile);
    if (base.status != UITextureStatus::Ready || base.image.mips.empty() || !base.image.width || !base.image.height)
        return false;
    rgba = base.image.mips[0];
    width = base.image.width;
    height = base.image.height;
    return true;
}

Texture2DResidency *effectWhiteTexture(D3D11Backend &backend)
{
    return WhiteTexture(backend);
}

bool emitEffect(D3D11Backend &backend, const EffectBatch &b)
{
    State &s = S();
    if (!b.vertexCount || !b.indexCount || b.vertexCount > kChunkVertices || b.indexCount > kChunkIndices) return false;
    Chunk &chunk = ChunkWithRoom(backend, b.vertexCount, b.indexCount);
    const unsigned chunkIndex = static_cast<unsigned>(s.activeChunk);
    const unsigned base = static_cast<unsigned>(chunk.vertices.size());
    chunk.vertices.insert(chunk.vertices.end(), b.vertices, b.vertices + b.vertexCount);
    Batch out = {};
    out.chunk = chunkIndex;
    out.firstIndex = static_cast<unsigned>(chunk.indices.size());
    for (unsigned i = 0; i < b.indexCount; ++i)
        chunk.indices.push_back(static_cast<unsigned short>(base + b.indices[i]));
    out.indexCount = b.indexCount;
    out.texture = b.texture ? b.texture : WhiteTexture(backend);
    out.blend = b.blend;
    out.linear = true;
    out.mipLinear = true;
    out.clamp = b.clamp;
    out.depthWrite = b.depthWrite;
    out.doubleSided = b.doubleSided;
    out.noDiffuse = b.noDiffuse;
    out.alphaTest = b.alphaTest;
    out.distance = b.distance;
    out.translucent = b.layer == EffectLayerTranslucent || b.layer == EffectLayerParticle;
    out.layer = static_cast<unsigned char>(b.layer);
    out.stencilOnce = b.stencilOnce;
    out.depthAlways = b.depthAlways;
    out.waterKind = b.waterKind;
    out.waterSparkle = b.waterSparkle; out.waterNoise = b.waterNoise; out.waterEdge = b.waterEdge;
    out.waterNoiseOffset[0] = b.waterNoiseOffset[0]; out.waterNoiseOffset[1] = b.waterNoiseOffset[1];
    out.waterNoiseScale = b.waterNoiseScale;
    s.batches.push_back(out);
    s.stats.vertices += b.vertexCount;
    s.stats.triangles += b.indexCount / 3;
    ++s.stats.effectBatches;
    return true;
}

int renderObjects(D3D11Backend &backend, CameraClass &camera, const SceneLighting &lights,
                  const SceneLighting &infantryLights, const ObjectInput *objects, unsigned count)
{
    State &s = S();
    if (s.backend && s.backend != &backend) releaseAll();
    s.backend = &backend;

    ++s.stats.frames;
    s.batches.clear();
    for (Chunk &c : s.chunks) { c.vertices.clear(); c.indices.clear(); }   // geometry residencies are retained
    s.activeChunk = 0;

    Frame f = {};
    f.backend = &backend;
    f.camera = &camera;
    f.cameraPosition = camera.Get_Position();
    f.view = camera.Get_View_Matrix();
    f.lights = lights;
    f.infantryLights = infantryLights;

    unsigned drawnObjects = 0;
    for (unsigned i = 0; i < count; ++i) {
        const ObjectInput &input = objects[i];
        RenderObjClass *object = input.object;
        ++s.stats.objects;
        f.hexColor = input.hexColor;
        f.textureOverride = input.textureOverride;
        f.foliage = input.foliage;
        f.opacity = input.opacity;
        f.tint[0] = input.tint[0]; f.tint[1] = input.tint[1]; f.tint[2] = input.tint[2];
        f.infantry = input.infantry;
        f.shrouded = input.shrouded;
        const SphereClass &sphere = object->Get_Bounding_Sphere();
        if (camera.Cull_Sphere(sphere)) { ++s.stats.culled; object->Set_Visible(0); continue; }
        object->Set_Visible(1);
        const Vector3 delta = sphere.Center - f.cameraPosition;
        f.distance = delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z;
        if (!(f.distance - f.distance == 0.0f)) f.distance = 0.0f;   // non-finite bounds must not poison the sort
        f.drawnMeshes = 0;
        ProcessObject(f, object, s, 0);
        if (f.drawnMeshes) ++drawnObjects;
    }

    // Game-state-driven effects (particles, decals, shadows, water, shroud) join the same batch lists.
    NativeWorldEffects::collect(backend, camera);
    if (g_frameHook) g_frameHook(backend);

    // Upload each chunk once, then submit: opaque in order, translucent back to front.
    WorldSubmission base;
    FillBase(camera, base);
    for (Chunk &chunk : s.chunks) {
        if (chunk.vertices.empty() || chunk.indices.empty()) continue;
        backend.Update_Geometry_Vertices(*chunk.geometry, 0, chunk.vertices.data(),
                                         static_cast<unsigned>(chunk.vertices.size()));
        backend.Update_Geometry_Indices(*chunk.geometry, 0, chunk.indices.data(),
                                        static_cast<unsigned>(chunk.indices.size()));
    }
    auto submit = [&](const Batch &b) {
        WorldSubmission sub = base;
        sub.geometry = s.chunks[b.chunk].geometry.get();
        sub.first_index = b.firstIndex;
        sub.index_count = b.indexCount;
        sub.texture = b.texture;
        sub.linear = b.linear;
        sub.mip_linear = b.mipLinear;
        sub.clamp = b.clamp;
        sub.depth_write = b.depthWrite;
        sub.alpha_test = b.alphaTest;
        sub.double_sided = b.doubleSided;
        sub.no_diffuse = b.noDiffuse;
        sub.blend = b.blend;
        sub.stencil_once = b.stencilOnce;
        sub.depth_always = b.depthAlways;
        if (b.waterKind) {
            sub.material = WorldMaterial::Water;
            sub.water_river = b.waterKind == 2;
            sub.cloud = b.waterSparkle; sub.noise = b.waterNoise; sub.aux = b.waterEdge;
            sub.cloud_offset[0] = b.waterNoiseOffset[0]; sub.cloud_offset[1] = b.waterNoiseOffset[1];
            sub.texgen_scale = b.waterNoiseScale;
        }
        backend.Submit_World(sub);
    };
    int submissions = 0;
    unsigned translucent = 0;
    // Draw order: ground overlays (bibs, waypoint lines), opaque objects (registration order), terrain decals
    // (emission order), translucent objects/water back to front, particles back to front, overlays (shroud).
    // X64.29 review: the bound was left at LayerOverlay after the layers were renumbered, so the particle layer
    // (and every effect after it) was emitted but never submitted. Iterate by count, never by name.
    for (unsigned order = 0; order < LayerCount; ++order) {
        const unsigned layer = kLayerOrder[order];
        std::vector<const Batch *> list;
        for (const Batch &b : s.batches) {
            const unsigned bl = b.layer;
            if (bl == layer) list.push_back(&b);
        }
        if (layer == LayerTranslucent || layer == LayerParticle)
            std::stable_sort(list.begin(), list.end(), [](const Batch *a, const Batch *b) { return a->distance > b->distance; });
        for (const Batch *b : list) {
            submit(*b); ++submissions;
            ++s.stats.layerDraws[layer];
            if (layer != LayerOpaque) ++translucent;
        }
    }

    s.stats.batches += static_cast<unsigned>(submissions);
    s.stats.translucentBatches += translucent;
    s.stats.objectsDrawn += drawnObjects;
    s.stats.lastObjectsDrawn = drawnObjects;
    s.stats.lastBatches = static_cast<unsigned>(submissions);
    unsigned tris = 0;
    for (const Batch &b : s.batches) tris += b.indexCount / 3;
    s.stats.lastTriangles = tris;
    return submissions;
}

void addFrameObject(RenderObjClass *object)
{
    if (object && g_frameObjects.size() < 1024) g_frameObjects.push_back(object);
}

int render(D3D11Backend &backend, CameraClass &camera)
{
    State &s = S();
    std::vector<RenderObjClass *> frameObjects;
    frameObjects.swap(g_frameObjects);   // valid for this frame only
    if (s.entries.empty()) return 0;
    std::vector<ObjectInput> inputs;
    inputs.reserve(s.entries.size() + frameObjects.size());
    for (const Entry &entry : s.entries) {
        RenderObjClass *object = entry.object;
        if (object->Is_Hidden()) { ++s.stats.hidden; object->Set_Visible(0); continue; }
        ObjectInput input = {};
        input.object = object;
        input.hexColor = entry.hexColor;
        input.textureOverride = entry.textureOverride.empty() ? nullptr : entry.textureOverride.c_str();
        input.foliage = entry.foliage;
        input.opacity = 1.0f;
        DrawableInfo *info = static_cast<DrawableInfo *>(object->Get_User_Data());
        // RTS3DScene::renderOneObject: a drawable info without a drawable is a ghost (a fogged remnant), which
        // is always fogged; live objects use the partition shroud status with the 2 second grace after a
        // clear sighting (aircraft that pop out of the shroud). Anything past CLEAR is drawn through the shroud.
        if (info && !info->m_drawable) input.shrouded = true;
        // Trees and props (no drawable) are drawn through the shroud by the legacy tree/prop buffers (the shroud
        // texture in the tree shader, the shroud material pass for props). The ground-level overlay cannot
        // reach them, exactly like the bridges, so they take the per-vertex shroud level at their position
        // (clear cells multiply by 1: nothing changes where the player can see).
        if (!info) input.shrouded = true;
        if (info && info->m_drawable) {
            Drawable *draw = info->m_drawable;
            if (draw->isDrawableEffectivelyHidden() || draw->getFullyObscuredByShroud()) {
                ++s.stats.hidden; object->Set_Visible(0); continue;
            }
            if (const Object *owner = draw->getObject()) {
                Int local = 0;
                if (ThePlayerList && ThePlayerList->getLocalPlayer()) local = ThePlayerList->getLocalPlayer()->getPlayerIndex();
                ObjectShroudStatus ss = owner->getShroudedStatus(local);
                if (ss == OBJECTSHROUD_CLEAR) {
                    draw->setShroudClearFrame(TheGameLogic ? TheGameLogic->getFrame() : 0);
                } else if (ss >= OBJECTSHROUD_FOGGED && draw->getShroudClearFrame() != InvalidShroudClearFrame && TheGameLogic) {
                    UnsignedInt limit = 2 * LOGICFRAMES_PER_SECOND;
                    if (owner->isEffectivelyDead()) limit += 3 * LOGICFRAMES_PER_SECOND;
                    if (TheGameLogic->getFrame() < limit + draw->getShroudClearFrame()) ss = OBJECTSHROUD_PARTIAL_CLEAR;
                }
                if (TheGlobalData && !TheGlobalData->m_shroudOn) ss = OBJECTSHROUD_CLEAR;
                input.shrouded = ss > OBJECTSHROUD_CLEAR;
            }
            input.opacity = draw->getEffectiveOpacity();
            input.infantry = draw->isKindOf(KINDOF_INFANTRY);
            if (const Vector3 *tint = draw->getTintColor()) {
                input.tint[0] += tint->X; input.tint[1] += tint->Y; input.tint[2] += tint->Z;
            }
            if (const Vector3 *sel = draw->getSelectionColor()) {
                input.tint[0] += sel->X; input.tint[1] += sel->Y; input.tint[2] += sel->Z;
            }
        }
        // else: a ghost object (fogged remnant). It is drawn like the building it remembers; the shroud
        // overlay dims it with the surrounding fog.
        inputs.push_back(input);
    }
    for (RenderObjClass *object : frameObjects) {
        if (object->Is_Hidden()) continue;
        ObjectInput input = {};
        input.object = object;
        input.opacity = 1.0f;
        inputs.push_back(input);
    }
    SceneLighting lights, infantry;
    BuildLights(lights, false);
    BuildLights(infantry, true);
    return renderObjects(backend, camera, lights, infantry, inputs.data(), static_cast<unsigned>(inputs.size()));
}

void setFrameEffectHookForTests(std::function<void(D3D11Backend &)> hook)
{
    g_frameHook = std::move(hook);
}

void setRenderTargetSizeForTests(unsigned width, unsigned height)
{
    g_testTargetSize[0] = width;
    g_testTargetSize[1] = height;
}

void setFileReaderForTests(FileReader reader)
{
    g_testReader = std::move(reader);
    S().textures.clear();
}

Stats stats()
{
    return S().stats;
}

void resetStatsForTests()
{
    S().stats = Stats{};
    S().failedCount = 0;
}

unsigned failedTextureCount()
{
    return S().failedCount;
}

const char *failedTexture(unsigned index)
{
    State &s = S();
    return index < kFailedNames && index < s.failedCount ? s.failed[index].c_str() : "";
}


namespace {
void SetEntryState(RenderObjClass *object, const char *texture, bool foliage)
{
    State &s = S();
    auto found = s.index.find(object);
    if (found == s.index.end()) return;
    Entry &entry = s.entries[found->second];
    entry.textureOverride = texture ? texture : "";
    entry.foliage = foliage;
}
}

void addScenery(int kind, int id, const char *model, const char *texture, const Coord3D &location,
                float scale, float angle)
{
    if (!active() || !model || !*model) return;
    State &s = S();
    removeScenery(kind, id);
    if (!W3DDisplay::m_assetManager) return;
    RenderObjClass *object = W3DDisplay::m_assetManager->Create_Render_Obj(model);
    if (!object) return;
    Matrix3D tm(true);
    tm.Rotate_Z(angle);
    tm.Scale(scale);
    tm.Set_Translation(Vector3(location.x, location.y, location.z));
    object->Set_Transform(tm);
    object->Set_ObjectScale(scale);
    object->Set_Collision_Type(0);          // scenery never blocks or answers unit picking rays
    add(object, 0);
    SetEntryState(object, texture && *texture ? texture : nullptr, kind == SceneryTree);
    const SphereClass &sphere = object->Get_Bounding_Sphere();
    s.scenery[{kind, id}] = Scenery{object, location.x, location.y, location.z, sphere.Radius, kind};
    object->Release_Ref();                  // the registry holds the reference
}

void removeScenery(int kind, int id)
{
    State &s = S();
    auto found = s.scenery.find({kind, id});
    if (found == s.scenery.end()) return;
    remove(found->second.object);
    s.scenery.erase(found);
}

bool updateScenery(int kind, int id, const Coord3D &location, float angle)
{
    State &s = S();
    auto found = s.scenery.find({kind, id});
    if (found == s.scenery.end()) return false;
    Matrix3D tm(true);
    tm.Rotate_Z(angle);
    tm.Scale(found->second.object->Get_ObjectScale());
    tm.Set_Translation(Vector3(location.x, location.y, location.z));
    found->second.object->Set_Transform(tm);
    found->second.x = location.x; found->second.y = location.y; found->second.z = location.z;
    return true;
}

void removeAllScenery(int kind)
{
    State &s = S();
    for (auto it = s.scenery.begin(); it != s.scenery.end();) {
        if (it->second.kind == kind) { remove(it->second.object); it = s.scenery.erase(it); }
        else ++it;
    }
}

void removeSceneryForConstruction(const Coord3D *position, const GeometryInfo &geometry, float angle)
{
    // Same collision rule as W3DTreeBuffer/W3DPropBuffer::remove*ForConstruction.
    State &s = S();
    for (auto it = s.scenery.begin(); it != s.scenery.end();) {
        Coord3D at;
        at.set(it->second.x, it->second.y, it->second.z);
        const float radius = it->second.kind == SceneryTree ? 7.0f : it->second.radius;
        GeometryInfo info(GEOMETRY_CYLINDER, false, 5 * radius, 2 * radius, 2 * radius);
        if (ThePartitionManager && ThePartitionManager->geomCollidesWithGeom(position, geometry, angle, &at, info, 0.0f)) {
            remove(it->second.object);
            it = s.scenery.erase(it);
        } else {
            ++it;
        }
    }
}

unsigned sceneryCount()
{
    return static_cast<unsigned>(S().scenery.size());
}

void releaseAll()
{
    NativeWorldEffects::releaseAll();
    State &s = S();
    for (const Entry &e : s.entries) e.object->Release_Ref();
    s.entries.clear();
    s.index.clear();
    s.scenery.clear();
    s.batches.clear();
    s.chunks.clear();
    s.textures.clear();
    s.white.reset();
    s.backend = nullptr;
}
} // namespace NativeWorldObjects

#endif
