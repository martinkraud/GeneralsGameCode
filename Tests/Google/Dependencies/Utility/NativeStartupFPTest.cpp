// SPDX-License-Identifier: GPL-3.0-or-later
// X64.11 numerical cases retained; X64.12 exercises the shared AMD64 production policy.
#include <gtest/gtest.h>
#include "Common/NativeFloatingPoint.h"
#include <float.h>
#include <stdlib.h>
#include <errno.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
unsigned invalidCalls;
void InvalidParameter(const wchar_t *, const wchar_t *, const wchar_t *, unsigned, uintptr_t) {
    ++invalidCalls;
}
struct State {
    unsigned control = 0, mxcsr = _mm_getcsr();
    _invalid_parameter_handler handler = _set_thread_local_invalid_parameter_handler(InvalidParameter);
    State() { _controlfp_s(&control, 0, 0); invalidCalls = 0; }
    ~State() {
        unsigned ignored;
        unsigned mask = _MCW_RC | _MCW_EM | _MCW_DN;
#if defined(_M_IX86)
        mask |= _MCW_PC | _MCW_IC;
#endif
        _controlfp_s(&ignored, control, mask);
        _mm_setcsr(mxcsr);
        _set_thread_local_invalid_parameter_handler(handler);
    }
};
void ExistingGameLogicPolicy() {
    _fpreset();
    unsigned value = _statusfp();
    value = (value & ~_MCW_RC) | (_RC_NEAR & _MCW_RC);
    value = (value & ~_MCW_PC) | (_PC_24 & _MCW_PC);
    _controlfp(value, _MCW_PC | _MCW_RC);
}
// Historical test names retained; this wrapper calls the production helper.
int ProposedNativePolicy() { return NativeFloatingPoint::reset(); }
unsigned Bits(float value) { unsigned bits; std::memcpy(&bits, &value, sizeof(bits)); return bits; }
}

TEST(NativeStartupFP, ExistingPrecisionRequest) {
#if defined(_M_AMD64) && defined(_DEBUG)
    GTEST_SKIP() << "Unsupported precision request is probed only against the measured Release CRT";
#endif
    State saved;
    errno = 0;
    ExistingGameLogicPolicy();
    unsigned control = 0; _controlfp_s(&control, 0, 0);
#if defined(_M_AMD64)
    // The installed Release UCRT ignores the unsupported precision bits.
    // Do not mistake successful return for x87 PC_24 semantics on SSE2.
    EXPECT_EQ(invalidCalls, 0u);
    EXPECT_EQ(errno, 0);
    EXPECT_EQ(control & _MCW_PC, 0u);
#else
    EXPECT_EQ(invalidCalls, 0u);
    EXPECT_EQ(control & _MCW_PC, _PC_24);
#endif
    std::printf("legacy invalid_calls=%u errno=%d control=%08x mxcsr=%08x\n",
        invalidCalls, errno, control, _mm_getcsr());
}

TEST(NativeStartupFP, ProposedNativeResetAndIdempotence) {
    State saved;
    _mm_setcsr((_mm_getcsr() | 0x8040u | 0x2000u) & ~0x4000u);
    ASSERT_EQ(ProposedNativePolicy(), 0);
    unsigned control = 0; ASSERT_EQ(_controlfp_s(&control, 0, 0), 0);
    EXPECT_EQ(control & _MCW_RC, _RC_NEAR);
    EXPECT_EQ(control & _MCW_EM, _MCW_EM);
    EXPECT_EQ(_mm_getcsr() & 0xffffu, 0x1f80u);
    const unsigned first = _mm_getcsr();
    EXPECT_EQ(ProposedNativePolicy(), 0);
    EXPECT_EQ(_mm_getcsr(), first);
    EXPECT_EQ(invalidCalls, 0u);
    std::printf("proposed control=%08x mxcsr=%08x\n", control, first);
}

TEST(NativeStartupFP, ProposedGradualUnderflowAndNearestEven) {
    State saved;
    ASSERT_EQ(ProposedNativePolicy(), 0);
    volatile float small = std::numeric_limits<float>::denorm_min();
    volatile float doubled = small + small;
    EXPECT_EQ(Bits(doubled), 2u);
    volatile float one = 1.0f, halfUlp = 0x1p-24f;
    volatile float tie = one + halfUlp;
    EXPECT_EQ(Bits(tie), Bits(1.0f));
}

TEST(NativeStartupFP, PrecisionControlIsNotCrossArchitectureEquivalence) {
    State saved;
    ASSERT_EQ(ProposedNativePolicy(), 0);
    volatile float a = 16777216.0f, b = 1.0f;
    volatile float single = (a + b) - a;
    volatile double wide = (static_cast<double>(a) + static_cast<double>(b)) - static_cast<double>(a);
    EXPECT_EQ(single, 0.0f);
    EXPECT_EQ(wide, 1.0);
#if defined(_M_IX86)
    unsigned ignored; float pc24, pc53;
    _controlfp_s(&ignored, _PC_24, _MCW_PC);
    __asm { fld dword ptr a
        fadd dword ptr b
        fsub dword ptr a
        fstp dword ptr pc24 }
    _controlfp_s(&ignored, _PC_53, _MCW_PC);
    __asm { fld dword ptr a
        fadd dword ptr b
        fsub dword ptr a
        fstp dword ptr pc53 }
    EXPECT_EQ(pc24, 0.0f); EXPECT_EQ(pc53, 1.0f);
#endif
}
