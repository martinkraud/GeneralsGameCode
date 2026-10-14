// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 FIX3 Options categories. See OptionsTabs.h and Common/OptionsPageLayout.h.
//
// Ownership model (FIX3). FIX2 hid only the children of the panels it knew about. Several retail controls (the
// volume/scroll sliders, the unnamed section headers) are not children of those panels, so after a page switch they
// kept drawing at the rows of the page they had been placed on: segmented slider art through the Resolution, Detail
// and Unit Motion combos, and the "VIDEO"/"AUDIO" headers under the tab strip. Visibility is now decided for the
// WHOLE Options layout tree on every page switch, by exclusion: a window is visible only if it is shared chrome, an
// item of the active page (or the retail Network block while it is active), or an ancestor of one of those.
#include "PreRTS.h"

#include "GameClient/OptionsTabs.h"

#include "Common/DisplayRuntime.h"
#include "Common/ModernExperience.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionsPageLayout.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/ModernDisplayOptions.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
using namespace OptionsPageLayout;

const char *const kTabIds[kCategoryCount] = {
	"OptionsMenu.wnd:TabDisplay", "OptionsMenu.wnd:TabGraphics", "OptionsMenu.wnd:TabAudio",
	"OptionsMenu.wnd:TabControls", "OptionsMenu.wnd:TabGameplay", "OptionsMenu.wnd:TabNetwork",
};
const wchar_t *const kFallbackWide[kCategoryCount] = {L"DISPLAY", L"GRAPHICS", L"AUDIO", L"CONTROLS", L"GAMEPLAY", L"NETWORK"};
// retail windows that stay on every page
const char *const kChrome[] = {"OptionsMenuParent", "OptionsMenuParentOld", "ButtonBack", "ButtonAccept", "ButtonDefaults",
                               "LabelVersion", "Line"};
// panels that host the pages' retail controls (moved/resized to the content rectangle; they are containers)
const char *const kPanels[] = {"VideoParent", "AudioParent", "ScrollParent"};

GameWindow *s_tabs[kCategoryCount] = {};
NameKeyType s_tabKeys[kCategoryCount] = {};
int s_active = -1;
double s_scale = 1.0;
int s_originX = 0, s_originY = 0;      // physical position of reference (0,0) of the Options canvas
GameWindow *s_canvas = nullptr;
GameWindow *s_root = nullptr;
GameWindow *s_popup = nullptr;          // retail Advanced Display panel: its subtree is never touched
GameWindow *s_network = nullptr;
std::vector<GameWindow *> s_initiallyHidden;

GameWindow *byName(const char *name)
{
	if (!name)
		return nullptr;
	char full[128];
	snprintf(full, sizeof(full), "OptionsMenu.wnd:%s", name);
	return TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(full));
}

NameKeyType keyOf(const char *name)
{
	char full[128];
	snprintf(full, sizeof(full), "OptionsMenu.wnd:%s", name);
	return TheNameKeyGenerator->nameToKey(full);
}

bool wasHidden(GameWindow *w)
{
	for (size_t i = 0; i < s_initiallyHidden.size(); ++i)
		if (s_initiallyHidden[i] == w)
			return true;
	return false;
}

void recordHidden(GameWindow *w)
{
	for (GameWindow *c = w->winGetChild(); c; c = c->winGetNext())
	{
		if (c->winIsHidden())
			s_initiallyHidden.push_back(c);
		recordHidden(c);
	}
}

bool isContainer(GameWindow *w)
{
	return BitIsSet(w->winGetStyle(), GWS_USER_WINDOW) != FALSE;
}

struct Visibility
{
	std::vector<NameKeyType> active;     // ids of the active page's controls and labels
	NameKeyType chrome[sizeof(kChrome) / sizeof(kChrome[0])];
	int activeCategory = -1;
};

bool isOneOf(NameKeyType id, const std::vector<NameKeyType> &v)
{
	for (size_t i = 0; i < v.size(); ++i)
		if (v[i] == id)
			return true;
	return false;
}

bool isTab(GameWindow *w)
{
	const NameKeyType id = static_cast<NameKeyType>(w->winGetWindowId());
	for (int i = 0; i < kCategoryCount; ++i)
		if (s_tabs[i] && id == s_tabKeys[i])
			return true;
	return false;
}

// The OPTIONS title: the unnamed retail static text above the tab strip.
bool isTitle(GameWindow *w)
{
	if (!BitIsSet(w->winGetStyle(), GWS_STATIC_TEXT))
		return false;
	Int x, y;
	w->winGetScreenPosition(&x, &y);
	return (y - s_originY) < scaled(kTabStrip.y, s_scale);
}

