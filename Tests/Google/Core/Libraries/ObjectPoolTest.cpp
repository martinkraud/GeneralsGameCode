// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "WWLib/multilist.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Test-executable-only allocation boundary. Observe the exact block addresses
// released by the real pool destructor, without a production allocator hook.
namespace {
void *watched[8] = {};
void *released[8] = {};
unsigned releaseCount = 0;
void release(void *p) {
    if (p) for (void *block : watched) if (p == block) {
        if (releaseCount < 8) released[releaseCount++] = p;
        break;
    }
    std::free(p);
}
}
void *operator new(std::size_t size) {
    if (void *p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { release(p); }
void operator delete[](void *p) noexcept { release(p); }
void operator delete(void *p, std::size_t) noexcept { release(p); }
void operator delete[](void *p, std::size_t) noexcept { release(p); }

// The exact production instantiation that faulted: this also leaves its real
// static pool destructor registered with the CRT after the test main returns.
DEFINE_AUTO_POOL(MultiListNodeClass,256);

namespace {
template<class T, int N> struct InspectPool : ObjectPoolClass<T,N> {
    void *block() const { return this->BlockListHead; }
    int freeCount() const { return this->FreeObjectCount; }
    int capacity() const { return this->TotalObjectCount; }
    static void *next(void *block) {
        void *value; std::memcpy(&value,block,sizeof(value)); return value;
    }
};
template<int N> MultiListNodeClass *newNode(InspectPool<MultiListNodeClass,N> &pool) {
    return ::new (pool.Allocate_Object_Memory()) MultiListNodeClass;
}
struct FourAligned { std::uint32_t words[4]; };
static_assert(alignof(FourAligned) == 4);
static_assert(alignof(MultiListNodeClass) == alignof(void *));

TEST(ObjectPool, LegacyOverlapIsSpecificToEightBytePointers) {
    // Reproduce the old layout as bytes, never construct a corrupt live pool.
    for (unsigned width : {4u,8u}) {
        std::array<unsigned char,32> old{}, corrected{};
        const std::uint64_t link=0x12345678abcdef01ULL;
        std::memcpy(old.data()+4,&link,width);
        std::memcpy(corrected.data()+width,&link,width);
        bool oldHeaderZero=true, correctedHeaderZero=true;
        for (unsigned i=0;i<width;++i) {
            oldHeaderZero &= old[i]==0;
            correctedHeaderZero &= corrected[i]==0;
        }
        EXPECT_EQ(oldHeaderZero,width==4);
        EXPECT_TRUE(correctedHeaderZero);
    }
}

TEST(ObjectPool, ActualMultiListNodesHaveNonoverlappingAlignedPayload) {
    InspectPool<MultiListNodeClass,4> pool;
    std::array<MultiListNodeClass *,4> nodes{};
    for (unsigned i=0;i<nodes.size();++i) {
        nodes[i]=newNode(pool);
        auto payload=static_cast<unsigned char *>(pool.block())+sizeof(void *);
        EXPECT_EQ(reinterpret_cast<unsigned char *>(nodes[i]),payload+i*sizeof(MultiListNodeClass));
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(nodes[i])%alignof(MultiListNodeClass),0u);
        // Write every byte of every node; header must still be the null link.
        std::memset(nodes[i],0xa5,sizeof(*nodes[i]));
        EXPECT_EQ(pool.next(pool.block()),nullptr);
        EXPECT_EQ(pool.capacity(),4); EXPECT_EQ(pool.freeCount(),3-static_cast<int>(i));
    }
    for (auto node:nodes) { pool.Free_Object(node); EXPECT_EQ(pool.next(pool.block()),nullptr); }
    EXPECT_EQ(pool.freeCount(),pool.capacity());
}

TEST(ObjectPool, MultipleBlockLinksAndDestructionReleaseEachBlockInReverseOrder) {
    std::fill(std::begin(watched),std::end(watched),nullptr);
    std::fill(std::begin(released),std::end(released),nullptr); releaseCount=0;
    {
        InspectPool<MultiListNodeClass,4> pool;
        std::array<MultiListNodeClass *,13> nodes{};
        for (unsigned i=0;i<nodes.size();++i) {
            nodes[i]=newNode(pool);
            if (i%4==0) watched[i/4]=pool.block();
            std::memset(nodes[i],0x6b,sizeof(*nodes[i]));
            EXPECT_EQ(pool.next(pool.block()),i<4?nullptr:watched[i/4-1]);
        }
        EXPECT_EQ(pool.capacity(),16); EXPECT_EQ(pool.freeCount(),3);
        for (auto node:nodes) pool.Free_Object(node);
        EXPECT_EQ(pool.freeCount(),16);
        for (int i=3;i>=0;--i) EXPECT_EQ(pool.next(watched[i]),i?watched[i-1]:nullptr);
    } // The unmodified production destructor walks/deletes all four blocks.
    EXPECT_EQ(releaseCount,4u);
    for (unsigned i=0;i<4;++i) EXPECT_EQ(released[i],watched[3-i]);
    std::fill(std::begin(watched),std::end(watched),nullptr);
}

TEST(ObjectPool, ReuseIsLifoAndDoesNotGrowCapacityOrChangeBlockLinks) {
    InspectPool<MultiListNodeClass,2> pool;
    auto a=newNode(pool),b=newNode(pool),c=newNode(pool),d=newNode(pool);
    void *head=pool.block(),*previous=pool.next(head);
    pool.Free_Object(b); pool.Free_Object(c); pool.Free_Object(a);
    auto r1=newNode(pool),r2=newNode(pool),r3=newNode(pool);
    EXPECT_EQ(r1,a); EXPECT_EQ(r2,c); EXPECT_EQ(r3,b);
    EXPECT_EQ(pool.block(),head); EXPECT_EQ(pool.next(head),previous);
    EXPECT_EQ(pool.capacity(),4); EXPECT_EQ(pool.freeCount(),0);
    pool.Free_Object(r1); pool.Free_Object(r2); pool.Free_Object(r3); pool.Free_Object(d);
}

TEST(ObjectPool, FourByteAlignedObjectsKeepLegacyWin32OffsetAndSequentialOrder) {
    InspectPool<FourAligned,3> pool;
    std::array<FourAligned *,7> nodes{};
    for (unsigned i=0;i<nodes.size();++i) {
        nodes[i]=pool.Allocate_Object_Memory();
        const auto offset=reinterpret_cast<unsigned char *>(nodes[i])-static_cast<unsigned char *>(pool.block());
#if defined(_WIN64)
        EXPECT_EQ(offset,8+(i%3)*sizeof(FourAligned));
#else
        EXPECT_EQ(offset,4+(i%3)*sizeof(FourAligned)); // Exact legacy Win32 layout.
#endif
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(nodes[i])%alignof(FourAligned),0u);
        std::memset(nodes[i],0x39,sizeof(*nodes[i]));
    }
    EXPECT_EQ(pool.capacity(),9); EXPECT_EQ(pool.freeCount(),2);
    for (auto node:nodes) pool.Free_Object_Memory(node);
}

struct Constructed {
    void *link;
    static int live;
    Constructed() : link(nullptr) { ++live; }
    ~Constructed() { --live; }
};
int Constructed::live=0;
TEST(ObjectPool, ObjectConstructionAndDestructionOwnershipAreUnchanged) {
    InspectPool<Constructed,2> pool;
    auto a=pool.Allocate_Object(),b=pool.Allocate_Object(),c=pool.Allocate_Object();
    EXPECT_EQ(Constructed::live,3);
    pool.Free_Object(b); EXPECT_EQ(Constructed::live,2);
    auto reused=pool.Allocate_Object(); EXPECT_EQ(reused,b); EXPECT_EQ(Constructed::live,3);
    pool.Free_Object(a); pool.Free_Object(c); pool.Free_Object(reused);
    EXPECT_EQ(Constructed::live,0); EXPECT_EQ(pool.freeCount(),pool.capacity());
}

TEST(ObjectPool, ProductionAutoPoolCapacityCrossesThreeBlocksAndExitsCleanly) {
    std::array<MultiListNodeClass *,513> nodes{};
    for (auto &node:nodes) {
        node=new MultiListNodeClass;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(node)%alignof(MultiListNodeClass),0u);
        EXPECT_EQ(node->Prev,nullptr); EXPECT_EQ(node->Next,nullptr);
    }
    auto last=nodes.back();
    delete last;
    nodes.back()=new MultiListNodeClass;
    EXPECT_EQ(nodes.back(),last);
    for (auto node:nodes) delete node;
    // Capacity stays resident until the unchanged CRT static destructor frees
    // these three blocks. CTest requires the actual process to exit with code0.
}
}
