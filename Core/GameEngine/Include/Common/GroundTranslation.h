/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "Common/PresentationClock.h"
#include "WWMath/matrix3d.h"
#include <cmath>

namespace GroundTranslation
{
	inline bool supportsRootDraw(bool knownModule, bool attachedRoot) { return knownModule && !attachedRoot; }
	inline bool supportsRootOrientationDraw(bool knownModule, bool attachedRoot, bool articulated)
	{ return supportsRootDraw(knownModule, attachedRoot) && !articulated; }
	inline bool validHeading(float heading)
	{ return std::isfinite(heading) && std::fabs(heading) <= WWMATH_TWO_PI; }
	inline float interpolateHeading(float previous, float current, float alpha)
	{
		if (!validHeading(previous) || !validHeading(current) || !std::isfinite(alpha)) return current;
		const float a = alpha < 0 ? 0 : (alpha > 1 ? 1 : alpha);
		return WWMath::Normalize_Angle(previous + WWMath::Normalize_Angle(current - previous) * a);
	}
	inline bool supportsHeadingBasis(const Matrix3D& matrix)
	{
		for (int row = 0; row < 3; ++row)
			for (int col = 0; col < 3; ++col)
				if (!std::isfinite(matrix[row][col])) return false;
		return matrix[0][0] * matrix[0][0] + matrix[1][0] * matrix[1][0] > 0.000001f;
	}
	bool isEnabled();
	void setEnabled(bool enabled); // Process-local developer gate; default off, never saved.
	const float MaxSampleDistance = 12.0f; // Conservative fallback, not a semantic teleport flag.
}

// Owned directly by one Drawable, reconstructed with every pooled lifetime.
// No Object references, address-keyed registry, snapshot or CRC participation.
class GroundTranslationHistory
{
public:
	GroundTranslationHistory() { reset(); }
	void reset()
	{
		m_previous.Set(0, 0, 0);
		m_current.Set(0, 0, 0);
		m_previousGeneration = m_generation = m_epoch = 0;
		m_haveSample = m_havePair = false;
		resetOrientation();
	}
	void resetOrientation()
	{
		m_previousHeading = m_currentHeading = 0;
		m_haveHeading = m_haveHeadingPair = false;
	}
	void observeEpoch(unsigned int epoch)
	{
		if (m_haveSample && epoch != m_epoch)
			reset();
	}
	void capture(const Vector3& position, unsigned int generation, unsigned int epoch,
		float heading = 0, bool orientationEligible = false)
	{
		observeEpoch(epoch);
		if (m_haveSample && generation == m_generation)
			return; // Multiple client/views cannot rotate the same completed sample.
		const bool consecutive = m_haveSample && generation > m_generation
			&& generation == m_generation + 1;
		const bool smallStep = m_haveSample && (position - m_current).Length2()
			<= GroundTranslation::MaxSampleDistance * GroundTranslation::MaxSampleDistance;
		m_havePair = consecutive && smallStep;
		const bool haveHeading = orientationEligible && GroundTranslation::validHeading(heading);
		m_haveHeadingPair = m_havePair && m_haveHeading && haveHeading;
		m_previousHeading = m_haveHeadingPair ? m_currentHeading : heading;
		m_currentHeading = heading;
		m_haveHeading = haveHeading;
		m_previous = m_havePair ? m_current : position;
		m_previousGeneration = m_havePair ? m_generation : generation;
		m_current = position;
		m_generation = generation;
		m_epoch = epoch;
		m_haveSample = true;
	}
	Matrix3D getRenderTransform(const Matrix3D& canonical, const PresentationTiming& timing, bool eligible, bool orientationEligible = false) const
	{
		Matrix3D result = canonical;
		if (!eligible || !timing.valid || !m_havePair || timing.epoch != m_epoch
			|| timing.generation != m_generation || timing.previousGeneration != m_previousGeneration
			|| canonical.Get_Translation() != m_current || timing.alpha != timing.alpha)
			return result;
		const float alpha = timing.alpha < 0.0f ? 0.0f : (timing.alpha > 1.0f ? 1.0f : timing.alpha);
		Vector3 position;
		Vector3::Lerp(m_previous, m_current, alpha, &position);
		result.Set_Translation(position);
		if (orientationEligible && m_haveHeadingPair && GroundTranslation::supportsHeadingBasis(canonical)
			&& canonical.Get_Z_Rotation() == m_currentHeading && alpha < 1.0f)
		{
			const float heading = GroundTranslation::interpolateHeading(m_previousHeading, m_currentHeading, alpha);
			result.In_Place_Pre_Rotate_Z(WWMath::Normalize_Angle(heading - m_currentHeading));
		} // Only a local copy; current residual basis/decorations and canonical state stay intact.
		return result;
	}
	Vector3 getPresentationPosition(const Vector3& canonical, const PresentationTiming& timing, bool eligible) const
	{
		Matrix3D root(true);
		root.Set_Translation(canonical);
		return getRenderTransform(root, timing, eligible).Get_Translation();
	}
private:
	Vector3 m_previous;
	Vector3 m_current;
	float m_previousHeading;
	float m_currentHeading;
	unsigned int m_previousGeneration;
	unsigned int m_generation;
	unsigned int m_epoch;
	bool m_haveSample;
	bool m_havePair;
	bool m_haveHeading;
	bool m_haveHeadingPair;
};
