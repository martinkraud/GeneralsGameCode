/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "Common/PresentationClock.h"
#include "WWMath/matrix3d.h"

namespace GroundTranslation
{
	inline bool supportsRootDraw(bool knownModule, bool attachedRoot) { return knownModule && !attachedRoot; }
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
	}
	void observeEpoch(unsigned int epoch)
	{
		if (m_haveSample && epoch != m_epoch)
			reset();
	}
	void capture(const Vector3& position, unsigned int generation, unsigned int epoch)
	{
		observeEpoch(epoch);
		if (m_haveSample && generation == m_generation)
			return; // Multiple client/views cannot rotate the same completed sample.
		const bool consecutive = m_haveSample && generation > m_generation
			&& generation == m_generation + 1;
		const bool smallStep = m_haveSample && (position - m_current).Length2()
			<= GroundTranslation::MaxSampleDistance * GroundTranslation::MaxSampleDistance;
		m_havePair = consecutive && smallStep;
		m_previous = m_havePair ? m_current : position;
		m_previousGeneration = m_havePair ? m_generation : generation;
		m_current = position;
		m_generation = generation;
		m_epoch = epoch;
		m_haveSample = true;
	}
	Matrix3D getRenderTransform(const Matrix3D& canonical, const PresentationTiming& timing, bool eligible) const
	{
		Matrix3D result = canonical;
		if (!eligible || !timing.valid || !m_havePair || timing.epoch != m_epoch
			|| timing.generation != m_generation || timing.previousGeneration != m_previousGeneration
			|| canonical.Get_Translation() != m_current || timing.alpha != timing.alpha)
			return result;
		const float alpha = timing.alpha < 0.0f ? 0.0f : (timing.alpha > 1.0f ? 1.0f : timing.alpha);
		Vector3 position;
		Vector3::Lerp(m_previous, m_current, alpha, &position);
		result.Set_Translation(position); // Only a local copy; basis and canonical state stay intact.
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
	unsigned int m_previousGeneration;
	unsigned int m_generation;
	unsigned int m_epoch;
	bool m_haveSample;
	bool m_havePair;
};
