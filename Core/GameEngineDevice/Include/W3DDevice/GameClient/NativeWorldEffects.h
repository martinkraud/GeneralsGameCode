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
// Shroud overlay over the cell rectangle [x0,x1) x [y0,y1); `ground` supplies the surface height.
bool shroud(const unsigned char *levels, int cellsX, int cellsY, float cell, unsigned char border, bool filter,
            int x0, int y0, int x1, int y1, int sub, float (*ground)(float, float), const float color[3], Mesh &out);
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
    unsigned waterPatches;        // water areas emitted
    unsigned shroudCells;         // shroud overlay cells emitted
    unsigned scorches;            // scorch marks emitted
    unsigned dropped;             // emissions refused by the chunk pool
};
Stats stats();
void resetStatsForTests();

// Line render objects registered with NativeWorldObjects (called from the object pass).
bool emitLine3D(D3D11Backend &backend, Line3DClass &line, float distanceSquared);
bool emitSegLine(D3D11Backend &backend, SegmentedLineClass &line, const Vector3 &cameraPosition, float distanceSquared);

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
inline void addScorch(float, float, float, float, int, bool) {}
inline void clearScorches() {}
}
#endif
