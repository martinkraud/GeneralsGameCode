// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience main menu: the binding layer between the pure model (Common/ModernMainMenuModel.h, ModernFocus.h, ModernTheme.h, ModernComponents.h, ModernUiLayout.h) and real Generals windows.
// UI.2 built the shell; the Modern Frontend milestone (UI.3) redrew it with the design system (GameClient/ModernDraw.h): a full-height navigation band with text-forward items, no retail button art.
//
// Ownership and lifetime
//   * The shell owns NO window outright: every window it creates (the band, which also draws the chrome, the logo, one push button per action) is a CHILD of the retail MainMenuParent, so it is destroyed
//     with the retail layout and never outlives it. init() is idempotent: it finds its windows by name and creates them only when missing (the layout is re-initialised every time a
//     child screen is popped, and re-created after a match).
//   * The retail main-menu windows keep existing (callbacks, automation and transitions look them up by name) but are hidden while the Modern shell is active.
//   * The logical state (page stack, remembered focus, selected campaign) lives in a process-wide ModernMainMenu::State so returning from a child screen restores the page and the focus.
//   * Layout is computed from the pure model when the surface or the UI scale changes (or the page changes), never per frame.
// Behaviour: a Modern button forwards to the title's retail behaviour through Hooks (the title's MainMenu.cpp owns the retail statics: campaign selection, transitions, shell pushes).
#pragma once

#include "Common/ModernMainMenuModel.h"

class GameWindow;

namespace ModernMenuShell
{
struct Hooks
{
	// The engine side of an Effect (SelectCampaign / SelectChallenge / CancelSelection / StartSelected / Engine). Returns true when the main menu is being left
	// (a screen was pushed, the game starts, the exit was confirmed); false when the menu stays (Options overlay, a declined exit).
	bool (*perform)(const ModernMainMenu::Effect &effect);
};

struct Stats
{
	unsigned relayouts = 0;                  // geometry recomputations (surface / scale / first show)
	unsigned pageChanges = 0;
	unsigned long long relayoutMicros = 0;
	unsigned updates = 0;                    // update() calls
	unsigned long long updateMicros = 0;     // time spent in update() in total
	unsigned long long worstUpdateMicros = 0;
};

// True when the Modern frontend is in use for this process: the modern display (normal native launch), not -classic, the feature enabled, and not switched off for comparison with
// GENERALS_MODERN_FRONTEND=0 (developer aid).
bool active();

void init(GameWindow *menuParent, ModernTheme::Title title, const Hooks &hooks);
bool built();
void reveal(bool instant);       // first interaction (or immediately when returning): show the panel
bool revealed();
void update(bool leaving);       // once per frame from MainMenuUpdate; `leaving` = the retail menu is being left (mirrors buttonPushed / game start)
bool onKey(int key, int state);  // GWM_CHAR; true when consumed
bool onSelected(GameWindow *control);   // GBM_SELECTED of a Modern button; true when it was one
bool onHover(GameWindow *control);      // GBM_MOUSE_ENTERING of a Modern button; true when it was one
bool performRetail(GameWindow *retailButton);   // automation / compatibility: the retail control id -> the same Modern action (true when mapped)
void forgetNavigation();         // the game starts: the next menu starts at the root page
const Stats &stats();

// ---- diagnostics for the audit (read-only) ----
ModernTheme::Title title();
int visibleButtonCount();
GameWindow *buttonWindow(ModernMainMenu::Action action);
GameWindow *panelWindow();
ModernMainMenu::Page currentPage();
const ModernMainMenu::Geometry &geometry();
int focusedActionId();
}
