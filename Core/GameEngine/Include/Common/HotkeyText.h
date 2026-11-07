// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Options > Keyboard: how a CommandMap binding is written for a player ("Ctrl+Shift+F5", "Num +", "Page Up"). Pure text helpers; the engine glue reads TheMetaMap (read-only).
#pragma once

#include <cstring>
#include <string>

namespace HotkeyText
{
// raw key names are the CommandMap.ini names ("KEY_A", "KEY_KPPLUS", "KEY_PGUP"); returns a short readable form
inline std::string keyName(const char *raw)
{
	if (!raw || !raw[0]) return std::string();
	std::string s = raw;
	if (s.compare(0, 4, "KEY_") == 0) s.erase(0, 4);
	struct Map { const char *from; const char *to; };
	static const Map map[] = {
		{ "KPPLUS", "Num +" }, { "KPMINUS", "Num -" }, { "KPSTAR", "Num *" }, { "KPDEL", "Num ." },
		{ "KP0", "Num 0" }, { "KP1", "Num 1" }, { "KP2", "Num 2" }, { "KP3", "Num 3" }, { "KP4", "Num 4" }, { "KP5", "Num 5" }, { "KP6", "Num 6" }, { "KP7", "Num 7" }, { "KP8", "Num 8" }, { "KP9", "Num 9" },
		{ "MINUS", "-" }, { "EQUAL", "=" }, { "LBRACKET", "[" }, { "RBRACKET", "]" }, { "SEMICOLON", ";" }, { "APOSTROPHE", "'" }, { "TICK", "`" },
		{ "BACKSLASH", "\\" }, { "COMMA", "," }, { "PERIOD", "." }, { "SLASH", "/" },
		{ "PGUP", "Page Up" }, { "PGDN", "Page Down" }, { "INS", "Insert" }, { "DEL", "Delete" }, { "ESC", "Esc" },
		{ "UP", "Up" }, { "DOWN", "Down" }, { "LEFT", "Left" }, { "RIGHT", "Right" }, { "HOME", "Home" }, { "END", "End" },
		{ "SPACE", "Space" }, { "ENTER", "Enter" }, { "TAB", "Tab" }, { "BACKSPACE", "Backspace" },
	};
	for (const Map &m : map) if (s == m.from) return m.to;
	return s;
}

// "Ctrl+Alt+Shift+" prefix for the modifier flags (fixed order, empty when none)
inline std::string modifiers(bool ctrl, bool alt, bool shift)
{
	std::string s;
	if (ctrl) s += "Ctrl+";
	if (alt) s += "Alt+";
	if (shift) s += "Shift+";
	return s;
}

inline std::string combo(const char *rawKey, bool ctrl, bool alt, bool shift)
{
	const std::string key = keyName(rawKey);
	if (key.empty() || key == "NONE") return std::string();
	return modifiers(ctrl, alt, shift) + key;
}
}
