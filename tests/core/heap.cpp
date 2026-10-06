//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The managed heap: one reserved range, 64 KB pages from 2 MB chunks, a side
// table mapping every page (of a range, too) to its header.
#include "tests/managed_pages.h"
#include "tests/types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <random>
#include <thread>
#include <unordered_set>
#include <vector>
#if !defined(_WIN32)
#include <unistd.h>
#endif

// An array is rooted only by a pointer to its first element: an interior
// pointer, tracked or not, keeps nothing once the owner is gone.
TEST(Heap_Tests, InteriorPointerDoesNotRootAnArray) {
    const size_t before = collector::get_live_object_count();
    {
        int* alias = nullptr;   // a raw word in this frame: a candidate root for the scan
        off_frame([&] {
            sgcl::vector<int> big(100000, 7);          // ~400 KB, 7 pages
            ASSERT_TRUE(sgcl::detail::Heap::contains(big.data()));
            alias = &big[50000];                       // 4th page of the range
            EXPECT_EQ(*alias, 7);
            // every page of the range maps to the same header
            auto page = sgcl::detail::Page::page_of(&big[0]);
            EXPECT_EQ(sgcl::detail::Page::page_of(&big[50000]), page);
            EXPECT_EQ(sgcl::detail::Page::page_of(&big[99999]), page);
            EXPECT_GE(page->page_count, 7u);
        });
        // the vector is gone; the alias into the array does not keep it
        EXPECT_EQ(collector::get_live_object_count(), before);
        EXPECT_EQ(sgcl::detail::Heap::page_of_checked(alias), nullptr);
    }
    // a pointer to the first element does, wherever it is: here a raw word
    // in a frame of its own, which the count after that frame no longer sees
    off_frame([&] {
        int* first = nullptr;
        off_frame([&] {
            sgcl::vector<int> big(100000, 9);
            first = big.data();
        });
        EXPECT_EQ(collector::get_live_object_count(), before + 1);
        EXPECT_EQ(first[99999], 9);
    });
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(Heap_Tests, LargeRangesAreReusedAndDecommitted) {
    const size_t before = collector::get_live_object_count();
    auto& heap = sgcl::detail::Heap::instance();
    const auto committed_before = heap.committed_bytes();
    std::mt19937 rng(7);
    std::uniform_int_distribution<size_t> size(20000, 400000);   // 80 KB .. 1.6 MB
    off_frame([&] {
        for (int round = 0; round < 40; ++round) {
            sgcl::vector<sgcl::vector<int>> batch;
            for (int i = 0; i < 8; ++i) {
                batch.emplace_back(size(rng), round);
            }
            for (auto& v : batch) {
                ASSERT_EQ(v[v.size() / 2], round);
            }
            if (round % 10 == 9) {
                batch.clear();
                collector::force_collect(true);
            }
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), before);
    // Everything was freed. Chunks go back to the system once they have been
    // free for a whole cycle, beyond the committed reserve, so give the
    // collector a few cycles and allow the reserve plus a little slack.
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    auto slack = (sgcl::config::heap_free_chunk_reserve + 8) * sgcl::detail::Heap::ChunkSize;
    EXPECT_LE(heap.committed_bytes(), committed_before + slack);
}

namespace {
    // n objects of T made and dropped at once, in a frame of their own
    template<class T>
    SGCL_NOINLINE void churn(size_t n) {
        for (size_t i = 0; i < n; ++i) {
            tracked_ptr<T> p = make_tracked<T>();
        }
    }

    // A cycle with the caller's dead frames zeroed, three times: the
    // sweep, the release of the pages, the return of the chunks
    SGCL_ALWAYS_INLINE size_t live_bytes_settled() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
        return collector::get_statistics().live_bytes;
    }
}

// A page whose objects all die goes back to the heap. The sweep counts the
// slots it frees in the page's unused_counter_gc (page.h: uint16_t), and the
// release takes a page once the count exceeds half its slots: a page of
// 1-byte objects has 65536 slots, and 65536 freed at once wraps the counter
// to 0, so the page is never released. Four pages of uint16_t (32768 a
// page) come back; four pages of char must too.
TEST(Heap_Tests, PagesOfOneByteObjectsAreReleased) {
    const auto before = live_bytes_settled();
    churn<uint16_t>(4 * 32768 + 10);
    const auto after_u16 = live_bytes_settled();
    // the allocator keeps the partly used page it owns: at most one page more
    EXPECT_LE(after_u16, before + config::page_size) << "uint16_t: " << (after_u16 - before) / config::page_size << " pages kept";
    churn<char>(4 * 65536 + 10);
    const auto after_char = live_bytes_settled();
    EXPECT_LE(after_char, after_u16 + config::page_size) << "char: " << (after_char - after_u16) / config::page_size << " pages kept";
}

TEST(Heap_Tests, CheckedLookupRejectsForeignPointers) {
    int local = 0;
    auto foreign = std::make_unique<int>(0);
    EXPECT_FALSE(sgcl::detail::Heap::contains(&local));
    EXPECT_FALSE(sgcl::detail::Heap::contains(foreign.get()));
    EXPECT_EQ(sgcl::detail::Heap::page_of_checked(&local), nullptr);
    EXPECT_EQ(sgcl::detail::Heap::page_of_checked(foreign.get()), nullptr);
    tracked_ptr<int> managed = make_tracked<int>(1);
    EXPECT_TRUE(sgcl::detail::Heap::contains(managed.get()));
    EXPECT_NE(sgcl::detail::Heap::page_of_checked(managed.get()), nullptr);
    // a free page inside the reservation has no header
    auto past = (char*)managed.get() + 64 * sgcl::detail::Heap::ChunkSize;
    if (sgcl::detail::Heap::contains(past)) {
        EXPECT_EQ(sgcl::detail::Heap::page_of_checked(past), nullptr);
    }
}

// Per-page retention of large arrays: an alias into an element keeps only the
// pages of that element (and page 0) alive; the other pages lose their
// elements, exactly once, and go back to the heap.
namespace {
    constexpr int ElemCount = 160000;   // 16-byte elements: ~39 pages
    sgcl::atomic<int> elem_destroyed[ElemCount];

    struct Child {
        int v;
    };

    struct Elem {
        ~Elem() {
            if (id >= 0 && id < ElemCount) {
                elem_destroyed[id].fetch_add(1);
            }
        }
        int id = -1;
        tracked_ptr<Child> child;
    };

    void reset_destroyed() {
        for (auto& d : elem_destroyed) {
            d.store(0);
        }
    }

    size_t elements_per_page() {
        return sgcl::config::page_size / sizeof(Elem);
    }
}

TEST(Heap_Tests, LargeArrayFullyRetainedByItsOwner) {
    constexpr int Count = 40;
    constexpr int Elements = 250000;   // 1 MB of int per vector, 17 pages
    sgcl::vector<sgcl::vector<int>> keep;
    for (int i = 0; i < Count; ++i) {
        keep.emplace_back(Elements, i);
    }
    sgcl::vector<int> on_stack[4];
    for (int i = 0; i < 4; ++i) {
        on_stack[i] = sgcl::vector<int>(Elements, -1 - i);
    }
    for (int round = 0; round < 4; ++round) {
        collector::force_collect(true);
        // fresh garbage of the same shape would land in any freed pages
        for (int i = 0; i < Count; ++i) {
            sgcl::vector<int> garbage(Elements, 1000 + i);
            ASSERT_EQ(garbage[Elements - 1], 1000 + i);
        }
        collector::force_collect(true);
        for (int i = 0; i < Count; ++i) {
            auto base = (char*)keep[i].data();
            auto page = sgcl::detail::Page::page_of(base);
            for (size_t p = 0; p < page->page_count; ++p) {
                ASSERT_EQ(sgcl::detail::Heap::page_of_checked(base + p * sgcl::config::page_size), page) << "vector " << i << " page " << p;
            }
            for (int j = 0; j < Elements; j += 4093) {
                ASSERT_EQ(keep[i][j], i) << "vector " << i << " element " << j;
            }
            ASSERT_EQ(keep[i][Elements - 1], i);
        }
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < Elements; j += 4093) {
                ASSERT_EQ(on_stack[i][j], -1 - i);
            }
        }
    }
}

