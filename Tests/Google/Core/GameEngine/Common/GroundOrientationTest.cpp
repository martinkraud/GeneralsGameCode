/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <new>
#include "Common/GroundTranslation.h"
#include "Common/GroundTranslationPolicy.h"
namespace
{
Matrix3D root(float heading, float x = 2)
{
	Matrix3D m(true); m.Rotate_Z(heading); m.Set_Translation(Vector3(x, 4, 6)); return m;
}
PresentationTiming phase(float alpha = 0.5f) { return {10, 11, 1, alpha, true}; }
void sample(GroundTranslationHistory& h, const Matrix3D& m, unsigned int generation,
	unsigned int epoch = 1, bool eligible = true)
{
	h.capture(m.Get_Translation(), generation, epoch, m.Get_Z_Rotation(), eligible);
}
void pair(GroundTranslationHistory& h, float previous = 0, float current = 1)
{ sample(h, root(previous, 0), 10); sample(h, root(current), 11); }
void expectMatrix(const Matrix3D& a, const Matrix3D& b, float tolerance = 0.00001f)
{
	for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) EXPECT_NEAR(a[r][c], b[r][c], tolerance);
}
void expectAngle(float a, float b)
{ EXPECT_NEAR(WWMath::Normalize_Angle(a - b), 0, 0.0001f); }
class HeadingAlpha : public testing::TestWithParam<float> {};
TEST_P(HeadingAlpha, XYZAndHeadingEndpointsMidpointClamp)
{
	GroundTranslationHistory h; pair(h);
	const float a = GetParam(), clamped = a < 0 ? 0 : (a > 1 ? 1 : a);
	const Matrix3D canonical = root(1), original = canonical;
	const Matrix3D m = h.getRenderTransform(canonical, phase(a), true, true);
	expectAngle(m.Get_Z_Rotation(), clamped);
	EXPECT_NEAR(m.Get_X_Translation(), 2 * clamped, 0.00001f);
	expectMatrix(canonical, original, 0);
	if (a >= 1) expectMatrix(m, canonical, 0);
}
INSTANTIATE_TEST_SUITE_P(Weights, HeadingAlpha, testing::Values(-1.0f, 0.0f, 0.5f, 1.0f, 2.0f));
TEST(GroundOrientation, WraparoundBothDirectionsAndFullRangeBoundaries)
{
	const float d = WWMATH_PI / 180;
	for (const auto direction : {1.0f, -1.0f})
	{
		GroundTranslationHistory h; pair(h, -direction * d, direction * d);
		expectAngle(h.getRenderTransform(root(direction * d), phase(), true, true).Get_Z_Rotation(), 0);
	}
	expectAngle(GroundTranslation::interpolateHeading(359*d, d, 0.5f), 0);
	expectAngle(GroundTranslation::interpolateHeading(d, 359*d, 0.5f), 0);
	expectAngle(GroundTranslation::interpolateHeading(-WWMATH_TWO_PI, WWMATH_TWO_PI, 0.5f), 0);
}
TEST(GroundOrientation, ExactAndNearHalfTurnDeterministic)
{
	EXPECT_NEAR(GroundTranslation::interpolateHeading(0, WWMATH_PI, 0.5f), -WWMATH_PI/2, 0.00001f);
	EXPECT_NEAR(GroundTranslation::interpolateHeading(0, -WWMATH_PI, 0.5f), -WWMATH_PI/2, 0.00001f);
	EXPECT_GT(GroundTranslation::interpolateHeading(0, WWMATH_PI-0.001f, 0.5f), 0);
	EXPECT_LT(GroundTranslation::interpolateHeading(0, WWMATH_PI+0.001f, 0.5f), 0);
}
TEST(GroundOrientation, StationaryAndRapidDirectionChanges)
{
	GroundTranslationHistory h; pair(h, 0.7f, 0.7f);
	expectAngle(h.getRenderTransform(root(0.7f), phase(), true, true).Get_Z_Rotation(), 0.7f);
	sample(h, root(-2.7f), 12); PresentationTiming t={11,12,1,0.5f,true};
	expectAngle(h.getRenderTransform(root(-2.7f), t, true, true).Get_Z_Rotation(),
		GroundTranslation::interpolateHeading(root(0.7f).Get_Z_Rotation(), root(-2.7f).Get_Z_Rotation(), 0.5f));
}
TEST(GroundOrientation, FirstGapEpochResetSnap)
{
	GroundTranslationHistory h; sample(h,root(1),11);
	expectMatrix(h.getRenderTransform(root(1),phase(),true,true),root(1));
	pair(h); sample(h,root(2),14); PresentationTiming t={13,14,1,0.5f,true};
	expectMatrix(h.getRenderTransform(root(2),t,true,true),root(2));
	h.reset(); pair(h); h.observeEpoch(2);
	expectMatrix(h.getRenderTransform(root(1),phase(),true,true),root(1));
	h.reset(); pair(h); h.reset();
	expectMatrix(h.getRenderTransform(root(1),phase(),true,true),root(1));
}
TEST(GroundOrientation, OrientationResetRetainsTranslationAndNeedsFreshPair)
{
	GroundTranslationHistory h; pair(h); h.resetOrientation();
	auto m=h.getRenderTransform(root(1),phase(),true,true);
	expectAngle(m.Get_Z_Rotation(),1); EXPECT_EQ(m.Get_X_Translation(),1);
	sample(h,root(1.2f),12); PresentationTiming t={11,12,1,0.5f,true};
	expectAngle(h.getRenderTransform(root(1.2f),t,true,true).Get_Z_Rotation(),1.2f);
	sample(h,root(1.4f),13); t={12,13,1,0.5f,true};
	expectAngle(h.getRenderTransform(root(1.4f),t,true,true).Get_Z_Rotation(),1.3f);
}
TEST(GroundOrientation, UnsupportedHeadingKeepsTranslation)
{
	GroundTranslationHistory h; pair(h);
	auto m=h.getRenderTransform(root(1),phase(),true,false);
	expectAngle(m.Get_Z_Rotation(),1); EXPECT_EQ(m.Get_X_Translation(),1);
	h.reset(); sample(h,root(0,0),10,1,false); sample(h,root(1),11);
	expectAngle(h.getRenderTransform(root(1),phase(),true,true).Get_Z_Rotation(),1);
}
TEST(GroundOrientation, InvalidHeadingAndNaNAlphaFailClosed)
{
	const float nan=std::numeric_limits<float>::quiet_NaN(), inf=std::numeric_limits<float>::infinity();
	for (float invalid : {nan,inf,-inf,1.0e30f})
	{
		GroundTranslationHistory h; sample(h,root(0,0),10);
		h.capture(root(1).Get_Translation(),11,1,invalid,true);
		auto m=h.getRenderTransform(root(1),phase(),true,true);
		expectAngle(m.Get_Z_Rotation(),1); EXPECT_EQ(m.Get_X_Translation(),1);
		EXPECT_EQ(GroundTranslation::interpolateHeading(invalid,1,0.5f),1);
	}
	GroundTranslationHistory h; pair(h);
	expectMatrix(h.getRenderTransform(root(1),phase(nan),true,true),root(1));
	Matrix3D bad=root(1); bad[1][2]=inf;
	EXPECT_FALSE(GroundTranslation::supportsHeadingBasis(bad));
	bad=root(1); bad[0][0]=bad[1][0]=0;
	EXPECT_FALSE(GroundTranslation::supportsHeadingBasis(bad));
}
TEST(GroundOrientation, StaleHeadingInvalidTimingAndWrongPairSnapHeading)
{
	GroundTranslationHistory h; pair(h);
	auto m=h.getRenderTransform(root(2),phase(),true,true);
	expectAngle(m.Get_Z_Rotation(),2); EXPECT_EQ(m.Get_X_Translation(),1);
	for (int i=0;i<4;++i)
	{
		auto t=phase(); if(i==0)t.valid=false; if(i==1)++t.epoch; if(i==2)++t.generation; if(i==3)--t.previousGeneration;
		expectMatrix(h.getRenderTransform(root(1),t,true,true),root(1));
	}
}
TEST(GroundOrientation, CurrentPitchRollScaleAndLaterDecorationPreserved)
{
	Matrix3D canonical=root(1); canonical.Rotate_Y(0.3f); canonical.Rotate_X(-0.2f); canonical.Scale(1.25f,0.8f,1.6f);
	Matrix3D residual=canonical; residual.Set_Translation(Vector3(0,0,0)); residual.In_Place_Pre_Rotate_Z(-canonical.Get_Z_Rotation());
	GroundTranslationHistory h; sample(h,root(0,0),10); sample(h,canonical,11);
	const Matrix3D original=canonical;
	Matrix3D rendered=h.getRenderTransform(canonical,phase(),true,true);
	Matrix3D expected=residual; expected.In_Place_Pre_Rotate_Z(0.5f*canonical.Get_Z_Rotation()); expected.Set_Translation(Vector3(1,4,6));
	expectMatrix(rendered,expected); expectMatrix(canonical,original,0);
	// Existing instance then local physics Y/X/Z and Z decoration, including turret-relative bone composition.
	Matrix3D instance(true); instance.Rotate_Z(0.1f); instance.Set_Translation(Vector3(1,0,0));
	rendered.postMul(instance); expected.postMul(instance);
	rendered.Translate(0,0,0.2f); expected.Translate(0,0,0.2f);
	rendered.Rotate_Y(0.12f); expected.Rotate_Y(0.12f);
	rendered.Rotate_X(-0.09f); expected.Rotate_X(-0.09f);
	rendered.Rotate_Z(0.05f); expected.Rotate_Z(0.05f); expectMatrix(rendered,expected);
	const float turretAngle=0.6f; Matrix3D turret(true); turret.Rotate_Z(turretAngle); const Matrix3D before=turret;
	rendered.postMul(turret); expectMatrix(turret,before,0); // No AI/turret setter participates.
}
TEST(GroundOrientation, InfantryMovementVehicleCombatAndUnknownStates)
{
	using namespace GroundTranslation;
	for(auto state:{AI_IDLE,AI_MOVE_TO,AI_WAIT,AI_MOVE_OUT_OF_THE_WAY,AI_MOVE_AND_TIGHTEN})
	{ EXPECT_TRUE(supportsOrientationMovement(state,true,false)); EXPECT_FALSE(supportsOrientationMovement(state,true,true)); }
	for(auto state:{AI_ATTACK_OBJECT,AI_FORCE_ATTACK_OBJECT,AI_ATTACK_MOVE_TO,AI_ATTACK_AND_FOLLOW_OBJECT,AI_GUARD})
	{ EXPECT_TRUE(supportsOrientationMovement(state,false,false)); EXPECT_FALSE(supportsOrientationMovement(state,true,false)); }
	EXPECT_FALSE(supportsOrientationMovement(static_cast<AIStateType>(-1),false,false));
}
TEST(GroundOrientation, RootCapabilitiesRejectAttachedArticulatedAndCustom)
{
	EXPECT_TRUE(GroundTranslation::supportsRootOrientationDraw(true,false,false)); // Model/Tank/unarticulated Truck adapter.
	EXPECT_FALSE(GroundTranslation::supportsRootOrientationDraw(false,false,false));
	EXPECT_FALSE(GroundTranslation::supportsRootOrientationDraw(true,true,false));
	EXPECT_FALSE(GroundTranslation::supportsRootOrientationDraw(true,false,true));
}
TEST(GroundOrientation, UnsupportedTranslationAndThirtyFPSRemainCanonical)
{
	GroundTranslationHistory h; pair(h);
	expectMatrix(h.getRenderTransform(root(1),phase(),false,true),root(1));
	PresentationClock clock; clock.observeScheduler(0,0); clock.advance(11,true,1.0f/30,false);
	expectMatrix(h.getRenderTransform(root(1),clock.getTiming(),true,true),root(1));
}
TEST(GroundOrientation, ScriptBlockedPolicyAndGateOffSnapCanonical)
{
	GroundTranslationHistory h; pair(h);
	GroundTranslation::Eligibility c={true,true,true,true,true,AI_MOVE_TO,1,true};
	GroundTranslation::setEnabled(true);
	expectMatrix(h.getRenderTransform(root(1),phase(),GroundTranslation::canInterpolate(c),true),root(1));
	c.blocked=false; GroundTranslation::setEnabled(false);
	expectMatrix(h.getRenderTransform(root(1),phase(),GroundTranslation::canInterpolate(c),true),root(1));
}
TEST(GroundOrientation, RepeatedCaptureAndDrawDoNotRotatePair)
{
	GroundTranslationHistory h; pair(h);
	for(int i=0;i<20;++i) { sample(h,root(-1),11); expectAngle(h.getRenderTransform(root(1),phase(),true,true).Get_Z_Rotation(),0.5f); }
}
TEST(GroundOrientation, LifetimeReuseCannotInheritHeading)
{
	alignas(GroundTranslationHistory) unsigned char storage[sizeof(GroundTranslationHistory)];
	auto* h=new(storage) GroundTranslationHistory; pair(*h); h->~GroundTranslationHistory();
	h=new(storage) GroundTranslationHistory; expectMatrix(h->getRenderTransform(root(1),phase(),true,true),root(1)); h->~GroundTranslationHistory();
}
TEST(GroundOrientation, SyntheticRenderCapsShareClockPair)
{
	for(int fps:{60,120,144,165,240})
	{
		GroundTranslationHistory h; PresentationClock clock; float remainder=0; unsigned int frame=0; int checks=0;
		for(int i=0;i<fps*2;++i)
		{
			const float dt=1.0f/fps; remainder+=dt; const bool completed=remainder>=1.0f/30;
			if(completed){remainder-=1.0f/30; ++frame;}
			clock.observeScheduler(remainder,1.0f/30); clock.advance(frame,completed,dt,true);
			const auto t=clock.getTiming(); h.observeEpoch(t.epoch);
			const auto current=root(frame*0.02f,0);
			if(completed)sample(h,current,frame,t.epoch);
			const auto rendered=h.getRenderTransform(current,t,true,true);
			if(t.valid && frame>1){expectAngle(rendered.Get_Z_Rotation(),(frame-1+t.alpha)*0.02f); ++checks;}
		}
		EXPECT_GT(checks,0);
	}
}
}