// returns true when `w` (or a descendant) stays visible
bool apply(GameWindow *w, const Visibility &v, bool inNetwork)
{
	if (w == s_popup)
		return true;                      // the Advanced Display panel manages itself
	if (isTab(w))
		return true;
	const NameKeyType id = static_cast<NameKeyType>(w->winGetWindowId());
	bool self = isOneOf(id, v.active);
	for (size_t i = 0; i < sizeof(v.chrome) / sizeof(v.chrome[0]) && !self; ++i)
		self = id == v.chrome[i];
	if (!self && isTitle(w))
		self = true;               // the retail OPTIONS title: any static text above the tab strip
	if (inNetwork && !wasHidden(w))
		self = true;                      // the retail Network block is kept whole (as it was shown)
	if (self && wasHidden(w) && !isOneOf(id, v.active))
		self = false;                     // retail hid it deliberately (e.g. the obsolete send-delay switch)

	bool any = false;
	if (isContainer(w) || w == s_root)
	{
		const bool childNetwork = inNetwork || (w == s_network && v.activeCategory == static_cast<int>(Category::Network));
		for (GameWindow *c = w->winGetChild(); c; c = c->winGetNext())
			any |= apply(c, v, childNetwork);
	}
	const bool show = self || any;
	w->winHide(show ? FALSE : TRUE);
	return show;
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

bool itemShown(const PageItem &item)
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
	const PageTable table = pageItems(static_cast<Category>(category));

	int rows = 0;
	for (int i = 0; i < table.count; ++i)
		if (itemShown(table.items[i]))
			++rows;
	const OptionsPageLayout::Page layout = computePage(s_scale, rows, s_originX, s_originY);

	// 1. geometry: the hosting panels cover the content rectangle; every item goes to its row (relative to its own
	//    parent, whichever panel that is)
	for (const char *name : kPanels)
		if (GameWindow *panel = byName(name))
			placePanel(panel, layout.content, true);
	if (s_network)
		placePanel(s_network, layout.content, false);
	std::vector<GameWindow *> frontToBack;
	int row = 0;
	for (int i = 0; i < table.count; ++i)
	{
		const PageItem &it = table.items[i];
		if (!itemShown(it))
			continue;
		GameWindow *control = byName(it.control);
		GameWindow *label = it.label ? byName(it.label) : nullptr;
		if (!control)
			continue;
		const Row &r = layout.rows[row++];
		if (it.kind == ControlKind::Check)
		{
			place(control, Rect{r.label.x, r.label.y, r.control.right() - r.label.x, r.control.h});
		}
		else
		{
			if (label)
			{
				place(label, r.label);
				leftAlign(label);
			}
			place(control, r.control);
		}
		frontToBack.push_back(control);
	}

	// 2. visibility for the whole layout tree
	Visibility v;
	v.activeCategory = category;
	for (int i = 0; i < table.count; ++i)
	{
		const PageItem &it = table.items[i];
		if (!itemShown(it))
			continue;
		if (it.control) v.active.push_back(keyOf(it.control));
		if (it.label) v.active.push_back(keyOf(it.label));
	}
	for (size_t i = 0; i < sizeof(kChrome) / sizeof(kChrome[0]); ++i)
		v.chrome[i] = keyOf(kChrome[i]);
	apply(s_root, v, false);

	// 3. z-order: upper rows are brought to the front last, so a dropdown opens over the rows below it
	for (size_t i = frontToBack.size(); i-- > 0;)
		frontToBack[i]->winBringToTop();
	for (int i = kCategoryCount - 1; i >= 0; --i)
		if (s_tabs[i])
			s_tabs[i]->winBringToTop();

	// 4. selection look is drawn from s_active (tabDraw)
}

// ---- tab drawing: dark Generals-style tab with orange line work; the selected tab is gold ---------------------
void tabDraw(GameWindow *window, WinInstanceData *instData)
{
	Int x, y, w, h;
	window->winGetScreenPosition(&x, &y);
	window->winGetSize(&w, &h);
	const int index = static_cast<int>(reinterpret_cast<intptr_t>(GadgetButtonGetData(window)));
	const bool selected = index == s_active;
	const bool hilited = BitIsSet(instData->getState(), WIN_STATE_HILITED) != FALSE;
	const bool enabled = BitIsSet(window->winGetStatus(), WIN_STATUS_ENABLED) != FALSE;

	const Color fill = selected ? GameMakeColor(34, 24, 0, 245) : GameMakeColor(0, 0, 0, 230);
	const Color line = selected ? GameMakeColor(255, 214, 0, 255) : (hilited ? GameMakeColor(255, 170, 40, 255) : GameMakeColor(190, 105, 0, 255));
	const Color text = selected ? GameMakeColor(255, 230, 90, 255) : (hilited ? GameMakeColor(255, 255, 255, 255) : GameMakeColor(236, 170, 70, 255));
	const Int border = std::max<Int>(1, static_cast<Int>(DisplayRuntime::uiScale() + 0.5));

	TheDisplay->drawFillRect(x, y, w, h, fill);
	TheDisplay->drawOpenRect(x, y, w, h, selected ? border * 2 : border, line);

	DisplayString *label = instData->getTextDisplayString();
	if (!label || label->getTextLength() == 0)
		return;
	if (label->getFont() != window->winGetFont())
		label->setFont(window->winGetFont());
	label->setWordWrap(w);
	Int tw, th;
	label->getSize(&tw, &th);
	label->draw(x + (w - tw) / 2, y + (h - th) / 2, enabled ? text : GameMakeColor(120, 120, 120, 255), GameMakeColor(0, 0, 0, 255));
}

