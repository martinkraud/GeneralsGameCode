// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Frontend screen kit, part 3 (pure, engine-free): the pending-changes model of a settings screen.
//
// A screen registers every control once with the value it had when the screen opened (the baseline), feeds the current value every frame, and asks `dirty()`: the status line, the Back
// confirmation ("discard your changes?") and the Apply / Cancel logic all read this one answer. Putting a value back to its baseline makes the screen clean again (no stale "unsaved" state),
// accept() makes the current values the new baseline (after Apply), revert() forgets the changes (after Cancel). Fixed capacity, no heap, no engine types: a control is just (key, int value).
#pragma once

#include <type_traits>

namespace ModernSettings
{
constexpr int kMaxControls = 64;

struct Tracker
{
	int count = 0;
	int key[kMaxControls] = {};
	int baseline[kMaxControls] = {};
	int current[kMaxControls] = {};

	void clear() { count = 0; }

	// Registers a control (or re-baselines a known key). Returns its slot, -1 when the tracker is full.
	int add(int controlKey, int value)
	{
		for (int i = 0; i < count; ++i)
			if (key[i] == controlKey) { baseline[i] = current[i] = value; return i; }
		if (count >= kMaxControls) return -1;
		key[count] = controlKey; baseline[count] = current[count] = value;
		return count++;
	}

	int find(int controlKey) const
	{
		for (int i = 0; i < count; ++i) if (key[i] == controlKey) return i;
		return -1;
	}

	// Feeds the current value of a control; false when the key is unknown.
	bool set(int controlKey, int value)
	{
		const int i = find(controlKey);
		if (i < 0) return false;
		current[i] = value;
		return true;
	}

	bool dirtyAt(int slot) const { return slot >= 0 && slot < count && current[slot] != baseline[slot]; }

	int dirtyCount() const
	{
		int n = 0;
		for (int i = 0; i < count; ++i) n += current[i] != baseline[i] ? 1 : 0;
		return n;
	}

	bool dirty() const { return dirtyCount() > 0; }

	// After Apply: what is on screen is now the saved state.
	void accept() { for (int i = 0; i < count; ++i) baseline[i] = current[i]; }

	// After Cancel: the controls return to their baseline (the caller pushes the values back into the gadgets if it wants to keep the screen open).
	void revert() { for (int i = 0; i < count; ++i) current[i] = baseline[i]; }
};

static_assert(std::is_trivially_copyable<Tracker>::value, "plain data");
} // namespace ModernSettings
