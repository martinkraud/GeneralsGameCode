/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// X64.52A: the pure rules of the Skirmish "Superweapons" match rule. The engine glue (identification of the structures, counting, the build check) is covered by the
// source contract (test_superweapon_limit_contract.py) and by the headless dynamic scenarios (Invoke-AiBaseExperiments.ps1 -Sw / -SwAid).

#include <gtest/gtest.h>
#include <set>
#include <string>
#include "Common/SuperweaponLimit.h"
#include "Common/SuperweaponLimitXfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include <cstdio>

using namespace SuperweaponLimit;

TEST(SuperweaponLimit, DefaultIsUnlimitedAndChecksNothing)
{
	EXPECT_EQ(kUnlimited, 0);
	EXPECT_FALSE(isActive(kUnlimited));
	for (unsigned owned : { 0u, 1u, 5u, 50u, 1000u }) EXPECT_FALSE(blocks(kUnlimited, owned)) << owned;
}

TEST(SuperweaponLimit, OnlyTheFiveSelectableValuesAreValid)
{
	for (unsigned v : { 0u, 1u, 3u, 5u, (unsigned)kDisabled }) EXPECT_TRUE(isValid(v)) << v;
	for (unsigned v : { 2u, 4u, 6u, 7u, 100u, 0xFFFEu, 0x10000u }) EXPECT_FALSE(isValid(v)) << v;
}

TEST(SuperweaponLimit, MaxNBlocksAtNOwnedAndFreesWhenOneIsDestroyed)
{
	for (unsigned cap : { 1u, 3u, 5u })
	{
		for (unsigned owned = 0; owned < cap; ++owned) EXPECT_FALSE(blocks(cap, owned)) << "cap " << cap << " owned " << owned;
		for (unsigned owned = cap; owned < cap + 4; ++owned) EXPECT_TRUE(blocks(cap, owned)) << "cap " << cap << " owned " << owned;
		// not a lifetime quota: the decision depends only on what is owned NOW
		EXPECT_TRUE(blocks(cap, cap));
		EXPECT_FALSE(blocks(cap, cap - 1));
	}
}

TEST(SuperweaponLimit, CaptureNeverDestroysItOnlyBlocksFurtherConstruction)
{
	// Max 1, the player owns one structure and captures another: owned = 2 > cap. The rule has no notion of "forced removal": it only answers "may I START one more"
	EXPECT_TRUE(blocks(1, 2));
	EXPECT_TRUE(blocks(1, 3));
	// ... and construction becomes legal again once the player is below the cap
	EXPECT_FALSE(blocks(3, 2));
	EXPECT_TRUE(blocks(3, 4));
	EXPECT_FALSE(blocks(3, 2));
}

TEST(SuperweaponLimit, DisabledBlocksEverything)
{
	for (unsigned owned : { 0u, 1u, 7u }) EXPECT_TRUE(blocks(kDisabled, owned));
}

TEST(SuperweaponLimit, OptionStringRoundTripAndStrictness)
{
	char text[4];
	EXPECT_FALSE(formatOptionValue(kUnlimited, text, sizeof text)) << "Unlimited writes nothing: default replay headers stay byte-identical";
	EXPECT_FALSE(formatOptionValue(2, text, sizeof text));
	EXPECT_STREQ(optionKey(), "SW");
	std::set<std::string> written;
	for (unsigned v : { 1u, 3u, 5u, (unsigned)kDisabled })
	{
		ASSERT_TRUE(formatOptionValue(v, text, sizeof text)) << v;
		written.insert(text);
		uint16_t parsed = 0;
		ASSERT_TRUE(parseOptionValue(text, parsed)) << text;
		EXPECT_EQ(parsed, v);
	}
	EXPECT_EQ(written, (std::set<std::string>{ "1", "3", "5", "D" }));
	uint16_t keep = 77;
	// refused, and the result is not modified: never "read as Unlimited"
	for (const char *bad : { "", "0", "2", "4", "6", "9", "10", "55", "X", "d1", "D ", "-1", "99999", "Max1", "u" })
	{
		keep = 77;
		EXPECT_FALSE(parseOptionValue(bad, keep)) << bad;
		EXPECT_EQ(keep, 77) << bad;
	}
	EXPECT_FALSE(parseOptionValue(nullptr, keep));
	EXPECT_TRUE(parseOptionValue("d", keep)); EXPECT_EQ(keep, kDisabled);   // case-insensitive D
}

TEST(SuperweaponLimit, PreferenceTextRoundTrip)
{
	for (unsigned v : { 0u, 1u, 3u, 5u, (unsigned)kDisabled })
	{
		uint16_t parsed = 99;
		ASSERT_TRUE(parsePreferenceText(preferenceText(v), parsed)) << v;
		EXPECT_EQ(parsed, v);
	}
	uint16_t keep = 5;
	EXPECT_FALSE(parsePreferenceText("Max2", keep)); EXPECT_EQ(keep, 5);
	EXPECT_FALSE(parsePreferenceText("", keep));
	EXPECT_FALSE(parsePreferenceText(nullptr, keep));
	EXPECT_TRUE(parsePreferenceText("unlimited", keep)); EXPECT_EQ(keep, kUnlimited);   // hand-edited preference files may differ in case
}

TEST(SuperweaponLimit, SaveVersion6RoundTripsEveryValueAndOlderVersionsReadUnlimited)
{
	const char *file = "x64_52a_superweapon_limit.tmp";
	for (unsigned v : { 0u, 1u, 3u, 5u, (unsigned)kDisabled })
	{
		UnsignedShort saved = (UnsignedShort)v, loaded = 7;
		{ XferSave out; out.open(file); xferLimit(&out, saved, 6); out.close(); }
		{ XferLoad in; in.open(file); xferLimit(&in, loaded, 6); in.close(); }
		EXPECT_EQ(loaded, saved) << v;
	}
	// a stored value that is not selectable (damaged / foreign file) reads as Unlimited
	{
		UnsignedShort bad = 4, loaded = 5;
		{ XferSave out; out.open(file); out.xferUnsignedShort(&bad); out.close(); }
		{ XferLoad in; in.open(file); xferLimit(&in, loaded, 6); in.close(); }
		EXPECT_EQ(loaded, kUnlimited);
	}
	// versions below 6 carry no field: nothing is written (the stream stays empty) and loading leaves the rule Unlimited
	{
		UnsignedShort saved = 3, loaded = 5;
		{ XferSave out; out.open(file); xferLimit(&out, saved, 5); out.close(); }
		{ XferLoad in; in.open(file); xferLimit(&in, loaded, 5); in.close(); }
		EXPECT_EQ(loaded, kUnlimited);
		EXPECT_EQ(saved, 3);
	}
	std::remove(file);
}
