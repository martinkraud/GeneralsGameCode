// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience Options (X64.33 categories, redesigned by the Modern Frontend milestone). See GameClient/OptionsTabs.h.
//
// Ownership model. The retail Options layout is a tree of containers (the Video / Audio / Scroll / Network panels) with the gadgets inside. Visibility is decided for the WHOLE tree on every
// page switch by exclusion (a window is visible only if it is shared chrome, an item of the active page and inside the scroll viewport, or an ancestor of one of those), so a retail control
// can never keep drawing at the row of the page it was last placed on. The Modern chrome (panel, title, rules, values, status) is ONE backdrop window drawing a ModernDraw::Decor; the
// controls are the retail gadgets, skinned with the Modern components and re-placed from ModernScreen::compute(). Their callbacks are the retail ones.
#include "PreRTS.h"

#include "GameClient/OptionsTabs.h"

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <vector>

#include "Common/DisplayRuntime.h"
#include "Common/ModernBranding.h"
#include "Common/ModernExperience.h"
#include "Common/ModernSettingsModel.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionsPageLayout.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/Display.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetCheckBox.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetSlider.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/MessageBox.h"
#include "GameClient/ModernDisplayOptions.h"
#include "GameClient/ModernDraw.h"

namespace
{
using namespace OptionsPageLayout;
using ModernTheme::TypeRole;

const char *const kTabIds[kCategoryCount] = {
	"OptionsMenu.wnd:TabDisplay", "OptionsMenu.wnd:TabGraphics", "OptionsMenu.wnd:TabAudio",
	"OptionsMenu.wnd:TabControls", "OptionsMenu.wnd:TabGameplay", "OptionsMenu.wnd:TabModern", "OptionsMenu.wnd:TabNetwork",
};
const wchar_t *const kFallbackWide[kCategoryCount] = { L"Display", L"Graphics", L"Audio", L"Controls", L"Gameplay", L"Modern Experience", L"Network" };
// retail windows that stay on every page (the Modern backdrop is added at run time)
const char *const kChrome[] = { "OptionsMenuParent", "ButtonBack", "ButtonAccept", "ButtonDefaults" };
// panels that host the pages' retail controls (moved/resized to the content rectangle; they are containers)
const char *const kPanels[] = { "VideoParent", "AudioParent", "ScrollParent" };

GameWindow *s_tabs[kCategoryCount] = {};
NameKeyType s_tabKeys[kCategoryCount] = {};
int s_active = -1;
int s_firstRow = 0;
int s_surfaceW = 0, s_surfaceH = 0, s_uiPercent = 0;
ModernScreen::Layout s_layout;
GameWindow *s_canvas = nullptr;
GameWindow *s_root = nullptr;
GameWindow *s_popup = nullptr;          // retail Advanced Display panel: its subtree is never touched
GameWindow *s_network = nullptr;
GameWindow *s_backdrop = nullptr;
bool s_advancedOpen = false;
ModernDraw::Decor s_decor;
std::vector<GameWindow *> s_initiallyHidden;

ModernSettings::Tracker s_tracker;                       // the pending-changes model (pure)
GameWindow *s_tracked[ModernSettings::kMaxControls] = {};   // the gadget behind each tracker slot
bool s_dirty = false;
int s_lastSliderSum = -1;
void (*s_discard)() = nullptr;

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

bool isContainer(GameWindow *w) { return BitIsSet(w->winGetStyle(), GWS_USER_WINDOW) != FALSE; }

struct Visibility
{
	std::vector<NameKeyType> active;     // ids of the visible controls and labels of the active page
	std::vector<NameKeyType> chrome;
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

// returns true when `w` (or a descendant) stays visible
bool apply(GameWindow *w, const Visibility &v, bool inNetwork)
{
	if (w == s_popup)
		return true;                      // the Advanced Display panel manages itself
	if (isTab(w))
		return true;
	const NameKeyType id = static_cast<NameKeyType>(w->winGetWindowId());
	bool self = isOneOf(id, v.active) || isOneOf(id, v.chrome);
	if (inNetwork && !wasHidden(w))
		self = true;                      // the retail Network block is kept whole (as it was shown)
	if (self && wasHidden(w) && !isOneOf(id, v.active) && !isOneOf(id, v.chrome))
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

void place(GameWindow *w, const ModernScreen::Rect &r)
{
	GameWindow *parent = w->winGetParent();
	Int px = 0, py = 0;
	if (parent)
		parent->winGetScreenPosition(&px, &py);
	w->winSetPosition(r.x - px, r.y - py);
	w->winSetSize(r.w, r.h);
}

void placePanel(GameWindow *panel, const ModernScreen::Rect &content, bool resize)
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

UnicodeString upper(const UnicodeString &s)
{
	UnicodeString u;
	for (const WideChar *p = s.str(); p && *p; ++p) u.concat(static_cast<WideChar>(towupper(*p)));
	return u;
}

int pageRowCount(int category)
{
	const PageTable table = pageItems(static_cast<Category>(category));
	int rows = 0;
	for (int i = 0; i < table.count; ++i)
		if (itemShown(table.items[i]))
			++rows;
	return rows;
}

// ---- values of the settings (dirty state, slider labels) ----
int controlValue(GameWindow *w)
{
	if (!w) return 0;
	const UnsignedInt style = w->winGetStyle();
	if (BitIsSet(style, GWS_COMBO_BOX)) { Int pos = -1; GadgetComboBoxGetSelectedPos(w, &pos); return pos; }
	if (BitIsSet(style, GWS_HORZ_SLIDER)) return GadgetSliderGetPosition(w);
	if (BitIsSet(style, GWS_CHECK_BOX)) return GadgetCheckBoxIsChecked(w) ? 1 : 0;
	return 0;
}

void takeSnapshot()
{
	s_tracker.clear();
	for (GameWindow *&w : s_tracked) w = nullptr;
	for (int c = 0; c < static_cast<int>(Category::Network); ++c)
	{
		const PageTable t = pageItems(static_cast<Category>(c));
		for (int i = 0; i < t.count; ++i)
			if (GameWindow *w = byName(t.items[i].control))
			{
				const int slot = s_tracker.add(static_cast<int>(w->winGetWindowId()), controlValue(w));
				if (slot >= 0) s_tracked[slot] = w;
			}
	}
	s_dirty = false;
}

bool computeDirty()
{
	for (int i = 0; i < s_tracker.count; ++i)
		if (s_tracked[i]) s_tracker.current[i] = controlValue(s_tracked[i]);
	return s_tracker.dirty();
}

int sliderSum()
{
	int sum = 0;
	for (int i = 0; i < s_tracker.count; ++i)
		if (s_tracked[i] && BitIsSet(s_tracked[i]->winGetStyle(), GWS_HORZ_SLIDER))
			sum = sum * 31 + GadgetSliderGetPosition(s_tracked[i]) + 1;
	return sum;
}

// ---- decor (the Modern chrome of the screen) ----
UnicodeString captionOf(int category)
{
	return TheGameText->FETCH_OR_SUBSTITUTE(categoryTextKey(static_cast<Category>(category)), kFallbackWide[category]);
}

void rebuildDecor()
{
	ModernDraw::beginScreen(ModernDraw::Screen::Options);
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	const ModernTheme::Palette &p = ModernDraw::palette();
	const ModernScreen::Layout &L = s_layout;
	const ModernScreen::Rect &P = L.panel;
	auto relX = [&](int x) { return x - P.x; };
	auto relY = [&](int y) { return y - P.y; };
	s_decor.clear();

	// the Options screen is an overlay (above the main menu or the game): dim everything below it so the screen underneath never shows through the panel
	ModernDraw::DecorItem &dim = s_decor.add(ModernDraw::DecorKind::Scrim);
	dim.x = -P.x; dim.y = -P.y; dim.w = s_surfaceW; dim.h = s_surfaceH; dim.color = ModernTheme::Color{ 3, 6, 10, 186 };
	ModernDraw::DecorItem &under = s_decor.add(ModernDraw::DecorKind::Scrim);   // the panel itself is translucent over the shot map; an overlay must not show the screen below through it
	under.x = 0; under.y = 0; under.w = P.w; under.h = P.h; under.color = ModernTheme::Color{ 3, 6, 10, 205 };
	const int cut = ModernDraw::px(m.chamfer);
	ModernDraw::DecorItem &panel = s_decor.add(ModernDraw::DecorKind::Panel);
	panel.x = 0; panel.y = 0; panel.w = P.w; panel.h = P.h; panel.cutTL = cut; panel.cutBR = cut;
	ModernDraw::DecorItem &bar = s_decor.add(ModernDraw::DecorKind::Tick);
	bar.x = cut; bar.y = 0; bar.w = ModernDraw::px(110); bar.h = ModernDraw::px(3);

	ModernDraw::DecorItem &title = s_decor.add(ModernDraw::DecorKind::Text);
	title.x = relX(L.title.x); title.y = relY(L.title.y); title.w = L.title.w; title.h = L.title.h; title.role = TypeRole::Title; title.color = p.text;
	const UnicodeString t = upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:Options", L"Options"));
	wcsncpy(title.text, t.str(), 95);
	ModernDraw::DecorItem &rule = s_decor.add(ModernDraw::DecorKind::Rule);
	rule.x = relX(L.titleRule.x); rule.y = relY(L.titleRule.y); rule.w = L.titleRule.w * 14 / 100; rule.h = L.titleRule.h; rule.color = p.accent;
	ModernDraw::DecorItem &rest = s_decor.add(ModernDraw::DecorKind::Rule);
	rest.x = rule.x + rule.w; rest.y = rule.y; rest.w = L.titleRule.w - rule.w; rest.h = L.titleRule.h; rest.color = p.hairline;

	// rail separator
	ModernDraw::DecorItem &sep = s_decor.add(ModernDraw::DecorKind::Rule);
	sep.w = ModernDraw::px(1) > 0 ? ModernDraw::px(1) : 1;
	sep.x = relX(L.rail.right() + (L.content.x - L.rail.right()) / 2); sep.y = relY(L.rail.y); sep.h = L.rail.h; sep.color = p.hairline;

	// the active category name above the rows is the selected tab itself; rows get quiet separators, sliders a value label
	const PageTable table = pageItems(static_cast<Category>(s_active < 0 ? 0 : s_active));
	int shown = 0;
	for (int i = 0; i < table.count; ++i)
	{
		if (!itemShown(table.items[i])) continue;
		const int visual = shown - L.firstRow;
		++shown;
		if (visual < 0 || visual >= L.visibleRows) continue;
		const ModernScreen::Rect &r = L.row[visual];
		ModernDraw::DecorItem &hair = s_decor.add(ModernDraw::DecorKind::Rule);
		const int gapHalf = (L.visibleRows > 1 ? (L.row[1].y - L.row[0].y - r.h) : ModernDraw::px(10)) / 2;
		hair.x = relX(r.x); hair.y = relY(r.bottom() + gapHalf); hair.w = r.w; hair.h = 1; hair.color = p.hairline.withAlpha(80);
		if (table.items[i].kind == ControlKind::Slider)
		{
			GameWindow *w = byName(table.items[i].control);
			Int mn = 0, mx = 0;
			if (w) GadgetSliderGetMinMax(w, &mn, &mx);
			const int pct = (w && mx > mn) ? (GadgetSliderGetPosition(w) - mn) * 100 / (mx - mn) : 0;
			ModernDraw::DecorItem &val = s_decor.add(ModernDraw::DecorKind::Text);
			const int vw = ModernDraw::px(56);
			val.x = relX(L.control[visual].right() - vw); val.y = relY(L.control[visual].y); val.w = vw; val.h = L.control[visual].h;
			val.role = TypeRole::Value; val.align = 2; val.color = p.accent;
			swprintf(val.text, 95, L"%d%%", pct);
		}
	}
	if (L.scrollable)
	{
		ModernDraw::DecorItem &track = s_decor.add(ModernDraw::DecorKind::Rule);
		track.x = relX(L.scrollTrack.x); track.y = relY(L.scrollTrack.y); track.w = L.scrollTrack.w; track.h = L.scrollTrack.h; track.color = p.hairline;
		ModernDraw::DecorItem &thumb = s_decor.add(ModernDraw::DecorKind::Rule);
		thumb.x = relX(L.scrollThumb.x); thumb.y = relY(L.scrollThumb.y); thumb.w = L.scrollThumb.w; thumb.h = L.scrollThumb.h; thumb.color = p.accent;
	}

	// status line: the dirty state (or what Apply does) and the build footer
	ModernDraw::DecorItem &st = s_decor.add(ModernDraw::DecorKind::Text);
	st.x = relX(L.status.x); st.y = relY(L.status.y); st.w = L.status.w * 62 / 100; st.h = L.status.h;
	st.role = TypeRole::Status;
	if (s_dirty)
	{
		st.color = p.warning;
		wcsncpy(st.text, upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:OptionsUnsaved", L"Unsaved changes - Apply to keep them")).str(), 95);
	}
	else
	{
		st.role = TypeRole::Helper; st.color = p.textMuted;
		wcsncpy(st.text, TheGameText->FETCH_OR_SUBSTITUTE("GUI:OptionsHint", L"Volume and scroll speed apply as you move them; the rest applies with Apply.").str(), 95);
	}
	ModernDraw::DecorItem &foot = s_decor.add(ModernDraw::DecorKind::Text);
	foot.x = relX(L.status.x + L.status.w * 62 / 100); foot.y = relY(L.status.y); foot.w = L.status.w - L.status.w * 62 / 100; foot.h = L.status.h;
	foot.role = TypeRole::Helper; foot.align = 2; foot.color = p.textMuted.withAlpha(180);
	wcsncpy(foot.text, ModernBranding::footerText(), 95);
	s_decor.refreshFonts();
}

void styleControls()
{
	ModernDraw::beginScreen(ModernDraw::Screen::Options);
	// the retail hosting panels are filled black with a blue border: they only host, the Modern backdrop is the surface
	for (const char *name : { "VideoParent", "AudioParent", "ScrollParent", "NetworkParent" })
		if (GameWindow *panel = byName(name))
		{
			panel->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
			panel->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
		}
	for (int c = 0; c < static_cast<int>(Category::Network); ++c)
	{
		const PageTable t = pageItems(static_cast<Category>(c));
		for (int i = 0; i < t.count; ++i)
		{
			GameWindow *control = byName(t.items[i].control);
			GameWindow *label = t.items[i].label ? byName(t.items[i].label) : nullptr;
			if (label) ModernDraw::skinText(label, TypeRole::Label, ModernDraw::palette().textSecondary);
			if (!control) continue;
			switch (t.items[i].kind)
			{
			case ControlKind::Combo: ModernDraw::skinCombo(control); break;
			case ControlKind::Slider: ModernDraw::skinSlider(control); break;
			case ControlKind::Check: ModernDraw::skinCheck(control); break;
			default: break;
			}
		}
	}
	if (GameWindow *w = byName("ButtonAccept")) { ModernDraw::skinButton(w, ModernComponents::Kind::Primary, TypeRole::MenuSecondary); GadgetButtonSetText(w, upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:OptionsApply", L"Apply"))); }
	if (GameWindow *w = byName("ButtonDefaults")) { ModernDraw::skinButton(w, ModernComponents::Kind::Secondary, TypeRole::MenuSecondary); GadgetButtonSetText(w, upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:Defaults", L"Defaults"))); }
	if (GameWindow *w = byName("ButtonBack")) { ModernDraw::skinButton(w, ModernComponents::Kind::Secondary, TypeRole::MenuSecondary); GadgetButtonSetText(w, upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:Cancel", L"Cancel"))); }
	for (int i = 0; i < kCategoryCount; ++i)
		if (s_tabs[i]) ModernDraw::skinButton(s_tabs[i], ModernComponents::Kind::Tab, TypeRole::MenuSecondary);
}

void computeLayout()
{
	ModernDraw::beginScreen(ModernDraw::Screen::Options);
	s_surfaceW = TheDisplay->getWidth();
	s_surfaceH = TheDisplay->getHeight();
	s_uiPercent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (s_uiPercent <= 0) s_uiPercent = 100;
	ModernScreen::Input in;
	in.surfaceW = s_surfaceW; in.surfaceH = s_surfaceH; in.uiPercent = s_uiPercent;
	in.tabCount = kCategoryCount;
	in.rowCount = s_active >= 0 ? pageRowCount(s_active) : 0;
	in.firstRow = s_firstRow;
	s_layout = ModernScreen::compute(in);
	s_firstRow = s_layout.firstRow;
	ModernDraw::setScalePercent(s_layout.effectiveScale);
}

void placeChrome()
{
	const ModernScreen::Layout &L = s_layout;
	const int inflate = ModernDraw::px(8);
	place(s_canvas, ModernScreen::Rect{ L.panel.x - inflate, L.panel.y - inflate, L.panel.w + 2 * inflate, L.panel.h + 2 * inflate });
	// OptionsMenuParentOld is the retail frame AND the parent of every gadget of the screen (a child is hit only inside every ancestor's rectangle): it covers the panel and draws nothing
	if (GameWindow *frame = byName("OptionsMenuParentOld"))
	{
		place(frame, L.panel);
		frame->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
		frame->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	}
	if (s_backdrop) place(s_backdrop, L.panel);
	if (GameWindow *w = byName("ButtonDefaults")) place(w, L.defaults);
	if (GameWindow *w = byName("ButtonAccept")) place(w, L.apply);
	if (GameWindow *w = byName("ButtonBack")) place(w, L.back);
	for (int i = 0; i < kCategoryCount; ++i)
		if (s_tabs[i]) place(s_tabs[i], L.tab[i]);
}

void showCategory(int category, bool keepScroll = false)
{
	if (category < 0 || category >= kCategoryCount)
		return;
	if (category != s_active && !keepScroll)
		s_firstRow = 0;
	s_active = category;
	computeLayout();
	placeChrome();
	const PageTable table = pageItems(static_cast<Category>(category));
	const ModernScreen::Layout &L = s_layout;

	// 1. geometry: the hosting panels cover the content viewport; every visible item goes to its row (relative to its own parent, whichever panel that is)
	for (const char *name : kPanels)
		if (GameWindow *panel = byName(name))
			placePanel(panel, L.content, true);
	if (s_network)
		placePanel(s_network, L.content, false);   // the retail Network rows are placed from the content origin
	std::vector<GameWindow *> frontToBack;
	Visibility v;
	v.activeCategory = category;
	int shown = 0;
	for (int i = 0; i < table.count; ++i)
	{
		const PageItem &it = table.items[i];
		if (!itemShown(it))
			continue;
		const int visual = shown - L.firstRow;
		++shown;
		GameWindow *control = byName(it.control);
		GameWindow *label = it.label ? byName(it.label) : nullptr;
		if (!control || visual < 0 || visual >= L.visibleRows)
			continue;
		const ModernScreen::Rect &lab = L.label[visual], &ctl = L.control[visual];
		if (it.kind == ControlKind::Check)
		{
			place(control, ModernScreen::Rect{ lab.x, ctl.y, ctl.right() - lab.x, ctl.h });
		}
		else
		{
			if (label)
			{
				place(label, lab);
				leftAlign(label);
			}
			ModernScreen::Rect c = ctl;
			if (it.kind == ControlKind::Slider) c.w -= ModernDraw::px(56) + ModernDraw::px(8);   // room for the value label
			place(control, c);
		}
		frontToBack.push_back(control);
		v.active.push_back(keyOf(it.control));
		if (it.label) v.active.push_back(keyOf(it.label));
	}

	// 2. visibility for the whole layout tree
	for (const char *c : kChrome)
		v.chrome.push_back(keyOf(c));
	if (s_backdrop)
		v.chrome.push_back(static_cast<NameKeyType>(s_backdrop->winGetWindowId()));
	apply(s_root, v, false);

	// 3. z-order: the backdrop first (everything else is brought above it), upper rows last so a dropdown opens over the rows below it
	if (s_backdrop)
		s_backdrop->winBringToTop();
	if (GameWindow *frame = byName("OptionsMenuParentOld"))
		frame->winBringToTop();                 // the retail frame hosts every gadget: it must draw above the Modern backdrop, never below it
	if (s_network && category == static_cast<int>(Category::Network))
		s_network->winBringToTop();
	for (size_t i = frontToBack.size(); i-- > 0;)
		frontToBack[i]->winBringToTop();
	for (const char *c : { "ButtonDefaults", "ButtonAccept", "ButtonBack" })
		if (GameWindow *w = byName(c)) w->winBringToTop();
	for (int i = kCategoryCount - 1; i >= 0; --i)
		if (s_tabs[i])
		{
			s_tabs[i]->winBringToTop();
			ModernDraw::setSelected(s_tabs[i], i == s_active);
		}

	s_dirty = computeDirty();
	s_lastSliderSum = sliderSum();
	rebuildDecor();
}

void scrollTo(int firstRow)
{
	const int clamped = ModernScreen::clampFirstRow(firstRow, s_layout.rowCount, s_layout.visibleRows);
	if (clamped == s_firstRow) return;
	s_firstRow = clamped;
	showCategory(s_active, true);
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

	// restored retail checkboxes join the host panel of their page (one parent per page; see OptionsPageLayout::kReparented)
	for (const OptionsPageLayout::HostedControl &h : OptionsPageLayout::kReparented)
	{
		GameWindow *control = byName(h.control);
		GameWindow *host = byName(h.hostPanel);
		if (control && host && control->winGetParent() != host)
			control->winSetParent(host);
	}

	ModernDraw::beginScreen(ModernDraw::Screen::Options);
	ModernDraw::clearRegistry(ModernDraw::Screen::Options);   // the title variant was set by the Modern main menu that opened this screen
	s_decor.clear();
	recordHidden(s_root);
	// the retail layout root dims the shell behind the old 800x600 frame; the Modern panel brings its own surface
	s_root->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
	s_root->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));

	// the rail: one push button per category (the retail text of the button is the caption)
	for (int i = 0; i < kCategoryCount; ++i)
	{
		WinInstanceData instance;
		instance.m_font = donor->winGetFont();
		instance.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
		s_tabKeys[i] = TheNameKeyGenerator->nameToKey(kTabIds[i]);
		instance.m_id = s_tabKeys[i];
		GameWindow *tab = TheWindowManager->gogoGadgetPushButton(s_canvas, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP, 0, 0, 100, 30, &instance, instance.m_font, FALSE);
		if (!tab)
			continue;
		GadgetButtonSetText(tab, upper(captionOf(i)));
		GadgetButtonSetData(tab, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
		s_tabs[i] = tab;
	}
	s_backdrop = ModernDraw::createBackdrop(s_canvas, "OptionsMenu.wnd:ModernBackdrop", &s_decor);
	if (s_backdrop)
		s_backdrop->winSetOwner(s_canvas);
	styleControls();

	if (s_popup)
	{
		// the retail Advanced Display panel keeps its size and is centred in the screen
		Int pw, ph;
		s_popup->winGetSize(&pw, &ph);
		GameWindow *pp = s_popup->winGetParent();
		Int px = 0, py = 0;
		if (pp)
			pp->winGetScreenPosition(&px, &py);
		s_popup->winSetPosition((TheDisplay->getWidth() - pw) / 2 - px, (TheDisplay->getHeight() - ph) / 2 - py);
	}

	takeSnapshot();
	s_firstRow = 0;
	showCategory(static_cast<int>(Category::Display));
	char detail[260];
	snprintf(detail, sizeof(detail), "tabs=%d scale=%d effective=%d panel=%dx%d content=%dx%d rows=%d visible=%d", kCategoryCount, s_uiPercent,
		s_layout.effectiveScale, s_layout.panel.w, s_layout.panel.h, s_layout.content.w, s_layout.content.h, s_layout.rowCount, s_layout.visibleRows);
	StartupDiagnostics::checkpoint("options_tabs", "complete", detail);
	return true;
}

// The Advanced Display panel opens as a sub-screen: the rail, the page and the action row are hidden underneath so nothing can collide with it. Closing it restores the page.
void setAdvancedOpen(bool open)
{
	if (s_active < 0 || !s_popup)
		return;
	s_advancedOpen = open;
	if (open)
	{
		Int pw = 0, ph = 0;
		s_popup->winGetSize(&pw, &ph);
		ModernScreen::Rect r{ s_layout.panel.x + (s_layout.panel.w - pw) / 2, s_layout.panel.y + (s_layout.panel.h - ph) / 2, pw, ph };
		if (r.y < s_layout.panel.y) r.y = s_layout.panel.y;
		place(s_popup, r);
		Visibility v;
		v.activeCategory = -1;
		v.chrome.push_back(keyOf("OptionsMenuParent"));
		if (s_backdrop)
			v.chrome.push_back(static_cast<NameKeyType>(s_backdrop->winGetWindowId()));
		apply(s_root, v, false);
		for (int i = 0; i < kCategoryCount; ++i)
			if (s_tabs[i])
				s_tabs[i]->winHide(TRUE);
		s_popup->winHide(FALSE);
		s_popup->winBringToTop();
	}
	else
	{
		for (int i = 0; i < kCategoryCount; ++i)
			if (s_tabs[i])
				s_tabs[i]->winHide(FALSE);
		showCategory(s_active, true);
	}
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

void update()
{
	if (s_active < 0 || !s_canvas || s_advancedOpen)
		return;
	const int uiPercent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (TheDisplay->getWidth() != s_surfaceW || TheDisplay->getHeight() != s_surfaceH || (uiPercent > 0 ? uiPercent : 100) != s_uiPercent)
	{
		showCategory(s_active, true);
		return;
	}
	ModernDraw::setFocusWindow(TheWindowManager->winGetFocus());
	const bool dirtyNow = computeDirty();
	const int sum = sliderSum();
	if (dirtyNow != s_dirty || sum != s_lastSliderSum)
	{
		s_dirty = dirtyNow;
		s_lastSliderSum = sum;
		rebuildDecor();
	}
}

bool onKey(int key, int state)
{
	if (s_active < 0 || s_advancedOpen || !(state & KEY_STATE_DOWN))
		return false;
	int target = s_active;
	switch (key)
	{
	case KEY_PGDN: target = (s_active + 1) % kCategoryCount; break;
	case KEY_PGUP: target = (s_active + kCategoryCount - 1) % kCategoryCount; break;
	case KEY_HOME: if (state & KEY_STATE_CONTROL) target = 0; else return false; break;
	case KEY_END: if (state & KEY_STATE_CONTROL) target = kCategoryCount - 1; else return false; break;
	default: return false;
	}
	if (target != s_active)
		showCategory(target);
	return true;
}

bool onWheel(bool up)
{
	if (s_active < 0 || s_advancedOpen || !s_layout.scrollable)
		return false;
	scrollTo(s_firstRow + (up ? -1 : 1));
	return true;
}

bool dirty() { return s_active >= 0 && computeDirty(); }

bool requestLeave(void (*onDiscard)())
{
	if (s_active < 0 || !computeDirty())
		return true;
	s_discard = onDiscard;
	MessageBoxYesNo(upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:OptionsUnsavedTitle", L"Unsaved changes")),
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:OptionsUnsavedBody", L"Leave Options and discard the changes you made?"),
		[]() { if (s_discard) { void (*f)() = s_discard; s_discard = nullptr; f(); } },
		[]() { s_discard = nullptr; });
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
	s_tracker.clear();
	for (GameWindow *&w : s_tracked) w = nullptr;
	s_decor.clear();
	s_canvas = s_root = s_popup = s_network = s_backdrop = nullptr;
	s_active = -1;
	s_firstRow = 0;
	s_advancedOpen = false;
	s_dirty = false;
	s_discard = nullptr;
	ModernDraw::clearRegistry(ModernDraw::Screen::Options);
}

int activeCategory() { return s_active; }
const ModernScreen::Layout &layout() { return s_layout; }
int firstRow() { return s_firstRow; }

}  // namespace OptionsTabs
