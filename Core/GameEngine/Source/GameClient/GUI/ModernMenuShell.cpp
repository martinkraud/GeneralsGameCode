// SPDX-License-Identifier: GPL-3.0-or-later
// UI.2 Modern Experience main menu binding layer. See GameClient/ModernMenuShell.h for ownership, lifetime and behaviour.
#include "PreRTS.h"

#include "GameClient/ModernMenuShell.h"

#include <cstdlib>
#include <cstring>
#include <windows.h>

#include "Common/AudioEventRTS.h"
#include "Common/DisplayRuntime.h"
#include "Common/GameAudio.h"
#include "Common/ModernBranding.h"
#include "Common/ModernExperience.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionPreferences.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/Display.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/WinInstanceData.h"

namespace
{
using namespace ModernMainMenu;
using ModernFocus::Direction;

constexpr int kMaxButtons = 24;
constexpr int kSlideMs = 260;

struct ButtonSlot
{
	Action action = Action::None;
	GameWindow *window = nullptr;
};

struct Shell
{
	bool built = false;
	bool revealed = false;
	bool instantReveal = false;
	bool pageDirty = true;
	GameWindow *parent = nullptr, *panel = nullptr, *logo = nullptr, *heading = nullptr, *footnote = nullptr;
	ButtonSlot buttons[kMaxButtons];
	int buttonCount = 0;
	State state;
	bool stateValid = false;
	bool forget = false;
	ModernMenuShell::Hooks hooks = {};
	Title title = Title::Generals;
	Geometry geo;
	int surfaceW = 0, surfaceH = 0, uiPercent = 0;
	DWORD slideStart = 0;
	bool sliding = false;
	GameFont *fontPrimary = nullptr, *fontSecondary = nullptr, *fontHeading = nullptr, *fontHelper = nullptr;
	GameWindow *buttonDonor = nullptr;
	ModernMenuShell::Stats stats;
} g;

bool g_feature = true;

NameKeyType keyFor(const char *name) { return TheNameKeyGenerator->nameToKey(AsciiString(name)); }

const char *actionName(Action a)
{
	switch (a)
	{
	case Action::GoSoloPlay: return "SoloPlay";   case Action::GoCampaign: return "Campaign"; case Action::GoMultiplayer: return "Multiplayer";
	case Action::GoLoad: return "Load";           case Action::Back: return "Back";           case Action::Training: return "Training";
	case Action::CampaignUSA: return "USA";       case Action::CampaignGLA: return "GLA";     case Action::CampaignChina: return "China";
	case Action::Challenge: return "Challenge";   case Action::DiffEasy: return "Easy";       case Action::DiffMedium: return "Medium";
	case Action::DiffHard: return "Hard";         case Action::Skirmish: return "Skirmish";   case Action::Network: return "Network";
	case Action::Online: return "Online";         case Action::LoadGame: return "LoadGame";   case Action::LoadReplay: return "LoadReplay";
	case Action::Options: return "Options";       case Action::Credits: return "Credits";     case Action::Exit: return "Exit";
	default: return "None";
	}
}

AsciiString buttonName(Action a)
{
	AsciiString s;
	s.format("MainMenu.wnd:Modern%s", actionName(a));
	return s;
}

GameFont *fontFor(ModernTheme::TypeRole role, int effectivePercent)
{
	const ModernTheme::TypeStyle &style = ModernTheme::type(role);
	int pt = (style.referenceSize * effectivePercent + 50) / 100;
	if (pt < 6) pt = 6;
	return TheFontLibrary ? TheFontLibrary->getFont(AsciiString(style.family), pt, style.bold) : nullptr;
}

Color toColor(const ModernTheme::Color &c) { return GameMakeColor(c.r, c.g, c.b, c.a); }

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
}

int focusItemId(Action a) { return static_cast<int>(a); }

ButtonSlot *slotFor(Action a)
{
	for (int i = 0; i < g.buttonCount; ++i) if (g.buttons[i].action == a) return &g.buttons[i];
	return nullptr;
}
ButtonSlot *slotFor(GameWindow *w)
{
	if (!w) return nullptr;
	for (int i = 0; i < g.buttonCount; ++i) if (g.buttons[i].window == w) return &g.buttons[i];
	return nullptr;
}

