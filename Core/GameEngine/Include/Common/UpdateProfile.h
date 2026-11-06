// SPDX-License-Identifier: GPL-3.0-or-later
// X64.aq: opt-in profile of the per-object update (GENERALS_SIM_UPDPROF=<output file>): inclusive time and call count of every update module type (keyed by its v-table, symbolised in the report), and the
// AI update broken down by what the object's state machine was doing, its turret updates and its locomotion. Read-only timing; inert (one boolean test per update call) unless the variable is set.
#pragma once

namespace UpdateProfile
{
extern bool g_on;
inline bool enabled() { return g_on; }
unsigned long long now();
void noteModule(const void *vtable, unsigned long long ticks);
void addAiState(int stateId, unsigned long long ticks);
void addAiPart(int part, unsigned long long ticks);   ///< 0 whole AIUpdateInterface::update, 1 state machine, 2 turret AI, 3 locomotor
void report();
}
