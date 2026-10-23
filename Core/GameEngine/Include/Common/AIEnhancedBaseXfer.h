// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52: save / load of the Enhanced base policy state (AIPlayer xfer version 3). Kept apart from AIEnhancedBase.h so that header stays engine-free; shared by the
// engine (AIPlayer::enhancedXferState) and the round-trip unit test.
#pragma once

#include "Common/AIEnhancedBase.h"
#include "Common/Xfer.h"

namespace AIEnhancedBase
{
inline void xferState(Xfer *xfer, State &s)
{
	xfer->xferUnsignedInt(&s.seed);
	xfer->xferInt(&s.layoutVariant);
	xfer->xferInt(&s.recoveryGrants);
	xfer->xferInt(&s.threatDefenses);
	xfer->xferInt(&s.expansions);
	xfer->xferInt(&s.expansionFailures);
	xfer->xferInt(&s.arcSector);
	xfer->xferUnsignedInt(&s.lastThreatScan);
	xfer->xferUnsignedInt(&s.lastThreatDefense);
	xfer->xferUnsignedInt(&s.lastExpansion);
	xfer->xferUnsignedInt(&s.lastRecovery);
	for (int i = 0; i < State::kFailSlots; ++i) { xfer->xferUnsignedInt(&s.failKey[i]); xfer->xferUnsignedInt(&s.failFrame[i]); }
	xfer->xferInt(&s.failNext);
	xfer->xferUnsignedInt(&s.stuckKey);
	xfer->xferUnsignedInt(&s.stuckSince);
	for (int i = 0; i < ThreatMap::kSectors; ++i) xfer->xferInt(&s.threat.weight[i]);
}
}
