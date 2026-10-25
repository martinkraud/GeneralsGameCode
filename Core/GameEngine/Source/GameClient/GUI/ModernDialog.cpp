// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience modal dialogs. See GameClient/ModernDialog.h.
#include "PreRTS.h"

#include "GameClient/ModernDialog.h"

#include <cwchar>
#include <cwctype>

#include "Common/DisplayRuntime.h"
#include "Common/ModernExperience.h"
#include "Common/ModernScreenLayout.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/ModernDraw.h"
#include "GameClient/ModernMenuShell.h"

namespace
{
using ModernTheme::TypeRole;

// A dialog's chrome (scrim, panel, title rule) is one backdrop. Dialogs can stack (a confirmation over a confirmation), so a few decors rotate; a decor is rebuilt when reused.
constexpr int kPool = 4;
ModernDraw::Decor g_decors[kPool];
int g_next = 0;

UnicodeString upper(const UnicodeString &s)
{
	UnicodeString u;
	for (const WideChar *p = s.str(); p && *p; ++p) u.concat(static_cast<WideChar>(towupper(*p)));
	return u;
}

GameWindow *child(GameWindow *parent, const char *menuName, const char *name)
{
	AsciiString key = menuName;
	key.concat(name);
	return TheWindowManager->winGetWindowFromId(parent, TheNameKeyGenerator->nameToKey(key));
}

void leftAligned(GameWindow *w, bool centeredVertically)
{
	if (!w || !BitIsSet(w->winGetStyle(), GWS_STATIC_TEXT)) return;
	w->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));          // the retail text boxes are filled black with a border: the Modern panel is the surface
	w->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	if (TextData *data = static_cast<TextData *>(w->winGetUserData()))
	{
		data->centered = FALSE;
		data->centeredVertically = centeredVertically ? TRUE : FALSE;
	}
}
}

