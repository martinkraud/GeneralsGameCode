// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Deliberately independent of engine allocation, strings and debug initialization.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <exception>

namespace StartupDiagnostics {
struct State {
    bool configured, requested, valid, healthy;
    bool graphicsSmoke, displaySmoke, uiImageSmoke, uiSentenceSmoke, uiPanelSmoke;
    HANDLE file;
    unsigned records;
    char root[MAX_PATH], userData[MAX_PATH];
};
inline State &state() { static State value = {}; return value; }
inline bool enabled() { return state().requested && state().valid; }
inline bool graphicsSmoke() { return enabled() && state().graphicsSmoke; }
inline bool displaySmoke() { return enabled() && state().displaySmoke; }
inline bool uiImageSmoke() { return enabled() && state().uiImageSmoke; }
inline bool uiSentenceSmoke() { return enabled() && state().uiSentenceSmoke; }
inline bool uiPanelSmoke() { return enabled() && state().uiPanelSmoke; }
inline bool healthy() { return !enabled() || state().healthy; }
inline const char *userData() { return enabled() ? state().userData : nullptr; }

// Strict grammar prevents startup flags from accidentally starting a scenario.
inline bool validArguments(int argc, char **argv) {
    return argc == 4 && !_stricmp(argv[1], "-headless") &&
        !_stricmp(argv[2], "-nativeInitOnly") && std::strlen(argv[3]) < 180;
}
inline bool validGraphicsArguments(int argc, char **argv) {
    return argc == 4 && !_stricmp(argv[1], "-headless") &&
        !_stricmp(argv[2], "-nativeGraphicsSmoke") && std::strlen(argv[3]) < 180;
}
inline bool validDisplayArguments(int argc, char **argv) {
    return argc == 4 && !_stricmp(argv[1], "-headless") &&
        !_stricmp(argv[2], "-nativeDisplaySmoke") && std::strlen(argv[3]) < 180;
}
inline bool validUIImageArguments(int argc, char **argv) {
    return argc == 4 && !_stricmp(argv[1], "-headless") &&
        !_stricmp(argv[2], "-nativeUIImageSmoke") && std::strlen(argv[3]) < 180;
}
inline bool validUISentenceArguments(int argc, char **argv) {
    return argc == 4 && !_stricmp(argv[1], "-headless") &&
        !_stricmp(argv[2], "-nativeUISentenceSmoke") && std::strlen(argv[3]) < 180;
}
inline bool validUIPanelArguments(int argc, char **argv) {
    return argc == 4 && !_stricmp(argv[1], "-headless") &&
        !_stricmp(argv[2], "-nativeUIPanelSmoke") && std::strlen(argv[3]) < 180;
}
inline void checkpoint(const char *stage, const char *status,
                       const char *detail = "", unsigned code = 0) {
    State &s = state();
    if (!enabled() || !s.healthy) return;
    if (s.records >= 2048) { s.healthy = false; return; }
    char clean[768]; unsigned i = 0;
    for (; detail[i] && i < sizeof(clean)-1; ++i)
        clean[i] = detail[i] == '\t' || detail[i] == '\n' || detail[i] == '\r' ? ' ' : detail[i];
    clean[i] = 0;
    FILETIME utc; GetSystemTimeAsFileTime(&utc);
    LARGE_INTEGER qpc; QueryPerformanceCounter(&qpc);
    char line[1200];
    const int size = sprintf_s(line, "%u\t%08lx%08lx\t%lld\t%lu\t%lu\t%s\t%s\t%u\t%s\r\n",
        ++s.records, utc.dwHighDateTime, utc.dwLowDateTime, qpc.QuadPart,
        GetCurrentProcessId(), GetCurrentThreadId(), stage, status, code, clean);
    DWORD written = 0;
    if (size <= 0 || !WriteFile(s.file, line, size, &written, nullptr) ||
        written != static_cast<DWORD>(size) || !FlushFileBuffers(s.file)) s.healthy = false;
}
inline bool configure(int argc, char **argv) {
    State &s = state();
    if (s.configured) return !s.requested || (s.valid && s.healthy);
    s.configured = true;
    for (int i = 1; i < argc; ++i)
        if (!_stricmp(argv[i], "-nativeInitOnly")) s.requested = true;
        else if (!_stricmp(argv[i], "-nativeGraphicsSmoke")) { s.requested = true; s.graphicsSmoke = true; }
        else if (!_stricmp(argv[i], "-nativeDisplaySmoke")) { s.requested = true; s.displaySmoke = true; }
        else if (!_stricmp(argv[i], "-nativeUIImageSmoke")) { s.requested = true; s.displaySmoke = s.uiImageSmoke = true; }
        else if (!_stricmp(argv[i], "-nativeUISentenceSmoke")) { s.requested = true; s.displaySmoke = s.uiImageSmoke = s.uiSentenceSmoke = true; }
        else if (!_stricmp(argv[i], "-nativeUIPanelSmoke")) { s.requested = true; s.displaySmoke = s.uiImageSmoke = s.uiSentenceSmoke = s.uiPanelSmoke = true; }
    if (!s.requested) return true;
#if !defined(_WIN64)
    return false; // Native diagnostic option must never change the Win32 route.
#else
    if (!(s.uiPanelSmoke ? validUIPanelArguments(argc, argv) : s.uiSentenceSmoke ? validUISentenceArguments(argc, argv) : s.uiImageSmoke ? validUIImageArguments(argc, argv) : s.displaySmoke ? validDisplayArguments(argc, argv) : s.graphicsSmoke ? validGraphicsArguments(argc, argv) : validArguments(argc, argv))) return false;
    const char *root = argv[3];
    if (!((root[0] >= 'A' && root[0] <= 'Z') || (root[0] >= 'a' && root[0] <= 'z')) ||
        root[1] != ':' || root[2] != '\\') return false;
    for (unsigned i = 3; root[i]; ++i)
        if (static_cast<unsigned char>(root[i]) < 32 || static_cast<unsigned char>(root[i]) > 126 ||
            root[i] == ':' || root[i] == '"' || root[i] == '/' || root[i] == '*' || root[i] == '?') return false;
    // Require canonical, existing local directories and refuse junction ancestors.
    char full[MAX_PATH];
    if (!GetFullPathNameA(root, MAX_PATH, full, nullptr) || _stricmp(root, full)) return false;
    strcpy_s(s.root, full);
    for (;;) {
        const DWORD attributes = GetFileAttributesA(full);
        if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
        if (std::strlen(full) <= 3) break;
        char *slash = std::strrchr(full, '\\');
        if (!slash) return false;
        if (slash == full + 2) slash[1] = 0; else *slash = 0;
    }
    sprintf_s(s.userData, "%s\\userdata\\", s.root);
    const DWORD attributes = GetFileAttributesA(s.userData);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    char dump[MAX_PATH]; sprintf_s(dump, "%sCrashDumps", s.userData);
    const DWORD dumpAttributes = GetFileAttributesA(dump);
    if (dumpAttributes == INVALID_FILE_ATTRIBUTES || !(dumpAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (dumpAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    char log[MAX_PATH]; sprintf_s(log, "%s\\startup.log", s.root);
    s.file = CreateFileA(log, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (s.file == INVALID_HANDLE_VALUE) return false;
    s.valid = s.healthy = true;
    checkpoint("diagnostic", "begin", s.root);
    checkpoint("userdata", "isolated", s.userData);
    return s.healthy;
#endif
}
inline void close() {
    State &s = state();
    if (s.valid && s.file && s.file != INVALID_HANDLE_VALUE) {
        CloseHandle(s.file); s.file = nullptr; s.healthy = false;
    }
}
// Scoped init phase: a missing completion or unwind is visible without a debugger.
struct Phase {
    const char *name;
    int exceptions;
    explicit Phase(const char *value) : name(value), exceptions(enabled() ? std::uncaught_exceptions() : 0) {
        checkpoint(name, "begin");
    }
    ~Phase() { if (enabled()) checkpoint(name, std::uncaught_exceptions() > exceptions ? "fail" : "complete"); }
};
}
