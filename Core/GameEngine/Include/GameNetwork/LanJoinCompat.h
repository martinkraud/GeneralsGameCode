// SPDX-License-Identifier: GPL-3.0-or-later
// LAN join compatibility (Public Alpha gate: "multiplayer version / build compatibility check"): pure, engine-free rules.
//
// A LAN match is deterministic lockstep: every machine must run the same executable AND the same game data (INI / script CRC), otherwise the match goes out of sync after a while
// or crashes. The join request already carries both values (`GameToJoin.exeCRC` / `iniCRC`, filled by LANAPI::RequestGameJoin) and the host compares them
// (LANAPI::handleRequestJoin). Retail Generals enforced this; the retail Zero Hour code compiled the check out ("@todo Enable CRC checks"), so two different Zero Hour builds were
// accepted and failed later, silently, as a desync. The native 64-bit builds (the Modern Engine) now enforce it in BOTH titles: a joiner with a different build or different data is
// refused at the door with a clear reason instead of joining a doomed match.
//
// The wire format is unchanged (same fields, same reason code RET_CRC_MISMATCH).
#pragma once

namespace LanJoinCompat
{
struct Identity
{
	unsigned exeCRC = 0;   // CRC of the running executable file + game version + multiplayer scripts (GlobalData::generateExeCRC)
	unsigned iniCRC = 0;   // CRC of the loaded INI data (GameEngine::init)
};

// Same build and same data: the only joinable combination.
inline bool compatible(const Identity &host, const Identity &joiner)
{
	return host.exeCRC == joiner.exeCRC && host.iniCRC == joiner.iniCRC;
}

// Where the check is enforced: retail Generals (always was) and every native 64-bit build (both titles). Only a 32-bit Zero Hour build keeps the upstream "not enforced" behaviour.
constexpr bool enforced(bool zeroHour, bool native64) { return !zeroHour || native64; }

// What the joiner is told (Modern presentation; Classic keeps the retail text of the string table).
inline const wchar_t *modernDenyText()
{
	return L"This game was started with a different version of the game or different game data. Everyone must use the same MKRaudstein Modern Engine package.";
}
} // namespace LanJoinCompat
