// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX2 Options categories. See OptionsTabs.h and Common/OptionsPageLayout.h.
#include "PreRTS.h"

#include "GameClient/OptionsTabs.h"

#include "Common/DisplayRuntime.h"
#include "Common/ModernExperience.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionsPageLayout.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/ModernDisplayOptions.h"

#include <cstring>
#include <vector>

namespace
{
using namespace OptionsPageLayout;

enum Kind { KIND_COMBO, KIND_SLIDER, KIND_CHECK };

struct Item
{
	const char *label;     // window name without the layout prefix (nullptr: the control carries its own text)
	const char *control;
	Kind kind;
};

const Item kDisplay[] = {
	{"ResolutionLabel", "ComboBoxResolution", KIND_COMBO},
	{"ModernModeLabel", "ModernMode", KIND_COMBO},
	{"ModernMonitorLabel", "ModernMonitor", KIND_COMBO},
	{"ModernUiScaleLabel", "ModernUiScale", KIND_COMBO},
	{"ModernVsyncLabel", "ModernVsync", KIND_COMBO},
	{"FrameRateLimitLabel", "ComboBoxFrameRateLimit", KIND_COMBO},
};
const Item kGraphics[] = {
	{"DetailLabel", "ComboBoxDetail", KIND_COMBO},
	{"GammaLabel", "SliderGamma", KIND_SLIDER},
};
const Item kAudio[] = {
	{"SFXVolumeLabel", "SliderSFXVolume", KIND_SLIDER},
	{"MusicVolumeLabel", "SliderMusicVolume", KIND_SLIDER},
	{"VoiceVolumeLabel", "SliderVoiceVolume", KIND_SLIDER},
};
const Item kControls[] = {
	{nullptr, "CheckAlternateMouse", KIND_CHECK},
	{"ScrollSpeedLabel", "SliderScrollSpeed", KIND_SLIDER},
};
const Item kGameplay[] = {
	{"ModernMotionLabel", "ModernMotion", KIND_COMBO},
};

struct Page
{
	const Item *items;
	int count;
	const char *panel;      // retail panel that hosts the page's controls
};

// Network keeps its retail panel (rows already spaced for a 485 unit wide area) and is only moved.
const Page kPages[kCategoryCount] = {
	{kDisplay, 6, "VideoParent"},
	{kGraphics, 2, "VideoParent"},
	{kAudio, 3, "AudioParent"},
	{kControls, 2, "ScrollParent"},
	{kGameplay, 1, "VideoParent"},
	{nullptr, 0, "NetworkParent"},
};

const wchar_t *const kFallbackWide[kCategoryCount] = {L"DISPLAY", L"GRAPHICS", L"AUDIO", L"CONTROLS", L"GAMEPLAY", L"NETWORK"};
const char *const kTabIds[kCategoryCount] = {
	"OptionsMenu.wnd:TabDisplay", "OptionsMenu.wnd:TabGraphics", "OptionsMenu.wnd:TabAudio",
	"OptionsMenu.wnd:TabControls", "OptionsMenu.wnd:TabGameplay", "OptionsMenu.wnd:TabNetwork",
};
const char *const kRetiredLines[] = {"Line1", "Line2", "Line3", "Line4"};

GameWindow *s_tabs[kCategoryCount] = {};
NameKeyType s_tabKeys[kCategoryCount] = {};
int s_active = -1;
double s_scale = 1.0;
int s_originX = 0, s_originY = 0;           // physical position of reference (0,0) of the Options canvas
std::vector<GameWindow *> s_panels;         // every panel that hosts a page
std::vector<GameWindow *> s_initiallyHidden;

GameWindow *byName(const char *name)
{
	if (!name)
		return nullptr;
	char full[128];
	snprintf(full, sizeof(full), "OptionsMenu.wnd:%s", name);
	return TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(full));
}

bool wasHidden(GameWindow *w)
{
	for (size_t i = 0; i < s_initiallyHidden.size(); ++i)
		if (s_initiallyHidden[i] == w)
			return true;
	return false;
}

