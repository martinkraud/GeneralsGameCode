// SPDX-License-Identifier: GPL-3.0-or-later
// X64.38: the determinism fingerprint (pure).

#include <gtest/gtest.h>

#include "Common/DeterminismFingerprint.h"

using namespace DeterminismFingerprint;

static std::vector<Checkpoint> trace(int n)
{
	std::vector<Checkpoint> t;
	for (int i = 0; i < n; ++i) t.push_back({static_cast<std::uint32_t>(i * 30), 0xA0000000u + static_cast<std::uint32_t>(i) * 7919u, 100u + static_cast<std::uint32_t>(i)});
	return t;
}

TEST(DeterminismFingerprint, EmptyDigestIsTheFnvOffsetBasis)
{
	EXPECT_EQ(digest({}), kFnvOffset);
	EXPECT_EQ(formatDigest(kFnvOffset), "CBF29CE484222325");
}

TEST(DeterminismFingerprint, EqualTracesHaveEqualDigestsAndNoDivergence)
{
	const auto a = trace(50), b = trace(50);
	EXPECT_EQ(digest(a), digest(b));
	EXPECT_EQ(firstDivergence(a, b), -1);
}

TEST(DeterminismFingerprint, AnySingleFieldChangeChangesTheDigestAndIsLocalised)
{
	const auto base = trace(40);
	for (int field = 0; field < 3; ++field)
	{
		auto changed = base;
		if (field == 0) changed[17].frame += 1;
		if (field == 1) changed[17].crc ^= 1u;
		if (field == 2) changed[17].objects += 1;
		EXPECT_NE(digest(base), digest(changed));
		EXPECT_EQ(firstDivergence(base, changed), 17);
	}
}

TEST(DeterminismFingerprint, OrderMattersBecauseTheTraceIsOrdered)
{
	auto a = trace(10);
	auto b = a;
	std::swap(b[3], b[4]);
	EXPECT_NE(digest(a), digest(b));
	EXPECT_EQ(firstDivergence(a, b), 3);
}

TEST(DeterminismFingerprint, APrefixDivergesAtTheShorterLength)
{
	const auto a = trace(10);
	auto b = a;
	b.resize(6);
	EXPECT_EQ(firstDivergence(a, b), 6);
	EXPECT_NE(digest(a), digest(b));
}

TEST(DeterminismFingerprint, TextFormIsStable)
{
	EXPECT_EQ(format({300, 0xDEADBEEFu, 42}), "frame=300 crc=DEADBEEF objects=42");
}
