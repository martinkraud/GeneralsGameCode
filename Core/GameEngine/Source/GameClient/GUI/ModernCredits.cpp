// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience credits screen. See GameClient/ModernCredits.h.
#include "PreRTS.h"

#include "GameClient/ModernCredits.h"

#include "Common/DisplayRuntime.h"
#include "Common/ModernExperience.h"
#include "Common/ModernTheme.h"
#include "GameClient/Display.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/ModernDraw.h"
#include "GameClient/ModernMenuShell.h"

namespace
{
struct State
{
	bool built = false;
	GameWindow *parent = nullptr, *backdrop = nullptr;
	int surfaceW = 0, surfaceH = 0, uiPercent = 0;
	ModernDraw::Decor decor;
} s;

// the retail screen is a stack of image / colour windows (a soldier portrait, a grid frame): none of them draws any more, the text window keeps drawing the scroller
void clearArt(GameWindow *w)
{
	w->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
	w->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	w->winClearStatus(WIN_STATUS_IMAGE);
	w->winSetEnabledImage(0, nullptr);
	w->winSetDisabledImage(0, nullptr);
	w->winSetHiliteImage(0, nullptr);
	for (GameWindow *c = w->winGetChild(); c; c = c->winGetNext())
		clearArt(c);
}

void relayout()
{
	s.surfaceW = TheDisplay->getWidth();
	s.surfaceH = TheDisplay->getHeight();
	s.uiPercent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (s.uiPercent <= 0) s.uiPercent = 100;
	const ModernTheme::Palette &p = ModernDraw::palette();
	// the text column is centred (CreditsManager centres every line on the surface); the scrim band is as wide as a comfortable reading column of the 16:9 composition box
	int compW = static_cast<int>(s.surfaceH * ModernTheme::kCompositionMaxAspect + 0.5);
	if (compW > s.surfaceW) compW = s.surfaceW;
	int bandW = compW * 46 / 100;
	const int minW = (900 * s.uiPercent + 50) / 100;
	if (bandW < minW) bandW = minW < s.surfaceW ? minW : s.surfaceW;
	const int x = (s.surfaceW - bandW) / 2;
	s.parent->winSetPosition(0, 0);
	s.parent->winSetSize(s.surfaceW, s.surfaceH);
	s.decor.clear();
	ModernDraw::DecorItem &band = s.decor.add(ModernDraw::DecorKind::Scrim);
	band.x = x; band.y = 0; band.w = bandW; band.h = s.surfaceH;
	const int edge = ModernDraw::px(2);
	for (int side = 0; side < 2; ++side)
	{
		ModernDraw::DecorItem &line = s.decor.add(ModernDraw::DecorKind::Rule);
		line.x = side ? x + bandW - edge : x; line.y = 0; line.w = edge; line.h = s.surfaceH; line.color = p.hairline;
	}
	ModernDraw::DecorItem &tick = s.decor.add(ModernDraw::DecorKind::Tick);
	tick.x = x; tick.y = 0; tick.w = bandW; tick.h = ModernDraw::px(3);
	if (s.backdrop) { s.backdrop->winSetPosition(0, 0); s.backdrop->winSetSize(s.surfaceW, s.surfaceH); }
}
}

namespace ModernCredits
{
bool attach(GameWindow *creditsParent)
{
	forget();
	if (!ModernMenuShell::active() || !creditsParent || !TheDisplay) return false;
	s.parent = creditsParent;
	// the retail background fill and frame are black: they draw nothing, the scrim band is the surface
	clearArt(creditsParent);
	creditsParent->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
	creditsParent->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	s.backdrop = ModernDraw::createBackdrop(creditsParent, "CreditsMenu.wnd:ModernBackdrop", &s.decor);
	relayout();
	if (s.backdrop)
		for (GameWindow *c = creditsParent->winGetChild(); c; c = c->winGetNext())
			if (c != s.backdrop) c->winBringToTop();   // the retail text window draws the scroller: above the scrim
	s.built = true;
	return true;
}

void update()
{
	if (!s.built) return;
	const int uiPercent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (TheDisplay->getWidth() != s.surfaceW || TheDisplay->getHeight() != s.surfaceH || (uiPercent > 0 ? uiPercent : 100) != s.uiPercent)
		relayout();
}

void forget()
{
	s.built = false;
	s.parent = s.backdrop = nullptr;
	s.decor.clear();
}

bool active() { return s.built; }
}
