/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Runtime diagnostic addresses only; never use these types for game IDs or files.
#pragma once
#include <windows.h>
#include <imagehlp.h>

#if defined(_WIN64) && !defined(_M_X64) && !defined(__x86_64__)
#error "Windows diagnostics currently support native x86 and AMD64 only"
#endif

#define RTS_DIAGNOSTIC_STRINGIFY_IMPL(name) #name
#define RTS_DIAGNOSTIC_EXPORT_NAME(name) RTS_DIAGNOSTIC_STRINGIFY_IMPL(name)

#if defined(_WIN64)
#define RTS_DIAGNOSTIC_ADDRESS_FORMAT "%016I64X"
#define RTS_DIAGNOSTIC_OFFSET_FORMAT "%I64X"
#else
#define RTS_DIAGNOSTIC_ADDRESS_FORMAT "%08lX"
#define RTS_DIAGNOSTIC_OFFSET_FORMAT "%lX"
#endif

namespace WindowsDiagnostics
{
    typedef ULONG_PTR Address;
#if defined(_WIN64)
    typedef DWORD64 ApiAddress;
    const DWORD MachineType = IMAGE_FILE_MACHINE_AMD64;
#else
    typedef DWORD ApiAddress;
    const DWORD MachineType = IMAGE_FILE_MACHINE_I386;
#endif
    // The SDK maps these structures and callback types to their 64-bit variants
    // on Win64. Symbol displacement is address-sized; line displacement is DWORD.
    typedef STACKFRAME Frame;
    typedef ApiAddress SymbolDisplacement;

    inline Address InstructionPointer(const CONTEXT& context)
    {
#if defined(_WIN64)
        return context.Rip;
#else
        return context.Eip;
#endif
    }
    inline Address StackPointer(const CONTEXT& context)
    {
#if defined(_WIN64)
        return context.Rsp;
#else
        return context.Esp;
#endif
    }
    inline Address FramePointer(const CONTEXT& context)
    {
#if defined(_WIN64)
        return context.Rbp;
#else
        return context.Ebp;
#endif
    }
    inline void InitializeFrame(Frame& frame, const CONTEXT& context)
    {
        ZeroMemory(&frame, sizeof(frame));
        frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrPC.Offset = InstructionPointer(context);
        frame.AddrStack.Offset = StackPointer(context);
        frame.AddrFrame.Offset = FramePointer(context);
    }
    inline void* WalkContext(CONTEXT& context)
    {
#if defined(_WIN64)
        // AMD64 unwind metadata needs the full, mutable register context.
        return &context;
#else
        // Preserve the existing Win32 StackWalk contract/capture path.
        return nullptr;
#endif
    }
    // IMAGEHLP_SYMBOL contains a variable length name. Keep storage aligned to
    // the native symbol structure without allocating in a crash handler.
    union SymbolBuffer
    {
        IMAGEHLP_SYMBOL symbol;
        unsigned char bytes[1024];
    };
}
