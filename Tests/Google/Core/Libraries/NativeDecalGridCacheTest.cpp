// SPDX-License-Identifier: GPL-3.0-or-later
// X64.35 decal-geometry reuse: the cache must be exact (bit-identical to the canonical generation and to the former
// inline EmitDecal loop), reuse only bit-identical inputs including the terrain identity/revision, and never return stale
// geometry.
#include <gtest/gtest.h>
#if defined(_WIN64)
#include "W3DDevice/GameClient/NativeDecalGridCache.h"
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
using NativeDecalGrid::Cache;
using NativeDecalGrid::Params;

// A deterministic stand-in for the terrain: a bumpy height field that also depends on a mutable "revision" the way the
// real height map does (a changed height changes the lookups).
struct Terrain
{
    float bump = 0.0f;
    float operator()(float x, float y) const { return 3.0f + 0.01f * x + 0.02f * y + bump * std::sin(0.3f * x) * std::cos(0.2f * y); }
};

Params MakeParams(float cx = 120.0f, float cy = 80.0f, float yaw = 0.7f, float sx = 18.0f, float sy = 12.0f)
{
    Params p;
    p.cx = cx; p.cy = cy; p.yaw = yaw; p.sx = sx; p.sy = sy;
    NativeDecalGrid::Dimensions(sx, sy, p.nu, p.nv);
    p.color = 0x80ffffffu;
    p.terrainSerial = 7;
    p.terrainRevision = 1;
    return p;
}

