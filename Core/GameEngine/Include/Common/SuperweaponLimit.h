// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52A: the Skirmish match rule "Superweapons" - how many superweapon STRUCTURES (Particle Cannon, Nuclear Missile, SCUD Storm and their general-specific variants) a player may
// own at the same time. A MATCH rule: it applies to humans and to both AI Behaviour modes, never to General Powers, never to Campaign / General's Challenge.
//
//   Unlimited (default, stored as 0)  retail behaviour: nothing is checked, nothing is written to a replay header or a save
//   Max 1 / 3 / 5                     a player may not START another superweapon structure while he owns that many (completed, under construction, a standing GLA rebuild hole
//                                     of one, or captured). Destroying / selling one frees the capacity at once; capturing one never destroys anything.
//   Disabled                          no superweapon structure may be started
//
// Pure helpers only (no engine state): the engine glue is in GameLogic/System/SuperweaponLimit.cpp.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace SuperweaponLimit
{
constexpr uint16_t kUnlimited = 0;
constexpr uint16_t kDisabled = 0xFFFF;

// the values a player can select (and the only ones a replay header / save may carry)
inline bool isValid(unsigned value) { return value == kUnlimited || value == 1 || value == 3 || value == 5 || value == kDisabled; }
inline bool isActive(unsigned value) { return value != kUnlimited; }

// true when a player who owns `owned` applicable structures may NOT start another one
inline bool blocks(unsigned limit, unsigned owned)
{
	if (limit == kUnlimited) return false;
	if (limit == kDisabled) return true;
	return owned >= limit;
}

// options string: key "SW", written only for a non-default value ("SW=1", "SW=3", "SW=5", "SW=D"); anything else is refused on read
inline const char *optionKey() { return "SW"; }
inline bool formatOptionValue(unsigned limit, char *out, size_t size)
{
	if (!isValid(limit) || limit == kUnlimited || size < 3) return false;
	if (limit == kDisabled) { out[0] = 'D'; out[1] = '\0'; return true; }
	out[0] = (char)('0' + limit); out[1] = '\0';
	return true;
}
inline bool parseOptionValue(const char *text, uint16_t &limit)
{
	if (!text || !text[0] || text[1]) return false;
	if (text[0] == 'D' || text[0] == 'd') { limit = kDisabled; return true; }
	if (text[0] == '1' || text[0] == '3' || text[0] == '5') { limit = (uint16_t)(text[0] - '0'); return true; }
	return false;   // "0" is never written (Unlimited writes nothing); other numbers are not selectable values
}

// stable preference text of the Skirmish selector
inline const char *preferenceText(unsigned limit)
{
	switch (limit) { case 1: return "Max1"; case 3: return "Max3"; case 5: return "Max5"; case kDisabled: return "Disabled"; default: return "Unlimited"; }
}
inline bool parsePreferenceText(const char *text, uint16_t &limit)
{
	if (!text) return false;
	struct Entry { const char *name; uint16_t value; };
	static const Entry entries[] = { { "Unlimited", kUnlimited }, { "Max1", 1 }, { "Max3", 3 }, { "Max5", 5 }, { "Disabled", kDisabled } };
	for (const Entry &e : entries)
	{
		const char *a = text, *b = e.name;
		while (*a && *b && ((*a | 0x20) == (*b | 0x20))) { ++a; ++b; }
		if (!*a && !*b) { limit = e.value; return true; }
	}
	return false;
}
}
