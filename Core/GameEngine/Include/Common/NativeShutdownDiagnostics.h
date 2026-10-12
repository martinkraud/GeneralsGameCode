// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Common/StartupDiagnostics.h"
#include <cstddef>

// One-shot evidence, not an exception handler or a replacement for MiniDumper.
// Armed only at the end of the explicit AMD64 init-only diagnostic lifecycle.
namespace NativeShutdownDiagnostics {
#if defined(_WIN64)
struct Evidence {
    DWORD version, size, process, thread;
    ULONG_PTR executableBase, faultModuleBase;
    char faultModule[MAX_PATH];
    EXCEPTION_RECORD exception;
    CONTEXT context;
    SIZE_T stackBytes;
    unsigned char stack[16384]; // Raw bytes, NOT a decoded/validated call stack.
};
struct State {
    HANDLE file;
    PVOID observer;
    volatile LONG recorded;
    Evidence evidence;
};
inline State &state() { static State value = {}; return value; }
inline LONG WINAPI observe(EXCEPTION_POINTERS *p) {
    State &s = state();
    if (!s.observer || !p || !p->ExceptionRecord || !p->ContextRecord ||
        (p->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
         p->ExceptionRecord->ExceptionCode != EXCEPTION_IN_PAGE_ERROR) ||
        InterlockedCompareExchange(&s.recorded, 1, 0)) return EXCEPTION_CONTINUE_SEARCH;
    // No engine strings/allocator, CRT formatting, locks, DbgHelp or new threads.
    // Write the original context BEFORE querying memory, so a secondary failure
    // cannot erase the original exception. The first record is always authoritative.
    Evidence &e = s.evidence;
    e.thread = GetCurrentThreadId();
    e.exception = *p->ExceptionRecord;
    e.context = *p->ContextRecord;
    DWORD written = 0;
    const DWORD prefix = static_cast<DWORD>(offsetof(Evidence, stack));
    WriteFile(s.file, &e, prefix, &written, nullptr);
    FlushFileBuffers(s.file);
    MEMORY_BASIC_INFORMATION memory = {};
    if (VirtualQuery(p->ExceptionRecord->ExceptionAddress, &memory, sizeof(memory))) {
        e.faultModuleBase = reinterpret_cast<ULONG_PTR>(memory.AllocationBase);
        GetModuleFileNameA(static_cast<HMODULE>(memory.AllocationBase), e.faultModule, MAX_PATH);
    }
    ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(e.context.Rsp),
                      e.stack, sizeof(e.stack), &e.stackBytes);
    SetFilePointer(s.file, 0, nullptr, FILE_BEGIN);
    WriteFile(s.file, &e, sizeof(e), &written, nullptr);
    FlushFileBuffers(s.file);
    const char message[] = "native_shutdown\texception\toriginal context in shutdown-exception.bin\r\n";
    WriteFile(StartupDiagnostics::state().file, message, sizeof(message)-1, &written, nullptr);
    FlushFileBuffers(StartupDiagnostics::state().file);
    return EXCEPTION_CONTINUE_SEARCH; // Never consume a fault or turn it into success.
}
inline bool arm() {
    if (!StartupDiagnostics::enabled()) return true;
    State &s = state();
    if (s.observer) return true;
    char path[MAX_PATH];
    sprintf_s(path, "%s\\shutdown-exception.bin", StartupDiagnostics::state().root);
    s.file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (s.file == INVALID_HANDLE_VALUE) return false;
    s.evidence.version = 1;
    s.evidence.size = sizeof(Evidence);
    s.evidence.process = GetCurrentProcessId();
    s.evidence.executableBase = reinterpret_cast<ULONG_PTR>(GetModuleHandleA(nullptr));
    s.observer = AddVectoredExceptionHandler(1, observe);
    if (!s.observer) { CloseHandle(s.file); s.file = nullptr; return false; }
    StartupDiagnostics::checkpoint("native_shutdown", "armed", "return/CRT/process teardown; first AV or in-page error");
    return StartupDiagnostics::healthy();
}
// Keep the OS handles/observer until process termination: an atexit cleanup
// callback registered here would run BEFORE earlier global destructors.
inline bool armed() { return state().observer != nullptr; }
#else
inline bool arm() { return true; }
inline bool armed() { return false; }
#endif
}
