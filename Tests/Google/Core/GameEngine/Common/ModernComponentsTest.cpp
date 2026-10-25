// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Frontend design system: component states, style table, contrast and the chamfered-shape geometry (pure, engine-free).
#include <gtest/gtest.h>

#include "Common/ModernComponents.h"

using namespace ModernComponents;
using ModernTheme::Title;

namespace
{
const Title kTitles[] = { Title::Generals, Title::ZeroHour };
}

TEST(ModernComponents, StateResolutionOrder)
{
	Flags f;
	EXPECT_EQ(resolve(f), State::Normal);
	f.selected = true;  EXPECT_EQ(resolve(f), State::Selected);
	f.hover = true;     EXPECT_EQ(resolve(f), State::Hover);       // hover beats the resting selected look
	f.focus = true;     EXPECT_EQ(resolve(f), State::Focus);       // focus beats hover
	f.pressed = true;   EXPECT_EQ(resolve(f), State::Pressed);     // a held press beats focus
	f.enabled = false;  EXPECT_EQ(resolve(f), State::Disabled);    // disabled beats everything
}

TEST(ModernComponents, EveryKindAndStateHasAReadableStyle)
{
	for (Title t : kTitles)
	{
		const ModernTheme::Palette &p = ModernTheme::variant(t).palette;
		// the components sit on the scrim, the scrim sits on the shell map: judge against a bright sandy map patch seen THROUGH the scrim
		const ModernTheme::Color bright = { 190, 180, 150, 255 };
		const ModernTheme::Color behind = mix(bright, ModernTheme::Color{ p.scrim.r, p.scrim.g, p.scrim.b, 255 }, p.scrim.a * 100 / 255);
		for (int k = 0; k < (int)Kind::Count; ++k)
			for (int s = 0; s <= (int)State::Selected; ++s)
			{
				if ((State)s == State::Disabled) continue;   // disabled text is deliberately quiet
				const Style st = style((Kind)k, (State)s, p);
				const ModernTheme::Color surface = st.fill.a ? st.fill : ModernTheme::Color{ behind.r, behind.g, behind.b, 0 };
				// text is bold and at least 12 pt: 4.5:1 at rest, 3:1 (large-text criterion) on a pressed / filled-accent surface
				const double need = ((State)s == State::Pressed) ? 3.0 : 4.5;
				EXPECT_GE(contrast(st.text, surface, behind), need) << "kind " << k << " state " << s << " title " << (int)t;
			}
	}
}

TEST(ModernComponents, FocusIsVisiblyDistinctFromRestForInteractiveKinds)
{
	for (Title t : kTitles)
	{
		const ModernTheme::Palette &p = ModernTheme::variant(t).palette;
		for (Kind k : { Kind::Nav, Kind::NavMinor, Kind::NavDanger, Kind::Primary, Kind::Secondary, Kind::Danger, Kind::Tab, Kind::Row })
		{
			const Style rest = style(k, State::Normal, p), focus = style(k, State::Focus, p);
			const bool differs = rest.fill.argb() != focus.fill.argb() || rest.edge.argb() != focus.edge.argb() || rest.tick.argb() != focus.tick.argb() || rest.text.argb() != focus.text.argb();
			EXPECT_TRUE(differs) << (int)k;
		}
	}
}

TEST(ModernComponents, DisabledNeverLooksActive)
{
	for (Title t : kTitles)
	{
		const ModernTheme::Palette &p = ModernTheme::variant(t).palette;
		for (int k = 0; k < (int)Kind::Count; ++k)
		{
			const Style d = style((Kind)k, State::Disabled, p);
			EXPECT_EQ(d.text.argb(), p.textDisabled.argb()) << k;
			EXPECT_EQ(d.tick.a, 0) << k;
		}
	}
}

TEST(ModernComponents, DangerKindsUseTheDangerColourAndNotTheAccent)
{
	for (Title t : kTitles)
	{
		const ModernTheme::Palette &p = ModernTheme::variant(t).palette;
		EXPECT_EQ(style(Kind::Danger, State::Hover, p).edge.argb(), p.danger.argb());
		EXPECT_EQ(style(Kind::NavDanger, State::Focus, p).tick.argb(), p.danger.argb());
		EXPECT_EQ(style(Kind::Nav, State::Focus, p).tick.argb(), p.accent.argb());
		EXPECT_NE(p.danger.argb(), p.accent.argb());
	}
}