void playClick()
{
	if (!TheAudio) return;
	AudioEventRTS click("GUIClick");
	TheAudio->addAudioEvent(&click);
}

// ---- drawing: the panel chrome, the heading rule, the focus bracket ----
void line(int x1, int y1, int x2, int y2, int width, UnsignedInt color)
{
	TheDisplay->drawLine(x1, y1, x2, y2, static_cast<Real>(width), color);
}

void corner(int x, int y, int dx, int dy, int len, int width, UnsignedInt color)
{
	line(x, y, x + dx * len, y, width, color);
	line(x, y, x, y + dy * len, width, color);
}

void panelDraw(GameWindow *window, WinInstanceData *)
{
	if (!g.built || !TheDisplay) return;
	LARGE_INTEGER drawStart, drawEnd, drawFreq;
	QueryPerformanceFrequency(&drawFreq);
	QueryPerformanceCounter(&drawStart);
	const ModernTheme::Variant &v = ModernTheme::variant(g.title);
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	int x = 0, y = 0, w = 0, h = 0;
	window->winGetScreenPosition(&x, &y);
	window->winGetSize(&w, &h);
	const int s = g.geo.effectiveScale;
	const int thin = s >= 200 ? 2 : 1;
	TheDisplay->drawFillRect(x, y, w, h, toColor(v.palette.panelFill));
	TheDisplay->drawOpenRect(x, y, w, h, static_cast<Real>(thin), toColor(v.palette.panelEdge));
	const int bl = (m.bracketLength * s + 50) / 100;
	const UnsignedInt accent = toColor(v.palette.accent);
	const int bw = thin + 1;
	corner(x, y, +1, +1, bl, bw, accent);
	corner(x + w - 1, y, -1, +1, bl, bw, accent);
	corner(x, y + h - 1, +1, -1, bl, bw, accent);
	corner(x + w - 1, y + h - 1, -1, -1, bl, bw, accent);
	// rules
	if (g.state.page() != Page::Root)
	{
		const Rect &r = g.geo.headingRule;
		TheDisplay->drawFillRect(x + r.x, y + r.y, r.w, r.h, accent);
	}
	const Rect &fr = g.geo.footerRule;
	TheDisplay->drawFillRect(x + fr.x, y + fr.y, fr.w, fr.h, toColor(v.palette.rule));
	// focus bracket around the focused entry (keyboard focus and hover focus look the same)
	if (g.revealed && g.state.nav.ring.hasFocus())
	{
		const ModernFocus::Item &it = g.state.nav.ring.item(g.state.nav.ring.focusedIndex());
		const int pad = (3 * s + 50) / 100;
		const int fx = x + it.rect.x - pad, fy = y + it.rect.y - pad, fw = it.rect.w + 2 * pad, fh = it.rect.h + 2 * pad;
		const UnsignedInt fc = toColor(v.palette.focus);
		const int arm = (m.bracketLength * s + 50) / 100;
		corner(fx, fy, +1, +1, arm, bw, fc);
		corner(fx + fw - 1, fy, -1, +1, arm, bw, fc);
		corner(fx, fy + fh - 1, +1, -1, arm, bw, fc);
		corner(fx + fw - 1, fy + fh - 1, -1, -1, arm, bw, fc);
	}
	QueryPerformanceCounter(&drawEnd);
	++g.stats.draws;
	g.stats.drawMicros += static_cast<unsigned long long>((drawEnd.QuadPart - drawStart.QuadPart) * 1000000LL / drawFreq.QuadPart);
}

