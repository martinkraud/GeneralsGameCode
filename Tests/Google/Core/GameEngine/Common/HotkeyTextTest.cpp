/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// Modern Options > Keyboard: how bindings are written for a player.

#include <gtest/gtest.h>
#include "Common/HotkeyText.h"

TEST(HotkeyText, LettersFunctionKeysAndNamedKeys)
{
	EXPECT_EQ(HotkeyText::keyName("KEY_A"), "A");
	EXPECT_EQ(HotkeyText::keyName("KEY_F5"), "F5");
	EXPECT_EQ(HotkeyText::keyName("KEY_PGUP"), "Page Up");
	EXPECT_EQ(HotkeyText::keyName("KEY_PGDN"), "Page Down");
	EXPECT_EQ(HotkeyText::keyName("KEY_KPPLUS"), "Num +");
	EXPECT_EQ(HotkeyText::keyName("KEY_KP7"), "Num 7");
	EXPECT_EQ(HotkeyText::keyName("KEY_BACKSLASH"), "\\");
	EXPECT_EQ(HotkeyText::keyName("KEY_TICK"), "`");
	EXPECT_EQ(HotkeyText::keyName(nullptr), "");
	EXPECT_EQ(HotkeyText::keyName(""), "");
}

TEST(HotkeyText, ModifiersAreInAFixedOrder)
{
	EXPECT_EQ(HotkeyText::modifiers(false, false, false), "");
	EXPECT_EQ(HotkeyText::modifiers(true, false, false), "Ctrl+");
	EXPECT_EQ(HotkeyText::modifiers(true, true, true), "Ctrl+Alt+Shift+");
	EXPECT_EQ(HotkeyText::modifiers(false, true, true), "Alt+Shift+");
}

TEST(HotkeyText, ComboAndUnassigned)
{
	EXPECT_EQ(HotkeyText::combo("KEY_F5", true, false, true), "Ctrl+Shift+F5");
	EXPECT_EQ(HotkeyText::combo("KEY_KPMINUS", true, false, false), "Ctrl+Num -");
	EXPECT_EQ(HotkeyText::combo("KEY_NONE", true, false, false), "");   // an unassigned command has no combination, whatever its modifier bits say
	EXPECT_EQ(HotkeyText::combo(nullptr, false, false, false), "");
}
