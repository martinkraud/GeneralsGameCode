// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "Common/StartupDiagnostics.h"
#include "Common/NativeShutdownDiagnostics.h"
#include "Common/NativeOrdinaryHost.h"
#include <fstream>
#include <string>

// The child is this test executable, never a game. Fault after main returns,
// while the CRT invokes an exit callback, with no engine allocator involved.
static void shutdownFault() {
    *reinterpret_cast<volatile unsigned *>(static_cast<ULONG_PTR>(1)) = 7;
}
static LONG WINAPI childFilter(EXCEPTION_POINTERS *p) {
#if defined(_WIN64)
    // A later filter must not overwrite the first exception (including recursion).
    p->ExceptionRecord->ExceptionInformation[1] = 999;
    NativeShutdownDiagnostics::observe(p);
#endif
    return EXCEPTION_EXECUTE_HANDLER;
}
int main(int argc, char **argv) {
#if defined(_WIN64)
    if (argc == 4 && !std::strcmp(argv[1], "--shutdown-child")) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        SetUnhandledExceptionFilter(childFilter);
        char exe[]="probe",head[]="-headless",flag[]="-nativeInitOnly";
        char *args[]={exe,head,flag,argv[2]};
        if (!StartupDiagnostics::configure(4,args) || !NativeShutdownDiagnostics::arm()) return 87;
        if (!std::strcmp(argv[3],"fault")) std::atexit(shutdownFault);
        StartupDiagnostics::checkpoint("WinMain","complete","test main return");
        return 0;
    }
#endif
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(NativeShutdownGate, DefaultOff) {
    StartupDiagnostics::state()={};
    EXPECT_TRUE(NativeShutdownDiagnostics::arm());
    EXPECT_FALSE(NativeShutdownDiagnostics::armed());
}

#if defined(_WIN64)
TEST(NativeShutdownGate, RealPostMainFaultPreservesContextAndNonzeroExit) {
    for (const char *mode : {"normal", "fault"}) {
        char temp[MAX_PATH],unique[MAX_PATH],exe[MAX_PATH];
        ASSERT_NE(GetTempPathA(MAX_PATH,temp),0u);
        ASSERT_NE(GetTempFileNameA(temp,"x13",0,unique),0u);
        ASSERT_TRUE(DeleteFileA(unique)); ASSERT_TRUE(CreateDirectoryA(unique,nullptr));
        std::string root(unique),data=root+"\\userdata",dump=data+"\\CrashDumps";
        ASSERT_TRUE(CreateDirectoryA(data.c_str(),nullptr)); ASSERT_TRUE(CreateDirectoryA(dump.c_str(),nullptr));
        ASSERT_NE(GetModuleFileNameA(nullptr,exe,MAX_PATH),0u);
        std::string command="\""+std::string(exe)+"\" --shutdown-child \""+root+"\" "+mode;
        STARTUPINFOA si={}; si.cb=sizeof(si); PROCESS_INFORMATION pi={};
        ASSERT_TRUE(CreateProcessA(exe,&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi));
        DWORD wait=WaitForSingleObject(pi.hProcess,15000);
        if (wait != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess,99); WaitForSingleObject(pi.hProcess,5000); }
        DWORD code=0; ASSERT_TRUE(GetExitCodeProcess(pi.hProcess,&code));
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        ASSERT_EQ(wait,WAIT_OBJECT_0);
        std::ifstream bin(root+"\\shutdown-exception.bin",std::ios::binary);
        if (!std::strcmp(mode,"normal")) {
            EXPECT_EQ(code,0u); EXPECT_EQ(bin.peek(),std::char_traits<char>::eof());
        } else {
            EXPECT_EQ(code,static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));
            NativeShutdownDiagnostics::Evidence e={};
            bin.read(reinterpret_cast<char *>(&e),sizeof(e));
            ASSERT_EQ(bin.gcount(),sizeof(e));
            EXPECT_EQ(e.version,1u); EXPECT_EQ(e.size,sizeof(e));
            EXPECT_EQ(e.exception.ExceptionCode,static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));
            EXPECT_EQ(e.exception.ExceptionInformation[0],1u); // write
            EXPECT_EQ(e.exception.ExceptionInformation[1],1u); // target
            EXPECT_EQ(reinterpret_cast<ULONG_PTR>(e.exception.ExceptionAddress),e.context.Rip);
            EXPECT_EQ(e.executableBase,e.faultModuleBase);
            EXPECT_NE(e.context.Rsp,0u); EXPECT_GT(e.stackBytes,0u);
            EXPECT_LE(e.stackBytes,sizeof(e.stack)); EXPECT_NE(e.faultModule[0],0);
            EXPECT_EQ(bin.peek(),std::char_traits<char>::eof()); // one bounded record
        }
        bin.close();
        std::ifstream log(root+"\\startup.log");
        std::string text((std::istreambuf_iterator<char>(log)),{}); log.close();
        EXPECT_NE(text.find("WinMain\tcomplete"),std::string::npos);
        EXPECT_EQ(text.find("native_shutdown\texception")!=std::string::npos,std::strcmp(mode,"normal")!=0);
        ASSERT_TRUE(DeleteFileA((root+"\\startup.log").c_str()));
        ASSERT_TRUE(DeleteFileA((root+"\\shutdown-exception.bin").c_str()));
        ASSERT_TRUE(RemoveDirectoryA(dump.c_str())); ASSERT_TRUE(RemoveDirectoryA(data.c_str()));
        ASSERT_TRUE(RemoveDirectoryA(root.c_str()));
    }
}
#endif

