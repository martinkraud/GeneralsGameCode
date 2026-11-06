// SPDX-License-Identifier: GPL-3.0-or-later
// X64.gb: authoritative-state fingerprint of a replay played in the NORMAL (graphical) client, so a seeded graphical benchmark can prove that two runs - or a graphical and a headless run - simulate exactly the same game.
// GENERALS_SIM_FINGERPRINT=<file> writes one line "frame=<n> crc=<hex> objects=<n>" every GENERALS_SIM_FP_INTERVAL logic frames (default 30), in the format of the headless bench's fingerprint file, taken at the same point
// of the tick (right after the frame counter advanced). Read-only (GameLogic::getCRC(CRC_RECALC) is what the headless bench calls); inert unless the variable is set.
#pragma once

namespace BenchFingerprint
{
void onFrameAdvanced();   ///< called by GameLogic::update right after the logic frame counter was incremented
}
