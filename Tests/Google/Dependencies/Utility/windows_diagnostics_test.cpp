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

#include <gtest/gtest.h>
#include "Utility/windows_diagnostics.h"
#include "DbgHelpLoader.h"
#include <cstdio>
#include <cstring>
#include <type_traits>

// Compare to the real SDK, including native callback and displacement types.
#define CHECK_API(name) static_assert(std::is_same<decltype(&DbgHelp::name), decltype(&::name)>::value, #name " API contract")
CHECK_API(SymInitialize);
CHECK_API(SymCleanup);
CHECK_API(SymLoadModule);
CHECK_API(SymUnloadModule);
CHECK_API(SymGetModuleBase);
CHECK_API(SymGetSymFromAddr);
CHECK_API(SymGetLineFromAddr);
CHECK_API(SymFunctionTableAccess);
CHECK_API(StackWalk);
CHECK_API(MiniDumpWriteDump);
#undef CHECK_API
static_assert(sizeof(WindowsDiagnostics::Address) == sizeof(void*), "Runtime address width");
static_assert(sizeof(WindowsDiagnostics::ApiAddress) == sizeof(void*), "DbgHelp address width");
static_assert(alignof(WindowsDiagnostics::SymbolBuffer) >= alignof(IMAGEHLP_SYMBOL), "Symbol storage alignment");

TEST(WindowsDiagnostics, PreservesHighAddressBitsAndInitializesFrame)
{
    CONTEXT original = {};
#if defined(_WIN64)
    original.Rip = 0x1234567887654321ULL;
    original.Rsp = 0x2345678998765432ULL;
    original.Rbp = 0x3456789aa9876543ULL;
    EXPECT_EQ(IMAGE_FILE_MACHINE_AMD64, WindowsDiagnostics::MachineType);
#else
    original.Eip = 0x87654321UL;
    original.Esp = 0x98765432UL;
    original.Ebp = 0xa9876543UL;
    EXPECT_EQ(IMAGE_FILE_MACHINE_I386, WindowsDiagnostics::MachineType);
#endif
    const CONTEXT before = original;
    WindowsDiagnostics::Frame frame;
    std::memset(&frame, 0xff, sizeof(frame));
    WindowsDiagnostics::InitializeFrame(frame, original);
#if defined(_WIN64)
    EXPECT_EQ(0x1234567887654321ULL, frame.AddrPC.Offset);
    EXPECT_EQ(0x2345678998765432ULL, frame.AddrStack.Offset);
    EXPECT_EQ(0x3456789aa9876543ULL, frame.AddrFrame.Offset);
#else
    EXPECT_EQ(0x87654321UL, frame.AddrPC.Offset);
    EXPECT_EQ(0x98765432UL, frame.AddrStack.Offset);
    EXPECT_EQ(0xa9876543UL, frame.AddrFrame.Offset);
#endif
    EXPECT_EQ(WindowsDiagnostics::InstructionPointer(original), frame.AddrPC.Offset);
    EXPECT_EQ(WindowsDiagnostics::StackPointer(original), frame.AddrStack.Offset);
    EXPECT_EQ(WindowsDiagnostics::FramePointer(original), frame.AddrFrame.Offset);
    EXPECT_EQ(AddrModeFlat, frame.AddrPC.Mode);
    EXPECT_EQ(AddrModeFlat, frame.AddrStack.Mode);
    EXPECT_EQ(AddrModeFlat, frame.AddrFrame.Mode);
    EXPECT_EQ(0u, frame.AddrReturn.Offset);
    EXPECT_EQ(0, std::memcmp(&before, &original, sizeof(original)));
#if defined(_WIN64)
    EXPECT_EQ(&original, WindowsDiagnostics::WalkContext(original));
#else
    EXPECT_EQ(nullptr, WindowsDiagnostics::WalkContext(original));
#endif
}

