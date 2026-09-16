//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The managed heap: one reserved range, 64 KB pages from 2 MB chunks, a side
// table mapping every page (of a range, too) to its header.
#include "types.h"

#include <random>
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
    auto slack = (sgcl::config::HeapFreeChunkReserve + 8) * sgcl::detail::Heap::ChunkSize;
    EXPECT_LE(heap.committed_bytes(), committed_before + slack);
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
    gc::atomic<int> elem_destroyed[ElemCount];

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
        return sgcl::config::PageSize / sizeof(Elem);
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
                ASSERT_EQ(sgcl::detail::Heap::page_of_checked(base + p * sgcl::config::PageSize), page) << "vector " << i << " page " << p;
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
// and, if that does not free enough, throws bad_alloc instead of letting the
// process grow into the OOM killer.
TEST(Heap_Tests, CommitLimitCollectsThenThrows) {
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

    // 2. live data beyond the limit: bad_alloc, and the heap stays under the cap
    bool thrown = false;
    {
        sgcl::vector<sgcl::vector<int>> keep;
        try {
            for (int i = 0; i < 300; ++i) {
                keep.emplace_back(250000, i);
            }
        } catch (const std::bad_alloc&) {
            thrown = true;
        }
        EXPECT_TRUE(thrown);
        EXPECT_LE(collector::get_committed_memory(), limit);
        EXPECT_GT(keep.size(), 8u);                     // it got well past the first chunks
        for (size_t i = 0; i < keep.size(); ++i) {      // and what it kept is intact
            ASSERT_EQ(keep[i][249999], int(i));
        }
    }

    collector::set_memory_limit(original);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
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
            EXPECT_TRUE(p + pages * sgcl::config::PageSize <= r.p || r.p + r.pages * sgcl::config::PageSize <= p);
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
