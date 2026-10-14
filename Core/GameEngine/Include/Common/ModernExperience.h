// SPDX-License-Identifier: GPL-3.0-or-later
// X64.32: single switch for the project's opt-out "modern experience" extensions.
//
// Generals' default is the original game plus a small number of quality-of-life extensions that never alter
// game rules (Starting Cash on the Skirmish screen; the shortcut-sidebar overlay is staged data, see
// scripts/native-runtime/modern_extensions.py). `-classic` on the command line turns every code-level extension
// off; staging with -Classic omits the data overlay. Future settings (Options > Gameplay) will drive the same flag.
#pragma once

namespace ModernExperience
{
enum Feature
{
	FEATURE_STARTING_CASH,   // Skirmish Starting Cash selector (Zero Hour semantics, Generals default amount)
	FEATURE_MODERN_DISPLAY,  // X64.33: native-resolution window/swap chain, borderless, UI scale + anchors, Hor+ camera,
	                         //         VSync and Options > Display Options (normal launch only; see DisplayRuntime.h)
	FEATURE_SMOOTH_MOTION,   // X64.33: Options > Unit Motion (ground interpolation); off by default until native-accepted
	FEATURE_COUNT
};

inline bool &classicFlag()
{
	static bool classic = false;
	return classic;
}

inline void setClassic(bool classic) { classicFlag() = classic; }
inline bool classic() { return classicFlag(); }
inline bool enabled(Feature feature) { return feature >= 0 && feature < FEATURE_COUNT && !classicFlag(); }
}
