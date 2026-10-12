// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "Common/StartupDiagnostics.h"
#include "Common/NativeShutdownDiagnostics.h"
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
