/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
namespace PerformanceProfile
{
using Tick = std::uint64_t;
enum class Category : unsigned int
{
	Outer, EngineUpdate, Pacer, Logic, Client, ObjectUpdates, AIGlobal, AIPlayer, AIObject,
	Movement, PathQueue, PathRequest, PathSearch, PathBuild, WeaponFire, WeaponStore,
	Missile, DrawableUpdates, Terrain, DisplayUpdate, Particles, DisplayDraw, SceneView,
	RenderEnd, Network, Messages, Count
};
enum class Counter : unsigned int { CompletedTicks, UpdateModules, ObjectsVisited, DrawablesUpdated, QueuedPaths, QueueCells, Count };
constexpr unsigned int CategoryCount = static_cast<unsigned int>(Category::Count);
constexpr unsigned int CounterCount = static_cast<unsigned int>(Counter::Count);
constexpr unsigned int MaxFrames = 16384, MaxDepth = 64;
enum class PathKind : unsigned int
{
	Request, Internal, Ground, Hierarchical, Closest, Attack, Safe, Patch, MoveAway,
	Queue, Dispatch, Reconstruction, Cleanup, Zones, ZoneFlags, Obstacle, Count
};
enum class PathWork : unsigned int
{
	HeadPops, InfoAttempts, InfoNew, InfoFailed, OpenInserts, ForwardHops, ReverseHops,
	CleanedCells, BlockZoneQueries, HierarchyFallbacks, Count
};
enum class PathOutcome : unsigned int { Observed, ReturnedPath, ReturnedClosest, NullBeforeWork, NullAfterWork };
constexpr unsigned int PathWorkCount=static_cast<unsigned int>(PathWork::Count), MaxPaths=32768;
struct PathSample
{
	Tick id=0,parent=0,offset=0,inclusive=0,exclusive=0,sourceCategories=0;
	unsigned int outer=0,logic=0,object=0,surfaces=0;
	int layer=-1,radius=-1,human=-1,crusher=-1,closestAllowed=-1,queueBefore=-1,queueAfter=-1;
	float fromX=0,fromY=0,toX=0,toY=0;
	bool hasCoordinates=false,zoneRejected=false;
	PathKind kind=PathKind::Request,request=PathKind::Count;
	PathOutcome outcome=PathOutcome::Observed;
	std::array<Tick,PathWorkCount> work{};
};
struct Metric { Tick inclusive=0, exclusive=0, calls=0; };
struct Frame
{
	Tick offset=0;
	unsigned int logicBefore=0, logicAfter=0;
	int requestedFps=0, effectiveFps=0;
	std::array<Metric,CategoryCount> metrics{};
	std::array<Tick,CounterCount> counters{};
};
struct Statistics { std::size_t count=0; Tick total=0, minimum=0, maximum=0, p50=0, p95=0, p99=0; double mean=0; };
Statistics statistics(std::vector<Tick> values);
const char* name(Category category);
const char* name(Counter counter);
class Recorder
{
public:
	explicit Recorder(unsigned int capacity=MaxFrames,unsigned int pathCapacity=0);
	void start(Tick now);
	void stop();
	bool running() const { return m_running; }
	bool full() const { return m_frames.size() >= m_capacity; }
	bool inFrame() const { return m_inFrame; }
	bool beginFrame(Tick now, unsigned int logicFrame, int requestedFps, int effectiveFps);
	void endFrame(Tick now, unsigned int logicFrame);
	bool push(Category category, Tick now);
	void pop(Tick now);
	void add(Counter counter, Tick amount=1);
	const std::vector<Frame>& frames() const { return m_frames; }
	Tick started() const { return m_started; }
	Tick elapsed() const { return m_elapsed; }
	Tick errors() const { return m_errors; }
	bool deepPaths() const { return m_pathCapacity!=0; }
	bool pathsFull() const { return deepPaths() && m_paths.size()>=m_pathCapacity; }
	Tick droppedPaths() const { return m_droppedPaths; }
	const std::vector<PathSample>& paths() const { return m_paths; }
	PathSample* beginPath(PathKind kind,Tick timestamp,Tick parent);
	std::vector<std::size_t> slowest(Category category, std::size_t count, bool completedOnly=false) const;
private:
	struct Node { Category category; Tick started, children; bool recursive; };
	std::vector<Frame> m_frames;
	std::vector<PathSample> m_paths;
	unsigned int m_pathCapacity=0;
	Tick m_pathSequence=0,m_droppedPaths=0;
	std::array<Node,MaxDepth> m_stack{};
	unsigned int m_capacity, m_depth=0;
	Tick m_activeCategories=0, m_started=0, m_elapsed=0, m_errors=0;
	bool m_running=false, m_inFrame=false;
};
struct Metadata
{
	std::string title, git, timestamp, startedUtc, label, build, mode;
	bool dirty=false, interpolation=false;
	std::uint64_t frequency=1;
};
void report(const Recorder& recorder, const Metadata& metadata, std::ostream& summary,
	std::ostream& frames, std::ostream& categories, std::ostream& slowFrames);
Tick now();
Tick frequency();
extern thread_local Recorder* activeRecorder;
using Clock = Tick(*)();
class PathScope;
extern thread_local PathScope* activePath;
class PathScope
{
public:
	explicit PathScope(PathKind kind,Clock clock=now);
	~PathScope();
	PathScope(const PathScope&)=delete;
	PathScope& operator=(const PathScope&)=delete;
	bool enabled() const { return m_sample!=nullptr; }
	PathSample* sample() { return m_sample; }
	void result(bool returned);
	void closest() { if(m_sample)m_sample->outcome=PathOutcome::ReturnedClosest; }
	void add(PathWork work,Tick amount=1) { if(m_sample)m_sample->work[static_cast<unsigned int>(work)]+=amount; }
private:
	PathSample* m_sample=nullptr;
	PathScope* m_parent=nullptr;
	Clock m_clock;
	Tick m_started=0,m_children=0;
	bool m_guard=false;
};
inline void pathCount(PathWork work,Tick amount=1) { if(activePath)activePath->add(work,amount); }
void reportPaths(const Recorder& recorder,const Metadata& metadata,std::ostream& summary,std::ostream& paths);
void enablePathDetails();
class Scope
{
public:
	explicit Scope(Category category, Clock clock=now) : m_recorder(activeRecorder), m_clock(clock)
	{ if (m_recorder && !m_recorder->push(category,m_clock())) m_recorder=nullptr; }
	~Scope() { if (m_recorder) m_recorder->pop(m_clock()); }
	Scope(const Scope&)=delete;
	Scope& operator=(const Scope&)=delete;
private:
	Recorder* m_recorder;
	Clock m_clock;
};
inline void count(Counter counter, Tick amount=1) { if (activeRecorder) activeRecorder->add(counter,amount); }
// Runtime frontend: never controls game timing or command/input state.
bool configure(const char* absoluteDirectory);
bool configured();
void beginOuterFrame(unsigned int logicFrame, int requestedFps, int effectiveFps);
void endOuterFrame(unsigned int logicFrame);
void shutdown();
}
#define PERFORMANCE_PROFILE_JOIN_(a,b) a##b
#define PERFORMANCE_PROFILE_JOIN(a,b) PERFORMANCE_PROFILE_JOIN_(a,b)
#define PERFORMANCE_PROFILE_SCOPE(category) PerformanceProfile::Scope PERFORMANCE_PROFILE_JOIN(performanceProfileScope_,__LINE__)(PerformanceProfile::Category::category)
