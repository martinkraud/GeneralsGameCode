/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

// X64.37 v3: Render Scale (internal 3D resolution). Pure arithmetic, settings and independence from the UI scale; no GPU needed.

#include <gtest/gtest.h>

#include "Common/DisplayGeometry.h"
#include "Common/DisplaySettings.h"
#include "Common/EnhancedHud.h"
#include "WW3D2/Backend/WorldRenderScale.h"

#include <cmath>

using namespace WorldRenderScale;

TEST(WorldRenderScale, OffersTheDocumentedValuesAndDefaultsToNative)
{
	ASSERT_EQ(kChoiceCount, 6);
	const unsigned expected[] = {100, 90, 80, 75, 67, 50};
	for (int i = 0; i < kChoiceCount; ++i) EXPECT_EQ(kChoices[i], expected[i]);
	EXPECT_EQ(kDefaultPercent, 100u);
	EXPECT_FALSE(active(100));
	for (int i = 1; i < kChoiceCount; ++i) EXPECT_TRUE(active(kChoices[i]));
	for (int i = 0; i < kChoiceCount; ++i) EXPECT_EQ(indexOf(kChoices[i]), i);
}

TEST(WorldRenderScale, InvalidAndOutOfRangeValuesAreSnappedOrDefaulted)
{
	EXPECT_EQ(sanitize(0), 100u);
	EXPECT_EQ(sanitize(-30), 100u);
	EXPECT_EQ(sanitize(100000), 100u);
	EXPECT_EQ(sanitize(401), 100u);
	EXPECT_EQ(sanitize(73), 75u);
	EXPECT_EQ(sanitize(60), 67u);
	EXPECT_EQ(sanitize(10), 50u);                 // "too small" is the smallest offered value, never an unusable size
	EXPECT_EQ(sanitize(120), 100u);
	for (unsigned c : kChoices) EXPECT_EQ(sanitize(static_cast<int>(c)), c);
}

TEST(WorldRenderScale, InternalDimensionsRoundSafelyAndNeverBecomeInvalid)
{
	EXPECT_EQ(internalSize(2560, 1440, 100).width, 2560u);
	EXPECT_EQ(internalSize(2560, 1440, 75).width, 1920u);
	EXPECT_EQ(internalSize(2560, 1440, 75).height, 1080u);
	EXPECT_EQ(internalSize(2560, 1440, 50).width, 1280u);
	EXPECT_EQ(internalSize(2560, 1440, 50).height, 720u);
	EXPECT_EQ(internalSize(1920, 1080, 67).width, 1286u);        // 1286.4 -> 1286
	EXPECT_EQ(internalSize(800, 600, 50).width, 400u);
	// never zero, never above the presentation size, never below the minimum (or the presentation size when it is smaller)
	for (unsigned full : {1u, 2u, 17u, 40u, 63u, 64u, 65u, 100u, 800u, 5120u, 16384u})
		for (unsigned percent : kChoices)
		{
			const unsigned d = scaledDimension(full, percent);
			EXPECT_GE(d, 1u) << full << " " << percent;
			EXPECT_LE(d, full) << full << " " << percent;
			EXPECT_GE(d, full < kMinDimension ? full : kMinDimension) << full << " " << percent;
		}
	EXPECT_EQ(scaledDimension(0, 50), 0u);                          // nothing to scale
}

TEST(WorldRenderScale, TheAspectRatioIsPreservedOnEverySupportedSurface)
{
	const unsigned sizes[][2] = {{800, 600}, {1024, 768}, {1280, 720}, {1280, 1024}, {1920, 1080}, {1920, 1200}, {2560, 1440},
	                             {3440, 1440}, {5120, 1440}, {1111, 777}, {1777, 999}};
	for (const auto &s : sizes)
		for (unsigned percent : kChoices)
		{
			const Size low = internalSize(s[0], s[1], percent);
			const double a = static_cast<double>(s[0]) / s[1], b = static_cast<double>(low.width) / low.height;
			EXPECT_LT(std::fabs(a - b) / a, 0.01) << s[0] << "x" << s[1] << " " << percent;
		}
}

