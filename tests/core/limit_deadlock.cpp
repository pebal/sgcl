//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A destructor that allocates past the memory ceiling while the sweep runs
// it: the allocation finds no page (page_allocator.h, object_allocator.h),
// calls collect_before_bad_alloc, which is force_collect(true), and waits
// for the end of a cycle that the waiting thread itself is in the middle
// of. The process never returns: run under a CTest timeout.
#include "sgcl/sgcl.h"

#include <cstdio>

using namespace sgcl;

template<class F>
SGCL_NOINLINE void off_frame(F&& f) {
    f();
}

struct AllocatesWhenDying {
    ~AllocatesWhenDying() {
        std::fprintf(stderr, "destructor on the sweep: allocating 4 MB at the ceiling\n");
        try {
            sgcl::vector<char> buffer(4u << 20);   // two chunks: past the ceiling
            buffer[0] = 1;
            std::fprintf(stderr, "destructor: allocated\n");
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "destructor: bad_alloc caught\n");
        }
    }
};

int main() {
    off_frame([] {
        tracked_ptr<AllocatesWhenDying> p = make_tracked<AllocatesWhenDying>();
    });
    collector::clear_stack(SIZE_MAX);
    // no chunk may be committed from here on: what is committed is what there is
    collector::set_memory_limit(collector::get_committed_memory());
    std::fprintf(stderr, "limit %zu bytes, forcing a cycle\n", collector::get_memory_limit());
    collector::force_collect(true);
    std::fprintf(stderr, "cycle done\n");
    return 0;
}
