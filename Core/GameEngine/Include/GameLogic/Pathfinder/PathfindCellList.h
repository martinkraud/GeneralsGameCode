/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

class PathfindCell;

#include "GameLogic/Pathfinder/OpenListIndex.h"

// TheSuperHackers @info The PathfindCellList class acts as a new management class for the pathfindcell open and closed lists
class PathfindCellList
{
	friend struct RetailOpenListTestAccess; // Test-only access; no runtime state.
	friend class PathfindCell;

public:
	PathfindCellList() : m_head(nullptr), m_tail(nullptr), m_index(nullptr), m_generation(1) {}

#if RETAIL_COMPATIBLE_PATHFINDING
	void reset(PathfindCell* newHead = nullptr);   // X64.40: defined in AIPathfind.cpp (restarts the open-list index together with the list)
	void attachIndex(OpenListIndex<PathfindCell>* index) { m_index = index; }
#else
	void reset() { m_head = nullptr; m_tail = nullptr; ++m_generation; }
#endif

	PathfindCell* getHead() const { return m_head; }
	// X64.41: increases with every reset, i.e. with every new search; per-search memos are valid only for the generation they were written in
	UnsignedInt searchGeneration() const { return m_generation; }

	Bool empty() const { return m_head == nullptr; }

	Bool canReverseSort(PathfindCell& currentCell) const;

private:
	PathfindCell* m_head;
	PathfindCell* m_tail;
	UnsignedInt m_generation;
	OpenListIndex<PathfindCell>* m_index;   // X64.40: exact accelerator of the retail insertion walk (null = the original walk only)
};