TEST(NativeStartupGate, GrammarExcludesSimulationAndAmbiguousFlags) {
    char exe[]="probe", head[]="-headless", flag[]="-nativeInitOnly", root[]="C:\\probe";
    char replay[]="-simulateReplays";
    char *valid[]={exe,head,flag,root};
    EXPECT_TRUE(StartupDiagnostics::validArguments(4,valid));
    EXPECT_FALSE(StartupDiagnostics::validArguments(3,valid));
    char *invalid[]={exe,replay,flag,root};
    EXPECT_FALSE(StartupDiagnostics::validArguments(4,invalid));
    char *extra[]={exe,head,flag,root,replay};
    EXPECT_FALSE(StartupDiagnostics::validArguments(5,extra));
}
TEST(NativeStartupGate, DefaultOffAndInvalidRequest) {
    auto &s=StartupDiagnostics::state(); s={};
    char exe[]="probe", flag[]="-nativeInitOnly"; char *normal[]={exe};
    EXPECT_TRUE(StartupDiagnostics::configure(1,normal));
    EXPECT_FALSE(StartupDiagnostics::enabled());
    EXPECT_EQ(StartupDiagnostics::userData(),nullptr);
    s={}; char *bad[]={exe,flag};
    EXPECT_FALSE(StartupDiagnostics::configure(2,bad));
    EXPECT_FALSE(StartupDiagnostics::enabled()); s={};
}
TEST(NativeStartupGate, FlushedIsolatedLogAndFailClosedCapacity) {
    char temp[MAX_PATH], unique[MAX_PATH];
    ASSERT_NE(GetTempPathA(MAX_PATH,temp),0u);
    ASSERT_NE(GetTempFileNameA(temp,"x12",0,unique),0u);
    ASSERT_TRUE(DeleteFileA(unique)); ASSERT_TRUE(CreateDirectoryA(unique,nullptr));
    std::string root(unique), data=root+"\\userdata", log=root+"\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(),nullptr));
    std::string dump=data+"\\CrashDumps";
    ASSERT_TRUE(CreateDirectoryA(dump.c_str(),nullptr));
    char exe[]="probe",head[]="-headless",flag[]="-nativeInitOnly";
    char *args[]={exe,head,flag,unique};
    auto &s=StartupDiagnostics::state();s={};
#if defined(_WIN64)
    ASSERT_TRUE(StartupDiagnostics::configure(4,args));
    EXPECT_EQ(std::string(StartupDiagnostics::userData()),data+"\\");
    { StartupDiagnostics::Phase phase("test"); }
    // Read while writer remains open: checkpoint has reached the file already.
    std::ifstream in(log); std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
    EXPECT_NE(text.find("test\tbegin"),std::string::npos);
    EXPECT_NE(text.find("test\tcomplete"),std::string::npos);
    s.records=2048; StartupDiagnostics::checkpoint("overflow","begin");
    EXPECT_FALSE(StartupDiagnostics::healthy());
    StartupDiagnostics::close();s={};
    EXPECT_FALSE(StartupDiagnostics::configure(4,args)); // CREATE_NEW, never overwrite evidence.
    EXPECT_FALSE(StartupDiagnostics::enabled());s={};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
#else
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));s={};
#endif
    ASSERT_TRUE(RemoveDirectoryA(dump.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(data.c_str())); ASSERT_TRUE(RemoveDirectoryA(root.c_str()));
}

