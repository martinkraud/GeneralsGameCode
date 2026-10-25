// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience main menu binding layer (UI.2, visual system revised by the Modern Frontend milestone). See GameClient/ModernMenuShell.h for ownership, lifetime and behaviour.
#include "PreRTS.h"

#include "GameClient/ModernMenuShell.h"

#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
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
#include "GameClient/ModernDraw.h"
#include "GameClient/WinInstanceData.h"

namespace
{
using namespace ModernMainMenu;
using ModernFocus::Direction;
using ModernTheme::TypeRole;

constexpr int kMaxButtons = 24;
constexpr int kSlideMs = 240;

struct ButtonSlot
{
	Action action = Action::None;
	GameWindow *window = nullptr;
};

struct Shell
{
	bool built = false;
	bool revealed = false;
	GameWindow *parent = nullptr, *panel = nullptr, *logo = nullptr;
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
	wchar_t footnote[96] = {};
	ModernDraw::Decor decor;
	ModernMenuShell::Stats stats;
} g;

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

UnicodeString upper(const UnicodeString &s)
{
	UnicodeString u;
	for (const WideChar *p = s.str(); p && *p; ++p) u.concat(static_cast<WideChar>(towupper(*p)));
	return u;
}

ModernComponents::Kind kindOf(Action a)
{
	if (a == Action::Back) return ModernComponents::Kind::NavMinor;
	if (a == Action::Exit) return ModernComponents::Kind::NavDanger;
	return ModernComponents::Kind::Nav;
}

TypeRole roleOf(Action a) { return (a == Action::Back || a == Action::Exit) ? TypeRole::MenuSecondary : TypeRole::MenuPrimary; }

GameWindow *createButton(GameWindow *parent, GameWindow *owner, Action a)
{
	WinInstanceData inst;
	inst.m_font = ModernDraw::font(roleOf(a));
	inst.m_id = keyFor(buttonName(a).str());
	inst.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
	GameWindow *b = TheWindowManager->gogoGadgetPushButton(parent, WIN_STATUS_ENABLED, 0, 0, 10, 10, &inst, inst.m_font, FALSE);
	if (!b) return nullptr;
	ModernDraw::skinButton(b, kindOf(a), roleOf(a));
	b->winSetOwner(owner);   // the gadget sends its click / hover to the retail main menu callback
	return b;
}

void hideRetailVisuals()
{
	for (GameWindow *c = g.parent->winGetChild(); c; c = c->winGetNext())
		if (c != g.panel)
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

void build()
{
	if (!g.parent) return;
	ModernDraw::setTitle(g.title);
	const ModernTheme::Variant &v = ModernTheme::variant(g.title);
	ModernDraw::clearRegistry();
	g.decor.clear();
	g.logo = nullptr;
	g.buttonCount = 0;
	// the band: one window below its children that draws the chrome (the decor); it takes input so the whole band is a click target
	g.panel = ModernDraw::createBackdrop(g.parent, "MainMenu.wnd:ModernPanel", &g.decor, true);
	if (!g.panel) return;
	g.panel->winSetOwner(g.parent);

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

	Action actions[kMaxButtons];
	int n = 0;
	collectActions(actions, n);
	for (int i = 0; i < n; ++i)
	{
		GameWindow *b = TheWindowManager->winGetWindowFromId(g.panel, keyFor(buttonName(actions[i]).str()));
		if (b) ModernDraw::skinButton(b, kindOf(actions[i]), roleOf(actions[i]));
		else b = createButton(g.panel, g.parent, actions[i]);
		if (b)
		{
			g.buttons[g.buttonCount].action = actions[i];
			g.buttons[g.buttonCount].window = b;
			++g.buttonCount;
		}
	}
	g.built = g.panel && g.logo && g.buttonCount > 0;
}

void setLabel(GameWindow *button, const Entry &e)
{
	button->winSetText(upper(TheGameText->FETCH_OR_SUBSTITUTE(e.labelKey, e.fallback)));   // the navigation roles are set in capitals
}

void positionPanel(int xOffset)
{
	if (!g.panel) return;
	g.panel->winSetPosition(g.geo.panel.x + xOffset, g.geo.panel.y);
}

// The static chrome of the band: scrim, edge hairline, accent block, page heading, rules, footnote. Coordinates are relative to the band window.
void rebuildDecor()
{
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	const ModernTheme::Palette &p = ModernTheme::variant(g.title).palette;
	const PageDef &d = pageDef(g.title, g.state.page());
	const bool root = g.state.page() == Page::Root;
	const Rect &pn = g.geo.panel;
	g.decor.clear();

	ModernDraw::DecorItem &scrim = g.decor.add(ModernDraw::DecorKind::Scrim);
	scrim.x = 0; scrim.y = 0; scrim.w = pn.w; scrim.h = pn.h;

	const int edge = ModernDraw::px(2);
	ModernDraw::DecorItem &line = g.decor.add(ModernDraw::DecorKind::Rule);
	line.x = pn.w - edge; line.y = 0; line.w = edge; line.h = pn.h; line.color = p.hairline;
	ModernDraw::DecorItem &tick = g.decor.add(ModernDraw::DecorKind::Tick);
	tick.w = ModernDraw::px(m.accentTick) + edge; tick.x = pn.w - tick.w; tick.y = g.geo.stackTop; tick.h = g.geo.stackBottom - g.geo.stackTop;

	if (!root)
	{
		ModernDraw::DecorItem &head = g.decor.add(ModernDraw::DecorKind::Text);
		head.x = g.geo.heading.x; head.y = g.geo.heading.y; head.w = g.geo.heading.w; head.h = g.geo.heading.h;
		head.role = TypeRole::MajorHeading; head.color = p.accent;
		const UnicodeString t = upper(TheGameText->FETCH_OR_SUBSTITUTE(d.headingKey, d.headingFallback));
		wcsncpy(head.text, t.str(), 95);
		ModernDraw::DecorItem &rule = g.decor.add(ModernDraw::DecorKind::Rule);
		rule.x = g.geo.headingRule.x; rule.y = g.geo.headingRule.y; rule.w = g.geo.headingRule.w * 38 / 100; rule.h = g.geo.headingRule.h; rule.color = p.accent;
		ModernDraw::DecorItem &rest = g.decor.add(ModernDraw::DecorKind::Rule);
		rest.x = rule.x + rule.w; rest.y = g.geo.headingRule.y; rest.w = g.geo.headingRule.w - rule.w; rest.h = g.geo.headingRule.h; rest.color = p.hairline;
	}
	ModernDraw::DecorItem &foot = g.decor.add(ModernDraw::DecorKind::Rule);
	foot.x = g.geo.footerRule.x; foot.y = g.geo.footerRule.y; foot.w = g.geo.footerRule.w; foot.h = g.geo.footerRule.h; foot.color = p.hairline;

	ModernDraw::DecorItem &note = g.decor.add(ModernDraw::DecorKind::Text);
	note.x = g.geo.footnote.x - pn.x; note.y = g.geo.footnote.y - pn.y; note.w = g.geo.footnote.w; note.h = g.geo.footnote.h;
	note.role = TypeRole::Helper; note.color = p.textMuted;
	wcsncpy(note.text, g.footnote, 95);
	g.decor.refreshFonts();
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
	ModernDraw::setScalePercent(g.geo.effectiveScale);
	// The retail MainMenuParent is the 800x600 canvas, uniformly scaled and centred (it draws nothing). A child is hit only inside EVERY ancestor's rectangle, so on a wide surface a band
	// docked outside that canvas could not be clicked: the parent becomes the whole surface (all its retail children are hidden while the Modern shell is active).
	g.parent->winSetPosition(0, 0);
	g.parent->winSetSize(g.surfaceW, g.surfaceH);
	g.panel->winSetSize(g.geo.panel.w, g.geo.panel.h);
	positionPanel(g.sliding ? g.geo.slideFrom : 0);
	g.logo->winSetPosition(g.geo.logo.x, g.geo.logo.y);
	g.logo->winSetSize(g.geo.logo.w, g.geo.logo.h);
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
	rebuildDecor();
	++g.stats.pageChanges;
}

void syncFocusWindow()
{
	ButtonSlot *s = g.state.nav.ring.hasFocus() ? slotFor(static_cast<Action>(g.state.nav.ring.focusedId())) : nullptr;
	ModernDraw::setFocusWindow(s ? s->window : nullptr);
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
		syncFocusWindow();
		return;
	case EffectKind::SelectCampaign:
	case EffectKind::SelectChallenge:
		if (g.hooks.perform) g.hooks.perform(e);
		applyPage();
		defaultDifficultyFocus();
		syncFocusWindow();
		return;
	case EffectKind::CancelSelection:
		if (g.hooks.perform) g.hooks.perform(e);
		applyPage();
		syncFocusWindow();
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
	if (!g.stateValid || g.state.title != title || g.forget) { g.state.reset(title); g.stateValid = true; g.forget = false; }
	g.state.busy = false;
	build();
	if (!g.built) return;
	// the retail version label text becomes the footnote
	GameWindow *retailVersion = TheWindowManager->winGetWindowFromId(menuParent, keyFor("MainMenu.wnd:LabelVersion"));
	UnicodeString note = retailVersion ? GadgetStaticTextGetText(retailVersion) : UnicodeString(ModernBranding::footerText());
	wcsncpy(g.footnote, note.str(), 95);
	hideRetailVisuals();
	relayout();
	applyPage();
	syncFocusWindow();
	g.sliding = false;
	g.panel->winHide(!g.revealed);
	StartupDiagnostics::checkpoint("modernmenu", "init", title == Title::ZeroHour ? "ZeroHour" : "Generals");
	char face[160];
	sprintf_s(face, "nav=%s heading=%s body=%s", ModernDraw::resolvedFamily(TypeRole::MenuPrimary), ModernDraw::resolvedFamily(TypeRole::MajorHeading), ModernDraw::resolvedFamily(TypeRole::Body));
	StartupDiagnostics::checkpoint("modernmenu", "fonts", face);
}

void reveal(bool instant)
{
	if (!g.built) return;
	if (g.revealed && !g.sliding) return;
	g.revealed = true;
	g.panel->winHide(FALSE);
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
		syncFocusWindow();
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
	bool moved = false;
	switch (key)
	{
	case KEY_DOWN:  moved = ring.move(Direction::Down); break;
	case KEY_UP:    moved = ring.move(Direction::Up); break;
	case KEY_LEFT:  moved = ring.move(Direction::Left); break;
	case KEY_RIGHT: moved = ring.move(Direction::Right); break;
	case KEY_TAB:   moved = (state & KEY_STATE_SHIFT) ? ring.previous() : ring.next(); break;
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
	if (moved) syncFocusWindow();
	return moved;
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
	syncFocusWindow();
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
