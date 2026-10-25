// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience Match Setup (Skirmish). See GameClient/ModernMatchSetup.h.
#include "PreRTS.h"

#include "GameClient/ModernMatchSetup.h"

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <vector>

#include "Common/DisplayRuntime.h"
#include "Common/ModernBranding.h"
#include "Common/ModernExperience.h"
#include "Common/NameKeyGenerator.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/Display.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetCheckBox.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/MatchOptionControls.h"
#include "GameClient/ModernDraw.h"

namespace
{
using ModernTheme::TypeRole;
using ModernMatchSetup::kSlots;
using ModernMatchSetup::kColumns;
using ModernMatchSetup::kHonorRows;

const char *const kMenu = "SkirmishGameOptionsMenu.wnd:";

const ModernMatchSetupScreen::RuleSpec kRules[] = {
	{ "StartingCash", "GUI:StartingMoney", L"Starting Cash", "ComboBoxStartingCash", nullptr },
	{ "AIBehavior", "GUI:AIBehavior", L"AI Behaviour", "ComboBoxAIBehavior", nullptr },
	{ "Superweapons", "GUI:Superweapons", L"Superweapons", "ComboBoxSuperweapons", nullptr },
	{ "LimitSuperweapons", "GUI:LimitSuperweapons", L"Limit Superweapons", "CheckboxLimitSuperweapons", "ComboBoxSuperweapons" },
};

struct Rule { const ModernMatchSetupScreen::RuleSpec *spec; GameWindow *window; bool check; };

struct State
{
	bool built = false;
	GameWindow *parent = nullptr, *sub = nullptr, *backdrop = nullptr;
	ModernMatchSetupScreen::Hooks hooks = {};
	ModernMatchSetup::Layout layout;
	int surfaceW = 0, surfaceH = 0, uiPercent = 0;
	std::vector<Rule> rules;
	GameWindow *cell[kSlots][kColumns] = {};
	GameWindow *map = nullptr, *mapName = nullptr, *selectMap = nullptr, *start = nullptr, *back = nullptr;
	GameWindow *honorLabel[kHonorRows] = {}, *honorValue[kHonorRows] = {};
	std::vector<GameWindow *> kept;                       // in back-to-front order (the last one is brought to the top last)
	ModernDraw::Decor decor;
	unsigned relayouts = 0;
} s;

GameWindow *byName(const char *name)
{
	char full[160];
	snprintf(full, sizeof(full), "%s%s", kMenu, name);
	return TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(full));
}

UnicodeString upper(const UnicodeString &u)
{
	UnicodeString r;
	for (const WideChar *p = u.str(); p && *p; ++p) r.concat(static_cast<WideChar>(towupper(*p)));
	return r;
}

// absolute surface rectangle -> relative to the parent's screen position
void place(GameWindow *w, const ModernScreen::Rect &r)
{
	if (!w) return;
	GameWindow *parent = w->winGetParent();
	Int px = 0, py = 0;
	if (parent) parent->winGetScreenPosition(&px, &py);
	w->winSetPosition(r.x - px, r.y - py);
	w->winSetSize(r.w, r.h);
}

void placeCombo(GameWindow *combo, const ModernScreen::Rect &r)
{
	if (!combo) return;
	GameWindow *parent = combo->winGetParent();
	Int px = 0, py = 0;
	if (parent) parent->winGetScreenPosition(&px, &py);
	MatchOptionControls::placeCombo(combo, MatchOptionsLayout::Rect{ r.x - px, r.y - py, r.w, r.h });
}

void leftAligned(GameWindow *w, bool vcenter = true)
{
	if (!w || !BitIsSet(w->winGetStyle(), GWS_STATIC_TEXT)) return;
	if (TextData *data = static_cast<TextData *>(w->winGetUserData()))
	{
		data->centered = FALSE;
		data->centeredVertically = vcenter ? TRUE : FALSE;
	}
}

bool contains(const std::vector<GameWindow *> &v, GameWindow *w)
{
	for (GameWindow *x : v) if (x == w) return true;
	return false;
}

