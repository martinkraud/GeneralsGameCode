// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience modal dialogs (confirm / exit / warning): the retail MessageBox.wnd / QuitMessageBox.wnd windows keep their behaviour (callbacks, modality, button routing, sounds)
// and are re-skinned and re-laid out as a centred Modern panel over a scrim. Geometry: Common/ModernScreenLayout.h (computeDialog). Classic is never touched (apply() is a no-op there).
#pragma once

#include "Common/UnicodeString.h"

class GameWindow;

namespace ModernDialog
{
struct Buttons { bool ok, yes, no, cancel; };

// True when the Modern frontend is in use (same gate as the Modern main menu).
bool active();

// Called by GameWindowManager::gogoMessageBox once the retail window exists. `menuName` is "MessageBox.wnd:" or "QuitMessageBox.wnd:".
void apply(GameWindow *trueParent, GameWindow *parent, const char *menuName, const Buttons &buttons, const UnicodeString &title, const UnicodeString &body);
}
