// SPDX-License-Identifier: GPL-3.0-or-later
// Native (AMD64/D3D11) producers of the world effects that the legacy scene drew through DX8 vertex
// buffers: line render objects (tracers, ropes, lasers, projectile streams), particle systems, terrain
// decals and planar shadows, water and the shroud overlay. Every producer emits already-lit world-space
// triangles into the NativeWorldObjects per-frame batch lists (see EffectBatch there); nothing touches DX8.
#pragma once

#include "Common/GameCommon.h"
#include "Common/GameType.h"
#include "GameClient/Color.h"
#include "GameClient/Shadow.h"

#if defined(_WIN64)
class CameraClass;
class D3D11Backend;
class RenderObjClass;
class Line3DClass;
class SegmentedLineClass;
class Vector3;

#include <WW3D2/WorldSubmission.h>
#include <vector>

namespace NativeWorldEffects
{
// Game-state independent geometry builders (world space, legacy-equivalent; unit-tested offline).
namespace Geometry
{
struct Mesh
{
    std::vector<WorldVertexXYZDUV2> vertices;
    std::vector<unsigned short> indices;
};
// Flat standing-water patch (drawTrapezoidWater without feathering); corners ordered as in renderWater.
void standingWater(const float corners[4][3], float scrollPhase, unsigned diffuse, Mesh &out);
// River strip (drawRiverWater) from the polygon points (x,y,z triples); `edge` is the alpha profile across
// the width (empty = opaque). False when the polygon is not a valid river.
bool river(const float *points, int pointCount, int riverStart, float scrollPhase, unsigned diffuse,
           const std::vector<float> &edge, Mesh &out);
// Shroud overlay over the terrain lattice rectangle [lx0,lx1) x [ly0,ly1) (10 world units per lattice cell,
// origin at the playable area): the terrain's own heights (`height`) and cell diagonals (`flip`), so the overlay
// lies in the terrain surface. `levels` are the cellsX x cellsY shroud cells of `cell` world units.
bool shroud(const unsigned char *levels, int cellsX, int cellsY, float cell, unsigned char border, bool filter,
            int lx0, int ly0, int lx1, int ly1, float (*height)(int, int), bool (*flip)(int, int),
            const float color[3], Mesh &out);
// Packed ARGB texel of a shroud level (level * shroud color, fully lit at 255).
unsigned ShroudTexel(float level, const float color[3]);
// One camera-facing particle quad (PointGroupClass QUADS layout).
void billboard(const float center[3], const float right[3], const float up[3], float size, float angle,
               unsigned color, Mesh &out);
}

struct Stats
{
    unsigned lineBoxes;           // Line3D boxes emitted
    unsigned segLines;            // segmented lines emitted
    unsigned segQuads;            // ribbon quads emitted for segmented lines
    unsigned particleSystems;     // particle systems with at least one drawn particle
    unsigned particlesDrawn;      // particle quads emitted
    unsigned particleBatches;     // particle batches emitted
    unsigned streaks;             // streak ribbons emitted
    unsigned volumeLayers;        // extra volume-particle layers emitted
    unsigned particlesCulled;     // particles skipped by the frustum test
    unsigned smudgeSystems;       // smudge (heat distortion) systems skipped: no native form yet
    unsigned decals;              // terrain decals emitted
    unsigned shadowDecals;        // object decal shadows emitted
    unsigned volumeShadows;       // planar projected shadow silhouettes emitted
    unsigned shadowsSeen;         // registered shadows considered
    unsigned shadowNoGeometry;    // volume shadows whose object had no shadow-casting geometry
    unsigned shadowCulled;        // shadows skipped: hidden or outside the view
    unsigned shadowCacheHits;     // planar volume shadows reused bit-exactly from the previous frame
    unsigned shadowCacheMisses;   // planar volume shadows recomputed (changed inputs / first sight)
    unsigned shadowUncached;      // volume shadows that keep the canonical path (structures: non-planar)
    unsigned waterPatches;        // water areas emitted
    unsigned shroudCells;         // shroud overlay cells emitted
    unsigned scorches;            // scorch marks emitted
    unsigned tracks;              // terrain track strips emitted (vehicle tread marks)
    unsigned bibs;                // building bibs emitted (normal + highlighted)
    unsigned skyBoxFrames;        // frames the sky box was handed to the object pass
    unsigned waterSparkles;       // standing-water patches drawn with the sparkle/noise pixel shader
    unsigned waterRivers;         // river strips drawn with the river pixel shader (alpha edge on the GPU)
    unsigned groundLines;         // waypoint / rally lines emitted on the ground layer
    unsigned dropped;             // emissions refused by the chunk pool
    // Particle pipeline (ParticleSystemManager::PipelineStats mirrored at render time):
    unsigned pipelineRequested;       // createParticleSystem calls (FX nuggets, weapons, damage states ...)
    unsigned pipelineNullTemplate;    // ... refused: template not found
    unsigned pipelineCreated;         // systems instantiated
    unsigned pipelineRemoved;         // systems that ended
    unsigned pipelineParticlesAdded;  // particles created
    unsigned pipelineUpdates;         // manager update calls (logic frames)
    unsigned pipelineDraws;           // render-rate integrations
    unsigned pipelinePeakSystems;
    unsigned pipelinePeakParticles;
    unsigned systemsAlive;            // right now
    unsigned particlesAlive;          // right now
    unsigned particlesAliveMax;
};
Stats stats();
void resetStatsForTests();

// Line render objects registered with NativeWorldObjects (called from the object pass).
bool emitLine3D(D3D11Backend &backend, Line3DClass &line, float distanceSquared);
bool emitSegLine(D3D11Backend &backend, SegmentedLineClass &line, const Vector3 &cameraPosition, float distanceSquared);
// Terrain-pass overlays of the legacy renderer (drawn with the terrain, before objects, PASS_ALWAYS): the
// waypoint/rally lines are snapshotted here while the waypoint buffer runs inside the terrain pass and emitted
// on the ground layer by the next collect(). `line` is copied; it may be reused by the caller immediately.
void queueGroundLine(SegmentedLineClass &line);

// Game-state-driven producers, called once per frame after the objects were processed.
void collect(D3D11Backend &backend, CameraClass &camera);
void releaseAll();

// Native stand-in for the legacy shadow manager (decals, planar shadows). install() makes
// TheProjectedShadowManager non-null; uninstall() (also done by releaseAll) removes it again.
void install();
void uninstall();
Shadow *addShadow(RenderObjClass *object, Shadow::ShadowTypeInfo *info);
void setShadowColor(unsigned argb);

// W3DDisplay::setShroudLevel et al. feed the CPU shroud used by the overlay.
void setShroudLevel(int x, int y, unsigned char level);
void setBorderShroudLevel(unsigned char level);
void setShroudFilter(bool enable);
// Multiplier (r,g,b in 0..1) the shroud applies at a world position (objects that are fogged or shrouded are
// drawn through the shroud like the legacy shroud material pass). 1,1,1 when no shroud is active.
void shroudMultiplierAt(float x, float y, float rgb[3]);
// Sky box (WaterRenderObjClass::m_skyBox, GameData DrawSkyBox / SkyBoxPositionZ / SkyBoxScale): the legacy water
// render object owns it and does not exist in the play profile. The same "new_skybox" model is created lazily,
// centered on the camera each frame, and handed to the object pass as a frame object (visible beyond the map
// edge only, like the original). `replaceSkyboxTexture` mirrors WaterRenderObjClass::replaceSkyboxTexture.
void addFrameObjects(CameraClass &camera);
void replaceSkyboxTexture(const char *oldTexture, const char *newTexture);
// World size of one logic shroud cell (the partition cell the logic sends levels for); the radar maps
// logic cells to radar pixels with it where the legacy code asks W3DShroud::getCellWidth().
float shroudCellWorldSize();
// Test hook: installs a shroud level grid without a loaded map (cleared by uninstall()/releaseAll()).
void setShroudForTests(int cellsX, int cellsY, float cellSize, const unsigned char *levels, unsigned char border);

// Scorch marks (W3DScorch equivalent): terrain-following decals from the 3x3 EXScorch01 atlas.
void addScorch(float x, float y, float z, float radius, int type, bool permanent);
void clearScorches();
}
#else
class RenderObjClass;
namespace NativeWorldEffects
{
inline void install() {}
inline void uninstall() {}
inline Shadow *addShadow(RenderObjClass *, Shadow::ShadowTypeInfo *) { return nullptr; }
inline void setShadowColor(unsigned) {}
inline void setShroudLevel(int, int, unsigned char) {}
inline void setBorderShroudLevel(unsigned char) {}
inline void setShroudFilter(bool) {}
inline void shroudMultiplierAt(float, float, float rgb[3]) { rgb[0] = rgb[1] = rgb[2] = 1.0f; }
inline float shroudCellWorldSize() { return 0.0f; }
inline void setShroudForTests(int, int, float, const unsigned char *, unsigned char) {}
inline void addScorch(float, float, float, float, int, bool) {}
inline void clearScorches() {}
}
#endif