bool isCombo(GameWindow *w) { return w && BitIsSet(w->winGetStyle(), GWS_COMBO_BOX); }

void collectWindows()
{
	char name[96];
	s.sub = byName("SubParent");
	s.map = byName("MapWindow"); s.mapName = byName("TextEntryMapDisplay"); s.selectMap = byName("ButtonSelectMap");
	s.start = byName("ButtonStart"); s.back = byName("ButtonBack");
	for (int r = 0; r < kSlots; ++r)
	{
		if (r == 0) s.cell[r][0] = byName("TextEntryPlayerName");
		else { snprintf(name, sizeof(name), "ComboBoxPlayer%d", r); s.cell[r][0] = byName(name); }
		snprintf(name, sizeof(name), "ComboBoxColor%d", r); s.cell[r][1] = byName(name);
		snprintf(name, sizeof(name), "ComboBoxPlayerTemplate%d", r); s.cell[r][2] = byName(name);
		snprintf(name, sizeof(name), "ComboBoxTeam%d", r); s.cell[r][3] = byName(name);
	}
	static const char *const honor[kHonorRows][2] = { { "StaticTextWins", "StaticTextWinsValue" }, { "StaticTextLosses", "StaticTextLossesValue" },
		{ "StaticTextStreak", "StaticTextStreakValue" }, { "StaticTextBestStreak", "StaticTextBestStreakValue" } };
	for (int i = 0; i < kHonorRows; ++i) { s.honorLabel[i] = byName(honor[i][0]); s.honorValue[i] = byName(honor[i][1]); }

	// the rules: a spec applies when its control exists and is not replaced by another control that exists
	s.rules.clear();
	for (const ModernMatchSetupScreen::RuleSpec &spec : kRules)
	{
		GameWindow *w = byName(spec.window);
		if (!w) continue;
		if (spec.replacedBy && byName(spec.replacedBy)) continue;
		s.rules.push_back(Rule{ &spec, w, BitIsSet(w->winGetStyle(), GWS_CHECK_BOX) != 0 });
	}
}

void skinAll()
{
	const ModernTheme::Palette &p = ModernDraw::palette();
	for (int r = 0; r < kSlots; ++r)
		for (int c = 0; c < kColumns; ++c)
		{
			GameWindow *w = s.cell[r][c];
			if (!w) continue;
			if (isCombo(w)) ModernDraw::skinCombo(w);
			else ModernDraw::skinEntry(w);
		}
	for (const Rule &r : s.rules)
		if (r.check) ModernDraw::skinCheck(r.window); else ModernDraw::skinCombo(r.window);
	if (s.mapName) { ModernDraw::skinText(s.mapName, TypeRole::Value, p.text); leftAligned(s.mapName); }
	if (s.selectMap) ModernDraw::skinButton(s.selectMap, ModernComponents::Kind::Secondary, TypeRole::MenuSecondary);
	if (s.start) ModernDraw::skinButton(s.start, ModernComponents::Kind::Primary, TypeRole::MenuSecondary);
	if (s.back) ModernDraw::skinButton(s.back, ModernComponents::Kind::Secondary, TypeRole::MenuSecondary);
	for (int i = 0; i < kHonorRows; ++i)
	{
		if (s.honorLabel[i]) { ModernDraw::skinText(s.honorLabel[i], TypeRole::Label, p.textSecondary); leftAligned(s.honorLabel[i]); }
		if (s.honorValue[i]) { ModernDraw::skinText(s.honorValue[i], TypeRole::Value, p.text); if (TextData *d = static_cast<TextData *>(s.honorValue[i]->winGetUserData())) { d->centered = FALSE; d->centeredVertically = TRUE; } }
	}
}

