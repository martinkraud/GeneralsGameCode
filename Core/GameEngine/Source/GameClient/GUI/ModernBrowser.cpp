// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience list browsers. See GameClient/ModernBrowser.h.
#include "PreRTS.h"

#include "GameClient/ModernBrowser.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <cwchar>
#include <cwctype>
#include <vector>

#include "Common/DisplayRuntime.h"
#include "Common/ModernBranding.h"
#include "Common/ModernExperience.h"
#include "Common/NameKeyGenerator.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/ModernDraw.h"

namespace
{
using ModernTheme::TypeRole;
constexpr int kMaxDialogs = 4;

struct DialogState
{
	GameWindow *container = nullptr, *body = nullptr, *confirm = nullptr, *cancel = nullptr, *entry = nullptr, *backdrop = nullptr;
	ModernDraw::Decor decor;
};

struct State
{
	bool built = false;
	ModernBrowser::Spec spec = {};
	GameWindow *list = nullptr, *host = nullptr, *outer = nullptr, *actionContainer = nullptr, *backdrop = nullptr;
	GameWindow *action[6] = {};
	std::vector<GameWindow *> chain;          // from the outermost ancestor down to the host (the list's parent)
	DialogState dialog[kMaxDialogs];
	ModernScreen::BrowserLayout layout;
	int surfaceW = 0, surfaceH = 0, uiPercent = 0;
	ModernDraw::Decor decor;
} s;

GameWindow *byName(const char *name)
{
	if (!name) return nullptr;
	char full[200];
	snprintf(full, sizeof(full), "%s%s", s.spec.menu, name);
	return TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(full));
}

UnicodeString upper(const UnicodeString &u)
{
	UnicodeString r;
	for (const WideChar *p = u.str(); p && *p; ++p) r.concat(static_cast<WideChar>(towupper(*p)));
	return r;
}

void place(GameWindow *w, const ModernScreen::Rect &r)
{
	if (!w) return;
	GameWindow *parent = w->winGetParent();
	Int px = 0, py = 0;
	if (parent) parent->winGetScreenPosition(&px, &py);
	w->winSetPosition(r.x - px, r.y - py);
	w->winSetSize(r.w, r.h);
}

void transparent(GameWindow *w)
{
	w->winSetEnabledColor(0, GameMakeColor(0, 0, 0, 0));
	w->winSetEnabledBorderColor(0, GameMakeColor(0, 0, 0, 0));
	w->winClearStatus(WIN_STATUS_IMAGE);
	w->winClearStatus(WIN_STATUS_BORDER);
}

void leftAligned(GameWindow *w, bool vcenter)
{
	if (!w || !BitIsSet(w->winGetStyle(), GWS_STATIC_TEXT)) return;
	transparent(w);
	if (TextData *data = static_cast<TextData *>(w->winGetUserData()))
	{
		data->centered = FALSE;
		data->centeredVertically = vcenter ? TRUE : FALSE;
	}
}

GameWindow *firstStaticText(GameWindow *container)
{
	for (GameWindow *c = container->winGetChild(); c; c = c->winGetNext())
		if (BitIsSet(c->winGetStyle(), GWS_STATIC_TEXT)) return c;
	return nullptr;
}

int measureHeight(const UnicodeString &text, TypeRole role, int scale, int width)
{
	int h = 0;
	if (TheDisplayStringManager)
		if (DisplayString *ds = TheDisplayStringManager->newDisplayString())
		{
			ds->setFont(ModernDraw::fontAt(role, scale));
			ds->setWordWrap(width);
			ds->setText(text);
			int w = 0;
			ds->getSize(&w, &h);
			TheDisplayStringManager->freeDisplayString(ds);
		}
	return h;
}

