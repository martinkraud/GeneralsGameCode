// SPDX-License-Identifier: GPL-3.0-or-later
// Opt-in statistical profiler for the headless replay benchmark (GENERALS_SIM_SAMPLE=<output file>). A helper thread suspends the simulation thread every ~1 ms, records its
// instruction pointer and resumes it; at the end the samples are grouped by function (dbghelp, loaded dynamically, using the executable's PDB) and written as a table.
// It never touches simulation state: the sampled thread is only suspended for the microseconds of a GetThreadContext. Timing runs and sampling runs are kept apart.
#include "PreRTS.h"

#include "Common/SimSampler.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace SimSampler
{
namespace
{
std::atomic<bool> s_stop{false};
HANDLE s_thread = nullptr;
HANDLE s_target = nullptr;
std::vector<unsigned long long> s_samples;
std::string s_path;

typedef BOOL (WINAPI *SymInitializeFn)(HANDLE, PCSTR, BOOL);
typedef BOOL (WINAPI *SymFromAddrFn)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef DWORD (WINAPI *SymSetOptionsFn)(DWORD);
typedef BOOL (WINAPI *SymGetLineFn)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);

DWORD WINAPI samplerMain(LPVOID)
{
	s_samples.reserve(400000);
	while (!s_stop.load(std::memory_order_relaxed))
	{
		Sleep(1);
		if (SuspendThread(s_target) == static_cast<DWORD>(-1))
			continue;
		CONTEXT context = {};
		context.ContextFlags = CONTEXT_CONTROL;
		if (GetThreadContext(s_target, &context))
			s_samples.push_back(context.Rip);
		ResumeThread(s_target);
	}
	return 0;
}
}

bool start(const char *outputFile)
{
	if (!outputFile || !*outputFile)
		return false;
	s_path = outputFile;
	if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_target, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
		return false;
	s_stop = false;
	s_thread = CreateThread(nullptr, 0, samplerMain, nullptr, 0, nullptr);
	return s_thread != nullptr;
}

void stopAndReport()
{
	if (!s_thread)
		return;
	s_stop = true;
	WaitForSingleObject(s_thread, 5000);
	CloseHandle(s_thread);
	s_thread = nullptr;
	CloseHandle(s_target);
	s_target = nullptr;

	HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
	SymInitializeFn symInitialize = dbghelp ? reinterpret_cast<SymInitializeFn>(GetProcAddress(dbghelp, "SymInitialize")) : nullptr;
	SymFromAddrFn symFromAddr = dbghelp ? reinterpret_cast<SymFromAddrFn>(GetProcAddress(dbghelp, "SymFromAddr")) : nullptr;
	SymSetOptionsFn symSetOptions = dbghelp ? reinterpret_cast<SymSetOptionsFn>(GetProcAddress(dbghelp, "SymSetOptions")) : nullptr;
	SymGetLineFn symGetLine = dbghelp ? reinterpret_cast<SymGetLineFn>(GetProcAddress(dbghelp, "SymGetLineFromAddr64")) : nullptr;
	const HANDLE process = GetCurrentProcess();
	bool symbols = false;
	if (symInitialize && symFromAddr)
	{
		if (symSetOptions)
			symSetOptions(0x00000002 /*SYMOPT_UNDNAME*/ | 0x00000004 /*SYMOPT_DEFERRED_LOADS*/ | 0x00000010 /*SYMOPT_LOAD_LINES*/);
		symbols = symInitialize(process, nullptr, TRUE) != FALSE;
	}

	std::map<unsigned long long, unsigned> byAddress;
	for (unsigned long long rip : s_samples)
		++byAddress[rip];
	std::map<std::string, unsigned> byFunction, byLine;
	for (const auto &entry : byAddress)
	{
		std::string name = "?";
		if (symbols)
		{
			char buffer[sizeof(SYMBOL_INFO) + 256] = {};
			SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = 255;
			DWORD64 displacement = 0;
			if (symFromAddr(process, entry.first, &displacement, symbol))
				name = symbol->Name;
		}
		byFunction[name] += entry.second;
		if (symbols && symGetLine)
		{
			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(line);
			DWORD displacement = 0;
			if (symGetLine(process, entry.first, &displacement, &line))
			{
				const char *file = line.FileName ? line.FileName : "?";
				const char *slash = strrchr(file, 92);
				byLine[name + "  " + (slash ? slash + 1 : file) + ":" + std::to_string(line.LineNumber)] += entry.second;
			}
		}
	}
	std::vector<std::pair<unsigned, std::string>> ranked;
	for (const auto &entry : byFunction)
		ranked.push_back(std::make_pair(entry.second, entry.first));
	std::sort(ranked.begin(), ranked.end(), [](const std::pair<unsigned, std::string> &a, const std::pair<unsigned, std::string> &b) { return a.first > b.first; });

	if (FILE *f = fopen(s_path.c_str(), "wb"))
	{
		fprintf(f, "samples %zu (about 1 ms apart), symbols %s\n", s_samples.size(), symbols ? "yes" : "no");
		for (size_t i = 0; i < ranked.size() && i < 60; ++i)
			fprintf(f, "%6.2f%%  %7u  %s\n", 100.0 * ranked[i].first / static_cast<double>(s_samples.empty() ? 1 : s_samples.size()), ranked[i].first, ranked[i].second.c_str());
		fprintf(f, "\n--- hottest source lines ---\n");
		std::vector<std::pair<unsigned, std::string>> lines;
		for (const auto &entry : byLine)
			lines.push_back(std::make_pair(entry.second, entry.first));
		std::sort(lines.begin(), lines.end(), [](const std::pair<unsigned, std::string> &a, const std::pair<unsigned, std::string> &b) { return a.first > b.first; });
		for (size_t i = 0; i < lines.size() && i < 70; ++i)
			fprintf(f, "%6.2f%%  %7u  %s\n", 100.0 * lines[i].first / static_cast<double>(s_samples.empty() ? 1 : s_samples.size()), lines[i].first, lines[i].second.c_str());
		fclose(f);
	}
}