TEST(NativeGraphicsGate, StrictSeparateGrammarAndArchitecture) {
    char exe[]="probe",head[]="-headless",flag[]="-nativeGraphicsSmoke",old[]="-nativeInitOnly",root[]="C:\\probe";
    char *args[]={exe,head,flag,root};
    EXPECT_TRUE(StartupDiagnostics::validGraphicsArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validArguments(4,args));
    char *oldArgs[]={exe,head,old,root};
    EXPECT_TRUE(StartupDiagnostics::validArguments(4,oldArgs));
    EXPECT_FALSE(StartupDiagnostics::validGraphicsArguments(4,oldArgs));
    char *mixed[]={exe,head,flag,root,old};
    EXPECT_FALSE(StartupDiagnostics::validGraphicsArguments(5,mixed));
    EXPECT_FALSE(StartupDiagnostics::validGraphicsArguments(3,args));
    StartupDiagnostics::state()={};
    EXPECT_FALSE(StartupDiagnostics::configure(5,mixed));
    EXPECT_FALSE(StartupDiagnostics::enabled());
    StartupDiagnostics::state()={};
#if !defined(_WIN64)
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
    EXPECT_FALSE(StartupDiagnostics::graphicsSmoke());
#endif
    StartupDiagnostics::state()={};
}
TEST(NativeGraphicsGate, FlushedModeUsesSameIsolatedPathsWithoutInitOnlySuccess) {
    char temp[MAX_PATH],root[MAX_PATH]; ASSERT_NE(GetTempPathA(MAX_PATH,temp),0u);
    ASSERT_NE(GetTempFileNameA(temp,"x16",0,root),0u); ASSERT_TRUE(DeleteFileA(root));
    ASSERT_TRUE(CreateDirectoryA(root,nullptr));
    std::string data=std::string(root)+"\\userdata",dump=data+"\\CrashDumps",log=std::string(root)+"\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(),nullptr)); ASSERT_TRUE(CreateDirectoryA(dump.c_str(),nullptr));
    char exe[]="probe",head[]="-headless",flag[]="-nativeGraphicsSmoke"; char *args[]={exe,head,flag,root};
    StartupDiagnostics::state()={};
#if defined(_WIN64)
    ASSERT_TRUE(StartupDiagnostics::configure(4,args)); EXPECT_TRUE(StartupDiagnostics::graphicsSmoke());
    EXPECT_EQ(std::string(StartupDiagnostics::userData()),data+"\\");
    StartupDiagnostics::checkpoint("graphics_boundary","complete","execute/replay/simulation skipped");
    std::ifstream in(log); std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
    EXPECT_NE(text.find("graphics_boundary\tcomplete"),std::string::npos);
    EXPECT_EQ(text.find("init_only\tcomplete"),std::string::npos);
    StartupDiagnostics::close(); ASSERT_TRUE(DeleteFileA(log.c_str()));
#else
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
#endif
    StartupDiagnostics::state()={};
    ASSERT_TRUE(RemoveDirectoryA(dump.c_str())); ASSERT_TRUE(RemoveDirectoryA(data.c_str())); ASSERT_TRUE(RemoveDirectoryA(root));
}
TEST(NativeDisplayGate, SeparateStrictGrammarAndArchitecture) {
    char exe[]="probe",head[]="-headless",flag[]="-nativeDisplaySmoke",old[]="-nativeGraphicsSmoke",root[]="C:\\probe";
    char *args[]={exe,head,flag,root};
    EXPECT_TRUE(StartupDiagnostics::validDisplayArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validGraphicsArguments(4,args));EXPECT_FALSE(StartupDiagnostics::validArguments(4,args));
    char *oldArgs[]={exe,head,old,root};EXPECT_FALSE(StartupDiagnostics::validDisplayArguments(4,oldArgs));
    char *mixed[]={exe,head,flag,root,old};
    StartupDiagnostics::state()={};EXPECT_FALSE(StartupDiagnostics::configure(5,mixed));EXPECT_FALSE(StartupDiagnostics::enabled());
    EXPECT_FALSE(StartupDiagnostics::validDisplayArguments(3,args));
    StartupDiagnostics::state()={};
#if !defined(_WIN64)
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));EXPECT_FALSE(StartupDiagnostics::displaySmoke());
#endif
    StartupDiagnostics::state()={};
}
TEST(NativeDisplayGate, SameFlushedIsolationWithDistinctMode) {
    char temp[MAX_PATH],root[MAX_PATH];ASSERT_NE(GetTempPathA(MAX_PATH,temp),0u);
    ASSERT_NE(GetTempFileNameA(temp,"x17",0,root),0u);ASSERT_TRUE(DeleteFileA(root));ASSERT_TRUE(CreateDirectoryA(root,nullptr));
    std::string data=std::string(root)+"\\userdata",dump=data+"\\CrashDumps",log=std::string(root)+"\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(),nullptr));ASSERT_TRUE(CreateDirectoryA(dump.c_str(),nullptr));
    char exe[]="probe",head[]="-headless",flag[]="-nativeDisplaySmoke";char *args[]={exe,head,flag,root};
    StartupDiagnostics::state()={};
#if defined(_WIN64)
    ASSERT_TRUE(StartupDiagnostics::configure(4,args));EXPECT_TRUE(StartupDiagnostics::displaySmoke());EXPECT_FALSE(StartupDiagnostics::graphicsSmoke());
    EXPECT_EQ(std::string(StartupDiagnostics::userData()),data+"\\");
    StartupDiagnostics::checkpoint("display_attach","complete");StartupDiagnostics::close();
    std::ifstream in(log);std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
    EXPECT_NE(text.find("display_attach\tcomplete"),std::string::npos);ASSERT_TRUE(DeleteFileA(log.c_str()));
#else
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
#endif
    StartupDiagnostics::state()={};ASSERT_TRUE(RemoveDirectoryA(dump.c_str()));ASSERT_TRUE(RemoveDirectoryA(data.c_str()));ASSERT_TRUE(RemoveDirectoryA(root));
}

TEST(NativeUIImageGate, IndependentGrammarRejectsMixedOrWin32Execution) {
    char exe[]="probe",head[]="-headless",flag[]="-nativeUIImageSmoke",root[]="C:\\probe";
    char old[]="-nativeDisplaySmoke", replay[]="-simulateReplays";
    char *args[]={exe,head,flag,root};
    EXPECT_TRUE(StartupDiagnostics::validUIImageArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validDisplayArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validGraphicsArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validUIImageArguments(3,args));
    char *extra[]={exe,head,flag,root,replay};
    EXPECT_FALSE(StartupDiagnostics::validUIImageArguments(5,extra));
    StartupDiagnostics::state()={}; EXPECT_FALSE(StartupDiagnostics::configure(5,extra));
    char *prior[]={exe,head,old,root}; EXPECT_FALSE(StartupDiagnostics::validUIImageArguments(4,prior));
    StartupDiagnostics::state()={};
#if !defined(_WIN64)
    EXPECT_FALSE(StartupDiagnostics::configure(4,args)); EXPECT_FALSE(StartupDiagnostics::uiImageSmoke());
#endif
    StartupDiagnostics::state()={};
}

TEST(NativeUIImageGate, FlushedIsolationWithDistinctMode) {
    char temp[MAX_PATH],root[MAX_PATH];ASSERT_NE(GetTempPathA(MAX_PATH,temp),0u);
    ASSERT_NE(GetTempFileNameA(temp,"x18",0,root),0u);ASSERT_TRUE(DeleteFileA(root));ASSERT_TRUE(CreateDirectoryA(root,nullptr));
    std::string data=std::string(root)+"\\userdata",dump=data+"\\CrashDumps",log=std::string(root)+"\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(),nullptr));ASSERT_TRUE(CreateDirectoryA(dump.c_str(),nullptr));
    char exe[]="probe",head[]="-headless",flag[]="-nativeUIImageSmoke";char *args[]={exe,head,flag,root};
    StartupDiagnostics::state()={};
#if defined(_WIN64)
    ASSERT_TRUE(StartupDiagnostics::configure(4,args));EXPECT_TRUE(StartupDiagnostics::displaySmoke());EXPECT_FALSE(StartupDiagnostics::graphicsSmoke());EXPECT_TRUE(StartupDiagnostics::uiImageSmoke());
    EXPECT_EQ(std::string(StartupDiagnostics::userData()),data+"\\");
    StartupDiagnostics::checkpoint("display_attach","complete");StartupDiagnostics::close();
    std::ifstream in(log);std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
    EXPECT_NE(text.find("display_attach\tcomplete"),std::string::npos);ASSERT_TRUE(DeleteFileA(log.c_str()));
#else
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
#endif
    StartupDiagnostics::state()={};ASSERT_TRUE(RemoveDirectoryA(dump.c_str()));ASSERT_TRUE(RemoveDirectoryA(data.c_str()));ASSERT_TRUE(RemoveDirectoryA(root));
}