namespace ModernDialog
{
bool active() { return ModernMenuShell::active(); }

void apply(GameWindow *trueParent, GameWindow *parent, const char *menuName, const Buttons &buttons, const UnicodeString &title, const UnicodeString &body)
{
	if (!active() || !trueParent || !parent || !TheDisplay) return;
	const ModernTheme::Palette &p = ModernDraw::palette();
	const int surfaceW = TheDisplay->getWidth(), surfaceH = TheDisplay->getHeight();
	int percent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (percent <= 0) percent = 100;
	const int scale = ModernScreen::dialogScale(surfaceW, surfaceH, percent);
	ModernDraw::beginScreen(ModernDraw::Screen::Dialog);   // a dialog has its own scale; the screen below keeps its own
	ModernDraw::setScalePercent(scale);

	// measure the wrapped body at its final width
	int bodyH = 0;
	if (TheDisplayStringManager)
		if (DisplayString *ds = TheDisplayStringManager->newDisplayString())
		{
			ds->setFont(ModernDraw::fontAt(TypeRole::Body, scale));
			ds->setWordWrap(ModernScreen::dialogInnerWidth(surfaceW, scale));
			ds->setText(body);
			int w = 0;
			ds->getSize(&w, &bodyH);
			TheDisplayStringManager->freeDisplayString(ds);
		}

	GameWindow *btn[4] = { child(parent, menuName, "ButtonCancel"), child(parent, menuName, "ButtonNo"), child(parent, menuName, "ButtonYes"), child(parent, menuName, "ButtonOk") };
	const bool present[4] = { buttons.cancel, buttons.no, buttons.yes, buttons.ok };
	const char *const keys[4] = { "GUI:Cancel", "GUI:No", "GUI:Yes", "GUI:Ok" };
	const wchar_t *const fallbacks[4] = { L"Cancel", L"No", L"Yes", L"OK" };
	int count = 0;
	for (int i = 0; i < 4; ++i) count += present[i] && btn[i] ? 1 : 0;
	if (count == 0) return;

	ModernScreen::DialogInput in;
	in.surfaceW = surfaceW; in.surfaceH = surfaceH; in.uiPercent = percent; in.buttonCount = count; in.bodyHeight = bodyH;
	const ModernScreen::DialogLayout L = ModernScreen::computeDialog(in);

	// the retail frame: the dim layer and the black panel draw nothing, every decorative child is hidden, the named children are re-placed
	trueParent->winSetPosition(0, 0);
	trueParent->winSetSize(surfaceW, surfaceH);
	trueParent->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
	trueParent->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	parent->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
	parent->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	parent->winSetPosition(L.panel.x, L.panel.y);
	parent->winSetSize(L.panel.w, L.panel.h);
	GameWindow *titleWin = child(parent, menuName, "StaticTextTitle"), *bodyWin = child(parent, menuName, "StaticTextMessage");
	for (GameWindow *c = parent->winGetChild(); c; c = c->winGetNext())
		if (c != titleWin && c != bodyWin && c != btn[0] && c != btn[1] && c != btn[2] && c != btn[3])
			c->winHide(TRUE);

	auto rel = [&](const ModernScreen::Rect &r) { return ModernScreen::Rect{ r.x - L.panel.x, r.y - L.panel.y, r.w, r.h }; };
	// backdrop: scrim over the whole surface, the panel, the title rule
	ModernDraw::Decor &decor = g_decors[g_next++ % kPool];
	decor.clear();
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	auto pxs = [&](int refUnits) { return (refUnits * scale + 50) / 100; };
	ModernDraw::DecorItem &scrim = decor.add(ModernDraw::DecorKind::Scrim);
	scrim.x = -L.panel.x; scrim.y = -L.panel.y; scrim.w = surfaceW; scrim.h = surfaceH;
	ModernDraw::DecorItem &panel = decor.add(ModernDraw::DecorKind::Panel);
	panel.x = 0; panel.y = 0; panel.w = L.panel.w; panel.h = L.panel.h; panel.cutTL = pxs(m.chamfer); panel.cutBR = pxs(m.chamfer); panel.accent = true;
	const ModernScreen::Rect rule = rel(L.titleRule);
	ModernDraw::DecorItem &r1 = decor.add(ModernDraw::DecorKind::Rule);
	r1.x = rule.x; r1.y = rule.y; r1.w = rule.w * 18 / 100; r1.h = rule.h; r1.color = p.accent;
	ModernDraw::DecorItem &r2 = decor.add(ModernDraw::DecorKind::Rule);
	r2.x = rule.x + r1.w; r2.y = rule.y; r2.w = rule.w - r1.w; r2.h = rule.h; r2.color = p.hairline;
	GameWindow *backdrop = ModernDraw::createBackdrop(parent, "ModernDialogBackdrop", &decor);
	if (backdrop) { backdrop->winSetPosition(0, 0); backdrop->winSetSize(L.panel.w, L.panel.h); }

	if (titleWin)
	{
		GadgetStaticTextSetText(titleWin, upper(title));
		ModernDraw::skinText(titleWin, TypeRole::MajorHeading, p.accent);
		titleWin->winSetFont(ModernDraw::fontAt(TypeRole::MajorHeading, scale));
		const ModernScreen::Rect r = rel(L.title);
		titleWin->winSetPosition(r.x, r.y); titleWin->winSetSize(r.w, r.h);
		leftAligned(titleWin, true);
		titleWin->winBringToTop();
	}
	if (bodyWin)
	{
		ModernDraw::skinText(bodyWin, TypeRole::Body, p.text);
		bodyWin->winSetFont(ModernDraw::fontAt(TypeRole::Body, scale));
		const ModernScreen::Rect r = rel(L.body);
		bodyWin->winSetPosition(r.x, r.y); bodyWin->winSetSize(r.w, r.h);
		leftAligned(bodyWin, false);
		bodyWin->winBringToTop();
	}
	int slot = 0;
	for (int i = 0; i < 4; ++i)
	{
		if (!present[i] || !btn[i]) continue;
		const ModernScreen::Rect r = rel(L.button[slot++]);
		const bool primary = (i == 2 || i == 3);
		ModernDraw::skinDialogButton(btn[i], primary ? ModernComponents::Kind::Primary : ModernComponents::Kind::Secondary, TypeRole::MenuSecondary, scale);
		GadgetButtonSetText(btn[i], upper(TheGameText->FETCH_OR_SUBSTITUTE(keys[i], fallbacks[i])));
		btn[i]->winSetPosition(r.x, r.y); btn[i]->winSetSize(r.w, r.h);
		btn[i]->winHide(FALSE);
		btn[i]->winBringToTop();
	}
}
}
