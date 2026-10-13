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
    bool graphicsSmoke, displaySmoke, uiImageSmoke, uiSentenceSmoke, uiPanelSmoke, uiMenuSmoke;
    // Ordinary application/display lifetime with the UI presentation host. World,
    // audio and radar selection stays on the accepted headless variants.
    bool ordinaryHost;
    // Ordinary host plus the real window manager/Shell/mouse/keyboard UI lifetime
    // and the bounded menu-only loop. Implies ordinaryHost.
    bool ordinaryMenu;
    // Ordinary menu plus the ordinary engine loop and a live GameLogic (headless
    // world selection, UI-only draw). Implies ordinaryMenu and ordinaryHost.
    bool ordinaryGame;
    // Ordinary game plus the first native terrain world frame (R2): terrain-only
    // D3D11 pass under the original UI. Implies ordinaryGame/Menu/Host.
    bool ordinaryTerrain;
    // Ordinary terrain plus native world objects, picking, the in-game UI draw and the full
    // Control Bar update, with user-driven play (no scripted camera, no logic-frame bound).
    // Implies ordinaryTerrain and everything below it.
    bool ordinaryPlay;
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
inline bool uiMenuSmoke() { return enabled() && state().uiMenuSmoke; }
inline bool uiPanelSmoke() { return enabled() && state().uiPanelSmoke; }
inline bool ordinaryHost() { return enabled() && state().ordinaryHost; }
inline bool ordinaryMenu() { return enabled() && state().ordinaryMenu; }
inline bool ordinaryGame() { return enabled() && state().ordinaryGame; }
inline bool ordinaryTerrain() { return enabled() && state().ordinaryTerrain; }
inline bool ordinaryPlay() { return enabled() && state().ordinaryPlay; }
inline bool healthy() { return !enabled() || state().healthy; }
// A verified-contract violation is recorded by the caller first; this only
// forces the process exit classification to failure.
inline void markFailed() { if (enabled()) state().healthy = false; }
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
inline bool validUIMenuArguments(int argc, char **argv) {
    return argc == 4 && !_stricmp(argv[1], "-headless") &&
        !_stricmp(argv[2], "-nativeUIMenuSmoke") && std::strlen(argv[3]) < 180;
}
inline bool validUIPanelArguments(int argc, char **argv) {
    return argc == 4 && !_stricmp(argv[1], "-headless") &&
        !_stricmp(argv[2], "-nativeUIPanelSmoke") && std::strlen(argv[3]) < 180;
}
// Strict ordinary-host grammar. Every token is mandatory and exact: the profile is
// windowed 800x600, no intro/sizzle, no shell map, Windows cursors, isolated root.
// Nothing is coerced; any extra, missing, reordered or different token is rejected.
// Generals only: the Zero Hour title has not been audited for this profile.
inline bool validOrdinaryArguments(int argc, char **argv, const char *mode) {
#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR
    (void)argc; (void)argv; (void)mode; return false;
#else
    static const char *const fixed[] = { "-win", "-xres", "800", "-yres", "600",
        "-nologo", "-noshellmap", "-winCursors" };
    if (argc != 11 || std::strlen(argv[10]) >= 180) return false;
    for (int i = 0; i < 8; ++i)
        if (_stricmp(argv[i + 1], fixed[i])) return false;
    return !_stricmp(argv[9], mode);
#endif
}
inline bool validOrdinaryHostArguments(int argc, char **argv) {
    return validOrdinaryArguments(argc, argv, "-nativeOrdinaryHost");
}
// Same profile plus the interactive UI lifetime; a distinct, equally strict token.
inline bool validOrdinaryMenuArguments(int argc, char **argv) {
    return validOrdinaryArguments(argc, argv, "-nativeOrdinaryMenu");
}
inline bool validOrdinaryGameArguments(int argc, char **argv) {
    return validOrdinaryArguments(argc, argv, "-nativeOrdinaryGame");
}
inline bool validOrdinaryTerrainArguments(int argc, char **argv) {
    return validOrdinaryArguments(argc, argv, "-nativeOrdinaryTerrain");
}
inline bool validOrdinaryPlayArguments(int argc, char **argv) {
    return validOrdinaryArguments(argc, argv, "-nativeOrdinaryPlay");
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
        else if (!_stricmp(argv[i], "-nativeUIMenuSmoke")) { s.requested = true; s.displaySmoke = s.uiMenuSmoke = true; }
        else if (!_stricmp(argv[i], "-nativeUIPanelSmoke")) { s.requested = true; s.displaySmoke = s.uiImageSmoke = s.uiSentenceSmoke = s.uiPanelSmoke = true; }
        else if (!_stricmp(argv[i], "-nativeOrdinaryHost")) { s.requested = true; s.ordinaryHost = true; }
        else if (!_stricmp(argv[i], "-nativeOrdinaryMenu")) { s.requested = true; s.ordinaryHost = s.ordinaryMenu = true; }
        else if (!_stricmp(argv[i], "-nativeOrdinaryPlay")) { s.requested = true; s.ordinaryHost = s.ordinaryMenu = s.ordinaryGame = s.ordinaryTerrain = s.ordinaryPlay = true; }
        else if (!_stricmp(argv[i], "-nativeOrdinaryTerrain")) { s.requested = true; s.ordinaryHost = s.ordinaryMenu = s.ordinaryGame = s.ordinaryTerrain = true; }
        else if (!_stricmp(argv[i], "-nativeOrdinaryGame")) { s.requested = true; s.ordinaryHost = s.ordinaryMenu = s.ordinaryGame = true; }
    if (!s.requested) return true;
#if !defined(_WIN64)
    return false; // Native diagnostic option must never change the Win32 route.
#else
    if (!(s.ordinaryPlay ? validOrdinaryPlayArguments(argc, argv) : s.ordinaryTerrain ? validOrdinaryTerrainArguments(argc, argv) : s.ordinaryGame ? validOrdinaryGameArguments(argc, argv) : s.ordinaryMenu ? validOrdinaryMenuArguments(argc, argv) : s.ordinaryHost ? validOrdinaryHostArguments(argc, argv) : s.uiMenuSmoke ? validUIMenuArguments(argc, argv) : s.uiPanelSmoke ? validUIPanelArguments(argc, argv) : s.uiSentenceSmoke ? validUISentenceArguments(argc, argv) : s.uiImageSmoke ? validUIImageArguments(argc, argv) : s.displaySmoke ? validDisplayArguments(argc, argv) : s.graphicsSmoke ? validGraphicsArguments(argc, argv) : validArguments(argc, argv))) return false;
    const char *root = argv[s.ordinaryHost ? 10 : 3];
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
// Bounded admission for per-call/per-frame receipts. The 2048-record log is a
// fail-closed capacity guard (X64.24 run 20261012T2112467768379Z overflowed it with
// one FP receipt per logic step); repeated hot-path receipts must admit only the
// first `limit` calls plus state changes, and report the total once at the end.
struct RepeatGate {
    unsigned calls = 0, logged = 0;
    bool admit(unsigned limit = 64) { ++calls; if (logged < limit) { ++logged; return true; } return false; }
};

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
