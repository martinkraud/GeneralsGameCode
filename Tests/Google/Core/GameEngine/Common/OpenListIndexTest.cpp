// SPDX-License-Identifier: GPL-3.0-or-later
// X64.40: differential oracle for the open-list index. A verbatim model of the retail insertion walk (head-first, <= cost, capped) is driven side by side with the
// index-assisted insertion on identical operation sequences; after EVERY operation the complete list order must be identical.

#include <gtest/gtest.h>

#include "GameLogic/Pathfinder/OpenListIndex.h"

#include <cstdint>
#include <vector>

namespace
{
struct TNode
{
	TNode *prev = nullptr, *next = nullptr;
	unsigned cost = 0;       // current total cost (may change while linked, exactly like the engine does before it unlinks the node)
	unsigned key = 0;        // the cost under which the index registered the node
	bool linked = false;
	bool indexed = false;
	int id = 0;
};

struct Access
{
	unsigned cost(TNode *n) const { return n->cost; }
	TNode *next(TNode *n) const { return n->next; }
	bool visit(TNode *) const { return false; }
};

struct Lcg
{
	uint64_t s;
	explicit Lcg(uint64_t seed) : s(seed * 2862933555777941757ull + 3037000493ull) {}
	uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<uint32_t>(s >> 33); }
	unsigned below(unsigned n) { return next() % n; }
};

// the legacy list: verbatim walk
struct LegacyList
{
	TNode *head = nullptr;
	unsigned cap;
	explicit LegacyList(unsigned c) : cap(c) {}
	void insert(TNode *n)
	{
		n->linked = true;
		if (!head) { head = n; n->prev = n->next = nullptr; return; }
		TNode *current = head, *previous = nullptr;
		unsigned count = 0;
		while (current && count < cap && current->cost <= n->cost) { ++count; previous = current; current = current->next; }
		if (current) {
			if (current->prev) current->prev->next = n; else head = n;
			n->prev = current->prev;
			current->prev = n;
			n->next = current;
		} else {
			previous->next = n;
			n->prev = previous;
			n->next = nullptr;
		}
	}
	void remove(TNode *n)
	{
		if (n->next) n->next->prev = n->prev;
		if (n->prev) n->prev->next = n->next; else head = n->next;
		n->prev = n->next = nullptr;
		n->linked = false;
	}
};

// the accelerated list: same operations, engine-shaped bookkeeping
struct FastList
{
	TNode *head = nullptr;
	unsigned cap;
	OpenListIndex<TNode> index;
	unsigned walkedHops = 0, indexedInserts = 0, walkedInserts = 0;
	explicit FastList(unsigned c) : cap(c) {}
	void insert(TNode *n)
	{
		n->linked = true;
		n->indexed = false;
		if (!head) {
			index.clear();
			head = n; n->prev = n->next = nullptr;
			n->key = n->cost; index.noteInserted(n, n->key); n->indexed = true;
			return;
		}
		Access access;
		unsigned hops = 0;
		bool registerNew = false;
		TNode *previous = openListInsertionPredecessor(&index, head, n->cost, cap, access, &hops, &registerNew);
		walkedHops += hops;
		(hops ? walkedInserts : indexedInserts)++;
		TNode *current = previous ? previous->next : head;
		if (current) {
			if (current->prev) current->prev->next = n; else head = n;
			n->prev = current->prev;
			current->prev = n;
			n->next = current;
		} else {
			previous->next = n;
			n->prev = previous;
			n->next = nullptr;
		}
		if (registerNew) { n->key = n->cost; index.noteInserted(n, n->key); n->indexed = true; }
	}
	void remove(TNode *n)
	{
		if (index.sorted() && n->indexed) {
			index.noteRemoved(n, n->key, n->prev, n->prev ? n->prev->key : 0);
		}
		if (n->next) n->next->prev = n->prev;
		if (n->prev) n->prev->next = n->next; else head = n->next;
		n->prev = n->next = nullptr;
		n->linked = false;
		n->indexed = false;
		if (!head) index.clear();
	}
};

std::vector<int> order(TNode *head)
{
	std::vector<int> v;
	for (TNode *p = head; p; p = p->next) v.push_back(p->id);
	return v;
}