void layoutDialogs()
{
	const ModernTheme::Palette &p = ModernDraw::palette();
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	const int scale = s.layout.effectiveScale;
	for (int i = 0; i < s.spec.dialogCount && i < kMaxDialogs; ++i)
	{
		DialogState &d = s.dialog[i];
		if (!d.container) continue;
		const ModernBrowser::InlineDialog &spec = s.spec.dialogs[i];
		const UnicodeString body = d.body ? d.body->winGetText() : UnicodeString::TheEmptyString;
		ModernScreen::DialogInput in;
		in.surfaceW = s.layout.panel.w; in.surfaceH = s.layout.panel.h; in.uiPercent = scale; in.buttonCount = 2; in.extraHeight = d.entry ? 36 : 0;
		in.bodyHeight = measureHeight(body, TypeRole::Body, scale, ModernScreen::dialogInnerWidth(s.layout.panel.w, scale));
		ModernScreen::DialogLayout L = ModernScreen::computeDialog(in);
		// computeDialog centres in the given "surface": shift into the browser panel
		auto shift = [&](ModernScreen::Rect &r) { r.x += s.layout.panel.x; r.y += s.layout.panel.y; };
		shift(L.panel); shift(L.title); shift(L.titleRule); shift(L.body); shift(L.extra); shift(L.button[0]); shift(L.button[1]);
		place(d.container, L.panel);
		transparent(d.container);
		// chrome: dim the browser behind, the dialog panel, the title and its rule
		d.decor.clear();
		auto rel = [&](const ModernScreen::Rect &r) { return ModernScreen::Rect{ r.x - L.panel.x, r.y - L.panel.y, r.w, r.h }; };
		ModernDraw::DecorItem &scrim = d.decor.add(ModernDraw::DecorKind::Scrim);
		scrim.x = s.layout.panel.x - L.panel.x; scrim.y = s.layout.panel.y - L.panel.y; scrim.w = s.layout.panel.w; scrim.h = s.layout.panel.h;
		ModernDraw::DecorItem &panel = d.decor.add(ModernDraw::DecorKind::Panel);
		panel.x = 0; panel.y = 0; panel.w = L.panel.w; panel.h = L.panel.h; panel.cutTL = ModernDraw::px(m.chamfer); panel.cutBR = ModernDraw::px(m.chamfer); panel.accent = true;
		const ModernScreen::Rect t = rel(L.title);
		ModernDraw::DecorItem &title = d.decor.add(ModernDraw::DecorKind::Text);
		title.x = t.x; title.y = t.y; title.w = t.w; title.h = t.h; title.role = TypeRole::MajorHeading; title.color = p.accent;
		wcsncpy(title.text, upper(TheGameText->FETCH_OR_SUBSTITUTE(spec.titleKey, spec.titleFallback)).str(), 95);
		const ModernScreen::Rect rl = rel(L.titleRule);
		ModernDraw::DecorItem &r1 = d.decor.add(ModernDraw::DecorKind::Rule);
		r1.x = rl.x; r1.y = rl.y; r1.w = rl.w * 18 / 100; r1.h = rl.h; r1.color = p.accent;
		ModernDraw::DecorItem &r2 = d.decor.add(ModernDraw::DecorKind::Rule);
		r2.x = rl.x + r1.w; r2.y = rl.y; r2.w = rl.w - r1.w; r2.h = rl.h; r2.color = p.hairline;
		d.decor.refreshFonts();
		if (!d.backdrop) d.backdrop = ModernDraw::createBackdrop(d.container, (std::string("ModernBrowserDialogBackdrop") + std::to_string(i)).c_str(), &d.decor);
		if (d.backdrop) { place(d.backdrop, L.panel); d.backdrop->winBringToTop(); }
		for (GameWindow *c = d.container->winGetChild(); c; c = c->winGetNext())
			if (c != d.body && c != d.confirm && c != d.cancel && c != d.entry && c != d.backdrop) c->winHide(TRUE);
		if (d.body) { ModernDraw::skinText(d.body, TypeRole::Body, p.text); d.body->winSetFont(ModernDraw::fontAt(TypeRole::Body, scale)); leftAligned(d.body, false); place(d.body, L.body); d.body->winBringToTop(); }
		if (d.entry) { ModernDraw::skinEntry(d.entry); d.entry->winSetFont(ModernDraw::fontAt(TypeRole::Value, scale)); place(d.entry, L.extra); d.entry->winBringToTop(); }
		if (d.cancel) { ModernDraw::skinDialogButton(d.cancel, ModernComponents::Kind::Secondary, TypeRole::MenuSecondary, scale); place(d.cancel, L.button[0]); d.cancel->winBringToTop(); }
		if (d.confirm) { ModernDraw::skinDialogButton(d.confirm, ModernComponents::Kind::Primary, TypeRole::MenuSecondary, scale); place(d.confirm, L.button[1]); d.confirm->winBringToTop(); }
	}
}

