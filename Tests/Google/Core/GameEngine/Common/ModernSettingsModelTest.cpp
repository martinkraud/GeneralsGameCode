// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Frontend screen kit: the pending-changes model, and the contract of the Options page tables that feed the generated rows.
#include <gtest/gtest.h>

#include <cstring>
#include <set>
#include <string>

#include "Common/ModernSettingsModel.h"
#include "Common/OptionsPageLayout.h"

using ModernSettings::Tracker;

TEST(ModernSettingsModel, CleanUntilSomethingChangesAndCleanAgainWhenItIsPutBack)
{
	Tracker t;
	EXPECT_FALSE(t.dirty());
	t.add(1, 50); t.add(2, 0); t.add(3, 7);
	EXPECT_FALSE(t.dirty());
	t.set(1, 80);
	EXPECT_TRUE(t.dirty()); EXPECT_EQ(t.dirtyCount(), 1); EXPECT_TRUE(t.dirtyAt(t.find(1))); EXPECT_FALSE(t.dirtyAt(t.find(2)));
	t.set(1, 50);                                         // put back: not dirty any more
	EXPECT_FALSE(t.dirty());
	t.set(2, 1); t.set(3, 8);
	EXPECT_EQ(t.dirtyCount(), 2);
}

TEST(ModernSettingsModel, AcceptMakesTheCurrentValuesTheBaselineAndRevertForgetsTheChanges)
{
	Tracker t;
	t.add(10, 1); t.add(11, 2);
	t.set(10, 5);
	t.accept();
	EXPECT_FALSE(t.dirty());
	t.set(10, 1);                                          // differs from the NEW baseline
	EXPECT_TRUE(t.dirty());
	t.revert();
	EXPECT_FALSE(t.dirty());
	EXPECT_EQ(t.current[t.find(10)], 5);
}

TEST(ModernSettingsModel, UnknownKeysAreIgnoredAndCapacityIsBounded)
{
	Tracker t;
	EXPECT_FALSE(t.set(99, 1));
	EXPECT_EQ(t.find(99), -1);
	for (int i = 0; i < ModernSettings::kMaxControls; ++i) EXPECT_GE(t.add(1000 + i, i), 0);
	EXPECT_EQ(t.add(5000, 1), -1);                         // full: refused, nothing corrupted
	EXPECT_EQ(t.count, ModernSettings::kMaxControls);
	EXPECT_EQ(t.add(1000, 42), 0);                         // re-baselining a known key is not a new control
	EXPECT_FALSE(t.dirty());
}

TEST(OptionsPages, EveryCategoryHasItsRowsAndNoControlLivesOnTwoPages)
{
	using namespace OptionsPageLayout;
	std::set<std::string> controls, labels;
	for (int c = 0; c < kCategoryCount; ++c)
	{
		const PageTable table = pageItems(static_cast<Category>(c));
		if (static_cast<Category>(c) == Category::Network) { EXPECT_EQ(table.count, 0); continue; }   // the retail block, kept whole
		EXPECT_GE(table.count, 1) << c;
		EXPECT_LE(table.count, kMaxRows) << c;
		for (int i = 0; i < table.count; ++i)
		{
			const PageItem &item = table.items[i];
			ASSERT_NE(item.control, nullptr);
			EXPECT_TRUE(controls.insert(item.control).second) << item.control << " appears on two pages";
			EXPECT_EQ(ownerOf(item.control), c) << item.control;
			if (item.kind == ControlKind::Check) EXPECT_EQ(item.label, nullptr) << item.control << ": a check box carries its own caption";
			else { ASSERT_NE(item.label, nullptr) << item.control; EXPECT_TRUE(labels.insert(item.label).second) << item.label; EXPECT_EQ(ownerOf(item.label), c); }
		}
	}
	EXPECT_GE(controls.size(), 14u);                        // the real settings of the six pages, no placeholders
}

TEST(OptionsPages, EveryCategoryHasACaptionKeyAndTheModernPageHoldsTheEngineFeatures)
{
	using namespace OptionsPageLayout;
	std::set<std::string> keys;
	for (int c = 0; c < kCategoryCount; ++c)
	{
		const char *key = categoryTextKey(static_cast<Category>(c));
		ASSERT_NE(key, nullptr);
		EXPECT_EQ(std::strncmp(key, "GUI:OptionsTab", 14), 0) << key;
		EXPECT_TRUE(keys.insert(key).second) << key << " is used by two categories";
	}
	EXPECT_EQ(ownerOf("ModernMotion"), static_cast<int>(Category::Modern));
	EXPECT_EQ(ownerOf("ModernInterface"), static_cast<int>(Category::Modern));
	EXPECT_EQ(ownerOf("ComboBoxResolution"), static_cast<int>(Category::Display));
	EXPECT_EQ(ownerOf("ModernRenderScale"), static_cast<int>(Category::Graphics));
}