// ---- window creation ----
GameWindow *createText(GameWindow *parent, NameKeyType id, GameFont *font, const ModernTheme::Color &color, bool centeredV)
{
	WinInstanceData inst;
	inst.m_font = font;
	inst.m_style = GWS_STATIC_TEXT;
	inst.m_id = id;
	TextData text = {};
	text.centered = FALSE;
	text.centeredVertically = centeredV ? TRUE : FALSE;
	GameWindow *w = TheWindowManager->gogoGadgetStaticText(parent, WIN_STATUS_ENABLED | WIN_STATUS_NO_INPUT, 0, 0, 10, 10, &inst, &text, font, FALSE);
	if (w)
	{
		w->winSetFont(font);
		w->winSetEnabledTextColors(toColor(color), GameMakeColor(0, 0, 0, 255));
		if (TextData *d = static_cast<TextData *>(w->winGetUserData()))
		{
			d->centered = FALSE;
			d->centeredVertically = centeredV ? TRUE : FALSE;
		}
	}
	return w;
}

GameWindow *createButton(GameWindow *parent, GameWindow *owner, GameWindow *donor, Action a, GameFont *font)
{
	WinInstanceData inst;
	inst.m_font = font;
	inst.m_id = keyFor(buttonName(a).str());
	inst.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
	GameWindow *b = TheWindowManager->gogoGadgetPushButton(parent, WIN_STATUS_ENABLED | WIN_STATUS_IMAGE, 0, 0, 10, 10, &inst, font, FALSE);
	if (!b) return nullptr;
	if (donor) copyAppearance(b, donor);
	b->winSetFont(font);
	b->winSetOwner(owner);   // the gadget sends its click / hover to the retail main menu callback
	return b;
}

void hideRetailVisuals()
{
	for (GameWindow *c = g.parent->winGetChild(); c; c = c->winGetNext())
		if (c != g.panel && c != g.footnote)
			c->winHide(TRUE);
}

// ---- building ----
void collectActions(Action *out, int &count)
{
	count = 0;
	for (int p = 0; p < static_cast<int>(Page::Count); ++p)
	{
		const PageDef &d = pageDef(g.title, static_cast<Page>(p));
		auto add = [&](Action a) {
			if (a == Action::None || count >= kMaxButtons) return;
			for (int i = 0; i < count; ++i) if (out[i] == a) return;
			out[count++] = a;
		};
		for (int i = 0; i < d.count; ++i) add(d.entries[i].action);
		add(d.footer.action);
	}
}

void applyFonts()
{
	g.fontPrimary = fontFor(ModernTheme::TypeRole::MenuPrimary, g.geo.effectiveScale);
	g.fontSecondary = fontFor(ModernTheme::TypeRole::MenuSecondary, g.geo.effectiveScale);
	g.fontHeading = fontFor(ModernTheme::TypeRole::SectionHeading, g.geo.effectiveScale);
	g.fontHelper = fontFor(ModernTheme::TypeRole::Helper, g.geo.effectiveScale);
	for (int i = 0; i < g.buttonCount; ++i)
		if (g.buttons[i].window) g.buttons[i].window->winSetFont(g.buttons[i].action == Action::Back || g.buttons[i].action == Action::Exit ? g.fontSecondary : g.fontPrimary);
	if (g.heading && g.fontHeading) g.heading->winSetFont(g.fontHeading);
	if (g.footnote && g.fontHelper) g.footnote->winSetFont(g.fontHelper);
}