TEST(NativeUISentenceGate, StrictGrammarAndIndependentModes) {
    char exe[]="probe", head[]="-headless", flag[]="-nativeUISentenceSmoke", root[]="C:\\reviewed", extra[]="-replay";
    char *args[]={exe,head,flag,root,extra};
    EXPECT_TRUE(StartupDiagnostics::validUISentenceArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validUIImageArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validDisplayArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validUISentenceArguments(5,args));
    EXPECT_FALSE(StartupDiagnostics::validUISentenceArguments(3,args));
    StartupDiagnostics::state()={};
#if !defined(_WIN64)
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
    EXPECT_FALSE(StartupDiagnostics::uiSentenceSmoke());
#endif
    StartupDiagnostics::state()={};
}
TEST(NativeUISentenceGate, FlushedIsolationWithDistinctMode) {
    char temp[MAX_PATH],root[MAX_PATH];ASSERT_NE(GetTempPathA(MAX_PATH,temp),0u);
    ASSERT_NE(GetTempFileNameA(temp,"x19",0,root),0u);ASSERT_TRUE(DeleteFileA(root));ASSERT_TRUE(CreateDirectoryA(root,nullptr));
    std::string data=std::string(root)+"\\userdata",dump=data+"\\CrashDumps",log=std::string(root)+"\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(),nullptr));ASSERT_TRUE(CreateDirectoryA(dump.c_str(),nullptr));
    char exe[]="probe",head[]="-headless",flag[]="-nativeUISentenceSmoke";char *args[]={exe,head,flag,root};
    StartupDiagnostics::state()={};
#if defined(_WIN64)
    ASSERT_TRUE(StartupDiagnostics::configure(4,args));EXPECT_TRUE(StartupDiagnostics::displaySmoke());EXPECT_FALSE(StartupDiagnostics::graphicsSmoke());EXPECT_TRUE(StartupDiagnostics::uiSentenceSmoke());
    EXPECT_EQ(std::string(StartupDiagnostics::userData()),data+"\\");
    StartupDiagnostics::checkpoint("display_attach","complete");StartupDiagnostics::close();
    std::ifstream in(log);std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
    EXPECT_NE(text.find("display_attach\tcomplete"),std::string::npos);ASSERT_TRUE(DeleteFileA(log.c_str()));
#else
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
#endif
    StartupDiagnostics::state()={};ASSERT_TRUE(RemoveDirectoryA(dump.c_str()));ASSERT_TRUE(RemoveDirectoryA(data.c_str()));ASSERT_TRUE(RemoveDirectoryA(root));
}

TEST(NativeUIPanelGate, StrictGrammarAndIndependentModes) {
    char exe[]="probe", head[]="-headless", flag[]="-nativeUIPanelSmoke", root[]="C:\\reviewed", extra[]="-replay";
    char *args[]={exe,head,flag,root,extra};
    EXPECT_TRUE(StartupDiagnostics::validUIPanelArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validUIImageArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validDisplayArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validUIPanelArguments(5,args));
    EXPECT_FALSE(StartupDiagnostics::validUIPanelArguments(3,args));
    StartupDiagnostics::state()={};
#if !defined(_WIN64)
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
    EXPECT_FALSE(StartupDiagnostics::uiPanelSmoke());
#endif
    StartupDiagnostics::state()={};
}
TEST(NativeUIPanelGate, FlushedIsolationWithDistinctMode) {
    char temp[MAX_PATH],root[MAX_PATH];ASSERT_NE(GetTempPathA(MAX_PATH,temp),0u);
    ASSERT_NE(GetTempFileNameA(temp,"x20",0,root),0u);ASSERT_TRUE(DeleteFileA(root));ASSERT_TRUE(CreateDirectoryA(root,nullptr));
    std::string data=std::string(root)+"\\userdata",dump=data+"\\CrashDumps",log=std::string(root)+"\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(),nullptr));ASSERT_TRUE(CreateDirectoryA(dump.c_str(),nullptr));
    char exe[]="probe",head[]="-headless",flag[]="-nativeUIPanelSmoke";char *args[]={exe,head,flag,root};
    StartupDiagnostics::state()={};
#if defined(_WIN64)
    ASSERT_TRUE(StartupDiagnostics::configure(4,args));EXPECT_TRUE(StartupDiagnostics::displaySmoke());EXPECT_FALSE(StartupDiagnostics::graphicsSmoke());EXPECT_TRUE(StartupDiagnostics::uiPanelSmoke());
    EXPECT_EQ(std::string(StartupDiagnostics::userData()),data+"\\");
    StartupDiagnostics::checkpoint("display_attach","complete");StartupDiagnostics::close();
    std::ifstream in(log);std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
    EXPECT_NE(text.find("display_attach\tcomplete"),std::string::npos);ASSERT_TRUE(DeleteFileA(log.c_str()));
#else
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
#endif
    StartupDiagnostics::state()={};ASSERT_TRUE(RemoveDirectoryA(dump.c_str()));ASSERT_TRUE(RemoveDirectoryA(data.c_str()));ASSERT_TRUE(RemoveDirectoryA(root));
}

