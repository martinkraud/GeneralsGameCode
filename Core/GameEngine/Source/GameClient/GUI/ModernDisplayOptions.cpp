// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 Options > Display / Gameplay page gadgets (Modern Experience). See ModernDisplayOptions.h.
#include "PreRTS.h"

#include "GameClient/ModernDisplayOptions.h"

#include "Common/DisplayRuntime.h"
#include "Common/DisplayModes.h"
#include "Common/DisplaySettings.h"
#include "Common/EnhancedHud.h"
#include "Common/GlobalData.h"
#include "Common/GroundTranslation.h"
#include "Common/ModernExperience.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionPreferences.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetLayering.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Shell.h"

#include <vector>

namespace
{
using DisplayConfig::Settings;
using DisplayConfig::WindowMode;

const int kUiScaleChoices[] = { 0, 75, 100, 125, 150, 175, 200, 250, 300 };   // 0 = Auto
constexpr int kUiScaleCount = sizeof(kUiScaleChoices) / sizeof(kUiScaleChoices[0]);

GameWindow *s_panelHost = nullptr;          // the Video panel that owns the runtime gadgets (X64.33 FIX2: Options pages)
GameWindow *s_monitor = nullptr;
GameWindow *s_mode = nullptr;
GameWindow *s_uiScale = nullptr;
GameWindow *s_vsync = nullptr;
GameWindow *s_motion = nullptr;
GameWindow *s_interface = nullptr;   // Original | Enhanced (applies from the next match; not live-switched)
bool s_inMatch = false;
std::vector<DisplayModes::Monitor> s_monitors;

void copyAppearance(GameWindow *target, GameWindow *source)
{
	// Borrowed image/color/font data only (same rule as FrameRateOptions).
	WinInstanceData *to = target->winGetInstanceData();
	const WinInstanceData *from = source->winGetInstanceData();
	memcpy(to->m_enabledDrawData, from->m_enabledDrawData, sizeof(to->m_enabledDrawData));
	memcpy(to->m_disabledDrawData, from->m_disabledDrawData, sizeof(to->m_disabledDrawData));
	memcpy(to->m_hiliteDrawData, from->m_hiliteDrawData, sizeof(to->m_hiliteDrawData));
	to->m_enabledText = from->m_enabledText;
	to->m_disabledText = from->m_disabledText;
	to->m_hiliteText = from->m_hiliteText;
	target->winSetStatus(source->winGetStatus() & WIN_STATUS_IMAGE);
	target->winSetDrawFunc(source->winGetDrawFunc());
	target->winSetFont(source->winGetFont());
}

GameWindow *makeCaption(GameWindow *parent, GameWindow *labelTemplate, GameWindow *fontSource, Int x, Int y, Int w, Int h,
                        const WideChar *fallbackText, const char *key)
{
	WinInstanceData instance;
	instance.m_font = fontSource->winGetFont();
	instance.m_enabledText = fontSource->winGetInstanceData()->m_enabledText;
	instance.m_disabledText = fontSource->winGetInstanceData()->m_disabledText;
	instance.m_hiliteText = fontSource->winGetInstanceData()->m_hiliteText;
	instance.m_style = GWS_STATIC_TEXT;
	TextData text = {};
	text.centeredVertically = TRUE;
	GameWindow *caption = TheWindowManager->gogoGadgetStaticText(parent, WIN_STATUS_ENABLED, x, y, w, h,
		&instance, &text, instance.m_font, FALSE);
	copyAppearance(caption, labelTemplate);
	caption->winSetFont(fontSource->winGetFont());
	GadgetStaticTextSetText(caption, TheGameText->FETCH_OR_SUBSTITUTE(key, fallbackText));
	return caption;
}

GameWindow *makeCombo(GameWindow *parent, GameWindow *comboTemplate, Int x, Int y, Int w, Int h, Int entries)
{
	WinInstanceData instance;
	instance.m_font = comboTemplate->winGetFont();
	instance.m_enabledText = comboTemplate->winGetInstanceData()->m_enabledText;
	instance.m_disabledText = comboTemplate->winGetInstanceData()->m_disabledText;
	instance.m_hiliteText = comboTemplate->winGetInstanceData()->m_hiliteText;
	instance.m_style = GWS_COMBO_BOX | GWS_MOUSE_TRACK;
	ComboBoxData data = {};
	data.maxDisplay = entries > 8 ? 8 : entries;
	data.maxChars = 40;
	data.entryData = NEW EntryData;
	memset(data.entryData, 0, sizeof(EntryData));
	data.entryData->maxTextLen = data.maxChars;
	data.listboxData = NEW ListboxData;
	memset(data.listboxData, 0, sizeof(ListboxData));
	data.listboxData->listLength = entries;
	data.listboxData->forceSelect = TRUE;
	data.listboxData->columns = 1;
	GameWindow *combo = TheWindowManager->gogoGadgetComboBox(parent, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP,
		x, y, w, h, &instance, &data, instance.m_font, FALSE);
	copyAppearance(combo, comboTemplate);
	copyAppearance(GadgetComboBoxGetEditBox(combo), GadgetComboBoxGetEditBox(comboTemplate));
	copyAppearance(GadgetComboBoxGetDropDownButton(combo), GadgetComboBoxGetDropDownButton(comboTemplate));
	copyAppearance(GadgetComboBoxGetListBox(combo), GadgetComboBoxGetListBox(comboTemplate));
	return combo;
}

void addEntry(GameWindow *combo, const UnicodeString &text)
{
	GadgetComboBoxAddEntry(combo, text, combo->winGetEnabledTextColor());
}

Int selected(GameWindow *combo)
{
	Int index = -1;
	if (combo)
		GadgetComboBoxGetSelectedPos(combo, &index);
	return index;
}

void select(GameWindow *combo, Int index)
{
	if (combo && index >= 0)
		GadgetComboBoxSetSelectedPos(combo, index);
}

int uiScaleIndex(int percent)
{
	for (int i = 0; i < kUiScaleCount; ++i)
		if (kUiScaleChoices[i] == percent)
			return i;
	return 0;
}

// Combos mirror the live settings (also used by Back and Defaults-on-open).
void syncFromRuntime()
{
	const Settings &s = DisplayRuntime::state().settings;
	select(s_mode, DisplayRuntime::state().resolved.mode == WindowMode::Windowed ? 0 : 1);
	select(s_monitor, DisplayRuntime::state().resolved.monitorIndex);
	select(s_uiScale, uiScaleIndex(s.uiScalePercent));
	select(s_vsync, s.vsync ? 1 : 0);
	select(s_motion, GroundTranslation::isEnabled() ? 1 : 0);
	select(s_interface, s.enhancedHud ? 1 : 0);
}

}  // namespace

