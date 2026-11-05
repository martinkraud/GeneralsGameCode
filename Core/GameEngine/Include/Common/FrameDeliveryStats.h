// SPDX-License-Identifier: GPL-3.0-or-later
// Pure statistics for the frame-delivery telemetry (see FrameDeliveryTelemetry.h). No engine dependencies: unit-testable.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace FrameDelivery
{
struct Percentiles
{
	unsigned count = 0;
	double mean = 0.0, p50 = 0.0, p95 = 0.0, p99 = 0.0, max = 0.0;
};

// Nearest-rank percentiles over a copy of the samples. Empty input yields zeros.
inline Percentiles summarize(std::vector<float> samples)
{
	Percentiles r;
	if (samples.empty())
		return r;
	std::sort(samples.begin(), samples.end());
	const size_t n = samples.size();
	double sum = 0.0;
	for (float v : samples)
		sum += v;
	auto at = [&](double q) {
		size_t rank = static_cast<size_t>(std::ceil(q * static_cast<double>(n)));
		if (rank < 1) rank = 1;
		if (rank > n) rank = n;
		return static_cast<double>(samples[rank - 1]);
	};
	r.count = static_cast<unsigned>(n);
	r.mean = sum / static_cast<double>(n);
	r.p50 = at(0.50);
	r.p95 = at(0.95);
	r.p99 = at(0.99);
	r.max = static_cast<double>(samples.back());
	return r;
}

// One outer-loop (render) iteration as seen after FramePacer::update().
struct FrameSample
{
	float intervalMs = 0.0f;   ///< measured render interval (limiter-paced)
	unsigned ticks = 0;        ///< completed logic ticks in this iteration (0 or 1 in production)
	bool alphaValid = false;   ///< PresentationTiming::valid
	float alpha = 1.0f;        ///< PresentationTiming::alpha
	unsigned epoch = 0;        ///< PresentationTiming::epoch
	bool stalled = false;      ///< network waiting for other players' commands
};

// Accumulates one reporting window.
class Window
{
public:
	void reset() { *this = Window(); }

	void addFrame(const FrameSample &f)
	{
		++m_frames;
		m_intervals.push_back(f.intervalMs);
		m_timeMs += f.intervalMs;
		if (f.ticks == 0) ++m_ticks0;
		else if (f.ticks == 1) ++m_ticks1;
		else ++m_ticks2plus;
		if (f.alphaValid) ++m_validFrames;
		if (f.stalled) ++m_stalledFrames;
		if (m_haveEpoch && f.epoch != m_lastEpoch) ++m_epochChanges;
		m_lastEpoch = f.epoch;
		m_haveEpoch = true;
		// Frames in which presentation cannot advance although a tick is not due: alpha pinned at 1 or invalid.
		if (!f.alphaValid || f.alpha >= 1.0f) ++m_frozenFrames;
		// Alpha step monotonicity: a decrease within one valid epoch without a tick is a presentation bug.
		if (f.alphaValid && m_prevValid && f.ticks == 0 && f.epoch == m_prevEpoch && f.alpha + 1e-6f < m_prevAlpha) ++m_alphaRegressions;
		m_prevValid = f.alphaValid;
		m_prevAlpha = f.alpha;
		m_prevEpoch = f.epoch;
	}

	// Tick start/end are timestamps in ms on one monotonic clock.
	void addTick(double startMs, double durationMs)
	{
		m_tickDurations.push_back(static_cast<float>(durationMs));
		if (m_haveLastTick)
			m_tickSpacing.push_back(static_cast<float>(startMs - m_lastTickStartMs));
		m_lastTickStartMs = startMs;
		m_haveLastTick = true;
	}

	unsigned frames() const { return m_frames; }
	double timeMs() const { return m_timeMs; }
	unsigned ticks() const { return static_cast<unsigned>(m_tickDurations.size()); }
	unsigned framesWithTicks(unsigned n) const { return n == 0 ? m_ticks0 : (n == 1 ? m_ticks1 : m_ticks2plus); }
	unsigned validFrames() const { return m_validFrames; }
	unsigned frozenFrames() const { return m_frozenFrames; }
	unsigned stalledFrames() const { return m_stalledFrames; }
	unsigned epochChanges() const { return m_epochChanges; }
	unsigned alphaRegressions() const { return m_alphaRegressions; }
	double validPercent() const { return m_frames ? 100.0 * m_validFrames / m_frames : 0.0; }
	double renderFps() const { return m_timeMs > 0.0 ? 1000.0 * m_frames / m_timeMs : 0.0; }
	double tickRate() const { return m_timeMs > 0.0 ? 1000.0 * m_tickDurations.size() / m_timeMs : 0.0; }
	Percentiles interval() const { return summarize(m_intervals); }
	Percentiles tickDuration() const { return summarize(m_tickDurations); }
	Percentiles tickSpacing() const { return summarize(m_tickSpacing); }
	// Frames whose interval exceeds 1.5x the window median: the visible hitches.
	unsigned hitches() const
	{
		const double limit = interval().p50 * 1.5;
		unsigned n = 0;
		for (float v : m_intervals)
			if (v > limit) ++n;
		return n;
	}

private:
	unsigned m_frames = 0, m_ticks0 = 0, m_ticks1 = 0, m_ticks2plus = 0, m_validFrames = 0, m_frozenFrames = 0, m_stalledFrames = 0;
	unsigned m_epochChanges = 0, m_lastEpoch = 0, m_alphaRegressions = 0, m_prevEpoch = 0;
	bool m_haveEpoch = false, m_prevValid = false, m_haveLastTick = false;
	float m_prevAlpha = 1.0f;
	double m_timeMs = 0.0, m_lastTickStartMs = 0.0;
	std::vector<float> m_intervals, m_tickDurations, m_tickSpacing;
};
}
