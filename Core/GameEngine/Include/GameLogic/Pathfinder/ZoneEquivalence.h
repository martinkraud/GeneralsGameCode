// SPDX-License-Identifier: GPL-3.0-or-later
// X64.39: zone-equivalence primitives of the pathfinder's zone calculation (pure, engine-free, header-only).
//
// PathfindZoneManager::calculateZones and ZoneBlock::blockCalculateZones merge "zones" (connected areas of one terrain type) through
// equivalence arrays E (UnsignedShort, one entry per zone number). Two implementations of the merge primitives live here:
//
//   * LEGACY    - the original code, verbatim: merging two classes scans the WHOLE array and rewrites every entry of one class. O(n) per merge.
//   * MINROOT   - a disjoint-set (union-find) over the same array: the root of a class is always its smallest member, find uses path halving,
//                 union keeps the smaller root. Near-constant per merge; the array must be flattened (E[i] = find(i)) before anyone reads
//                 E[i] as a final label.
//
// PROVEN INVARIANT (see X64_39_ZONECALC_OPTIMIZATION.md): starting from the identity (E[i] = i), after ANY sequence of legacy merges every
// entry E[i] equals the SMALLEST index of i's equivalence class, and E[label] = label. A merge of the classes labelled a < b rewrites every
// entry equal to b with a, which is again the minimum of the union. Min-root union-find computes exactly that minimum, so after flattening the
// two arrays are bit-identical for the same merge sequence, and the result is independent of the order of merges.
//
// The one place where the invariant does not hold is flattenZones() (its first loop rewrites entries through a second table, so a later
// legacy merge can see a non-idempotent array); that function is therefore kept LEGACY here and reported separately.
#pragma once