void rebuildDecor()
{
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	const ModernTheme::Palette &p = ModernDraw::palette();
	const ModernScreen::BrowserLayout &L = s.layout;
	s.decor.clear();
	const int cut = ModernDraw::px(m.chamfer);
	ModernDraw::DecorItem &panel = s.decor.add(ModernDraw::DecorKind::Panel);
	panel.x = 0; panel.y = 0; panel.w = L.panel.w; panel.h = L.panel.h; panel.cutTL = cut; panel.cutBR = cut;
	ModernDraw::DecorItem &bar = s.decor.add(ModernDraw::DecorKind::Tick);
	bar.x = cut; bar.y = 0; bar.w = ModernDraw::px(110); bar.h = ModernDraw::px(3);
	ModernDraw::DecorItem &title = s.decor.add(ModernDraw::DecorKind::Text);
	title.x = L.title.x - L.panel.x; title.y = L.title.y - L.panel.y; title.w = L.title.w; title.h = L.title.h; title.role = TypeRole::Title; title.color = p.text;
	wcsncpy(title.text, upper(TheGameText->FETCH_OR_SUBSTITUTE(s.spec.titleKey, s.spec.titleFallback)).str(), 95);
	ModernDraw::DecorItem &rule = s.decor.add(ModernDraw::DecorKind::Rule);
	rule.x = L.titleRule.x - L.panel.x; rule.y = L.titleRule.y - L.panel.y; rule.w = L.titleRule.w * 12 / 100; rule.h = L.titleRule.h; rule.color = p.accent;
	ModernDraw::DecorItem &rest = s.decor.add(ModernDraw::DecorKind::Rule);
	rest.x = rule.x + rule.w; rest.y = rule.y; rest.w = L.titleRule.w - rule.w; rest.h = rule.h; rest.color = p.hairline;
	ModernDraw::DecorItem &field = s.decor.add(ModernDraw::DecorKind::Field);
	field.x = L.list.x - L.panel.x; field.y = L.list.y - L.panel.y; field.w = L.list.w; field.h = L.list.h;
	ModernDraw::DecorItem &sep = s.decor.add(ModernDraw::DecorKind::Rule);
	sep.x = L.separator.x - L.panel.x; sep.y = L.separator.y - L.panel.y; sep.w = L.separator.w; sep.h = L.separator.h; sep.color = p.hairline;
	s.decor.refreshFonts();
}