void build()
{
	if (!g.parent) return;
	const ModernTheme::Variant &v = ModernTheme::variant(g.title);
	const NameKeyType panelId = keyFor("MainMenu.wnd:ModernPanel");
	g.panel = TheWindowManager->winGetWindowFromId(g.parent, panelId);
	g.logo = g.heading = g.footnote = nullptr;
	g.buttonCount = 0;
	if (!g.panel)
	{
		WinInstanceData inst;
		inst.m_id = panelId;
		g.panel = TheWindowManager->winCreate(g.parent, WIN_STATUS_ENABLED, 0, 0, 10, 10, GameWinDefaultSystem, &inst);
		if (!g.panel) return;
		g.panel->winSetDrawFunc(panelDraw);
	}
	g.panel->winSetDrawFunc(panelDraw);
	g.panel->winSetOwner(g.parent);

	// logo (a mapped image, no fill)
	const NameKeyType logoId = keyFor("MainMenu.wnd:ModernLogo");
	g.logo = TheWindowManager->winGetWindowFromId(g.panel, logoId);
	if (!g.logo)
	{
		WinInstanceData inst;
		inst.m_id = logoId;
		g.logo = TheWindowManager->winCreate(g.panel, WIN_STATUS_ENABLED | WIN_STATUS_IMAGE | WIN_STATUS_NO_INPUT, 0, 0, 10, 10, GameWinDefaultSystem, &inst);
		if (g.logo)
		{
			const Image *image = TheMappedImageCollection ? TheMappedImageCollection->findImageByName(v.logoImage) : nullptr;
			g.logo->winSetEnabledImage(0, image);
			g.logo->winSetEnabledColor(0, GameMakeColor(255, 255, 255, 255));
			g.logo->winSetEnabledBorderColor(0, GameMakeColor(255, 255, 255, 0));
		}
	}

	g.heading = TheWindowManager->winGetWindowFromId(g.panel, keyFor("MainMenu.wnd:ModernHeading"));
	if (!g.heading) { g.heading = createText(g.panel, keyFor("MainMenu.wnd:ModernHeading"), fontFor(ModernTheme::TypeRole::SectionHeading, 100), v.palette.accent, true); }
	g.footnote = TheWindowManager->winGetWindowFromId(g.parent, keyFor("MainMenu.wnd:ModernFootnote"));
	if (!g.footnote) { g.footnote = createText(g.parent, keyFor("MainMenu.wnd:ModernFootnote"), fontFor(ModernTheme::TypeRole::Helper, 100), v.palette.textMuted, true); }

	Action actions[kMaxButtons];
	int n = 0;
	collectActions(actions, n);
	for (int i = 0; i < n; ++i)
	{
		GameWindow *b = TheWindowManager->winGetWindowFromId(g.panel, keyFor(buttonName(actions[i]).str()));
		if (!b) b = createButton(g.panel, g.parent, g.buttonDonor, actions[i], fontFor(ModernTheme::TypeRole::MenuPrimary, 100));
		if (b)
		{
			g.buttons[g.buttonCount].action = actions[i];
			g.buttons[g.buttonCount].window = b;
			++g.buttonCount;
		}
	}
	g.built = g.panel && g.logo && g.heading && g.footnote && g.buttonCount > 0;
}

void setLabel(GameWindow *button, const Entry &e)
{
	button->winSetText(TheGameText->FETCH_OR_SUBSTITUTE(e.labelKey, e.fallback));
}

void positionPanel(int xOffset)
{
	if (!g.panel) return;
	g.panel->winSetPosition(g.geo.panel.x + xOffset, g.geo.panel.y);
}

// Recompute the geometry (surface / scale changed or first show) and place the static parts.
void relayout()
{
	LARGE_INTEGER t0, t1, f;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&t0);
	g.surfaceW = TheDisplay->getWidth();
	g.surfaceH = TheDisplay->getHeight();
	g.uiPercent = static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5);
	if (g.uiPercent <= 0) g.uiPercent = 100;
	g.geo = compute(g.surfaceW, g.surfaceH, g.uiPercent, g.title);
	// The retail MainMenuParent is the 800x600 canvas, uniformly scaled and centred (it draws nothing). A child is hit only inside EVERY ancestor's rectangle, so on a wide surface a panel
	// docked outside that canvas could not be clicked: the parent becomes the whole surface (all its retail children are hidden while the Modern shell is active).
	g.parent->winSetPosition(0, 0);
	g.parent->winSetSize(g.surfaceW, g.surfaceH);
	g.panel->winSetSize(g.geo.panel.w, g.geo.panel.h);
	positionPanel(g.sliding ? g.geo.slideFrom : 0);
	g.logo->winSetPosition(g.geo.logo.x, g.geo.logo.y);
	g.logo->winSetSize(g.geo.logo.w, g.geo.logo.h);
	g.heading->winSetPosition(g.geo.heading.x, g.geo.heading.y);
	g.heading->winSetSize(g.geo.heading.w, g.geo.heading.h);
	g.footnote->winSetPosition(g.geo.footnote.x, g.geo.footnote.y);
	g.footnote->winSetSize(g.geo.footnote.w, g.geo.footnote.h);
	applyFonts();
	g.pageDirty = true;
	QueryPerformanceCounter(&t1);
	++g.stats.relayouts;
	g.stats.relayoutMicros += static_cast<unsigned long long>((t1.QuadPart - t0.QuadPart) * 1000000LL / f.QuadPart);
}