TEST(WorldRenderScale, LabelsShowTheEffectiveInternalResolution)
{
	EXPECT_EQ(label(75, 2560, 1440), "75% (1920 x 1080)");
	EXPECT_EQ(label(100, 1920, 1080), "100% (1920 x 1080)");
	EXPECT_EQ(label(50, 3440, 1440), "50% (1720 x 720)");
}

TEST(WorldRenderScale, AWorldViewportMapsIntoTheIntermediateTargetAndAlwaysCoversItself)
{
	const unsigned fullW = 2560, fullH = 1440;
	for (unsigned percent : kChoices)
	{
		const Size low = internalSize(fullW, fullH, percent);
		// the whole surface maps onto the whole target
		const Rect all = mapRect({0, 0, fullW, fullH}, fullW, fullH, low.width, low.height);
		EXPECT_EQ(all.x, 0u); EXPECT_EQ(all.y, 0u); EXPECT_EQ(all.w, low.width); EXPECT_EQ(all.h, low.height);
		// a sub rectangle (the tactical view above the control bar) stays inside and is never empty
		const Rect view = mapRect({0, 0, fullW, 1100}, fullW, fullH, low.width, low.height);
		EXPECT_GT(view.w, 0u); EXPECT_GT(view.h, 0u);
		EXPECT_LE(view.x + view.w, low.width);
		EXPECT_LE(view.y + view.h, low.height);
		// a one pixel strip at the very edge is still at least one pixel
		const Rect edge = mapRect({fullW - 1, fullH - 1, 1, 1}, fullW, fullH, low.width, low.height);
		EXPECT_GE(edge.w, 1u); EXPECT_GE(edge.h, 1u);
		EXPECT_LE(edge.x + edge.w, low.width);
	}
}

TEST(RenderScaleSettings, DefaultInvalidStoredValuesPersistenceAndTransitions)
{
	using namespace DisplayConfig;
	// default and invalid stored values
	KeyValues kv = {{"DisplayVersion", "1"}};
	EXPECT_EQ(load(kv, false).renderScalePercent, 100u);                 // no key: native
	kv["RenderScale"] = "abc";
	EXPECT_EQ(load(kv, false).renderScalePercent, 100u);
	kv["RenderScale"] = "-5";
	EXPECT_EQ(load(kv, false).renderScalePercent, 100u);
	kv["RenderScale"] = "73";
	EXPECT_EQ(load(kv, false).renderScalePercent, 75u);                  // snapped to an offered value
	EXPECT_EQ(load(kv, true).renderScalePercent, 100u);                  // Classic ignores stored values entirely
	// persistence round trip for every value
	for (unsigned percent : kChoices)
	{
		Settings s;
		s.explicitSettings = true;
		s.renderScalePercent = percent;
		KeyValues file;
		store(s, file);
		EXPECT_EQ(file["RenderScale"], std::to_string(percent));
		EXPECT_EQ(load(file, false).renderScalePercent, percent);
	}
	// 100 -> 50 -> 100: a safe field, persisted at once (no Keep / Revert), never touching the risky ones
	Settings stored;
	stored.explicitSettings = true;
	stored.uiScalePercent = 150;
	stored.mode = WindowMode::Borderless;
	Settings requested = stored;
	requested.renderScalePercent = 50;
	PersistPlan plan = planPersistence(stored, stored, requested);
	EXPECT_FALSE(plan.riskyChanged);
	EXPECT_EQ(plan.immediate.renderScalePercent, 50u);
	EXPECT_EQ(plan.immediate.uiScalePercent, 150);
	EXPECT_EQ(plan.immediate.mode, WindowMode::Borderless);
	Settings halved = stored;
	halved.renderScalePercent = 50;
	requested = halved;
	requested.renderScalePercent = 100;
	plan = planPersistence(halved, halved, requested);
	EXPECT_FALSE(plan.riskyChanged);
	EXPECT_EQ(plan.immediate.renderScalePercent, 100u);
}

