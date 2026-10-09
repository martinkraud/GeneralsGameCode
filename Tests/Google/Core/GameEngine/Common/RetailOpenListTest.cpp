// Copyright 2026 TheSuperHackers. SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "Common/Debug.h"
#include "Common/PerformanceProfile.h"
#include "GameLogic/AIPathfind.h"

#if RETAIL_COMPATIBLE_PATHFINDING
// Access-only friend. Production representation, allocation, insertion and removal
// are used directly. No fake list implementation is used for baseline timing.
struct RetailOpenListTestAccess
{
    static constexpr unsigned PATHFIND_CELLS_PER_FRAME = 5000;
    static PathfindCellInfo* info(PathfindCell& c) { return c.m_info; }
    static PathfindCell* tail(const PathfindCellList& l) { return l.m_tail; }
    __declspec(noinline) static void reference(PathfindCell& cell, PathfindCellList& list)
    {
        PathfindCell* self = &cell;
        PathfindCellInfo* const m_info = self->m_info;
#include "RetailOpenListReference.inc"
    }
    __declspec(noinline) static void candidate(PathfindCell& cell, PathfindCellList& list)
    {
        PathfindCell* self = &cell;
        PathfindCellInfo* const m_info = self->m_info;
#include "RetailOpenListUnrolledCandidate.inc"
    }
    static void detach(PathfindCell& c) { c.m_info = nullptr; }
    static void restore(PathfindCell& c, PathfindCellInfo* i) { c.m_info = i; }
    static void next(PathfindCell& c, PathfindCellInfo* i) { c.m_info->m_nextOpen = i; }
    static PathfindCell* owner(PathfindCellInfo* i) { return i->m_cell; }
    static void owner(PathfindCellInfo* i, PathfindCell* c) { i->m_cell=c; }
    static std::array<int, 11> state(PathfindCellInfo* i,
        const std::unordered_map<const void*, int>& ids)
    {
        auto id = [&](const void* p) { return p ? ids.at(p) : -1; };
        return {id(i->m_nextOpen), id(i->m_prevOpen), id(i->m_pathParent), id(i->m_cell),
            i->m_totalCost, i->m_costSoFar, int(i->m_open), int(i->m_closed), int(i->m_isFree),
            i->m_pos.x, i->m_pos.y};
    }
};
namespace
{
using Access = RetailOpenListTestAccess;
using Insert = void(*)(PathfindCell&, PathfindCellList&);
void production(PathfindCell& c, PathfindCellList& l) { c.forwardInsertionSortRetailCompatible(l); }
struct Pool
{
    Pool() { PathfindCellInfo::allocateCellInfos(); }
    ~Pool() { PathfindCellInfo::releaseCellInfos(); }
};
struct Fixture
{
    std::vector<PathfindCell> cells;
    std::vector<PathfindCellInfo*> infos;
    std::unordered_map<const void*, int> ids;
    PathfindCellList list;
    explicit Fixture(size_t n) : cells(n)
    {
        for (size_t j=0; j<n; ++j)
        {
            ICoord2D pos = {int(j), 0};
            if (!cells[j].allocateInfo(pos)) throw std::bad_alloc();
            infos.push_back(Access::info(cells[j]));
            ids[&cells[j]]=int(j); ids[infos.back()]=int(j);
        }
    }
    int id(const void* p) const { return p ? ids.at(p) : -1; }
    std::vector<int> order() const
    {
        std::vector<int> out;
        for (auto* c=list.getHead(); c; c=c->getNextOpen())
        {
            out.push_back(id(c));
            if (out.size()>cells.size()) throw std::runtime_error("list cycle");
        }
        return out;
    }
    void insert(size_t j, unsigned cost, Insert fn)
    { cells[j].setTotalCost(cost); fn(cells[j],list); }
    void remove(size_t j) { cells[j].removeFromOpenList(list); }
    // Snapshot disconnected records too: repair may intentionally leave them detached.
    std::vector<std::array<int,11>> snapshot() const
    {
        std::vector<std::array<int,11>> out;
        for(auto* i:infos) out.push_back(Access::state(i,ids));
        return out;
    }
};
uint64_t counted(Fixture& f, size_t j, unsigned cost, Insert fn)
{
    using namespace PerformanceProfile;
    Recorder recorder(1,1,false); recorder.start(0); recorder.beginFrame(0,0,120,120);
    activeRecorder=&recorder;
    Tick hops=0;
    {
        PathScope scope(PathKind::Internal);
        f.insert(j,cost,fn);
        hops=scope.sample()->work[static_cast<unsigned>(PathWork::ForwardHops)];
        EXPECT_EQ(scope.sample()->work[static_cast<unsigned>(PathWork::OpenInserts)],0u);
    }
    activeRecorder=nullptr;
    return hops;
}
void same(const Fixture& a, const Fixture& b)
{
    EXPECT_EQ(a.id(a.list.getHead()), b.id(b.list.getHead()));
    EXPECT_EQ(a.id(Access::tail(a.list)),
              b.id(Access::tail(b.list)));
    EXPECT_EQ(a.order(),b.order()); EXPECT_EQ(a.snapshot(),b.snapshot());
}
uint32_t next(uint32_t& seed) { seed=seed*1664525u+1013904223u; return seed; }
std::vector<unsigned> costs(unsigned mode, unsigned n, uint32_t seed=0x4b3001)
{
    std::vector<unsigned> out(n);
    for(unsigned j=0;j<n;++j)
    {
        switch(mode)
        {
        case 0: out[j]=j; break;
        case 1: out[j]=n-j; break;
        case 2: out[j]=next(seed)&65535u; break;
        case 3: out[j]=100; break;
        case 4: out[j]=1000+(next(seed)%8); break;
        case 5: out[j]=(j&1)?60000:10; break;
        default: out[j]=next(seed)%68; break;
        }
    }
    return out;
}
void sequence(const std::vector<unsigned>& values, bool intermediate)
{
    Pool pool; Fixture reference(values.size()), actual(values.size()), candidate(values.size());
    std::vector<int> model;
    for(size_t j=0;j<values.size();++j)
    {
        uint64_t a=counted(reference,j,values[j],Access::reference);
        EXPECT_EQ(a,counted(actual,j,values[j],production));
        EXPECT_EQ(a,counted(candidate,j,values[j],Access::candidate));
        // Independent identity-aware specification, including the truncated prefix.
        size_t pos=0;
        while(pos<model.size() && pos<5000 && values[model[pos]]<=values[j]) ++pos;
        EXPECT_EQ(a,pos);
        model.insert(model.begin()+pos,int(j));
        if(intermediate) { same(reference,actual); same(reference,candidate); EXPECT_EQ(actual.order(),model); }
    }
    same(reference,actual); same(reference,candidate); EXPECT_EQ(actual.order(),model);
}
TEST(RetailOpenList, EmptyOneAndTailSemantics)
{
    Pool pool; Fixture f(2);
    EXPECT_TRUE(f.list.empty());
    EXPECT_EQ(counted(f,0,10,production),0u);
    EXPECT_EQ(f.list.getHead(),&f.cells[0]); EXPECT_EQ(Access::tail(f.list),nullptr);
    EXPECT_EQ(counted(f,1,10,production),1u);
    EXPECT_EQ(f.order(),(std::vector<int>{0,1})); EXPECT_EQ(Access::tail(f.list),nullptr);
    f.remove(1); EXPECT_EQ(Access::tail(f.list),&f.cells[0]);
    f.insert(1,10,production); EXPECT_EQ(Access::tail(f.list),&f.cells[0]); // retail insertion does not maintain tail
}
TEST(RetailOpenList, DeterministicPatternsExactIntermediateOrder)
{
    for(unsigned mode=0;mode<7;++mode) { SCOPED_TRACE(mode); sequence(costs(mode,128),true); }
}
TEST(RetailOpenList, ManySeedsAndStored16BitCosts)
{
    for(unsigned seed=0;seed<128;++seed) { SCOPED_TRACE(seed); sequence(costs(2,96,seed),true); }
    Pool pool; Fixture f(3); f.insert(0,65536,production); f.insert(1,65535,production); f.insert(2,65537,production);
    EXPECT_EQ(f.order(),(std::vector<int>{0,2,1}));
}
TEST(RetailOpenList, LongEqualAndIncreasingCutoff)
{
    sequence(costs(0,6002),false); sequence(costs(3,6002),false);
}
TEST(RetailOpenList, Exact4999_5000_5001Boundary)
{
    for(unsigned n:{4999u,5000u,5001u})
    {
        SCOPED_TRACE(n); Pool pool; Fixture f(n+1);
        for(unsigned j=0;j<n;++j) f.insert(j,10,production);
        EXPECT_EQ(counted(f,n,20,production),std::min(n,5000u));
        auto order=f.order(); EXPECT_EQ(order[std::min(n,5000u)],int(n));
        EXPECT_EQ(order.size(),n+1u);
        if(n==5001) { EXPECT_EQ(order[5001],5000); EXPECT_EQ(f.cells[order[5000]].getTotalCost(),20u); EXPECT_EQ(f.cells[order[5001]].getTotalCost(),10u); }
    }
}
TEST(RetailOpenList, RemoveReinsertExactLinksAndTail)
{
    Pool pool; Fixture a(128), b(128), c(128);
    for(unsigned j=0;j<128;++j) { a.insert(j,j%7,Access::reference); b.insert(j,j%7,production); c.insert(j,j%7,Access::candidate); }
    uint32_t seed=17;
    for(unsigned j=0;j<512;++j)
    {
        unsigned id=next(seed)%128, cost=next(seed)&65535;
        a.remove(id);b.remove(id);c.remove(id);same(a,b);same(a,c);
        EXPECT_EQ(counted(a,id,cost,Access::reference),counted(b,id,cost,production));
        counted(c,id,cost,Access::candidate); same(a,b);same(a,c);
    }
}
void repair(Fixture& f, Insert fn, bool traversed)
{
    f.insert(0,10,fn);f.insert(1,20,fn);
    // Keep allocated record alive but make its owner cell lose m_info, exactly
    // the condition inspected by the production repair branch. No null m_cell dereference.
    Access::detach(f.cells[1]); f.insert(2,traversed?30:5,fn);
    if(traversed) EXPECT_EQ(Access::owner(f.infos[1]),nullptr);
    else EXPECT_EQ(Access::owner(f.infos[1]),&f.cells[1]);
    Access::restore(f.cells[1],f.infos[1]);
}
TEST(RetailOpenList, RepairOnlyWhenGuardIsTraversed)
{
    for(bool traversed:{false,true})
    {
        Pool pool;Fixture a(3),b(3),c(3);
        repair(a,Access::reference,traversed);repair(b,production,traversed);repair(c,Access::candidate,traversed);
        same(a,b);same(a,c);
        EXPECT_EQ(b.order(),traversed?(std::vector<int>{0,2}):(std::vector<int>{2,0,1}));
    }
}
TEST(RetailOpenList, SeededHeadAndLongRepresentativePatterns)
{
    { Pool pool;Fixture a(2),b(2),c(2);
      a.cells[0].setTotalCost(10);b.cells[0].setTotalCost(10);c.cells[0].setTotalCost(10);
      a.list.reset(&a.cells[0]);b.list.reset(&b.cells[0]);c.list.reset(&c.cells[0]);
      a.insert(1,10,Access::reference);b.insert(1,10,production);c.insert(1,10,Access::candidate);
      same(a,b);same(a,c); EXPECT_FALSE(b.cells[0].getOpen()); }
    for(unsigned mode:{1u,2u,4u,5u,6u}) sequence(costs(mode,6002),false);
}
TEST(RetailOpenList, RepairBeforeVersusBeyondCutoff)
{
    for(bool visited:{false,true})
    {
        Pool pool; Fixture a(5003),b(5003),c(5003);
        Fixture* fixtures[]={&a,&b,&c}; Insert functions[]={Access::reference,production,Access::candidate};
        for(unsigned k=0;k<3;++k)
        {
            auto& f=*fixtures[k];
            for(unsigned j=0;j<5002;++j) f.insert(j,10,functions[k]);
            auto order=f.order();unsigned broken=order[visited?5000:5001];
            Access::detach(f.cells[broken]);
            EXPECT_EQ(counted(f,5002,20,functions[k]),5000u);
            EXPECT_EQ(Access::owner(f.infos[broken]),visited?nullptr:&f.cells[broken]);
            Access::restore(f.cells[broken],f.infos[broken]);
        }
        same(a,b);same(a,c);
    }
}
TEST(RetailOpenList, LiveWindowChurnExactIntermediateState)
{
    for(unsigned window:{64u,384u})
    {
        Pool pool;Fixture a(1536),b(1536),c(1536);auto values=costs(2,1536);
        for(unsigned j=0;j<values.size();++j)
        {
            if(j>=window) {a.remove(j-window);b.remove(j-window);c.remove(j-window);}
            auto hops=counted(a,j,values[j],Access::reference);
            EXPECT_EQ(hops,counted(b,j,values[j],production));
            EXPECT_EQ(hops,counted(c,j,values[j],Access::candidate));
            same(a,b);same(a,c);
        }
    }
}
// A non-null next record can refer to a cell whose m_info is a different record.
// The legacy walk follows next->m_cell->m_info, not next directly. An optimization
// that collapses that round trip would inspect a different cost/link here.
TEST(RetailOpenList, NonCanonicalOwnerRoundTripRemainsObservable)
{
    Pool pool;Fixture a(4),b(4),c(4);
    Fixture* fixtures[]={&a,&b,&c};Insert functions[]={Access::reference,production,Access::candidate};
    for(unsigned k=0;k<3;++k)
    {
        auto& f=*fixtures[k];f.insert(0,10,functions[k]);f.insert(1,100,functions[k]);
        f.cells[2].setTotalCost(20);Access::owner(f.infos[1],&f.cells[2]);
        EXPECT_EQ(counted(f,3,30,functions[k]),2u);
        EXPECT_EQ(f.order(),(std::vector<int>{0,2,3}));
    }
    same(a,b);same(a,c);
}
volatile uint64_t sink=0;
TEST(RetailOpenListBenchmark, DISABLED_ProductionAndTwoStep)
{
#ifdef _DEBUG
    GTEST_SKIP()<<"Release timing only";
#endif
    const char* names[]={"increasing","decreasing","random","equal","clustered","alternating","small_range","live64","live384"};
    for(unsigned mode=0;mode<9;++mode)
    {
        const unsigned n=6002; auto values=costs(mode<7?mode:2,n);
        const unsigned window=mode==7?64:(mode==8?384:0);
        uint64_t total=0,maxHops=0;
        { Pool pool;Fixture f(n);
          for(unsigned j=0;j<n;++j) {if(window && j>=window)f.remove(j-window);auto h=counted(f,j,values[j],production);total+=h;maxHops=std::max(maxHops,h);}
          auto order=f.order(); sink=order.size();
          // Complete identity order from the real production primitive, outside timing.
          std::printf("RETAIL_ORDER,%s",names[mode]);for(int id:order)std::printf(",%d",id);std::printf("\n");
        }
        std::array<std::vector<double>,3> times;
        for(unsigned repeat=0;repeat<7;++repeat)
        {
            // Rotate order to reduce systematic temperature/scheduling/order bias.
            for(unsigned turn=0;turn<3;++turn)
            {
                unsigned impl=(repeat+turn)%3;
                Pool pool; Fixture f(n);
                Insert fn=impl==0?production:(impl==1?Access::reference:Access::candidate);
                auto start=std::chrono::steady_clock::now();
                for(unsigned j=0;j<n;++j) {if(window && j>=window)f.remove(j-window);f.insert(j,values[j],fn);}
                times[impl].push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
                sink=f.order().size();
            }
        }
        for(unsigned impl=0;impl<3;++impl)
        {
            auto& t=times[impl];std::sort(t.begin(),t.end());
            std::printf("RETAIL_LIST,%s,%s,n,%u,hops,%llu,mean_hops,%.4f,max_hops,%llu,min_ms,%.6f,median_ms,%.6f,max_ms,%.6f,window,%u\n",
                names[mode],impl==0?"production":(impl==1?"frozen_reference":"two_step_candidate"),n,
                (unsigned long long)total,double(total)/n,(unsigned long long)maxHops,t.front(),t[3],t.back(),window);
        }
    }
}
}
#else
TEST(RetailOpenList, RetailBuildRequired) { GTEST_SKIP()<<"Retail-compatible pathfinding disabled"; }
#endif
