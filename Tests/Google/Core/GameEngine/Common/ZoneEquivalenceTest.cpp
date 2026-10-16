// SPDX-License-Identifier: GPL-3.0-or-later
// X64.39: differential oracle for the zone-equivalence primitives. The LEGACY code (verbatim) and the MINROOT union-find are driven with the
// same merge sequences and compared bit for bit - representative labels, not just connectivity.

#include <gtest/gtest.h>

#include "GameLogic/Pathfinder/ZoneEquivalence.h"

#include <cstdint>
#include <vector>

using namespace ZoneEquivalence;
typedef unsigned short Z;

namespace
{
struct Lcg
{
	uint64_t s;
	explicit Lcg(uint64_t seed) : s(seed * 2862933555777941757ull + 3037000493ull) {}
	uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<uint32_t>(s >> 33); }
	int below(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

std::vector<Z> identity(int n) { std::vector<Z> v(n); for (int i = 0; i < n; ++i) v[i] = static_cast<Z>(i); return v; }

std::vector<Z> flattened(std::vector<Z> v, int n) { minRootFlatten(v.data(), n); return v; }

// every operation runs on both implementations; the returned decision and (optionally) the flattened candidate are compared to legacy
struct Pair
{
	int n;
	std::vector<Z> legacy, cand;
	explicit Pair(int count) : n(count), legacy(identity(count)), cand(identity(count)) {}
	void apply(int target, int source, bool checkEveryOp)
	{
		int l = -1, c = -1;
		const int rl = legacyApply(legacy.data(), n, target, source, &l);
		const int rc = minRootApply(cand.data(), n, target, source, &c);
		ASSERT_EQ(rl, rc) << "target=" << target << " source=" << source;
		if (rl) ASSERT_EQ(l, c) << "new target zone, target=" << target << " source=" << source;
		if (checkEveryOp) ASSERT_EQ(legacy, flattened(cand, n));
	}
	void expectEqualFinal() { ASSERT_EQ(legacy, flattened(cand, n)); }
};
}  // namespace

TEST(ZoneEquivalence, ProvenInvariantEveryEntryIsItsClassMinimum)
{
	// after arbitrary legacy merges: E[i] <= i, E[E[i]] == E[i], and E[i] is the smallest member of the class (checked by brute force)
	Lcg r(1);
	const int n = 300;
	std::vector<Z> e = identity(n);
	std::vector<int> comp(n);
	for (int i = 0; i < n; ++i) comp[i] = i;
	for (int op = 0; op < 400; ++op) {
		const int a = 1 + r.below(n - 1), b = 1 + r.below(n - 1);
		legacyResolveZones(a, b, e.data(), n);
		const int ca = comp[a], cb = comp[b];
		for (int i = 0; i < n; ++i) if (comp[i] == cb) comp[i] = ca;
		for (int i = 0; i < n; ++i) {
			ASSERT_LE(e[i], i);
			ASSERT_EQ(e[e[i]], e[i]);
			int minimum = i;
			for (int j = 0; j < n; ++j) if (comp[j] == comp[i] && j < minimum) minimum = j;
			ASSERT_EQ(e[i], minimum) << "op " << op << " entry " << i;
		}
	}
}

TEST(ZoneEquivalence, AscendingDescendingAndChainMerges)
{
	for (int dir = 0; dir < 3; ++dir) {
		Pair p(2000);
		for (int i = 1; i < 1999; ++i) {
			int a = i, b = i + 1;
			if (dir == 1) { a = 1999 - i; b = a + 1 > 1999 ? 1999 : a + 1; }
			if (dir == 2) { a = (i % 2) ? i : 1999 - i; b = a + 1; if (b > 1999) b = 1999; }
			p.apply(a, b, i % 50 == 0);
			if (HasFatalFailure()) return;
		}
		p.expectEqualFinal();
	}
}

TEST(ZoneEquivalence, StarsRepeatedAndAlreadyEqualMerges)
{
	Pair p(1500);
	for (int i = 1; i < 1500; ++i) { p.apply(1, i, i % 40 == 0); if (HasFatalFailure()) return; }
	for (int i = 1; i < 1500; ++i) { p.apply(i, 1, false); p.apply(i, i, false); if (HasFatalFailure()) return; }
	p.expectEqualFinal();
	Pair q(1500);
	for (int i = 1; i < 1500; ++i) { q.apply(1499, i, i % 40 == 0); if (HasFatalFailure()) return; }
	q.expectEqualFinal();
}

TEST(ZoneEquivalence, DisconnectedSetsStayDisconnected)
{
	Pair p(3000);
	for (int base = 1; base < 2900; base += 100)
		for (int k = 0; k < 60; ++k) { p.apply(base + k, base + k + 1, false); if (HasFatalFailure()) return; }
	p.expectEqualFinal();
	std::vector<Z> f = flattened(p.cand, p.n);
	EXPECT_EQ(f[1], 1);
	EXPECT_EQ(f[61], 1);
	EXPECT_EQ(f[101], 101);
	EXPECT_EQ(f[2999], 2999);
}

TEST(ZoneEquivalence, RandomSparseAndDenseGraphs)
{
	for (int seed = 1; seed <= 12; ++seed) {
		for (int density = 0; density < 3; ++density) {
			const int n = 2500;
			const int ops = density == 0 ? 400 : density == 1 ? 2500 : 9000;
			Lcg r(static_cast<uint64_t>(seed) * 7 + density);
			Pair p(n);
			for (int op = 0; op < ops; ++op) {
				p.apply(1 + r.below(n - 1), 1 + r.below(n - 1), op % 400 == 0);
				if (HasFatalFailure()) return;
			}
			p.expectEqualFinal();
			if (HasFatalFailure()) return;
		}
	}
}

TEST(ZoneEquivalence, LabelScanWithUnassignedTargetsMatches)
{
	// the label scan: cells start with zone 0; apply() hands out the neighbour label; compare a whole simulated scan (cell zones included)
	for (int seed = 1; seed <= 6; ++seed) {
		const int w = 90, h = 70, maxZones = 24000;
		Lcg r(100 + seed);
		std::vector<int> terrain(w * h);
		for (int& t : terrain) t = r.below(100) < 30 ? 1 : 0;
		std::vector<Z> el = identity(maxZones), ec = identity(maxZones);
		std::vector<int> zl(w * h, 0), zc(w * h, 0);
		int nextL = 1, nextC = 1;
		for (int j = 0; j < h; ++j) {
			for (int i = 0; i < w; ++i) {
				const int idx = j * w + i;
				zl[idx] = 0; zc[idx] = 0;
				for (int dir = 0; dir < 2; ++dir) {
					const int nIdx = dir == 0 ? idx - 1 : idx - w;
					if ((dir == 0 && i == 0) || (dir == 1 && j == 0)) continue;
					if (terrain[idx] != terrain[nIdx]) continue;
					int l = 0, c = 0;
					if (legacyApply(el.data(), nextL, zl[idx], zl[nIdx], &l)) zl[idx] = l;
					if (minRootApply(ec.data(), nextC, zc[idx], zc[nIdx], &c)) zc[idx] = c;
				}
				if (zl[idx] == 0) zl[idx] = nextL++;
				if (zc[idx] == 0) zc[idx] = nextC++;
				ASSERT_EQ(zl[idx], zc[idx]) << "cell " << idx;
			}
		}
		ASSERT_EQ(nextL, nextC);
		minRootFlatten(ec.data(), nextC);
		ASSERT_EQ(el, ec);
		ASSERT_EQ(zl, zc);
	}
}

TEST(ZoneEquivalence, BlockTablesWithOffsetsMatch)
{
	for (int first : {0, 1, 7, 100, 4097, 20000}) {
		for (int n : {2, 3, 17, 100, 400}) {
			Lcg r(static_cast<uint64_t>(first) + n);
			std::vector<Z> l(n), c(n);
			for (int i = 0; i < n; ++i) l[i] = c[i] = static_cast<Z>(i + first);
			for (int op = 0; op < 3 * n; ++op) {
				const int a = first + r.below(n), b = first + r.below(n);
				legacyApplyBlock(l.data(), first, n, a, b);
				minRootApplyBlock(c.data(), first, n, a, b);
			}
			minRootFlattenBlock(c.data(), first, n);
			ASSERT_EQ(l, c) << "first=" << first << " n=" << n;
		}
	}
}

TEST(ZoneEquivalence, FlattenStageGivesTheSameResultFromEitherInput)
{
	// the tables feed the (legacy) flatten stage together with the hierarchical table; the stage must produce identical output
	for (int seed = 1; seed <= 8; ++seed) {
		const int n = 1800;
		Lcg r(900 + seed);
		std::vector<Z> hl = identity(n), hc = identity(n), al = identity(n), ac = identity(n);
		for (int op = 0; op < 900; ++op) {
			const int a = 1 + r.below(n - 1), b = 1 + r.below(n - 1);
			legacyResolveZones(a, b, hl.data(), n);
			minRootUnion(hc.data(), a, b);
			if (op % 2) { legacyResolveZones(a, b, al.data(), n); minRootUnion(ac.data(), a, b); }
		}
		for (int op = 0; op < 400; ++op) {
			const int a = 1 + r.below(n - 1), b = 1 + r.below(n - 1);
			legacyResolveZones(a, b, al.data(), n);
			minRootUnion(ac.data(), a, b);
		}
		minRootFlatten(hc.data(), n);
		minRootFlatten(ac.data(), n);
		ASSERT_EQ(hl, hc);
		ASSERT_EQ(al, ac);
		legacyFlatten(al.data(), hl.data(), n);
		legacyFlatten(ac.data(), hc.data(), n);
		ASSERT_EQ(al, ac);
	}
}

TEST(ZoneEquivalence, NearMaximumTableSize)
{
	const int n = 24000;
	Lcg r(77);
	Pair p(n);
	for (int op = 0; op < 1500; ++op) {
		p.apply(1 + r.below(n - 1), 1 + r.below(n - 1), false);
		if (HasFatalFailure()) return;
	}
	p.expectEqualFinal();
}
