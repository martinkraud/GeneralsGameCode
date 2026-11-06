// SPDX-License-Identifier: GPL-3.0-or-later
// Game Speed match rule (Skirmish): how fast authoritative game time advances relative to wall-clock time. It is the pacing of the logic scheduler only:
// one logic frame keeps its meaning (1/30 game second, the retail cadence), every gameplay quantity stays a function of logic frames, and the simulation result after N
// frames is identical at every speed (and at every render frame rate). Normal (100 %) is exactly the original 30 logic frames per second.
//
// The speed is stored as a PERCENT of Normal (the logic frames per second are 30 x percent / 100): 75 Slow, 100 Normal, 125 Fast, 150 Very Fast. Exposed levels are decided by
// measurement (docs/modernization/GAME_SPEED.md); 50 and 200 are valid to the parsers only if listed in kLevels.
//
// Pure helpers only (no engine state), shared by the match setup, the options string / replay header, the save xfer and the unit tests.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace GameSpeed
{
constexpr uint16_t kNormal = 100;                       // 1.00x = the original 30 logic frames per second (the default; never written to a string or a replay header)
constexpr int kBaseLogicFps = 30;                       // LOGICFRAMES_PER_SECOND

struct Level { uint16_t percent; const char *prefText; const char *labelKey; const wchar_t *fallback; };
constexpr Level kLevels[] = {
	{ 75,  "Slow",     "GUI:GameSpeedSlow",     L"Slow - 0.75x" },
	{ 100, "Normal",   "GUI:GameSpeedNormal",   L"Normal - 1.00x" },
	{ 125, "Fast",     "GUI:GameSpeedFast",     L"Fast - 1.25x" },
	{ 150, "VeryFast", "GUI:GameSpeedVeryFast", L"Very Fast - 1.50x" },
};
constexpr int kLevelCount = static_cast<int>(sizeof(kLevels) / sizeof(kLevels[0]));

inline bool isValid(unsigned percent) { for (const Level &l : kLevels) if (l.percent == percent) return true; return false; }
inline bool isActive(unsigned percent) { return percent != kNormal; }
inline unsigned sanitize(unsigned percent) { return isValid(percent) ? percent : kNormal; }
inline int levelIndex(unsigned percent) { for (int i = 0; i < kLevelCount; ++i) if (kLevels[i].percent == percent) return i; return -1; }

// Logic frames per second at a speed (exact; 22.5 / 37.5 are not rounded) and the wall-clock seconds one logic frame takes.
inline double logicFps(unsigned percent) { return kBaseLogicFps * static_cast<double>(sanitize(percent)) / 100.0; }
inline double secondsPerFrame(unsigned percent) { return 1.0 / logicFps(percent); }

// The next / previous exposed level for the runtime Speed Up / Speed Down (clamped at the ends: no wrap, no arbitrary values).
inline unsigned step(unsigned percent, int direction)
{
	int i = levelIndex(percent);
	if (i < 0) i = levelIndex(kNormal);
	i += direction > 0 ? 1 : (direction < 0 ? -1 : 0);
	if (i < 0) i = 0;
	if (i >= kLevelCount) i = kLevelCount - 1;
	return kLevels[i].percent;
}

// options string / replay header: key "GS", written only for a non-Normal value ("GS=75", "GS=125", "GS=150"); anything else is refused on read.
inline const char *optionKey() { return "GS"; }
inline bool formatOptionValue(unsigned percent, char *out, size_t size)
{
	if (!isValid(percent) || percent == kNormal || size < 4) return false;
	out[0] = static_cast<char>('0' + percent / 100 % 10);
	out[1] = static_cast<char>('0' + percent / 10 % 10);
	out[2] = static_cast<char>('0' + percent % 10);
	out[3] = '\0';
	if (out[0] == '0') { out[0] = out[1]; out[1] = out[2]; out[2] = '\0'; }
	return true;
}
inline bool parseOptionValue(const char *text, uint16_t &percent)
{
	if (!text || !text[0]) return false;
	unsigned v = 0;
	for (const char *p = text; *p; ++p)
	{
		if (*p < '0' || *p > '9' || v > 999) return false;
		v = v * 10 + static_cast<unsigned>(*p - '0');
	}
	if (v == kNormal || !isValid(v)) return false;   // "100" is never written; other numbers are not selectable levels
	percent = static_cast<uint16_t>(v);
	return true;
}

// Developer / test aid (like GENERALS_AUTOFLOW_*): GENERALS_FORCE_GAME_SPEED=<percent> makes a new Skirmish start at that exact level without touching the user's Skirmish.ini.
inline bool forcedPercent(uint16_t &percent)
{
	const char *text = std::getenv("GENERALS_FORCE_GAME_SPEED");
	if (!text || !text[0]) return false;
	const unsigned v = static_cast<unsigned>(std::atoi(text));
	if (!isValid(v)) return false;
	percent = static_cast<uint16_t>(v);
	return true;
}

// stable preference text of the Skirmish selector (Skirmish.ini "GameSpeed")
inline const char *preferenceText(unsigned percent) { const int i = levelIndex(percent); return i >= 0 ? kLevels[i].prefText : "Normal"; }
inline bool parsePreferenceText(const char *text, uint16_t &percent)
{
	if (!text) return false;
	for (const Level &l : kLevels) if (std::strcmp(text, l.prefText) == 0) { percent = l.percent; return true; }
	return false;
}
}
