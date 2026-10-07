/*
** Copyright 2026 TheSuperHackers
** This program is free software: you can redistribute it and/or modify it
** under the terms of the GNU General Public License, version 3 or later.
** This program is distributed WITHOUT ANY WARRANTY; see the GNU GPL for details.
*/
#include <gtest/gtest.h>
#include "Common/FrameRateLimit.h"
#include "Common/OptionPreferences.h"

TEST(OptionPreferences, MissingRenderPreferenceUses60WithoutAddingKeys)
{
	OptionPreferences preferences(FALSE);
	preferences.setAsciiString("FPSLimit", "no"); // legacy limiter switch does not select a finite cap
	EXPECT_EQ(preferences.getFrameRateLimit(), 60);
	EXPECT_EQ(preferences.find("FrameRateLimit"), preferences.end());
}

TEST(OptionPreferences, SupportedRenderPreferencesAreAccepted)
{
	OptionPreferences preferences(FALSE);
	const Int rates[] = {30, 60, 120, 144, 165, 240};
	for (const Int rate : rates)
	{
		SCOPED_TRACE(rate);
		preferences.setInt("FrameRateLimit", rate);
		EXPECT_EQ(preferences.getFrameRateLimit(), rate);
		EXPECT_TRUE(RenderFpsPreset::isOptionFpsValue(rate));
	}
}

TEST(OptionPreferences, MalformedAndUnsupportedRenderPreferencesFallBackTo60)
{
	OptionPreferences preferences(FALSE);
	const char *values[] = {"", "0", "-1", "15", "29", "50", "75", "100", "180", "241", "480",
		"1000000", "Unlimited", "60fps", "60.0", "999999999999999999999999999999"};
	for (const char *value : values)
	{
		SCOPED_TRACE(value);
		preferences.setAsciiString("FrameRateLimit", value);
		EXPECT_EQ(preferences.getFrameRateLimit(), 60);
	}
}

TEST(OptionPreferences, SettingRenderPreferencePreservesOtherKeysAndValidatesInput)
{
	OptionPreferences preferences(FALSE);
	preferences.setAsciiString("Resolution", "1920 1080");
	preferences.setAsciiString("UnknownModOption", "preserved");
	preferences.setFrameRateLimit(144);
	EXPECT_EQ(preferences.getFrameRateLimit(), 144);
	EXPECT_STREQ(preferences.getAsciiString("FrameRateLimit", "").str(), "144");
	preferences.setFrameRateLimit(-1);
	EXPECT_EQ(preferences.getFrameRateLimit(), 60);
	EXPECT_STREQ(preferences.getAsciiString("Resolution", "").str(), "1920 1080");
	EXPECT_STREQ(preferences.getAsciiString("UnknownModOption", "").str(), "preserved");
}
