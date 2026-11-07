/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// Modern Options help: every exposed setting has exactly one short factual help sentence, and the page tables (incl. the generated Network rows) own the controls the help describes.

#include <gtest/gtest.h>
#include <cwchar>
#include <set>
#include <string>
#include "Common/OptionsPageLayout.h"
#include "Common/SettingHelp.h"

TEST(SettingHelp, EveryPageControlHasHelpAndNoHelpIsOrphaned)
{
	using namespace OptionsPageLayout;
	std::set<std::string> controls;
	for (int c = 0; c < kCategoryCount; ++c)
	{
		const PageTable t = pageItems(static_cast<Category>(c));
		for (int i = 0; i < t.count; ++i)
		{
			controls.insert(t.items[i].control);
			EXPECT_NE(SettingHelp::find(t.items[i].control), nullptr) << "no help for " << t.items[i].control;
		}
	}
	for (const SettingHelp::Entry &e : SettingHelp::kEntries)
		EXPECT_EQ(controls.count(e.control), 1u) << "orphaned help entry " << e.control;
	std::set<std::string> seen;
	for (const SettingHelp::Entry &e : SettingHelp::kEntries)
		EXPECT_TRUE(seen.insert(e.control).second) << "duplicate help entry " << e.control;
}

TEST(SettingHelp, TextIsShortFactualAndKeyed)
{
	const wchar_t *const banned[] = { L"amazing", L"stunning", L"revolution", L"ultimate", L"best", L"incredible", L"powerful" };
	for (const SettingHelp::Entry &e : SettingHelp::kEntries)
	{
		const std::wstring text = e.fallback;
		EXPECT_GE(text.size(), 20u) << e.control;
		EXPECT_LE(text.size(), 230u) << e.control;              // two lines in the help strip
		EXPECT_EQ(text.back(), L'.') << e.control;
		EXPECT_EQ(std::string(e.key).rfind("GUI:Help", 0), 0u) << e.control;
		for (const wchar_t *word : banned) EXPECT_EQ(text.find(word), std::wstring::npos) << e.control << " " << std::wstring(word).size();
	}
}

TEST(SettingHelp, GameSpeedAndMatchRulesAreNotGlobalSettings)
{
	// Game Speed, Starting Cash, AI Behaviour and Superweapons are per-match rules (Match Setup), never rows of the global Options pages
	using namespace OptionsPageLayout;
	for (const char *name : { "ComboBoxGameSpeed", "ComboBoxAIBehavior", "ComboBoxSuperweapons", "ComboBoxStartingCash" })
		EXPECT_EQ(ownerOf(name), -1) << name;
}

TEST(SettingHelp, NetworkIsAGeneratedPageAndControlsPageHasTheZeroHourOptions)
{
	using namespace OptionsPageLayout;
	EXPECT_EQ(pageItems(Category::Network).count, 4);
	EXPECT_EQ(ownerOf("ComboBoxIP"), static_cast<int>(Category::Network));
	EXPECT_EQ(ownerOf("TextEntryFirewallPortOverride"), static_cast<int>(Category::Network));
	EXPECT_EQ(ownerOf("ButtonFirewallRefresh"), static_cast<int>(Category::Network));
	EXPECT_EQ(ownerOf("Retaliation"), static_cast<int>(Category::Controls));
	EXPECT_EQ(ownerOf("CheckDoubleClickAttackMove"), static_cast<int>(Category::Controls));
	// the retail HTTP proxy and send-delay controls stay hidden (obsolete online-service switches)
	EXPECT_EQ(ownerOf("TextEntryHTTPProxy"), -1);
	EXPECT_EQ(ownerOf("CheckSendDelay"), -1);
}
