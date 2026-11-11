// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Options: one short, factual help sentence per exposed setting, shown in the help strip of the Options screen while the setting is focused or hovered. Pure data (no engine state): the
// text is looked up through GameText by `key` (localisable) and falls back to `fallback`. Every control of every page table (Common/OptionsPageLayout.h) has an entry (unit test).
//
// Style rules (pinned by the unit test): one or two sentences, no marketing words, no claim the setting does not have. A setting that changes only at the main menu or at the next match says so.
#pragma once

#include <cstring>

namespace SettingHelp
{
struct Entry { const char *control; const char *key; const wchar_t *fallback; };

constexpr Entry kEntries[] = {
	// Display
	{ "ModernMode", "GUI:HelpDisplayMode", L"Windowed runs in a window. Borderless Fullscreen fills the desktop without changing the display mode. Applies from the main menu." },
	{ "ModernMonitor", "GUI:HelpMonitor", L"The display the game uses. Applies from the main menu." },
	{ "ComboBoxResolution", "GUI:HelpResolution", L"Size of the game window. Borderless Fullscreen always uses the desktop size. Applies from the main menu." },
	{ "ModernUiScale", "GUI:HelpUiScale", L"Auto picks the largest interface scale that fits your display. A fixed percentage overrides it; a scale that does not fit is reduced automatically. Applies from the main menu." },
	{ "ModernVsync", "GUI:HelpVsync", L"Synchronises the picture to the display refresh. On avoids tearing; Off lowers input latency and can tear." },
	{ "ComboBoxFrameRateLimit", "GUI:HelpFrameLimit", L"Caps how often the picture is drawn. It does not change game speed: the simulation keeps its own rate." },
	// Graphics
	{ "ComboBoxDetail", "GUI:HelpDetail", L"Overall detail preset for textures, shadows, particles and effects. Custom opens the advanced panel. Applies from the main menu, to the next match." },
	{ "ModernRenderScale", "GUI:HelpRenderScale", L"Draws the 3D world at a lower internal resolution and scales it up; the interface stays at native resolution. Lower values are faster and softer." },
	// Audio
	{ "SliderSFXVolume", "GUI:HelpSfxVolume", L"Volume of sound effects: weapons, explosions and unit sounds." },
	{ "SliderMusicVolume", "GUI:HelpMusicVolume", L"Volume of the music." },
	{ "SliderVoiceVolume", "GUI:HelpVoiceVolume", L"Volume of unit voices and spoken announcements." },
	// Controls
	{ "CheckAlternateMouse", "GUI:HelpAlternateMouse", L"Uses the alternate mouse button layout of the original game instead of the default one." },
	{ "SliderScrollSpeed", "GUI:HelpScrollSpeed", L"How fast the view moves when you scroll with the mouse or the keyboard." },
	{ "CheckBoxDrawAnchor", "GUI:HelpDrawAnchor", L"Draws a marker where a middle-mouse scroll started." },
	{ "CheckBoxMoveAnchor", "GUI:HelpMoveAnchor", L"Lets the middle-mouse scroll anchor follow the cursor while you drag the view." },
	{ "CheckDoubleClickAttackMove", "GUI:HelpDoubleClickAttackMove", L"Double-clicking the ground gives the selected units a guard order at that location." },
	{ "Retaliation", "GUI:HelpRetaliation", L"Your units automatically return fire when attacked." },
	// Gameplay
	{ "CheckBoxUseCamera", "GUI:HelpUseCamera", L"When you watch a replay, uses the camera positions that were saved in it." },
	{ "CheckBoxSaveCamera", "GUI:HelpSaveCamera", L"Saves your camera movements in the replays of the games you play." },
	// Modern Experience
	{ "ModernMotion", "GUI:HelpMotion", L"Original moves units once per simulation tick. Smooth interpolates their motion between ticks on faster displays; gameplay is identical. Experimental. Applies from the main menu." },
	{ "ModernInterface", "GUI:HelpInterface", L"Original is the classic control bar. Enhanced is the redesigned HUD for USA, China and GLA with the same commands and layout logic." },
	{ "ModernSelectionRing", "GUI:HelpSelectionRing", L"Shows a ground indicator around selected units and structures. Turning it off hides only the ring; health bars and the command UI stay. Does not affect gameplay." },
	// Network
	{ "ComboBoxIP", "GUI:HelpLanAddress", L"The network adapter address the game uses for LAN games. Choose the one on your local network." },
	{ "ComboBoxOnlineIP", "GUI:HelpOnlineAddress", L"Used by the original online service, which is no longer available. It does not affect LAN play." },
	{ "TextEntryFirewallPortOverride", "GUI:HelpFirewallPort", L"Used by the original online service, which is no longer available. It does not affect LAN play." },
	{ "ButtonFirewallRefresh", "GUI:HelpFirewallRefresh", L"Used by the original online service, which is no longer available. It does not affect LAN play." },
};
constexpr int kEntryCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));

inline const Entry *find(const char *control)
{
	if (!control) return nullptr;
	for (const Entry &e : kEntries)
		if (std::strcmp(e.control, control) == 0) return &e;
	return nullptr;
}
}