TEST(NativeUIMenuGate, StrictGrammarAndIndependentModes) {
    char exe[]="probe", head[]="-headless", flag[]="-nativeUIMenuSmoke", root[]="C:\\reviewed", extra[]="-replay";
    char *args[]={exe,head,flag,root,extra};
    EXPECT_TRUE(StartupDiagnostics::validUIMenuArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validUIImageArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validDisplayArguments(4,args));
    EXPECT_FALSE(StartupDiagnostics::validUIMenuArguments(5,args));
    EXPECT_FALSE(StartupDiagnostics::validUIMenuArguments(3,args));
    StartupDiagnostics::state()={};
#if !defined(_WIN64)
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
    EXPECT_FALSE(StartupDiagnostics::uiMenuSmoke());
#endif
    StartupDiagnostics::state()={};
}
TEST(NativeUIMenuGate, FlushedIsolationWithDistinctMode) {
    char temp[MAX_PATH],root[MAX_PATH];ASSERT_NE(GetTempPathA(MAX_PATH,temp),0u);
    ASSERT_NE(GetTempFileNameA(temp,"x21",0,root),0u);ASSERT_TRUE(DeleteFileA(root));ASSERT_TRUE(CreateDirectoryA(root,nullptr));
    std::string data=std::string(root)+"\\userdata",dump=data+"\\CrashDumps",log=std::string(root)+"\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(),nullptr));ASSERT_TRUE(CreateDirectoryA(dump.c_str(),nullptr));
    char exe[]="probe",head[]="-headless",flag[]="-nativeUIMenuSmoke";char *args[]={exe,head,flag,root};
    StartupDiagnostics::state()={};
#if defined(_WIN64)
    ASSERT_TRUE(StartupDiagnostics::configure(4,args));EXPECT_TRUE(StartupDiagnostics::displaySmoke());EXPECT_FALSE(StartupDiagnostics::graphicsSmoke());EXPECT_TRUE(StartupDiagnostics::uiMenuSmoke());
    EXPECT_EQ(std::string(StartupDiagnostics::userData()),data+"\\");
    StartupDiagnostics::checkpoint("display_attach","complete");StartupDiagnostics::close();
    std::ifstream in(log);std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
    EXPECT_NE(text.find("display_attach\tcomplete"),std::string::npos);ASSERT_TRUE(DeleteFileA(log.c_str()));
#else
    EXPECT_FALSE(StartupDiagnostics::configure(4,args));
#endif
    StartupDiagnostics::state()={};ASSERT_TRUE(RemoveDirectoryA(dump.c_str()));ASSERT_TRUE(RemoveDirectoryA(data.c_str()));ASSERT_TRUE(RemoveDirectoryA(root));
}

// ---------------------------------------------------------------------------
// Ordinary-host profile (X64.22): strict grammar, window geometry, no-D3D8 audit.
// ---------------------------------------------------------------------------
namespace {
struct HostArgs {
    char root[MAX_PATH] = "C:\\probe";
    const char *tokens[11] = { "probe", "-win", "-xres", "800", "-yres", "600", "-nologo",
        "-noshellmap", "-winCursors", "-nativeOrdinaryHost", root };
    bool valid(int count = 11) {
        char *copy[12] = {};
        for (int i = 0; i < count && i < 11; ++i) copy[i] = const_cast<char *>(tokens[i]);
        return StartupDiagnostics::validOrdinaryHostArguments(count, copy);
    }
};
}

TEST(NativeOrdinaryHostGate, ExactGrammarAcceptedOnlyForGenerals) {
    HostArgs args;
#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR
    EXPECT_FALSE(args.valid());
#else
    EXPECT_TRUE(args.valid());
    // Case-insensitive option names, exact values.
    args.tokens[1] = "-WIN"; EXPECT_TRUE(args.valid()); args.tokens[1] = "-win";
#endif
}

TEST(NativeOrdinaryHostGate, RejectsEveryMissingChangedReorderedOrExtraToken) {
    for (int position = 1; position <= 9; ++position) {
        HostArgs args;
        args.tokens[position] = "-other";
        EXPECT_FALSE(args.valid()) << "changed token " << position;
    }
    { HostArgs a; a.tokens[3] = "1024"; EXPECT_FALSE(a.valid()); }
    { HostArgs a; a.tokens[3] = "0800"; EXPECT_FALSE(a.valid()); }
    { HostArgs a; a.tokens[5] = "768"; EXPECT_FALSE(a.valid()); }
    { HostArgs a; EXPECT_FALSE(a.valid(10)); }              // missing root
    { HostArgs a; std::swap(a.tokens[2], a.tokens[4]); EXPECT_FALSE(a.valid()); } // reordered
    { // extra flag after the root (gameplay/replay/developer switches)
        HostArgs a; char probe[] = "-simulateReplays";
        char *copy[12] = {};
        for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(a.tokens[i]);
        copy[11] = probe;
        EXPECT_FALSE(StartupDiagnostics::validOrdinaryHostArguments(12, copy));
    }
    { // fullscreen request is not coerced to windowed
        HostArgs a; a.tokens[1] = "-fullscreen"; EXPECT_FALSE(a.valid());
    }
    { // existing diagnostic grammars are distinct and unchanged
        char exe[] = "probe", head[] = "-headless", flag[] = "-nativeUIMenuSmoke", root[] = "C:\\probe";
        char *menu[] = { exe, head, flag, root };
        EXPECT_TRUE(StartupDiagnostics::validUIMenuArguments(4, menu));
        EXPECT_FALSE(StartupDiagnostics::validOrdinaryHostArguments(4, menu));
    }
}

