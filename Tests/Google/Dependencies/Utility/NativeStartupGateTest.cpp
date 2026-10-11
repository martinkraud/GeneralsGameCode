// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "Common/StartupDiagnostics.h"
#include <fstream>
#include <string>

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