int measure(GameWindow *donor, const UnicodeString &text, double fallbackPerChar)
{
	int width = static_cast<int>(text.getLength() * fallbackPerChar);
	if (TheDisplayStringManager)
	{
		if (DisplayString *ds = TheDisplayStringManager->newDisplayString())
		{
			ds->setFont(donor->winGetFont());
			ds->setText(text);
			Int w = 0, h = 0;
			ds->getSize(&w, &h);
			if (w > 0)
				width = w;
			TheDisplayStringManager->freeDisplayString(ds);
		}
	}
	return width;
}
}  // namespace

namespace OptionsTabs
{

bool create()
{
	forget();
	if (!DisplayRuntime::modern() || ModernExperience::classic())
		return false;
	s_canvas = byName("OptionsMenuParent");
	GameWindow *donor = byName("ButtonDefaults");
	GameWindow *video = byName("VideoParent");
	if (!s_canvas || !donor || !video || !byName("ComboBoxResolution"))
		return false;
	s_popup = byName("WinAdvancedDisplayOptions");
	s_network = byName("NetworkParent");
	s_root = s_canvas;
	while (s_root->winGetParent())
		s_root = s_root->winGetParent();

	Int cx, cy, cw, ch;
	s_canvas->winGetScreenPosition(&cx, &cy);
	s_canvas->winGetSize(&cw, &ch);
	s_scale = cw > 0 ? cw / 541.0 : 1.0;
	s_originX = cx - scaled(120, s_scale);
	s_originY = cy - scaled(12, s_scale);

	recordHidden(s_root);

	// tab strip: measured caption widths (retail Arial at the live scale), padding, slack shared so the strip
	// aligns with the page content on both edges
	UnicodeString captions[kCategoryCount];
	int measured[kCategoryCount];
	for (int i = 0; i < kCategoryCount; ++i)
	{
		captions[i] = TheGameText->FETCH_OR_SUBSTITUTE(categoryTextKey(static_cast<Category>(i)), kFallbackWide[i]);
		measured[i] = measure(donor, captions[i], 7.0 * s_scale);
	}
	const TabStrip strip = computeTabStrip(measured, s_scale, s_originX + scaled(kTabStrip.x, s_scale),
		s_originY + scaled(kTabStrip.y, s_scale), scaled(kTabStrip.w, s_scale), scaled(kTabStrip.h, s_scale));

	Int tx, ty;
	s_canvas->winGetScreenPosition(&tx, &ty);
	for (int i = 0; i < kCategoryCount; ++i)
	{
		WinInstanceData instance;
		instance.m_font = donor->winGetFont();
		instance.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
		s_tabKeys[i] = TheNameKeyGenerator->nameToKey(kTabIds[i]);
		instance.m_id = s_tabKeys[i];
		GameWindow *tab = TheWindowManager->gogoGadgetPushButton(s_canvas, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP,
			strip.tab[i].x - tx, strip.tab[i].y - ty, strip.tab[i].w, strip.tab[i].h, &instance, instance.m_font, FALSE);
		if (!tab)
			continue;
		tab->winSetFont(donor->winGetFont());
		tab->winSetDrawFunc(tabDraw);
		GadgetButtonSetText(tab, captions[i]);
		GadgetButtonSetData(tab, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
		s_tabs[i] = tab;
	}

	showCategory(static_cast<int>(Category::Display));
	char detail[200];
	snprintf(detail, sizeof(detail), "tabs=%d scale=%.2f content=%dx%d strip=%dx%d padded=%d", kCategoryCount, s_scale,
		scaled(kContent.w, s_scale), scaled(kContent.h, s_scale), strip.strip.w, strip.strip.h, strip.padded ? 1 : 0);
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
	s_initiallyHidden.clear();
	s_canvas = s_root = s_popup = s_network = nullptr;
	s_active = -1;
}

int activeCategory() { return s_active; }

}  // namespace OptionsTabs
