// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 Options > Display popup (Modern Experience). See ModernDisplayOptions.h.
#include "PreRTS.h"

#include "GameClient/ModernDisplayOptions.h"

#include "Common/DisplayRuntime.h"
#include "Common/DisplayModes.h"
#include "Common/DisplaySettings.h"
#include "Common/GlobalData.h"
#include "Common/GroundTranslation.h"
#include "Common/ModernExperience.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionPreferences.h"
#include "Common/OptionsPanelLayout.h"
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

GameWindow *s_popup = nullptr;
GameWindow *s_entry = nullptr;
GameWindow *s_monitor = nullptr;
GameWindow *s_mode = nullptr;
GameWindow *s_uiScale = nullptr;
GameWindow *s_vsync = nullptr;
GameWindow *s_motion = nullptr;
NameKeyType s_entryId = NAMEKEY_INVALID;
NameKeyType s_acceptId = NAMEKEY_INVALID;
NameKeyType s_backId = NAMEKEY_INVALID;
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
}

WindowMsgHandledType popupSystem(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	if (msg == GBM_SELECTED)
	{
		GameWindow *control = (GameWindow *)mData1;
		const NameKeyType id = control ? static_cast<NameKeyType>(control->winGetWindowId()) : NAMEKEY_INVALID;
		if (id == s_acceptId)
		{
			if (s_popup) s_popup->winHide(TRUE);
			return MSG_HANDLED;
		}
		if (id == s_backId)
		{
			syncFromRuntime();
			if (s_popup) s_popup->winHide(TRUE);
			return MSG_HANDLED;
		}
	}
	return MSG_IGNORED;
}
}  // namespace

