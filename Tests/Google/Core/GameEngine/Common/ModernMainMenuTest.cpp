/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// UI.2: the Modern main menu model, reducer, geometry and theme tokens (Common/ModernMainMenuModel.h, ModernTheme.h).

#include <gtest/gtest.h>
#include <chrono>
#include <cstring>
#include <cwchar>
#include <string>
#include "Common/ModernMainMenuModel.h"

using namespace ModernMainMenu;
using ModernTheme::Title;

namespace
{
const Title kTitles[] = { Title::Generals, Title::ZeroHour };
const Page kPages[] = { Page::Root, Page::SoloPlay, Page::Campaign, Page::Difficulty, Page::Multiplayer, Page::Load };
struct Surface { int w, h; };
const Surface kSurfaces[] = { { 1920, 1080 }, { 2560, 1440 }, { 3440, 1440 }, { 3840, 2160 }, { 5120, 1440 } };
const int kScales[] = { 75, 100, 125, 150, 200, 300 };

bool inside(const Rect &outer, const Rect &r) { return r.x >= outer.x && r.y >= outer.y && r.x + r.w <= outer.x + outer.w && r.y + r.h <= outer.y + outer.h; }
bool overlap(const Rect &a, const Rect &b) { return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y; }

State freshState(Title t) { State s; s.reset(t); return s; }

// walk the shell to a page the way a user would
void goTo(State &s, Page p)
{
	switch (p)
	{
	case Page::SoloPlay:    apply(s, Action::GoSoloPlay); break;
	case Page::Campaign:    apply(s, Action::GoSoloPlay); apply(s, Action::GoCampaign); break;
	case Page::Difficulty:  apply(s, Action::GoSoloPlay); apply(s, Action::GoCampaign); apply(s, s.title == Title::ZeroHour ? Action::CampaignUSA : Action::Training); break;
	case Page::Multiplayer: apply(s, Action::GoMultiplayer); break;
	case Page::Load:        apply(s, Action::GoLoad); break;
	default: break;
	}
}
}

// ---- model -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TEST(ModernMainMenu, EveryPageHasLabelsAndFitsTheSlots)
{
	for (Title t : kTitles)
		for (Page p : kPages)
		{
			const PageDef &d = pageDef(t, p);
			EXPECT_EQ(d.id, p);
			EXPECT_GE(d.count, 1);
			EXPECT_LE(d.count, kSlots);
			ASSERT_NE(d.headingKey, nullptr);
			for (int i = 0; i < d.count; ++i)
			{
				EXPECT_NE(d.entries[i].action, Action::None) << (int)p << ":" << i;
				EXPECT_NE(d.entries[i].labelKey, nullptr);
				EXPECT_GT(std::wcslen(d.entries[i].fallback), 0u);
				EXPECT_TRUE(existsIn(t, d.entries[i].action));
			}
			EXPECT_NE(d.footer.action, Action::None);
		}
}

TEST(ModernMainMenu, TitleSpecificEntriesFollowTheRetailDifferences)
{
	auto has = [](Title t, Page p, Action a) { const PageDef &d = pageDef(t, p); for (int i = 0; i < d.count; ++i) if (d.entries[i].action == a) return true; return false; };
	EXPECT_TRUE(has(Title::Generals, Page::Campaign, Action::Training));      // Generals: the Training campaign
	EXPECT_FALSE(has(Title::ZeroHour, Page::Campaign, Action::Training));
	EXPECT_TRUE(has(Title::ZeroHour, Page::SoloPlay, Action::Challenge));     // Zero Hour: the General's Challenge
	EXPECT_FALSE(has(Title::Generals, Page::SoloPlay, Action::Challenge));
	for (Title t : kTitles)
	{
		EXPECT_TRUE(has(t, Page::SoloPlay, Action::Skirmish));
		EXPECT_TRUE(has(t, Page::SoloPlay, Action::GoCampaign));
		for (Action a : { Action::CampaignUSA, Action::CampaignGLA, Action::CampaignChina }) EXPECT_TRUE(has(t, Page::Campaign, a));
		EXPECT_TRUE(has(t, Page::Multiplayer, Action::Network));
		EXPECT_TRUE(has(t, Page::Multiplayer, Action::Online));
		EXPECT_TRUE(has(t, Page::Load, Action::LoadGame));
		EXPECT_TRUE(has(t, Page::Load, Action::LoadReplay));
		EXPECT_TRUE(has(t, Page::Root, Action::Options));
		EXPECT_TRUE(has(t, Page::Root, Action::Credits));
		EXPECT_EQ(pageDef(t, Page::Root).footer.action, Action::Exit);
	}
	EXPECT_FALSE(existsIn(Title::ZeroHour, Action::Training));
	EXPECT_FALSE(existsIn(Title::Generals, Action::Challenge));
}

