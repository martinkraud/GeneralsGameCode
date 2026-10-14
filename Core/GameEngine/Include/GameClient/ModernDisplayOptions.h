// SPDX-License-Identifier: GPL-3.0-or-later
// X64.33 Options > Display (Modern Experience). Native Generals gadgets built at runtime from the packaged
// OptionsMenu.wnd (same draw data, fonts and panel as the original screen); nothing is replaced on disk.
#pragma once

class GameWindow;
class OptionPreferences;

namespace ModernDisplayOptions
{
// Splits the Resolution row of the Video panel and adds the "Display..." entry button plus the popup panel. Returns
// false (and adds nothing) in Classic, in diagnostic profiles and when a mod layout leaves no room.
bool create(GameWindow *resolutionCombo, GameWindow *popupTemplate, GameWindow *buttonTemplate,
            const OptionPreferences &preferences, bool inMatch);

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
