/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
// X64.34: presentation smoothness contracts (articulation history, airborne step limits, render-rate independence,
// authoritative equivalence) and the performance receipt.
#include <gtest/gtest.h>

#include <cmath>
#include <sstream>
#include <vector>

#include "Common/GroundTranslation.h"
#include "Common/PerformanceProfile.h"

namespace
{
PresentationTiming timing(unsigned previous, unsigned generation, float alpha, unsigned epoch = 1, bool valid = true)
{
	PresentationTiming t = {};
	t.previousGeneration = previous;
	t.generation = generation;
	t.epoch = epoch;
	t.alpha = alpha;
	t.valid = valid;
	return t;
}
const float kPi = 3.14159265358979f;
}

// ---- articulation (turret yaw / barrel pitch) --------------------------------------------------------------------

TEST(AnglePairHistory, InterpolatesBetweenCompletedSamples)
{
	AnglePairHistory h;
	h.observe(0.0f, 0.0f, 10, 1);
	h.observe(0.2f, 0.1f, 11, 1);
	float yaw = -1, pitch = -1;
	ASSERT_TRUE(h.present(timing(10, 11, 0.5f), 0.2f, 0.1f, yaw, pitch));
	EXPECT_NEAR(yaw, 0.1f, 1e-6f);
	EXPECT_NEAR(pitch, 0.05f, 1e-6f);
	ASSERT_TRUE(h.present(timing(10, 11, 1.0f), 0.2f, 0.1f, yaw, pitch));
	EXPECT_NEAR(yaw, 0.2f, 1e-6f);               // alpha 1 is exactly the canonical sample
	ASSERT_TRUE(h.present(timing(10, 11, 0.0f), 0.2f, 0.1f, yaw, pitch));
	EXPECT_NEAR(yaw, 0.0f, 1e-6f);
}

TEST(AnglePairHistory, YawTakesTheShortestPathAcrossTheWrap)
{
	AnglePairHistory h;
	h.observe(kPi - 0.05f, 0, 5, 1);
	h.observe(-kPi + 0.05f, 0, 6, 1);          // crossed +pi -> -pi: a 0.1 rad turn, not a 2pi sweep
	float yaw = 0, pitch = 0;
	ASSERT_TRUE(h.present(timing(5, 6, 0.5f), -kPi + 0.05f, 0, yaw, pitch));
	EXPECT_GT(std::fabs(yaw), kPi - 0.06f);    // stays near +-pi, never near 0
}

TEST(AnglePairHistory, SnapsInsteadOfSweeping)
{
	AnglePairHistory h;
	float yaw = 0, pitch = 0;
	h.observe(0.0f, 0.0f, 1, 1);
	h.observe(2.5f, 0.0f, 2, 1);               // implausible single-tick turret change (state reset)
	EXPECT_FALSE(h.present(timing(1, 2, 0.5f), 2.5f, 0.0f, yaw, pitch));
	h.observe(2.5f, 0.0f, 4, 1);               // generation gap
	EXPECT_FALSE(h.present(timing(3, 4, 0.5f), 2.5f, 0.0f, yaw, pitch));
	h.observe(2.55f, 0.0f, 5, 1);              // consecutive again: interpolation resumes
	EXPECT_TRUE(h.present(timing(4, 5, 0.5f), 2.55f, 0.0f, yaw, pitch));
	h.observe(2.6f, 0.0f, 6, 2);               // epoch change (load/map change) resets
	EXPECT_FALSE(h.present(timing(5, 6, 0.5f, 2), 2.6f, 0.0f, yaw, pitch));
}

TEST(AnglePairHistory, RefusesInvalidTimingStaleCanonicalAndNonFinite)
{
	AnglePairHistory h;
	float yaw = 7, pitch = 7;
	h.observe(0.0f, 0.0f, 1, 1);
	h.observe(0.1f, 0.0f, 2, 1);
	EXPECT_FALSE(h.present(timing(1, 2, 0.5f, 1, false), 0.1f, 0.0f, yaw, pitch));            // paused/stalled timing
	EXPECT_FALSE(h.present(timing(1, 2, std::nanf(""), 1, true), 0.1f, 0.0f, yaw, pitch));
	EXPECT_FALSE(h.present(timing(1, 2, 0.5f), 0.3f, 0.0f, yaw, pitch));                       // canonical moved on
	EXPECT_FALSE(h.present(timing(2, 3, 0.5f), 0.1f, 0.0f, yaw, pitch));                       // wrong generation
	EXPECT_EQ(yaw, 7.0f);                                                                       // outputs untouched
	h.observe(std::nanf(""), 0.0f, 3, 1);                                                       // non-finite resets history
	EXPECT_FALSE(h.present(timing(2, 3, 0.5f), 0.1f, 0.0f, yaw, pitch));
}