void rememberHidden(GameWindow *panel)
{
	for (GameWindow *c = panel->winGetChild(); c; c = c->winGetNext())
		if (c->winIsHidden() && !wasHidden(c))
			s_initiallyHidden.push_back(c);
}

void leftAlign(GameWindow *w)
{
	if (!w || !BitIsSet(w->winGetStyle(), GWS_STATIC_TEXT))
		return;
	TextData *data = (TextData *)w->winGetUserData();
	if (data)
	{
		data->centered = FALSE;
		data->centeredVertically = TRUE;
	}
}

// panel-relative placement of `w` at the absolute physical rectangle `r`
void place(GameWindow *w, const Rect &r)
{
	GameWindow *parent = w->winGetParent();
	Int px = 0, py = 0;
	if (parent)
		parent->winGetScreenPosition(&px, &py);
	w->winSetPosition(r.x - px, r.y - py);
	w->winSetSize(r.w, r.h);
}

void placePanel(GameWindow *panel, const Rect &content, bool resize)
{
	GameWindow *parent = panel->winGetParent();
	Int px = 0, py = 0;
	if (parent)
		parent->winGetScreenPosition(&px, &py);
	panel->winSetPosition(content.x - px, content.y - py);
	if (resize)
		panel->winSetSize(content.w, content.h);
}

bool itemShown(const Item &item)
{
	if (item.control && std::strcmp(item.control, "ModernMonitor") == 0)
		return ModernDisplayOptions::hasMultipleMonitors();
	return true;
}

void showCategory(int category)
{
	if (category < 0 || category >= kCategoryCount)
		return;
	s_active = category;
	const Page &page = kPages[category];

	// hide every page panel first, then show/arrange the ones this category uses
	for (size_t i = 0; i < s_panels.size(); ++i)
		s_panels[i]->winHide(TRUE);
	GameWindow *panel = byName(page.panel);
	if (!panel)
		return;

	// count the rows that are visible for this category (the Monitor row only exists with several monitors)
	int rows = 0;
	for (int i = 0; i < page.count; ++i)
		if (itemShown(page.items[i]))
			++rows;
	const OptionsPageLayout::Page layout = computePage(s_scale, rows, s_originX, s_originY);

	if (!page.items)
	{
		// Network: keep the retail rows, move the panel to the content origin
		placePanel(panel, layout.content, false);
		panel->winHide(FALSE);
	}
	else
	{
		placePanel(panel, layout.content, true);
		panel->winHide(FALSE);
		// hide every child that is not an item of this page (and was not hidden by the layout in the first place)
		for (GameWindow *c = panel->winGetChild(); c; c = c->winGetNext())
		{
			bool listed = false;
			for (int i = 0; i < page.count && !listed; ++i)
			{
				const Item &it = page.items[i];
				listed = c == byName(it.control) || (it.label && c == byName(it.label));
			}
			if (!listed)
				c->winHide(TRUE);
		}
		int row = 0;
		for (int i = 0; i < page.count; ++i)
		{
			const Item &it = page.items[i];
			GameWindow *control = byName(it.control);
			GameWindow *label = it.label ? byName(it.label) : nullptr;
			if (!itemShown(it))
			{
				if (control) control->winHide(TRUE);
				if (label) label->winHide(TRUE);
				continue;
			}
			if (!control)
				continue;
			const Row &r = layout.rows[row++];
			if (it.kind == KIND_CHECK)
			{
				Rect span = {r.label.x, r.label.y, r.control.right() - r.label.x, r.control.h};
				place(control, span);
			}
			else
			{
				if (label)
				{
					place(label, r.label);
					leftAlign(label);
					label->winHide(FALSE);
				}
				place(control, r.control);
			}
			control->winHide(FALSE);
		}
		// z-order: upper rows in front of lower rows so a dropdown opens over what is below it
		for (int i = page.count - 1; i >= 0; --i)
			if (GameWindow *control = byName(page.items[i].control))
				if (!control->winIsHidden() && control->winGetParent() == panel)
					control->winBringToTop();
	}

	// tab selection look
	for (int i = 0; i < kCategoryCount; ++i)
		if (s_tabs[i])
			GadgetCheckLikeButtonSetVisualCheck(s_tabs[i], i == category);
}