#if defined(_WIN64) && !(defined(RTS_ZEROHOUR) && RTS_ZEROHOUR)
TEST(NativeOrdinaryHostGate, ConfigureEnablesIsolatedProfileWithoutDiagnosticModes) {
    char temp[MAX_PATH], unique[MAX_PATH];
    ASSERT_NE(GetTempPathA(MAX_PATH, temp), 0u);
    ASSERT_NE(GetTempFileNameA(temp, "x22", 0, unique), 0u);
    ASSERT_TRUE(DeleteFileA(unique)); ASSERT_TRUE(CreateDirectoryA(unique, nullptr));
    std::string root(unique), data = root + "\\userdata", dump = data + "\\CrashDumps", log = root + "\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(), nullptr)); ASSERT_TRUE(CreateDirectoryA(dump.c_str(), nullptr));
    HostArgs args;
    std::strcpy(args.root, unique);
    char *copy[11];
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    StartupDiagnostics::state() = {};
    ASSERT_TRUE(StartupDiagnostics::configure(11, copy));
    EXPECT_TRUE(StartupDiagnostics::enabled());
    EXPECT_TRUE(StartupDiagnostics::ordinaryHost());
    EXPECT_FALSE(StartupDiagnostics::displaySmoke());
    EXPECT_FALSE(StartupDiagnostics::graphicsSmoke());
    EXPECT_FALSE(StartupDiagnostics::uiMenuSmoke());
    EXPECT_EQ(std::string(StartupDiagnostics::userData()), data + "\\");
    StartupDiagnostics::markFailed();
    EXPECT_FALSE(StartupDiagnostics::healthy()); // residency/contract failures become exit 1
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    // A relative root is rejected and nothing is enabled.
    std::strcpy(args.root, "relative"); copy[10] = args.root;
    EXPECT_FALSE(StartupDiagnostics::configure(11, copy));
    EXPECT_FALSE(StartupDiagnostics::ordinaryHost());
    StartupDiagnostics::state() = {};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(dump.c_str())); ASSERT_TRUE(RemoveDirectoryA(data.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(root.c_str()));
}
#endif

#if !defined(_WIN64)
TEST(NativeOrdinaryHostGate, Win32NeverEnablesTheProfile) {
    HostArgs args;
    char *copy[11];
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    StartupDiagnostics::state() = {};
    EXPECT_FALSE(StartupDiagnostics::configure(11, copy));
    EXPECT_FALSE(StartupDiagnostics::ordinaryHost());
    StartupDiagnostics::state() = {};
}
#endif

TEST(NativeOrdinaryHostWindow, ClientAreaIsExactlyTheProfileSizeWithLegacyStyle) {
    NativeOrdinaryHost::WindowGeometry geometry = {};
    ASSERT_TRUE(NativeOrdinaryHost::computeWindowGeometry(1920, 1080, geometry));
    EXPECT_EQ(geometry.style, static_cast<DWORD>(WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_SYSMENU | WS_DLGFRAME | WS_CAPTION));
    EXPECT_EQ(geometry.x, 1920 / 2 - 400);
    EXPECT_EQ(geometry.y, 1080 / 2 - 300);
    EXPECT_GT(geometry.width, 800);
    EXPECT_GT(geometry.height, 600);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "NativeOrdinaryHostWindowTest";
    ASSERT_NE(RegisterClassA(&wc), 0);
    // Hidden: this verifies geometry only; the visible style bit is applied by WinMain.
    HWND window = CreateWindowA(wc.lpszClassName, "t", geometry.style & ~static_cast<DWORD>(WS_VISIBLE),
        geometry.x, geometry.y, geometry.width, geometry.height, nullptr, nullptr, wc.hInstance, nullptr);
    ASSERT_NE(window, nullptr);
    EXPECT_TRUE(NativeOrdinaryHost::clientIsExact(window));
    EXPECT_TRUE(SetWindowPos(window, nullptr, 0, 0, geometry.width + 1, geometry.height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    EXPECT_FALSE(NativeOrdinaryHost::clientIsExact(window)); // resize is detected, never coerced
    EXPECT_FALSE(NativeOrdinaryHost::clientIsExact(nullptr));
    DestroyWindow(window);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
}

TEST(NativeOrdinaryHostAudit, LegacyModulesAreDetectedAndTestProcessHasNone) {
    const NativeOrdinaryHost::ModuleAudit audit = NativeOrdinaryHost::auditLoadedModules();
    EXPECT_FALSE(audit.d3d8);   // this process never touches the DX8 route
    EXPECT_FALSE(audit.d3dx8);
    EXPECT_TRUE(NativeOrdinaryHost::auditAcceptable(audit));
    NativeOrdinaryHost::ModuleAudit bad = audit; bad.d3d8 = true;
    EXPECT_FALSE(NativeOrdinaryHost::auditAcceptable(bad));
    bad = audit; bad.d3dx8 = true;
    EXPECT_FALSE(NativeOrdinaryHost::auditAcceptable(bad));
    EXPECT_TRUE(NativeOrdinaryHost::residencyReleasedBeforeBackend(0));
    EXPECT_FALSE(NativeOrdinaryHost::residencyReleasedBeforeBackend(1));
}

// ---------------------------------------------------------------------------
// Ordinary-menu profile (X64.23): same strict profile, distinct mode token.
// ---------------------------------------------------------------------------
TEST(NativeOrdinaryMenuGate, DistinctStrictTokenSharesTheHostGrammar) {
    HostArgs args;
    char *copy[12] = {};
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    // The host token is not the menu token and vice versa.
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryMenuArguments(11, copy));
    char menu[] = "-nativeOrdinaryMenu";
    copy[9] = menu;
#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryMenuArguments(11, copy));
#else
    EXPECT_TRUE(StartupDiagnostics::validOrdinaryMenuArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryHostArguments(11, copy));
    // Every other token is as strict as for the host profile.
    for (int position = 1; position <= 8; ++position) {
        char *mutated[12] = {};
        for (int i = 0; i < 11; ++i) mutated[i] = copy[i];
        char other[] = "-other"; mutated[position] = other;
        EXPECT_FALSE(StartupDiagnostics::validOrdinaryMenuArguments(11, mutated)) << position;
    }
    char extra[] = "-simulateReplays"; copy[11] = extra;
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryMenuArguments(12, copy));
#endif
}