TEST(ModernMainMenu, NoDeadButtonsEveryEntryProducesAnEffect)
{
	for (Title t : kTitles)
		for (Page p : kPages)
		{
			const PageDef &d = pageDef(t, p);
			for (int i = 0; i <= d.count; ++i)
			{
				State s = freshState(t);
				goTo(s, p);
				ASSERT_EQ(s.page(), p) << (int)p;
				if (p == Page::Difficulty) { /* walked through a campaign selection */ }
				const Action a = i < d.count ? d.entries[i].action : d.footer.action;
				const Effect e = apply(s, a);
				EXPECT_NE(e.kind, EffectKind::None) << "title " << (int)t << " page " << (int)p << " entry " << i << " action " << (int)a;
			}
		}
}

TEST(ModernMainMenu, ActionsMapToTheRetailBehaviour)
{
	for (Title t : kTitles)
	{
		State s = freshState(t);
		struct Case { Action a; EffectKind k; };
		const Case engineActions[] = { { Action::Skirmish, EffectKind::Engine }, { Action::Network, EffectKind::Engine }, { Action::Online, EffectKind::Engine },
		                               { Action::LoadGame, EffectKind::Engine }, { Action::LoadReplay, EffectKind::Engine }, { Action::Options, EffectKind::Engine },
		                               { Action::Credits, EffectKind::Engine }, { Action::Exit, EffectKind::Engine } };
		for (const Case &c : engineActions)
		{
			State fresh = freshState(t);
			const Effect e = apply(fresh, c.a);
			EXPECT_EQ(e.kind, c.k) << (int)c.a;
			EXPECT_EQ(e.action, c.a);
			EXPECT_TRUE(fresh.busy);
		}
	}
	// campaign flow: faction -> difficulty page -> start, with the right difficulty
	State s = freshState(Title::ZeroHour);
	apply(s, Action::GoSoloPlay); apply(s, Action::GoCampaign);
	Effect e = apply(s, Action::CampaignChina);
	EXPECT_EQ(e.kind, EffectKind::SelectCampaign); EXPECT_EQ(e.action, Action::CampaignChina);
	EXPECT_EQ(s.page(), Page::Difficulty);
	e = apply(s, Action::DiffHard);
	EXPECT_EQ(e.kind, EffectKind::StartSelected); EXPECT_EQ(e.difficulty, 2);
	EXPECT_EQ(s.selected, Action::CampaignChina);
	// Zero Hour Challenge goes through the same difficulty page and starts as a challenge
	State c = freshState(Title::ZeroHour);
	apply(c, Action::GoSoloPlay);
	e = apply(c, Action::Challenge);
	EXPECT_EQ(e.kind, EffectKind::SelectChallenge);
	EXPECT_TRUE(c.challenge);
	e = apply(c, Action::DiffEasy);
	EXPECT_EQ(e.kind, EffectKind::StartSelected); EXPECT_EQ(e.difficulty, 0);
}