std::string describeAddress(unsigned long long address)
{
	static bool tried = false, symbols = false;
	static SymFromAddrFn symFromAddr = nullptr;
	static SymGetLineFn symGetLine = nullptr;
	const HANDLE process = GetCurrentProcess();
	if (!tried)
	{
		tried = true;
		HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
		SymInitializeFn symInitialize = dbghelp ? reinterpret_cast<SymInitializeFn>(GetProcAddress(dbghelp, "SymInitialize")) : nullptr;
		symFromAddr = dbghelp ? reinterpret_cast<SymFromAddrFn>(GetProcAddress(dbghelp, "SymFromAddr")) : nullptr;
		SymSetOptionsFn symSetOptions = dbghelp ? reinterpret_cast<SymSetOptionsFn>(GetProcAddress(dbghelp, "SymSetOptions")) : nullptr;
		symGetLine = dbghelp ? reinterpret_cast<SymGetLineFn>(GetProcAddress(dbghelp, "SymGetLineFromAddr64")) : nullptr;
		if (symInitialize && symFromAddr)
		{
			if (symSetOptions) symSetOptions(0x00000002 | 0x00000004 | 0x00000010);
			symbols = symInitialize(process, nullptr, TRUE) != FALSE;
		}
	}
	char hex[32];
	snprintf(hex, sizeof hex, "0x%llx", address);
	if (!symbols) return hex;
	char buffer[sizeof(SYMBOL_INFO) + 256] = {};
	SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol->MaxNameLen = 255;
	DWORD64 displacement = 0;
	if (!symFromAddr(process, address, &displacement, symbol)) return hex;
	std::string text = symbol->Name;
	if (symGetLine)
	{
		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(line);
		DWORD d = 0;
		if (symGetLine(process, address, &d, &line))
		{
			const char *file = line.FileName ? line.FileName : "?";
			const char *slash = strrchr(file, 92);
			text += std::string("  ") + (slash ? slash + 1 : file) + ":" + std::to_string(line.LineNumber);
		}
	}
	return text;
}
}