namespace ModernDisplayOptions
{

bool create(GameWindow *resolutionCombo, GameWindow *popupTemplate, GameWindow *buttonTemplate,
            const OptionPreferences &, bool inMatch)
{
	forget();
	if (!DisplayRuntime::modern() || ModernExperience::classic() || !resolutionCombo || !popupTemplate || !buttonTemplate)
		return false;
	GameWindow *label = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("OptionsMenu.wnd:ResolutionLabel"));
	GameWindow *backTemplate = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("OptionsMenu.wnd:ButtonAdvanceBack"));
	GameWindow *rowParent = resolutionCombo->winGetParent();
	GameWindow *popupParent = popupTemplate->winGetParent();
	if (!label || !backTemplate || !rowParent || label->winGetParent() != rowParent)
		return false;

	// --- entry button: right half of the Resolution row --------------------------------------------------
	Int x, y, width, height, parentWidth, parentHeight, labelX, labelY, labelWidth, labelHeight;
	resolutionCombo->winGetPosition(&x, &y);
	resolutionCombo->winGetSize(&width, &height);
	rowParent->winGetSize(&parentWidth, &parentHeight);
	label->winGetPosition(&labelX, &labelY);
	label->winGetSize(&labelWidth, &labelHeight);
	const Int gap = max(2, height / 4);
	const Int columnWidth = (parentWidth - 2 * x - gap) / 2;
	if (columnWidth < height * 3)
		return false;   // incompatible mod layout: leave the original controls alone

	s_entryId = NAMEKEY("OptionsMenu.wnd:ButtonModernDisplay");
	s_acceptId = NAMEKEY("OptionsMenu.wnd:ModernDisplay.Accept");
	s_backId = NAMEKEY("OptionsMenu.wnd:ModernDisplay.Back");
	s_inMatch = inMatch;

	WinInstanceData entry;
	entry.m_font = resolutionCombo->winGetFont();
	entry.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
	entry.m_id = s_entryId;
	s_entry = TheWindowManager->gogoGadgetPushButton(rowParent, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP,
		x + columnWidth + gap, y, columnWidth, height, &entry, entry.m_font, FALSE);
	copyAppearance(s_entry, buttonTemplate);
	GadgetButtonSetText(s_entry, TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayOptions", L"Display Options..."));
	GameWindow *entryCaption = makeCaption(rowParent, label, resolutionCombo, x + columnWidth + gap, labelY, columnWidth,
		labelHeight, L"Window, Scale, VSync", "GUI:DisplayOptionsCaption");
	resolutionCombo->winSetSize(columnWidth, height);
	label->winSetSize(columnWidth, labelHeight);
	// Behind the Resolution combo (whose dropdown opens over the Detail row below) but in front of the rows beneath.
	GadgetLayering::placeBehind(s_entry, resolutionCombo);
	GadgetLayering::placeBehind(entryCaption, s_entry);

	// --- popup panel (same footprint and draw data as the Advanced Display Options panel) -------------
	Int px, py, pw, ph;
	popupTemplate->winGetPosition(&px, &py);
	popupTemplate->winGetSize(&pw, &ph);
	WinInstanceData panel;
	panel.m_style = GWS_USER_WINDOW;
	panel.m_id = NAMEKEY("OptionsMenu.wnd:ModernDisplayPopup");
	s_popup = TheWindowManager->winCreate(popupParent, WIN_STATUS_ENABLED | WIN_STATUS_HIDDEN | WIN_STATUS_NO_FOCUS,
		px, py, pw, ph, popupSystem, &panel);
	if (!s_popup)
		return false;
	copyAppearance(s_popup, popupTemplate);
	s_popup->winSetDrawFunc(popupTemplate->winGetDrawFunc());

	// monitor list
	s_monitors = DisplayModes::enumerateMonitors();
	GameWindow *comboTemplate = resolutionCombo;

	// Geometry comes from OptionsPanelLayout (reference units x the live UI scale, rows below the title, notes
	// above the button zone); nothing here is a fixed pixel number.
	Int bx0, by0, bw0, bh0;
	buttonTemplate->winGetPosition(&bx0, &by0);
	buttonTemplate->winGetSize(&bw0, &bh0);
	const int rowCount = 5;
	const OptionsPanelLayout::Panel layout = OptionsPanelLayout::compute(pw, ph, height, DisplayRuntime::uiScale(),
		rowCount, by0 - py);
	if (!layout.fits)
		StartupDiagnostics::checkpoint("display_options", "observed", "panel rows exceed the button zone; compact layout");

	makeCaption(s_popup, label, comboTemplate, layout.title.x, layout.title.y, layout.title.w, layout.title.h,
		L"DISPLAY OPTIONS", "GUI:DisplayOptionsTitle");

	// Rows are created BOTTOM TO TOP: a later-created sibling is drawn in front, and an upper row's dropdown must
	// open over the rows below it (see GadgetLayering.h).
	auto caption = [&](int row, const WideChar *fallback, const char *key) {
		makeCaption(s_popup, label, comboTemplate, layout.label[row].x, layout.label[row].y, layout.label[row].w,
			layout.label[row].h, fallback, key);
	};
	auto combo = [&](int row, Int entries) {
		return makeCombo(s_popup, comboTemplate, layout.control[row].x, layout.control[row].y, layout.control[row].w,
			layout.control[row].h, entries);
	};

	makeCaption(s_popup, label, comboTemplate, layout.note.x, layout.note.y, layout.note.w, layout.note.h,
		L"Window, monitor and UI scale changes apply from the menus. Frame Rate Limit is on the Options screen.",
		"GUI:DisplayOptionsNote");

	caption(4, L"Unit Motion", "GUI:UnitMotion");
	s_motion = combo(4, 2);
	addEntry(s_motion, TheGameText->FETCH_OR_SUBSTITUTE("GUI:UnitMotionOriginal", L"Original (30 Hz steps)"));
	addEntry(s_motion, TheGameText->FETCH_OR_SUBSTITUTE("GUI:UnitMotionSmooth", L"Smooth (experimental)"));

	caption(3, L"VSync", "GUI:VSync");
	s_vsync = combo(3, 2);
	addEntry(s_vsync, TheGameText->FETCH_OR_SUBSTITUTE("GUI:Off", L"Off"));
	addEntry(s_vsync, TheGameText->FETCH_OR_SUBSTITUTE("GUI:On", L"On"));

	caption(2, L"UI Scale", "GUI:UIScale");
	s_uiScale = combo(2, kUiScaleCount);
	for (int i = 0; i < kUiScaleCount; ++i)
	{
		UnicodeString text;
		if (kUiScaleChoices[i] == 0)
			text = TheGameText->FETCH_OR_SUBSTITUTE("GUI:UIScaleAuto", L"Auto");
		else
			text.format(L"%d%%", kUiScaleChoices[i]);
		addEntry(s_uiScale, text);
	}

	caption(1, L"Display Mode", "GUI:DisplayMode");
	s_mode = combo(1, 2);
	addEntry(s_mode, TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayModeWindowed", L"Windowed"));
	addEntry(s_mode, TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayModeBorderless", L"Borderless Fullscreen"));

	caption(0, L"Monitor", "GUI:DisplayMonitor");
	s_monitor = combo(0, s_monitors.empty() ? 1 : static_cast<Int>(s_monitors.size()));
	if (s_monitors.empty())
		addEntry(s_monitor, UnicodeString(L"1"));
	for (size_t i = 0; i < s_monitors.size(); ++i)
	{
		UnicodeString text;
		text.format(L"%d%s  %dx%d", static_cast<int>(i + 1), s_monitors[i].primary ? L" (Primary)" : L"",
			s_monitors[i].bounds.width, s_monitors[i].bounds.height);
		addEntry(s_monitor, text);
	}

	// accept / back buttons, same art as the Advanced panel's
	Int bx, by, bw, bh, cx, cy, cw, ch;
	buttonTemplate->winGetPosition(&bx, &by);
	buttonTemplate->winGetSize(&bw, &bh);
	backTemplate->winGetPosition(&cx, &cy);
	backTemplate->winGetSize(&cw, &ch);
	WinInstanceData accept;
	accept.m_font = buttonTemplate->winGetFont();
	accept.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
	accept.m_id = s_acceptId;
	GameWindow *acceptButton = TheWindowManager->gogoGadgetPushButton(s_popup, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP,
		bx - px, by - py, bw, bh, &accept, accept.m_font, FALSE);
	copyAppearance(acceptButton, buttonTemplate);
	GadgetButtonSetText(acceptButton, TheGameText->FETCH_OR_SUBSTITUTE("GUI:OK", L"OK"));
	WinInstanceData back;
	back.m_font = backTemplate->winGetFont();
	back.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
	back.m_id = s_backId;
	GameWindow *backButton = TheWindowManager->gogoGadgetPushButton(s_popup, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP,
		cx - px, cy - py, cw, ch, &back, back.m_font, FALSE);
	copyAppearance(backButton, backTemplate);
	GadgetButtonSetText(backButton, TheGameText->FETCH_OR_SUBSTITUTE("GUI:Cancel", L"Cancel"));

	syncFromRuntime();
	if (s_monitors.size() < 2 && s_monitor)
		s_monitor->winEnable(FALSE);
	if (inMatch)
	{
		// A running match keeps its window; only presentation-only settings change there.
		for (GameWindow *w : { s_monitor, s_mode, s_uiScale, s_motion })
			if (w) w->winEnable(FALSE);
	}
	return true;
}

bool onButton(int controlId)
{
	if (s_entry && controlId == static_cast<int>(s_entryId) && s_popup)
	{
		syncFromRuntime();
		s_popup->winHide(FALSE);
		return true;
	}
	return false;
}

void reset()
{
	// Defaults: automatic UI scale, vsync off, original unit motion; window mode and monitor are left as they are.
	select(s_uiScale, 0);
	select(s_vsync, 0);
	select(s_motion, 0);
}

bool apply(OptionPreferences &preferences, bool layoutsAlreadyRebuilt)
{
	if (!DisplayRuntime::modern() || !s_popup)
		return false;
	DisplayRuntime::State &state = DisplayRuntime::state();
	Settings next = state.settings;
	next.explicitSettings = true;

	const Int monitorIndex = selected(s_monitor);
	const Int modeIndex = selected(s_mode);
	const Int scaleIndex = selected(s_uiScale);
	const Int vsyncIndex = selected(s_vsync);
	const Int motionIndex = selected(s_motion);
	const bool inMatch = s_inMatch;

	const WindowMode previousMode = state.resolved.mode;
	const int previousMonitor = state.resolved.monitorIndex;
	const int previousScale = state.settings.uiScalePercent;
	const bool previousVsync = state.settings.vsync;

	if (!inMatch)
	{
		if (modeIndex >= 0) next.mode = modeIndex == 0 ? WindowMode::Windowed : WindowMode::Borderless;
		if (monitorIndex >= 0 && monitorIndex < static_cast<Int>(s_monitors.size())) next.monitor = monitorIndex;
		if (scaleIndex >= 0 && scaleIndex < kUiScaleCount) next.uiScalePercent = kUiScaleChoices[scaleIndex];
		if (motionIndex >= 0) next.smoothMotion = motionIndex == 1;
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
	if (next.vsync != previousVsync)
	{
		if (DisplayRuntime::VsyncFn vsync = DisplayRuntime::vsyncHook())
			vsync(next.vsync);
	}
	if (!inMatch)
		GroundTranslation::setEnabled(next.smoothMotion);

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
	s_popup = s_entry = s_monitor = s_mode = s_uiScale = s_vsync = s_motion = nullptr;
	s_monitors.clear();
	s_inMatch = false;
}

}  // namespace ModernDisplayOptions
