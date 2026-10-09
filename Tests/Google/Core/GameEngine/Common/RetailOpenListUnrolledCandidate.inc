// Test-only: two exact original steps; same intervening stop condition and repairs.
{
	DEBUG_ASSERTCRASH(m_info, ("Has to have info."));
	DEBUG_ASSERTCRASH(m_info->m_closed == FALSE && m_info->m_open == FALSE, ("Serious error - Invalid flags. jba"));

	// mark the newCell as being on the open list
	m_info->m_open = true;
	m_info->m_closed = false;

	if (list.m_head == nullptr)
	{
		list.m_head = self;
		m_info->m_prevOpen = nullptr;
		m_info->m_nextOpen = nullptr;
		return;
	}

	// insertion sort
	PathfindCell* currentCell = list.m_head;
	PathfindCell* previousCell = nullptr;
	UnsignedInt cellCount = 0;
	while (currentCell && cellCount < PATHFIND_CELLS_PER_FRAME && currentCell->m_info->m_totalCost <= m_info->m_totalCost)
	{
		// Prevent a retail crash where a pathfindCell has an m_info with a dangling nextOpen pointer
		if (currentCell->m_info->m_nextOpen && !currentCell->m_info->m_nextOpen->m_cell->m_info)
		{
			currentCell->m_info->m_nextOpen->m_cell = nullptr;
			currentCell->m_info->m_nextOpen = nullptr;
		}

		cellCount++;
		previousCell = currentCell;
		currentCell = currentCell->getNextOpen();
		if (!(currentCell && cellCount < PATHFIND_CELLS_PER_FRAME && currentCell->m_info->m_totalCost <= m_info->m_totalCost)) break;
		// Prevent a retail crash where a pathfindCell has an m_info with a dangling nextOpen pointer
		if (currentCell->m_info->m_nextOpen && !currentCell->m_info->m_nextOpen->m_cell->m_info)
		{
			currentCell->m_info->m_nextOpen->m_cell = nullptr;
			currentCell->m_info->m_nextOpen = nullptr;
		}

		cellCount++;
		previousCell = currentCell;
		currentCell = currentCell->getNextOpen();
	}
	PerformanceProfile::pathCount(PerformanceProfile::PathWork::ForwardHops,cellCount);

	if (currentCell)
	{
		// insert just before "currentCell"
		if (currentCell->m_info->m_prevOpen)
			currentCell->m_info->m_prevOpen->m_nextOpen = self->m_info;
		else
			list.m_head = self;

		m_info->m_prevOpen = currentCell->m_info->m_prevOpen;
		currentCell->m_info->m_prevOpen = self->m_info;

		m_info->m_nextOpen = currentCell->m_info;

	}
	else
	{
		// append after "previousCell" - we are at the end of the list
		previousCell->m_info->m_nextOpen = self->m_info;
		m_info->m_prevOpen = previousCell->m_info;
		m_info->m_nextOpen = nullptr;
	}
}