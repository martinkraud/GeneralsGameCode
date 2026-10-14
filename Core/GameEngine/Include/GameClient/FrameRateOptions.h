#pragma once

class GameWindow;
class OptionPreferences;

// Native Options gadgets. The containing layout owns their lifetime.
// `tabbed` (Modern Experience Options pages): the caption and combo are plain gadgets next to the Detail combo
// (ids OptionsMenu.wnd:FrameRateLimitLabel / ComboBoxFrameRateLimit); the page layout positions them and the Detail row
// is not split. The default keeps the retail split used by Classic and Zero Hour.
GameWindow *createFrameRateOptions(GameWindow *detail, const OptionPreferences &preferences, bool tabbed = false);
void resetFrameRateOptions(GameWindow *combo);
void acceptFrameRateOptions(GameWindow *combo, OptionPreferences &preferences);
