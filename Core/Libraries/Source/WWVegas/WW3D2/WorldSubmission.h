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
// TerrainCombined is the single-pass retail form (shaders\terrain.pso / terrainnoise.pso /
// terrainnoise2.pso, ps_1_1):
//   rgb = lerp(tex(uv0), tex(uv1), diffuse.a) * diffuse.rgb [* cloud] [* noise]
// cloud/noise are world-space XY texture-generated (legacy D3DTSS_TCI_CAMERASPACEPOSITION through
// inverse(view) == the vertex position transformed by the world matrix), scaled by
// `texgen_scale`; cloud additionally scrolls by `cloud_offset`. Both wrap. Cloud samples
// bilinear; noise samples min=point, mag=linear (the legacy stage states). Opaque.
//
// TerrainOverlay is the retail road/extra-blend/bridge/shoreline family (ST_ROAD_BASE*, roadnoise2.pso,
// detailAlphaShader, the destination-alpha shoreline pass), always as one draw:
//   rgba = tex(uv0) * [diffuse] * [cloud] * [noise]         (all four channels, as roadnoise2.pso)
// then an optional alpha test (clip alpha < alpha_test), source-alpha blend over the target
// (no blend in `alpha_only` mode, which writes only the destination alpha channel).
enum class WorldMaterial { TerrainBase, TerrainBlend, TerrainCombined, TerrainOverlay };

// Blend equations of the TerrainOverlay family (legacy ShaderClass src/dst blend pairs seen on
// roads, bridges and W3D meshes). SrcAlpha is the zero-initialized default.
enum class WorldBlend : unsigned char
{
    SrcAlpha,         // SRC_ALPHA, ONE_MINUS_SRC_ALPHA
    Opaque,           // ONE, ZERO (blending disabled)
    Additive,         // ONE, ONE
    AdditiveAlpha,    // SRC_ALPHA, ONE
    Multiply,         // ZERO, SRC_COLOR
    InvMultiply,      // ZERO, ONE_MINUS_SRC_COLOR
    Premultiplied,    // ONE, ONE_MINUS_SRC_ALPHA
    DestAlpha,        // DEST_ALPHA, INV_DEST_ALPHA (soft water edge from the destination alpha)
    DestAlphaAdditive,// DEST_ALPHA, ONE
    Count
};

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
    // TerrainCombined only (ignored by the other materials; null = factor 1).
    Texture2DResidency *cloud;
    Texture2DResidency *noise;
    float cloud_offset[2];
    float texgen_scale;
    // TerrainOverlay only (zero-initialized defaults reproduce the road/extra-blend state).
    float alpha_test;               // > 0: discard when result alpha < alpha_test (0..1)
    bool double_sided;              // cull mode none (bridges)
    bool alpha_only;                // write destination alpha only, no blending (shoreline)
    bool no_diffuse;                // ignore vertex diffuse (GRADIENT_DISABLE presets)
    bool stencil_once;              // TerrainOverlay: depth test only, each pixel passes once per stencil clear
    WorldBlend blend;               // blend equation (ignored when alpha_only)
};

inline void World_Identity(float (&m)[16])
{
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

// Legacy Westwood matrices are row-major with column-vector semantics (v' = M * v).
// The world contract is Direct3D row-vector (v' = v * M). These are the exact
// transposes legacy To_D3DMATRIX produces, kept SDK-free so they are unit-testable.
// Matrix3D: 3 rows x 4 columns (rotation + translation column), implicit last row 0,0,0,1.
inline void World_From_Matrix3D(const float *rows3x4, float (&out)[16])
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c) out[c * 4 + r] = rows3x4[r * 4 + c];
    out[3] = out[7] = out[11] = 0.0f; out[15] = 1.0f;
}
// Matrix4x4: 4 rows x 4 columns.
inline void World_From_Matrix4x4(const float *rows4x4, float (&out)[16])
{
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out[c * 4 + r] = rows4x4[r * 4 + c];
}