// EmitDecal's grid code exactly as it stood before this change (inline, calling the height function directly).
void LegacyGrid(const Params &p, const Terrain &terrain, std::vector<WorldVertexXYZDUV2> &vertices,
                std::vector<unsigned short> &indices)
{
    const float sx = p.sx, sy = p.sy, cx = p.cx, cy = p.cy, yaw = p.yaw;
    int nu = static_cast<int>(std::ceil(sx / 2.5f)) + 1, nv = static_cast<int>(std::ceil(sy / 2.5f)) + 1;
    nu = std::max(2, std::min(nu, 33)); nv = std::max(2, std::min(nv, 33));
    const float stepX = sx / static_cast<float>(nu - 1), stepY = sy / static_cast<float>(nv - 1);
    const float c = std::cos(yaw), s = std::sin(yaw);
    vertices.clear(); indices.clear();
    const unsigned color = p.color;
    for (int j = 0; j < nv; ++j) {
        for (int i = 0; i < nu; ++i) {
            const float fu = static_cast<float>(i) / static_cast<float>(nu - 1);
            const float fv = static_cast<float>(j) / static_cast<float>(nv - 1);
            const float lx = (fu - 0.5f) * sx, ly = (fv - 0.5f) * sy;
            WorldVertexXYZDUV2 v;
            v.x = cx + lx * c - ly * s;
            v.y = cy + lx * s + ly * c;
            const float h = terrain(v.x, v.y);
            const float gx = (terrain(v.x + 1.25f, v.y) - h) / 1.25f, gy = (terrain(v.x, v.y + 1.25f) - h) / 1.25f;
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

bool SameBytes(const Cache::Result &r, const std::vector<WorldVertexXYZDUV2> &v, const std::vector<unsigned short> &i)
{
    return r.vertexCount == v.size() && r.indexCount == i.size() &&
           std::memcmp(r.vertices, v.data(), v.size() * sizeof(WorldVertexXYZDUV2)) == 0 &&
           std::memcmp(r.indices, i.data(), i.size() * sizeof(unsigned short)) == 0;
}

void ExpectCanonical(const Cache::Result &r, const Params &p, const Terrain &t)
{
    std::vector<WorldVertexXYZDUV2> v;
    std::vector<unsigned short> i;
    LegacyGrid(p, t, v, i);
    EXPECT_TRUE(SameBytes(r, v, i));
}
} // namespace

TEST(NativeDecalGridCache, BuildEqualsTheFormerInlineLoopAndHitsAreBitIdentical)
{
    Cache cache;
    Terrain terrain;
    terrain.bump = 1.5f;
    const int owner = 0;
    const Params p = MakeParams();
    const Cache::Result miss = cache.get(&owner, p, terrain);
    EXPECT_FALSE(miss.hit);
    ExpectCanonical(miss, p, terrain);
    const Cache::Result hit = cache.get(&owner, p, terrain);
    EXPECT_TRUE(hit.hit);
    ExpectCanonical(hit, p, terrain);
    EXPECT_EQ(cache.statistics().hits, 1u);
    EXPECT_EQ(cache.statistics().misses, 1u);
}

TEST(NativeDecalGridCache, EveryGeometryInputChangeRecomputes)
{
    Cache cache;
    Terrain terrain;
    terrain.bump = 1.0f;
    const int owner = 0;
    ASSERT_FALSE(cache.get(&owner, MakeParams(), terrain).hit);
    ASSERT_TRUE(cache.get(&owner, MakeParams(), terrain).hit);
    auto expectMiss = [&](Params p, const char *what, bool derivedDims = true) {
        const Cache::Result r = cache.get(&owner, p, terrain);
        EXPECT_FALSE(r.hit) << what;
        if (derivedDims) ExpectCanonical(r, p, terrain);   // the former loop derives the dimensions from the size
        else {
            EXPECT_EQ(r.vertexCount, static_cast<unsigned>(p.nu * p.nv)) << what;
            Cache fresh;
            const int other = 0;
            const Cache::Result f = fresh.get(&other, p, terrain);   // a fresh cache computes the same grid
            EXPECT_TRUE(SameBytes(r, std::vector<WorldVertexXYZDUV2>(f.vertices, f.vertices + f.vertexCount),
                                  std::vector<unsigned short>(f.indices, f.indices + f.indexCount))) << what;
        }
        EXPECT_TRUE(cache.get(&owner, p, terrain).hit) << what;   // the entry now holds the new inputs
    };
    expectMiss(MakeParams(121.0f), "center x");
    expectMiss(MakeParams(121.0f, 81.0f), "center y");
    expectMiss(MakeParams(121.0f, 81.0f, 0.71f), "yaw");
    expectMiss(MakeParams(121.0f, 81.0f, 0.71f, 19.0f, 12.0f), "size x");
    expectMiss(MakeParams(121.0f, 81.0f, 0.71f, 19.0f, 13.0f), "size y");
    Params p = MakeParams(121.0f, 81.0f, 0.71f, 19.0f, 13.0f);
    p.color = 0x40ffffffu;                                       expectMiss(p, "colour");
    Params dims = p; dims.nu = p.nu - 1;                         expectMiss(dims, "grid width", false);
    dims = p; dims.nv = p.nv - 1;                                expectMiss(dims, "grid height", false);
    Params t = p; t.terrainRevision = p.terrainRevision + 1;     expectMiss(t, "terrain revision");
    t.terrainSerial = p.terrainSerial + 1;                       expectMiss(t, "terrain identity (map replaced)");
}

TEST(NativeDecalGridCache, TerrainChangeWithANewRevisionFollowsTheNewHeights)
{
    Cache cache;
    Terrain terrain;
    const int owner = 0;
    Params p = MakeParams();
    const Cache::Result first = cache.get(&owner, p, terrain);
    std::vector<WorldVertexXYZDUV2> before(first.vertices, first.vertices + first.vertexCount);
    // the terrain is modified (construction flattening) and the revision is bumped: the grid must follow
    terrain.bump = 4.0f;
    p.terrainRevision += 1;
    const Cache::Result after = cache.get(&owner, p, terrain);
    EXPECT_FALSE(after.hit);
    ExpectCanonical(after, p, terrain);
    EXPECT_NE(std::memcmp(before.data(), after.vertices, before.size() * sizeof(WorldVertexXYZDUV2)), 0);
}

TEST(NativeDecalGridCache, UnknownTerrainSourceIsNeverCached)
{
    Cache cache;
    Terrain terrain;
    const int owner = 0;
    Params p = MakeParams();
    p.terrainSerial = 0;
    const Cache::Result a = cache.get(&owner, p, terrain);
    EXPECT_FALSE(a.hit);
    ExpectCanonical(a, p, terrain);
    terrain.bump = 2.0f;                                          // heights changed with no stamp: still correct
    const Cache::Result b = cache.get(&owner, p, terrain);
    EXPECT_FALSE(b.hit);
    ExpectCanonical(b, p, terrain);
    EXPECT_EQ(cache.statistics().entries, 0u);
    EXPECT_EQ(cache.statistics().uncacheable, 2u);
    const Cache::Result noOwner = cache.get(nullptr, MakeParams(), terrain);
    EXPECT_FALSE(noOwner.hit);
    EXPECT_EQ(cache.statistics().entries, 0u);
}

TEST(NativeDecalGridCache, MovingCasterFollowsTheCanonicalResultEveryFrame)
{
    Cache cache;
    Terrain terrain;
    terrain.bump = 1.0f;
    const int owner = 0;
    for (int frame = 0; frame < 40; ++frame) {
        const Params p = MakeParams(100.0f + 0.37f * static_cast<float>(frame), 80.0f, 0.1f * static_cast<float>(frame));
        const Cache::Result r = cache.get(&owner, p, terrain);
        EXPECT_FALSE(r.hit);                  // a moving caster never matches the previous frame
        ExpectCanonical(r, p, terrain);
    }
    const Params still = MakeParams(110.0f, 80.0f, 0.4f);
    cache.get(&owner, still, terrain);
    EXPECT_TRUE(cache.get(&owner, still, terrain).hit);   // once it stops, it is served from the cache
}

TEST(NativeDecalGridCache, ForgetAndOwnerAddressReuseCannotReturnStaleGeometry)
{
    Cache cache;
    Terrain terrain;
    const int owner = 0;
    const Params a = MakeParams(100.0f), b = MakeParams(140.0f);
    ASSERT_FALSE(cache.get(&owner, a, terrain).hit);
    cache.forget(&owner);
    EXPECT_FALSE(cache.contains(&owner));
    EXPECT_EQ(cache.statistics().bytes, 0u);
    Cache::Result r = cache.get(&owner, b, terrain);        // the address is reused by a new shadow
    EXPECT_FALSE(r.hit);
    ExpectCanonical(r, b, terrain);
    r = cache.get(&owner, a, terrain);                      // same address, different decal, no forget
    EXPECT_FALSE(r.hit);
    ExpectCanonical(r, a, terrain);
}

TEST(NativeDecalGridCache, EvictionIsBoundedAndNeverReturnsStaleGeometry)
{
    Terrain terrain;
    terrain.bump = 0.5f;
    Cache probe;
    const int probeOwner = 0;
    probe.get(&probeOwner, MakeParams(), terrain);
    const size_t one = probe.statistics().bytes;
    ASSERT_GT(one, 0u);

    Cache cache(one * 3 + one / 2);                          // room for three entries
    int owners[8];
    std::vector<Params> params;
    for (int i = 0; i < 8; ++i) params.push_back(MakeParams(100.0f + 5.0f * static_cast<float>(i)));
    for (int i = 0; i < 8; ++i) ASSERT_FALSE(cache.get(&owners[i], params[i], terrain).hit);
    EXPECT_LE(cache.statistics().bytes, cache.budget());
    EXPECT_GT(cache.statistics().evictions, 0u);
    EXPECT_LE(cache.statistics().entries, 3u);
    for (int i = 0; i < 8; ++i) ExpectCanonical(cache.get(&owners[i], params[i], terrain), params[i], terrain);
    Cache tiny(16);                                          // larger than the whole budget: never retained
    const Cache::Result r = tiny.get(&owners[0], params[0], terrain);
    EXPECT_FALSE(r.hit);
    ExpectCanonical(r, params[0], terrain);
    EXPECT_EQ(tiny.statistics().entries, 0u);
    EXPECT_EQ(tiny.statistics().bytes, 0u);
}

TEST(NativeDecalGridCache, ClearDropsEverything)
{
    Cache cache;
    Terrain terrain;
    int a = 0, b = 0;
    cache.get(&a, MakeParams(100.0f), terrain);
    cache.get(&b, MakeParams(110.0f), terrain);
    ASSERT_EQ(cache.statistics().entries, 2u);
    cache.clear();
    EXPECT_EQ(cache.statistics().entries, 0u);
    EXPECT_EQ(cache.statistics().bytes, 0u);
    EXPECT_FALSE(cache.get(&a, MakeParams(100.0f), terrain).hit);
}
#else
TEST(NativeDecalGridCache, Win64Only) { SUCCEED(); }
#endif
