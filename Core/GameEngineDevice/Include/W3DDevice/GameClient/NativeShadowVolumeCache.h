// SPDX-License-Identifier: GPL-3.0-or-later
// X64.35 optimization: exact frame-to-frame reuse of planar shadow-volume geometry.
//
// The native world renderer rebuilt every shadow silhouette every frame (gather posed vertices, project onto the
// terrain plane, convert indices). For casters whose gathered positions, topology and projection inputs are
// bit-identical to the previous frame the result is bit-identical too, so it can be reused. The key is *all* inputs
// of the projection, compared with memcmp: there is no version counter and no assumption about animation or object
// identity, so a stale result cannot be returned. Any difference (or any uncertainty) recomputes from scratch.
//
// Render-only: the cache never reads or writes simulation state; its inputs are passed by const pointer.
#pragma once
#include <WW3D2/WorldSubmission.h>
#include <WWMath/vector3.h>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NativeShadowVolume
{
struct Inputs
{
    const Vector3 *positions = nullptr;      // gathered world-space vertex positions
    unsigned positionCount = 0;
    const unsigned *triangles = nullptr;     // triangle list indices into positions
    unsigned indexCount = 0;
    Vector3 light = Vector3(0, 0, -1);       // direction the light travels (normalized)
    unsigned color = 0;                      // vertex diffuse
    Vector3 center = Vector3(0, 0, 0);       // caster position (plane fit origin)
    float g0 = 0.0f, gx = 0.0f, gy = 0.0f;   // ground height at the center and its slope
};

// The canonical planar projection (the former inline loop of EmitVolumeShadow, unchanged arithmetic).
inline void ProjectPlanar(const Inputs &in, std::vector<WorldVertexXYZDUV2> &vertices, std::vector<unsigned short> &indices)
{
    vertices.resize(in.positionCount);
    for (unsigned i = 0; i < in.positionCount; ++i) {
        const Vector3 &p = in.positions[i];
        float x = p.X, y = p.Y, ground = in.g0;
        // z(x, y) = g0 + gx (x - cx) + gy (y - cy); solve p + d t on that plane.
        const float denom = in.light.Z - in.gx * in.light.X - in.gy * in.light.Y;
        float t = std::fabs(denom) > 1e-4f ? (in.g0 + in.gx * (p.X - in.center.X) + in.gy * (p.Y - in.center.Y) - p.Z) / denom : 0.0f;
        if (t < 0.0f) t = 0.0f;
        x = p.X + in.light.X * t;
        y = p.Y + in.light.Y * t;
        ground = in.g0 + in.gx * (x - in.center.X) + in.gy * (y - in.center.Y);
        vertices[i].x = x; vertices[i].y = y; vertices[i].z = ground + 0.3f;
        vertices[i].diffuse = in.color;
        vertices[i].u0 = vertices[i].v0 = vertices[i].u1 = vertices[i].v1 = 0.0f;
    }
    indices.resize(in.indexCount);
    for (unsigned i = 0; i < in.indexCount; ++i) indices[i] = static_cast<unsigned short>(in.triangles[i]);
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

    explicit Cache(size_t budgetBytes = 16u * 1024u * 1024u) : m_budget(budgetBytes) {}

    // Geometry for `in`, from the entry of `owner` when every input is bit-identical, otherwise recomputed (and the
    // entry replaced). The returned pointers are valid until the next get/forget/clear call.
    Result get(const void *owner, const Inputs &in)
    {
        if (!owner || !in.positions || !in.triangles || in.positionCount == 0 || in.indexCount == 0) {
            ++m_stats.uncacheable;
            compute(in, m_scratch);
            return fill(m_scratch, false);
        }
        Entry &e = m_entries[owner];
        if (e.valid && matches(e, in)) {
            e.stamp = ++m_clock;
            ++m_stats.hits;
            return fill(e, true);
        }
        // miss: recompute and replace the entry
        if (e.valid) m_stats.bytes -= e.bytes;
        store(e, in);
        e.stamp = ++m_clock;
        e.valid = true;
        m_stats.bytes += e.bytes;
        ++m_stats.misses;
        if (e.bytes > m_budget) {     // never retain an entry larger than the whole budget
            m_scratch.vertices = std::move(e.vertices);
            m_scratch.indices = std::move(e.indices);
            m_stats.bytes -= e.bytes;
            m_entries.erase(owner);
            return fill(m_scratch, false);
        }
        evictFor(owner);
        // evictFor never removes `owner`, so the reference is still the live entry.
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
        std::vector<Vector3> positions;
        std::vector<unsigned> triangles;
        Vector3 light, center;
        unsigned color = 0;
        float g0 = 0, gx = 0, gy = 0;
        std::vector<WorldVertexXYZDUV2> vertices;
        std::vector<unsigned short> indices;
        size_t bytes = 0;
        unsigned long long stamp = 0;
    };

    static bool sameBits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }
    static bool sameBits(const Vector3 &a, const Vector3 &b)
    { return sameBits(a.X, b.X) && sameBits(a.Y, b.Y) && sameBits(a.Z, b.Z); }

    static bool matches(const Entry &e, const Inputs &in)
    {
        return e.positions.size() == in.positionCount && e.triangles.size() == in.indexCount && e.color == in.color &&
               sameBits(e.light, in.light) && sameBits(e.center, in.center) && sameBits(e.g0, in.g0) &&
               sameBits(e.gx, in.gx) && sameBits(e.gy, in.gy) &&
               std::memcmp(e.positions.data(), in.positions, sizeof(Vector3) * in.positionCount) == 0 &&
               std::memcmp(e.triangles.data(), in.triangles, sizeof(unsigned) * in.indexCount) == 0;
    }
    static void compute(const Inputs &in, Entry &out) { ProjectPlanar(in, out.vertices, out.indices); }
    static void store(Entry &e, const Inputs &in)
    {
        e.positions.assign(in.positions, in.positions + in.positionCount);
        e.triangles.assign(in.triangles, in.triangles + in.indexCount);
        e.light = in.light; e.center = in.center; e.color = in.color; e.g0 = in.g0; e.gx = in.gx; e.gy = in.gy;
        ProjectPlanar(in, e.vertices, e.indices);
        e.bytes = e.positions.size() * sizeof(Vector3) + e.triangles.size() * sizeof(unsigned) +
                  e.vertices.size() * sizeof(WorldVertexXYZDUV2) + e.indices.size() * sizeof(unsigned short);
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
} // namespace NativeShadowVolume
