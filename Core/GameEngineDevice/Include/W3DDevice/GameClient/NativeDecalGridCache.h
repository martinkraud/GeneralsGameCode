// SPDX-License-Identifier: GPL-3.0-or-later
// X64.35 optimization: exact frame-to-frame reuse of terrain-conforming decal-shadow grids.
//
// EmitDecal rebuilt, for every decal shadow and every frame, a grid of up to 33 x 33 vertices with three terrain height
// lookups per vertex (C-D1: 0.87 ms/frame, 53 decals). The grid depends only on the decal centre, yaw, size, colour, the
// grid dimensions and the terrain height field. When every one of those inputs - including the identity and revision of
// the height map the lookups read - is bit-identical to the previous frame, the grid is bit-identical too and is reused.
// Anything else (or any uncertainty) recomputes through Build(), the single canonical implementation.
//
// Render-only: nothing here reads or writes simulation state. The terrain stamp is supplied by the caller.
#pragma once
#include <WW3D2/WorldSubmission.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NativeDecalGrid
{
struct Params
{
    float cx = 0.0f, cy = 0.0f, yaw = 0.0f;   // decal centre (world XY) and rotation
    float sx = 0.0f, sy = 0.0f;               // decal size
    int nu = 0, nv = 0;                       // grid dimensions (see Dimensions)
    unsigned color = 0;                       // vertex diffuse
    unsigned terrainSerial = 0;               // identity of the height map the heights come from (0 = unknown)
    unsigned terrainRevision = 0;             // incremented on every height change of that map
};

// Grid dimensions exactly as EmitDecal derived them: 2.5 unit steps, 2..33 per axis.
inline void Dimensions(float sx, float sy, int &nu, int &nv)
{
    nu = static_cast<int>(std::ceil(sx / 2.5f)) + 1;
    nv = static_cast<int>(std::ceil(sy / 2.5f)) + 1;
    nu = std::max(2, std::min(nu, 33));
    nv = std::max(2, std::min(nv, 33));
}

// The canonical grid generation (the former inline body of EmitDecal, unchanged arithmetic and order).
// `height(x, y)` is the terrain height lookup (TerrainLogic::getGroundHeight in the game).
template <class HeightFn>
inline void Build(const Params &p, HeightFn &&height, std::vector<WorldVertexXYZDUV2> &vertices,
                  std::vector<unsigned short> &indices)
{
    const int nu = p.nu, nv = p.nv;
    const float stepX = p.sx / static_cast<float>(nu - 1), stepY = p.sy / static_cast<float>(nv - 1);
    const float c = std::cos(p.yaw), s = std::sin(p.yaw);
    vertices.clear(); indices.clear();
    const unsigned color = p.color;
    for (int j = 0; j < nv; ++j) {
        for (int i = 0; i < nu; ++i) {
            const float fu = static_cast<float>(i) / static_cast<float>(nu - 1);
            const float fv = static_cast<float>(j) / static_cast<float>(nv - 1);
            const float lx = (fu - 0.5f) * p.sx, ly = (fv - 0.5f) * p.sy;
            WorldVertexXYZDUV2 v;
            v.x = p.cx + lx * c - ly * s;
            v.y = p.cy + lx * s + ly * c;
            const float h = height(v.x, v.y);
            const float gx = (height(v.x + 1.25f, v.y) - h) / 1.25f, gy = (height(v.x, v.y + 1.25f) - h) / 1.25f;
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
}

class Cache
{
public:
    struct Result
    {
        const WorldVertexXYZDUV2 *vertices = nullptr;
        unsigned vertexCount = 0;
        const unsigned short *indices = nullptr;
        unsigned indexCount = 0;
        bool hit = false;
    };
    struct Statistics
    {
        unsigned long long hits = 0, misses = 0, evictions = 0, uncacheable = 0;
        size_t bytes = 0, entries = 0;
    };

    explicit Cache(size_t budgetBytes = 8u * 1024u * 1024u) : m_budget(budgetBytes) {}

    // Grid for `p` from the entry of `owner` when every input is bit-identical, otherwise rebuilt with `height` (and the
    // entry replaced). A terrain stamp of 0 (unknown source) or a null owner is never cached. Returned pointers are valid
    // until the next get/forget/clear.
    template <class HeightFn>
    Result get(const void *owner, const Params &p, HeightFn &&height)
    {
        if (!owner || p.terrainSerial == 0 || p.nu < 2 || p.nv < 2) {
            ++m_stats.uncacheable;
            Build(p, height, m_scratch.vertices, m_scratch.indices);
            return fill(m_scratch, false);
        }
        Entry &e = m_entries[owner];
        if (e.valid && same(e.key, p)) {
            e.stamp = ++m_clock;
            ++m_stats.hits;
            return fill(e, true);
        }
        if (e.valid) m_stats.bytes -= e.bytes;
        e.key = p;
        Build(p, height, e.vertices, e.indices);
        e.bytes = e.vertices.size() * sizeof(WorldVertexXYZDUV2) + e.indices.size() * sizeof(unsigned short);
        e.valid = true;
        e.stamp = ++m_clock;
        m_stats.bytes += e.bytes;
        ++m_stats.misses;
        if (e.bytes > m_budget) {   // never retain an entry larger than the whole budget
            m_scratch.vertices = std::move(e.vertices);
            m_scratch.indices = std::move(e.indices);
            m_stats.bytes -= e.bytes;
            m_entries.erase(owner);
            return fill(m_scratch, false);
        }
        evictFor(owner);
        return fill(m_entries[owner], false);
    }

    void forget(const void *owner)
    {
        auto it = m_entries.find(owner);
        if (it == m_entries.end()) return;
        if (it->second.valid) m_stats.bytes -= it->second.bytes;
        m_entries.erase(it);
    }
    void clear() { m_entries.clear(); m_stats.bytes = 0; }
    void setBudget(size_t bytes) { m_budget = bytes; }
    size_t budget() const { return m_budget; }
    Statistics statistics() const { Statistics s = m_stats; s.entries = m_entries.size(); return s; }
    bool contains(const void *owner) const { return m_entries.find(owner) != m_entries.end(); }

private:
    struct Entry
    {
        bool valid = false;
        Params key;
        std::vector<WorldVertexXYZDUV2> vertices;
        std::vector<unsigned short> indices;
        size_t bytes = 0;
        unsigned long long stamp = 0;
    };

    static bool bits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }
    static bool same(const Params &a, const Params &b)
    {
        return bits(a.cx, b.cx) && bits(a.cy, b.cy) && bits(a.yaw, b.yaw) && bits(a.sx, b.sx) && bits(a.sy, b.sy) &&
               a.nu == b.nu && a.nv == b.nv && a.color == b.color && a.terrainSerial == b.terrainSerial &&
               a.terrainRevision == b.terrainRevision;
    }
    static Result fill(const Entry &e, bool hit)
    {
        Result r;
        r.vertices = e.vertices.data(); r.vertexCount = static_cast<unsigned>(e.vertices.size());
        r.indices = e.indices.data(); r.indexCount = static_cast<unsigned>(e.indices.size());
        r.hit = hit;
        return r;
    }
    void evictFor(const void *keep)
    {
        while (m_stats.bytes > m_budget) {
            auto victim = m_entries.end();
            for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
                if (it->first == keep || !it->second.valid) continue;
                if (victim == m_entries.end() || it->second.stamp < victim->second.stamp) victim = it;
            }
            if (victim == m_entries.end()) break;
            m_stats.bytes -= victim->second.bytes;
            m_entries.erase(victim);
            ++m_stats.evictions;
        }
    }

    std::unordered_map<const void *, Entry> m_entries;
    Entry m_scratch;
    Statistics m_stats;
    unsigned long long m_clock = 0;
    size_t m_budget;
};
} // namespace NativeDecalGrid
