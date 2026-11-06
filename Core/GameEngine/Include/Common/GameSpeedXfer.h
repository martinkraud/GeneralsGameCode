// SPDX-License-Identifier: GPL-3.0-or-later
// Save / load of the Game Speed match rule (SkirmishGameInfo xfer version 7). Kept apart from GameSpeed.h so that header stays engine-free.
#pragma once

#include "Common/GameSpeed.h"
#include "Common/Xfer.h"

namespace GameSpeed
{
// version >= 7: the value is part of the stream (an invalid value in a damaged / foreign file reads as Normal, never as a pace nobody chose); older versions have no such
// field: loading one leaves Normal. A save below version 7 writes nothing (the caller only chooses a lower version for Normal).
inline void xferPercent(Xfer *xfer, UnsignedShort &percent, XferVersion version)
{
	if (version >= 7)
	{
		UnsignedShort value = percent;
		xfer->xferUnsignedShort(&value);
		percent = static_cast<UnsignedShort>(GameSpeed::sanitize(value));
	}
	else if (xfer->getXferMode() == XFER_LOAD)
	{
		percent = GameSpeed::kNormal;
	}
}
}
