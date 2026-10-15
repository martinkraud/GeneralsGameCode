// SPDX-License-Identifier: GPL-3.0-or-later
// X64.35 shadow-volume reuse: the cache must be exact (byte-identical to a fresh computation and to the former inline
// loop), reuse only bit-identical inputs, and never return stale geometry.
#include <gtest/gtest.h>
#if defined(_WIN64)
#include "W3DDevice/GameClient/NativeShadowVolumeCache.h"
#include <cstring>
#include <vector>

namespace
{
using NativeShadowVolume::Cache;
using NativeShadowVolume::Inputs;

struct Caster
{
    std::vector<Vector3> positions;
    std::vector<unsigned> triangles;
    Inputs inputs() const
    {
        Inputs in;
        in.positions = positions.data(); in.positionCount = static_cast<unsigned>(positions.size());
        in.triangles = triangles.data(); in.indexCount = static_cast<unsigned>(triangles.size());
        in.light = Vector3(0.3f, 0.2f, -0.9f);
        in.color = 0xff7f7f7fu;
        in.center = Vector3(100.0f, 50.0f, 3.0f);
        in.g0 = 3.0f; in.gx = 0.05f; in.gy = -0.02f;
        return in;
    }
};

Caster MakeCaster(unsigned quads, float offset = 0.0f)
{
    Caster c;
    for (unsigned q = 0; q < quads; ++q) {
        const float x = 100.0f + static_cast<float>(q) * 1.5f + offset;
        const unsigned base = static_cast<unsigned>(c.positions.size());
        c.positions.push_back(Vector3(x, 50.0f, 3.0f));
        c.positions.push_back(Vector3(x + 1.0f, 50.0f, 3.0f));
        c.positions.push_back(Vector3(x + 1.0f, 51.0f, 4.5f));
        c.positions.push_back(Vector3(x, 51.0f, 4.5f));
        for (unsigned i : {0u, 1u, 2u, 0u, 2u, 3u}) c.triangles.push_back(base + i);
    }
    return c;
}

// The projection loop exactly as EmitVolumeShadow carried it before this change.
void LegacyPlanarLoop(const Inputs &in, std::vector<WorldVertexXYZDUV2> &vertices, std::vector<unsigned short> &indices)
{
    vertices.resize(in.positionCount);
    for (size_t i = 0; i < in.positionCount; ++i) {
        const Vector3 &p = in.positions[i];
        float x = p.X, y = p.Y, ground = in.g0;
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
    for (size_t i = 0; i < in.indexCount; ++i) indices[i] = static_cast<unsigned short>(in.triangles[i]);
}

bool SameBytes(const Cache::Result &r, const std::vector<WorldVertexXYZDUV2> &v, const std::vector<unsigned short> &i)
{
    return r.vertexCount == v.size() && r.indexCount == i.size() &&
           std::memcmp(r.vertices, v.data(), v.size() * sizeof(WorldVertexXYZDUV2)) == 0 &&
           std::memcmp(r.indices, i.data(), i.size() * sizeof(unsigned short)) == 0;
}

void ExpectMatchesFresh(const Cache::Result &r, const Inputs &in)
{
    std::vector<WorldVertexXYZDUV2> v;
    std::vector<unsigned short> i;
    LegacyPlanarLoop(in, v, i);
    EXPECT_TRUE(SameBytes(r, v, i));
}
} // namespace

TEST(NativeShadowVolumeCache, ResultIsByteIdenticalToTheLegacyLoopOnMissAndOnHit)
{
    Cache cache;
    const Caster c = MakeCaster(40);
    const Cache::Result miss = cache.get(&c, c.inputs());
    EXPECT_FALSE(miss.hit);
    ExpectMatchesFresh(miss, c.inputs());
    const Cache::Result hit = cache.get(&c, c.inputs());
    EXPECT_TRUE(hit.hit);
    ExpectMatchesFresh(hit, c.inputs());
    EXPECT_EQ(cache.statistics().hits, 1u);
    EXPECT_EQ(cache.statistics().misses, 1u);
}

TEST(NativeShadowVolumeCache, EveryInputChangeForcesRecomputation)
{
    Cache cache;
    const Caster c = MakeCaster(8);
    ASSERT_FALSE(cache.get(&c, c.inputs()).hit);
    ASSERT_TRUE(cache.get(&c, c.inputs()).hit);

    auto expectMiss = [&](Inputs in, const char *what) {
        const Cache::Result r = cache.get(&c, in);
        EXPECT_FALSE(r.hit) << what;
        ExpectMatchesFresh(r, in);                       // and the new result is the fresh one
        EXPECT_TRUE(cache.get(&c, in).hit) << what;      // the entry now holds the new inputs
    };
    Inputs in = c.inputs();
    in.light = Vector3(0.31f, 0.2f, -0.9f);          expectMiss(in, "light");
    in = c.inputs(); in.color = 0xff808080u;         expectMiss(in, "color");
    in = c.inputs(); in.g0 = 3.5f;                   expectMiss(in, "g0");
    in = c.inputs(); in.gx = 0.06f;                  expectMiss(in, "gx");
    in = c.inputs(); in.gy = -0.03f;                 expectMiss(in, "gy");
    in = c.inputs(); in.center = Vector3(100.5f, 50.0f, 3.0f); expectMiss(in, "center");
    // -0.0f vs 0.0f are different bit patterns: treated as a change (conservative)
    Caster z = c; z.positions[0].X = 0.0f;
    ASSERT_FALSE(cache.get(&z, z.inputs()).hit);
    z.positions[0].X = -0.0f;
    EXPECT_FALSE(cache.get(&z, z.inputs()).hit);
}

TEST(NativeShadowVolumeCache, PositionOrTopologyChangeForcesRecomputation)
{
    Cache cache;
    Caster c = MakeCaster(8);
    ASSERT_FALSE(cache.get(&c, c.inputs()).hit);
    ASSERT_TRUE(cache.get(&c, c.inputs()).hit);
    c.positions[5].Z += 0.001f;                       // one vertex moved a hair (animation)
    Cache::Result r = cache.get(&c, c.inputs());
    EXPECT_FALSE(r.hit);
    ExpectMatchesFresh(r, c.inputs());
    c.triangles[4] = c.triangles[3];                  // same positions, different topology
    r = cache.get(&c, c.inputs());
    EXPECT_FALSE(r.hit);
    ExpectMatchesFresh(r, c.inputs());
    c = MakeCaster(9);                                // different vertex count (LOD change)
    r = cache.get(&c, c.inputs());
    EXPECT_FALSE(r.hit);
    ExpectMatchesFresh(r, c.inputs());
}

TEST(NativeShadowVolumeCache, ForgetAndOwnerAddressReuseCannotReturnStaleGeometry)
{
    Cache cache;
    const Caster a = MakeCaster(10, 0.0f), b = MakeCaster(10, 7.0f);
    const int owner = 0;                              // a stand-in for a NativeShadow address
    ASSERT_FALSE(cache.get(&owner, a.inputs()).hit);
    cache.forget(&owner);
    EXPECT_FALSE(cache.contains(&owner));
    EXPECT_EQ(cache.statistics().bytes, 0u);
    // the address is reused by a new shadow with different geometry: must be a miss and fresh
    Cache::Result r = cache.get(&owner, b.inputs());
    EXPECT_FALSE(r.hit);
    ExpectMatchesFresh(r, b.inputs());
    // even without an explicit forget a different caster at the same address is recomputed
    r = cache.get(&owner, a.inputs());
    EXPECT_FALSE(r.hit);
    ExpectMatchesFresh(r, a.inputs());
}

TEST(NativeShadowVolumeCache, EvictionIsBoundedAndNeverReturnsStaleGeometry)
{
    const Caster c = MakeCaster(20);
    Cache probe;
    probe.get(&c, c.inputs());
    const size_t one = probe.statistics().bytes;
    ASSERT_GT(one, 0u);

    Cache cache(one * 3 + one / 2);                   // room for three entries
    int owners[8];
    std::vector<Caster> casters;
    for (int i = 0; i < 8; ++i) casters.push_back(MakeCaster(20, static_cast<float>(i)));
    for (int i = 0; i < 8; ++i) ASSERT_FALSE(cache.get(&owners[i], casters[i].inputs()).hit);
    EXPECT_LE(cache.statistics().bytes, cache.budget());
    EXPECT_GT(cache.statistics().evictions, 0u);
    EXPECT_LE(cache.statistics().entries, 3u);
    // oldest owners were evicted: asking again recomputes and equals the fresh result
    for (int i = 0; i < 8; ++i) {
        const Cache::Result r = cache.get(&owners[i], casters[i].inputs());
        ExpectMatchesFresh(r, casters[i].inputs());
    }
    // an entry larger than the whole budget is never retained but still correct
    Cache tiny(16);
    const Cache::Result r = tiny.get(&owners[0], casters[0].inputs());
    EXPECT_FALSE(r.hit);
    ExpectMatchesFresh(r, casters[0].inputs());
    EXPECT_EQ(tiny.statistics().entries, 0u);
    EXPECT_EQ(tiny.statistics().bytes, 0u);
}

TEST(NativeShadowVolumeCache, UncertainInputsUseTheCanonicalPathAndAreNotStored)
{
    Cache cache;
    Inputs empty;
    const Cache::Result r = cache.get(nullptr, empty);
    EXPECT_FALSE(r.hit);
    EXPECT_EQ(r.vertexCount, 0u);
    const Caster c = MakeCaster(3);
    const Cache::Result noOwner = cache.get(nullptr, c.inputs());
    EXPECT_FALSE(noOwner.hit);
    ExpectMatchesFresh(noOwner, c.inputs());
    EXPECT_EQ(cache.statistics().entries, 0u);
    EXPECT_EQ(cache.statistics().uncacheable, 2u);
}

TEST(NativeShadowVolumeCache, InputsAreNeverModified)
{
    Cache cache;
    Caster c = MakeCaster(6);
    const std::vector<Vector3> positions = c.positions;
    const std::vector<unsigned> triangles = c.triangles;
    Inputs in = c.inputs();
    cache.get(&c, in);
    cache.get(&c, in);
    ASSERT_EQ(c.positions.size(), positions.size());
    EXPECT_EQ(std::memcmp(c.positions.data(), positions.data(), positions.size() * sizeof(Vector3)), 0);
    EXPECT_EQ(c.triangles, triangles);
    // a hit returns copies owned by the cache, not pointers into the caller's arrays
    const Cache::Result r = cache.get(&c, in);
    EXPECT_NE(static_cast<const void *>(r.vertices), static_cast<const void *>(c.positions.data()));
}
#else
TEST(NativeShadowVolumeCache, Win64Only) { SUCCEED(); }
#endif