// Commit limit: above the pressure threshold the collector cycles quickly and
// returns every free chunk; at the limit an allocation forces a collection
// and, if that does not free enough, ends the program with one line on
// stderr instead of letting the process grow into the OOM killer.
TEST(Heap_Tests, CommitLimitCollectsThenEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    const auto original = collector::get_memory_limit();
    EXPECT_GT(sgcl::detail::os::memory_limit(), 0u);
    EXPECT_LE(sgcl::detail::os::memory_limit(), sgcl::detail::os::physical_memory());
    EXPECT_GT(original, 0u);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    const auto base = collector::get_committed_memory();
    const auto limit = base + 32 * sgcl::detail::Heap::ChunkSize;   // 64 MB of headroom
    collector::set_memory_limit(limit);

    // 1. short-lived garbage far beyond the limit: the collector keeps up
    for (int i = 0; i < 300; ++i) {
        sgcl::vector<int> v(250000, i);                 // 1 MB each, 300 MB in total
        ASSERT_EQ(v[1000], i);
    }
    EXPECT_LE(collector::get_committed_memory(), limit);

    // 2. live data beyond the limit: the collection frees nothing, and the
    // program ends with the diagnostic, the heap still under the cap
    EXPECT_DEATH({
        sgcl::vector<sgcl::vector<int>> keep;
        for (int i = 0; i < 300; ++i) {
            keep.emplace_back(250000, i);
        }
    }, "sgcl: out of managed memory: [0-9]+ bytes committed, limit [0-9]+");
    EXPECT_LE(collector::get_committed_memory(), limit);

    // 3. a destructor run by the sweep that allocates past the limit: it
    // cannot wait for the cycle it is part of (collector.h:
    // collect_for_allocation), so the program ends at once rather than
    // hanging
    EXPECT_DEATH({
        struct AllocatesWhenDying {
            ~AllocatesWhenDying() {
                sgcl::vector<char> buffer(size_t(256) << 20);   // past the limit, and past any free committed range
                buffer[0] = 1;
            }
        };
        off_frame([] {
            tracked_ptr<AllocatesWhenDying> p = make_tracked<AllocatesWhenDying>();
        });
        collector::clear_stack(SIZE_MAX);
        collector::set_memory_limit(collector::get_committed_memory());   // no chunk may be committed from here on
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }, "sgcl: out of managed memory");

    collector::set_memory_limit(original);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
}

// Plain memory the system refuses (a malloc or realloc of the library's
// own, an object of the system's) ends the program as managed memory
// does: one line on stderr naming what was refused, then std::terminate
TEST(Heap_Tests, RefusedPlainMemoryEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    static_assert(noexcept(sgcl::detail::os::memory_refused("a block", 1)));
    EXPECT_DEATH(sgcl::detail::os::memory_refused("a block of the test", 4096),
                 "sgcl: out of memory: a block of the test of 4096 bytes was refused");
    EXPECT_DEATH(sgcl::detail::os::memory_refused("an object of the system"),
                 "sgcl: out of memory: an object of the system was refused");
}

