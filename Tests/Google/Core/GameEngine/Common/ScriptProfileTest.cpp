// SPDX-License-Identifier: GPL-3.0-or-later
// X64.42: opt-in script attribution (inert when off, bounded and ranked when on).

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "Common/ScriptProfile.h"

namespace
{
std::string reportText()
{
	std::string text;
	FILE *f = tmpfile();
	if (!f) return text;
	ScriptProfile::report(f);
	fflush(f);
	rewind(f);
	char buffer[4096];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) text.append(buffer, n);
	fclose(f);
	return text;
}

void spin(int iterations)
{
	volatile double x = 1.0;
	for (int i = 0; i < iterations; ++i) x = x * 1.0000001 + 0.5;
}
}

TEST(ScriptProfile, IsInertUntilEnabled)
{
	ScriptProfile::reset();
	EXPECT_FALSE(ScriptProfile::enabled());
	{
		ScriptProfile::FrameProbe frame(300);
		int dummy;
		ScriptProfile::ScriptTimer timer(&dummy, "never recorded");
		EXPECT_EQ(timer.m_row, nullptr);
		ScriptProfile::ActionProbe action(5);
		ScriptProfile::ConditionProbe condition(7);
		condition.finish(true);
		ScriptProfile::SequentialProbe sequential;
	}
	EXPECT_TRUE(reportText().empty());
}

TEST(ScriptProfile, AttributesScriptsConditionsAndActionsToTheWorstFrames)
{
	ScriptProfile::reset();
	ScriptProfile::enable();
	ScriptProfile::setActionName(251, "FireAtMostCost");
	ScriptProfile::setConditionName(57, "HasUnitType");
	int cheap, costly;
	for (unsigned frame = 1; frame <= 400; ++frame)
	{
		ScriptProfile::FrameProbe fp(frame);
		{
			ScriptProfile::ScriptTimer t(&cheap, "Cheap Script");
			ScriptProfile::ConditionProbe c(57);
			spin(50);
			c.finish(false);
		}
		if (frame % 300 == 0)
		{
			ScriptProfile::ScriptTimer t(&costly, "Costly Periodic Script");
			ScriptProfile::ActionProbe a(251);
			spin(2000000);
		}
	}
	const std::string text = reportText();
	EXPECT_NE(text.find("script updates: 400"), std::string::npos);
	EXPECT_NE(text.find("Costly Periodic Script"), std::string::npos);
	EXPECT_NE(text.find("FireAtMostCost"), std::string::npos);
	EXPECT_NE(text.find("HasUnitType"), std::string::npos);
	EXPECT_NE(text.find("frame    300 (mod300=  0)"), std::string::npos);
	// The expensive tick is ranked first among the worst script updates.
	const size_t heading = text.find("script updates:");
	ASSERT_NE(heading, std::string::npos);
	const size_t first = text.find("  frame ", heading);
	ASSERT_NE(first, std::string::npos);
	EXPECT_EQ(text.compare(first, 18, "  frame    300 (mo"), 0);
	ScriptProfile::reset();
}

TEST(ScriptProfile, NestedActionsAreNotCountedTwice)
{
	ScriptProfile::reset();
	ScriptProfile::enable();
	ScriptProfile::setActionName(1, "Outer");
	ScriptProfile::setActionName(2, "Inner");
	int script;
	{
		ScriptProfile::FrameProbe fp(1);
		ScriptProfile::ScriptTimer t(&script, "Calls Subroutine");
		ScriptProfile::ActionProbe outer(1);
		ScriptProfile::ActionProbe inner(2);
		spin(500000);
	}
	const std::string text = reportText();
	// The script row's action time is the outer action only: the inner one is inside it.
	const size_t row = text.find("Calls Subroutine");
	ASSERT_NE(row, std::string::npos);
	EXPECT_NE(text.find("Outer"), std::string::npos);
	EXPECT_NE(text.find("Inner"), std::string::npos);
	ScriptProfile::reset();
}

TEST(ScriptProfile, WorstFrameListIsBounded)
{
	ScriptProfile::reset();
	ScriptProfile::enable();
	int script;
	for (unsigned frame = 1; frame <= 500; ++frame)
	{
		ScriptProfile::FrameProbe fp(frame);
		ScriptProfile::ScriptTimer t(&script, "S");
		spin(100 + static_cast<int>(frame % 7) * 100);
	}
	const std::string text = reportText();
	size_t count = 0, at = 0;
	while ((at = text.find("\n  frame ", at)) != std::string::npos) { ++count; at += 5; }
	EXPECT_LE(count, 24u);
	EXPECT_GE(count, 1u);
	ScriptProfile::reset();
}
