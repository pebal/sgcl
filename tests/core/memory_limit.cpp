//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The memory ceiling (collector::set_memory_limit): what the pool allocator
// is left in when the heap refuses it a page and the maker's collection
// (maker.h: MakerBase::allocate) frees enough for the second try.
#include "tests/types.h"

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

// A pool allocator whose _next_page() found no page (the ceiling) has
// already given its current page up (owned = false, its bitmap word and
// summary bit zeroed); it must not name it any more (_refill sets
// _current_page to null first). With the page still named, the retry
// after the collection, which may have swept the page and handed it to
// the type's buffer with its bitmap rebuilt, ran _refill on a page it does
// not own: the bitmap word and the summary bit zeroed again, the page
// popped from the buffer with no free word left, and the slot handed out
// data + 64 * object_size, outside the page. Garbage far past a small
// ceiling makes the heap refuse pages again and again, the collection
// freeing them each time: every slot must lie inside a page in use.
TEST(MemoryLimit_Tests, AllocationAfterARefusalStaysInsideItsPage) {
    const auto limit_before = collector::get_memory_limit();
    collector::force_collect(true);
    collector::set_memory_limit(collector::get_committed_memory() + (4u << 20));
    int outside = 0;
    for (int i = 0; i < 20000; ++i) {   // 80 MB of 4 KB objects past a 4 MB headroom
        auto next = detail::Maker<LimitBig>::make_tracked_data();   // raw storage: nothing written through the pointer
        auto raw = (uintptr_t)next.get();
        auto home = detail::Heap::page_of_checked((void*)raw);
        if (!(home && home->is_used && raw >= home->data && raw + sizeof(LimitBig) <= home->data + config::page_size)) {
            ++outside;
            next.release();   // the deleter would look the page of an address outside every page up
        }
    }
    collector::set_memory_limit(limit_before);
    EXPECT_EQ(outside, 0);
}