TEST(AnglePairHistory, AlphaIsClampedNeverExtrapolated)
{
	AnglePairHistory h;
	h.observe(0.0f, 0.0f, 1, 1);
	h.observe(0.2f, 0.0f, 2, 1);
	float yaw = 0, pitch = 0;
	ASSERT_TRUE(h.present(timing(1, 2, 3.0f), 0.2f, 0.0f, yaw, pitch));    // a stall cannot push past the sample
	EXPECT_NEAR(yaw, 0.2f, 1e-6f);
	ASSERT_TRUE(h.present(timing(1, 2, -2.0f), 0.2f, 0.0f, yaw, pitch));
	EXPECT_NEAR(yaw, 0.0f, 1e-6f);
}

// ---- airborne classes: larger legitimate steps, same discontinuity rules ----------------------------------------

TEST(GroundTranslationHistory, AirborneStepLimitsAreDistinctAndRelocationsSnap)
{
	const Vector3 a(0, 0, 100), farAircraft(45, 0, 100), relocatedAircraft(150, 0, 100);
	// 45 units per tick: a relocation for a ground unit, a normal step for an aircraft
	GroundTranslationHistory ground;
	ground.capture(a, 1, 1);
	ground.capture(farAircraft, 2, 1);
	EXPECT_FALSE(ground.getPresentationPosition(farAircraft, timing(1, 2, 0.5f), true) != farAircraft);
	GroundTranslationHistory air;
	air.capture(a, 1, 1, 0, false, GroundTranslation::MaxAircraftSampleDistance);
	air.capture(farAircraft, 2, 1, 0, false, GroundTranslation::MaxAircraftSampleDistance);
	const Vector3 mid = air.getPresentationPosition(farAircraft, timing(1, 2, 0.5f), true);
	EXPECT_NEAR(mid.X, 22.5f, 1e-4f);
	// a 150 unit jump is a relocation even for an aircraft: no interpolation across the map
	GroundTranslationHistory jump;
	jump.capture(a, 1, 1, 0, false, GroundTranslation::MaxAircraftSampleDistance);
	jump.capture(relocatedAircraft, 2, 1, 0, false, GroundTranslation::MaxAircraftSampleDistance);
	EXPECT_TRUE(jump.getPresentationPosition(relocatedAircraft, timing(1, 2, 0.5f), true) == relocatedAircraft);
	// projectiles tolerate more per tick, but not unbounded
	EXPECT_GT(GroundTranslation::MaxProjectileSampleDistance, GroundTranslation::MaxAircraftSampleDistance);
	EXPECT_GT(GroundTranslation::MaxAircraftSampleDistance, GroundTranslation::MaxSampleDistance);
}

// ---- render-rate independence and authoritative equivalence ------------------------------------------------------

namespace
{
struct RunResult
{
	std::vector<float> authoritativeX;      // canonical position at each completed tick
	std::vector<float> presented;           // presented X at every render
	std::vector<float> renderTimes;         // authoritative X at each render
};

// Fixed 30 TPS simulation; renders at `fps`. Position advances 2.0 units per tick (authoritative). The presented
// value comes from the production history and an alpha derived exactly like the scheduler's remainder / period.
RunResult run(int fps, bool smooth, int ticks)
{
	RunResult out;
	const double period = 1.0 / 30.0, frame = 1.0 / fps;
	double accumulator = 0, now = 0;
	unsigned generation = 0;
	float x = 0;
	GroundTranslationHistory history;
	const double endTime = ticks * period;
	while (now < endTime)
	{
		now += frame;
		accumulator += frame;
		bool ticked = false;
		while (accumulator >= period)
		{
			accumulator -= period;
			++generation;
			x += 2.0f;                                   // authoritative simulation step: independent of render rate
			out.authoritativeX.push_back(x);
			history.capture(Vector3(x, 0, 0), generation, 1);
			ticked = true;
		}
		(void)ticked;
		float shown = x;
		if (smooth && generation >= 2)
		{
			const float alpha = static_cast<float>(accumulator / period);
			shown = history.getPresentationPosition(Vector3(x, 0, 0), timing(generation - 1, generation, alpha), true).X;
		}
		out.presented.push_back(shown);
		out.renderTimes.push_back(x);   // authoritative position at this render
	}
	return out;
}
}

TEST(PresentationSmoothness, AuthoritativeStateIsIdenticalForEveryRenderRateAndMode)
{
	const int rates[] = {30, 60, 120, 144, 165, 240, 1000};
	const RunResult reference = run(30, false, 120);
	for (int fps : rates)
		for (bool smooth : {false, true})
		{
			const RunResult r = run(fps, smooth, 120);
			// the simulation produced exactly the same sequence of authoritative positions
			ASSERT_GE(r.authoritativeX.size(), 119u);
			for (size_t i = 0; i < 119; ++i)
				EXPECT_EQ(r.authoritativeX[i], reference.authoritativeX[i]) << fps << " smooth=" << smooth << " tick " << i;
		}
}