void rebuildDecor()
{
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	const ModernTheme::Palette &p = ModernDraw::palette();
	const ModernMatchSetup::Layout &L = s.layout;
	auto rx = [&](int x) { return x - L.panel.x; };
	auto ry = [&](int y) { return y - L.panel.y; };
	s.decor.clear();
	const int cut = ModernDraw::px(m.chamfer);
	ModernDraw::DecorItem &panel = s.decor.add(ModernDraw::DecorKind::Panel);
	panel.x = 0; panel.y = 0; panel.w = L.panel.w; panel.h = L.panel.h; panel.cutTL = cut; panel.cutBR = cut;
	ModernDraw::DecorItem &bar = s.decor.add(ModernDraw::DecorKind::Tick);
	bar.x = cut; bar.y = 0; bar.w = ModernDraw::px(110); bar.h = ModernDraw::px(3);

	auto text = [&](const ModernScreen::Rect &r, TypeRole role, const ModernTheme::Color &color, const UnicodeString &t, int align = 0) {
		ModernDraw::DecorItem &d = s.decor.add(ModernDraw::DecorKind::Text);
		d.x = rx(r.x); d.y = ry(r.y); d.w = r.w; d.h = r.h; d.role = role; d.color = color; d.align = align;
		wcsncpy(d.text, t.str(), 95);
	};
	text(L.title, TypeRole::Title, p.text, upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:Skirmish", L"Skirmish")));
	ModernDraw::DecorItem &rule = s.decor.add(ModernDraw::DecorKind::Rule);
	rule.x = rx(L.titleRule.x); rule.y = ry(L.titleRule.y); rule.w = L.titleRule.w * 10 / 100; rule.h = L.titleRule.h; rule.color = p.accent;
	ModernDraw::DecorItem &rest = s.decor.add(ModernDraw::DecorKind::Rule);
	rest.x = rule.x + rule.w; rest.y = rule.y; rest.w = L.titleRule.w - rule.w; rest.h = rule.h; rest.color = p.hairline;

	for (const ModernScreen::Rect *z : { &L.mapZone, &L.playersZone, &L.rulesZone })
	{
		ModernDraw::DecorItem &zone = s.decor.add(ModernDraw::DecorKind::Raised);
		zone.x = rx(z->x); zone.y = ry(z->y); zone.w = z->w; zone.h = z->h; zone.cutTL = ModernDraw::px(m.chamferSmall); zone.cutBR = ModernDraw::px(m.chamferSmall);
	}
	text(L.mapHeading, TypeRole::SectionHeading, p.accent, upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:MapPreview", L"Map")));
	text(L.playersHeading, TypeRole::SectionHeading, p.accent, upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:Players", L"Players")));
	text(L.rulesHeading, TypeRole::SectionHeading, p.accent, upper(TheGameText->FETCH_OR_SUBSTITUTE("GUI:MatchRules", L"Match Rules")));
	static const char *const headKeys[kColumns] = { "GUI:Players", "GUI:Color", "GUI:Faction", "GUI:Team" };
	static const wchar_t *const headFallback[kColumns] = { L"Player", L"Color", L"Faction", L"Team" };
	for (int c = 0; c < kColumns; ++c)
		text(L.columnHead[c], TypeRole::Helper, p.textMuted, upper(TheGameText->FETCH_OR_SUBSTITUTE(headKeys[c], headFallback[c])));
	// the map preview frame
	ModernDraw::DecorItem &field = s.decor.add(ModernDraw::DecorKind::Field);
	field.x = rx(L.mapPreview.x); field.y = ry(L.mapPreview.y); field.w = L.mapPreview.w; field.h = L.mapPreview.h;
	for (int i = 0; i < L.ruleCount && i < static_cast<int>(s.rules.size()); ++i)
		if (!s.rules[i].check)
			text(L.ruleLabel[i], TypeRole::Label, p.textSecondary, TheGameText->FETCH_OR_SUBSTITUTE(s.rules[i].spec->labelKey, s.rules[i].spec->fallback));
	if (L.hasHonors)
	{
		ModernDraw::DecorItem &sep = s.decor.add(ModernDraw::DecorKind::Rule);
		sep.x = rx(L.honorLabel[0].x); sep.y = ry(L.honorLabel[0].y) - ModernDraw::px(6); sep.w = L.honorLabel[0].w + L.honorValue[0].w; sep.h = 1; sep.color = p.hairline;
	}
	text(L.status, TypeRole::Helper, p.textMuted, TheGameText->FETCH_OR_SUBSTITUTE("GUI:SkirmishHint", L"Click a start position on the map to take it."));
	ModernDraw::DecorItem &foot = s.decor.add(ModernDraw::DecorKind::Text);
	foot.x = rx(L.status.right()); foot.y = ry(L.status.y); foot.w = L.panel.right() - ModernDraw::px(24) - L.status.right() - 0; foot.h = L.status.h;
	foot.role = TypeRole::Helper; foot.align = 2; foot.color = p.textMuted.withAlpha(150);
	wcsncpy(foot.text, ModernBranding::footerText(), 95);
	if (foot.w < ModernDraw::px(160)) foot.w = 0;
	s.decor.refreshFonts();
}

// Hides every retail window of the screen that the Modern composition does not use (decorative frames, captions, the retired Game Speed and Battle Honors blocks).
void hideUnused()
{
	std::vector<GameWindow *> use;
	for (int r = 0; r < kSlots; ++r) for (int c = 0; c < kColumns; ++c) if (s.cell[r][c]) use.push_back(s.cell[r][c]);
	for (const Rule &r : s.rules) use.push_back(r.window);
	for (GameWindow *w : { s.map, s.mapName, s.selectMap, s.start, s.back, s.backdrop }) if (w) use.push_back(w);
	for (int i = 0; i < kHonorRows; ++i) { if (s.honorLabel[i]) use.push_back(s.honorLabel[i]); if (s.honorValue[i]) use.push_back(s.honorValue[i]); }
	for (GameWindow *c = s.sub->winGetChild(); c; c = c->winGetNext())
		if (!contains(use, c))
			c->winHide(TRUE);
}

void relayout()
{
	s.surfaceW = TheDisplay->getWidth();
	s.surfaceH = TheDisplay->getHeight();
	s.uiPercent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (s.uiPercent <= 0) s.uiPercent = 100;
	ModernMatchSetup::Input in;
	in.surfaceW = s.surfaceW; in.surfaceH = s.surfaceH; in.uiPercent = s.uiPercent; in.ruleCount = static_cast<int>(s.rules.size());
	in.honors = s.honorLabel[0] && s.honorValue[0];
	s.layout = ModernMatchSetup::compute(in);
	const ModernMatchSetup::Layout &L = s.layout;
	ModernDraw::beginScreen(ModernDraw::Screen::MatchSetup);
	ModernDraw::setScalePercent(L.effectiveScale);

	// containers first (a child is hit only inside every ancestor's rectangle): the layout parent covers the surface, SubParent the panel
	s.parent->winSetPosition(0, 0);
	s.parent->winSetSize(s.surfaceW, s.surfaceH);
	s.sub->winSetPosition(L.panel.x, L.panel.y);
	s.sub->winSetSize(L.panel.w, L.panel.h);
	s.sub->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
	s.sub->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	s.parent->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
	s.parent->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	for (GameWindow *w : { s.parent, s.sub }) { w->winClearStatus(WIN_STATUS_IMAGE); w->winClearStatus(WIN_STATUS_BORDER); }   // the retail frame art
	if (s.backdrop) place(s.backdrop, L.panel);

	for (int r = 0; r < kSlots; ++r)
		for (int c = 0; c < kColumns; ++c)
		{
			GameWindow *w = s.cell[r][c];
			if (!w) continue;
			if (isCombo(w)) placeCombo(w, L.cell[r][c]); else place(w, L.cell[r][c]);
		}
	place(s.map, L.mapPreview);
	place(s.mapName, L.mapName);
	place(s.selectMap, L.selectMap);
	place(s.start, L.start);
	place(s.back, L.back);
	for (int i = 0; i < static_cast<int>(s.rules.size()) && i < L.ruleCount; ++i)
	{
		if (s.rules[i].check)
			place(s.rules[i].window, ModernScreen::Rect{ L.ruleLabel[i].x, L.ruleLabel[i].y, L.ruleLabel[i].w, L.ruleLabel[i].h + L.ruleControl[i].h });
		else
			placeCombo(s.rules[i].window, L.ruleControl[i]);
	}
	for (int i = 0; i < kHonorRows; ++i)
	{
		place(s.honorLabel[i], L.honorLabel[i]);
		place(s.honorValue[i], L.honorValue[i]);
	}
	hideUnused();

	// z-order: the backdrop at the back; lower rows before upper rows so a dropdown opens over what is below it
	if (s.backdrop) s.backdrop->winBringToTop();
	for (int i = static_cast<int>(s.rules.size()) - 1; i >= 0; --i) s.rules[i].window->winBringToTop();
	for (int r = kSlots - 1; r >= 0; --r)
		for (int c = kColumns - 1; c >= 0; --c)
			if (s.cell[r][c]) s.cell[r][c]->winBringToTop();
	for (GameWindow *w : { s.map, s.mapName, s.selectMap, s.start, s.back }) if (w) w->winBringToTop();
	for (int i = 0; i < kHonorRows; ++i) { if (s.honorLabel[i]) s.honorLabel[i]->winBringToTop(); if (s.honorValue[i]) s.honorValue[i]->winBringToTop(); }

	rebuildDecor();
	if (s.hooks.repositionStartSpots) s.hooks.repositionStartSpots();
	++s.relayouts;
}
}

namespace ModernMatchSetupScreen
{
const RuleSpec *ruleSpecs(int &count) { count = static_cast<int>(sizeof(kRules) / sizeof(kRules[0])); return kRules; }

bool create(GameWindow *menuParent, const Hooks &hooks)
{
	forget();
	if (!DisplayRuntime::modern() || ModernExperience::classic() || !menuParent) return false;
	s.parent = menuParent;
	s.hooks = hooks;
	collectWindows();
	if (!s.sub || !s.map || !s.start || !s.back || !s.cell[0][0] || !s.cell[1][1]) return false;
	// the career record statics live in an unnamed frame; they join SubParent so the frame can be hidden
	for (int i = 0; i < kHonorRows; ++i)
	{
		if (s.honorLabel[i] && s.honorLabel[i]->winGetParent() != s.sub) s.honorLabel[i]->winSetParent(s.sub);
		if (s.honorValue[i] && s.honorValue[i]->winGetParent() != s.sub) s.honorValue[i]->winSetParent(s.sub);
	}
	ModernDraw::beginScreen(ModernDraw::Screen::MatchSetup);
	ModernDraw::clearRegistry(ModernDraw::Screen::MatchSetup);
	s.backdrop = ModernDraw::createBackdrop(s.sub, "SkirmishGameOptionsMenu.wnd:ModernBackdrop", &s.decor);
	skinAll();
	relayout();
	s.built = true;
	char detail[240];
	snprintf(detail, sizeof(detail), "scale=%d effective=%d panel=%dx%d rules=%d", s.uiPercent, s.layout.effectiveScale, s.layout.panel.w, s.layout.panel.h, static_cast<int>(s.rules.size()));
	StartupDiagnostics::checkpoint("matchsetup", "complete", detail);
	return true;
}

void update()
{
	if (!s.built || !s.parent) return;
	const int uiPercent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (TheDisplay->getWidth() != s.surfaceW || TheDisplay->getHeight() != s.surfaceH || (uiPercent > 0 ? uiPercent : 100) != s.uiPercent)
		relayout();
	else
		hideUnused();   // the retail fade-in (FULLFADE on SubParent) shows its children again after Init: whatever the composition does not use stays hidden
}

void forget()
{
	s.built = false;
	s.parent = s.sub = s.backdrop = nullptr;
	s.map = s.mapName = s.selectMap = s.start = s.back = nullptr;
	s.rules.clear();
	for (auto &row : s.cell) for (GameWindow *&w : row) w = nullptr;
	for (int i = 0; i < kHonorRows; ++i) s.honorLabel[i] = s.honorValue[i] = nullptr;
	s.decor.clear();
	ModernDraw::clearRegistry(ModernDraw::Screen::MatchSetup);
}

bool active() { return s.built; }
const ModernMatchSetup::Layout &layout() { return s.layout; }
int ruleCount() { return static_cast<int>(s.rules.size()); }
}