// The free ranges are binned by size; ranges handed out never overlap and
// freed neighbours coalesce back into one range.
TEST(Heap_Tests, FreeRangesAreBinnedAndCoalesce) {
    auto& heap = sgcl::detail::Heap::instance();
    std::mt19937_64 rng(11);
    struct R {
        char* p;
        size_t pages;
    };
    std::vector<R> ranges;
    for (int i = 0; i < 200; ++i) {
        size_t pages = 1 + rng() % 70;
        auto p = (char*)heap.alloc_range(pages);
        ASSERT_NE(p, nullptr);
        for (auto& r : ranges) {   // no overlap with anything still held
            EXPECT_TRUE(p + pages * sgcl::config::page_size <= r.p || r.p + r.pages * sgcl::config::page_size <= p);
        }
        ranges.push_back({p, pages});
        if (i % 3 == 2) {   // churn: free a random one
            auto k = rng() % ranges.size();
            heap.free_range(ranges[k].p, ranges[k].pages);
            ranges.erase(ranges.begin() + k);
        }
    }
    auto held = heap.free_range_count();
    std::shuffle(ranges.begin(), ranges.end(), rng);
    for (auto& r : ranges) {
        heap.free_range(r.p, r.pages);
    }
    // everything freed coalesces: the ranges that were free before, plus at
    // most the spans the test carved from fresh chunks (they touch each
    // other, so they merge into one)
    EXPECT_LE(heap.free_range_count(), held);
    EXPECT_GE(held, 1u);
}

// A range of more pages than the whole heap is refused before anything is
// computed from its count: 2^32 pages and more put the request's bin past
// the 32 bits of the bins' mask (a shift by 32 or more, undefined; UBSan
// reports it). One page past the heap, 2^32 pages and SIZE_MAX / page_size
// pages each give null and leave the free ranges as they were; through a
// buffer's allocator the null ends the program as any refused range does.
TEST(Heap_Tests, ARangeLargerThanTheHeapIsRefused) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    auto& heap = sgcl::detail::Heap::instance();
    const size_t heap_pages = sgcl::detail::Heap::globals.size / sgcl::config::page_size;
    const size_t free_ranges = heap.free_range_count();
    for (size_t pages : {heap_pages + 1, size_t(1) << 32, SIZE_MAX / sgcl::config::page_size}) {
        EXPECT_EQ(heap.alloc_range(pages), nullptr) << pages;
        EXPECT_EQ(heap.free_range_count(), free_ranges) << pages;
    }
    // the heap still hands out a range after the refusals
    auto p = heap.alloc_range(3);
    ASSERT_NE(p, nullptr);
    heap.free_range(p, 3);

    auto past_the_heap = [] {
        auto data = sgcl::detail::Maker<uint64_t[]>::make_tracked_data((size_t(1) << 32) * sgcl::config::page_size / 8);
        (void)data;
    };
    auto largest_buffer = [] {
        auto data = sgcl::detail::Maker<uint64_t[]>::make_tracked_data(sgcl::detail::buffer_max_capacity(8));
        (void)data;
    };
    EXPECT_DEATH(past_the_heap(), "sgcl: out of managed memory");
    EXPECT_DEATH(largest_buffer(), "sgcl: out of managed memory");
}

// collector::get_statistics(): counters stored at the end of a cycle, read
// without stopping the collector.
TEST(Heap_Tests, StatisticsFollowTheCycles) {
    auto before = collector::get_statistics();
    {
        sgcl::tracked_ptr<int> p = sgcl::make_tracked<int>(1);
        sgcl::tracked_ptr<int> q = sgcl::make_tracked<int>(2);
        collector::force_collect(true);
        auto during = collector::get_statistics();
        EXPECT_GT(during.cycles, before.cycles);
        EXPECT_GE(during.cycles, during.full_cycles);
        EXPECT_GE(during.live_objects, 2u);
        EXPECT_GE(during.last_cycle_ms, 0.0);
        EXPECT_GE(during.helper_threads, during.last_helpers_used);
        EXPECT_GT(during.live_bytes, 0u);
        EXPECT_GE(during.committed_bytes, during.live_bytes);
    }
    collector::force_collect(true);
    auto after = collector::get_statistics();
    EXPECT_GT(after.cycles, before.cycles + 1);
    EXPECT_EQ(after.live_objects, collector::get_live_object_count());
}

#if !defined(_WIN32)
// The child of a fork reads the managed heap (a copy-on-write snapshot)
// and exits; a managed allocation that needs a page, or a collection,
// fails there with a message instead of hanging on a copied lock or
// waiting for the collector's thread, which the child does not have.
// gtest's death tests fork exactly so.
TEST(Heap_Tests, TheChildOfAForkReadsAndExitsOrFails) {
    struct Node { int value; tracked_ptr<Node> next; };
    struct Fresh { long a[64]; };
    tracked_ptr node = make_tracked<Node>(1, make_tracked<Node>(2));
    collector::force_collect(true);   // the collector's thread exists: the atfork handler is registered
    EXPECT_EXIT(::_exit(node->value == 1 && node->next->value == 2 ? 0 : 1), ::testing::ExitedWithCode(0), "");   // the snapshot read
    EXPECT_DEATH(collector::force_collect(true), "a collection requested in the child of a fork");
    EXPECT_DEATH((void)make_tracked<Fresh>(), "a managed allocation in the child of a fork");                     // the pages of a fresh type
}
#endif

// The Metadata of a type (metadata.h): what every page of the type shares,
// made once per type on its first use and never freed. Checked field by
// field for each kind the collector treats apart: a pool type with a
// pointer and a destructor, a plain one, one larger than a page, the
// weak cells, the blocks of cells, the holders of a to_shared, a
// conservative type, a size class of the strings and of the buffers, and
// the range of a buffer past a page.
namespace {
    struct MetaNode {
        long value;
        tracked_ptr<MetaNode> next;
    };

    struct MetaPlain {
        int a, b, c;
    };

