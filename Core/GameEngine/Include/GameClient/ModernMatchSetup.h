// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience Match Setup (Skirmish): the retail SkirmishGameOptionsMenu.wnd gadgets - the player grid, the map preview, Starting Cash, AI Behaviour, Superweapons, Start, Back -
// are skinned with the Modern components and re-composed as MAP | PLAYERS | MATCH RULES by Common/ModernMatchSetupLayout.h. Every callback, message and game-info update of the
// retail screen stays in place; this layer only moves, skins and hides windows and draws the chrome (one backdrop window with a ModernDraw::Decor).
//
// The rules are generated from a SPEC LIST (ModernMatchSetupScreen::rule specs): a spec names a caption, a control window and when it applies; the layout takes only the number of rules,
// so a new rule is one spec row and gets a card like every other rule - no bespoke geometry. Classic never calls create() (the retail screen is untouched).
#pragma once

#include "Common/ModernMatchSetupLayout.h"

class GameWindow;

namespace ModernMatchSetupScreen
{
struct Hooks
{
	void (*repositionStartSpots)();   // the retail start-position markers are placed relative to the map window: call after it moved / resized
};

// One rule of the Match Rules zone.
struct RuleSpec
{
	const char *id;               // stable name (tests, diagnostics)
	const char *labelKey;         // GameText key of the caption
	const wchar_t *fallback;      // caption when the key is missing
	const char *window;           // control window, name inside SkirmishGameOptionsMenu.wnd
	const char *replacedBy;       // optional: when this window exists the rule is not offered (the legacy check box is replaced by the Superweapons dropdown)
};
const RuleSpec *ruleSpecs(int &count);

// Builds the screen over the retail one (after the retail Init created every gadget). Returns false in Classic or when the layout is not the retail Skirmish layout.
bool create(GameWindow *menuParent, const Hooks &hooks);

// Once per frame: re-lays out when the surface or the UI scale changed.
void update();

// The screen is closing.
void forget();

bool active();
const ModernMatchSetup::Layout &layout();     // for the audit
int ruleCount();
}
