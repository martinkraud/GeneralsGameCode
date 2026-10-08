/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "PreRTS.h"
#include "Common/PerformanceProfile.h"
#include "Common/GroundTranslation.h"
#include "gitinfo.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <ctime>
namespace PerformanceProfile
{
Tick now() { LARGE_INTEGER q;return QueryPerformanceCounter(&q)?static_cast<Tick>(q.QuadPart):0; }
Tick frequency() { LARGE_INTEGER q;return QueryPerformanceFrequency(&q)?static_cast<Tick>(q.QuadPart):0; }
namespace
{
bool enabled=false;
bool pathDetails=false;
std::filesystem::path directory;
std::unique_ptr<Recorder> recorder;
Tick clockFrequency=0,nextPoll=0;
std::string lastCommand;
std::string captureLabel,captureStartUtc;
unsigned int serial=0;
std::string utcStamp()
{
	const std::time_t time=std::time(nullptr);std::tm utc{};gmtime_s(&utc,&time);
	char buffer[32];std::strftime(buffer,sizeof(buffer),"%Y%m%dT%H%M%SZ",&utc);return buffer;
}
void flush(const char* reason)
{
	if(!recorder || !recorder->running())return;
	activeRecorder=nullptr;recorder->stop();
	const auto stamp=utcStamp();
	const auto base=directory/(stamp+"-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++serial));
	std::ofstream summary(base.string()+"-summary.txt"),frames(base.string()+"-frames.csv"),categories(base.string()+"-categories.csv"),slow(base.string()+"-slow.csv");
	if(!summary || !frames || !categories || !slow){OutputDebugStringA("Stage4A: report files could not be opened.\n");return;}
	Metadata metadata;
#if RTS_ZEROHOUR
	metadata.title="Zero Hour";
#else
	metadata.title="Generals";
#endif
	metadata.git=GitSHA1;metadata.dirty=GitUncommittedChanges;metadata.timestamp=stamp;
	metadata.label=captureLabel;metadata.startedUtc=captureStartUtc;
	metadata.build="MSVC "+std::to_string(_MSC_FULL_VER)+"; " __DATE__ " " __TIME__;
	metadata.mode=reason;metadata.frequency=clockFrequency;metadata.interpolation=GroundTranslation::isEnabled();
	report(*recorder,metadata,summary,frames,categories,slow);
	if(recorder->deepPaths())
	{
		std::ofstream paths(base.string()+"-paths.csv");
		if(paths){reportPaths(*recorder,metadata,summary,paths);if(!paths)OutputDebugStringA("Stage4A.1: path report write failed.\n");}
		else OutputDebugStringA("Stage4A.1: path report could not be opened.\n");
	}
	if(!summary || !frames || !categories || !slow)OutputDebugStringA("Stage4A: report write failed.\n");
}
}
bool configured(){return enabled;}
void enablePathDetails(){pathDetails=true;}
bool configure(const char* path)
{
	try
	{
		if(enabled || !path)return false;
		const std::filesystem::path candidate(path);
		// An explicit existing absolute output directory is required; no implicit user-data writes.
		if(!candidate.is_absolute() || !std::filesystem::is_directory(candidate))return false;
		const Tick freq=frequency();if(!freq)return false;
		directory=std::filesystem::canonical(candidate);clockFrequency=freq;nextPoll=0;enabled=true;return true;
	}
	catch(...){return false;}
}
void beginOuterFrame(unsigned int logic,int requested,int effective)
{
	if(!enabled)return;
	try
	{
		Tick timestamp=now();
		if(recorder && recorder->running() && (recorder->full() || recorder->pathsFull() || timestamp-recorder->started()>=clockFrequency*60))flush(recorder->full() || recorder->pathsFull()?"capture_capacity":"60_second_limit");
		if(timestamp>=nextPoll)
		{
			nextPoll=timestamp+clockFrequency;
			std::ifstream control(directory/"command.txt");char buffer[130]{};control.getline(buffer,sizeof(buffer));
			const std::string command=control?buffer:"";
			if(!command.empty() && command!=lastCommand)
			{
				lastCommand=command;
				if(command.rfind("start ",0)==0 && (!recorder || !recorder->running()))
				{
					if(!recorder)recorder=std::make_unique<Recorder>(MaxFrames,pathDetails?MaxPaths:0);
					captureLabel=command.substr(6);captureStartUtc=utcStamp();recorder->start(now());
				}
				else if(command.rfind("stop ",0)==0)flush("manual_stop");
			}
		}
		if(recorder && recorder->running() && recorder->beginFrame(now(),logic,requested,effective))activeRecorder=recorder.get();
	}
	catch(...){activeRecorder=nullptr;enabled=false;recorder.reset();OutputDebugStringA("Stage4A disabled after profiler I/O/allocation error.\n");}
}
void endOuterFrame(unsigned int logic)
{
	if(!activeRecorder)return;
	Recorder* current=activeRecorder;activeRecorder=nullptr;current->endFrame(now(),logic);
}
void shutdown()
{
	try{if(enabled)flush("process_exit");}catch(...){OutputDebugStringA("Stage4A exit report failed.\n");}
	enabled=false;activeRecorder=nullptr;recorder.reset();
	pathDetails=false;activePath=nullptr;
	// Release allocations before the engine's global memory allocator is shut down,
	// including when a previous capture error already disabled profiling.
	std::filesystem::path().swap(directory);
	std::string().swap(lastCommand);std::string().swap(captureLabel);std::string().swap(captureStartUtc);
}
}