TEST(ModernComponents, TitleVariantsChangeOnlyTheAccentFamily)
{
	const ModernTheme::Palette &g = ModernTheme::variant(Title::Generals).palette, &z = ModernTheme::variant(Title::ZeroHour).palette;
	for (int k = 0; k < (int)Kind::Count; ++k)
	{
		const Style a = style((Kind)k, State::Normal, g), b = style((Kind)k, State::Normal, z);
		EXPECT_EQ(a.edgeWidth, b.edgeWidth);
		EXPECT_EQ(a.chamfer, b.chamfer);
	}
	EXPECT_NE(style(Kind::Primary, State::Normal, g).fill.argb(), style(Kind::Primary, State::Normal, z).fill.argb());
}

TEST(ModernComponents, MixAndContrastBasics)
{
	const ModernTheme::Color black{ 0, 0, 0, 255 }, white{ 255, 255, 255, 255 };
	EXPECT_EQ(mix(black, white, 0).argb(), black.argb());
	EXPECT_EQ(mix(black, white, 100).argb(), white.argb());
	EXPECT_NEAR(contrast(white, black, black), 21.0, 0.01);
	EXPECT_NEAR(contrast(white, white, black), 1.0, 0.01);
	// a translucent surface lets the backdrop through
	EXPECT_LT(contrast(white, ModernTheme::Color{ 255, 255, 255, 128 }, black), contrast(white, ModernTheme::Color{ 0, 0, 0, 128 }, black));
}

TEST(ModernChamfer, SquareShapeIsOneSpan)
{
	Span s[kMaxSpans];
	ASSERT_EQ(chamferSpans(40, 20, 0, 0, 0, 0, s, kMaxSpans), 1);
	EXPECT_EQ(s[0].x, 0); EXPECT_EQ(s[0].y, 0); EXPECT_EQ(s[0].w, 40); EXPECT_EQ(s[0].h, 20);
}

TEST(ModernChamfer, SpansTileTheShapeWithoutOverlapAndStayInside)
{
	const int w = 60, h = 30;
	for (int tl : { 0, 6 }) for (int tr : { 0, 12 }) for (int br : { 0, 6 }) for (int bl : { 0, 12 })
	{
		Span s[kMaxSpans];
		const int n = chamferSpans(w, h, tl, tr, br, bl, s, kMaxSpans);
		ASSERT_GT(n, 0);
		int area = 0;
		for (int i = 0; i < n; ++i)
		{
			EXPECT_GE(s[i].x, 0); EXPECT_GE(s[i].y, 0);
			EXPECT_LE(s[i].x + s[i].w, w); EXPECT_LE(s[i].y + s[i].h, h);
			area += s[i].w * s[i].h;
			if (i) EXPECT_GE(s[i].y, s[i - 1].y + s[i - 1].h);   // rows only advance: no overlap
		}
		// the removed corners are right triangles of leg c: c*(c+1)/2 pixels each (a cut of c leaves rows c, c-1 ... 1 removed)
		auto tri = [](int c) { return c * (c + 1) / 2; };
		EXPECT_EQ(area, w * h - tri(tl) - tri(tr) - tri(br) - tri(bl));
	}
}

TEST(ModernChamfer, CornersAreCutAndTheCentreIsInside)
{
	EXPECT_FALSE(inChamfer(0, 0, 40, 40, 10, 0, 0, 0));
	EXPECT_TRUE(inChamfer(20, 20, 40, 40, 10, 10, 10, 10));
	EXPECT_FALSE(inChamfer(39, 39, 40, 40, 0, 0, 10, 0));
	EXPECT_TRUE(inChamfer(0, 39, 40, 40, 10, 10, 10, 0));
	EXPECT_FALSE(inChamfer(-1, 5, 40, 40, 0, 0, 0, 0));
}

TEST(ModernChamfer, DegenerateInputIsSafe)
{
	Span s[kMaxSpans];
	EXPECT_EQ(chamferSpans(0, 10, 3, 3, 3, 3, s, kMaxSpans), 0);
	EXPECT_EQ(chamferSpans(10, 10, 3, 3, 3, 3, s, 0), 0);
	// an oversized cut is clamped to half the shorter side and still yields a valid shape
	const int n = chamferSpans(10, 4, 50, 50, 50, 50, s, kMaxSpans);
	EXPECT_GT(n, 0);
	for (int i = 0; i < n; ++i) EXPECT_GT(s[i].w, 0);
	// a small span budget truncates instead of overflowing
	Span tiny[3];
	EXPECT_LE(chamferSpans(100, 100, 40, 40, 40, 40, tiny, 3), 3);
}
