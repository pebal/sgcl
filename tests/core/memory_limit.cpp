//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The memory ceiling (collector::set_memory_limit): what the allocators are
// left in when an allocation throws bad_alloc and the program carries on.
#include "tests/types.h"

#include <bit>
#include <vector>

namespace {
    // 16 slots per 64 KB page: one Flags word, one free-bitmap word, one
    // summary word (object_pool_allocator_base.h: _refill, _next_cursor)
    struct LimitBig {
        char bytes[4096];
    };
    static_assert(detail::TypeInfo<LimitBig>::ObjectCount == 16);
    static_assert(detail::TypeInfo<LimitBig>::FlagsCount == 1);
}

// A pool allocator whose _next_page() threw bad_alloc (the ceiling) has
// already given its current page up (owned = false, its bitmap word and
// summary bit zeroed) but still points at it. The program catches the
// exception, frees memory, a cycle sweeps the page and hands it to the
// type's buffer with its bitmap rebuilt; the next allocation of the type
// on this thread then runs _refill on a page it does not own: the bitmap
// word and the summary bit are zeroed again, the page is popped from the
// buffer with no free word left, and the slot handed out is
// data + 64 * object_size, outside the page.
TEST(MemoryLimit_Tests, AllocationAfterBadAllocStaysInsideItsPage) {
    using detail::Page;
    const auto limit_before = collector::get_memory_limit();
    std::vector<unique_ptr<LimitBig>> keep;
    keep.push_back(make_tracked<LimitBig>());
    collector::set_memory_limit(collector::get_committed_memory() + (4u << 20));
    bool threw = false;
    try {
        for (;;) {
            keep.push_back(make_tracked<LimitBig>());
        }
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
    // the allocator's current page: the one of the last object it handed out
    auto page = Page::page_of(keep.back().get());
    ASSERT_NE(page, nullptr);
    // one survivor keeps the page partly used, so that the cycle sends it to
    // the type's buffer (not back to the heap); the other 15 slots die
    auto survivor = std::move(keep.back());
    keep.pop_back();
    keep.clear();
    collector::set_memory_limit(limit_before);
    collector::force_collect(true);
    collector::force_collect(true);
    const auto free_before = std::popcount(page->free_bits()[0]);
    EXPECT_EQ(free_before, 15) << "the cycle should have rebuilt the page's bitmap: summary " << page->summary()[0]
                               << " owned " << page->owned.load() << " on_empty_list " << page->on_empty_list.load();

    // the next allocation of the type on this thread: raw storage, no
    // constructor, so that nothing is written through the pointer here
    auto next = detail::Maker<LimitBig>::make_tracked_data();
    auto raw = (uintptr_t)next.get();
    auto home = detail::Heap::page_of_checked((void*)raw);
    const bool inside_a_page = home && home->is_used && raw >= home->data && raw < home->data + config::PageSize;
    EXPECT_TRUE(inside_a_page) << "the slot lies " << (long)(raw - page->data) << " bytes past the start of the page it should come from"
                               << " (page size " << config::PageSize << "): free_bits[0] " << page->free_bits()[0]
                               << " summary[0] " << page->summary()[0] << " owned " << page->owned.load();
    // the free slots of the page after one allocation: 14 if it came from
    // this page, 15 if the allocator took another
    const auto free_after = std::popcount(page->free_bits()[0]);
    EXPECT_TRUE(free_after == 14 || free_after == 15) << "free slots on the page after one allocation: " << free_after;
    if (!inside_a_page) {
        // the deleter would look the page of an address outside every page up
        next.release();
    }
}
