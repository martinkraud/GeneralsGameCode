// SPDX-License-Identifier: GPL-3.0-or-later
// X64.sc: diagnostic unit multiplier for scalability measurements (GENERALS_SIM_CLONE=<k>@<frame>). Inert unless that variable is set. At the given logic frame every mobile, non-aircraft,
// non-structure unit owned by a computer player gets k extra copies of the same type on the same team, placed on legal cells near the original in a fixed spiral; the ordinary AI, locomotion and
// rendering then deal with them. Nothing about the rules changes, only the number of units. It exists to find where the engine's cost stops being acceptable; it is not a gameplay feature.
// The setup is a pure function of the simulation state at that frame, so a headless replay with the same setting gives the same simulation and digest. Never use it in multiplayer.
#pragma once

namespace ScalabilityMultiplier
{
void onLogicUpdate();   ///< called once per GameLogic::update, before the frame's work
}