    struct MetaLarge {
        char bytes[70000];
    };

    struct MetaFresh {
        long a, b, c, d, e;
    };

    struct MetaRaced {
        long a, b, c;
    };

    template<class T>
    void expect_kinds(const detail::Metadata& m, bool weak_cell, bool cell_block, bool root_holder, bool string) {
        EXPECT_FALSE(m.is_array) << typeid(T).name();
        EXPECT_EQ(m.is_weak_cell, weak_cell) << typeid(T).name();
        EXPECT_EQ(m.is_cell_block, cell_block) << typeid(T).name();
        EXPECT_EQ(m.is_root_holder, root_holder) << typeid(T).name();
        EXPECT_EQ(m.is_string, string) << typeid(T).name();
    }

    // The fields every type's Metadata takes from the type itself
    template<class T>
    void expect_type_metadata(size_t object_size, size_t object_count, bool pool, bool destroy) {
        using Info = detail::TypeInfo<T>;
        auto& m = Info::private_metadata();
        EXPECT_EQ(&m, &Info::private_metadata()) << "made once";
#if defined(SGCL_ASAN)
        // the slot holds a redzone past the object (page_info.h: SlotSize)
        EXPECT_EQ(m.user_size, object_size) << typeid(T).name();
        EXPECT_EQ(m.object_size, Info::SlotSize) << typeid(T).name();
        EXPECT_EQ(m.object_count, Info::ObjectCount) << typeid(T).name();
        (void)object_count;
#else
        EXPECT_EQ(m.object_size, object_size) << typeid(T).name();
        EXPECT_EQ(m.object_count, object_count) << typeid(T).name();
#endif
        EXPECT_EQ(m.pool_allocated, pool) << typeid(T).name();
        EXPECT_EQ(m.destroy != nullptr, destroy) << typeid(T).name();
        EXPECT_EQ(m.destroy, Info::get_destroy_function()) << typeid(T).name();
        EXPECT_EQ(m.free, Info::Allocator::free) << typeid(T).name();
        if (pool) {
            EXPECT_EQ(m.free, &detail::ObjectPoolAllocatorBase::free_pool_pages) << typeid(T).name();
        }
        EXPECT_EQ(m.type_info, typeid(T));
        EXPECT_EQ(&m.child_pointers, &Info::child_pointers()) << typeid(T).name();
        EXPECT_EQ(m.child_pointers.type, typeid(T));
        EXPECT_EQ(m.header_slab, &Info::header_slab()) << typeid(T).name();
        EXPECT_NE(m.header_slab, nullptr);
    }
}

