// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 Options > Display / Gameplay page gadgets (Modern Experience). See ModernDisplayOptions.h.
// X64.37: transactional display changes (Apply -> Keep / Revert / timeout), stable monitor identity, mode-aware resolution list.
#include "PreRTS.h"

#include "GameClient/ModernDisplayOptions.h"

#include "Common/DisplayRuntime.h"
#include "Common/DisplayModes.h"
#include "Common/DisplaySettings.h"
#include "Common/DisplayTransaction.h"
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
#include "GameClient/HeaderTemplate.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameClient/Shell.h"

#include <vector>

namespace
{
using DisplayConfig::Settings;
using DisplayConfig::WindowMode;
using DisplayTransaction::Config;
using DisplayTransaction::Outcome;
using DisplayTransaction::Phase;

const int kUiScaleChoices[] = { 0, 75, 100, 125, 150, 175, 200, 250, 300 };   // 0 = Auto
constexpr int kUiScaleCount = sizeof(kUiScaleChoices) / sizeof(kUiScaleChoices[0]);
constexpr Int kDialogCorner = 10;   // top-left offset of the Keep / Revert box, as the retail resolution dialog

GameWindow *s_panelHost = nullptr;          // the Video panel that owns the runtime gadgets (X64.33 FIX2: Options pages)
GameWindow *s_resolution = nullptr;         // the retail Resolution combo (filled by populateResolution)
GameWindow *s_monitor = nullptr;
GameWindow *s_mode = nullptr;
GameWindow *s_uiScale = nullptr;
GameWindow *s_vsync = nullptr;
GameWindow *s_motion = nullptr;
GameWindow *s_interface = nullptr;   // Original | Enhanced (applies from the next match; not live-switched)
bool s_inMatch = false;
bool s_refreshing = false;           // programmatic combo changes must not re-enter onSelectionChanged
std::vector<DisplayModes::Monitor> s_monitors;
std::vector<DisplayGeometry::Size> s_sizes;   // what the Resolution combo currently offers (index == combo index)

// ---- the display transaction (Apply -> Keep / Revert) ---------------------------------------------------------------------------
DisplayTransaction::Transaction s_tx;
DisplayConfig::PersistPlan s_plan;           // what to write when the pending change is kept
bool s_failed = false;                       // the last apply could not be used and was rolled back
GameWindow *s_box = nullptr;                 // the Keep / Revert message box
int s_boxSeconds = -1;

unsigned nowMs()
{
	return static_cast<unsigned>(GetTickCount());
}

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

// ---- configuration <-> runtime ---------------------------------------------------------------------------------------------------
Config configOfRuntime()
{
	const DisplayRuntime::State &state = DisplayRuntime::state();
	Config c;
	c.monitorIndex = state.resolved.monitorIndex;
	c.monitorName = state.resolved.monitorName;
	c.mode = state.resolved.mode == WindowMode::Windowed ? WindowMode::Windowed : WindowMode::Borderless;
	c.client = state.resolved.mode == WindowMode::Windowed ? state.resolved.client
		: (state.settings.resolution.valid() ? state.settings.resolution : DisplayGeometry::kReferenceSize);
	c.uiScalePercent = state.settings.uiScalePercent;
	return c;
}

// Applies a configuration to the live window / swap chain / UI scale. The shell layouts are rebuilt by the caller (once).
bool applyConfig(const Config &c)
{
	DisplayRuntime::ApplyModeFn hook = DisplayRuntime::applyModeHook();
	if (!hook)
		return false;
	const std::vector<DisplayModes::Monitor> monitors = DisplayModes::enumerateMonitors();
	const int monitor = DisplayConfig::chooseMonitor(monitors, c.monitorName, c.monitorIndex);
	const DisplayGeometry::Size size = c.client.valid() ? c.client : DisplayGeometry::kReferenceSize;
	if (!hook(static_cast<unsigned>(size.width), static_cast<unsigned>(size.height), c.mode != WindowMode::Windowed, monitor))
		return false;
	DisplayRuntime::setUiScalePercent(c.uiScalePercent);
	return true;
}

void relayoutAll()
{
	TheHeaderTemplateManager->onResolutionChanged();
	TheMouse->onResolutionChanged();
	TheShell->recreateWindowLayouts();
	TheInGameUI->recreateControlBar();
	TheInGameUI->refreshCustomUiResources();
}

void persist(const Settings &s)
{
	OptionPreferences preferences;
	DisplayConfig::KeyValues kv;
	DisplayConfig::store(s, kv);
	for (const auto &entry : kv)
		preferences[AsciiString(entry.first.c_str())] = AsciiString(entry.second.c_str());
	preferences.write();
	DisplayRuntime::state().stored = s;
}

void writeInto(OptionPreferences &preferences, const Settings &s)
{
	DisplayConfig::KeyValues kv;
	DisplayConfig::store(s, kv);
	for (const auto &entry : kv)
		preferences[AsciiString(entry.first.c_str())] = AsciiString(entry.second.c_str());
	DisplayRuntime::state().stored = s;
}

// The sizes the Resolution combo offers for a (pending) mode / monitor.
void rebuildSizes(bool borderless, int monitorIndex)
{
	s_sizes.clear();
	if (DisplayRuntime::SizeChoicesFn fn = DisplayRuntime::sizeChoicesHook())
		fn(borderless, monitorIndex, s_sizes);
	if (s_sizes.empty())
		s_sizes.push_back(DisplayRuntime::state().resolved.client);
}

bool pendingBorderless()
{
	if (s_mode)
		return selected(s_mode) == 1;
	return DisplayRuntime::state().resolved.mode != WindowMode::Windowed;
}

int pendingMonitor()
{
	if (s_monitor && selected(s_monitor) >= 0)
		return selected(s_monitor);
	return DisplayRuntime::state().resolved.monitorIndex;
}

void fillResolutionCombo(GameWindow *combo, DisplayGeometry::Size keep)
{
	if (!combo)
		return;
	s_refreshing = true;
	GadgetComboBoxReset(combo);
	int selectedIndex = 0;
	for (size_t i = 0; i < s_sizes.size(); ++i)
	{
		const std::string label = DisplayConfig::resolutionChoiceLabel(s_sizes[i]);
		UnicodeString text;
		text.translate(AsciiString(label.c_str()));
		addEntry(combo, text);
		if (s_sizes[i] == keep)
			selectedIndex = static_cast<int>(i);
	}
	GadgetComboBoxSetSelectedPos(combo, selectedIndex);
	s_refreshing = false;
}

// A label suffix for a control that cannot change during a match (so a disabled control always says why).
void markMainMenuOnly(const char *labelName)
{
	GameWindow *label = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY(labelName));
	if (!label)
		return;
	UnicodeString text = GadgetStaticTextGetText(label);
	if (text.isEmpty())
		return;
	text.concat(TheGameText->FETCH_OR_SUBSTITUTE("GUI:OptionsMainMenuOnly", L" (main menu)"));
	GadgetStaticTextSetText(label, text);
}

