// SPDX-License-Identifier: GPL-3.0-or-later
// UI.2 Modern main menu: the page / action model, the navigation reducer and the responsive geometry (pure, engine-free, deterministic).
//
// The Modern main menu is a PRESENTATION over the retail main-menu behaviour: every entry maps onto the same action the retail button performs (campaign selection, difficulty, Skirmish,
// LAN, Options, ...). This header decides WHAT is on each page for each title and what an action means for navigation; the binding layer (GameClient/ModernMenuShell) turns that into
// windows and performs the engine side of the actions; ModernUiLayout places the rectangles. Nothing here knows a window, a resolution or a device.
#pragma once

#include <cstdint>

#include "Common/ModernFocus.h"
#include "Common/ModernTheme.h"
#include "Common/ModernUiLayout.h"

namespace ModernMainMenu
{
using ModernTheme::Title;

constexpr int kSlots = 5;   // item rows reserved on every page, so the panel never changes size when the page changes

enum class Page : std::uint8_t { Root = 0, SoloPlay, Campaign, Difficulty, Multiplayer, Load, Count };

enum class Action : std::uint8_t
{
	None = 0,
	// navigation inside the shell
	GoSoloPlay, GoCampaign, GoMultiplayer, GoLoad, Back,
	// campaign selection (retail: the faction buttons) and difficulty (retail: Easy / Medium / Hard)
	Training, CampaignUSA, CampaignGLA, CampaignChina, Challenge, DiffEasy, DiffMedium, DiffHard,
	// leaving the main menu (retail: the same buttons)
	Skirmish, Network, Online, LoadGame, LoadReplay, Options, Credits, Exit,
	Count
};

struct Entry
{
	Action action;
	const char *labelKey;        // retail GameText label
	const wchar_t *fallback;     // shown when the label is missing
};

struct PageDef
{
	Page id;
	const char *headingKey;
	const wchar_t *headingFallback;
	Entry entries[kSlots];
	int count;
	Entry footer;                // Back / Exit Game
};

inline const PageDef &pageDef(Title title, Page page)
{
	static const Entry back = { Action::Back, "GUI:Back", L"BACK" };
	// ---- shared pages ----
	static const PageDef root = { Page::Root, "GUI:SinglePlayer", L"",   // the root shows the logo instead of a heading
		{ { Action::GoSoloPlay, "GUI:SinglePlayer", L"SOLO PLAY" }, { Action::GoMultiplayer, "GUI:Multiplayer", L"MULTIPLAYER" }, { Action::GoLoad, "GUI:ReplayMenu", L"LOAD" },
		  { Action::Options, "GUI:Options", L"OPTIONS" }, { Action::Credits, "GUI:Credits", L"CREDITS" } }, 5,
		{ Action::Exit, "GUI:Exit", L"EXIT GAME" } };
	static const PageDef difficulty = { Page::Difficulty, "GUI:SelectDifficulty", L"SELECT DIFFICULTY",
		{ { Action::DiffEasy, "GUI:EasyCaps", L"EASY" }, { Action::DiffMedium, "GUI:MediumDifficultyCaps", L"MEDIUM" }, { Action::DiffHard, "GUI:HardCaps", L"HARD" }, {}, {} }, 3, back };
	static const PageDef multiplayer = { Page::Multiplayer, "GUI:Multiplayer", L"MULTIPLAYER",
		{ { Action::Network, "GUI:Network", L"NETWORK" }, { Action::Online, "GUI:GeneralsOnline", L"ONLINE" }, {}, {}, {} }, 2, back };
	static const PageDef load = { Page::Load, "GUI:ReplayMenu", L"LOAD",
		{ { Action::LoadGame, "GUI:MainMenuLoadGame", L"LOAD GAME" }, { Action::LoadReplay, "GUI:MainMenuLoadReplay", L"LOAD REPLAY" }, {}, {}, {} }, 2, back };
	// ---- title specific pages (retail differences: Generals has the Training campaign, Zero Hour has the General's Challenge) ----
	static const PageDef soloGenerals = { Page::SoloPlay, "GUI:SinglePlayer", L"SOLO PLAY",
		{ { Action::GoCampaign, "GUI:Campaign", L"CAMPAIGN" }, { Action::Skirmish, "GUI:SkirmishLower", L"Skirmish" }, {}, {}, {} }, 2, back };
	static const PageDef soloZeroHour = { Page::SoloPlay, "GUI:SinglePlayer", L"SOLO PLAY",
		{ { Action::GoCampaign, "GUI:Campaign", L"CAMPAIGN" }, { Action::Challenge, "GUI:Generals_Challenge", L"CHALLENGE" }, { Action::Skirmish, "GUI:SkirmishLower", L"Skirmish" }, {}, {} }, 3, back };
	static const PageDef campaignGenerals = { Page::Campaign, "GUI:Campaign", L"CAMPAIGN",
		{ { Action::Training, "GUI:TRAINING", L"TRAINING" }, { Action::CampaignUSA, "GUI:USA", L"USA" }, { Action::CampaignGLA, "GUI:GLA", L"GLA" }, { Action::CampaignChina, "GUI:China", L"China" }, {} }, 4, back };
	static const PageDef campaignZeroHour = { Page::Campaign, "GUI:Campaign", L"CAMPAIGN",
		{ { Action::CampaignUSA, "GUI:USA", L"USA" }, { Action::CampaignGLA, "GUI:GLA", L"GLA" }, { Action::CampaignChina, "GUI:China", L"China" }, {}, {} }, 3, back };

	const bool zh = title == Title::ZeroHour;
	switch (page)
	{
	case Page::SoloPlay:    return zh ? soloZeroHour : soloGenerals;
	case Page::Campaign:    return zh ? campaignZeroHour : campaignGenerals;
	case Page::Difficulty:  return difficulty;
	case Page::Multiplayer: return multiplayer;
	case Page::Load:        return load;
	default:                return root;
	}
}

// Is the action part of this title's menu at all (Training exists only in Generals, Challenge only in Zero Hour)?
inline bool existsIn(Title title, Action action)
{
	if (action == Action::Training) return title == Title::Generals;
	if (action == Action::Challenge) return title == Title::ZeroHour;
	return true;
}

// ---- the reducer: what an action means for the shell ----------------------------------------------------------------------------------------------------------------------------------
enum class EffectKind : std::uint8_t
{
	None,
	PageChanged,      // the visible page changed (enter or back): rebuild the page content, restore focus
	Engine,           // leave the main menu through the retail behaviour of `action`
	SelectCampaign,   // retail: the faction button (sets the campaign); the shell moved to the difficulty page
	SelectChallenge,  // retail (Zero Hour): the Challenge button; the shell moved to the difficulty page
	CancelSelection,  // back from the difficulty page: forget the selected campaign / challenge
	StartSelected     // retail: Easy / Medium / Hard; start the selected campaign (or open the Challenge menu) at `difficulty`
};

struct Effect
{
	EffectKind kind = EffectKind::None;
	Action action = Action::None;
	int difficulty = 0;   // 0 easy, 1 medium, 2 hard (StartSelected)
};

struct State
{
	ModernFocus::Navigator nav;
	Title title = Title::Generals;
	Action selected = Action::None;     // the campaign chosen on the way to the difficulty page
	bool challenge = false;
	bool busy = false;                  // an engine action is in flight: no further actions until the screen is shown again