namespace ModernDisplayOptions
{

bool create(GameWindow *resolutionCombo, const OptionPreferences &, bool inMatch)
{
	forget();
	if (!DisplayRuntime::modern() || ModernExperience::classic() || !resolutionCombo)
		return false;
	GameWindow *label = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("OptionsMenu.wnd:ResolutionLabel"));
	GameWindow *host = resolutionCombo->winGetParent();
	if (!label || !host)
		return false;
	s_panelHost = host;
	s_inMatch = inMatch;
	s_monitors = DisplayModes::enumerateMonitors();

	// Provisional rectangles: the Options pages (OptionsTabs) position every control. Creation is bottom-up in the
	// page order so a row's dropdown is already in front of the rows below it.
	Int x, y, width, height;
	resolutionCombo->winGetPosition(&x, &y);
	resolutionCombo->winGetSize(&width, &height);
	GameWindow *comboTemplate = resolutionCombo;
	auto caption = [&](const char *id, const WideChar *fallback, const char *key) {
		GameWindow *c = makeCaption(host, label, comboTemplate, x, y, width, height, fallback, key);
		if (c)
			c->winSetWindowId(NAMEKEY(id));
		return c;
	};
	auto combo = [&](const char *id, Int entries) {
		GameWindow *c = makeCombo(host, comboTemplate, x, y, width, height, entries);
		if (c)
			c->winSetWindowId(NAMEKEY(id));
		return c;
	};