TEST(PresentationSmoothness, PresentedMotionIsMonotonicBoundedAndFinerAtHigherRates)
{
	size_t previousSteps = 0;
	for (int fps : {30, 60, 120, 144, 165, 240})
	{
		const RunResult r = run(fps, true, 90);
		float last = -1;
		size_t distinct = 0;
		for (size_t i = 0; i < r.presented.size(); ++i)
		{
			EXPECT_GE(r.presented[i], last - 1e-4f) << "presentation went backwards at " << fps << " fps";
			// never ahead of the authoritative position (no extrapolation) and at most one tick behind it
			const float authoritative = r.renderTimes[i];
			EXPECT_LE(r.presented[i], authoritative + 1e-3f);
			EXPECT_GE(r.presented[i], authoritative - 2.0f - 1e-3f);
			if (r.presented[i] != last) ++distinct;
			last = r.presented[i];
		}
		EXPECT_GE(distinct, previousSteps) << fps;   // more render frames never produce fewer distinct positions
		previousSteps = distinct;
	}
}

TEST(PresentationSmoothness, StallAndPauseNeverRunAhead)
{
	GroundTranslationHistory history;
	history.capture(Vector3(0, 0, 0), 1, 1);
	history.capture(Vector3(2, 0, 0), 2, 1);
	// a long frame: alpha would be 7.3 -> clamped to the current sample
	EXPECT_NEAR(history.getPresentationPosition(Vector3(2, 0, 0), timing(1, 2, 7.3f), true).X, 2.0f, 1e-5f);
	// pause/load/menu: timing invalid -> canonical, no catch-up flight
	EXPECT_NEAR(history.getPresentationPosition(Vector3(2, 0, 0), timing(1, 2, 0.5f, 1, false), true).X, 2.0f, 1e-5f);
	// the world advanced three ticks while rendering was blocked: no consecutive pair, canonical only
	history.capture(Vector3(8, 0, 0), 5, 1);
	EXPECT_NEAR(history.getPresentationPosition(Vector3(8, 0, 0), timing(4, 5, 0.5f), true).X, 8.0f, 1e-5f);
}

// ---- performance receipt -----------------------------------------------------------------------------------------

TEST(PerformanceReceipt, ReportsFramePercentilesLogicCatchUpAndRenderCounters)
{
	using namespace PerformanceProfile;
	activeRecorder = nullptr;
	Recorder r(64);
	r.start(0);
	Tick t = 0;
	for (int i = 0; i < 20; ++i)
	{
		ASSERT_TRUE(r.beginFrame(t, i, 144, 144));
		activeRecorder = &r;
		const Tick start = t;
		// frames of 10 ticks, one 50-tick spike; every 5th frame runs two logic ticks with a 30-tick logic scope
		const Tick length = i == 7 ? 50 : 10;
		if (i % 5 == 0)
		{
			r.push(Category::Logic, t + 1);
			r.add(Counter::CompletedTicks, 2);
			r.pop(t + 31);
		}
		r.add(Counter::DrawCalls, 100 + i);
		r.add(Counter::DrawablesDrawn, 50);
		r.add(Counter::ObjectsLive, 200);
		r.add(Counter::ParticlesLive, i == 3 ? 900 : 100);
		activeRecorder = nullptr;
		t = start + length;
		r.endFrame(t, i);
	}
	r.stop();

	Metadata meta;
	meta.frequency = 1000;   // 1 tick = 1 ms
	meta.title = "test";
	std::ostringstream summary, frames, categories, slow;
	report(r, meta, summary, frames, categories, slow);
	const std::string text = summary.str();
	EXPECT_NE(text.find("=== Generals Performance Receipt ==="), std::string::npos);
	EXPECT_NE(text.find("frames: 20"), std::string::npos);
	EXPECT_NE(text.find("frame_ms: avg"), std::string::npos);
	EXPECT_NE(text.find("max 50"), std::string::npos);                 // the spike frame
	EXPECT_NE(text.find("catch_up_frames: 4"), std::string::npos);     // frames 0,5,10,15 completed two ticks
	EXPECT_NE(text.find("logic_ticks: 8"), std::string::npos);
	EXPECT_NE(text.find("peak_draw_calls 119"), std::string::npos);
	EXPECT_NE(text.find("peak_particles 900"), std::string::npos);
	EXPECT_NE(text.find("peak_objects 200"), std::string::npos);
	EXPECT_NE(text.find("=== end receipt ==="), std::string::npos);
	// counters are named in the per-frame CSV header
	EXPECT_NE(frames.str().find("draw_calls"), std::string::npos);
	EXPECT_NE(frames.str().find("particles_live_gauge"), std::string::npos);
}

