// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience drawing / skinning layer (engine side of Common/ModernTheme.h + Common/ModernComponents.h).
//
//   * Resolves the typography roles to REAL installed fonts (never a silent GDI substitute) and hands out the fonts for the current UI scale.
//   * Draws the Modern components procedurally (chamfered surfaces, hairlines, ticks, text) through the display primitives: no retail button art.
//   * Skins retail gadgets: a retail window keeps its behaviour (callbacks, hit testing, sounds) and gets a Modern draw function, colours and font. The registry that remembers which Modern
//     kind a window has is per screen and cleared when the screen is left (windows are destroyed with their layout, never owned here).
//   * "Decor": the non-interactive chrome of a screen (panels, rules, headings, labels) is a flat list drawn by ONE backdrop window per screen (no window per rule).
#pragma once

#include "Common/ModernComponents.h"
#include "Common/ModernTheme.h"

class GameWindow;
class GameFont;

namespace ModernDraw
{
using ModernComponents::Kind;
using ModernTheme::TypeRole;

// ---- theme state ----
void setTitle(ModernTheme::Title title);
ModernTheme::Title title();
const ModernTheme::Palette &palette();
int scalePercent();                        // the effective UI scale the screen was laid out with
void setScalePercent(int percent);         // re-resolves fonts of every registered window and decor item

// ---- typography ----
const char *resolvedFamily(TypeRole role);          // the installed family the role really uses ("" before the first resolve)
bool familyInstalled(const char *family);
GameFont *font(TypeRole role);                      // at the current scale
GameFont *fontAt(TypeRole role, int scalePercent);  // at an explicit scale (dialogs lay themselves out at their own effective scale)

// ---- primitives (screen coordinates) ----
void fillChamfer(int x, int y, int w, int h, int cutTL, int cutTR, int cutBR, int cutBL, const ModernTheme::Color &c);
void strokeChamfer(int x, int y, int w, int h, int cutTL, int cutTR, int cutBR, int cutBL, int width, const ModernTheme::Color &c);
void bracket(int x, int y, int w, int h, int arm, int width, const ModernTheme::Color &c);   // four corner brackets
int px(int referenceUnits);                // reference units -> pixels at the current scale

// ---- skinning of retail windows ----
void skinButton(GameWindow *window, Kind kind, TypeRole role);
void skinDialogButton(GameWindow *window, Kind kind, TypeRole role, int scalePercent);   // not registered: a dialog's windows are destroyed independently of the screen below it
void skinText(GameWindow *window, TypeRole role, const ModernTheme::Color &color);
void skinCombo(GameWindow *window);
void skinList(GameWindow *window);
void skinEntry(GameWindow *window);
void skinCheck(GameWindow *window);
void skinSlider(GameWindow *window);
void setSelected(GameWindow *window, bool selected);   // the "selected" state of a skinned button (the active tab)
void setFocusWindow(GameWindow *window);               // keyboard focus indicator (navigation model)
GameWindow *focusWindow();
void clearRegistry();
unsigned registeredCount();

// ---- decor ----
enum class DecorKind : unsigned char { Panel, Raised, Scrim, Rule, Tick, Text, Bracket, Field };
struct DecorItem
{
	DecorKind kind = DecorKind::Panel;
	int x = 0, y = 0, w = 0, h = 0;                 // relative to the backdrop window
	int cutTL = 0, cutTR = 0, cutBR = 0, cutBL = 0;
	TypeRole role = TypeRole::Body;
	int align = 0;                                  // Text: 0 left, 1 centre, 2 right
	ModernTheme::Color color = {};                  // Text / Rule / Tick colour (alpha 0 = the role's default)
	wchar_t text[96] = {};
	bool accent = false;                            // Panel / Raised: accent edge instead of the neutral edge
};

class Decor
{
public:
	~Decor();
	void clear();
	DecorItem &add(DecorKind kind);
	int count() const { return m_count; }
	DecorItem &item(int i) { return m_items[i]; }
	void refreshFonts();
	void draw(int originX, int originY);   // item coordinates are relative to the backdrop window's top-left
private:
	static constexpr int kMax = 96;
	DecorItem m_items[kMax];
	void *m_strings[kMax] = {};       // DisplayString*
	int m_count = 0;
};

// The backdrop window of a screen: a window below every other child that draws a Decor.
// `container` = the backdrop also hosts child controls (it must stay enabled so the hit test descends into it); otherwise it is disabled and never takes a click.
GameWindow *createBackdrop(GameWindow *parent, const char *name, Decor *decor, bool container = false);
}