void relayout()
{
	s.surfaceW = TheDisplay->getWidth();
	s.surfaceH = TheDisplay->getHeight();
	s.uiPercent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (s.uiPercent <= 0) s.uiPercent = 100;
	ModernScreen::BrowserInput in;
	in.surfaceW = s.surfaceW; in.surfaceH = s.surfaceH; in.uiPercent = s.uiPercent; in.actionCount = s.spec.actionCount;
	s.layout = ModernScreen::computeBrowser(in);
	const ModernScreen::BrowserLayout &L = s.layout;
	ModernDraw::beginScreen(ModernDraw::Screen::Browser);
	ModernDraw::setScalePercent(L.effectiveScale);

	// the ancestors: the outermost covers the surface, the others the panel (a child is hit only inside every ancestor)
	for (size_t i = 0; i < s.chain.size(); ++i)
	{
		GameWindow *w = s.chain[i];
		transparent(w);
		if (i == 0) { w->winSetPosition(0, 0); w->winSetSize(s.surfaceW, s.surfaceH); }
		else place(w, L.panel);
	}
	if (s.backdrop) place(s.backdrop, L.panel);
	if (s.actionContainer) { place(s.actionContainer, L.actionColumn); transparent(s.actionContainer); }
	for (int i = 0; i < s.spec.actionCount; ++i)
		if (s.action[i]) place(s.action[i], L.action[i]);
	place(s.list, L.list);

	// everything else of the retail screen stays hidden (caption, lines, frames); the retail confirmation panels are managed by the screen itself
	std::vector<GameWindow *> keep = { s.list, s.backdrop, s.actionContainer };
	for (int i = 0; i < s.spec.actionCount; ++i) keep.push_back(s.action[i]);
	for (int i = 0; i < s.spec.dialogCount && i < kMaxDialogs; ++i) keep.push_back(s.dialog[i].container);
	for (size_t i = 0; i < s.chain.size(); ++i) keep.push_back(s.chain[i]);
	for (GameWindow *w : s.chain)
		for (GameWindow *c = w->winGetChild(); c; c = c->winGetNext())
		{
			bool k = false;
			for (GameWindow *x : keep) if (x == c) k = true;
			if (!k) c->winHide(TRUE);
		}
	if (s.backdrop) s.backdrop->winBringToTop();
	if (s.actionContainer) s.actionContainer->winBringToTop();
	for (int i = 0; i < s.spec.actionCount; ++i) if (s.action[i] && s.action[i]->winGetParent() == s.host) s.action[i]->winBringToTop();
	s.list->winBringToTop();
	rebuildDecor();
	layoutDialogs();
}
}