TEST(Heap_Tests, TheMetadataOfEachKindOfType) {
    using namespace sgcl::detail;
    // a pool type with a pointer and a destructor
    expect_type_metadata<MetaNode>(16, 4096, true, true);
    expect_kinds<MetaNode>(TypeInfo<MetaNode>::private_metadata(), false, false, false, false);
    EXPECT_EQ(TypeInfo<MetaNode>::child_pointers().map.size(), 1u);
    EXPECT_FALSE(TypeInfo<MetaNode>::child_pointers().conservative);
    // its objects' pages name it
    auto node = make_tracked<MetaNode>();
    EXPECT_EQ(Page::page_of(node.get())->metadata, &TypeInfo<MetaNode>::private_metadata());

    // plain data: no destructor, no pointer map
    expect_type_metadata<MetaPlain>(12, 65536 / 12, true, false);
    expect_kinds<MetaPlain>(TypeInfo<MetaPlain>::private_metadata(), false, false, false, false);
    EXPECT_EQ(TypeInfo<MetaPlain>::child_pointers().map.size(), 0u);
    EXPECT_FALSE(TypeInfo<MetaPlain>::child_pointers().any.load());

    // larger than a page: a range of its own, one object
    expect_type_metadata<MetaLarge>(70000, 1, false, false);
    expect_kinds<MetaLarge>(TypeInfo<MetaLarge>::private_metadata(), false, false, false, false);
    EXPECT_EQ(TypeInfo<MetaLarge>::private_metadata().free, &ObjectAllocator<MetaLarge>::free);

    // the kinds the collector treats apart
    expect_type_metadata<WeakCell>(sizeof(WeakCell), 65536 / sizeof(WeakCell), true, std::is_trivially_destructible_v<WeakCell> == false);
    expect_kinds<WeakCell>(TypeInfo<WeakCell>::private_metadata(), true, false, false, false);
    EXPECT_EQ(TypeInfo<WeakCell>::child_pointers().map.size(), 0u);
    expect_type_metadata<CellBlock>(sizeof(CellBlock), std::max<size_t>(1, 65536 / sizeof(CellBlock)), true, std::is_trivially_destructible_v<CellBlock> == false);
    expect_kinds<CellBlock>(TypeInfo<CellBlock>::private_metadata(), false, true, true, false);
    EXPECT_TRUE(TypeInfo<CellBlock>::child_pointers().any.load());
    expect_type_metadata<SharedHolder>(8, 8192, true, true);
    expect_kinds<SharedHolder>(TypeInfo<SharedHolder>::private_metadata(), false, false, true, false);

    // conservative: the map full and kept so
    expect_type_metadata<FrameWord>(8, 8192, true, false);
    expect_kinds<FrameWord>(TypeInfo<FrameWord>::private_metadata(), false, false, false, false);
    EXPECT_TRUE(TypeInfo<FrameWord>::child_pointers().conservative);
    EXPECT_EQ(TypeInfo<FrameWord>::child_pointers().map.size(), 1u);
    EXPECT_EQ(TypeInfo<FrameWord>::child_pointers().word(0), 1u);

    // a size class of the strings: a pool described by data
    sgcl::string text("abcdefghijklmnopqrstuvwxyz");
    auto& sm = *Page::page_of(text.data())->metadata;
    EXPECT_TRUE(sm.is_string);
    EXPECT_FALSE(sm.is_array);
    EXPECT_FALSE(sm.is_weak_cell || sm.is_cell_block || sm.is_root_holder);
    EXPECT_TRUE(sm.pool_allocated);
    EXPECT_EQ(sm.destroy, nullptr);
    EXPECT_EQ(sm.free, &ObjectPoolAllocatorBase::free_pool_pages);
    EXPECT_EQ(sm.object_count, 65536 / sm.object_size);
    EXPECT_EQ(sm.type_info, sm.child_pointers.type);
    EXPECT_EQ(sm.child_pointers.map.size(), 0u);
    EXPECT_NE(std::string(sm.type_info.name()).find("StringSlot"), std::string::npos);
#if defined(SGCL_ASAN)
    EXPECT_EQ(&sm, StringPools::metadata[(sm.user_size / 4) - 1].load());   // object_size is the slot, with its redzone (page_info.h: SlotSize)
#else
    EXPECT_EQ(&sm, StringPools::metadata[(sm.object_size / 4) - 1].load());
#endif

    // a size class of the buffers: 10 ints in the class of 48 bytes
#if defined(SGCL_ASAN)
    sgcl::vector<int> small(8);   // 32 bytes and the redzone of 16 the class is chosen with (maker.h)
#else
    sgcl::vector<int> small(10);
#endif
    auto& bm = *Page::page_of(small.data())->metadata;
    EXPECT_TRUE(bm.is_array);
    EXPECT_FALSE(bm.is_string || bm.is_weak_cell || bm.is_cell_block || bm.is_root_holder);
    EXPECT_TRUE(bm.pool_allocated);
    EXPECT_EQ(bm.object_size, 64u);
    EXPECT_EQ(bm.object_count, 1024u);
    EXPECT_EQ(bm.destroy, nullptr);
    EXPECT_EQ(bm.free, &ObjectPoolAllocatorBase::free_pool_pages);
    EXPECT_EQ(bm.type_info, typeid(Array<48>));
    EXPECT_EQ(bm.child_pointers.type, typeid(Array<48>));
    EXPECT_EQ(bm.child_pointers.map.size(), 0u);
    EXPECT_FALSE(bm.child_pointers.conservative);

    // a buffer past a page: a range of its own
    sgcl::vector<int> large(100000);
    auto& rm = *Page::page_of(large.data())->metadata;
    EXPECT_TRUE(rm.is_array);
    EXPECT_FALSE(rm.pool_allocated);
    EXPECT_EQ(rm.object_count, 1u);
    EXPECT_EQ(rm.object_size, sizeof(Array<PageDataSize>));
    EXPECT_EQ(rm.destroy, nullptr);
    // (the range's page layout and Metadata are those of a page's worth,
    // array.h: PageInfo<Array<>>)
    EXPECT_EQ(rm.free, &ObjectAllocator<Array<PageDataSize>>::free);
    EXPECT_EQ(rm.type_info, typeid(Array<PageDataSize>));
    EXPECT_EQ(&rm, &TypeInfo<Array<>>::private_metadata());

    // the buffers of an element type share its pointer map
    EXPECT_EQ(&TypeInfo<MetaNode>::array_metadata().child_pointers, &TypeInfo<MetaNode>::child_pointers());
    EXPECT_EQ(TypeInfo<MetaNode>::array_metadata().type_info, typeid(MetaNode[]));
    EXPECT_EQ(TypeInfo<MetaNode>::array_metadata().object_size, 16u);

    // the slab of a type's page headers hands out headers of its size
    // (a fresh slab: two headers in a row from its first block)
    auto& slab = TypeInfo<MetaFresh>::header_slab();
    auto h1 = (char*)slab.alloc();
    auto h2 = (char*)slab.alloc();
    EXPECT_EQ(size_t(h2 - h1), (TypeInfo<MetaFresh>::HeaderSize + config::cache_line_size - 1) & ~(config::cache_line_size - 1));
    slab.free(h2);
    slab.free(h1);
    EXPECT_EQ(TypeInfo<MetaFresh>::private_metadata().header_slab, &slab);

    // threads making the Metadata of a new type at once get one
    std::atomic<int> ready = {0};
    const Metadata* seen[4] = {};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            ready.fetch_add(1);
            while (ready.load() < 4) {
            }
            seen[t] = &TypeInfo<MetaRaced>::private_metadata();
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    for (auto m : seen) {
        EXPECT_EQ(m, seen[0]);
    }
#if defined(SGCL_ASAN)
    EXPECT_EQ(seen[0]->user_size, 24u);   // object_size is the slot, with its redzone (page_info.h: SlotSize)
#else
    EXPECT_EQ(seen[0]->object_size, 24u);
#endif
}

// What a slot holds before its constructor runs, where the collector may
// already read it (maker.h: _init). A page goes back to the heap as its last
// type left it (object_pool_allocator_base.h: _free); a type whose pointer
// map has an offset zeroes every page it takes from the heap (_next_page),
// a type of plain data takes it as it is. A slot on the type's own pages
// holds what the type's last object there left, its pointer words null.
using slot_probe::PageSet;
using slot_probe::Probed;
using slot_probe::Shape;
using slot_probe::probe_slots;
using heap_count::settle;

namespace {
    struct Garbage64 {
        uint64_t w[8];
    };

    struct Plain64 {
        uint64_t w[8];
    };

    struct Pointers64 {
        tracked_ptr<int> p[8];
    };

    struct Linked64 {
        tracked_ptr<int> a;
        uint64_t pad[6];
        tracked_ptr<int> b;
    };

    constexpr size_t PerPage64 = config::page_size / 64;