TEST(ModernMainMenu, BackNavigationUnwindsExactlyAndCancelsSelections)
{
	State s = freshState(Title::Generals);
	apply(s, Action::GoSoloPlay);
	apply(s, Action::GoCampaign);
	apply(s, Action::Training);
	EXPECT_EQ(s.page(), Page::Difficulty);
	Effect e = apply(s, Action::Back);
	EXPECT_EQ(e.kind, EffectKind::CancelSelection);
	EXPECT_EQ(s.page(), Page::Campaign);
	EXPECT_EQ(s.selected, Action::None);
	e = apply(s, Action::Back);  EXPECT_EQ(e.kind, EffectKind::PageChanged); EXPECT_EQ(s.page(), Page::SoloPlay);
	e = apply(s, Action::Back);  EXPECT_EQ(e.kind, EffectKind::PageChanged); EXPECT_EQ(s.page(), Page::Root);
	e = apply(s, Action::Back);  EXPECT_EQ(e.kind, EffectKind::None);      EXPECT_EQ(s.page(), Page::Root);   // Escape at the root does nothing
}

TEST(ModernMainMenu, GuardsAgainstImpossibleSequences)
{
	State s = freshState(Title::ZeroHour);
	EXPECT_EQ(apply(s, Action::DiffEasy).kind, EffectKind::None);          // a difficulty without a selection starts nothing
	EXPECT_EQ(apply(s, Action::Training).kind, EffectKind::None);          // Training does not exist in Zero Hour
	State g = freshState(Title::Generals);
	EXPECT_EQ(apply(g, Action::Challenge).kind, EffectKind::None);         // and Challenge does not exist in Generals
	EXPECT_EQ(apply(s, Action::None).kind, EffectKind::None);
	// once an engine action is in flight nothing else is accepted (double clicks)
	EXPECT_EQ(apply(s, Action::Skirmish).kind, EffectKind::Engine);
	EXPECT_EQ(apply(s, Action::Options).kind, EffectKind::None);
	EXPECT_EQ(apply(s, Action::GoMultiplayer).kind, EffectKind::None);
}

TEST(ModernMainMenu, FocusIsRememberedPerPage)
{
	State s = freshState(Title::ZeroHour);
	// root: focus the third entry (Load), enter it, come back
	ModernFocus::Item items[3];
	for (int i = 0; i < 3; ++i) { items[i].id = i; items[i].rect = { 0, i * 50, 100, 40 }; }
	s.nav.ring.setItems(items, 3);
	s.nav.ring.focusId(2);
	apply(s, Action::GoLoad);
	s.nav.ring.setItems(items, 2);
	s.nav.ring.focusFirst();
	apply(s, Action::Back);
	s.nav.ring.setItems(items, 3);
	EXPECT_TRUE(s.nav.restoreFocus());
	EXPECT_EQ(s.nav.ring.focusedId(), 2);
}

// ---- geometry ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TEST(ModernMainMenuGeometry, NoDefectAtTheResolutionAndScaleMatrix)
{
	for (Title t : kTitles)
		for (const Surface &sf : kSurfaces)
			for (int scale : kScales)
			{
				const Geometry g = compute(sf.w, sf.h, scale, t);
				const Rect surface{ 0, 0, sf.w, sf.h };
				const std::string what = std::to_string(sf.w) + "x" + std::to_string(sf.h) + " @" + std::to_string(scale);
				ASSERT_GT(g.panel.w, 0) << what;
				EXPECT_TRUE(inside(surface, g.panel)) << what;
				EXPECT_TRUE(inside(surface, g.footnote)) << what;
				EXPECT_TRUE(inside(g.panel, g.footnote)) << what;   // the band is full height: the footnote is part of it
				EXPECT_EQ(g.panel.h, sf.h) << what;
				const Rect local{ 0, 0, g.panel.w, g.panel.h };
				const Rect *children[] = { &g.logo, &g.heading, &g.headingRule, &g.slot[0], &g.slot[1], &g.slot[2], &g.slot[3], &g.slot[4], &g.footerRule, &g.footer };
				for (int i = 0; i < 10; ++i)
				{
					EXPECT_TRUE(inside(local, *children[i])) << what << " child " << i;
					EXPECT_GT(children[i]->w, 0); EXPECT_GT(children[i]->h, 0);
					for (int j = i + 1; j < 10; ++j) EXPECT_FALSE(overlap(*children[i], *children[j])) << what << " children " << i << "," << j;
				}
				// mouse hit targets are the full button rectangles, tall enough to hit
				for (int i = 0; i < kSlots; ++i) EXPECT_GE(g.slot[i].h, 20) << what;
				EXPECT_GE(g.footer.h, 20) << what;
				// the requested scale is honoured when it fits, reduced (never clipped) when it does not
				EXPECT_LE(g.effectiveScale, scale) << what;
				EXPECT_GE(g.effectiveScale, 30) << what;
			}
}