namespace ModernBrowser
{
const Spec &saveLoadSpec(const char *menuPrefix)
{
	using ModernComponents::Kind;
	static Spec full, popup;
	Spec &sp = (menuPrefix && !std::strncmp(menuPrefix, "Popup", 5)) ? popup : full;
	if (!sp.menu)
	{
		sp.menu = (&sp == &popup) ? "PopupSaveLoad.wnd:" : "SaveLoad.wnd:";
		sp.list = "ListboxGames";
		sp.actionContainer = "MenuButtonFrame";
		sp.titleKey = "GUI:SelectAGame"; sp.titleFallback = L"Select a game";
		sp.actions[0] = { "ButtonLoad", Kind::Primary, "GUI:LoadGame", L"Load game" };
		sp.actions[1] = { "ButtonSave", Kind::Secondary, "GUI:SaveGame", L"Save game" };
		sp.actions[2] = { "ButtonDelete", Kind::Danger, "GUI:DeleteGame", L"Delete game" };
		sp.actions[3] = { "ButtonBack", Kind::Secondary, "GUI:Back", L"Back" };
		sp.actionCount = 4;
		sp.dialogs[0] = { "LoadConfirmParent", "ButtonLoadConfirm", "ButtonLoadCancel", nullptr, "GUI:LoadGame", L"Load game" };
		sp.dialogs[1] = { "OverwriteConfirmParent", "ButtonOverwriteConfirm", "ButtonOverwriteCancel", nullptr, "GUI:SaveGame", L"Save game" };
		sp.dialogs[2] = { "SaveDescParent", "ButtonSaveDescConfirm", "ButtonSaveDescCancel", "EntryDesc", "GUI:SaveGame", L"Save game" };
		sp.dialogs[3] = { "DeleteConfirmParent", "ButtonDeleteConfirm", "ButtonDeleteCancel", nullptr, "GUI:DeleteGame", L"Delete game" };
		sp.dialogCount = 4;
	}
	return sp;
}

const Spec &replaySpec()
{
	using ModernComponents::Kind;
	static Spec sp;
	if (!sp.menu)
	{
		sp.menu = "ReplayMenu.wnd:";
		sp.list = "ListboxReplayFiles";
		sp.actionContainer = nullptr;
		sp.titleKey = "GUI:LoadReplay"; sp.titleFallback = L"Load replay";
		sp.actions[0] = { "ButtonLoadReplay", Kind::Primary, "GUI:LoadReplay", L"Load replay" };
		sp.actions[1] = { "ButtonCopyReplay", Kind::Secondary, "GUI:CopyReplay", L"Copy replay" };
		sp.actions[2] = { "ButtonDeleteReplay", Kind::Danger, "GUI:DeleteReplay", L"Delete replay" };
		sp.actions[3] = { "ButtonBack", Kind::Secondary, "GUI:Back", L"Back" };
		sp.actionCount = 4;
		sp.dialogCount = 0;
	}
	return sp;
}

bool create(const Spec &spec)
{
	forget();
	if (!DisplayRuntime::modern() || ModernExperience::classic()) return false;
	s.spec = spec;
	s.list = byName(spec.list);
	if (!s.list || !s.list->winGetParent()) return false;
	s.host = s.list->winGetParent();
	for (GameWindow *w = s.host; w; w = w->winGetParent()) s.chain.insert(s.chain.begin(), w);
	s.outer = s.chain.front();
	s.actionContainer = spec.actionContainer ? byName(spec.actionContainer) : nullptr;
	ModernDraw::beginScreen(ModernDraw::Screen::Browser);
	ModernDraw::clearRegistry(ModernDraw::Screen::Browser);
	for (int i = 0; i < spec.actionCount; ++i)
	{
		s.action[i] = byName(spec.actions[i].window);
		if (!s.action[i]) return false;
		ModernDraw::skinButton(s.action[i], spec.actions[i].kind, TypeRole::MenuSecondary);
		GadgetButtonSetText(s.action[i], upper(TheGameText->FETCH_OR_SUBSTITUTE(spec.actions[i].labelKey, spec.actions[i].fallback)));
	}
	ModernDraw::skinList(s.list);
	for (int i = 0; i < spec.dialogCount && i < kMaxDialogs; ++i)
	{
		DialogState &d = s.dialog[i];
		d.container = byName(spec.dialogs[i].container);
		if (!d.container) continue;
		d.body = firstStaticText(d.container);
		d.confirm = byName(spec.dialogs[i].confirm);
		d.cancel = byName(spec.dialogs[i].cancel);
		d.entry = spec.dialogs[i].entry ? byName(spec.dialogs[i].entry) : nullptr;
	}
	s.backdrop = ModernDraw::createBackdrop(s.host, (std::string(spec.menu) + "ModernBackdrop").c_str(), &s.decor);
	relayout();
	s.built = true;
	char detail[200];
	snprintf(detail, sizeof(detail), "scale=%d effective=%d panel=%dx%d list=%dx%d", s.uiPercent, s.layout.effectiveScale, s.layout.panel.w, s.layout.panel.h, s.layout.list.w, s.layout.list.h);
	StartupDiagnostics::checkpoint("browser", "complete", detail);
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
	s.list = s.host = s.outer = s.actionContainer = s.backdrop = nullptr;
	for (GameWindow *&w : s.action) w = nullptr;
	s.chain.clear();
	for (DialogState &d : s.dialog) { d.decor.clear(); d.container = d.body = d.confirm = d.cancel = d.entry = d.backdrop = nullptr; }
	s.decor.clear();
	ModernDraw::clearRegistry(ModernDraw::Screen::Browser);
}

bool active() { return s.built; }
const ModernScreen::BrowserLayout &layout() { return s.layout; }
GameWindow *actionWindow(int index) { return index >= 0 && index < 6 ? s.action[index] : nullptr; }
GameWindow *listWindow() { return s.list; }
GameWindow *dialogWindow(int index) { return index >= 0 && index < kMaxDialogs ? s.dialog[index].container : nullptr; }
}