	void reset(Title t) { title = t; selected = Action::None; challenge = false; busy = false; nav.reset(static_cast<int>(Page::Root)); }
	Page page() const { return static_cast<Page>(nav.page()); }
};

inline Effect apply(State &s, Action a)
{
	Effect e;
	if (s.busy || a == Action::None || !existsIn(s.title, a)) return e;
	auto go = [&](Page p) { if (s.nav.enter(static_cast<int>(p))) e.kind = EffectKind::PageChanged; };
	switch (a)
	{
	case Action::GoSoloPlay:    go(Page::SoloPlay); break;
	case Action::GoCampaign:    go(Page::Campaign); break;
	case Action::GoMultiplayer: go(Page::Multiplayer); break;
	case Action::GoLoad:        go(Page::Load); break;
	case Action::Back:
		if (s.page() == Page::Difficulty) { s.selected = Action::None; s.challenge = false; e.kind = EffectKind::CancelSelection; s.nav.back(); }
		else if (s.nav.back()) e.kind = EffectKind::PageChanged;
		break;
	case Action::Training: case Action::CampaignUSA: case Action::CampaignGLA: case Action::CampaignChina:
		s.selected = a; s.challenge = false; e.kind = EffectKind::SelectCampaign; e.action = a;
		s.nav.enter(static_cast<int>(Page::Difficulty));
		break;
	case Action::Challenge:
		s.selected = Action::None; s.challenge = true; e.kind = EffectKind::SelectChallenge; e.action = a;
		s.nav.enter(static_cast<int>(Page::Difficulty));
		break;
	case Action::DiffEasy: case Action::DiffMedium: case Action::DiffHard:
		if (s.selected == Action::None && !s.challenge) break;   // nothing selected: a difficulty alone starts nothing
		e.kind = EffectKind::StartSelected; e.action = a; e.difficulty = a == Action::DiffEasy ? 0 : (a == Action::DiffMedium ? 1 : 2);
		s.busy = true;
		break;
	default:   // Skirmish, Network, Online, LoadGame, LoadReplay, Options, Credits, Exit
		e.kind = EffectKind::Engine; e.action = a;
		s.busy = true;
		break;
	}
	return e;
}

// ---- geometry --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct Rect { int x = 0, y = 0, w = 0, h = 0; };

struct Geometry
{
	Rect composition;          // the centred box the menu is composed in (never wider than the composition aspect cap)
	Rect panel;                // absolute: the navigation band (full surface height, docked at the left edge of the composition box)
	// children, relative to the band's top-left (the band starts at y = 0, so y is also the absolute y)
	Rect logo, heading, headingRule, slot[kSlots], footerRule, footer;
	Rect footnote;             // absolute: version / branding line at the bottom of the band
	int stackTop = 0, stackBottom = 0;   // the vertical extent of the navigation stack (logo .. footer), absolute
	int effectiveScale = 100;  // the UI scale actually used (<= requested): the stack never scrolls, so it is reduced until it fits the surface
	bool compact = false;      // true when the requested scale had to be reduced
	int slideFrom = 0;         // x offset (negative) the band starts at for the reveal animation
};

namespace detail
{
inline int sc(int ref, int percent) { return (ref * percent + 50) / 100; }
}

inline Geometry compute(int surfaceW, int surfaceH, int uiScalePercent, Title title)
{
	using namespace detail;
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	const ModernTheme::Variant &v = ModernTheme::variant(title);
	Geometry g;
	if (surfaceW <= 0 || surfaceH <= 0) return g;
	int scale = uiScalePercent > 0 ? uiScalePercent : 100;

	// composition box
	int compW = static_cast<int>(surfaceH * ModernTheme::kCompositionMaxAspect + 0.5);
	if (compW > surfaceW) compW = surfaceW;
	g.composition = Rect{ (surfaceW - compW) / 2, 0, compW, surfaceH };

	// Fit loop: reduce the effective scale until the navigation stack (logo + heading + slots + footer) fits the surface height.
	for (int attempt = 0; attempt < 64; ++attempt)
	{
		const int pad = sc(m.panelPadding, scale), gap = sc(m.itemGap, scale);
		int bandW = sc(m.panelPreferredWidth, scale);
		const int minW = sc(m.panelMinWidth, scale), maxW = sc(m.panelMaxWidth, scale);
		if (bandW < minW) bandW = minW;
		if (bandW > maxW) bandW = maxW;
		if (bandW > compW * 4 / 10) bandW = compW * 4 / 10;   // never more than 40 % of the composition box
		const int innerW = bandW - 2 * pad;
		int logoW = innerW * 88 / 100;                          // the logo is a little narrower than the items: it is a mark, not a bar
		const int logoH = logoW * v.logoHeight / v.logoWidth;

		ModernUi::Tree tree;
		ModernUi::Node root; root.axis = ModernUi::Axis::Column; root.gap = gap; root.prefW = innerW;
		const int r = tree.add(ModernUi::kNone, root);
		auto leaf = [&](int h) { ModernUi::Node n; n.prefH = h; return tree.add(r, n); };
		const int iLogo = leaf(logoH);
		leaf(sc(18, scale));
		const int iHeading = leaf(sc(m.headingHeight, scale));
		const int iHeadRule = leaf(sc(m.ruleThickness, scale) > 0 ? sc(m.ruleThickness, scale) : 1);
		leaf(sc(6, scale));
		int iSlot[kSlots];
		for (int i = 0; i < kSlots; ++i) iSlot[i] = leaf(sc(m.itemHeight, scale));
		leaf(sc(14, scale));
		const int iFootRule = leaf(sc(m.ruleThickness, scale) > 0 ? sc(m.ruleThickness, scale) : 1);
		leaf(sc(6, scale));
		const int iFooter = leaf(sc(m.itemHeight, scale));

		ModernUi::Result probe;
		ModernUi::solve(tree, ModernUi::Rect{ 0, 0, innerW, 1 << 20 }, 100, probe);
		const int stackH = probe.content[r].h;
		const int marginV = sc(m.edgeMargin, scale);
		const int footnoteH = sc(m.footnoteHeight, scale);
		int limitH = surfaceH * m.panelMaxHeightPercent / 100;
		if (limitH > surfaceH - 2 * marginV - footnoteH) limitH = surfaceH - 2 * marginV - footnoteH;
		if (stackH <= limitH || scale <= 30)
		{
			ModernUi::Result out;
			ModernUi::solve(tree, ModernUi::Rect{ 0, 0, innerW, stackH }, 100, out);
			int top = (surfaceH - stackH) / 2 - surfaceH / 20;
			if (top < marginV) top = marginV;
			auto rel = [&](int i) { return Rect{ pad + out.rect[i].x, top + out.rect[i].y, out.rect[i].w, out.rect[i].h }; };
			g.logo = rel(iLogo);
			g.logo.w = logoW; g.logo.h = logoH;
			g.heading = rel(iHeading); g.headingRule = rel(iHeadRule);
			for (int i = 0; i < kSlots; ++i) g.slot[i] = rel(iSlot[i]);
			g.footerRule = rel(iFootRule); g.footer = rel(iFooter);
			int bandX = g.composition.x;
			g.panel = Rect{ bandX, 0, bandW, surfaceH };
			g.footnote = Rect{ bandX + pad, surfaceH - marginV / 2 - footnoteH, innerW, footnoteH };
			g.stackTop = top; g.stackBottom = top + stackH;
			g.effectiveScale = scale;
			g.compact = scale < (uiScalePercent > 0 ? uiScalePercent : 100);
			g.slideFrom = -(bandX + bandW);
			return g;
		}
		scale = scale * 92 / 100;   // reduce and try again
	}
	return g;
}

} // namespace ModernMainMenu