    // n objects of plain data filled with non-zero bytes and dropped; the
    // pages they took, entirely free, go back to the heap
    SGCL_NOINLINE PageSet fill_pages_with_garbage(size_t n) {
        PageSet pages;
        for (size_t i = 0; i < n; ++i) {
            tracked_ptr<Garbage64> g = make_tracked<Garbage64>();
            for (auto& w : g->w) {
                w = 0xA5A5A5A5A5A5A5A5ull ^ i;
            }
            pages.add(g.get());
        }
        return pages;
    }
}

TEST(Heap_Tests, APageAPointerTypeTakesFromTheHeapIsZeroed) {
    auto pages = fill_pages_with_garbage(64 * PerPage64);
    settle();
    auto r = probe_slots<Pointers64>(2 * 64 * PerPage64, pages);
    ASSERT_GT(r.reused, 0u) << "no page of the garbage came back";
    EXPECT_EQ(r.nonzero, 0u) << "of " << r.reused << " slots on pages the garbage left";
}

// The page of plain data is not written when it is taken: a type that cannot
// hold pointers gets the bytes the last type left (make_tracked.md)
TEST(Heap_Tests, APageAPlainTypeTakesFromTheHeapIsNotZeroed) {
    auto pages = fill_pages_with_garbage(64 * PerPage64);
    settle();
    auto r = probe_slots<Plain64>(2 * 64 * PerPage64, pages);
    ASSERT_GT(r.reused, 0u) << "no page of the garbage came back";
    EXPECT_GT(r.nonzero, 0u) << "of " << r.reused << " slots on pages the garbage left";
}

// The slots a type takes again on its own pages (more than half freed, kept
// for the type's allocators, not zeroed): every way an object of a type that
// may hold tracked pointers dies leaves its pointer words null, so the
// collector, which may read a slot from its allocation on, never meets a
// stale pointer there before the constructor runs. A test keeps some
// objects on every page, so that the pages stay the type's.
namespace {
    template<class T, class Make>
    SGCL_NOINLINE PageSet fill_and_keep_some(size_t n, Make&& make, sgcl::vector<tracked_ptr<T>>& kept) {
        PageSet pages;
        for (size_t i = 0; i < n; ++i) {
            tracked_ptr<T> p = make();
            pages.add(p.get());
            if (i % 256 == 0) {
                kept.push_back(p);
            }
        }
        return pages;
    }
}

TEST(Heap_Tests, ASlotReusedAfterTheSweepDestroyedItsObjectHoldsNullPointers) {
    tracked_ptr<int> target = make_tracked<int>(1);
    sgcl::vector<tracked_ptr<Linked64>> kept;
    auto pages = fill_and_keep_some<Linked64>(4 * PerPage64, [&] {
        tracked_ptr<Linked64> p = make_tracked<Linked64>();
        p->a = target;
        p->b = target;
        for (auto& w : p->pad) {
            w = 0x5A5A5A5A5A5A5A5Aull;
        }
        return p;
    }, kept);
    settle();
    Shape<Linked64> shape;
    auto r = probe_slots<Linked64>(4 * PerPage64, pages, shape.offsets({&shape->a, &shape->b}));
    ASSERT_GT(r.reused, 0u);
    EXPECT_EQ(r.nonzero, 0u) << "of " << r.reused << " reused slots";
}

TEST(Heap_Tests, ASlotReusedAfterAUniquePtrDestroyedItsObjectHoldsNullPointers) {
    tracked_ptr<int> target = make_tracked<int>(1);
    sgcl::vector<tracked_ptr<Linked64>> kept;
    PageSet pages;
    off_frame([&] {
        for (size_t i = 0; i < 4 * PerPage64; ++i) {
            unique_ptr<Linked64> u = make_tracked<Linked64>();
            u->a = target;
            u->b = target;
            pages.add(u.get());
            if (i % 256 == 0) {
                kept.push_back(std::move(u));
            }
        }
    });
    settle();
    Shape<Linked64> shape;
    auto r = probe_slots<Linked64>(4 * PerPage64, pages, shape.offsets({&shape->a, &shape->b}));
    ASSERT_GT(r.reused, 0u);
    EXPECT_EQ(r.nonzero, 0u) << "of " << r.reused << " reused slots";
}

TEST(Heap_Tests, ASlotOfATrackedPtrObjectReusedHoldsNull) {
    tracked_ptr<int> target = make_tracked<int>(1);
    sgcl::vector<tracked_ptr<tracked_ptr<int>>> kept;
    const size_t n = 4 * (config::page_size / sizeof(tracked_ptr<int>));
    auto pages = fill_and_keep_some<tracked_ptr<int>>(n, [&] {
        return tracked_ptr<tracked_ptr<int>>(make_tracked<tracked_ptr<int>>(target));
    }, kept);
    settle();
    auto r = probe_slots<tracked_ptr<int>>(n, pages);
    ASSERT_GT(r.reused, 0u);
    EXPECT_EQ(r.nonzero, 0u) << "of " << r.reused << " reused slots";
}

// The nodes of the containers: erase destroys the element and nulls the
// links before the slot's state says Destroyed (the sweep then frees the
// node without its destructor); a node that dies with its container keeps
// its element and links to the sweep, which runs its destructor.
namespace slot_probe {
    struct ListNodeTag {};
    auto exposed(ListNodeTag);
    template struct Expose<ListNodeTag, sgcl::list<tracked_ptr<int>>::Node>;

    struct ForwardListNodeTag {};
    auto exposed(ForwardListNodeTag);
    template struct Expose<ForwardListNodeTag, sgcl::forward_list<tracked_ptr<int>>::Node>;

    struct DequeBlockTag {};
    auto exposed(DequeBlockTag);
    template struct Expose<DequeBlockTag, sgcl::deque<tracked_ptr<int>>::Block>;
}