TEST(RenderScaleSettings, RenderScaleAndUiScaleAreIndependentSystems)
{
	using namespace DisplayConfig;
	// UI scale is a function of the PRESENTATION size only: the render scale never enters it
	const DisplayGeometry::Size display = {2560, 1440};
	const DisplayGeometry::Layout reference = DisplayGeometry::computeLayout(display, DisplayGeometry::kAutoUiScale);
	for (unsigned percent : kChoices)
	{
		Settings s;
		s.renderScalePercent = percent;                                   // changing it ...
		const DisplayGeometry::Layout again = DisplayGeometry::computeLayout(display, s.uiScalePercent);
		EXPECT_EQ(again.uiScalePercent, reference.uiScalePercent) << percent;   // ... leaves the UI scale and the canvas alone
		EXPECT_EQ(again.canvas.width, reference.canvas.width);
		EXPECT_EQ(again.canvas.height, reference.canvas.height);
		// and the Enhanced HUD is laid out on the presentation size, so it does not grow when the world is rendered smaller
		const EnhancedHudLayout::Result hud = EnhancedHudLayout::compute(display.width, display.height, again.scale());
		const EnhancedHudLayout::Result hud100 = EnhancedHudLayout::compute(display.width, display.height, reference.scale());
		EXPECT_EQ(hud.island.w, hud100.island.w);
		EXPECT_EQ(hud.island.h, hud100.island.h);
	}
	// changing the UI scale does not change the render scale
	Settings stored;
	stored.renderScalePercent = 67;
	Settings requested = stored;
	requested.uiScalePercent = 200;
	const PersistPlan plan = planPersistence(stored, stored, requested);
	EXPECT_TRUE(plan.riskyChanged);                                        // the UI scale is risky (Keep / Revert) ...
	EXPECT_EQ(plan.afterKeep.renderScalePercent, 67u);                     // ... the render scale is untouched either way
	EXPECT_EQ(plan.immediate.renderScalePercent, 67u);
}

// ---- the per-picture lifecycle of the intermediate world target (X64.37 v4) ----------------------------------------------------------
//
// X64.37 v3 passed every arithmetic test and rendered a black world natively: the composite triangle was culled by the UI raster state
// (CULL_BACK, FrontCounterClockwise) - invisible to arithmetic. These tests cover the lifecycle decisions the backend delegates to
// WorldRenderScale::PassTracker; the GPU output itself (winding, raster state) is covered by the source contract
// test_modern_settings_contract.py::test_composite_triangle_is_not_culled and by the native retest, NOT by this harness.

namespace
{
// One picture the way the native renderer drives it: Begin_Render (colour clear + Begin_Scene), the world phase, the composite, UI, present.
struct Frame
{
	int clears = 0, composites = 0;
	WorldRenderScale::Rect composited;
};
Frame runPicture(WorldRenderScale::PassTracker &t, const WorldRenderScale::Rect *worldDraws, int count)
{
	Frame f;
	t.onColorClear();
	t.onBeginScene();
	for (int i = 0; i < count; ++i)
		f.clears += t.onWorldDraw(worldDraws[i].x, worldDraws[i].y, worldDraws[i].w, worldDraws[i].h) ? 1 : 0;
	WorldRenderScale::Rect out;
	if (t.takeComposite(out)) { ++f.composites; f.composited = out; }          // End_World_Phase / the first 2D submission
	WorldRenderScale::Rect again;
	EXPECT_FALSE(t.takeComposite(again));                                       // the UI that follows finds nothing pending
	t.onPresented();
	return f;
}
}  // namespace

TEST(WorldRenderScalePass, AWorldPictureClearsOnceAndCompositesOnceOverTheUnionOfItsViewports)
{
	WorldRenderScale::PassTracker t;
	const WorldRenderScale::Rect draws[] = {{0, 0, 1920, 1000}, {0, 0, 1920, 1000}, {100, 50, 400, 300}};
	const Frame f = runPicture(t, draws, 3);
	EXPECT_EQ(f.clears, 1);                       // the intermediate target is cleared by the first world draw only
	EXPECT_EQ(f.composites, 1);
	EXPECT_EQ(f.composited.x, 0u); EXPECT_EQ(f.composited.y, 0u);
	EXPECT_EQ(f.composited.w, 1920u); EXPECT_EQ(f.composited.h, 1000u);
}

