#pragma once

class GameWindow;
class OptionPreferences;

// Native Options gadgets. The containing layout owns their lifetime.
GameWindow *createFrameRateOptions(GameWindow *detail, const OptionPreferences &preferences);
void resetFrameRateOptions(GameWindow *combo);
void acceptFrameRateOptions(GameWindow *combo, OptionPreferences &preferences);