namespace {
    using ListNode = decltype(exposed(slot_probe::ListNodeTag{}))::type;
    using ForwardListNode = decltype(exposed(slot_probe::ForwardListNodeTag{}))::type;
    using DequeBlock = decltype(exposed(slot_probe::DequeBlockTag{}))::type;

    std::vector<size_t> list_node_words() {
        Shape<ListNode> shape;
        return shape.offsets({&shape->prev, &shape->next, &shape->slot.value});
    }

    std::vector<size_t> forward_list_node_words() {
        Shape<ForwardListNode> shape;
        return shape.offsets({&shape->next, &shape->slot.value});
    }

    // Two containers filled in turns, so that their nodes share pages:
    // `dying` is emptied by `kill` (or dropped whole), `kept`, a node
    // for every four of the other, keeps the pages its type's
    template<class Node, class C, class Kill>
    Probed probe_nodes(size_t n, std::vector<size_t> words, Kill&& kill) {
        tracked_ptr<int> target = make_tracked<int>(1);
        C kept;
        PageSet pages;
        off_frame([&] {
            C dying;
            for (size_t i = 0; i < n; ++i) {
                dying.push_front(target);
                pages.add(&dying.front());
                if (i % 4 == 0) {
                    kept.push_front(target);   // a quarter: the pages end more than half free
                }
            }
            kill(dying);
        });
        settle();
        auto r = probe_slots<Node>(2 * n, pages, std::move(words));
        EXPECT_EQ(std::distance(kept.begin(), kept.end()), std::ptrdiff_t((n + 3) / 4));
        return r;
    }
}

TEST(Heap_Tests, AListNodeReusedHoldsNullPointers) {
    using List = sgcl::list<tracked_ptr<int>>;
    const size_t n = 2 * (config::page_size / sizeof(ListNode));
    auto erased = probe_nodes<ListNode, List>(n, list_node_words(), [](List& l) {
        for (auto it = l.begin(); it != l.end();) {
            it = l.erase(it);
        }
    });
    auto popped = probe_nodes<ListNode, List>(n, list_node_words(), [](List& l) {
        while (!l.empty()) {
            l.pop_front();
            if (!l.empty()) {
                l.pop_back();
            }
        }
    });
    auto cleared = probe_nodes<ListNode, List>(n, list_node_words(), [](List& l) {
        l.clear();
    });
    auto dropped = probe_nodes<ListNode, List>(n, list_node_words(), [](List&) {
        // the list dies with its elements: the sweep destroys its nodes
    });
    for (auto [name, r] : {std::pair{"erase", erased}, {"pop", popped}, {"clear", cleared}, {"dropped", dropped}}) {
        EXPECT_GT(r.reused, 0u) << name;
        EXPECT_EQ(r.nonzero, 0u) << name << ": of " << r.reused << " reused slots";
    }
}

TEST(Heap_Tests, AForwardListNodeReusedHoldsNullPointers) {
    using List = sgcl::forward_list<tracked_ptr<int>>;
    const size_t n = 2 * (config::page_size / sizeof(ForwardListNode));
    auto erased = probe_nodes<ForwardListNode, List>(n, forward_list_node_words(), [](List& l) {
        while (std::next(l.begin()) != l.end()) {
            l.erase_after(l.begin());
        }
        l.pop_front();
    });
    auto popped = probe_nodes<ForwardListNode, List>(n, forward_list_node_words(), [](List& l) {
        while (!l.empty()) {
            l.pop_front();
        }
    });
    auto cleared = probe_nodes<ForwardListNode, List>(n, forward_list_node_words(), [](List& l) {
        l.clear();
    });
    auto dropped = probe_nodes<ForwardListNode, List>(n, forward_list_node_words(), [](List&) {
    });
    for (auto [name, r] : {std::pair{"erase_after", erased}, {"pop_front", popped}, {"clear", cleared}, {"dropped", dropped}}) {
        EXPECT_GT(r.reused, 0u) << name;
        EXPECT_EQ(r.nonzero, 0u) << name << ": of " << r.reused << " reused slots";
    }
}

// A block of a deque emptied by pops and let go of: its elements were
// destroyed by the pops, so the block's slot holds null element words when
// a block of the type takes it again
TEST(Heap_Tests, ADequeBlockReusedHoldsNullPointers) {
    using Deque = sgcl::deque<tracked_ptr<int>>;
    constexpr size_t PerBlock = sizeof(DequeBlock::elems) / sizeof(tracked_ptr<int>);
    const size_t blocks = 4 * (config::page_size / sizeof(DequeBlock));
    tracked_ptr<int> target = make_tracked<int>(1);
    Deque kept;
    PageSet pages;
    off_frame([&] {
        Deque dying;
        for (size_t i = 0; i < blocks * PerBlock; ++i) {
            dying.push_back(target);
            if (i % PerBlock == 0) {
                pages.add(&dying.back());
                kept.push_back(target);   // a block of the other deque between every two of this one
                for (size_t k = 1; k < PerBlock; ++k) {
                    kept.push_back(target);
                }
            }
        }
        for (size_t i = 0; !dying.empty(); ++i) {
            if (i % 2) {
                dying.pop_back();
            } else {
                dying.pop_front();
            }
        }
    });
    settle();
    std::vector<size_t> words;
    for (size_t k = 0; k < PerBlock; ++k) {
        words.push_back(k * sizeof(tracked_ptr<int>));
    }
    auto r = probe_slots<DequeBlock>(2 * blocks, pages, words);
    ASSERT_GT(r.reused, 0u);
    EXPECT_EQ(r.nonzero, 0u) << "of " << r.reused << " reused blocks";
    EXPECT_EQ(kept.size(), blocks * PerBlock);
}

