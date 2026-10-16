// SPDX-License-Identifier: GPL-3.0-or-later
// X64.37: the transactional Apply / Keep / Revert model for display changes (pure, engine-free, deterministic).
//
//   LAST KNOWN GOOD  the configuration the player has accepted (or the one the game started with); the only thing that is ever
//                    returned to, and the only thing persisted as "the display".
//   PENDING          a requested configuration that has been APPLIED and is waiting for the player's confirmation.
//   CURRENT          what is running right now: PENDING while a confirmation is open, LAST KNOWN GOOD otherwise.
//
// Flow:  begin(lastKnownGood, requested)  -> apply(requested)
//          apply failed        -> apply(lastKnownGood)  -> RolledBack      (nothing was persisted)
//          rollback failed too -> apply(safe fallback)  -> SafeFallback
//          apply succeeded     -> Pending (a deadline starts)
//        keep()                -> lastKnownGood = pending                  (the caller persists the accepted configuration now)
//        revert() / timeout    -> apply(lastKnownGood) (else the safe fallback)
// The player can therefore never be left on an unusable display: an unanswered confirmation reverts by itself.
#pragma once

#include "Common/DisplayGeometry.h"
#include "Common/DisplaySettings.h"

#include <functional>
#include <string>

namespace DisplayTransaction
{
using DisplayConfig::WindowMode;
using DisplayGeometry::Size;

// Everything a display change can alter that may leave the player without a usable screen.
struct Config
{
	int monitorIndex = 0;
	std::string monitorName;           // stable device name (preferred over the index)
	WindowMode mode = WindowMode::Windowed;
	Size client;                       // windowed client size; borderless uses the desktop size and ignores it
	int uiScalePercent = DisplayGeometry::kAutoUiScale;

	bool operator==(const Config &o) const
	{
		return monitorIndex == o.monitorIndex && monitorName == o.monitorName && mode == o.mode && client == o.client &&
		       uiScalePercent == o.uiScalePercent;
	}
	bool operator!=(const Config &o) const { return !(*this == o); }
};

// The configuration that always works: a small window on the primary monitor.
inline Config safeFallback()
{
	Config c;
	c.mode = WindowMode::Windowed;
	c.client = DisplayGeometry::kReferenceSize;
	c.uiScalePercent = 100;
	return c;
}

enum class Phase { Idle, AwaitingConfirmation };
enum class Outcome
{
	Unchanged,       // requested == last known good: nothing applied
	Pending,         // applied, waiting for Keep / Revert / timeout
	RolledBack,      // the request could not be applied; the last known good was restored
	SafeFallback,    // not even the last known good could be restored; the safe fallback is running
};

using ApplyFn = std::function<bool(const Config &)>;

class Transaction
{
public:
	static constexpr unsigned kDefaultTimeoutMs = 15000;

	explicit Transaction(unsigned timeoutMs = kDefaultTimeoutMs) : m_timeoutMs(timeoutMs) {}

	Phase phase() const { return m_phase; }
	const Config &lastKnownGood() const { return m_lastKnownGood; }
	const Config &pending() const { return m_pending; }
	// What is running right now.
	const Config &current() const { return m_phase == Phase::AwaitingConfirmation ? m_pending : m_lastKnownGood; }

	// Milliseconds until the automatic revert (0 when nothing is pending or the deadline has passed).
	unsigned remainingMs(unsigned nowMs) const
	{
		if (m_phase != Phase::AwaitingConfirmation) return 0;
		const int elapsed = static_cast<int>(nowMs - m_start);
		return elapsed >= static_cast<int>(m_timeoutMs) ? 0u : m_timeoutMs - static_cast<unsigned>(elapsed < 0 ? 0 : elapsed);
	}

	Outcome begin(const Config &lastKnownGood, const Config &requested, const ApplyFn &apply, unsigned nowMs)
	{
		m_lastKnownGood = lastKnownGood;
		m_phase = Phase::Idle;
		if (requested == lastKnownGood)
			return Outcome::Unchanged;
		if (apply(requested))
		{
			m_pending = requested;
			m_start = nowMs;
			m_phase = Phase::AwaitingConfirmation;
			return Outcome::Pending;
		}
		return restore(apply);
	}

	// The player kept the change. Returns the configuration that is now the last known good (to persist).
	bool keep(Config *accepted = nullptr)
	{
		if (m_phase != Phase::AwaitingConfirmation) return false;
		m_lastKnownGood = m_pending;
		m_phase = Phase::Idle;
		if (accepted) *accepted = m_lastKnownGood;
		return true;
	}

	// The player declined (or the deadline passed): back to the last known good.
	Outcome revert(const ApplyFn &apply)
	{
		if (m_phase != Phase::AwaitingConfirmation) return Outcome::Unchanged;
		m_phase = Phase::Idle;
		return restore(apply);
	}

	// Call every frame while a confirmation is open. Returns true when the deadline passed and the revert was performed.
	bool tick(unsigned nowMs, const ApplyFn &apply, Outcome *outcome = nullptr)
	{
		if (m_phase != Phase::AwaitingConfirmation || remainingMs(nowMs) > 0) return false;
		const Outcome o = revert(apply);
		if (outcome) *outcome = o;
		return true;
	}

private:
	Outcome restore(const ApplyFn &apply)
	{
		if (apply(m_lastKnownGood))
			return Outcome::RolledBack;
		const Config safe = safeFallback();
		apply(safe);                          // best effort: nothing else is left to try
		m_lastKnownGood = safe;
		return Outcome::SafeFallback;
	}

	unsigned m_timeoutMs;
	unsigned m_start = 0;
	Phase m_phase = Phase::Idle;
	Config m_lastKnownGood;
	Config m_pending;
};

}  // namespace DisplayTransaction