#if defined(_WIN64) && !(defined(RTS_ZEROHOUR) && RTS_ZEROHOUR)
TEST(NativeOrdinaryMenuGate, ConfigureEnablesMenuImpliesHostAndNoDiagnosticModes) {
    char temp[MAX_PATH], unique[MAX_PATH];
    ASSERT_NE(GetTempPathA(MAX_PATH, temp), 0u);
    ASSERT_NE(GetTempFileNameA(temp, "x23", 0, unique), 0u);
    ASSERT_TRUE(DeleteFileA(unique)); ASSERT_TRUE(CreateDirectoryA(unique, nullptr));
    std::string root(unique), data = root + "\\userdata", dump = data + "\\CrashDumps", log = root + "\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(), nullptr)); ASSERT_TRUE(CreateDirectoryA(dump.c_str(), nullptr));
    HostArgs args;
    std::strcpy(args.root, unique);
    char menu[] = "-nativeOrdinaryMenu";
    char *copy[11];
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    copy[9] = menu;
    StartupDiagnostics::state() = {};
    ASSERT_TRUE(StartupDiagnostics::configure(11, copy));
    EXPECT_TRUE(StartupDiagnostics::ordinaryMenu());
    EXPECT_TRUE(StartupDiagnostics::ordinaryHost()); // shared display/window lifetime
    EXPECT_FALSE(StartupDiagnostics::displaySmoke());
    EXPECT_FALSE(StartupDiagnostics::uiMenuSmoke());
    EXPECT_EQ(std::string(StartupDiagnostics::userData()), data + "\\");
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    // Host mode never implies the menu loop.
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    copy[9] = const_cast<char *>("-nativeOrdinaryHost");
    ASSERT_TRUE(StartupDiagnostics::configure(11, copy));
    EXPECT_TRUE(StartupDiagnostics::ordinaryHost());
    EXPECT_FALSE(StartupDiagnostics::ordinaryMenu());
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(dump.c_str())); ASSERT_TRUE(RemoveDirectoryA(data.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(root.c_str()));
}
#endif

// ---------------------------------------------------------------------------
// Ordinary-game profile (X64.24): same strict profile, third distinct token.
// ---------------------------------------------------------------------------
TEST(NativeOrdinaryGameGate, RepeatedReceiptsAreBoundedBelowTheLogCapacity) {
    // Regression: X64.24 run 20261012T2112467768379Z filled the 2048-record log with
    // one receipt per logic step and lost its tail evidence.
    StartupDiagnostics::RepeatGate gate;
    unsigned admitted = 0;
    for (unsigned i = 0; i < 1000000; ++i) admitted += gate.admit(32) ? 1u : 0u;
    EXPECT_EQ(admitted, 32u);
    EXPECT_EQ(gate.calls, 1000000u);
    StartupDiagnostics::RepeatGate other;
    EXPECT_TRUE(other.admit(1));
    EXPECT_FALSE(other.admit(1));
}

TEST(NativeOrdinaryGameGate, DistinctStrictTokenAndImplications) {
    HostArgs args;
    char *copy[12] = {};
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    char game[] = "-nativeOrdinaryGame";
    copy[9] = game;
#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryGameArguments(11, copy));
#else
    EXPECT_TRUE(StartupDiagnostics::validOrdinaryGameArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryMenuArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryHostArguments(11, copy));
    char extra[] = "-file"; copy[11] = extra;
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryGameArguments(12, copy)); // no map/scenario switch
#endif
}

#if defined(_WIN64) && !(defined(RTS_ZEROHOUR) && RTS_ZEROHOUR)
TEST(NativeOrdinaryGameGate, ConfigureImpliesMenuAndHostButNeverTheReverse) {
    char temp[MAX_PATH], unique[MAX_PATH];
    ASSERT_NE(GetTempPathA(MAX_PATH, temp), 0u);
    ASSERT_NE(GetTempFileNameA(temp, "x24", 0, unique), 0u);
    ASSERT_TRUE(DeleteFileA(unique)); ASSERT_TRUE(CreateDirectoryA(unique, nullptr));
    std::string root(unique), data = root + "\\userdata", dump = data + "\\CrashDumps", log = root + "\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(), nullptr)); ASSERT_TRUE(CreateDirectoryA(dump.c_str(), nullptr));
    HostArgs args;
    std::strcpy(args.root, unique);
    char *copy[11];
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    copy[9] = const_cast<char *>("-nativeOrdinaryGame");
    StartupDiagnostics::state() = {};
    ASSERT_TRUE(StartupDiagnostics::configure(11, copy));
    EXPECT_TRUE(StartupDiagnostics::ordinaryGame());
    EXPECT_TRUE(StartupDiagnostics::ordinaryMenu());
    EXPECT_TRUE(StartupDiagnostics::ordinaryHost());
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    copy[9] = const_cast<char *>("-nativeOrdinaryMenu");
    ASSERT_TRUE(StartupDiagnostics::configure(11, copy));
    EXPECT_TRUE(StartupDiagnostics::ordinaryMenu());
    EXPECT_FALSE(StartupDiagnostics::ordinaryGame()); // menu never opens GameLogic
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(dump.c_str())); ASSERT_TRUE(RemoveDirectoryA(data.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(root.c_str()));
}
#endif

// ---------------------------------------------------------------------------
// Ordinary-terrain profile (X64.25): fourth distinct token; implies game/menu/host.
// ---------------------------------------------------------------------------
TEST(NativeOrdinaryTerrainGate, DistinctStrictToken) {
    HostArgs args;
    char *copy[12] = {};
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    char terrain[] = "-nativeOrdinaryTerrain";
    copy[9] = terrain;
#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryTerrainArguments(11, copy));
#else
    EXPECT_TRUE(StartupDiagnostics::validOrdinaryTerrainArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryGameArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryMenuArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryHostArguments(11, copy));
    char extra[] = "-file"; copy[11] = extra;
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryTerrainArguments(12, copy));
#endif
}

