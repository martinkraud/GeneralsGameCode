// SPDX-License-Identifier: GPL-3.0-or-later
// Native (AMD64/D3D11) world-object renderer. Owns the registry of render objects that the legacy
// code would have added to the 3D scene (W3DModelDraw & friends) and draws their W3D meshes with
// the TerrainOverlay backend material: CPU skinning, CPU vertex lighting equivalent to the DX8
// fixed-function path, shader-state mapping, team-colored textures. No DX8 object is touched.
#pragma once

class RenderObjClass;

#if defined(_WIN64)
#include <functional>
#include <string>
#include <vector>

class CameraClass;
class D3D11Backend;
class Texture2DResidency;
struct WorldVertexXYZDUV2;
enum class WorldBlend : unsigned char;
class RayCollisionTestClass;
struct Coord3D;
class GeometryInfo;

namespace NativeWorldObjects
{
// True only under the ordinary-terrain profile; otherwise add/remove are no-ops (headless
// profiles keep their previous behavior of not tracking render objects at all).
bool active();
// Test hook: makes the registry accept objects without the native profile (offline tests).
void setActiveForTests(bool active);
// `hexColor` is the legacy house color passed to Create_Render_Obj (0 = none); the renderer applies
// it to ZHC team-color textures and HOUSECOLOR vertex materials exactly as the legacy recolor did.
void add(RenderObjClass *object, int hexColor = 0);
void remove(RenderObjClass *object);
// Team color of a registered (or inherited) object; `inheritColor` lets a snapshot clone keep it.
int colorOf(const RenderObjClass *object);
void inheritColor(const RenderObjClass *to, const RenderObjClass *from);
bool contains(const RenderObjClass *object);
unsigned registered();
// Ray test against the registered objects, the native counterpart of RTS3DScene::castRay: only
// objects whose collision type matches `collisionType` and that were visible in the last pass
// (or all when `testAll`) are tested; on a hit `raytest.CollidedRenderObj` and the clipped ray are set.
bool castRay(RayCollisionTestClass &raytest, bool testAll, int collisionType);

struct Stats
{
    unsigned frames;           // frames in which the pass ran
    unsigned objects;          // registered objects visited (cumulative)
    unsigned objectsDrawn;     // objects with at least one drawn mesh
    unsigned hidden;           // skipped: drawable hidden / fully obscured
    unsigned culled;           // skipped: outside the view frustum
    unsigned meshes;           // meshes processed
    unsigned meshesDrawn;      // meshes that produced at least one batch
    unsigned skinned;          // skinned meshes deformed
    unsigned batches;          // draw submissions
    unsigned translucentBatches;
    unsigned vertices;         // vertices uploaded (cumulative)
    unsigned triangles;        // triangles drawn (cumulative)
    unsigned textureLoads;     // distinct decoded textures
    unsigned textureFailures;  // textures that could not be decoded (white fallback)
    unsigned teamColored;      // team-color recolors applied
    unsigned effectBatches;    // batches emitted by the effect producers (particles, lines, decals, water, shroud)
    unsigned layerDraws[8];    // draw submissions per layer: opaque, decal, translucent, shroud overlay, particles
    unsigned unsupportedClass; // render objects of classes not drawn by this pass
    unsigned unsupportedState; // shader/material states approximated or ignored (sum of the families below)
    // Family breakdown of unsupportedState (X64.29): measured per drawn mesh/batch, never silently dropped.
    unsigned stateMultiPass;       // informational: meshes with more than one material pass (all passes are drawn)
    unsigned stateMapper;          // materials whose mapper kind has no native evaluation (unsupported)
    unsigned mapperApplied;        // material blocks whose mapper was evaluated natively
    unsigned stateDualTexture;     // passes with a second texture stage (not combined natively)
    unsigned stateBlend;           // blend pairs outside the supported WorldBlend set
    unsigned stateModulate2x;      // GRADIENT_MODULATE2X batches
    unsigned stateSecondary;       // secondary (specular) gradient batches
    unsigned stateDetail;          // post-detail color functions
    unsigned mapperKinds[24];      // histogram by TextureMapperClass::Mapper_ID()
    unsigned chunkOverflow;    // meshes dropped because a chunk could not hold them
    unsigned lastObjectsDrawn; // objects drawn in the most recent frame
    unsigned lastBatches;      // batches in the most recent frame
    unsigned lastTriangles;    // triangles in the most recent frame
};

// Directional-light environment of the legacy scene, reduced to what the fixed-function vertex
// lighting needs. `direction` points from the surface toward the light.
struct SceneLighting
{
    float ambient[3];
    unsigned count;
    float direction[3][3];
    float diffuse[3][3];
};

// One drawable-level instance: the render object plus the per-drawable state the legacy scene applied.
struct ObjectInput
{
    RenderObjClass *object;
    int hexColor;       // legacy house color (0 = none)
    float opacity;      // Drawable::getEffectiveOpacity (1 = opaque)
    float tint[3];      // summed FX tint + selection color added to ambient
    bool infantry;      // uses the infantry light scale set
    bool shrouded;      // fogged/shrouded/partially clear object or fogged ghost: drawn through the shroud
    const char *textureOverride;   // replaces every polygon texture (tree atlas textures), or null
    bool foliage;       // alpha-tested, double-sided, depth-writing (the tree buffer shader state)
};

// Static scenery that the legacy tree/prop buffers draw from their own CPU data. Under the play profile
// those buffers do not exist; the same placements become native render objects (no sway/topple).
enum SceneryKind { SceneryTree = 0, SceneryProp = 1 };
void addScenery(int kind, int id, const char *model, const char *texture, const Coord3D &location,
                float scale, float angle);
void removeScenery(int kind, int id);
bool updateScenery(int kind, int id, const Coord3D &location, float angle);
void removeAllScenery(int kind);
void removeSceneryForConstruction(const Coord3D *position, const GeometryInfo &geometry, float angle);
unsigned sceneryCount();

// ---- Services for the native effect producers (NativeWorldEffects: particles, lines, decals, water,
// shroud). They emit already-lit world-space triangles into the same per-frame batch lists the object
// renderer uses; layers fix the draw order (opaque objects, terrain decals, sorted translucents, particles,
// overlays). Valid only inside the effect callbacks of a frame.
enum EffectLayer { EffectLayerDecal = 1, EffectLayerTranslucent = 2, EffectLayerOverlay = 3, EffectLayerParticle = 4 };
struct EffectBatch
{
    const WorldVertexXYZDUV2 *vertices;
    unsigned vertexCount;
    const unsigned short *indices;
    unsigned indexCount;
    Texture2DResidency *texture;   // null = the white texture
    WorldBlend blend;
    bool depthWrite;
    bool doubleSided;
    bool noDiffuse;
    bool clamp;
    bool stencilOnce;              // blend each covered pixel at most once (planar shadows)
    float alphaTest;               // 0 = none, else 0..1 reference
    float distance;                // squared camera distance for back-to-front sorting
    int layer;                     // EffectLayer
};
// Shader presets/states of the legacy renderer reduced to the native batch fields.
struct EffectState { WorldBlend blend; bool depthWrite, doubleSided, noDiffuse; float alphaTest; bool blendKnown; };
EffectState EffectStateFromShader(unsigned shaderBits, bool twoSided);
Texture2DResidency *effectTexture(D3D11Backend &backend, const char *name, int hexColor = 0);
Texture2DResidency *effectWhiteTexture(D3D11Backend &backend);
// Decodes a texture's top level to RGBA8 on the CPU (water edge profile, diagnostics).
bool effectImage(const char *name, std::vector<unsigned char> &rgba, unsigned &width, unsigned &height);
bool emitEffect(D3D11Backend &backend, const EffectBatch &batch);   // false when the chunk pool is full

// Draws every registered object visible to the camera on top of the terrain pass (same camera,
// viewport and depth buffer). Returns the number of draw submissions issued.
int render(D3D11Backend &backend, CameraClass &camera);
// Game-independent core (also used by the offline model-render test).
int renderObjects(D3D11Backend &backend, CameraClass &camera, const SceneLighting &lights,
                  const SceneLighting &infantryLights, const ObjectInput *objects, unsigned count);
// Test hook: replaces the engine file factory as the texture byte source (empty = engine factory).
using FileReader = std::function<bool(const std::string &path, std::vector<unsigned char> &bytes)>;
void setFileReaderForTests(FileReader reader);
// Test hook: called once per frame after the effect producers, before submission; may call emitEffect.
void setFrameEffectHookForTests(std::function<void(D3D11Backend &)> hook);
// Test hook: render-target size used for the viewport instead of WW3D's (0 = use WW3D).
void setRenderTargetSizeForTests(unsigned width, unsigned height);
Stats stats();
void resetStatsForTests();   // test hook: counters are cumulative per process
// Bounded names of the first textures/classes that were not drawn faithfully (diagnostics).
unsigned failedTextureCount();
const char *failedTexture(unsigned index);
// Releases all residencies and registry references (must precede backend/asset manager release).
void releaseAll();
}
#else
namespace NativeWorldObjects
{
inline bool active() { return false; }
inline void add(RenderObjClass *, int = 0) {}
inline void remove(RenderObjClass *) {}
inline bool contains(const RenderObjClass *) { return false; }
inline int colorOf(const RenderObjClass *) { return 0; }
inline void inheritColor(const RenderObjClass *, const RenderObjClass *) {}
}
#endif
