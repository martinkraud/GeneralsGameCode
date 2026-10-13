/*
** Copyright 2026 TheSuperHackers
** This program is free software: you can redistribute it and/or modify it
** under the terms of the GNU General Public License, version 3 or later.
** This program is distributed WITHOUT ANY WARRANTY; see the GNU GPL for details.
*/
// Regression coverage for the two X64.28 first-native-play failures:
//  1. Exit Game: the play tap was a static object handed to MessageStream::attachTranslator, which owns and
//     deletes every translator, so removeTranslator() freed a non-heap pointer.
//  2. Reconnaissance drone / radius cursor: RadiusDecalTemplate::createRadiusDecal dereferenced the null
//     TheProjectedShadowManager of the native (scene-less) play profile.
#include <gtest/gtest.h>
#include "Common/GameCommon.h"
#include "Common/GameType.h"
#include "GameClient/Color.h"
#include "Common/AsciiString.h"
#include "Common/MessageStream.h"
#include "GameClient/Shadow.h"

// The template keeps its parse state private; the test needs to set a texture name without an INI.
#define private public
#include "GameClient/RadiusDecal.h"
#undef private

namespace
{
int g_destroyed = 0;

class CountingTranslator : public GameMessageTranslator
{
public:
	GameMessageDisposition translateGameMessage(const GameMessage *) override { return KEEP_MESSAGE; }
	~CountingTranslator() override { ++g_destroyed; }
};
}

TEST(PlayWorldLifecycle, MessageStreamOwnsAndDeletesAttachedTranslators)
{
	g_destroyed = 0;
	{
		MessageStream stream;
		const TranslatorID id = stream.attachTranslator(new CountingTranslator, 998000000);
		EXPECT_NE(id, 0u);
		EXPECT_EQ(g_destroyed, 0);
		stream.removeTranslator(id);
		// Ownership contract the play tap violated: removal destroys the translator.
		EXPECT_EQ(g_destroyed, 1);
	}
	EXPECT_EQ(g_destroyed, 1);

	{
		MessageStream stream;
		stream.attachTranslator(new CountingTranslator, 1);
		EXPECT_EQ(g_destroyed, 1);
	}
	// A translator left attached is destroyed with the stream.
	EXPECT_EQ(g_destroyed, 2);
}

TEST(PlayWorldLifecycle, RadiusDecalWithoutProjectedShadowManagerIsLogicallyNonEmptyAndSafe)
{
	// The native play profile never creates the legacy shadow managers.
	ASSERT_EQ(TheProjectedShadowManager, nullptr);

	RadiusDecalTemplate tmpl;
	tmpl.m_name = "EXRadiusDecal";
	ASSERT_TRUE(tmpl.valid());

	RadiusDecal decal;
	Coord3D pos = {10.0f, 20.0f, 0.0f};
	// The player is never dereferenced before the missing manager is detected.
	const Player *owner = reinterpret_cast<const Player *>(static_cast<uintptr_t>(0x10));
	tmpl.createRadiusDecal(pos, 50.0f, owner, decal);

	// Net/sync rule from the header: emptiness must not depend on whether a decal could be drawn.
	EXPECT_FALSE(decal.isEmpty());
	decal.setPosition(pos);
	decal.setOpacity(0.5f);
	decal.clear();
	EXPECT_TRUE(decal.isEmpty());
}
