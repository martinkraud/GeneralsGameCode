// SPDX-License-Identifier: GPL-3.0-or-later
// Backend-neutral world (3D) geometry and draw contract. No graphics SDK
// declarations belong here. Purpose-built per subsystem: each WorldMaterial is
// one named, tested combiner, not a generic fixed-function state interpreter.
//
// Fidelity target (developer decision): equivalent modern shaders validated with
// tolerance-based golden images against CPU references of the legacy stage math.
#pragma once

#include "IRenderBackend.h"
#include <vector>

class Texture2DResidency;

// Legacy VertexFormatXYZDUV2: position, packed ARGB diffuse (D3DCOLOR), two UV sets.
struct WorldVertexXYZDUV2
{
    float x, y, z;
    unsigned diffuse;
    float u0, v0, u1, v1;
};
static_assert(sizeof(WorldVertexXYZDUV2) == 32, "legacy XYZDUV2 vertex layout");

// CPU-side geometry content. The renderer-neutral owner of vertex/index data;
// residency is a separate backend-owned object (same split as Texture2D).
class GeometryCPUContent
{
public:
    std::vector<WorldVertexXYZDUV2> vertices;
    std::vector<unsigned short> indices;
    bool Valid() const
    {
        if (vertices.empty() || indices.empty() || vertices.size() > 65535 || indices.size() % 3) return false;
        for (unsigned short i : indices) if (i >= vertices.size()) return false;
        return true;
    }
};

class GeometryResidency
{
public:
    virtual ~GeometryResidency() {}
    virtual unsigned Vertex_Count() const = 0;
    virtual unsigned Index_Count() const = 0;
};

// Terrain combiner (legacy ST_TERRAIN_BASE two-pass fixed-function path):
//   TerrainBase  pass 0: rgb = tex(uv0).rgb * diffuse.rgb, opaque (no blend)
//   TerrainBlend pass 1: rgb = tex(uv1).rgb * diffuse.rgb, a = tex.a * diffuse.a,
//                        SRCALPHA / INVSRCALPHA blend
// Diffuse: lighting is baked into the vertex colour (prelit material).
enum class WorldMaterial { TerrainBase, TerrainBlend };

struct WorldSubmission
{
    GeometryResidency *geometry;
    unsigned first_index;
    unsigned index_count;           // multiple of 3
    // Direct3D row-vector convention (v * World * View * Projection), z in [0,1].
    float world[16];
    float view[16];
    float projection[16];
    Texture2DResidency *texture;    // required by the terrain materials
    // Explicit sampler intent (the legacy terrain shader set these per pass).
    bool linear;                    // min/mag: linear, else point
    bool mip_linear;                // mip: linear, else point
    bool clamp;                     // address: clamp, else wrap
    WorldMaterial material;
    bool depth_write;               // depth test is LESS_EQUAL; write optional
    RenderBackendViewport viewport;
};

inline void World_Identity(float (&m)[16])
{
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}