// one scenario: a random mix of inserts, head pops, arbitrary removals and the "improve a linked node" pattern
void runScenario(unsigned seed, unsigned operations, unsigned costRange, unsigned cap, bool clustered, bool checkEveryOp)
{
	Lcg r(seed);
	const unsigned poolSize = operations + 16;
	std::vector<TNode> a(poolSize), b(poolSize);
	for (unsigned i = 0; i < poolSize; ++i) a[i].id = b[i].id = static_cast<int>(i);
	LegacyList legacy(cap);
	FastList fast(cap);
	std::vector<unsigned> linked;     // ids currently linked
	unsigned nextFree = 0;
	unsigned lastCost = costRange / 2;
	for (unsigned op = 0; op < operations; ++op) {
		const unsigned pick = r.below(100);
		if ((pick < 62 || linked.empty()) && nextFree < poolSize) {
			unsigned cost;
			if (clustered) { const int delta = static_cast<int>(r.below(41)) - 12; int c = static_cast<int>(lastCost) + delta; if (c < 0) c = 0; if (c >= static_cast<int>(costRange)) c = static_cast<int>(costRange) - 1; cost = static_cast<unsigned>(c); lastCost = cost; }
			else cost = r.below(costRange);
			const unsigned id = nextFree++;
			a[id].cost = b[id].cost = cost;
			legacy.insert(&a[id]); fast.insert(&b[id]);
			linked.push_back(id);
		} else if (pick < 80 && !linked.empty()) {
			// pop the head (the A* loop)
			const int headId = legacy.head->id;
			ASSERT_EQ(headId, fast.head->id);
			legacy.remove(&a[headId]); fast.remove(&b[headId]);
			for (size_t i = 0; i < linked.size(); ++i) if (static_cast<int>(linked[i]) == headId) { linked[i] = linked.back(); linked.pop_back(); break; }
		} else if (pick < 90 && !linked.empty()) {
			// remove an arbitrary node
			const size_t at = r.below(static_cast<unsigned>(linked.size()));
			const unsigned id = linked[at];
			legacy.remove(&a[id]); fast.remove(&b[id]);
			linked[at] = linked.back(); linked.pop_back();
		} else if (!linked.empty()) {
			// the engine improves a linked node: it changes the cost FIRST, then unlinks, then re-inserts
			const size_t at = r.below(static_cast<unsigned>(linked.size()));
			const unsigned id = linked[at];
			const unsigned newCost = r.below(costRange);
			a[id].cost = b[id].cost = newCost;
			legacy.remove(&a[id]); fast.remove(&b[id]);
			legacy.insert(&a[id]); fast.insert(&b[id]);
		}
		if (checkEveryOp || op + 1 == operations) {
			ASSERT_EQ(order(legacy.head), order(fast.head)) << "seed " << seed << " op " << op;
		}
	}
}
}  // namespace

TEST(OpenListIndex, EmptyAndSingleNode)
{
	OpenListIndex<TNode> idx;
	EXPECT_EQ(idx.lastAtOrBelow(0), nullptr);
	EXPECT_EQ(idx.lastAtOrBelow(65535), nullptr);
	TNode n; n.cost = 70;
	idx.noteInserted(&n, 70);
	EXPECT_EQ(idx.lastAtOrBelow(69), nullptr);
	EXPECT_EQ(idx.lastAtOrBelow(70), &n);
	EXPECT_EQ(idx.lastAtOrBelow(65535), &n);
	idx.noteRemoved(&n, 70, nullptr, 0);
	EXPECT_EQ(idx.lastAtOrBelow(65535), nullptr);
	EXPECT_EQ(idx.count(), 0u);
}

TEST(OpenListIndex, PredecessorSearchAcrossWordAndGroupBoundaries)
{
	OpenListIndex<TNode> idx;
	const unsigned keys[] = {0, 1, 63, 64, 65, 4095, 4096, 4097, 8191, 40000, 65534, 65535};
	std::vector<TNode> nodes(sizeof(keys) / sizeof(keys[0]));
	for (size_t i = 0; i < nodes.size(); ++i) { nodes[i].cost = keys[i]; nodes[i].id = static_cast<int>(i); idx.noteInserted(&nodes[i], keys[i]); }
	for (unsigned probe = 0; probe < 65536; probe += 7) {
		TNode *expected = nullptr;
		for (size_t i = 0; i < nodes.size(); ++i) if (keys[i] <= probe) expected = &nodes[i];
		ASSERT_EQ(idx.lastAtOrBelow(probe), expected) << probe;
	}
	for (unsigned probe : {0u, 62u, 63u, 64u, 4094u, 4095u, 4096u, 8190u, 8191u, 39999u, 40000u, 65533u, 65534u, 65535u}) {
		TNode *expected = nullptr;
		for (size_t i = 0; i < nodes.size(); ++i) if (keys[i] <= probe) expected = &nodes[i];
		ASSERT_EQ(idx.lastAtOrBelow(probe), expected) << probe;
	}
}