TEST(PerformanceReceipt, CounterNamesAreCompleteAndUnique)
{
	using namespace PerformanceProfile;
	for (unsigned a = 0; a < CounterCount; ++a)
	{
		ASSERT_NE(name(static_cast<Counter>(a)), nullptr);
		for (unsigned b = a + 1; b < CounterCount; ++b)
			EXPECT_STRNE(name(static_cast<Counter>(a)), name(static_cast<Counter>(b)));
	}
}

TEST(PerformanceReceipt, ReportsRendererStateCountersAndHardwareContext)
{
	using namespace PerformanceProfile;
	activeRecorder = nullptr;
	Recorder r(8);
	r.start(0);
	for (int i = 0; i < 4; ++i)
	{
		ASSERT_TRUE(r.beginFrame(i * 10, i, 60, 60));
		activeRecorder = &r;
		r.add(Counter::Draws2D, 10);
		r.add(Counter::DrawsWorld, 90);
		r.add(Counter::Triangles, 5000);
		r.add(Counter::StateRequested, 1800);
		r.add(Counter::StateChanged, 300);
		r.add(Counter::TextureBinds, 40);
		activeRecorder = nullptr;
		r.endFrame(i * 10 + 10, i);
	}
	r.stop();
	Metadata meta;
	meta.frequency = 1000;
	meta.hardware = "cpu=test; logical_cores=14; ram_mb=16384";
	meta.renderContext = "adapter=[Test GPU]; client=1280x720; vsync=0";
	std::ostringstream summary, frames, categories, slow;
	report(r, meta, summary, frames, categories, slow);
	const std::string text = summary.str();
	EXPECT_NE(text.find("avg_draws_2d 10"), std::string::npos);
	EXPECT_NE(text.find("avg_draws_world 90"), std::string::npos);
	EXPECT_NE(text.find("avg_triangles 5000"), std::string::npos);
	EXPECT_NE(text.find("avg_requested 1800 avg_changed 300"), std::string::npos);
	EXPECT_NE(text.find("avg_texture_binds 40"), std::string::npos);
	EXPECT_NE(text.find("hardware: cpu=test; logical_cores=14; ram_mb=16384"), std::string::npos);
	EXPECT_NE(text.find("render_context: adapter=[Test GPU]"), std::string::npos);
}

TEST(PerformanceReceipt, ReportsPresentSubmitGpuTextureAndMemoryFields)
{
	using namespace PerformanceProfile;
	activeRecorder = nullptr;
	Recorder r(8);
	r.start(0);
	for (int i = 0; i < 4; ++i)
	{
		ASSERT_TRUE(r.beginFrame(i * 10, i, 1000000, 1000000));
		activeRecorder = &r;
		r.add(Counter::SubmitTicks, 2);
		r.add(Counter::PresentTicks, 4);
		r.add(Counter::UploadTicks, 1);
		r.add(Counter::GpuMicros, 3000);   // 3 ms
		r.add(Counter::GpuFrames, 1);
		r.add(Counter::TexturesCreated, 2);
		r.add(Counter::TexturesDestroyed, 2);
		r.add(Counter::TexturesLive, 500 + i);
		r.add(Counter::PageFaults, 10);
		r.add(Counter::WorkingSetMb, 900 + i);
		r.add(Counter::PrivateMb, 700);
		activeRecorder = nullptr;
		r.endFrame(i * 10 + 10, i);
	}
	r.stop();
	Metadata meta;
	meta.frequency = 1000;
	meta.devFps = "uncapped (-devFps)";
	meta.extension = "\n=== Texture creation attribution (this capture) ===\n=== end texture attribution ===\n";
	std::ostringstream summary, frames, categories, slow;
	report(r, meta, summary, frames, categories, slow);
	const std::string text = summary.str();
	EXPECT_NE(text.find("fps_cap_override: uncapped (-devFps)"), std::string::npos);
	EXPECT_NE(text.find("submit 2 present 4 texture_upload 1"), std::string::npos);
	EXPECT_NE(text.find("frames_measured 4 avg_gpu_ms 3"), std::string::npos);
	EXPECT_NE(text.find("textures: created 8 destroyed 8 live_end 503"), std::string::npos);
	EXPECT_NE(text.find("page_faults 40 peak_faults_per_frame 10 working_set_mb_end 903"), std::string::npos);
	EXPECT_NE(text.find("=== Texture creation attribution"), std::string::npos);
	EXPECT_NE(text.find("=== end receipt ==="), std::string::npos);
	// the previous receipt fields are all still present
	for (const char *field : {"avg_draw_calls", "avg_draws_world", "avg_requested", "avg_texture_binds", "hardware:", "render_context:", "logic_ticks:"})
		EXPECT_NE(text.find(field), std::string::npos) << field;
}