TEST(ModernMainMenuGeometry, LeftDockedAndNeverStretchedOnUltrawide)
{
	for (Title t : kTitles)
		for (int scale : { 100, 150 })
		{
			const Geometry w169 = compute(2560, 1440, scale, t);
			const Geometry w219 = compute(3440, 1440, scale, t);
			const Geometry w329 = compute(5120, 1440, scale, t);
			// same panel size on every aspect: the menu is not stretched with the surface
			EXPECT_EQ(w169.panel.w, w219.panel.w); EXPECT_EQ(w169.panel.w, w329.panel.w);
			EXPECT_EQ(w169.panel.h, w329.panel.h);
			// left docked: the panel sits in the left half of the composition box, leaving the right of the surface to the shell map
			for (const Geometry *g : { &w169, &w219, &w329 })
			{
				EXPECT_LT(g->panel.x + g->panel.w, g->composition.x + g->composition.w / 2) << scale;
				EXPECT_GE(g->panel.x, g->composition.x);
				EXPECT_LE(g->composition.w, (int)(1440 * ModernTheme::kCompositionMaxAspect + 1));
			}
			// on 32:9 the panel does not hug the physical screen edge: it starts inside the centred 16:9 composition box
			EXPECT_GE(w329.panel.x, (5120 - 2560) / 2);
			EXPECT_GE(w219.panel.x, (3440 - 2560) / 2);
			// the shell map keeps most of the surface: the panel covers under a quarter of the width of a 16:9 surface and under a tenth of a 32:9 one
			EXPECT_LT(w169.panel.w * 4, 2560);
			EXPECT_LT(w329.panel.w * 8, 5120);
		}
}

TEST(ModernMainMenuGeometry, ComputationIsDeterministicAndIndependentOfThePage)
{
	const Geometry a = compute(3440, 1440, 125, Title::ZeroHour);
	const Geometry b = compute(3440, 1440, 125, Title::ZeroHour);
	EXPECT_EQ(a.panel.x, b.panel.x); EXPECT_EQ(a.panel.y, b.panel.y); EXPECT_EQ(a.panel.w, b.panel.w); EXPECT_EQ(a.panel.h, b.panel.h);
	for (int i = 0; i < kSlots; ++i) { EXPECT_EQ(a.slot[i].y, b.slot[i].y); EXPECT_EQ(a.slot[i].h, b.slot[i].h); }
	// the geometry has no page input: the panel keeps its size and the slots keep their positions on every page; the slots are evenly spaced
	const int pitch = a.slot[1].y - a.slot[0].y;
	for (int i = 2; i < kSlots; ++i) EXPECT_EQ(a.slot[i].y - a.slot[i - 1].y, pitch);
}

TEST(ModernMainMenuGeometry, PanelIsASidebarNotACanvasAtEveryResolutionAndScale)
{
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	for (Title t : kTitles)
		for (const Surface &sf : kSurfaces)
			for (int scale : kScales)
			{
				const Geometry g = compute(sf.w, sf.h, scale, t);
				EXPECT_LE(g.stackBottom - g.logo.y, sf.h * m.panelMaxHeightPercent / 100 + 1) << sf.w << "x" << sf.h << " @" << scale;   // the navigation stack, not the full-height band
				EXPECT_LE(g.panel.w * 100, g.composition.w * 60) << sf.w << "x" << sf.h << " @" << scale;
			}
	// a 1080p surface at the 175 % that Auto picks for it: reduced so the panel stays about a quarter of the width
	const Geometry g = compute(1920, 1080, 175, Title::ZeroHour);
	EXPECT_TRUE(g.compact);
	EXPECT_LT(g.panel.w * 100, 1920 * 30);
}