void copyAppearance(GameWindow *target, GameWindow *source)
{
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
}  // namespace

namespace OptionsTabs
{

bool create()
{
	forget();
	if (!DisplayRuntime::modern() || ModernExperience::classic())
		return false;
	GameWindow *canvas = byName("OptionsMenuParent");
	GameWindow *donor = byName("ButtonDefaults");
	GameWindow *video = byName("VideoParent");
	if (!canvas || !donor || !video || !byName("ComboBoxResolution"))
		return false;

	// reference (0,0) of the Options canvas, and the live scale, from the retail parent's own geometry (120,12 541x585)
	Int cx, cy, cw, ch;
	canvas->winGetScreenPosition(&cx, &cy);
	canvas->winGetSize(&cw, &ch);
	s_scale = cw > 0 ? cw / 541.0 : 1.0;
	s_originX = cx - scaled(120, s_scale);
	s_originY = cy - scaled(12, s_scale);

	for (int i = 0; i < kCategoryCount; ++i)
	{
		GameWindow *panel = byName(kPages[i].panel);
		if (!panel)
			continue;
		bool known = false;
		for (size_t j = 0; j < s_panels.size(); ++j)
			known |= s_panels[j] == panel;
		if (!known)
		{
			s_panels.push_back(panel);
			rememberHidden(panel);
		}
	}
	// the retail section separators belong to the single-page layout
	for (const char *name : kRetiredLines)
		if (GameWindow *line = byName(name))
			line->winHide(TRUE);

	const Tabs tabs = computeTabs(s_scale, s_originX, s_originY);
	GameWindow *tabParent = canvas;
	Int tx, ty;
	tabParent->winGetScreenPosition(&tx, &ty);
	for (int i = 0; i < kCategoryCount; ++i)
	{
		WinInstanceData instance;
		instance.m_font = donor->winGetFont();
		instance.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
		s_tabKeys[i] = TheNameKeyGenerator->nameToKey(kTabIds[i]);
		instance.m_id = s_tabKeys[i];
		GameWindow *tab = TheWindowManager->gogoGadgetPushButton(tabParent, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP,
			tabs.tab[i].x - tx, tabs.tab[i].y - ty, tabs.tab[i].w, tabs.tab[i].h, &instance, instance.m_font, FALSE);
		if (!tab)
			continue;
		copyAppearance(tab, donor);
		GadgetButtonEnableCheckLike(tab, TRUE, FALSE);
		GadgetButtonSetText(tab, TheGameText->FETCH_OR_SUBSTITUTE(categoryTextKey(static_cast<Category>(i)), kFallbackWide[i]));
		s_tabs[i] = tab;
	}
	// tab buttons are in front of the page panels
	for (int i = kCategoryCount - 1; i >= 0; --i)
		if (s_tabs[i])
			s_tabs[i]->winBringToTop();

	showCategory(static_cast<int>(Category::Display));
	char detail[160];
	snprintf(detail, sizeof(detail), "tabs=%d scale=%.2f content=%dx%d", kCategoryCount, s_scale,
		scaled(kContent.w, s_scale), scaled(kContent.h, s_scale));
	StartupDiagnostics::checkpoint("options_tabs", "complete", detail);
	return true;
}

bool onButton(int controlId)
{
	if (s_active < 0)
		return false;
	for (int i = 0; i < kCategoryCount; ++i)
		if (s_tabs[i] && controlId == static_cast<int>(s_tabKeys[i]))
		{
			if (i != s_active)
				showCategory(i);
			else
				GadgetCheckLikeButtonSetVisualCheck(s_tabs[i], TRUE);
			return true;
		}
	return false;
}

void forget()
{
	for (int i = 0; i < kCategoryCount; ++i)
	{
		s_tabs[i] = nullptr;
		s_tabKeys[i] = NAMEKEY_INVALID;
	}
	s_panels.clear();
	s_initiallyHidden.clear();
	s_active = -1;
}

int activeCategory() { return s_active; }

}  // namespace OptionsTabs
