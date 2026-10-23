// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52A: save / load of the Superweapons match rule (SkirmishGameInfo xfer version 6). Kept apart from SuperweaponLimit.h so that header stays engine-free; shared by
// SkirmishGameInfo::xfer and the round-trip unit test.
#pragma once

#include "Common/SuperweaponLimit.h"
#include "Common/Xfer.h"

namespace SuperweaponLimit
{
// version >= 6: the value is part of the stream (an invalid value in a damaged / foreign file reads as Unlimited, never as a restriction nobody chose);
// older versions have no such field: loading one leaves the rule Unlimited. Saving below version 6 writes nothing (the caller only chooses a lower version for Unlimited).
inline void xferLimit(Xfer *xfer, UnsignedShort &limit, XferVersion version)
{
	if (version >= 6)
	{
		UnsignedShort value = limit;
		xfer->xferUnsignedShort(&value);
		limit = SuperweaponLimit::isValid(value) ? value : SuperweaponLimit::kUnlimited;
	}
	else if (xfer->getXferMode() == XFER_LOAD)
	{
		limit = SuperweaponLimit::kUnlimited;
	}
}
}
