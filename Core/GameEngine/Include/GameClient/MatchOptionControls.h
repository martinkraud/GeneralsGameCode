// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52A: the dropdown controls of the Skirmish "Match Options" (AI Behaviour, Superweapons) and the geometry helper that places retail dropdowns, shared by both titles.
// Positions come from Common/MatchOptionsLayout.h; the controls copy the look of the screen's own dropdowns and labels.
#pragma once

#include "Common/MatchOptionsLayout.h"
#include "Common/NameKeyGenerator.h"

class GameWindow;

namespace MatchOptionControls
{
struct Entry { const char *textKey; const wchar_t *fallback; int value; };

// the entries of the two selectors, in display order
const Entry *aiBehaviorEntries(int &count);        // Original (default), Enhanced
const Entry *superweaponEntries(int &count);       // Disabled, Max 1, Max 3, Max 5, Unlimited (default)

/// Creates (once) a caption + dropdown pair inside `parent`; a second call finds the existing dropdown and refills it. `appearance` is an existing retail dropdown whose look is copied,
/// `labelDonor` an existing static text whose look is copied (may be null). Returns the combo box, or null when it could not be built.
GameWindow *createCombo(GameWindow *parent, const MatchOptionsLayout::Rect &label, const MatchOptionsLayout::Rect &control, GameWindow *appearance, GameWindow *labelDonor,
                        NameKeyType labelId, NameKeyType comboId, const char *labelKey, const wchar_t *labelFallback, const Entry *entries, int count);

/// Moves / resizes a retail static text or dropdown (and the entry field, drop-down button and list of a dropdown) to `where`; the list keeps its number of rows.
void place(GameWindow *window, const MatchOptionsLayout::Rect &where);
void placeCombo(GameWindow *combo, const MatchOptionsLayout::Rect &where);

/// Selects the entry holding `value` (no message is sent) / reads the selected entry's value (`fallback` when nothing is selected).
void select(GameWindow *combo, int value);
int selected(GameWindow *combo, int fallback);
}
