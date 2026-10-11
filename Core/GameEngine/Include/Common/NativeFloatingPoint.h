// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <float.h>
#include <xmmintrin.h>

namespace NativeFloatingPoint {
// AMD64 SSE2 policy; never request unsupported x87 precision control.
inline int reset() {
    _fpreset();
    unsigned control = 0;
    const int result = _controlfp_s(&control, _RC_NEAR | _MCW_EM, _MCW_RC | _MCW_EM);
    _mm_setcsr((_mm_getcsr() & ~0xe07fu) | 0x1f80u);
    return result;
}
}