void closeBox()
{
	if (s_box)
	{
		TheWindowManager->winDestroy(s_box);
		s_box = nullptr;
	}
	s_boxSeconds = -1;
}

UnicodeString confirmationBody(unsigned remainingMs)
{
	const Config &c = s_tx.pending();
	UnicodeString body = TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayKeepQuestion", L"Keep these display settings?");
	body.concat(L"\n");
	UnicodeString line;
	const char *mode = c.mode == WindowMode::Windowed ? "Windowed" : "Borderless Fullscreen";
	const DisplayGeometry::Size shown = DisplayRuntime::state().resolved.client;
	AsciiString ascii;
	ascii.format("%s  %d x %d  (%s)  monitor %d", mode, shown.width, shown.height, DisplayModes::aspectFamily(shown), c.monitorIndex + 1);
	line.translate(ascii);
	body.concat(line);
	UnicodeString countdown;
	countdown.format(L"\n%ls %u", TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayRevertingIn", L"Reverting in (seconds):").str(),
		(remainingMs + 999) / 1000);
	body.concat(countdown);
	return body;
}

void keepCallback()
{
	s_box = nullptr;                                      // the message box destroys itself after its callback
	s_boxSeconds = -1;
	if (!s_tx.keep())
		return;
	Settings accepted = s_plan.afterKeep;
	// persist what is actually running (a windowed size may have been reduced to fit the desktop)
	const DisplayRuntime::State &state = DisplayRuntime::state();
	if (state.resolved.mode == WindowMode::Windowed && state.resolved.client.valid() && accepted.resolution != state.stored.resolution)
		accepted.resolution = state.resolved.client;
	persist(accepted);
	StartupDiagnostics::checkpoint("display_transaction", "kept", DisplayModes::resolutionLabel(state.resolved.client).c_str());
}

