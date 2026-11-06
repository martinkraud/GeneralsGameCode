// SPDX-License-Identifier: GPL-3.0-or-later
// See BenchFingerprint.h.
#include "PreRTS.h"

#include "Common/BenchFingerprint.h"
#include "GameLogic/GameLogic.h"

#include <cstdio>
#include <cstdlib>

namespace BenchFingerprint
{
namespace
{
const char *s_path = nullptr;
unsigned s_interval = 30;
bool s_parsed = false;
unsigned s_lastFrame = 0xFFFFFFFFu;
}

void onFrameAdvanced()
{
	if (!s_parsed)
	{
		s_parsed = true;
		const char *v = getenv("GENERALS_SIM_FINGERPRINT");
		if (v && *v)
		{
			s_path = v;
			if (const char *i = getenv("GENERALS_SIM_FP_INTERVAL")) { const int n = atoi(i); if (n > 0) s_interval = static_cast<unsigned>(n); }
			// start with an empty file so a re-run never appends to an older fingerprint
			if (FILE *f = fopen(s_path, "wb")) fclose(f);
		}
	}
	if (!s_path || !TheGameLogic) return;
	const unsigned frame = static_cast<unsigned>(TheGameLogic->getFrame());
	if (frame == 0 || frame % s_interval != 0 || frame == s_lastFrame) return;
	s_lastFrame = frame;
	const unsigned crc = static_cast<unsigned>(TheGameLogic->getCRC(CRC_RECALC));
	if (FILE *f = fopen(s_path, "ab"))
	{
		fprintf(f, "frame=%u crc=%08X objects=%u\n", frame, crc, static_cast<unsigned>(TheGameLogic->getObjectCount()));
		fclose(f);
	}
}
}
