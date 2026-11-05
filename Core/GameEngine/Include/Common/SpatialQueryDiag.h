// SPDX-License-Identifier: GPL-3.0-or-later
// X64.sc: opt-in characterisation of PartitionManager::getClosestObjects (GENERALS_SIM_GCO=<output file>). Read-only: it counts what a query touched and who asked; it never changes
// what a query returns. When the variable is not set, a query pays for a few local integer increments and one boolean test.
#pragma once

namespace SpatialQueryDiag
{
struct QueryStats
{
	unsigned cells = 0;        ///< partition cells opened
	unsigned cois = 0;         ///< object-in-cell entries looked at
	unsigned doneSkips = 0;    ///< entries skipped because the object was already seen in this query (multi-cell objects)
	unsigned distPass = 0;     ///< objects that were inside the current best distance
	unsigned filterPass = 0;   ///< ... and that the filters also accepted
	unsigned ringsDone = 0;    ///< last ring radius opened
};

extern bool g_on;
inline bool enabled() { return g_on; }
unsigned long long now();

/// One finished query. identity is the asking object (or null for a position query); filterTypes are the vtable pointers of the filters (a filter-set fingerprint).
void record(unsigned frame, unsigned worldObjects, const void *identity, float px, float py, float maxDist, int distanceType, bool collectsAll,
	const void *const *filterTypes, int filterCount, const QueryStats &stats, unsigned long long ticks);
void report();   ///< writes the tables to the file named by GENERALS_SIM_GCO (no-op when not enabled)
}