TEST(Heap_Tests, AnErasedTreeNodeReusedHoldsNullPointers) {
    using Map = sgcl::sorted_map<int, tracked_ptr<int>>;
    using Node = sgcl::detail::RbNode<Map::value_type>;
    tracked_ptr<int> target = make_tracked<int>(1);
    Map m;
    PageSet pages;
    const size_t n = 4 * (config::page_size / sizeof(Node));
    off_frame([&] {
        for (size_t i = 0; i < n; ++i) {
            auto it = m.emplace(int(i), target).first;
            pages.add(&*it);
        }
        for (size_t i = 0; i < n; ++i) {
            if (i % 256) {
                m.erase(int(i));
            }
        }
    });
    settle();
    Shape<Node> shape;
    auto r = probe_slots<Node>(n, pages, shape.offsets({&shape->parent, &shape->left, &shape->right, &shape->slot.value.second}));
    ASSERT_GT(r.reused, 0u);
    EXPECT_EQ(r.nonzero, 0u) << "of " << r.reused << " reused slots";
    EXPECT_EQ(m.size(), (n + 255) / 256);
}

TEST(Heap_Tests, AnErasedHashNodeReusedHoldsNullPointers) {
    using Map = sgcl::map<int, tracked_ptr<int>>;
    using Node = sgcl::detail::HashNode<Map::value_type>;
    tracked_ptr<int> target = make_tracked<int>(1);
    Map m;
    PageSet pages;
    const size_t n = 4 * (config::page_size / sizeof(Node));
    off_frame([&] {
        for (size_t i = 0; i < n; ++i) {
            auto it = m.emplace(int(i), target).first;
            pages.add(&*it);
        }
        for (size_t i = 0; i < n; ++i) {
            if (i % 256) {
                m.erase(int(i));
            }
        }
    });
    settle();
    Shape<Node> shape;
    auto r = probe_slots<Node>(n, pages, shape.offsets({&shape->next, &shape->slot.value.second}));
    ASSERT_GT(r.reused, 0u);
    EXPECT_EQ(r.nonzero, 0u) << "of " << r.reused << " reused slots";
}

// A vector abandons its buffer without the destructors of tracked elements
// (vector.h: _destroy_range), so a dead buffer keeps its pointer words; the
// slot is taken again by any buffer of its size class, and a buffer whose
// elements may hold pointers is zeroed when it is issued (maker.h: _zero).
TEST(Heap_Tests, ABufferOfPointersReusingADeadVectorsSlotIsZeroed) {
    tracked_ptr<int> target = make_tracked<int>(1);
    sgcl::vector<sgcl::vector<tracked_ptr<int>>> kept;
    PageSet pages;
    constexpr size_t Capacity = 6;   // the 48-byte class
    const size_t n = 4 * (config::page_size / (48 + sizeof(sgcl::detail::ArrayBase)));
    off_frame([&] {
        for (size_t i = 0; i < n; ++i) {
            sgcl::vector<tracked_ptr<int>> v(Capacity, target);
            pages.add(v.data());
            if (i % 256 == 0) {
                kept.push_back(v);
            }
        }
    });
    settle();
    Probed plain;
    Probed pointers;
    {
        collector::stepper parked(true);
        parked.advance_to(collector::stepper::phase::start);
        // the dead buffers' words, seen through buffers of plain data of
        // the same class (not zeroed)
        for (size_t i = 0; i < n; ++i) {
            auto b = sgcl::detail::Maker<uint64_t[]>::make_tracked_data(Capacity);
            if (pages.has(b.get())) {
                ++plain.reused;
                for (size_t k = 0; k < Capacity; ++k) {
                    plain.nonzero += b.get()[k] == uintptr_t(target.get());
                }
            }
        }
        for (size_t i = 0; i < n; ++i) {
            auto b = sgcl::detail::Maker<tracked_ptr<int>[]>::make_tracked_data(Capacity);
            auto words = (const uintptr_t*)b.get();
            if (pages.has(words)) {
                ++pointers.reused;
                for (size_t k = 0; k < Capacity; ++k) {
                    pointers.nonzero += words[k] != 0;
                }
            }
        }
        parked.finish_cycle();
    }
    ASSERT_GT(pointers.reused, 0u);
    EXPECT_EQ(pointers.nonzero, 0u) << "of " << pointers.reused << " reused buffers";
    std::printf("[   info   ] dead vector buffers seen through plain buffers: %zu of %zu words still point at the target\n", plain.nonzero, plain.reused * Capacity);
}

#if !(defined(__has_feature) && (__has_feature(address_sanitizer) || __has_feature(thread_sanitizer)))
// The memory of many threads allocating plain data stays bounded: the pages
// the collector frees go back to the heap without a write on its thread, so
// it keeps up with the allocators (the zeroing of every freed page on the
// collector's thread let the heap grow without bound under 8 threads).
// The bound assumes the collector's threads get a core: the allocating
// threads never wait for it, so on a machine loaded by other programs the
// peak grows past it (712 MB at a load of 22; docs/garbage_collector/
// overview.md, Memory). A failure here under load is that, not a leak.
TEST(Heap_Tests, ManyThreadsAllocatingPlainDataStayBounded) {
    struct Small {
        int64_t w[2];
    };
    std::atomic<bool> stop = false;
    size_t peak = 0;
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            tracked_ptr<Small> keep;
            while (!stop.load(std::memory_order_relaxed)) {
                for (int i = 0; i < 4096; ++i) {
                    keep = make_tracked<Small>();
                }
            }
        });
    }
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < end) {
        peak = std::max(peak, collector::get_statistics().committed_bytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stop = true;
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_LT(peak, size_t(512) << 20) << "peak committed " << (peak >> 20) << " MB";
    std::printf("[   info   ] peak committed %zu MB\n", peak >> 20);
}
#endif