TEST(WindowsDiagnostics, FormatsFullWidthAddress)
{
    char output[32];
#if defined(_WIN64)
    WindowsDiagnostics::Address address = 0x1234567887654321ULL;
    const char* expected = "1234567887654321";
#else
    WindowsDiagnostics::Address address = 0x87654321UL;
    const char* expected = "87654321";
#endif
    std::snprintf(output, sizeof(output), RTS_DIAGNOSTIC_ADDRESS_FORMAT, address);
    EXPECT_STREQ(expected, output);
    std::snprintf(output, sizeof(output), RTS_DIAGNOSTIC_ADDRESS_FORMAT, WindowsDiagnostics::Address(0));
#if defined(_WIN64)
    EXPECT_STREQ("0000000000000000", output);
#else
    EXPECT_STREQ("00000000", output);
#endif
}

TEST(WindowsDiagnostics, UnloadedApisFailWithoutWritingOutputs)
{
    ASSERT_FALSE(DbgHelpLoader::isLoaded());
    WindowsDiagnostics::SymbolBuffer symbol = {};
    WindowsDiagnostics::SymbolDisplacement displacement = static_cast<WindowsDiagnostics::SymbolDisplacement>(-1);
    EXPECT_FALSE(DbgHelp::SymGetSymFromAddr(GetCurrentProcess(), 0, &displacement, &symbol.symbol));
    EXPECT_EQ(static_cast<WindowsDiagnostics::SymbolDisplacement>(-1), displacement);
    EXPECT_EQ(0u, DbgHelp::SymGetModuleBase(GetCurrentProcess(), 0));
}

namespace
{
struct LoadedSymbols
{
    bool loaded;
    LoadedSymbols() : loaded(DbgHelpLoader::load()) {}
    ~LoadedSymbols() { DbgHelpLoader::unload(); }
};
}

TEST(WindowsDiagnostics, ResolvesNativeExportsAndSymbolAddresses)
{
    LoadedSymbols session;
    ASSERT_TRUE(session.loaded);
    ASSERT_TRUE(DbgHelp::SymInitialize(GetCurrentProcess(), nullptr, TRUE));
    const WindowsDiagnostics::Address address = reinterpret_cast<WindowsDiagnostics::Address>(&GetCurrentProcess);
    EXPECT_NE(0u, DbgHelp::SymGetModuleBase(GetCurrentProcess(), address));
    WindowsDiagnostics::SymbolBuffer storage = {};
    storage.symbol.SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
    storage.symbol.MaxNameLength = sizeof(storage.bytes) - sizeof(IMAGEHLP_SYMBOL);
    WindowsDiagnostics::SymbolDisplacement displacement = 0;
    ASSERT_TRUE(DbgHelp::SymGetSymFromAddr(GetCurrentProcess(), address, &displacement, &storage.symbol));
    EXPECT_EQ(address, storage.symbol.Address + displacement);
    EXPECT_NE('\0', storage.symbol.Name[0]);
    // A source line may not exist for a system export. Its offset is DWORD on
    // BOTH architectures; guards catch any accidental 64-bit output write.
    struct { DWORD before; DWORD offset; DWORD after; } lineOffset = {0x12345678, 0, 0x87654321};
    IMAGEHLP_LINE line = {};
    line.SizeOfStruct = sizeof(line);
    DbgHelp::SymGetLineFromAddr(GetCurrentProcess(), address, &lineOffset.offset, &line);
    EXPECT_EQ(0x12345678u, lineOffset.before);
    EXPECT_EQ(0x87654321u, lineOffset.after);
}

TEST(WindowsDiagnostics, UnwindsRealCurrentContext)
{
    LoadedSymbols session;
    ASSERT_TRUE(session.loaded);
    ASSERT_TRUE(DbgHelp::SymInitialize(GetCurrentProcess(), nullptr, TRUE));
    CONTEXT original = {};
    RtlCaptureContext(&original);
    CONTEXT walkContext = original;
    WindowsDiagnostics::Frame frame;
    WindowsDiagnostics::InitializeFrame(frame, walkContext);
    ASSERT_TRUE(DbgHelp::StackWalk(WindowsDiagnostics::MachineType, GetCurrentProcess(), GetCurrentThread(),
        &frame, WindowsDiagnostics::WalkContext(walkContext), nullptr,
        DbgHelp::SymFunctionTableAccess, DbgHelp::SymGetModuleBase, nullptr));
    EXPECT_NE(0u, frame.AddrPC.Offset);
    EXPECT_NE(0u, DbgHelp::SymGetModuleBase(GetCurrentProcess(), frame.AddrPC.Offset));
}
