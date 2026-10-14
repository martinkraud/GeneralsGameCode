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

// Called when the Options screen closes (windows are owned by the layout).
void forget();
}
