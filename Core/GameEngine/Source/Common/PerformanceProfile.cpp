/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "PreRTS.h"
#include "Common/PerformanceProfile.h"
#include <algorithm>
#include <ostream>
namespace PerformanceProfile
{
thread_local Recorder* activeRecorder=nullptr;
thread_local PathScope* activePath=nullptr;
thread_local PathIteration* activeIteration=nullptr;
thread_local PathPhase* activePhase=nullptr;
const char* name(Category c)
{
	static const char* names[]={"outer_frame_including_pacer","engine_update","frame_pacer_wait_and_bookkeeping",
		"game_logic_update","game_client_update","object_update_loops","ai_global","ai_player_strategy",
		"ai_object_base_update","locomotor_move","path_queue","path_request_entries","path_search",
		"path_reconstruction","weapon_fire","weapon_store","missile_ai_update","drawable_client_updates",
		"terrain_visual_update","display_update","particle_manager_update","display_draw","scene_view_draw",
		"render_end_flush_present","network_update","message_propagation"};
	static_assert(sizeof(names)/sizeof(*names)==CategoryCount);
	return names[static_cast<unsigned int>(c)];
}
const char* name(Counter c)
{
	static const char* names[]={"completed_logic_ticks","update_modules_invoked","objects_visited_disabled_loop",
		"drawables_updated","queued_paths_processed","queue_cells_allocated"};
	static_assert(sizeof(names)/sizeof(*names)==CounterCount);
	return names[static_cast<unsigned int>(c)];
}
Statistics statistics(std::vector<Tick> v)
{
	Statistics s; if(v.empty())return s;
	std::sort(v.begin(),v.end()); s.count=v.size(); s.minimum=v.front(); s.maximum=v.back();
	for(Tick t:v)s.total+=t;
	s.mean=static_cast<double>(s.total)/static_cast<double>(s.count);
	auto percentile=[&](std::size_t p){return v[(p*v.size()+99)/100-1];};
	s.p50=percentile(50);s.p95=percentile(95);s.p99=percentile(99);return s;
}
Recorder::Recorder(unsigned int capacity,unsigned int pathCapacity,bool samplePhases) : m_pathCapacity(std::min(pathCapacity,MaxPaths)),m_capacity(std::min(capacity,MaxFrames)),m_samplePhases(samplePhases)
{ m_frames.reserve(m_capacity);m_paths.reserve(m_pathCapacity); }
void Recorder::start(Tick timestamp)
{
	if(m_inFrame)return;
	m_frames.clear();m_started=timestamp;m_elapsed=m_errors=m_activeCategories=0;m_depth=0;m_running=true;
	m_paths.clear();m_pathSequence=m_droppedPaths=0;
}
PathSample* Recorder::beginPath(PathKind kind,Tick timestamp,Tick parent)
{
	if(!m_inFrame || !deepPaths())return nullptr;
	if(pathsFull()){++m_droppedPaths;return nullptr;}
	m_paths.emplace_back();auto& sample=m_paths.back();
	sample.id=++m_pathSequence;sample.parent=parent;sample.kind=kind;
	sample.outer=static_cast<unsigned int>(m_frames.size()-1);sample.logic=m_frames.back().logicBefore;
	sample.offset=timestamp>=m_started?timestamp-m_started:0;sample.sourceCategories=m_activeCategories;
	sample.phases.enabled=samplePhases() && (kind==PathKind::Internal || kind==PathKind::Ground || kind==PathKind::Closest);
	return &sample;
}
PathScope::PathScope(PathKind kind,Clock clock) : m_clock(clock)
{
	if(!activeRecorder || !activeRecorder->deepPaths() || !activeRecorder->inFrame())return;
	m_guard=true;m_parent=activePath;m_started=m_clock();
	m_previousIteration=activeIteration;m_previousPhase=activePhase;
	activeIteration=nullptr;activePhase=nullptr;
	m_sample=activeRecorder->beginPath(kind,m_started,m_parent&&m_parent->m_sample?m_parent->m_sample->id:0);
	if(m_sample && m_parent && m_parent->m_sample)
	{
		const auto& p=*m_parent->m_sample;
		m_sample->request=p.request;m_sample->object=p.object;m_sample->layer=p.layer;
		m_sample->surfaces=p.surfaces;m_sample->human=p.human;m_sample->radius=p.radius;
		m_sample->fromX=p.fromX;m_sample->fromY=p.fromY;m_sample->toX=p.toX;m_sample->toY=p.toY;
		m_sample->hasCoordinates=p.hasCoordinates;m_sample->crusher=p.crusher;
	}
	if(m_sample && (kind==PathKind::Request || kind==PathKind::Closest || kind==PathKind::Attack || kind==PathKind::Safe || kind==PathKind::Patch || kind==PathKind::MoveAway))m_sample->request=kind;
	activePath=m_sample?this:nullptr;
}
PathScope::~PathScope()
{
	if(!m_guard)return;
	const Tick end=m_clock(),duration=end>=m_started?end-m_started:0;
	if(m_sample){m_sample->inclusive=duration;m_sample->exclusive=duration>=m_children?duration-m_children:0;}
	if(m_parent)
	{
		m_parent->m_children+=duration;
		if(m_sample && m_parent->m_sample)for(unsigned int c=0;c<PathWorkCount;++c)m_parent->m_sample->work[c]+=m_sample->work[c];
	}
	activePath=m_parent;
	activeIteration=m_previousIteration;activePhase=m_previousPhase;
}
PathIteration::PathIteration(Clock clock) : m_previous(activeIteration),m_previousPhase(activePhase),m_clock(clock)
{
	activePhase=nullptr;
	if(activePath && activePath->enabled() && activePath->sample()->phases.enabled)
	{
		auto* sample=activePath->sample();
		if(++sample->phases.iterations%PathPhaseStride==0){++sample->phases.selected;m_sample=sample;}
	}
	activeIteration=m_sample?this:nullptr;
}
PathIteration::~PathIteration() { activeIteration=m_previous;activePhase=m_previousPhase; }
PathPhase::PathPhase(PathPhaseKind kind) : m_kind(kind)
{
	if(!activeIteration)return;
	m_sample=activeIteration->sample();m_clock=activeIteration->clock();
	m_parent=activePhase;m_start=m_clock();activePhase=this;
}
PathPhase::~PathPhase()
{
	if(!m_sample)return;
	const Tick end=m_clock();
	const Tick duration=end>=m_start?end-m_start:0;
	// The enclosing neighbor scope excludes its separately measured line child,
	// but retains its own nested insertions. No phase or insertion overlaps another.
	const Tick inclusive=duration>=m_children?duration-m_children:0;
	if(end<m_start || duration<m_children || inclusive<m_insertion)++m_sample->phases.errors;
	auto& metric=m_sample->phases.metrics[static_cast<unsigned int>(m_kind)];
	++metric.calls;metric.inclusive+=inclusive;metric.insertion+=std::min(inclusive,m_insertion);metric.inserts+=m_inserts;
	if(m_parent)m_parent->m_children+=duration;
	activePhase=m_parent;
}
void PathInsertion::begin() { m_start=m_phase->m_clock(); }
void PathInsertion::finish()
{
	const Tick end=m_phase->m_clock();
	if(end<m_start)++m_phase->m_sample->phases.errors;
	m_phase->m_insertion+=end>=m_start?end-m_start:0;++m_phase->m_inserts;
}
void PathScope::result(bool returned)
{
	if(!m_sample)return;
	if(returned){if(m_sample->outcome!=PathOutcome::ReturnedClosest)m_sample->outcome=PathOutcome::ReturnedPath;}
	else m_sample->outcome=m_sample->work[static_cast<unsigned int>(PathWork::HeadPops)]?PathOutcome::NullAfterWork:PathOutcome::NullBeforeWork;
}
void reportPaths(const Recorder& r,const Metadata& meta,std::ostream& summary,std::ostream& paths)
{
	static const char* kinds[]={"request","internal","ground","hierarchical","closest","attack","safe","patch","move_away","queue","dispatch","reconstruction","cleanup","zones","zone_flags","obstacle"};
	static const char* outcomes[]={"observed","returned_path","returned_closest","null_before_work","null_after_work"};
	static const char* work[]={"open_head_pops","info_attempts","info_new","info_failed","open_inserts","forward_hops","reverse_hops","cleaned_cells","block_zone_queries","hierarchy_fallbacks"};
	static_assert(sizeof(kinds)/sizeof(*kinds)==static_cast<unsigned int>(PathKind::Count));
	static_assert(sizeof(work)/sizeof(*work)==PathWorkCount);
	const double ticksPerMs=static_cast<double>(std::max<Tick>(meta.frequency,1))/1000.0;
	Tick phaseErrors=0;
	for(const auto& sample:r.paths())phaseErrors+=sample.phases.errors;
	summary<<"path_detail_enabled="<<r.deepPaths()<<"\npath_detail_records="<<r.paths().size()<<"\npath_detail_dropped="<<r.droppedPaths()<<"\n"
		<<"path_phase_enabled="<<r.samplePhases()<<"\npath_phase_stride="<<PathPhaseStride<<"\n"
		<<"path_phase_errors="<<phaseErrors<<"\n"
		<<"Path details are individual function intervals; parent/child inclusive durations and work counts overlap.\n"
		<<"Null after work includes failed/limited searches; returned_path alone does not prove exact-goal success.\n";
	paths<<"id,parent_id,outer_index,logic_before,capture_offset_ms,kind,request_context,source_category_mask,object_id,layer,surfaces,radius,human,crusher,closest_allowed,has_coordinates,from_x,from_y,to_x,to_y,queue_before,queue_after,outcome,zone_rejected,inclusive_ms,exclusive_ms";
	for(const char* label:work)paths<<","<<label<<"_inclusive_count";
	paths<<",phase_iterations,phase_selected,phase_errors";
	for(const char* label:{"line","neighbor"})paths<<","<<label<<"_sample_calls,"<<label<<"_sample_inclusive_ms,"<<label<<"_sample_insertion_ms,"<<label<<"_sample_inserts";
	paths<<"\n";
	for(const auto& s:r.paths())
	{
		paths<<s.id<<","<<s.parent<<","<<s.outer<<","<<s.logic<<","<<s.offset/ticksPerMs<<","<<kinds[static_cast<unsigned int>(s.kind)]<<","<<(s.request==PathKind::Count?"none":kinds[static_cast<unsigned int>(s.request)])<<","<<s.sourceCategories<<","<<s.object<<","<<s.layer<<","<<s.surfaces<<","<<s.radius<<","<<s.human<<","<<s.crusher<<","<<s.closestAllowed<<","<<s.hasCoordinates<<","<<s.fromX<<","<<s.fromY<<","<<s.toX<<","<<s.toY<<","<<s.queueBefore<<","<<s.queueAfter<<","<<outcomes[static_cast<unsigned int>(s.outcome)]<<","<<s.zoneRejected<<","<<s.inclusive/ticksPerMs<<","<<s.exclusive/ticksPerMs;
		for(Tick value:s.work)paths<<","<<value;
		paths<<","<<s.phases.iterations<<","<<s.phases.selected<<","<<s.phases.errors;
		for(const auto& phase:s.phases.metrics)paths<<","<<phase.calls<<","<<phase.inclusive/ticksPerMs<<","<<phase.insertion/ticksPerMs<<","<<phase.inserts;
		paths<<"\n";
	}
	for(unsigned int k=0;k<static_cast<unsigned int>(PathKind::Count);++k)
	{
		std::vector<Tick> values;for(const auto& s:r.paths())if(static_cast<unsigned int>(s.kind)==k)values.push_back(s.inclusive);
		const auto stats=statistics(std::move(values));
		summary<<"path_detail_"<<kinds[k]<<" samples="<<stats.count<<" total_ms="<<stats.total/ticksPerMs<<" mean_ms="<<stats.mean/ticksPerMs<<" p50_ms="<<stats.p50/ticksPerMs<<" p95_ms="<<stats.p95/ticksPerMs<<" p99_ms="<<stats.p99/ticksPerMs<<" max_ms="<<stats.maximum/ticksPerMs<<"\n";
	}
}
void Recorder::stop() { if(!m_inFrame)m_running=false; }
bool Recorder::beginFrame(Tick timestamp,unsigned int logic,int requested,int effective)
{
	if(!m_running || m_inFrame || full())return false;
	m_frames.emplace_back();Frame& f=m_frames.back();
	f.offset=timestamp>=m_started?timestamp-m_started:0;f.logicBefore=logic;f.requestedFps=requested;f.effectiveFps=effective;
	m_inFrame=true;m_depth=0;m_activeCategories=0;push(Category::Outer,timestamp);return true;
}
void Recorder::endFrame(Tick timestamp,unsigned int logic)
{
	if(!m_inFrame)return;
	if(m_depth!=1)++m_errors;
	while(m_depth)pop(timestamp);
	m_frames.back().logicAfter=logic;m_elapsed=timestamp>=m_started?timestamp-m_started:0;m_inFrame=false;
}
bool Recorder::push(Category category,Tick timestamp)
{
	if(!m_inFrame)return false;
	if(m_depth==MaxDepth){++m_errors;return false;}
	const Tick bit=Tick(1)<<static_cast<unsigned int>(category);
	m_stack[m_depth++]={category,timestamp,0,(m_activeCategories&bit)!=0};m_activeCategories|=bit;return true;
}
void Recorder::pop(Tick timestamp)
{
	if(!m_depth || !m_inFrame){++m_errors;return;}
	const Node node=m_stack[--m_depth];
	if(timestamp<node.started)++m_errors;
	const Tick duration=timestamp>=node.started?timestamp-node.started:0;
	Metric& m=m_frames.back().metrics[static_cast<unsigned int>(node.category)];
	++m.calls;if(!node.recursive)m.inclusive+=duration;
	m.exclusive+=duration>=node.children?duration-node.children:0;
	if(!node.recursive)m_activeCategories&=~(Tick(1)<<static_cast<unsigned int>(node.category));
	if(m_depth)m_stack[m_depth-1].children+=duration;
}
void Recorder::add(Counter counter,Tick amount)
{ if(m_inFrame)m_frames.back().counters[static_cast<unsigned int>(counter)]+=amount; }
std::vector<std::size_t> Recorder::slowest(Category category,std::size_t limit,bool completedOnly) const
{
	std::vector<std::size_t> order;
	for(std::size_t i=0;i<m_frames.size();++i)
		if(!completedOnly || m_frames[i].counters[static_cast<unsigned int>(Counter::CompletedTicks)])order.push_back(i);
	std::stable_sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){return m_frames[a].metrics[static_cast<unsigned int>(category)].inclusive>m_frames[b].metrics[static_cast<unsigned int>(category)].inclusive;});
	if(order.size()>limit)order.resize(limit);return order;
}
void report(const Recorder& r,const Metadata& meta,std::ostream& summary,std::ostream& frames,std::ostream& categories,std::ostream& slow)
{
	const double ticksPerMs=static_cast<double>(std::max<Tick>(meta.frequency,1))/1000.0;
	std::array<Tick,CounterCount> counters{};
	for(const auto& f:r.frames())for(unsigned int c=0;c<CounterCount;++c)counters[c]+=f.counters[c];
	summary<<"Stage4A CPU QPC profile\ntitle="<<meta.title<<"\ngit="<<meta.git<<"\ndirty="<<meta.dirty
		<<"\nreport_timestamp_utc="<<meta.timestamp<<"\ncapture_started_utc="<<meta.startedUtc<<"\ncapture_label="<<meta.label
		<<"\nbuild="<<meta.build<<"\nmode="<<meta.mode
		<<"\nground_interpolation="<<meta.interpolation<<"\nqpc_frequency="<<meta.frequency
		<<"\nouter_frames="<<r.frames().size()<<"\ncompleted_logic_ticks="<<counters[0]
		<<"\ncanonical_target_tps=30\nelapsed_seconds="<<r.elapsed()/ticksPerMs/1000.0
		<<"\nstack_or_clock_errors="<<r.errors()<<"\n";
	if(!r.frames().empty())summary<<"initial_requested_fps="<<r.frames().front().requestedFps<<"\ninitial_effective_fps="<<r.frames().front().effectiveFps<<"\n";
	summary<<"Times are CPU wall durations, including preemption/waits. No GPU measurement.\n"
		<<"Category percentiles are sums per active outer frame, not per invocation.\n"
		<<"Inclusive categories overlap; exclusive totals partition instrumented scopes.\n"
		<<"Same-category recursion is counted once inclusively. Self/residual time is unclassified.\n"
		<<"Logic calls include early returns; top logic rows require completed ticks.\n"
		<<"Control polling, buffer start and report I/O are outside measured outer frames.\n";
	for(unsigned int c=0;c<CounterCount;++c)summary<<name(static_cast<Counter>(c))<<"="<<counters[c]<<"\n";
	frames<<"outer_index,capture_offset_ms,logic_before,logic_after,requested_fps,effective_fps";
	for(unsigned int c=0;c<CounterCount;++c)frames<<","<<name(static_cast<Counter>(c));
	for(unsigned int c=0;c<CategoryCount;++c)frames<<","<<name(static_cast<Category>(c))<<"_inclusive_ms,"<<name(static_cast<Category>(c))<<"_exclusive_ms,"<<name(static_cast<Category>(c))<<"_calls";
	frames<<"\n";
	for(std::size_t i=0;i<r.frames().size();++i)
	{
		const Frame& f=r.frames()[i];frames<<i<<","<<f.offset/ticksPerMs<<","<<f.logicBefore<<","<<f.logicAfter<<","<<f.requestedFps<<","<<f.effectiveFps;
		for(Tick v:f.counters)frames<<","<<v;
		for(const auto& m:f.metrics)frames<<","<<m.inclusive/ticksPerMs<<","<<m.exclusive/ticksPerMs<<","<<m.calls;
		frames<<"\n";
	}
	categories<<"category,time_kind,active_frames,invocations,total_ms,mean_active_frame_ms,min_ms,max_ms,p50_ms,p95_ms,p99_ms\n";
	for(unsigned int c=0;c<CategoryCount;++c)for(int exclusive=0;exclusive<2;++exclusive)
	{
		std::vector<Tick> values;Tick calls=0;
		for(const auto& f:r.frames()){const auto& m=f.metrics[c];calls+=m.calls;if(m.calls)values.push_back(exclusive?m.exclusive:m.inclusive);}
		const auto s=statistics(std::move(values));
		categories<<name(static_cast<Category>(c))<<","<<(exclusive?"exclusive":"inclusive")<<","<<s.count<<","<<calls<<","<<s.total/ticksPerMs<<","<<s.mean/ticksPerMs<<","<<s.minimum/ticksPerMs<<","<<s.maximum/ticksPerMs<<","<<s.p50/ticksPerMs<<","<<s.p95/ticksPerMs<<","<<s.p99/ticksPerMs<<"\n";
		if(!exclusive)summary<<name(static_cast<Category>(c))<<" active_frames="<<s.count<<" mean_ms="<<s.mean/ticksPerMs<<" p95_ms="<<s.p95/ticksPerMs<<" p99_ms="<<s.p99/ticksPerMs<<" max_ms="<<s.maximum/ticksPerMs<<"\n";
	}
	slow<<"ranking,rank,outer_index,capture_offset_ms,logic_before,logic_after,completed_ticks,category,inclusive_ms,exclusive_ms,calls\n";
	for(int logic=0;logic<2;++logic)
	{
		const auto indices=r.slowest(logic?Category::Logic:Category::Outer,10,logic!=0);
		for(std::size_t rank=0;rank<indices.size();++rank)
		{
			const auto index=indices[rank];const auto& f=r.frames()[index];
			for(unsigned int c=0;c<CategoryCount;++c){const auto& m=f.metrics[c];slow<<(logic?"completed_logic_frame":"outer_frame")<<","<<rank+1<<","<<index<<","<<f.offset/ticksPerMs<<","<<f.logicBefore<<","<<f.logicAfter<<","<<f.counters[0]<<","<<name(static_cast<Category>(c))<<","<<m.inclusive/ticksPerMs<<","<<m.exclusive/ticksPerMs<<","<<m.calls<<"\n";}
		}
	}
}
}
