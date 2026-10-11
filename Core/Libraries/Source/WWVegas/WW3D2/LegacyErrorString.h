// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace LegacyD3DX {
namespace ErrorStringDetail {
// Complete finite lookup measured in the pinned DX8 helper, not a modern
// DirectX error database. Explicit 32-bit HRESULT words need no SDK linkage.
inline const char *Name(std::uint32_t value) {
    switch (value) {
    case 0x00000000u: return "S_OK";
    case 0x80004001u: return "E_NOTIMPL";
    case 0x80004005u: return "E_FAIL";
    case 0x8007000eu: return "E_OUTOFMEMORY";
    case 0x88760818u: return "D3DERR_WRONGTEXTUREFORMAT";
    case 0x88760819u: return "D3DERR_UNSUPPORTEDCOLOROPERATION";
    case 0x8876081au: return "D3DERR_UNSUPPORTEDCOLORARG";
    case 0x8876081bu: return "D3DERR_UNSUPPORTEDALPHAOPERATION";
    case 0x8876081cu: return "D3DERR_UNSUPPORTEDALPHAARG";
    case 0x8876081du: return "D3DERR_TOOMANYOPERATIONS";
    case 0x8876081eu: return "D3DERR_CONFLICTINGTEXTUREFILTER";
    case 0x8876081fu: return "D3DERR_UNSUPPORTEDFACTORVALUE";
    case 0x88760821u: return "D3DERR_CONFLICTINGRENDERSTATE";
    case 0x88760822u: return "D3DERR_UNSUPPORTEDTEXTUREFILTER";
    case 0x88760826u: return "D3DERR_CONFLICTINGTEXTUREPALETTE";
    case 0x88760827u: return "D3DERR_DRIVERINTERNALERROR";
    case 0x88760866u: return "D3DERR_NOTFOUND";
    case 0x88760867u: return "D3DERR_MOREDATA";
    case 0x88760868u: return "D3DERR_DEVICELOST";
    case 0x88760869u: return "D3DERR_DEVICENOTRESET";
    case 0x8876086au: return "D3DERR_NOTAVAILABLE";
    case 0x8876017cu: return "D3DERR_OUTOFVIDEOMEMORY";
    case 0x8876086bu: return "D3DERR_INVALIDDEVICE";
    case 0x8876086cu: return "D3DERR_INVALIDCALL";
    case 0x8876086du: return "D3DERR_DRIVERINVALIDCALL";
    case 0x88760b54u: return "D3DXERR_CANNOTMODIFYINDEXBUFFER";
    case 0x88760b55u: return "D3DXERR_INVALIDMESH";
    case 0x88760b56u: return "D3DXERR_CANNOTATTRSORT";
    case 0x88760b57u: return "D3DXERR_SKINNINGNOTSUPPORTED";
    case 0x88760b58u: return "D3DXERR_TOOMANYINFLUENCES";
    case 0x88760b59u: return "D3DXERR_INVALIDDATA";
    case 0x88760b5au: return "D3DXERR_LOADEDMESHASNODATA";
    default: return nullptr;
    }
}
}

// Pinned D3DX8 always returns S_OK, including null/zero-size output. Preserve
// its exact-fit quirk: size==text length writes all characters WITHOUT NUL;
// smaller nonzero buffers reserve the final byte for NUL after truncation.
inline std::int32_t GetErrorStringA(std::uint32_t value, char *output, std::uint32_t size) {
    if (!output || !size) return 0;
    const char *text = ErrorStringDetail::Name(value);
    char fallback[] = "Unrecognized error: 0x00000000";
    if (!text) {
        static const char hex[] = "0123456789abcdef";
        for (unsigned i = 0; i < 8; ++i)
            fallback[22 + i] = hex[(value >> (28 - 4 * i)) & 15];
        text = fallback;
    }
    std::uint32_t copied = 0;
    while (copied < size && text[copied]) {
        output[copied] = text[copied];
        ++copied;
    }
    if (copied < size) output[copied] = '\0';
    else if (text[copied]) output[size - 1] = '\0';
    return 0;
}
} // namespace LegacyD3DX
