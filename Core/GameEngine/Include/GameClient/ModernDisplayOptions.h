// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 Options > Display (Modern Experience). Native Generals gadgets built at runtime from the packaged
// OptionsMenu.wnd (same draw data, fonts and panel as the original screen); nothing is replaced on disk.
#pragma once

class GameWindow;
class OptionPreferences;

namespace ModernDisplayOptions
{
// Creates the Modern Experience display/gameplay controls as plain gadgets next to the retail Resolution combo
// (same panel, same draw data): Display Mode, Monitor, UI Scale, VSync and Unit Motion. They are positioned by the
// Options pages (OptionsTabs). Returns false (and adds nothing) in Classic and in diagnostic profiles.
bool create(GameWindow *resolutionCombo, const OptionPreferences &preferences, bool inMatch);

// True when more than one monitor is attached (the Monitor row is only shown then).
bool hasMultipleMonitors();

// Button routing from OptionsMenuSystem: true when `controlId` belonged to this feature.
bool onButton(int controlId);

// Options > Defaults.
void reset();

// Called once from the options "Accept" path after the resolution combo was processed. Persists the display keys
// into `preferences`, applies what can be applied live and returns true when the shell layouts must be rebuilt.
bool apply(OptionPreferences &preferences, bool layoutsAlreadyRebuilt);

// Fills the retail Resolution combo from the real, mode- and monitor-aware size list (Modern Experience only).
void populateResolution(GameWindow *combo);

// The Display Mode / Monitor combos changed: the Resolution list follows the pending mode and monitor.
void onSelectionChanged(GameWindow *control);

// X64.37 transactional apply: after Accept, a display change that is applied waits for Keep / Revert.
bool confirmationPending();
void showConfirmation();   // the Keep / Revert dialog (call once the Options layout is gone)
void update();             // per frame while the shell runs: countdown and the automatic revert

// Called when the Options screen closes (windows are owned by the layout).
void forget();
}