TEST(OpenListIndex, RunTailFollowsRemovals)
{
	OpenListIndex<TNode> idx;
	TNode a, b, c;
	idx.noteInserted(&a, 10);
	idx.noteInserted(&b, 10);          // b is now the last node of the run
	idx.noteInserted(&c, 10);
	EXPECT_EQ(idx.lastAtOrBelow(10), &c);
	idx.noteRemoved(&c, 10, &b, 10);   // the tail is removed: its predecessor of the same cost takes over
	EXPECT_EQ(idx.lastAtOrBelow(10), &b);
	idx.noteRemoved(&a, 10, nullptr, 0); // a non-tail node leaves: no change
	EXPECT_EQ(idx.lastAtOrBelow(10), &b);
	idx.noteRemoved(&b, 10, nullptr, 0); // the run is empty
	EXPECT_EQ(idx.lastAtOrBelow(10), nullptr);
}

TEST(OpenListIndex, DifferentialRandomClusteredAndUniformCosts)
{
	for (unsigned seed = 1; seed <= 30; ++seed) {
		runScenario(seed, 3000, 400, 5000, seed % 2 == 0, true);
		if (HasFatalFailure()) return;
	}
}

TEST(OpenListIndex, DifferentialManyTiesAndWideRange)
{
	for (unsigned seed = 100; seed < 112; ++seed) {
		runScenario(seed, 4000, 3, 5000, false, true);      // plateaus of equal cost
		if (HasFatalFailure()) return;
		runScenario(seed + 50, 4000, 65536, 5000, false, true);   // the whole 16-bit range
		if (HasFatalFailure()) return;
	}
}

TEST(OpenListIndex, DifferentialWithTheCapReachedManyTimes)
{
	// a tiny cap makes the walk stop at the cap constantly; the index must hand over to the walk and mark itself unsorted without ever diverging
	for (unsigned cap : {1u, 2u, 3u, 7u, 25u, 60u}) {
		for (unsigned seed = 200; seed < 210; ++seed) {
			runScenario(seed + cap, 2500, 50, cap, seed % 2 == 0, true);
			if (HasFatalFailure()) return;
		}
	}
}

TEST(OpenListIndex, DifferentialLongListsAtTheRealCap)
{
	// lists that grow past the real 5000-node cap: the index is used below the cap, the walk above it, and the result must match throughout
	for (unsigned seed = 300; seed < 304; ++seed) {
		Lcg r(seed);
		const unsigned n = 14000;
		std::vector<TNode> a(n), b(n);
		for (unsigned i = 0; i < n; ++i) a[i].id = b[i].id = static_cast<int>(i);
		LegacyList legacy(5000);
		FastList fast(5000);
		unsigned popped = 0;
		for (unsigned i = 0; i < n; ++i) {
			const unsigned cost = (seed % 2) ? r.below(200) : 100 + r.below(4);
			a[i].cost = b[i].cost = cost;
			legacy.insert(&a[i]); fast.insert(&b[i]);
			if (i % 5 == 4) {      // pop some so the size hovers around and across the cap
				const int headId = legacy.head->id;
				legacy.remove(&a[headId]); fast.remove(&b[headId]);
				++popped;
			}
			if (i % 997 == 0) ASSERT_EQ(order(legacy.head), order(fast.head)) << "seed " << seed << " i " << i;
		}
		ASSERT_EQ(order(legacy.head), order(fast.head));
	}
}

TEST(OpenListIndex, TheIndexActuallySavesTheWalk)
{
	// a sorted list of 3000 nodes with insertions near the end: the walk would do thousands of hops, the index none
	FastList fast(5000);
	std::vector<TNode> nodes(3100);
	for (unsigned i = 0; i < 3000; ++i) { nodes[i].cost = i; nodes[i].id = static_cast<int>(i); fast.insert(&nodes[i]); }
	unsigned before = fast.walkedHops;
	for (unsigned i = 3000; i < 3100; ++i) { nodes[i].cost = 2990 + (i % 20); nodes[i].id = static_cast<int>(i); fast.insert(&nodes[i]); }
	EXPECT_EQ(fast.walkedHops, before);
	EXPECT_GE(fast.indexedInserts, 3000u);
}