void applyPage()
{
	const PageDef &d = pageDef(g.title, g.state.page());
	ModernFocus::Item items[ModernFocus::kMaxItems];
	int itemCount = 0;
	for (int i = 0; i < g.buttonCount; ++i)
	{
		ButtonSlot &b = g.buttons[i];
		int slot = -1;
		const Entry *entry = nullptr;
		bool isFooter = false;
		for (int e = 0; e < d.count; ++e) if (d.entries[e].action == b.action) { slot = e; entry = &d.entries[e]; }
		if (!entry && d.footer.action == b.action) { entry = &d.footer; isFooter = true; }
		if (!entry) { b.window->winHide(TRUE); continue; }
		const Rect &r = isFooter ? g.geo.footer : g.geo.slot[slot];
		b.window->winSetPosition(r.x, r.y);
		b.window->winSetSize(r.w, r.h);
		setLabel(b.window, *entry);
		b.window->winEnable(TRUE);
		b.window->winHide(FALSE);
	}
	// focus order: the entries top to bottom, then the footer (Back / Exit Game)
	for (int e = 0; e < d.count && itemCount < ModernFocus::kMaxItems; ++e)
	{
		ModernFocus::Item it;
		it.id = focusItemId(d.entries[e].action);
		it.rect = { g.geo.slot[e].x, g.geo.slot[e].y, g.geo.slot[e].w, g.geo.slot[e].h };
		ButtonSlot *s = slotFor(d.entries[e].action);
		it.enabled = s && s->window && BitIsSet(s->window->winGetStatus(), WIN_STATUS_ENABLED);
		it.visible = s && s->window && !s->window->winIsHidden();
		items[itemCount++] = it;
	}
	{
		ModernFocus::Item it;
		it.id = focusItemId(d.footer.action);
		it.rect = { g.geo.footer.x, g.geo.footer.y, g.geo.footer.w, g.geo.footer.h };
		ButtonSlot *s = slotFor(d.footer.action);
		it.enabled = s && s->window && BitIsSet(s->window->winGetStatus(), WIN_STATUS_ENABLED);
		it.visible = s && s->window && !s->window->winIsHidden();
		items[itemCount++] = it;
	}
	g.state.nav.ring.setItems(items, itemCount);
	if (!g.state.nav.restoreFocus())
		g.state.nav.ring.focusFirst();
	// the root shows only the logo, centred in the band the heading occupies on the other pages
	{
		const int top = g.geo.logo.y, bottom = g.geo.headingRule.y + g.geo.headingRule.h;
		const int y = g.state.page() == Page::Root ? top + (bottom - top - g.geo.logo.h) / 2 : top;
		g.logo->winSetPosition(g.geo.logo.x, y);
	}
	if (g.state.page() == Page::Root) GadgetStaticTextSetText(g.heading, UnicodeString::TheEmptyString);
	else GadgetStaticTextSetText(g.heading, TheGameText->FETCH_OR_SUBSTITUTE(d.headingKey, d.headingFallback));
	g.heading->winHide(g.state.page() == Page::Root);
	g.pageDirty = false;
	++g.stats.pageChanges;
}

void defaultDifficultyFocus()
{
	OptionPreferences pref;
	const Int diff = pref.getCampaignDifficulty();
	g.state.nav.ring.focusId(focusItemId(diff <= 0 ? Action::DiffEasy : (diff == 1 ? Action::DiffMedium : Action::DiffHard)));
}

void performAction(Action a)
{
	const Effect e = apply(g.state, a);
	switch (e.kind)
	{
	case EffectKind::None:
		return;
	case EffectKind::PageChanged:
		applyPage();
		return;
	case EffectKind::SelectCampaign:
	case EffectKind::SelectChallenge:
		if (g.hooks.perform) g.hooks.perform(e);
		applyPage();
		defaultDifficultyFocus();
		return;
	case EffectKind::CancelSelection:
		if (g.hooks.perform) g.hooks.perform(e);
		applyPage();
		return;
	case EffectKind::StartSelected:
	case EffectKind::Engine:
	{
		const bool leaving = g.hooks.perform ? g.hooks.perform(e) : false;
		if (e.kind == EffectKind::StartSelected && leaving) g.forget = true;   // the game starts: the next menu starts at the root page
		if (!leaving) g.state.busy = false;
		return;
	}
	}
}

} // namespace

