/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

// Presentation-only, never serialized or CRC'd. Generation IDs are GameLogic
// completed frame numbers, scoped by epoch. Invalid timing means use current
// canonical state (alpha=1), not a stale cached pair.
struct PresentationTiming
{
	unsigned int previousGeneration;
	unsigned int generation;
	unsigned int epoch;
	float alpha; // 0=previous completed sample, 1=current completed sample.
	bool valid; // A consecutive completed pair in one continuous timing epoch.
};

// Deterministic observer: no timer, scheduler decisions, Object state or RNG.
// FramePacer supplies one scheduler observation and the subsequent measured
// delta per outer iteration, after the world update has actually completed.
class PresentationClock
{
public:
	PresentationClock()
	{
		m_timing.epoch = 0;
		reset();
	}

	const PresentationTiming& getTiming() const { return m_timing; }

	void reset()
	{
		++m_timing.epoch;
		m_timing.previousGeneration = 0;
		m_timing.generation = 0;
		m_timing.alpha = 1.0f;
		m_timing.valid = false;
		m_observed = false;
		m_continuous = false;
		m_haveSample = false;
		m_pairReady = false;
		m_period = 0.0f;
		m_remainder = 0.0f;
		m_observedPeriod = 0.0f;
	}

	// Called after the existing scheduler decision (and subtraction, if any).
	// period=0 explicitly denotes a branch without usable accumulator timing.
	void observeScheduler(float remainder, float period)
	{
		m_remainder = remainder;
		m_observedPeriod = period;
		m_observed = true;
	}

	void advance(unsigned int frame, bool completed, float nextDelta, bool permitted)
	{
		const bool observed = m_observed;
		m_observed = false; // An observation is consumed exactly once.
		if (!permitted || !observed || !(m_observedPeriod > 0.0f && m_observedPeriod < 1.0f)
			|| !(m_remainder >= 0.0f && m_remainder < m_observedPeriod)
			|| !(nextDelta >= 0.0f && nextDelta < m_observedPeriod))
		{
			invalidate();
			m_timing.previousGeneration = m_timing.generation = frame;
			return;
		}

		if (m_continuous && m_period != m_observedPeriod)
			invalidate();
		m_continuous = true;
		m_period = m_observedPeriod;

		if (completed)
		{
			// Includes rewind, repeated completion, skipped frames and unsigned wrap.
			if (m_haveSample && (frame <= m_timing.generation || frame != m_timing.generation + 1))
			{
				invalidate();
				m_continuous = true;
			}
			m_timing.previousGeneration = m_haveSample ? m_timing.generation : frame;
			m_pairReady = m_haveSample;
			m_timing.generation = frame;
			m_haveSample = true;
		}
		else if (m_haveSample && frame != m_timing.generation)
		{
			invalidate();
			m_timing.previousGeneration = m_timing.generation = frame;
			return;
		}
		else if (!m_haveSample)
		{
			m_timing.previousGeneration = m_timing.generation = frame;
		}

		m_timing.valid = m_pairReady;
		const float phase = (m_remainder + nextDelta) / m_period;
		m_timing.alpha = m_pairReady && phase < 1.0f ? phase : 1.0f;
	}

private:
	void invalidate()
	{
		if (m_continuous)
			++m_timing.epoch;
		m_continuous = false;
		m_haveSample = false;
		m_pairReady = false;
		m_timing.valid = false;
		m_timing.alpha = 1.0f;
	}

	PresentationTiming m_timing;
	float m_remainder;
	float m_observedPeriod;
	float m_period;
	bool m_observed;
	bool m_continuous;
	bool m_haveSample;
	bool m_pairReady;
};