	caption("OptionsMenu.wnd:ModernInterfaceLabel", L"Interface", "GUI:InterfaceStyle");
	s_interface = combo("OptionsMenu.wnd:ModernInterface", 2);
	addEntry(s_interface, TheGameText->FETCH_OR_SUBSTITUTE("GUI:InterfaceOriginal", L"Original"));
	addEntry(s_interface, TheGameText->FETCH_OR_SUBSTITUTE("GUI:InterfaceEnhanced", L"Enhanced (USA prototype)"));

	caption("OptionsMenu.wnd:ModernMotionLabel", L"Unit Motion", "GUI:UnitMotion");
	s_motion = combo("OptionsMenu.wnd:ModernMotion", 2);
	addEntry(s_motion, TheGameText->FETCH_OR_SUBSTITUTE("GUI:UnitMotionOriginal", L"Original (30 Hz steps)"));
	addEntry(s_motion, TheGameText->FETCH_OR_SUBSTITUTE("GUI:UnitMotionSmooth", L"Smooth (experimental)"));

	caption("OptionsMenu.wnd:ModernVsyncLabel", L"VSync", "GUI:VSync");
	s_vsync = combo("OptionsMenu.wnd:ModernVsync", 2);
	addEntry(s_vsync, TheGameText->FETCH_OR_SUBSTITUTE("GUI:Off", L"Off"));
	addEntry(s_vsync, TheGameText->FETCH_OR_SUBSTITUTE("GUI:On", L"On"));

	caption("OptionsMenu.wnd:ModernUiScaleLabel", L"UI Scale", "GUI:UIScale");
	s_uiScale = combo("OptionsMenu.wnd:ModernUiScale", kUiScaleCount);
	for (int i = 0; i < kUiScaleCount; ++i)
	{
		UnicodeString text;
		if (kUiScaleChoices[i] == 0)
			text = TheGameText->FETCH_OR_SUBSTITUTE("GUI:UIScaleAuto", L"Auto");
		else
			text.format(L"%d%%", kUiScaleChoices[i]);
		addEntry(s_uiScale, text);
	}

	caption("OptionsMenu.wnd:ModernMonitorLabel", L"Monitor", "GUI:DisplayMonitor");
	s_monitor = combo("OptionsMenu.wnd:ModernMonitor", s_monitors.empty() ? 1 : static_cast<Int>(s_monitors.size()));
	if (s_monitors.empty())
		addEntry(s_monitor, UnicodeString(L"1"));
	for (size_t i = 0; i < s_monitors.size(); ++i)
	{
		UnicodeString text;
		text.format(L"%d%s  %dx%d", static_cast<int>(i + 1), s_monitors[i].primary ? L" (Primary)" : L"",
			s_monitors[i].bounds.width, s_monitors[i].bounds.height);
		addEntry(s_monitor, text);
	}