namespace ModernMenuShell
{
bool active()
{
	static const bool feature = [] { const char *t = std::getenv("GENERALS_MODERN_FRONTEND"); return !(t && *t == '0'); }();
	return feature && DisplayRuntime::modern() && ModernExperience::enabled(ModernExperience::FEATURE_MODERN_FRONTEND);
}

bool built() { return g.built; }
bool revealed() { return g.revealed; }
const Stats &stats() { return g.stats; }

void init(GameWindow *menuParent, ModernTheme::Title title, const Hooks &hooks)
{
	g.parent = menuParent;
	g.title = title;
	g.hooks = hooks;
	g.buttonDonor = TheWindowManager->winGetWindowFromId(menuParent, keyFor("MainMenu.wnd:ButtonSinglePlayer"));
	if (!g.stateValid || g.state.title != title || g.forget) { g.state.reset(title); g.stateValid = true; g.forget = false; }
	g.state.busy = false;
	build();
	if (!g.built) return;
	// the retail version label text becomes the footnote
	GameWindow *retailVersion = TheWindowManager->winGetWindowFromId(menuParent, keyFor("MainMenu.wnd:LabelVersion"));
	if (retailVersion) GadgetStaticTextSetText(g.footnote, GadgetStaticTextGetText(retailVersion));
	else if (ModernBranding::useModernFooter()) GadgetStaticTextSetText(g.footnote, UnicodeString(ModernBranding::footerText()));
	hideRetailVisuals();
	relayout();
	applyPage();
	g.sliding = false;
	g.panel->winHide(!g.revealed);
	g.footnote->winHide(!g.revealed);
	StartupDiagnostics::checkpoint("modernmenu", "init", title == Title::ZeroHour ? "ZeroHour" : "Generals");
}

void reveal(bool instant)
{
	if (!g.built) return;
	if (g.revealed && !g.sliding) return;
	g.revealed = true;
	g.panel->winHide(FALSE);
	g.footnote->winHide(FALSE);
	if (instant) { g.sliding = false; positionPanel(0); return; }
	g.sliding = true;
	g.slideStart = timeGetTime();
	positionPanel(g.geo.slideFrom);
	if (TheAudio) { AudioEventRTS fade("GUIButtonsFadeIn"); TheAudio->addAudioEvent(&fade); }
}

void update(bool leaving)
{
	if (!g.built) return;
	LARGE_INTEGER t0, t1, f;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&t0);
	if (TheDisplay->getWidth() != g.surfaceW || TheDisplay->getHeight() != g.surfaceH ||
		static_cast<int>(DisplayRuntime::uiScale() * 100.0 + 0.5) != g.uiPercent)
	{
		relayout();
		applyPage();
	}
	if (g.sliding)
	{
		const DWORD elapsed = timeGetTime() - g.slideStart;
		if (elapsed >= static_cast<DWORD>(kSlideMs)) { g.sliding = false; positionPanel(0); }
		else
		{
			const float t = static_cast<float>(elapsed) / kSlideMs;
			const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
			positionPanel(static_cast<int>(g.geo.slideFrom * (1.0f - eased)));
		}
	}
	if (!leaving && g.state.busy) g.state.busy = false;
	QueryPerformanceCounter(&t1);
	const unsigned long long us = static_cast<unsigned long long>((t1.QuadPart - t0.QuadPart) * 1000000LL / f.QuadPart);
	++g.stats.updates;
	g.stats.updateMicros += us;
	if (us > g.stats.worstUpdateMicros) g.stats.worstUpdateMicros = us;
}