TEST(ModernMainMenuGeometry, ReducesAnOversizedScaleInsteadOfClipping)
{
	const Geometry g = compute(1920, 1080, 300, Title::Generals);
	EXPECT_TRUE(g.compact);
	EXPECT_LT(g.effectiveScale, 300);
	EXPECT_LE(g.panel.y + g.panel.h, 1080);
	const Geometry fit = compute(2560, 1440, 100, Title::Generals);
	EXPECT_FALSE(fit.compact);
	EXPECT_EQ(fit.effectiveScale, 100);
}

TEST(ModernMainMenuGeometry, RelayoutCostIsNegligible)
{
	const auto t0 = std::chrono::steady_clock::now();
	int sink = 0;
	for (int i = 0; i < 2000; ++i) sink += compute(3440, 1440, 100 + (i % 4) * 50, Title::Generals).panel.w;
	const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 2000.0;
	EXPECT_GT(sink, 0);
	EXPECT_LT(us, 250.0) << "microseconds per compute(); it runs only when the surface, the UI scale or the page changes";
}

// ---- theme -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TEST(ModernTheme, TypographyRolesAreCompleteAndOrdered)
{
	using ModernTheme::TypeRole;
	for (int r = 0; r < (int)TypeRole::Count; ++r)
	{
		const ModernTheme::TypeStyle &s = ModernTheme::type((TypeRole)r);
		ASSERT_NE(s.families[0], nullptr);
		EXPECT_GT(std::strlen(s.families[0]), 0u);
		EXPECT_GT(s.referenceSize, 0);
	}
	EXPECT_GE(ModernTheme::type(TypeRole::Title).referenceSize, ModernTheme::type(TypeRole::MajorHeading).referenceSize);
	EXPECT_GE(ModernTheme::type(TypeRole::MajorHeading).referenceSize, ModernTheme::type(TypeRole::MenuPrimary).referenceSize);
	EXPECT_GE(ModernTheme::type(TypeRole::MenuPrimary).referenceSize, ModernTheme::type(TypeRole::Body).referenceSize);
	EXPECT_GE(ModernTheme::type(TypeRole::Body).referenceSize, ModernTheme::type(TypeRole::Helper).referenceSize);
	// the retail "Generals" family is not installed anywhere: the roles name real fonts only, in an ordered list that ends in a font every Windows has
	for (int r = 0; r < (int)TypeRole::Count; ++r)
	{
		const ModernTheme::TypeStyle &s = ModernTheme::type((TypeRole)r);
		for (int i = 0; i < ModernTheme::kFontCandidates && s.families[i]; ++i) EXPECT_STRNE(s.families[i], "Generals");
		EXPECT_STREQ(s.families[ModernTheme::kFontCandidates - 1], "Arial");
	}
}

TEST(ModernTheme, TitleVariantsAreDataNotDuplicatedLogic)
{
	const ModernTheme::Variant &g = ModernTheme::variant(Title::Generals);
	const ModernTheme::Variant &z = ModernTheme::variant(Title::ZeroHour);
	// shared base language
	EXPECT_EQ(g.palette.scrim.argb(), z.palette.scrim.argb());
	EXPECT_EQ(g.palette.panel.argb(), z.palette.panel.argb());
	EXPECT_EQ(g.palette.text.argb(), z.palette.text.argb());
	EXPECT_EQ(g.palette.hairline.argb(), z.palette.hairline.argb());
	// controlled differences: the accent (and what derives from it)
	EXPECT_NE(g.palette.accent.argb(), z.palette.accent.argb());
	EXPECT_NE(g.palette.accentSoft.argb(), z.palette.accentSoft.argb());
	EXPECT_STRNE(g.name, z.name);
	// the translucent surfaces keep the shell map readable
	EXPECT_GT(g.palette.scrim.a, 100); EXPECT_LT(g.palette.scrim.a, 250);
	EXPECT_NE(g.logoImage, nullptr);
	EXPECT_GT(g.logoWidth, g.logoHeight);
}
