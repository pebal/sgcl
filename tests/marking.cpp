//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>
#include <thread>

// A large live graph, cross-linked, with raw pointers into a large array
// (candidate words for the maps, never roots), must come out of cycles
// intact and counted once while another thread churns garbage through
// the collector.
namespace {
    struct Node {
        tracked_ptr<Node> next;
        tracked_ptr<Node> other;
        int* into_array = nullptr;   // raw pointer into a large array
        long id;
        long pad[2];
    };

    constexpr long Count = 400'000;

    SGCL_NOINLINE long check_graph(const tracked_ptr<Node>& head) {
        long seen = 0;
        for (auto n = head; n; n = n->next) {
            if (n->id != seen || !n->other || n->other->id != (seen * 7919) % Count || *n->into_array != int(seen % 1000)) {
                return -1;
            }
            ++seen;
        }
        return seen;
    }
}

// Waits, over a few forced cycles, for the live count to settle at `expected`
// (garbage of a thread that just exited may float for a cycle or two).
// Inlined into the caller: a frame of its own would sit where a dead frame
// holding pointers sat, and the counting functions zero only below the
// frame they are inlined into.
SGCL_ALWAYS_INLINE static size_t settled_live_count(size_t expected) {
    size_t count = 0;
    for (int i = 0; i < 8; ++i) {
        collector::force_collect(true);
        count = collector::get_live_object_count();
        if (count == expected) {
            break;
        }
    }
    return count;
}

// Everything that touches the graph lives in this frame; the test's own
// frame never sees a pointer, so its final count is exact.
SGCL_NOINLINE static void build_churn_and_check(size_t before) {
    tracked_ptr<Node> head;
    sgcl::vector<int> big(1'000'000);            // 4 MB: one large array
    off_frame([&] {
        sgcl::vector<tracked_ptr<Node>> nodes(Count);
        for (long i = 0; i < Count; ++i) {
            nodes[i] = make_tracked<Node>();
            nodes[i]->id = i;
            big[i % 1000] = int(i % 1000);
        }
        for (long i = 0; i < Count; ++i) {
            nodes[i]->next = i + 1 < Count ? nodes[i + 1] : nullptr;
            nodes[i]->other = nodes[(i * 7919) % Count];
            nodes[i]->into_array = &big[i % 1000];
        }
        head = nodes[0];
        for (int i = 0; i < 1000; ++i) {
            big[i] = i;
        }
    });
    sgcl::atomic<bool> stop = {false};
    std::thread churn([&] {                         // >= 32 MB of garbage per cycle
        while (!stop.load()) {
            for (int i = 0; i < 2000; ++i) {
                sgcl::vector<char> garbage(64 * 1024);
                garbage[100] = 1;
            }
        }
    });
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
        ASSERT_EQ(check_graph(head), Count);
    }
    stop.store(true);
    churn.join();
    // nothing lost, nothing counted twice: the graph, its array and the roots
    EXPECT_EQ(settled_live_count(before + Count + 1), before + Count + 1);
    EXPECT_EQ(check_graph(head), Count);
}

TEST(Marking_Tests, GraphIntactUnderAllocationChurn) {
    const size_t before = collector::get_live_object_count();
    build_churn_and_check(before);
    EXPECT_EQ(settled_live_count(before), before);   // the graph and the array are gone with their frame
}

// The same graph marked on the pool: needs the helpers on and a threshold
// the graph exceeds (-DSGCL_MARK_OBJECT_THRESHOLD=1024
// -DSGCL_HELPERS_GROWTH_THRESHOLD=0); with the defaults the policy decides
// and the pass is not guaranteed to run here.
TEST(Marking_Tests, ParallelMarkingKeepsTheGraph) {
    if (sgcl::config::HelpersGrowthThreshold != 0 || sgcl::config::MarkObjectThreshold > Count) {
        GTEST_SKIP() << "parallel marking not forced in this build";
    }
    const auto runs_before = sgcl::detail::collector_instance().parallel_mark_runs();
    const size_t before = collector::get_live_object_count();
    build_churn_and_check(before);
    EXPECT_EQ(settled_live_count(before), before);
    EXPECT_GT(sgcl::detail::collector_instance().parallel_mark_runs(), runs_before);
}