TEST(WorldRenderScalePass, MainMenuGameMainMenuGameLeavesNothingPendingAndEveryPictureStartsClean)
{
	WorldRenderScale::PassTracker t;
	const WorldRenderScale::Rect menu[] = {{0, 0, 1920, 1200}};               // shell map behind the menu
	const WorldRenderScale::Rect game[] = {{0, 0, 1920, 1000}};               // tactical view above the control bar
	for (int round = 0; round < 3; ++round)
	{
		for (const auto *scene : {menu, game, menu, game})
		{
			const Frame f = runPicture(t, scene, 1);
			EXPECT_EQ(f.clears, 1) << round;
			EXPECT_EQ(f.composites, 1) << round;
			EXPECT_FALSE(t.active);                                                // no scaled target "active" after the composite
		}
		// a frontend picture with no world at all (a pure 2D screen) composites nothing
		t.onColorClear(); t.onBeginScene();
		WorldRenderScale::Rect none;
		EXPECT_FALSE(t.takeComposite(none));
		t.onPresented();
	}
}

TEST(WorldRenderScalePass, AbandonedOrInterruptedPicturesNeverLeakIntoTheNextOne)
{
	WorldRenderScale::PassTracker t;
	t.onBeginScene();
	t.onWorldDraw(0, 0, 800, 480);                // a picture whose composite never happened (device lost, scene change, exception)
	EXPECT_TRUE(t.active);
	t.onBeginScene();                             // the next Begin_Scene discards it
	EXPECT_FALSE(t.active);
	EXPECT_TRUE(t.fresh);
	// a colour clear after world draws (a new picture) discards what was pending
	t.onWorldDraw(0, 0, 800, 480);
	t.onColorClear();
	WorldRenderScale::Rect out;
	EXPECT_FALSE(t.takeComposite(out));
	// a resize / scale change resets everything
	t.onWorldDraw(0, 0, 800, 480);
	t.reset();
	EXPECT_FALSE(t.takeComposite(out));
	EXPECT_TRUE(t.fresh);
}

TEST(WorldRenderScalePass, ASecondWorldBatchAfterTheCompositeDoesNotClearTheFirst)
{
	// the engine never interleaves, but if it did the already composited content must not be wiped by a second clear
	WorldRenderScale::PassTracker t;
	t.onColorClear(); t.onBeginScene();
	EXPECT_TRUE(t.onWorldDraw(0, 0, 100, 100));
	WorldRenderScale::Rect out;
	EXPECT_TRUE(t.takeComposite(out));
	EXPECT_FALSE(t.onWorldDraw(50, 50, 100, 100));
	EXPECT_TRUE(t.takeComposite(out));
	EXPECT_EQ(out.x, 50u);
}

TEST(WorldRenderScalePass, ScaleTransitionsBetweenPicturesAreIndependent)
{
	// 100 -> 75 -> 100 -> 50 -> 100: the percent decides per submission whether the tracker is used at all; the tracker itself carries
	// no scale, so a transition can never leave a half-state behind.
	WorldRenderScale::PassTracker t;
	const WorldRenderScale::Rect draws[] = {{0, 0, 1920, 1000}};
	for (unsigned percent : {100u, 75u, 100u, 50u, 100u, 67u, 100u})
	{
		if (!WorldRenderScale::active(percent))
		{
			// the reference path: the tracker is never consulted, and it is idle
			EXPECT_FALSE(t.active) << percent;
			continue;
		}
		const Frame f = runPicture(t, draws, 1);
		EXPECT_EQ(f.composites, 1) << percent;
		t.reset();                                  // Set_World_Render_Scale releases the target
		EXPECT_TRUE(t.fresh);
		EXPECT_FALSE(t.active);
	}
}

TEST(WorldRenderScalePass, TheCompositeRectangleStaysInsideThePresentationSpaceItWasDrawnIn)
{
	WorldRenderScale::PassTracker t;
	t.onBeginScene();
	t.onWorldDraw(0, 0, 2560, 1100);
	WorldRenderScale::Rect out;
	ASSERT_TRUE(t.takeComposite(out));
	EXPECT_LE(out.x + out.w, 2560u);
	EXPECT_LE(out.y + out.h, 1440u);
	// and maps into every intermediate target size without leaving it
	for (unsigned percent : WorldRenderScale::kChoices)
	{
		const WorldRenderScale::Size low = WorldRenderScale::internalSize(2560, 1440, percent);
		const WorldRenderScale::Rect m = WorldRenderScale::mapRect(out, 2560, 1440, low.width, low.height);
		EXPECT_LE(m.x + m.w, low.width);
		EXPECT_LE(m.y + m.h, low.height);
	}
}