#if defined(_WIN64) && !(defined(RTS_ZEROHOUR) && RTS_ZEROHOUR)
TEST(NativeOrdinaryTerrainGate, ConfigureImpliesGameStackAndGameNeverImpliesTerrain) {
    char temp[MAX_PATH], unique[MAX_PATH];
    ASSERT_NE(GetTempPathA(MAX_PATH, temp), 0u);
    ASSERT_NE(GetTempFileNameA(temp, "x25", 0, unique), 0u);
    ASSERT_TRUE(DeleteFileA(unique)); ASSERT_TRUE(CreateDirectoryA(unique, nullptr));
    std::string root(unique), data = root + "\\userdata", dump = data + "\\CrashDumps", log = root + "\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(), nullptr)); ASSERT_TRUE(CreateDirectoryA(dump.c_str(), nullptr));
    HostArgs args;
    std::strcpy(args.root, unique);
    char *copy[11];
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    copy[9] = const_cast<char *>("-nativeOrdinaryTerrain");
    StartupDiagnostics::state() = {};
    ASSERT_TRUE(StartupDiagnostics::configure(11, copy));
    EXPECT_TRUE(StartupDiagnostics::ordinaryTerrain());
    EXPECT_TRUE(StartupDiagnostics::ordinaryGame());
    EXPECT_TRUE(StartupDiagnostics::ordinaryMenu());
    EXPECT_TRUE(StartupDiagnostics::ordinaryHost());
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    copy[9] = const_cast<char *>("-nativeOrdinaryGame");
    ASSERT_TRUE(StartupDiagnostics::configure(11, copy));
    EXPECT_TRUE(StartupDiagnostics::ordinaryGame());
    EXPECT_FALSE(StartupDiagnostics::ordinaryTerrain()); // X64.24 profile never renders the world
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(dump.c_str())); ASSERT_TRUE(RemoveDirectoryA(data.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(root.c_str()));
}
#endif

// ---------------------------------------------------------------------------
// Ordinary-play profile (X64.28): fifth distinct token; implies terrain/game/menu/host.
// ---------------------------------------------------------------------------
TEST(NativeOrdinaryPlayGate, DistinctStrictToken) {
    HostArgs args;
    char *copy[12] = {};
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    char play[] = "-nativeOrdinaryPlay";
    copy[9] = play;
#if defined(RTS_ZEROHOUR) && RTS_ZEROHOUR
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryPlayArguments(11, copy));
#else
    EXPECT_TRUE(StartupDiagnostics::validOrdinaryPlayArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryTerrainArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryGameArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryMenuArguments(11, copy));
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryHostArguments(11, copy));
    char extra[] = "-file"; copy[11] = extra;
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryPlayArguments(12, copy));
    // The terrain token never validates as play.
    char terrain[] = "-nativeOrdinaryTerrain"; copy[9] = terrain;
    EXPECT_FALSE(StartupDiagnostics::validOrdinaryPlayArguments(11, copy));
#endif
}

#if defined(_WIN64) && !(defined(RTS_ZEROHOUR) && RTS_ZEROHOUR)
TEST(NativeOrdinaryPlayGate, ConfigureImpliesTerrainStackAndTerrainNeverImpliesPlay) {
    char temp[MAX_PATH], unique[MAX_PATH];
    ASSERT_NE(GetTempPathA(MAX_PATH, temp), 0u);
    ASSERT_NE(GetTempFileNameA(temp, "x28", 0, unique), 0u);
    ASSERT_TRUE(DeleteFileA(unique)); ASSERT_TRUE(CreateDirectoryA(unique, nullptr));
    std::string root(unique), data = root + "\\userdata", dump = data + "\\CrashDumps", log = root + "\\startup.log";
    ASSERT_TRUE(CreateDirectoryA(data.c_str(), nullptr)); ASSERT_TRUE(CreateDirectoryA(dump.c_str(), nullptr));
    HostArgs args;
    std::strcpy(args.root, unique);
    char *copy[11];
    for (int i = 0; i < 11; ++i) copy[i] = const_cast<char *>(args.tokens[i]);
    copy[9] = const_cast<char *>("-nativeOrdinaryPlay");
    StartupDiagnostics::state() = {};
    const bool configured = StartupDiagnostics::configure(11, copy);
    ASSERT_TRUE(configured) << "requested=" << StartupDiagnostics::state().requested
        << " valid=" << StartupDiagnostics::state().valid << " play=" << StartupDiagnostics::state().ordinaryPlay
        << " terrain=" << StartupDiagnostics::state().ordinaryTerrain << " root=" << copy[10];
    EXPECT_TRUE(StartupDiagnostics::ordinaryPlay());
    EXPECT_TRUE(StartupDiagnostics::ordinaryTerrain());
    EXPECT_TRUE(StartupDiagnostics::ordinaryGame());
    EXPECT_TRUE(StartupDiagnostics::ordinaryMenu());
    EXPECT_TRUE(StartupDiagnostics::ordinaryHost());
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    copy[9] = const_cast<char *>("-nativeOrdinaryTerrain");
    ASSERT_TRUE(StartupDiagnostics::configure(11, copy));
    EXPECT_TRUE(StartupDiagnostics::ordinaryTerrain());
    EXPECT_FALSE(StartupDiagnostics::ordinaryPlay()); // the accepted terrain profile stays scripted/bounded
    StartupDiagnostics::close(); StartupDiagnostics::state() = {};
    ASSERT_TRUE(DeleteFileA(log.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(dump.c_str())); ASSERT_TRUE(RemoveDirectoryA(data.c_str()));
    ASSERT_TRUE(RemoveDirectoryA(root.c_str()));
}
#endif