namespace ZoneEquivalence
{
// ---- LEGACY (verbatim semantics of AIPathfind.cpp before X64.39) --------------------------------------------------------------------

// resolveBlockZones: srcZone / targetZone are already class labels; keep the lower one.
template <class T>
inline void legacyResolveBlockZones(int srcZone, int targetZone, T *E, int sizeOfZE)
{
	if (targetZone < srcZone) {
		for (int i = 0; i < sizeOfZE; i++) {
			if (E[i] == srcZone) E[i] = static_cast<T>(targetZone);
		}
	} else {
		for (int i = 0; i < sizeOfZE; i++) {
			if (E[i] == targetZone) E[i] = static_cast<T>(srcZone);
		}
	}
}

// resolveZones: arguments are raw entry indices; both are mapped through E first.
template <class T>
inline void legacyResolveZones(int srcZone, int targetZone, T *E, int sizeOfZE)
{
	srcZone = E[srcZone];
	targetZone = E[targetZone];
	const int finalZone = (targetZone < srcZone) ? static_cast<int>(E[targetZone]) : static_cast<int>(E[srcZone]);
	const T finalValue = static_cast<T>(finalZone);
	for (int i = 0; i < sizeOfZE; i++) {
		const int ze = E[i];
		if (ze == targetZone || ze == srcZone) E[i] = finalValue;
	}
}

// ---- MINROOT (union-find) ----------------------------------------------------------------------------------------------------------

// Root (= smallest member) of the class of x; path halving. E is used as the parent array.
template <class T>
inline int minRootFind(T *E, int x)
{
	while (E[x] != x) {
		E[x] = E[E[x]];
		x = E[x];
	}
	return x;
}

// Merge the classes of a and b; the smaller root survives. Returns true when two different classes were merged.
template <class T>
inline bool minRootUnion(T *E, int a, int b)
{
	const int ra = minRootFind(E, a);
	const int rb = minRootFind(E, b);
	if (ra == rb) return false;
	if (ra < rb) E[rb] = static_cast<T>(ra);
	else E[ra] = static_cast<T>(rb);
	return true;
}

// E[i] = find(i) for every entry: afterwards the array is exactly the legacy array.
template <class T>
inline void minRootFlatten(T *E, int sizeOfZE)
{
	for (int i = 0; i < sizeOfZE; i++) {
		E[i] = static_cast<T>(minRootFind(E, i));
	}
}

// Block tables store ABSOLUTE labels (E[i] = i + firstZone initially); the same operations on the relative index.
template <class T>
inline int minRootFindBlock(T *E, int firstZone, int absoluteZone)
{
	int x = absoluteZone - firstZone;
	while (E[x] != x + firstZone) {
		E[x] = E[E[x] - firstZone];
		x = E[x] - firstZone;
	}
	return x + firstZone;
}

template <class T>
inline bool minRootUnionBlock(T *E, int firstZone, int absoluteA, int absoluteB)
{
	const int ra = minRootFindBlock(E, firstZone, absoluteA);
	const int rb = minRootFindBlock(E, firstZone, absoluteB);
	if (ra == rb) return false;
	if (ra < rb) E[rb - firstZone] = static_cast<T>(ra);
	else E[ra - firstZone] = static_cast<T>(rb);
	return true;
}

template <class T>
inline void minRootFlattenBlock(T *E, int firstZone, int sizeOfZE)
{
	for (int i = 0; i < sizeOfZE; i++) {
		E[i] = static_cast<T>(minRootFindBlock(E, firstZone, i + firstZone));
	}
}

// ---- the three call shapes used by the zone calculation, in both modes ------------------------------------------------------------

// applyZone for the label scan and the pair pass: the target cell's zone is `targetCellZone`, the neighbour's is `sourceCellZone`.
// Returns 1 when the target cell must take `*newTargetZone` (target had no zone yet), 0 otherwise (a merge may have happened).
template <class T>
inline int legacyApply(T *E, int sizeOfZE, int targetCellZone, int sourceCellZone, int *newTargetZone)
{
	const int srcZone = E[sourceCellZone];
	const int targetZone = E[targetCellZone];
	if (targetZone == 0) { *newTargetZone = srcZone; return 1; }
	if (targetZone == srcZone) return 0;
	legacyResolveZones(srcZone, targetZone, E, sizeOfZE);
	return 0;
}

template <class T>
inline int minRootApply(T *E, int /*sizeOfZE*/, int targetCellZone, int sourceCellZone, int *newTargetZone)
{
	const int srcZone = minRootFind(E, sourceCellZone);
	const int targetZone = minRootFind(E, targetCellZone);
	if (targetZone == 0) { *newTargetZone = srcZone; return 1; }
	if (targetZone == srcZone) return 0;
	if (srcZone < targetZone) E[targetZone] = static_cast<T>(srcZone);
	else E[srcZone] = static_cast<T>(targetZone);
	return 0;
}

// applyBlockZone: the arrays are indexed relative to firstZone and hold absolute labels.
template <class T>
inline void legacyApplyBlock(T *E, int firstZone, int sizeOfZE, int targetCellZone, int sourceCellZone)
{
	const int srcZone = E[sourceCellZone - firstZone];
	const int targetZone = E[targetCellZone - firstZone];
	if (targetZone == srcZone) return;
	legacyResolveBlockZones(srcZone, targetZone, E, sizeOfZE);
}

template <class T>
inline void minRootApplyBlock(T *E, int firstZone, int /*sizeOfZE*/, int targetCellZone, int sourceCellZone)
{
	minRootUnionBlock(E, firstZone, sourceCellZone, targetCellZone);
}

// ---- flattenZones (LEGACY only; its first loop breaks the invariant, see the header comment) ----------------------------------------

template <class T>
inline void legacyFlatten(T *zoneArray, const T *zoneHierarchical, int sizeOfZones)
{
	for (int i = 0; i < sizeOfZones; i++) {
		int zone1 = zoneArray[i];
		int zone2 = zoneHierarchical[zone1];
		zone1 = zoneArray[zone2];
		zone2 = zoneHierarchical[zone1];
		zoneArray[i] = static_cast<T>(zone2);
	}
	for (int i = 0; i < sizeOfZones; i++) {
		const int zone1 = zoneArray[i];
		const int zone2 = zoneHierarchical[i];
		if (zone1 != zone2) {
			legacyResolveZones(zone1, zone2, zoneArray, sizeOfZones);
		}
	}
}
}  // namespace ZoneEquivalence
