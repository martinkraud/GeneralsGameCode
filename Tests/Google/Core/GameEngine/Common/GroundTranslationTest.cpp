/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <gtest/gtest.h>
#include <new>
#include "Common/GroundTranslation.h"

namespace
{
PresentationTiming timing(unsigned int generation = 11, unsigned int epoch = 1, float alpha = 0.5f)
{
	PresentationTiming result = {generation - 1, generation, epoch, alpha, true};
	return result;
}
Matrix3D matrix(float x)
{
	Matrix3D result(true);
	result.Rotate_Z(0.7f);
	result.Scale(1.25f);
	result.Set_Translation(Vector3(x, x * 2, x * 3));
	return result;
}
void prime(GroundTranslationHistory& history)
{
	history.capture(matrix(0).Get_Translation(), 10, 1);
	history.capture(matrix(2).Get_Translation(), 11, 1);
}
void expectCanonical(const GroundTranslationHistory& history, const PresentationTiming& t, bool eligible = true)
{
	const Matrix3D canonical = matrix(2);
	EXPECT_EQ(history.getRenderTransform(canonical, t, eligible).Get_Translation(), canonical.Get_Translation());
}
TEST(GroundTranslation, FirstSampleSnapsAndConsecutivePairInterpolatesXYZ)
{
	GroundTranslationHistory history;
	history.capture(matrix(0).Get_Translation(), 10, 1);
	EXPECT_EQ(history.getRenderTransform(matrix(0), timing(10), true).Get_Translation(), Vector3(0, 0, 0));
	history.capture(matrix(2).Get_Translation(), 11, 1);
	EXPECT_EQ(history.getRenderTransform(matrix(2), timing(), true).Get_Translation(), Vector3(1, 2, 3));
}
class GroundAlpha : public testing::TestWithParam<float> {};
TEST_P(GroundAlpha, EndpointsMidpointAndClamping)
{
	GroundTranslationHistory history;
	prime(history);
	const float a = GetParam();
	const float clamped = a < 0 ? 0 : (a > 1 ? 1 : a);
	EXPECT_EQ(history.getRenderTransform(matrix(2), timing(11, 1, a), true).Get_Translation(),
		Vector3(2 * clamped, 4 * clamped, 6 * clamped));
}
INSTANTIATE_TEST_SUITE_P(Weights, GroundAlpha, testing::Values(-1.0f, 0.0f, 0.5f, 1.0f, 2.0f));
TEST(GroundTranslation, CanonicalMatrixBasisAndInstanceCompositionStayIntact)
{
	GroundTranslationHistory history;
	prime(history);
	const Matrix3D canonical = matrix(2);
	const Matrix3D original = canonical;
	const Matrix3D rendered = history.getRenderTransform(canonical, timing(), true);
	for (int row = 0; row < 3; ++row)
		for (int col = 0; col < 4; ++col)
		{
			EXPECT_EQ(canonical[row][col], original[row][col]);
			if (col < 3)
				EXPECT_EQ(rendered[row][col], canonical[row][col]);
		}
	Matrix3D instance(true);
	instance.Set_Translation(Vector3(4, 0, 0));
	Matrix3D renderedInstance = rendered;
	Matrix3D canonicalInstance = canonical;
	renderedInstance.postMul(instance);
	canonicalInstance.postMul(instance);
	const Vector3 difference = renderedInstance.Get_Translation() - canonicalInstance.Get_Translation();
	EXPECT_NEAR(difference.X, -1, 0.00001f);
	EXPECT_NEAR(difference.Y, -2, 0.00001f);
	EXPECT_NEAR(difference.Z, -3, 0.00001f);
}
TEST(GroundTranslation, InvalidTimingUnsupportedObjectAndMismatchedPairsSnap)
{
	GroundTranslationHistory history;
	prime(history);
	PresentationTiming t = timing();
	t.valid = false;
	expectCanonical(history, t);
	expectCanonical(history, timing(), false);
	expectCanonical(history, timing(12));
	expectCanonical(history, timing(11, 2));
	t = timing();
	t.previousGeneration = 9;
	expectCanonical(history, t);
}
TEST(GroundTranslation, EpochChangeGapAndResetRequireFreshPair)
{
	GroundTranslationHistory history;
	prime(history);
	history.observeEpoch(2);
	expectCanonical(history, timing());
	history.capture(matrix(2).Get_Translation(), 11, 2);
	expectCanonical(history, timing(11, 2));
	history.capture(matrix(2).Get_Translation(), 15, 2);
	expectCanonical(history, timing(15, 2));
	history.reset();
	expectCanonical(history, timing());
}
TEST(GroundTranslation, DestructionAndRecreationAtSameAddressCannotReusePair)
{
	alignas(GroundTranslationHistory) unsigned char storage[sizeof(GroundTranslationHistory)];
	GroundTranslationHistory* history = new (storage) GroundTranslationHistory;
	prime(*history);
	history->~GroundTranslationHistory();
	history = new (storage) GroundTranslationHistory;
	expectCanonical(*history, timing());
	history->~GroundTranslationHistory();
}
TEST(GroundTranslation, MultipleDrawsAndDuplicateCaptureDoNotAdvanceHistory)
{
	GroundTranslationHistory history;
	prime(history);
	for (int i = 0; i < 10; ++i)
	{
		history.capture(matrix(9).Get_Translation(), 11, 1);
		EXPECT_EQ(history.getRenderTransform(matrix(2), timing(), true).Get_Translation(), Vector3(1, 2, 3));
	}
}
TEST(GroundTranslation, OnlyFinalCompletedPositionIsSampled)
{
	GroundTranslationHistory history;
	history.capture(matrix(0).Get_Translation(), 10, 1);
	Matrix3D canonical = matrix(1);
	canonical = matrix(7); // Intermediate setter result, never captured.
	canonical = matrix(2);
	history.capture(canonical.Get_Translation(), 11, 1);
	EXPECT_EQ(history.getRenderTransform(canonical, timing(), true).Get_Translation(), Vector3(1, 2, 3));
}
TEST(GroundTranslation, LargeRepositionAndExplicitDiscontinuitySnap)
{
	GroundTranslationHistory history;
	prime(history);
	history.capture(matrix(50).Get_Translation(), 12, 1);
	EXPECT_EQ(history.getRenderTransform(matrix(50), timing(12), true).Get_Translation(), matrix(50).Get_Translation());
	history.reset(); // Semantic containment/rebind notification, even for a tiny move.
	expectCanonical(history, timing());
}
TEST(GroundTranslation, CanonicalChangesAfterCaptureCannotUseStalePresentation)
{
	GroundTranslationHistory history;
	prime(history);
	EXPECT_EQ(history.getRenderTransform(matrix(3), timing(), true).Get_Translation(), matrix(3).Get_Translation());
}
TEST(GroundTranslation, StationaryPairsRemainStationary)
{
	GroundTranslationHistory history;
	history.capture(matrix(2).Get_Translation(), 10, 1);
	history.capture(matrix(2).Get_Translation(), 11, 1);
	expectCanonical(history, timing());
}
TEST(GroundTranslation, SixRenderRatesConsumeClockWithoutFpsSpecificHistory)
{
	for (int fps : {30, 60, 120, 144, 165, 240})
	{
		PresentationClock clock;
		GroundTranslationHistory history;
		float remainder = 0;
		const float period = 1.0f / 30;
		const float delta = 1.0f / fps;
		unsigned int frame = 0;
		Matrix3D canonical = matrix(0);
		for (int i = 0; i < 1000; ++i)
		{
			remainder += delta;
			const bool completed = remainder >= period;
			if (completed) { remainder -= period; ++frame; canonical = matrix(float(frame)); }
			clock.observeScheduler(remainder, period);
			clock.advance(frame, completed, delta, true);
			const PresentationTiming t = clock.getTiming();
			history.observeEpoch(t.epoch);
			if (completed) history.capture(canonical.Get_Translation(), t.generation, t.epoch);
			const Vector3 rendered = history.getRenderTransform(canonical, t, true).Get_Translation();
			if (t.valid)
				EXPECT_NEAR(rendered.X, float(frame - 1) + t.alpha, 0.0001);
			else
				EXPECT_EQ(rendered, canonical.Get_Translation());
		}
	}
}
TEST(GroundTranslation, GateAndExactTemplateAllowlistAreConservative)
{
	EXPECT_FALSE(GroundTranslation::isEnabled());
	GroundTranslation::setEnabled(true);
	EXPECT_TRUE(GroundTranslation::isEnabled());
	GroundTranslation::setEnabled(false);
	EXPECT_TRUE(GroundTranslation::supportsTemplate("AmericaInfantryRanger"));
	EXPECT_TRUE(GroundTranslation::supportsTemplate("AmericaTankCrusader"));
	EXPECT_FALSE(GroundTranslation::supportsTemplate("AirF_AmericaInfantryRanger"));
	EXPECT_FALSE(GroundTranslation::supportsTemplate("AmericaJetRaptor"));
	EXPECT_FALSE(GroundTranslation::supportsTemplate(nullptr));
}
}
