// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience drawing / skinning layer. See GameClient/ModernDraw.h.
#include "PreRTS.h"

#include "GameClient/ModernDraw.h"

#include <cstring>
#include <vector>
#include <windows.h>

#include "Common/NameKeyGenerator.h"
#include "Common/StartupDiagnostics.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetCheckBox.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetSlider.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/WinInstanceData.h"

namespace ModernDraw
{
namespace
{
using ModernComponents::Flags;
using ModernComponents::Span;
using ModernComponents::State;
using ModernTheme::Color;

ModernTheme::Title g_title = ModernTheme::Title::Generals;
Stats g_stats;
int g_scale = 100;                              // the scale of the window being drawn (set by activate() at the start of every draw callback)
int g_scales[static_cast<int>(Screen::Count)] = { 100, 100, 100, 100, 100, 100 };   // the effective scale of each screen
Screen g_owner = Screen::Main;                  // the screen that is building / laying out / skinning right now
GameWindow *g_focus = nullptr;
char g_family[static_cast<int>(TypeRole::Count)][32] = {};
bool g_resolved[static_cast<int>(TypeRole::Count)] = {};

struct Entry
{
	GameWindow *window = nullptr;
	Kind kind = Kind::Secondary;
	TypeRole role = TypeRole::MenuSecondary;
	bool selected = false;
	char arrow = 0;          // 'u' / 'd' chevron button, 0 = a text button
	Screen owner = Screen::Main;
};
std::vector<Entry> g_entries;

UnsignedInt argb(const Color &c) { return GameMakeColor(c.r, c.g, c.b, c.a); }

Entry *find(GameWindow *w)
{
	for (Entry &e : g_entries) if (e.window == w) return &e;
	return nullptr;
}

// draw-time scale helpers: a draw callback runs for any screen at any time, so it first adopts the scale of the screen that owns its window
int dpx(int ref) { return (ref * g_scale + 50) / 100; }
void activate(GameWindow *w)
{
	if (const Entry *e = find(w)) g_scale = g_scales[static_cast<int>(e->owner)];
}

Entry &entryFor(GameWindow *w)
{
	if (Entry *e = find(w)) return *e;
	g_entries.push_back(Entry());
	g_entries.back().window = w;
	g_entries.back().owner = g_owner;
	return g_entries.back();
}

void line(int x1, int y1, int x2, int y2, int width, const Color &c)
{
	if (c.a == 0) return;
	TheDisplay->drawLine(x1, y1, x2, y2, static_cast<Real>(width), argb(c));
}

void fillRect(int x, int y, int w, int h, const Color &c)
{
	if (c.a == 0 || w <= 0 || h <= 0) return;
	TheDisplay->drawFillRect(x, y, w, h, argb(c));
}

Flags flagsOf(GameWindow *w, WinInstanceData *inst, const Entry *e, bool pressedFromSelected)
{
	Flags f;
	f.enabled = BitIsSet(w->winGetStatus(), WIN_STATUS_ENABLED) != 0;
	f.hover = BitIsSet(inst->getState(), WIN_STATE_HILITED) != 0;
	const bool stateSelected = BitIsSet(inst->getState(), WIN_STATE_SELECTED) != 0;
	f.pressed = pressedFromSelected && stateSelected && f.hover;
	f.focus = g_focus == w || (TheWindowManager && TheWindowManager->winGetFocus() == w);
	f.selected = (e && e->selected) || (!pressedFromSelected && stateSelected);
	return f;
}

void drawText(GameWindow *w, WinInstanceData *inst, const Color &color, int align, int inset, const Color &shadow)
{
	DisplayString *text = inst->getTextDisplayString();
	if (!text || text->getTextLength() == 0) return;
	int x = 0, y = 0, sw = 0, sh = 0, tw = 0, th = 0;
	w->winGetScreenPosition(&x, &y);
	w->winGetSize(&sw, &sh);
	if (text->getFont() != w->winGetFont()) text->setFont(w->winGetFont());
	text->setWordWrap(0);
	text->getSize(&tw, &th);
	int tx = x + inset;
	if (align == 1) tx = x + (sw - tw) / 2;
	else if (align == 2) tx = x + sw - tw - inset;
	text->draw(tx, y + (sh - th) / 2, argb(color), argb(shadow));
}

// ---- buttons ----
void drawButton(GameWindow *w, WinInstanceData *inst, const Entry *e);

void buttonDraw(GameWindow *w, WinInstanceData *inst)
{
	activate(w);
	LARGE_INTEGER t0, t1, freq;
	QueryPerformanceCounter(&t0);
	drawButton(w, inst, find(w));
	QueryPerformanceCounter(&t1);
	QueryPerformanceFrequency(&freq);
	++g_stats.buttonDraws;
	g_stats.buttonMicros += static_cast<unsigned long long>((t1.QuadPart - t0.QuadPart) * 1000000LL / freq.QuadPart);
}

// Dialog buttons are not in the registry (a dialog is created and destroyed independently of the screen below it): the component kind rides in the gadget's data pointer.
void dialogButtonDraw(GameWindow *w, WinInstanceData *inst)
{
	g_scale = g_scales[static_cast<int>(Screen::Dialog)];
	Entry e;
	e.window = w;
	e.kind = static_cast<Kind>(reinterpret_cast<intptr_t>(GadgetButtonGetData(w)));
	e.role = TypeRole::MenuSecondary;
	drawButton(w, inst, &e);
}

void drawButton(GameWindow *w, WinInstanceData *inst, const Entry *e)
{
	if (!e) return;
	const ModernTheme::Palette &p = ModernTheme::variant(g_title).palette;
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	int x = 0, y = 0, sw = 0, sh = 0;
	w->winGetScreenPosition(&x, &y);
	w->winGetSize(&sw, &sh);
	const Flags f = flagsOf(w, inst, e, true);
	const State st = ModernComponents::resolve(f);
	const ModernComponents::Style s = ModernComponents::style(e->kind, st, p);
	const int cut = dpx(m.chamferSmall);
	const bool nav = e->kind == Kind::Nav || e->kind == Kind::NavMinor || e->kind == Kind::NavDanger;
	const bool tab = e->kind == Kind::Tab;

	if (e->arrow)
	{
		// chevron button (combo drop-down, list scroll): a hairline field edge + a chevron
		const bool hot = f.enabled && (f.hover || f.focus);
		const Color c = !f.enabled ? p.textDisabled : (hot ? p.accent : p.textSecondary);
		const int cx = x + sw / 2, cy = y + sh / 2, a = (sw < sh ? sw : sh) / 5 + 1;
		if (e->arrow == 'd') { line(cx - a, cy - a / 2, cx, cy + a / 2, 2, c); line(cx, cy + a / 2, cx + a, cy - a / 2, 2, c); }
		else                 { line(cx - a, cy + a / 2, cx, cy - a / 2, 2, c); line(cx, cy - a / 2, cx + a, cy + a / 2, 2, c); }
		return;
	}

	int tcut = cut;
	if (nav) { tcut = dpx(m.chamferSmall); }
	if (s.fill.a)
	{
		if (nav || tab) fillChamfer(x, y, sw, sh, 0, tcut, 0, tcut, s.fill);
		else fillChamfer(x, y, sw, sh, 0, s.chamfer ? cut : 0, 0, s.chamfer ? cut : 0, s.fill);
	}
	if (s.edge.a && s.edgeWidth)
	{
		const int ew = s.edgeWidth * (g_scale >= 200 ? 2 : 1);
		strokeChamfer(x, y, sw, sh, 0, s.chamfer ? cut : 0, 0, s.chamfer ? cut : 0, ew, s.edge);
	}
	if (s.tick.a)
	{
		const int tw = dpx(m.accentTick);
		fillRect(x, y, tw, sh, s.tick);
	}
	const Color shadow = { 0, 0, 0, 150 };
	if (nav || tab || e->kind == Kind::Row)
		drawText(w, inst, s.text, 0, dpx(m.accentTick) + dpx(nav ? 16 : 12), shadow);
	else
		drawText(w, inst, s.text, 1, 0, shadow);
}

// ---- check box ----
void checkDraw(GameWindow *w, WinInstanceData *inst)
{
	activate(w);
	const ModernTheme::Palette &p = ModernTheme::variant(g_title).palette;
	int x = 0, y = 0, sw = 0, sh = 0;
	w->winGetScreenPosition(&x, &y);
	w->winGetSize(&sw, &sh);
	Flags f;
	f.enabled = BitIsSet(w->winGetStatus(), WIN_STATUS_ENABLED) != 0;
	f.hover = BitIsSet(inst->getState(), WIN_STATE_HILITED) != 0;
	f.focus = g_focus == w;
	const bool checked = BitIsSet(inst->getState(), WIN_STATE_SELECTED) != 0;
	const int box = (sh * 7) / 10 < dpx(26) ? (sh * 7) / 10 : dpx(26);
	const int by = y + (sh - box) / 2;
	const Color edge = !f.enabled ? p.hairline : (f.focus ? p.focus : (f.hover ? p.accent : p.edge));
	fillRect(x, by, box, box, f.enabled ? p.field : p.field.withAlpha(110));
	strokeChamfer(x, by, box, box, 0, 0, 0, 0, g_scale >= 200 ? 2 : 1, edge);
	if (checked)
	{
		const int in = box / 5 + 1;
		fillRect(x + in, by + in, box - 2 * in, box - 2 * in, f.enabled ? p.accent : p.textDisabled);
	}
	const Color shadow = { 0, 0, 0, 150 };
	drawText(w, inst, f.enabled ? (f.hover ? p.text : p.textSecondary) : p.textDisabled, 0, box + dpx(10), shadow);
}

// ---- horizontal slider track (the thumb is a child push button skinned as a handle) ----
void sliderDraw(GameWindow *w, WinInstanceData *inst)
{
	activate(w);
	const ModernTheme::Palette &p = ModernTheme::variant(g_title).palette;
	int x = 0, y = 0, sw = 0, sh = 0;
	w->winGetScreenPosition(&x, &y);
	w->winGetSize(&sw, &sh);
	const bool enabled = BitIsSet(w->winGetStatus(), WIN_STATUS_ENABLED) != 0;
	const int th = dpx(4), ty = y + (sh - th) / 2;
	fillRect(x, ty, sw, th, enabled ? p.hairline : p.hairline.withAlpha(90));
	Int mn = 0, mx = 0;
	GadgetSliderGetMinMax(w, &mn, &mx);
	const Int pos = GadgetSliderGetPosition(w);
	if (mx > mn && enabled)
	{
		const int filled = static_cast<int>((static_cast<long long>(pos - mn) * sw) / (mx - mn));
		fillRect(x, ty, filled < 0 ? 0 : (filled > sw ? sw : filled), th, p.accent);
	}
	(void)inst;
}

void thumbDraw(GameWindow *w, WinInstanceData *inst)
{
	activate(w);
	const ModernTheme::Palette &p = ModernTheme::variant(g_title).palette;
	int x = 0, y = 0, sw = 0, sh = 0;
	w->winGetScreenPosition(&x, &y);
	w->winGetSize(&sw, &sh);
	const bool enabled = BitIsSet(w->winGetStatus(), WIN_STATUS_ENABLED) != 0;
	const bool hot = BitIsSet(inst->getState(), WIN_STATE_HILITED) != 0 || BitIsSet(inst->getState(), WIN_STATE_SELECTED) != 0;
	const int tw = dpx(10) < sw ? dpx(10) : sw;
	const int tx = x + (sw - tw) / 2;
	fillChamfer(tx, y, tw, sh, 0, dpx(3), 0, dpx(3), !enabled ? p.textDisabled : (hot ? p.text : p.accent));
}

void verticalTrackDraw(GameWindow *w, WinInstanceData *inst)
{
	activate(w);
	const ModernTheme::Palette &p = ModernTheme::variant(g_title).palette;
	int x = 0, y = 0, sw = 0, sh = 0;
	w->winGetScreenPosition(&x, &y);
	w->winGetSize(&sw, &sh);
	fillRect(x + sw / 2 - dpx(2), y, dpx(4), sh, p.hairline);
	(void)inst;
}

void verticalThumbDraw(GameWindow *w, WinInstanceData *inst)
{
	activate(w);
	const ModernTheme::Palette &p = ModernTheme::variant(g_title).palette;
	int x = 0, y = 0, sw = 0, sh = 0;
	w->winGetScreenPosition(&x, &y);
	w->winGetSize(&sw, &sh);
	const bool hot = BitIsSet(inst->getState(), WIN_STATE_HILITED) != 0 || BitIsSet(inst->getState(), WIN_STATE_SELECTED) != 0;
	const int tw = dpx(6) < sw ? dpx(6) : sw;
	fillChamfer(x + (sw - tw) / 2, y, tw, sh, 0, dpx(2), 0, dpx(2), hot ? p.text : p.accent);
}

void applyTextColors(GameWindow *w, const ModernTheme::Palette &p)
{
	w->winSetEnabledTextColors(argb(p.text), argb(Color{ 0, 0, 0, 150 }));
	w->winSetHiliteTextColors(argb(p.text), argb(Color{ 0, 0, 0, 150 }));
	w->winSetDisabledTextColors(argb(p.textDisabled), argb(Color{ 0, 0, 0, 100 }));
}

void skinChildTree(GameWindow *w);
}   // namespace

// ---- theme state ----
void setTitle(ModernTheme::Title title) { g_title = title; }
ModernTheme::Title title() { return g_title; }
const Stats &stats() { return g_stats; }
void resetStats() { g_stats = Stats(); }
const ModernTheme::Palette &palette() { return ModernTheme::variant(g_title).palette; }
int scalePercent() { return g_scales[static_cast<int>(g_owner)]; }
int px(int ref) { return (ref * g_scales[static_cast<int>(g_owner)] + 50) / 100; }
void beginScreen(Screen screen) { g_owner = screen; g_scale = g_scales[static_cast<int>(screen)]; }

bool familyInstalled(const char *family)
{
	if (!family || !*family) return false;
	HDC dc = GetDC(nullptr);
	if (!dc) return false;
	LOGFONTA lf = {};
	lf.lfCharSet = DEFAULT_CHARSET;
	strncpy(lf.lfFaceName, family, LF_FACESIZE - 1);
	bool found = false;
	EnumFontFamiliesExA(dc, &lf, [](const LOGFONTA *, const TEXTMETRICA *, DWORD, LPARAM p) -> int { *reinterpret_cast<bool *>(p) = true; return 0; },
		reinterpret_cast<LPARAM>(&found), 0);
	ReleaseDC(nullptr, dc);
	return found;
}

const char *resolvedFamily(TypeRole role)
{
	const int i = static_cast<int>(role);
	if (!g_resolved[i])
	{
		const ModernTheme::TypeStyle &t = ModernTheme::type(role);
		const char *chosen = t.families[ModernTheme::kFontCandidates - 1];
		for (int c = 0; c < ModernTheme::kFontCandidates; ++c)
			if (t.families[c] && familyInstalled(t.families[c])) { chosen = t.families[c]; break; }
		strncpy(g_family[i], chosen ? chosen : "Arial", sizeof(g_family[i]) - 1);
		g_resolved[i] = true;
	}
	return g_family[i];
}

GameFont *font(TypeRole role) { return fontAt(role, g_scales[static_cast<int>(g_owner)]); }

GameFont *fontAt(TypeRole role, int scalePercent)
{
	if (!TheFontLibrary) return nullptr;
	const ModernTheme::TypeStyle &t = ModernTheme::type(role);
	int pt = (t.referenceSize * (scalePercent > 0 ? scalePercent : 100) + 50) / 100;
	if (pt < 7) pt = 7;
	return TheFontLibrary->getFont(AsciiString(resolvedFamily(role)), pt, t.bold);
}

// ---- primitives ----
void fillChamfer(int x, int y, int w, int h, int cutTL, int cutTR, int cutBR, int cutBL, const Color &c)
{
	if (c.a == 0 || w <= 0 || h <= 0 || !TheDisplay) return;
	Span spans[ModernComponents::kMaxSpans];
	const int n = ModernComponents::chamferSpans(w, h, cutTL, cutTR, cutBR, cutBL, spans, ModernComponents::kMaxSpans);
	for (int i = 0; i < n; ++i)
		TheDisplay->drawFillRect(x + spans[i].x, y + spans[i].y, spans[i].w, spans[i].h, argb(c));
}

void strokeChamfer(int x, int y, int w, int h, int cutTL, int cutTR, int cutBR, int cutBL, int width, const Color &c)
{
	if (c.a == 0 || w <= 1 || h <= 1 || !TheDisplay) return;
	const int lim = (w < h ? w : h) / 2;
	auto cl = [&](int v) { return v < 0 ? 0 : (v > lim ? lim : v); };
	cutTL = cl(cutTL); cutTR = cl(cutTR); cutBR = cl(cutBR); cutBL = cl(cutBL);
	const int r = x + w - 1, b = y + h - 1, half = width / 2;
	line(x + cutTL, y + half, r - cutTR, y + half, width, c);                 // top
	line(r - half, y + cutTR, r - half, b - cutBR, width, c);                 // right
	line(x + cutBL, b - half, r - cutBR, b - half, width, c);                 // bottom
	line(x + half, y + cutTL, x + half, b - cutBL, width, c);                 // left
	if (cutTR) line(r - cutTR, y + half, r - half, y + cutTR, width, c);
	if (cutBR) line(r - half, b - cutBR, r - cutBR, b - half, width, c);
	if (cutBL) line(x + cutBL, b - half, x + half, b - cutBL, width, c);
	if (cutTL) line(x + half, y + cutTL, x + cutTL, y + half, width, c);
}

void bracket(int x, int y, int w, int h, int arm, int width, const Color &c)
{
	line(x, y, x + arm, y, width, c);                 line(x, y, x, y + arm, width, c);
	line(x + w - 1, y, x + w - 1 - arm, y, width, c); line(x + w - 1, y, x + w - 1, y + arm, width, c);
	line(x, y + h - 1, x + arm, y + h - 1, width, c); line(x, y + h - 1, x, y + h - 1 - arm, width, c);
	line(x + w - 1, y + h - 1, x + w - 1 - arm, y + h - 1, width, c); line(x + w - 1, y + h - 1, x + w - 1, y + h - 1 - arm, width, c);
}

// ---- skinning ----
void setScalePercent(int percent)
{
	if (percent <= 0) percent = 100;
	g_scales[static_cast<int>(g_owner)] = percent;
	g_scale = percent;
	for (Entry &e : g_entries)
		if (e.window && e.owner == g_owner) e.window->winSetFont(font(e.role));
}

void skinButton(GameWindow *window, Kind kind, TypeRole role)
{
	if (!window) return;
	Entry &e = entryFor(window);
	e.kind = kind;
	e.role = role;
	e.arrow = 0;
	window->winSetDrawFunc(buttonDraw);
	window->winClearStatus(WIN_STATUS_IMAGE);
	window->winSetFont(font(role));
	applyTextColors(window, palette());
}

void skinDialogButton(GameWindow *window, Kind kind, TypeRole role, int scalePercent)
{
	if (!window) return;
	g_scales[static_cast<int>(Screen::Dialog)] = scalePercent > 0 ? scalePercent : 100;   // the dialog's own scale for the draw callback (px() of the layout scale is not used)
	GadgetButtonSetData(window, reinterpret_cast<void *>(static_cast<intptr_t>(kind)));
	window->winSetDrawFunc(dialogButtonDraw);
	window->winClearStatus(WIN_STATUS_IMAGE);
	window->winSetFont(fontAt(role, scalePercent));
	applyTextColors(window, palette());
}

void skinText(GameWindow *window, TypeRole role, const Color &color)
{
	if (!window) return;
	window->winSetFont(font(role));
	window->winSetEnabledTextColors(argb(color), argb(Color{ 0, 0, 0, 150 }));
	window->winSetDisabledTextColors(argb(palette().textDisabled), argb(Color{ 0, 0, 0, 90 }));
	window->winSetHiliteTextColors(argb(color), argb(Color{ 0, 0, 0, 150 }));
	Entry &e = entryFor(window);
	e.role = role;
}

void skinCheck(GameWindow *window)
{
	if (!window) return;
	Entry &e = entryFor(window);
	e.role = TypeRole::Label;
	window->winSetDrawFunc(checkDraw);
	window->winClearStatus(WIN_STATUS_IMAGE);
	window->winSetFont(font(TypeRole::Label));
	applyTextColors(window, palette());
}

void skinSlider(GameWindow *window)
{
	if (!window) return;
	entryFor(window).role = TypeRole::Value;
	window->winSetDrawFunc(sliderDraw);
	window->winClearStatus(WIN_STATUS_IMAGE);
	if (GameWindow *thumb = GadgetSliderGetThumb(window))
	{
		Entry &e = entryFor(thumb);
		e.kind = Kind::Secondary;
		thumb->winSetDrawFunc(thumbDraw);
		thumb->winClearStatus(WIN_STATUS_IMAGE);
	}
}

namespace
{
void skinVerticalSlider(GameWindow *slider)
{
	if (!slider) return;
	entryFor(slider).role = TypeRole::Value;
	slider->winSetDrawFunc(verticalTrackDraw);
	slider->winClearStatus(WIN_STATUS_IMAGE);
	if (GameWindow *thumb = GadgetSliderGetThumb(slider))
	{
		entryFor(thumb).role = TypeRole::Value;
		thumb->winSetDrawFunc(verticalThumbDraw);
		thumb->winClearStatus(WIN_STATUS_IMAGE);
	}
}

void arrowButton(GameWindow *b, char dir)
{
	if (!b) return;
	Entry &e = entryFor(b);
	e.kind = Kind::Secondary;
	e.arrow = dir;
	b->winSetDrawFunc(buttonDraw);
	b->winClearStatus(WIN_STATUS_IMAGE);
}

void fieldColors(const ModernTheme::Palette &p, Color &fill, Color &edge, Color &sel, Color &selEdge)
{
	fill = p.field; edge = p.hairline; sel = p.accentSoft.withAlpha(110); selEdge = p.accent;
}
}   // namespace

void skinList(GameWindow *window)
{
	if (!window) return;
	const ModernTheme::Palette &p = palette();
	Color fill, edge, sel, selEdge;
	fieldColors(p, fill, edge, sel, selEdge);
	window->winSetDrawFunc(TheWindowManager->getListBoxDrawFunc());
	GadgetListBoxSetColors(window, argb(fill), argb(edge), argb(sel), argb(selEdge),
		argb(fill.withAlpha(120)), argb(p.hairline), argb(sel), argb(selEdge),
		argb(fill), argb(p.edge), argb(sel), argb(selEdge));
	window->winSetEnabledTextColors(argb(p.text), argb(Color{ 0, 0, 0, 120 }));
	window->winSetHiliteTextColors(argb(p.text), argb(Color{ 0, 0, 0, 120 }));
	window->winSetDisabledTextColors(argb(p.textDisabled), argb(Color{ 0, 0, 0, 90 }));
	GadgetListBoxSetFont(window, font(TypeRole::Value));
	window->winSetFont(font(TypeRole::Value));
	skinVerticalSlider(GadgetListBoxGetSlider(window));
	arrowButton(GadgetListBoxGetUpButton(window), 'u');
	arrowButton(GadgetListBoxGetDownButton(window), 'd');
	entryFor(window).role = TypeRole::Value;
}

void skinEntry(GameWindow *window)
{
	if (!window) return;
	const ModernTheme::Palette &p = palette();
	window->winSetDrawFunc(TheWindowManager->getTextEntryDrawFunc());
	window->winSetEnabledColor(0, argb(p.field)); window->winSetEnabledBorderColor(0, argb(p.hairline));
	window->winSetHiliteColor(0, argb(p.field));  window->winSetHiliteBorderColor(0, argb(p.accent));
	window->winSetDisabledColor(0, argb(p.field.withAlpha(110))); window->winSetDisabledBorderColor(0, argb(p.hairline));
	window->winSetEnabledTextColors(argb(p.text), argb(Color{ 0, 0, 0, 120 }));
	window->winSetHiliteTextColors(argb(p.text), argb(Color{ 0, 0, 0, 120 }));
	window->winSetDisabledTextColors(argb(p.textDisabled), argb(Color{ 0, 0, 0, 90 }));
	window->winSetFont(font(TypeRole::Value));
	entryFor(window).role = TypeRole::Value;
}

void skinCombo(GameWindow *window)
{
	if (!window) return;
	const ModernTheme::Palette &p = palette();
	Color fill, edge, sel, selEdge;
	fieldColors(p, fill, edge, sel, selEdge);
	window->winSetDrawFunc(TheWindowManager->getComboBoxDrawFunc());
	GadgetComboBoxSetColors(window, argb(fill), argb(edge), argb(sel), argb(selEdge),
		argb(fill.withAlpha(120)), argb(p.hairline), argb(sel), argb(selEdge),
		argb(fill), argb(p.accent), argb(sel), argb(selEdge));
	GadgetComboBoxSetEnabledTextColors(window, argb(p.text), argb(Color{ 0, 0, 0, 120 }));
	GadgetComboBoxSetHiliteTextColors(window, argb(p.text), argb(Color{ 0, 0, 0, 120 }));
	GadgetComboBoxSetDisabledTextColors(window, argb(p.textDisabled), argb(Color{ 0, 0, 0, 90 }));
	GadgetComboBoxSetFont(window, font(TypeRole::Value));
	window->winSetFont(font(TypeRole::Value));
	if (GameWindow *edit = GadgetComboBoxGetEditBox(window)) skinEntry(edit);
	arrowButton(GadgetComboBoxGetDropDownButton(window), 'd');
	skinList(GadgetComboBoxGetListBox(window));
	entryFor(window).role = TypeRole::Value;
}

void setSelected(GameWindow *window, bool selected)
{
	if (Entry *e = find(window)) e->selected = selected;
}

void setFocusWindow(GameWindow *window) { g_focus = window; }
GameWindow *focusWindow() { return g_focus; }
void clearRegistry() { g_entries.clear(); g_focus = nullptr; }
void clearRegistry(Screen screen)
{
	for (size_t i = g_entries.size(); i-- > 0;)
		if (g_entries[i].owner == screen) g_entries.erase(g_entries.begin() + static_cast<std::ptrdiff_t>(i));
}
unsigned registeredCount() { return static_cast<unsigned>(g_entries.size()); }

// ---- decor ----
// Intentionally empty: a Decor is a process-wide static and the display string manager that owns its strings is already gone when static destructors run (screens call clear() on teardown).
Decor::~Decor() {}

void Decor::clear()
{
	for (int i = 0; i < kMax; ++i)
		if (m_strings[i]) { TheDisplayStringManager->freeDisplayString(static_cast<DisplayString *>(m_strings[i])); m_strings[i] = nullptr; }
	m_count = 0;
}

DecorItem &Decor::add(DecorKind kind)
{
	if (m_count >= kMax) return m_items[kMax - 1];
	DecorItem &it = m_items[m_count];
	it = DecorItem();
	it.kind = kind;
	++m_count;
	return it;
}

void Decor::refreshFonts()
{
	for (int i = 0; i < m_count; ++i)
	{
		if (m_items[i].kind != DecorKind::Text) continue;
		if (!m_strings[i]) { m_strings[i] = TheDisplayStringManager->newDisplayString(); ++g_stats.stringAllocations; }
		DisplayString *ds = static_cast<DisplayString *>(m_strings[i]);
		ds->setFont(font(m_items[i].role));
		ds->setText(UnicodeString(m_items[i].text));
	}
}

void Decor::draw(int ox, int oy)
{
	if (!TheDisplay) return;
	g_scale = g_scales[m_owner];
	LARGE_INTEGER t0, t1, freq;
	QueryPerformanceCounter(&t0);
	const ModernTheme::Palette &p = palette();
	const ModernTheme::Metrics &m = ModernTheme::metrics();
	const int thin = g_scale >= 200 ? 2 : 1;
	for (int i = 0; i < m_count; ++i)
	{
		DecorItem d = m_items[i];
		d.x += ox; d.y += oy;
		switch (d.kind)
		{
		case DecorKind::Scrim:  fillRect(d.x, d.y, d.w, d.h, d.color.a ? d.color : p.scrim); break;
		case DecorKind::Panel:
		case DecorKind::Raised:
			fillChamfer(d.x, d.y, d.w, d.h, d.cutTL, d.cutTR, d.cutBR, d.cutBL, d.kind == DecorKind::Panel ? p.panel : p.panelRaised);
			strokeChamfer(d.x, d.y, d.w, d.h, d.cutTL, d.cutTR, d.cutBR, d.cutBL, thin, d.accent ? p.accent : p.hairline);
			break;
		case DecorKind::Field:
			fillRect(d.x, d.y, d.w, d.h, p.field);
			strokeChamfer(d.x, d.y, d.w, d.h, 0, 0, 0, 0, thin, p.hairline);
			break;
		case DecorKind::Rule:   fillRect(d.x, d.y, d.w, d.h, d.color.a ? d.color : p.hairline); break;
		case DecorKind::Tick:   fillRect(d.x, d.y, d.w, d.h, d.color.a ? d.color : p.accent); break;
		case DecorKind::Bracket: bracket(d.x, d.y, d.w, d.h, dpx(m.bracketLength), thin + 1, d.color.a ? d.color : p.accent); break;
		case DecorKind::Text:
		{
			if (!m_strings[i]) break;
			DisplayString *ds = static_cast<DisplayString *>(m_strings[i]);
			int tw = 0, th = 0;
			ds->getSize(&tw, &th);
			int tx = d.x;
			if (d.align == 1) tx = d.x + (d.w - tw) / 2;
			else if (d.align == 2) tx = d.x + d.w - tw;
			ds->draw(tx, d.y + (d.h - th) / 2, argb(d.color.a ? d.color : p.text), argb(Color{ 0, 0, 0, 150 }));
			break;
		}
		}
	}
	QueryPerformanceCounter(&t1);
	QueryPerformanceFrequency(&freq);
	const unsigned long long us = static_cast<unsigned long long>((t1.QuadPart - t0.QuadPart) * 1000000LL / freq.QuadPart);
	++g_stats.decorDraws;
	g_stats.decorMicros += us;
	if (us > g_stats.worstDecorMicros) g_stats.worstDecorMicros = us;
}

namespace
{
void backdropDraw(GameWindow *window, WinInstanceData *)
{
	if (Decor *d = static_cast<Decor *>(window->winGetUserData()))
	{
		int x = 0, y = 0;
		window->winGetScreenPosition(&x, &y);
		d->draw(x, y);
	}
}
}

GameWindow *createBackdrop(GameWindow *parent, const char *name, Decor *decor, bool container)
{
	const NameKeyType id = TheNameKeyGenerator->nameToKey(AsciiString(name));
	GameWindow *w = TheWindowManager->winGetWindowFromId(parent, id);
	if (!w)
	{
		WinInstanceData inst;
		inst.m_id = id;
		// a pure backdrop is DISABLED as well as no-input: the hit test returns the topmost enabled window under the mouse and a no-input window loses the click instead of passing it on
		w = TheWindowManager->winCreate(parent, (container ? WIN_STATUS_ENABLED : WIN_STATUS_NONE) | WIN_STATUS_NO_INPUT, 0, 0, 10, 10, GameWinDefaultSystem, &inst);
		if (!w) return nullptr;
		w->winSetDrawFunc(backdropDraw);
	}
	if (decor) decor->setOwner(static_cast<unsigned char>(g_owner));
	w->winSetUserData(decor);
	return w;
}
}
