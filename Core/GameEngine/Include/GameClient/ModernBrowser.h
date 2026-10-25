// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience list browsers (Load Game, Replays): a screen that is "a list and a column of actions". The retail gadgets keep their callbacks and behaviour; this layer skins them with the
// Modern components, re-composes them from Common/ModernScreenLayout.h (computeBrowser) and draws the chrome (title, rules, panel) with one ModernDraw::Decor backdrop. The retail inline
// confirmation panels of the screen (overwrite / load / delete / description) are re-skinned as Modern dialogs over the browser panel. Classic never calls create().
#pragma once

#include "Common/ModernComponents.h"
#include "Common/ModernScreenLayout.h"

class GameWindow;

namespace ModernBrowser
{
struct Action
{
	const char *window;                       // button name inside the menu
	ModernComponents::Kind kind;              // Primary / Secondary / Danger
	const char *labelKey;                     // GameText key; the retail caption is replaced by it in capitals
	const wchar_t *fallback;
};

struct InlineDialog
{
	const char *container;                    // the retail confirmation panel (hidden until the screen shows it)
	const char *confirm;                      // its primary button
	const char *cancel;                       // its secondary button
	const char *entry;                        // optional text entry (the save description)
	const char *titleKey;
	const wchar_t *titleFallback;
};

struct Spec
{
	const char *menu;                         // window-name prefix, e.g. "SaveLoad.wnd:"
	const char *list;                         // the list box
	const char *actionContainer;              // optional: the retail frame that hosts the buttons (kept as their container and resized to the action column)
	const char *titleKey;
	const wchar_t *titleFallback;
	Action actions[6];
	int actionCount;
	InlineDialog dialogs[4];
	int dialogCount;
};

// The two browsers of the game; the Save / Load screen exists twice (the main menu's full-screen SaveLoad.wnd and the in-game PopupSaveLoad.wnd), `menuPrefix` is its window-name prefix.
const Spec &saveLoadSpec(const char *menuPrefix);   // "SaveLoad.wnd:" or "PopupSaveLoad.wnd:"
const Spec &replaySpec();

bool create(const Spec &spec);   // false in Classic or when the retail layout is not the expected one
void update();                   // once per frame: re-lays out on a surface / UI scale change
void forget();
bool active();
const ModernScreen::BrowserLayout &layout();
GameWindow *actionWindow(int index);
GameWindow *listWindow();
GameWindow *dialogWindow(int index);          // the retail inline confirmation panel `index` of the spec (null when the screen has none)
}