void revertCallback()
{
	s_box = nullptr;
	s_boxSeconds = -1;
	const Outcome outcome = s_tx.revert(applyConfig);
	if (outcome != Outcome::Unchanged)
		relayoutAll();
	StartupDiagnostics::checkpoint("display_transaction", "reverted", outcome == Outcome::SafeFallback ? "safe fallback" : "last known good");
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
	s_resolution = resolutionCombo;
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
	addEntry(s_interface, TheGameText->FETCH_OR_SUBSTITUTE("GUI:InterfaceEnhanced", L"Enhanced (USA, China, GLA)"));

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
		// "1 (Primary)  2560 x 1440 @ 144 Hz" - the refresh rate is the desktop rate Windows presents at (informational)
		AsciiString ascii;
		ascii.format("%d%s  %d x %d", static_cast<int>(i + 1), s_monitors[i].primary ? " (Primary)" : "",
			s_monitors[i].bounds.width, s_monitors[i].bounds.height);
		if (s_monitors[i].desktop.refreshHz > 0)
		{
			AsciiString hz;
			hz.format(" @ %d Hz", s_monitors[i].desktop.refreshHz);
			ascii.concat(hz);
		}
		UnicodeString text;
		text.translate(ascii);
		addEntry(s_monitor, text);
	}

	caption("OptionsMenu.wnd:ModernModeLabel", L"Display Mode", "GUI:DisplayMode");
	s_mode = combo("OptionsMenu.wnd:ModernMode", 2);
	addEntry(s_mode, TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayModeWindowed", L"Windowed"));
	addEntry(s_mode, TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayModeBorderless", L"Borderless Fullscreen"));

	syncFromRuntime();
	rebuildSizes(pendingBorderless(), pendingMonitor());
	if (inMatch)
	{
		// A running match keeps its window and layouts; the controls say so instead of silently greying out.
		for (GameWindow *w : { s_monitor, s_mode, s_uiScale, s_motion, s_interface, s_resolution })
			if (w) w->winEnable(FALSE);
		for (const char *labelName : { "OptionsMenu.wnd:ResolutionLabel", "OptionsMenu.wnd:ModernModeLabel", "OptionsMenu.wnd:ModernMonitorLabel",
		                               "OptionsMenu.wnd:ModernUiScaleLabel", "OptionsMenu.wnd:ModernMotionLabel", "OptionsMenu.wnd:ModernInterfaceLabel" })
			markMainMenuOnly(labelName);
	}
	return true;
}

void populateResolution(GameWindow *combo)
{
	if (!DisplayRuntime::modern() || !combo)
		return;
	if (s_monitors.empty())
		s_monitors = DisplayModes::enumerateMonitors();
	rebuildSizes(pendingBorderless(), pendingMonitor());
	const DisplayRuntime::State &state = DisplayRuntime::state();
	const DisplayGeometry::Size keep = pendingBorderless() ? DisplayGeometry::Size{} : state.resolved.client;
	fillResolutionCombo(combo, keep);
	if (!s_resolution)
		s_resolution = combo;
}

void onSelectionChanged(GameWindow *control)
{
	if (s_refreshing || !control || (control != s_mode && control != s_monitor) || !s_resolution)
		return;
	// the Resolution list follows the pending mode and monitor; the previous choice is kept when the new list offers it
	const DisplayGeometry::Size previous = (selected(s_resolution) >= 0 && selected(s_resolution) < static_cast<Int>(s_sizes.size()))
		? s_sizes[selected(s_resolution)] : DisplayGeometry::Size{};
	rebuildSizes(pendingBorderless(), pendingMonitor());
	DisplayGeometry::Size keep = previous;
	if (!pendingBorderless() && !keep.valid())
		keep = DisplayRuntime::state().settings.resolution;
	fillResolutionCombo(s_resolution, keep);
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
	(void)layoutsAlreadyRebuilt;
	if (!DisplayRuntime::modern() || !s_panelHost)
		return false;
	DisplayRuntime::State &state = DisplayRuntime::state();
	const Settings effective = state.settings;
	const bool inMatch = s_inMatch;

	// what the controls say now (in a match the risky fields are fixed to what is running)
	Settings requested = effective;
	requested.explicitSettings = true;
	if (!inMatch)
	{
		const Int modeIndex = selected(s_mode);
		if (modeIndex >= 0) requested.mode = modeIndex == 0 ? WindowMode::Windowed : WindowMode::Borderless;
		const Int monitorIndex = selected(s_monitor);
		if (monitorIndex >= 0 && monitorIndex < static_cast<Int>(s_monitors.size()))
		{
			requested.monitor = monitorIndex;
			requested.monitorName = DisplayConfig::monitorId(s_monitors[static_cast<size_t>(monitorIndex)]);
		}
		const Int scaleIndex = selected(s_uiScale);
		if (scaleIndex >= 0 && scaleIndex < kUiScaleCount) requested.uiScalePercent = kUiScaleChoices[scaleIndex];
		const Int motionIndex = selected(s_motion);
		if (motionIndex >= 0) requested.smoothMotion = motionIndex == 1;
		const Int interfaceIndex = selected(s_interface);
		if (interfaceIndex >= 0) requested.enhancedHud = interfaceIndex == 1;
		// windowed size: the combo (borderless always runs at the desktop size, the remembered windowed size is untouched)
		const Int sizeIndex = selected(s_resolution);
		if (requested.mode == WindowMode::Windowed && sizeIndex >= 0 && sizeIndex < static_cast<Int>(s_sizes.size()) &&
		    s_sizes[static_cast<size_t>(sizeIndex)] != state.resolved.client)
			requested.resolution = s_sizes[static_cast<size_t>(sizeIndex)];
	}
	const Int vsyncIndex = selected(s_vsync);
	if (vsyncIndex >= 0) requested.vsync = vsyncIndex == 1;

	DisplayConfig::PersistPlan plan = DisplayConfig::planPersistence(state.stored, effective, requested);
	bool relayout = false;
	s_failed = false;

	if (!inMatch && plan.riskyChanged)
	{
		const Config good = configOfRuntime();
		Config want = good;
		want.monitorIndex = requested.monitor;
		want.monitorName = requested.monitorName;
		want.mode = requested.mode;
		if (requested.mode == WindowMode::Windowed && requested.resolution.valid())
			want.client = requested.resolution;
		want.uiScalePercent = requested.uiScalePercent;
		const Outcome outcome = s_tx.begin(good, want, applyConfig, nowMs());
		if (outcome != Outcome::Unchanged)
			relayout = true;
		if (outcome == Outcome::Pending)
		{
			s_plan = plan;
			writeInto(preferences, plan.immediate);     // the risky fields stay as they were until the player keeps the change
		}
		else
		{
			s_failed = outcome == Outcome::RolledBack || outcome == Outcome::SafeFallback;
			writeInto(preferences, plan.immediate);     // nothing risky is persisted for a change that was not kept
		}
	}
	else
		writeInto(preferences, plan.afterKeep);         // nothing risky changed: afterKeep == immediate

	// live, low-risk settings
	if (requested.vsync != effective.vsync)
	{
		if (DisplayRuntime::VsyncFn vsync = DisplayRuntime::vsyncHook())
			vsync(requested.vsync);
		state.settings.vsync = requested.vsync;
	}
	if (!inMatch)
	{
		GroundTranslation::setEnabled(requested.smoothMotion);
		state.settings.smoothMotion = requested.smoothMotion;
		EnhancedHud::setRequested(requested.enhancedHud);   // takes effect when the next match builds its control bar
		if (requested.enhancedHud != effective.enhancedHud)
		{
			state.settings.enhancedHud = requested.enhancedHud;
			relayout = true;   // the control bar is rebuilt like for a UI scale change; the layout is chosen at its creation
		}
	}
	return relayout;
}

bool confirmationPending()
{
	return s_tx.phase() == Phase::AwaitingConfirmation || s_failed;
}

void showConfirmation()
{
	if (s_tx.phase() == Phase::AwaitingConfirmation)
	{
		closeBox();
		const UnicodeString body = confirmationBody(s_tx.remainingMs(nowMs()));
		s_box = TheWindowManager->gogoMessageBox(kDialogCorner, kDialogCorner, -1, -1, MSG_BOX_OK | MSG_BOX_CANCEL,
			TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayChange", L"Display Settings"), body, nullptr, nullptr, keepCallback, revertCallback);
		s_boxSeconds = static_cast<int>((s_tx.remainingMs(nowMs()) + 999) / 1000);
		StartupDiagnostics::checkpoint("display_transaction", "pending", DisplayModes::resolutionLabel(DisplayRuntime::state().resolved.client).c_str());
	}
	else if (s_failed)
	{
		s_failed = false;
		TheWindowManager->gogoMessageBox(kDialogCorner, kDialogCorner, -1, -1, MSG_BOX_OK,
			TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayChange", L"Display Settings"),
			TheGameText->FETCH_OR_SUBSTITUTE("GUI:DisplayChangeFailed",
				L"The selected display settings could not be applied. The previous settings were restored."),
			nullptr, nullptr, nullptr, nullptr);
		StartupDiagnostics::checkpoint("display_transaction", "rolled_back", "apply failed");
	}
}

void update()
{
	if (s_tx.phase() != Phase::AwaitingConfirmation)
		return;
	const unsigned now = nowMs();
	Outcome outcome = Outcome::Unchanged;
	if (s_tx.tick(now, applyConfig, &outcome))
	{
		// unanswered: back to the last known good, by itself (the message box is closed first, then the layouts are rebuilt)
		closeBox();
		relayoutAll();
		StartupDiagnostics::checkpoint("display_transaction", "timeout_revert", outcome == Outcome::SafeFallback ? "safe fallback" : "last known good");
		return;
	}
	if (s_box)
	{
		const int seconds = static_cast<int>((s_tx.remainingMs(now) + 999) / 1000);
		if (seconds != s_boxSeconds)
		{
			s_boxSeconds = seconds;
			if (GameWindow *text = TheWindowManager->winGetWindowFromId(s_box, NAMEKEY("MessageBox.wnd:StaticTextMessage")))
				GadgetStaticTextSetText(text, confirmationBody(s_tx.remainingMs(now)));
		}
	}
}

void forget()
{
	s_panelHost = s_resolution = s_monitor = s_mode = s_uiScale = s_vsync = s_motion = s_interface = nullptr;
	s_monitors.clear();
	s_inMatch = false;
	// s_sizes, the transaction and the message box outlive the Options screen on purpose: the confirmation is shown after it closed
}

}  // namespace ModernDisplayOptions
