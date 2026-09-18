//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <thread>
#include <vector>

// A race of the engine that no gate of the stepper can place: it is a few
// instructions wide between two marking threads. A bounded stress run,
// its length from an environment variable (short by default), that fails
// when it observes the loss and reports what it saw either way.
namespace {
    using phase = collector::stepper::phase;

    unsigned env_or(const char* name, unsigned fallback) {
        auto e = std::getenv(name);
        return e ? (unsigned)std::atoi(e) : fallback;
    }
}

// Item 4 (collector.h: _spill ~1377, _mark_slot<true> ~694-709, _mark_page
// ~1262-1284). A marker that lists a page (the CAS of page->reachable from
// 0 to its id in _mark_slot) sets the page's reachable bits with plain
// stores, and _mark_page reads and clears the words plain, on the
// strength of "only the thread a page belongs to sets its bits". _spill
// hands the older half of the marker's listed pages to the pile with the
// marker's id still on them; the taker stores its own id at the start of
// its _mark_page. Between the listing thread's load of the id (still its
// own) and its `|=` of the bit, the taker can store its id, read the word
// and clear it: the bit is lost, and an object reachable only through it
// stays unmarked and is swept. The window is a few instructions on both
// sides. The heap: a random recursive tree of a million nodes over 256
// pages, every node with exactly one incoming edge (a lost bit loses a
// subtree), each node's `a` a child on the same page (the listing) and its
// `b` a child on a random page (the stack, the spills). The stepper
// forces eight helpers on every pass and drives full cycles for the given
// time; a node destroyed while the root holds the tree is the loss.
// SGCL_SPILL_STRESS_SECONDS (default 2), SGCL_SPILL_HELPERS (default 8;
// 0 is the control: the collector's thread alone, no pile, no spill),
// SGCL_SPILL_RUN (default 8: the nodes of a run, chained on one page; 1
// makes every edge a random one, so that a page is listed only by the
// chance of a child on the page being traced, and the pile holds items,
// hardly ever pages).
namespace {
    struct Node {
        static inline std::atomic<size_t> destroyed = 0;
        ~Node() { destroyed.fetch_add(1, std::memory_order_relaxed); }
        tracked_ptr<Node> a;   // the next node of this run: on this page
        tracked_ptr<Node> b;   // the head of another run: on a random page
    };
}

TEST(Races, SpilledPageBitsWithEightMarkers) {
    constexpr size_t N = size_t(1) << 20;
    const size_t R = std::max(1u, env_or("SGCL_SPILL_RUN", 8));   // nodes per run: consecutive slots, one page
    const size_t B = N / R;
    auto seconds = env_or("SGCL_SPILL_STRESS_SECONDS", 2);
    auto helpers = env_or("SGCL_SPILL_HELPERS", 8);
    collector::stepper s;                               // the collector parked: nothing dies while the tree is built
    s.helpers(helpers);
    s.full(true);
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    tracked_ptr<Node> root;
    off_frame([&] {
        auto nodes = std::make_unique<std::vector<Node*>>(N);   // raw pointers in unmanaged memory: the scan does not read them
        tracked_ptr<Node> head;
        for (size_t i = 0; i < N; ++i) {                // allocated in slot order, all reachable through `head` meanwhile
            auto n = make_tracked<Node>();
            (*nodes)[i] = n.get();
            n->a = head;
            head = std::move(n);
        }
        std::mt19937_64 rng(20260918);
        std::vector<size_t> blocks(B);
        for (size_t i = 0; i < B; ++i) {
            blocks[i] = i;
        }
        std::shuffle(blocks.begin(), blocks.end(), rng);
        // each run: a chain through `a`; its head hung on a free pointer
        // (a `b`, or the `a` of a run's last node) of a random node of a
        // run placed before it, each pointer used once: a tree
        std::vector<size_t> free_slots;                 // index * 2 + (0: a, 1: b)
        free_slots.reserve(N * 2);
        for (size_t k = 0; k < B; ++k) {
            auto first = blocks[k] * R;
            for (size_t i = first; i + 1 < first + R; ++i) {
                (*nodes)[i]->a = tracked_ptr<Node>((*nodes)[i + 1]);
            }
            (*nodes)[first + R - 1]->a = nullptr;
            if (k) {
                auto pick = std::uniform_int_distribution<size_t>(0, free_slots.size() - 1)(rng);
                auto slot = free_slots[pick];
                free_slots[pick] = free_slots.back();
                free_slots.pop_back();
                auto& word = slot & 1 ? (*nodes)[slot >> 1]->b : (*nodes)[slot >> 1]->a;
                word = tracked_ptr<Node>((*nodes)[first]);
            }
            for (size_t i = first; i < first + R; ++i) {
                free_slots.push_back(i * 2 + 1);
            }
            free_slots.push_back((first + R - 1) * 2);
        }
        root = tracked_ptr<Node>((*nodes)[blocks[0] * R]);
        head = nullptr;
    });
    collector::clear_stack(SIZE_MAX);
    Node::destroyed = 0;
    s.finish_cycle();                                   // the tree marked whole, the chain's garbage none
    ASSERT_EQ(Node::destroyed.load(), 0u);
    auto live0 = collector::get_statistics().live_objects;
    ASSERT_GE(live0, N);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    size_t cycles = 0;
    size_t lost_at = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        collector::clear_stack(SIZE_MAX);
        s.finish_cycle();
        ++cycles;
        if (Node::destroyed.load() || collector::get_statistics().live_objects < live0) {
            lost_at = cycles;
            break;
        }
    }
    auto destroyed = Node::destroyed.load();
    auto live = collector::get_statistics().live_objects;
    std::fprintf(stderr, "[spill] %zu full cycles with %u helpers over %zu nodes in runs of %zu (%u s at most): destroyed %zu, live %zu (was %zu)%s\n",
                 cycles, helpers, N, R, seconds, destroyed, live, live0, lost_at ? " LOST" : "");
    EXPECT_EQ(destroyed, 0u) << "a node of a tree held whole by its root was destroyed at cycle " << lost_at;
    EXPECT_GE(live, live0);
    root = nullptr;
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
}
