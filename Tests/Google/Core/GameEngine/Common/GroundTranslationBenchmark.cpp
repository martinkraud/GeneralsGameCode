/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include "Common/GroundTranslation.h"
#include "Common/GroundTranslationPolicy.h"
#include "GameClient/Drawable.h"

namespace
{
volatile float benchmarkSink = 0;
// Materialize the complete returned matrix across a call boundary, rather than
// letting the caller discard every basis component. Includes harness call cost.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__)
__attribute__((noinline))
#endif
Matrix3D benchmarkRender(const GroundTranslationHistory& history, const Matrix3D& canonical,
	const PresentationTiming& timing, bool eligible, bool orientation = false)
{
	return history.getRenderTransform(canonical, timing, eligible, orientation);
}
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__)
__attribute__((noinline))
#endif
float benchmarkHeading(float previous, float current, float alpha)
{ return GroundTranslation::interpolateHeading(previous, current, alpha); }
struct KindView
{
	KindOfMaskType mask;
	bool isKindOf(KindOfType kind) const { return mask.test(kind); }
	bool isAnyKindOf(const KindOfMaskType& kinds) const { return mask.anyIntersectionWith(kinds); }
};
// Deliberately opt-in: wall-clock benchmarks are not correctness assertions.
TEST(GroundTranslationBenchmark, DISABLED_ReleaseCost)
{
#ifdef _DEBUG
	GTEST_SKIP() << "Release x86 measurements only";
#endif
	std::printf("MEMORY,history,%zu,Drawable,%zu,pointer,%zu\n",
		sizeof(GroundTranslationHistory), sizeof(Drawable), sizeof(void*));
	std::printf("BENCH,operation,count,iterations,repeats,median_ms,ns_per_object\n");
	const unsigned int iterations = 500;
	for (unsigned int count : {100u, 250u, 500u, 1000u, 2000u, 5000u})
	{
		std::vector<GroundTranslationHistory> history(count);
		std::vector<Matrix3D> canonical(count, Matrix3D(true));
		KindView kind; kind.mask.set(KINDOF_VEHICLE);
		GroundTranslation::Eligibility c = {true, true, false, true, true, AI_MOVE_TO, 1, true};
		PresentationTiming t = {10, 11, 1, 0.5f, true};
		for (unsigned int i = 0; i < count; ++i) { canonical[i].Rotate_Z(0.8f); canonical[i].Set_Translation(Vector3(float(i % 7), 2, 3)); }
		const char* operations[] = {"off_draw_branch", "off_hud_branch", "on_eligibility_policy",
			"on_capture_traversal", "on_render_matrix", "on_hud_selection_position", "on_combined", "on_heading_helper", "on_xyz_heading_matrix", "on_heading_capture"};
		for (unsigned int op = 0; op < 10; ++op)
		{
			GroundTranslation::setEnabled(op >= 2);
			std::vector<double> milliseconds;
			for (int repeat = 0; repeat < 5; ++repeat)
			{
				for (unsigned int i = 0; i < count; ++i)
				{
					history[i].reset();
					history[i].capture(canonical[i].Get_Translation() - Vector3(1, 0, 0), 10, 1, 0.2f, true);
					history[i].capture(canonical[i].Get_Translation(), 11, 1, canonical[i].Get_Z_Rotation(), true);
				}
				float checksum = 0;
				const auto start = std::chrono::steady_clock::now();
				for (unsigned int iteration = 0; iteration < iterations; ++iteration)
					for (unsigned int i = 0; i < count; ++i)
					{
						switch (op)
						{
						case 0:
							if (GroundTranslation::isEnabled()) checksum += history[i].getRenderTransform(canonical[i], t, true).Get_X_Translation();
							else checksum += canonical[i].Get_X_Translation();
							break;
						case 1:
							if (GroundTranslation::isEnabled()) checksum += history[i].getPresentationPosition(canonical[i].Get_Translation(), t, true).X;
							break;
						case 2:
							c.ordinaryGroundKind = GroundTranslation::supportsKinds(kind);
							checksum += GroundTranslation::canInterpolate(c) ? 1.0f : 0.0f;
							break;
						case 3:
							if (GroundTranslation::canInterpolate(c)) history[i].capture(canonical[i].Get_Translation(), iteration + 12, 1);
							checksum += canonical[i].Get_X_Translation();
							break;
						case 4: checksum += benchmarkRender(history[i], canonical[i], t, true).Get_X_Translation(); break;
						case 5: checksum += history[i].getPresentationPosition(canonical[i].Get_Translation(), t, true).X; break;
						case 7: checksum += benchmarkHeading(0.2f, 0.8f, t.alpha); break;
						case 8: checksum += benchmarkRender(history[i], canonical[i], t, true, true).Get_X_Translation(); break;
						case 9:
							history[i].capture(canonical[i].Get_Translation(), iteration + 12, 1, canonical[i].Get_Z_Rotation(), true);
							checksum += canonical[i].Get_X_Translation(); break;
						case 6:
							c.ordinaryGroundKind = GroundTranslation::supportsKinds(kind);
							checksum += benchmarkRender(history[i], canonical[i], t, GroundTranslation::canInterpolate(c)).Get_X_Translation();
							break;
						}
					}
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
				benchmarkSink = checksum;
				milliseconds.push_back(ms);
			}
			std::sort(milliseconds.begin(), milliseconds.end());
			const double median = milliseconds[2];
			std::printf("BENCH,%s,%u,%u,5,%.6f,%.3f\n", operations[op], count, iterations,
				median, median * 1000000.0 / (double(count) * iterations));
		}
	}
	GroundTranslation::setEnabled(false);
	EXPECT_EQ(sizeof(void*), 4u);
	EXPECT_EQ(sizeof(GroundTranslationHistory), 48u);
}
}
