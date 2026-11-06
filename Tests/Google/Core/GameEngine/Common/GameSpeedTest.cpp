/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

// The pure rules of the Skirmish "Game Speed" match rule (Common/GameSpeed.h): levels, exact logic rates, the options string / replay header key, the preference text, the runtime
// Speed Up / Down steps and the save round trip. The scheduler seam (FramePacer) is covered by the contract test test_game_speed_contract.py and the headless / graphical runs.

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include "Common/GameSpeed.h"
#include "Common/GameSpeedXfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"

using namespace GameSpeed;

TEST(GameSpeed, NormalIsTheDefaultAndExactlyTheRetailCadence)
{
	EXPECT_EQ(kNormal, 100);
	EXPECT_FALSE(isActive(kNormal));
	EXPECT_DOUBLE_EQ(logicFps(kNormal), 30.0);
	EXPECT_DOUBLE_EQ(secondsPerFrame(kNormal), 1.0 / 30.0);
}

TEST(GameSpeed, OnlyTheExposedLevelsAreValidAndRatesAreExact)
{
	for (unsigned v : { 75u, 100u, 125u, 150u }) EXPECT_TRUE(isValid(v)) << v;
	for (unsigned v : { 0u, 50u, 74u, 99u, 101u, 200u, 250u, 10000u }) EXPECT_FALSE(isValid(v)) << v;
	EXPECT_DOUBLE_EQ(logicFps(75), 22.5);
	EXPECT_DOUBLE_EQ(logicFps(125), 37.5);
	EXPECT_DOUBLE_EQ(logicFps(150), 45.0);
	EXPECT_EQ(sanitize(123), kNormal);          // never a pace nobody chose
	EXPECT_DOUBLE_EQ(logicFps(123), 30.0);
}

TEST(GameSpeed, TheOptionsStringKeyIsWrittenOnlyForANonNormalValue)
{
	char text[8];
	EXPECT_FALSE(formatOptionValue(kNormal, text, sizeof text));     // a default match leaves every options string byte-identical
	EXPECT_FALSE(formatOptionValue(123, text, sizeof text));
	for (const Level &l : kLevels)
	{
		if (l.percent == kNormal) continue;
		ASSERT_TRUE(formatOptionValue(l.percent, text, sizeof text)) << l.percent;
		uint16_t parsed = 0;
		ASSERT_TRUE(parseOptionValue(text, parsed)) << text;
		EXPECT_EQ(parsed, l.percent);
	}
	EXPECT_STREQ((formatOptionValue(75, text, sizeof text), text), "75");
	EXPECT_STREQ((formatOptionValue(125, text, sizeof text), text), "125");
	uint16_t keep = 77;
	for (const char *bad : { "", "0", "100", "99", "200", "12x", "-75", "1250", "abc" }) { EXPECT_FALSE(parseOptionValue(bad, keep)) << bad; EXPECT_EQ(keep, 77); }
	EXPECT_FALSE(parseOptionValue(nullptr, keep));
	EXPECT_STREQ(optionKey(), "GS");
}

TEST(GameSpeed, PreferenceTextRoundTripsAndIsStable)
{
	for (const Level &l : kLevels)
	{
		uint16_t parsed = 0;
		ASSERT_TRUE(parsePreferenceText(preferenceText(l.percent), parsed)) << l.prefText;
		EXPECT_EQ(parsed, l.percent);
	}
	EXPECT_STREQ(preferenceText(100), "Normal");
	EXPECT_STREQ(preferenceText(150), "VeryFast");
	uint16_t keep = 100;
	EXPECT_FALSE(parsePreferenceText("Warp", keep));
	EXPECT_FALSE(parsePreferenceText(nullptr, keep));
	EXPECT_EQ(keep, 100);
}

TEST(GameSpeed, RuntimeStepsAreDiscreteClampedAndNeverWrap)
{
	EXPECT_EQ(step(100, +1), 125);
	EXPECT_EQ(step(125, +1), 150);
	EXPECT_EQ(step(150, +1), 150);   // clamped at Very Fast
	EXPECT_EQ(step(100, -1), 75);
	EXPECT_EQ(step(75, -1), 75);     // clamped at Slow
	EXPECT_EQ(step(100, 0), 100);
	EXPECT_EQ(step(999, +1), 125);   // an unknown value is treated as Normal first
}

TEST(GameSpeed, SaveVersion7RoundTripsEveryLevelAndOlderVersionsReadNormal)
{
	const char *file = "game_speed_xfer.tmp";
	for (const Level &l : kLevels)
	{
		UnsignedShort saved = l.percent, loaded = 7;
		{ XferSave out; out.open(file); xferPercent(&out, saved, 7); out.close(); }
		{ XferLoad in; in.open(file); xferPercent(&in, loaded, 7); in.close(); }
		EXPECT_EQ(loaded, saved) << l.percent;
	}
	{   // a stored value that is not selectable (damaged / foreign file) reads as Normal
		UnsignedShort bad = 200, loaded = 150;
		{ XferSave out; out.open(file); out.xferUnsignedShort(&bad); out.close(); }
		{ XferLoad in; in.open(file); xferPercent(&in, loaded, 7); in.close(); }
		EXPECT_EQ(loaded, kNormal);
	}
	{   // versions below 7 carry no field: nothing is written and loading leaves Normal
		UnsignedShort saved = 150, loaded = 125;
		{ XferSave out; out.open(file); xferPercent(&out, saved, 6); out.close(); }
		{ XferLoad in; in.open(file); xferPercent(&in, loaded, 6); in.close(); }
		EXPECT_EQ(loaded, kNormal);
		EXPECT_EQ(saved, 150);
	}
	std::remove(file);
}