bool onKey(int key, int state)
{
	if (!g.built || !g.revealed || !(state & KEY_STATE_DOWN)) return false;
	ModernFocus::Ring &ring = g.state.nav.ring;
	switch (key)
	{
	case KEY_DOWN:  return ring.move(Direction::Down);
	case KEY_UP:    return ring.move(Direction::Up);
	case KEY_LEFT:  return ring.move(Direction::Left);
	case KEY_RIGHT: return ring.move(Direction::Right);
	case KEY_TAB:   return (state & KEY_STATE_SHIFT) ? ring.previous() : ring.next();
	case KEY_ENTER:
	case KEY_KPENTER:
	{
		const int id = ring.activate();
		if (id == ModernFocus::kNone) return true;
		playClick();
		performAction(static_cast<Action>(id));
		return true;
	}
	case KEY_ESC:
		if (!g.state.nav.atRoot()) { playClick(); performAction(Action::Back); }
		return true;
	default: return false;
	}
}

bool onSelected(GameWindow *control)
{
	ButtonSlot *s = slotFor(control);
	if (!s) return false;
	g.state.nav.ring.focusId(focusItemId(s->action));
	performAction(s->action);
	return true;
}

bool onHover(GameWindow *control)
{
	ButtonSlot *s = slotFor(control);
	if (!s) return false;
	g.state.nav.ring.focusId(focusItemId(s->action));
	return true;
}

bool performRetail(GameWindow *retail)
{
	if (!g.built || !retail || !retail->winGetInstanceData()) return false;
	const char *name = retail->winGetInstanceData()->m_decoratedNameString.str();
	if (!name || strncmp(name, "MainMenu.wnd:", 13) != 0) return false;
	name += 13;
	struct Map { const char *retail; Action action; };
	static const Map map[] = {
		{ "ButtonSinglePlayer", Action::GoSoloPlay }, { "ButtonMultiplayer", Action::GoMultiplayer }, { "ButtonLoadReplay", Action::GoLoad },
		{ "ButtonOptions", Action::Options }, { "ButtonCredits", Action::Credits }, { "ButtonExit", Action::Exit },
		{ "ButtonSkirmish", Action::Skirmish }, { "ButtonChallenge", Action::Challenge }, { "ButtonTRAINING", Action::Training },
		{ "ButtonUSA", Action::CampaignUSA }, { "ButtonGLA", Action::CampaignGLA }, { "ButtonChina", Action::CampaignChina },
		{ "ButtonEasy", Action::DiffEasy }, { "ButtonMedium", Action::DiffMedium }, { "ButtonHard", Action::DiffHard },
		{ "ButtonNetwork", Action::Network }, { "ButtonOnline", Action::Online }, { "ButtonLoadGame", Action::LoadGame }, { "ButtonReplay", Action::LoadReplay },
		{ "ButtonSingleBack", Action::Back }, { "ButtonMultiBack", Action::Back }, { "ButtonLoadReplayBack", Action::Back }, { "ButtonDiffBack", Action::Back },
	};
	for (const Map &m : map)
	{
		if (strcmp(name, m.retail) != 0) continue;
		// a campaign / difficulty press from automation must start at the page that offers it, exactly like the retail single player menu did
		if ((m.action == Action::CampaignUSA || m.action == Action::CampaignGLA || m.action == Action::CampaignChina || m.action == Action::Training) && g.state.page() == Page::Root)
			performAction(Action::GoSoloPlay);
		performAction(m.action);
		return true;
	}
	return false;
}

void forgetNavigation() { g.state.reset(g.title); g.stateValid = true; }

ModernTheme::Title title() { return g.title; }
int visibleButtonCount()
{
	int n = 0;
	for (int i = 0; i < g.buttonCount; ++i) if (g.buttons[i].window && !g.buttons[i].window->winIsHidden()) ++n;
	return n;
}
GameWindow *buttonWindow(ModernMainMenu::Action action) { ButtonSlot *s = slotFor(action); return s ? s->window : nullptr; }
GameWindow *panelWindow() { return g.panel; }
ModernMainMenu::Page currentPage() { return g.state.page(); }
const ModernMainMenu::Geometry &geometry() { return g.geo; }
int focusedActionId() { return g.state.nav.ring.focusedId(); }
} // namespace ModernMenuShell
