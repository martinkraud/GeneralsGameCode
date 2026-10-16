// SPDX-License-Identifier: GPL-3.0-or-later
// X64.40: an exact accelerator for the retail-compatible A* open list (pure, engine-free, header-only).
//
// The retail open list is a doubly linked list kept in ascending total-cost order by an insertion sort that starts at the HEAD every time and walks
// past every node whose cost is <= the new cost, but at most PATHFIND_CELLS_PER_FRAME (5000) nodes. In the big failing searches of the heavy benchmark
// that walk is ~80 % of the A* time (a single search performs 19 million hops).
//
// EXACTNESS ARGUMENT. While the list is sorted (it starts as one node or empty, insertion at the upper bound and removal preserve sortedness) and has fewer
// than 5000 nodes, the walk can never reach its 5000-node cap, so it ends at the UPPER BOUND of the new cost: directly after the last node whose cost is
// <= the new cost (or at the head when none is). This index finds that node without walking: it keeps, per cost value (16 bit), whether the cost is present
// and the LAST node of that run, plus a two-level bit set to find "the largest present cost <= c". Insertion registers the new node as the last of its run
// (it was just placed after every node <= its cost). Removal updates the run tail. Whenever the walk could be affected by the cap (5000 or more nodes) the
// caller uses the original walk instead, and if that walk ever stops at the cap the list may be out of order, so the index is marked UNSORTED and is not used
// again until the list is reset. The caller therefore reproduces the legacy list evolution exactly, at every step.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace OpenListIndexDetail
{
inline int highestBit32(uint32_t v)       // v != 0
{
#if defined(_MSC_VER)
	unsigned long index;
	_BitScanReverse(&index, v);
	return static_cast<int>(index);
#else
	return 31 - __builtin_clz(v);
#endif
}
inline int highestBit64(uint64_t v)       // v != 0
{
	const uint32_t high = static_cast<uint32_t>(v >> 32);
	return high ? 32 + highestBit32(high) : highestBit32(static_cast<uint32_t>(v));
}
}  // namespace OpenListIndexDetail

template <class Node>
class OpenListIndex
{
public:
	static const unsigned kKeys = 65536u;

	OpenListIndex() : m_tail(kKeys, nullptr) { clear(); }

	// The list is empty (or being restarted): forget everything.
	void clear()
	{
		std::memset(m_bits, 0, sizeof(m_bits));
		std::memset(m_summary, 0, sizeof(m_summary));
		m_count = 0;
		m_sorted = true;
	}

	unsigned count() const { return m_count; }
	bool sorted() const { return m_sorted; }
	void markUnsorted() { m_sorted = false; }

	// The last node of the largest present cost <= key, or nullptr when every present cost is greater (insert at the head).
	Node *lastAtOrBelow(unsigned key) const
	{
		const int cost = largestPresentAtOrBelow(key);
		return cost < 0 ? nullptr : m_tail[static_cast<unsigned>(cost)];
	}

	// `node` was just linked as the last node of its cost run.
	void noteInserted(Node *node, unsigned key)
	{
		m_bits[key >> 6] |= (uint64_t(1) << (key & 63));
		m_summary[key >> 12] |= (uint64_t(1) << ((key >> 6) & 63));
		m_tail[key] = node;
		++m_count;
	}

	// `node` (registered under `key`) is being unlinked; `prev` is its predecessor in the list (or nullptr) and `prevKey` the key that predecessor is registered under.
	void noteRemoved(Node *node, unsigned key, Node *prev, unsigned prevKey)
	{
		if (m_tail[key] == node) {
			if (prev && prevKey == key) {
				m_tail[key] = prev;
			} else {
				m_bits[key >> 6] &= ~(uint64_t(1) << (key & 63));
				if (m_bits[key >> 6] == 0) m_summary[key >> 12] &= ~(uint64_t(1) << ((key >> 6) & 63));
			}
		}
		if (m_count > 0) --m_count;
	}

private:
	// largest present cost <= key, or -1
	int largestPresentAtOrBelow(unsigned key) const
	{
		unsigned word = key >> 6;
		const uint64_t here = m_bits[word] & (~uint64_t(0) >> (63 - (key & 63)));
		if (here) return static_cast<int>((word << 6) + OpenListIndexDetail::highestBit64(here));
		// previous non-empty word: first inside the same 64-word summary group, then earlier groups
		unsigned group = word >> 6;
		const unsigned bit = word & 63;
		uint64_t below = bit ? (m_summary[group] & ((uint64_t(1) << bit) - 1)) : 0;
		while (true) {
			if (below) {
				const unsigned w = (group << 6) + static_cast<unsigned>(OpenListIndexDetail::highestBit64(below));
				return static_cast<int>((w << 6) + OpenListIndexDetail::highestBit64(m_bits[w]));
			}
			if (group == 0) return -1;
			--group;
			below = m_summary[group];
		}
	}

	uint64_t m_bits[kKeys / 64];                // present costs
	uint64_t m_summary[kKeys / 64 / 64];        // non-empty words of m_bits
	std::vector<Node *> m_tail;                 // last node of each present cost run (valid only where the bit is set)
	unsigned m_count;
	bool m_sorted;
};

// ---- the insertion decision, shared by the engine and the tests -----------------------------------------------------------------------------
//
// Returns the node after which the new node (total cost `cost`) must be linked (nullptr = at the head of a NON-empty list `head`), exactly where the
// legacy walk would put it. `access` provides: unsigned cost(Node*), Node *next(Node*), bool visit(Node*) (called for every node the walk passes, before
// stepping on; the engine uses it for the legacy dangling-pointer repair and returns true when it had to repair anything).
// `*hops` is the number of nodes walked (0 when the index answered), `*registerNew` tells the caller whether to register the new node with the index.
template <class Node, class Access>
Node *openListInsertionPredecessor(OpenListIndex<Node> *index, Node *head, unsigned cost, unsigned cap, Access &access, unsigned *hops, bool *registerNew)
{
	if (index && index->sorted() && index->count() < cap) {
		// fewer than `cap` nodes: the walk cannot reach its cap, so it ends at the upper bound of `cost`
		*hops = 0;
		*registerNew = true;
		return index->lastAtOrBelow(cost);
	}
	Node *current = head;
	Node *previous = nullptr;
	unsigned count = 0;
	bool repaired = false;
	while (current && count < cap && access.cost(current) <= cost) {
		if (access.visit(current)) repaired = true;
		++count;
		previous = current;
		current = access.next(current);
	}
	*hops = count;
	*registerNew = false;
	if (index && index->sorted()) {
		if (count >= cap || repaired) {
			index->markUnsorted();       // the cap (or a repair) may have left the list out of order / truncated
		} else {
			*registerNew = true;         // the walk ended at the upper bound: the list stays sorted
		}
	}
	return previous;
}