	caption("OptionsMenu.wnd:ModernModeLabel", L"Display Mode", "GUI:DisplayMode");
	s_mode = combo("OptionsMenu.wnd:ModernMode", 2);
	addEntry(s_mode, TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayModeWindowed", L"Windowed"));
	addEntry(s_mode, TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayModeBorderless", L"Borderless Fullscreen"));

	syncFromRuntime();
	if (s_monitors.size() < 2 && s_monitor)
		s_monitor->winEnable(FALSE);
	if (inMatch)
	{
		// A running match keeps its window; only presentation-only settings change there.
		for (GameWindow *w : { s_monitor, s_mode, s_uiScale, s_motion, s_interface })
			if (w) w->winEnable(FALSE);
	}
	return true;
}

bool hasMultipleMonitors()
{
	return s_monitors.size() > 1;
}

bool onButton(int)
{
	return false;   // the display controls are plain page gadgets (see OptionsTabs); nothing opens a sub-panel
}

void reset()
{
	// Defaults: automatic UI scale, vsync off, original unit motion; window mode and monitor are left as they are.
	select(s_uiScale, 0);
	select(s_vsync, 0);
	select(s_motion, 0);
	select(s_interface, 0);   // Original
}

bool apply(OptionPreferences &preferences, bool layoutsAlreadyRebuilt)
{
	if (!DisplayRuntime::modern() || !s_panelHost)
		return false;
	DisplayRuntime::State &state = DisplayRuntime::state();
	Settings next = state.settings;
	next.explicitSettings = true;

	const Int monitorIndex = selected(s_monitor);
	const Int modeIndex = selected(s_mode);
	const Int scaleIndex = selected(s_uiScale);
	const Int vsyncIndex = selected(s_vsync);
	const Int motionIndex = selected(s_motion);
	const Int interfaceIndex = selected(s_interface);
	const bool inMatch = s_inMatch;

	const WindowMode previousMode = state.resolved.mode;
	const int previousMonitor = state.resolved.monitorIndex;
	const int previousScale = state.settings.uiScalePercent;
	const bool previousVsync = state.settings.vsync;
	const bool previousInterface = state.settings.enhancedHud;

	if (!inMatch)
	{
		if (modeIndex >= 0) next.mode = modeIndex == 0 ? WindowMode::Windowed : WindowMode::Borderless;
		if (monitorIndex >= 0 && monitorIndex < static_cast<Int>(s_monitors.size())) next.monitor = monitorIndex;
		if (scaleIndex >= 0 && scaleIndex < kUiScaleCount) next.uiScalePercent = kUiScaleChoices[scaleIndex];
		if (motionIndex >= 0) next.smoothMotion = motionIndex == 1;
		if (interfaceIndex >= 0) next.enhancedHud = interfaceIndex == 1;
	}
	if (vsyncIndex >= 0) next.vsync = vsyncIndex == 1;

	bool relayout = false;
	if (!inMatch && (next.mode != previousMode || next.monitor != previousMonitor))
	{
		DisplayRuntime::ApplyModeFn applyMode = DisplayRuntime::applyModeHook();
		const DisplayGeometry::Size client = state.resolved.client;
		if (applyMode && applyMode(static_cast<unsigned>(client.width), static_cast<unsigned>(client.height),
		                           next.mode != WindowMode::Windowed, next.monitor))
		{
			relayout = true;
			next = state.settings;                 // the hook stored what was actually applied
			next.explicitSettings = true;
			next.uiScalePercent = scaleIndex >= 0 && scaleIndex < kUiScaleCount ? kUiScaleChoices[scaleIndex] : next.uiScalePercent;
			next.vsync = vsyncIndex == 1;
			next.smoothMotion = motionIndex == 1;
			next.enhancedHud = interfaceIndex == 1;
		}
		else
		{
			next.mode = previousMode;              // could not apply: keep (and persist) what is on screen
			next.monitor = previousMonitor;
		}
	}
	if (!inMatch && next.uiScalePercent != previousScale)
	{
		DisplayRuntime::setUiScalePercent(next.uiScalePercent);
		relayout = true;
	}
	if (!inMatch && next.enhancedHud != previousInterface)
		relayout = true;   // the control bar is rebuilt like for a UI scale change; the layout is chosen at its creation
	if (next.vsync != previousVsync)
	{
		if (DisplayRuntime::VsyncFn vsync = DisplayRuntime::vsyncHook())
			vsync(next.vsync);
	}
	if (!inMatch)
	{
		GroundTranslation::setEnabled(next.smoothMotion);
		EnhancedHud::setRequested(next.enhancedHud);   // takes effect when the next match builds its control bar
	}

	// Persisted: the windowed size stays the user's windowed size even while borderless is active.
	Settings store = next;
	if (state.resolved.mode == WindowMode::Windowed && state.resolved.client.valid())
		store.resolution = state.resolved.client;
	else
		store.resolution = state.settings.resolution;
	state.settings = next;
	state.settings.resolution = store.resolution;

	DisplayConfig::KeyValues kv;
	DisplayConfig::store(store, kv);
	for (const auto &entry : kv)
		preferences[AsciiString(entry.first.c_str())] = AsciiString(entry.second.c_str());

	(void)layoutsAlreadyRebuilt;   // a mode/scale change after a resolution change still needs its own rebuild
	return relayout;
}

void forget()
{
	s_panelHost = s_monitor = s_mode = s_uiScale = s_vsync = s_motion = s_interface = nullptr;
	s_monitors.clear();
	s_inMatch = false;
}

}  // namespace ModernDisplayOptions
