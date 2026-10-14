// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 closeout: the understated footer shown by the Modern Experience (Options and Main Menu version labels).
// Classic keeps the retail-style version label. The full product/version/hash string is still written to the log
// (checkpoint "version") for diagnostics. Not a logo, not an official-release claim.
#pragma once

#include "Common/DisplayRuntime.h"

namespace ModernBranding
{
inline const wchar_t *footerText()
{
	return L"Modern Experience by MKRaudstein | 64-bit • DirectX 11";
}

inline bool useModernFooter()
{
	return DisplayRuntime::modern();
}
}
