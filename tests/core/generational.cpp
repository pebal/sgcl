//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>

// Young cycles keep the marks of the previous cycles (sticky mark bits):
// objects marked once are neither traced nor swept until a full cycle. The
// pointers stored into them meanwhile are found through the cards of their
// pages (detail/page.h: mark_card). force_collect() runs full cycles; the
// young ones are requested here through the collector itself. Without
// -DSGCL_GENERATIONAL=1 every requested cycle is full: the survival tests
// still hold, the two that need old garbage to wait are skipped.
namespace {
    struct Payload {
        int v;
        Payload(int x) : v(x) { ++alive; }
        ~Payload() { v = -1; --alive; }
        inline static sgcl::atomic<int> alive = {0};
    };

    struct Holder {
        int tag = 0;
        tracked_ptr<Payload> child;
        tracked_ptr<Holder> next;
    };

    // clear_stack() first: the words of the frames that built the objects
    // would otherwise count as roots (README, "Stack roots").
    SGCL_ALWAYS_INLINE void young_cycles() {
        collector::clear_stack();
        detail::collector_instance().collect_young(true);
    }

    SGCL_ALWAYS_INLINE void full_cycle() {
        collector::force_collect(true);
    }

    // The holder becomes old: marked by a full cycle.
    tracked_ptr<Holder> make_old_holder() {
        auto h = make_tracked<Holder>();
        full_cycle();
        return h;
    }
}

// A young object referenced only by an old one survives the young cycles:
// the store carded the old object's page, so the young cycle traces it.
TEST(Generational_Tests, YoungChildOfOldObjectSurvives) {
    Payload::alive = 0;
    auto h = make_old_holder();
    off_frame([&] {
        h->child = make_tracked<Payload>(11);
    });
    for (int i = 0; i < 3; ++i) {
        young_cycles();
        EXPECT_EQ(Payload::alive.load(), 1);
        off_frame([&] { EXPECT_EQ(h->child->v, 11); });
    }
    full_cycle();
    EXPECT_EQ(Payload::alive.load(), 1);
    off_frame([&] { EXPECT_EQ(h->child->v, 11); });
    h = nullptr;
    full_cycle();
    EXPECT_EQ(Payload::alive.load(), 0);
}

// A chain of young objects hanging off an old one: the first is found
// through the card, the rest by tracing the first (young objects are traced
// like in a full cycle).
TEST(Generational_Tests, YoungChainBehindOldObject) {
    Payload::alive = 0;
    auto h = make_old_holder();
    off_frame([&] {
        auto tail = h;
        for (int i = 0; i < 100; ++i) {
            tracked_ptr<Holder> n = make_tracked<Holder>();
            n->tag = i;
            n->child = make_tracked<Payload>(i);
            tail->next = n;
            tail = n;
        }
    });
    young_cycles();
    young_cycles();
    EXPECT_EQ(Payload::alive.load(), 100);
    int count = 0;
    off_frame([&] {
        for (auto n = h->next; n; n = n->next) {
            EXPECT_EQ(n->child->v, n->tag);
            ++count;
        }
    });
    EXPECT_EQ(count, 100);
    h = nullptr;
    full_cycle();
    EXPECT_EQ(Payload::alive.load(), 0);
}

// Young garbage is reclaimed by a young cycle.
TEST(Generational_Tests, YoungGarbageIsSweptByYoungCycle) {
    Payload::alive = 0;
    off_frame([] {
        for (int i = 0; i < 1000; ++i) {
            tracked_ptr<Payload> p = make_tracked<Payload>(i);
            tracked_ptr<Payload> copy = p;   // stored once: state Reachable
        }
    });
    EXPECT_EQ(Payload::alive.load(), 1000);
    young_cycles();
    EXPECT_EQ(Payload::alive.load(), 0);
}

// Garbage among the marked objects waits for a full cycle: a young cycle
// neither traces nor sweeps them.
TEST(Generational_Tests, OldGarbageWaitsForFullCycle) {
    if (!config::Generational) {
        GTEST_SKIP() << "built without SGCL_GENERATIONAL";
    }
    Payload::alive = 0;
    auto h = make_old_holder();
    off_frame([&] {
        h->child = make_tracked<Payload>(5);
    });
    full_cycle();   // the child is old now
    EXPECT_EQ(Payload::alive.load(), 1);
    h = nullptr;
    young_cycles();
    EXPECT_EQ(Payload::alive.load(), 1);
    full_cycle();
    EXPECT_EQ(Payload::alive.load(), 0);
}

// An object that survived a young cycle is marked, and stays marked: once
// replaced in its old holder it is garbage that only a full cycle sweeps.
// A replacement that never saw a young cycle goes at the next one.
TEST(Generational_Tests, ReplacedChildWaitsIfPromoted) {
    if (!config::Generational) {
        GTEST_SKIP() << "built without SGCL_GENERATIONAL";
    }
    Payload::alive = 0;
    auto h = make_old_holder();
    off_frame([&] {
        h->child = make_tracked<Payload>(1);
    });
    young_cycles();   // child 1 promoted
    EXPECT_EQ(Payload::alive.load(), 1);
    off_frame([&] {
        h->child = make_tracked<Payload>(2);
        h->child = make_tracked<Payload>(3);   // child 2 was never marked
    });
    young_cycles();
    EXPECT_EQ(Payload::alive.load(), 2);   // 1 (promoted garbage) and 3
    off_frame([&] { EXPECT_EQ(h->child->v, 3); });
    full_cycle();
    EXPECT_EQ(Payload::alive.load(), 1);
    off_frame([&] { EXPECT_EQ(h->child->v, 3); });
    h = nullptr;
    full_cycle();
    EXPECT_EQ(Payload::alive.load(), 0);
}

// The elements of an old vector are pointers stored into the pages of its
// storage: a young element stored later is found through the card, in a
// large array (many pages, page.h: page_bits) as well as in a small one.
TEST(Generational_Tests, YoungElementOfOldVectorSurvives) {
    Payload::alive = 0;
    for (size_t size : {size_t(8), size_t(100000)}) {
        off_frame([&] {
            sgcl::vector<tracked_ptr<Payload>> v(size);
            full_cycle();   // the storage is old
            off_frame([&] {
                v[size / 2] = make_tracked<Payload>(7);
                v[size - 1] = make_tracked<Payload>(8);
            });
            young_cycles();
            young_cycles();
            EXPECT_EQ(Payload::alive.load(), 2);
            off_frame([&] {
                EXPECT_EQ(v[size / 2]->v, 7);
                EXPECT_EQ(v[size - 1]->v, 8);
            });
        });
        full_cycle();
        EXPECT_EQ(Payload::alive.load(), 0);
    }
}

// The live object count reported after a young cycle is the count after the
// previous cycle plus the objects marked for the first time.
TEST(Generational_Tests, LiveCountAccumulatesOverYoungCycles) {
    Payload::alive = 0;
    auto h = make_old_holder();
    auto before = collector::get_live_object_count();
    off_frame([&] {
        auto tail = h;
        for (int i = 0; i < 10; ++i) {
            tracked_ptr<Holder> n = make_tracked<Holder>();
            tail->next = n;
            tail = n;
        }
    });
    young_cycles();
    EXPECT_EQ(collector::get_live_object_count(), before + 10);
    h = nullptr;
    full_cycle();
}

// Many threads storing young objects into a shared old structure while
// young cycles run: nothing reachable is lost. The stores come in rounds of
// bounded size (young cycles keep what they promote, so an unbounded churn
// would only grow the heap until a full cycle).
TEST(Generational_Tests, YoungStoresFromManyThreads) {
    Payload::alive = 0;
    constexpr int Threads = 8;
    constexpr int Slots = 64;
    constexpr int Rounds = 20;
    sgcl::vector<tracked_ptr<Holder>> table(Threads * Slots);
    for (int t = 0; t < Threads * Slots; ++t) {
        table[t] = make_tracked<Holder>();
    }
    full_cycle();   // table and holders are old
    sgcl::atomic<int> go = {0};
    sgcl::atomic<int> done = {0};
    std::vector<std::thread> workers;
    for (int t = 0; t < Threads; ++t) {
        workers.emplace_back([&, t] {
            for (int round = 1; round <= Rounds; ++round) {
                while (go.load(std::memory_order_acquire) < round) {
                    std::this_thread::yield();
                }
                for (int s = 0; s < Slots; ++s) {
                    auto h = table[t * Slots + s];
                    h->child = make_tracked<Payload>(round * Slots + s);
                    h->tag = round * Slots + s;
                }
                done.fetch_add(1, std::memory_order_release);
            }
        });
    }
    for (int round = 1; round <= Rounds; ++round) {
        go.store(round, std::memory_order_release);
        // the workers store while these cycles run
        detail::collector_instance().collect_young(true);
        while (done.load(std::memory_order_acquire) < round * Threads) {
            std::this_thread::yield();
        }
    }
    for (auto& w : workers) {
        w.join();
    }
    off_frame([&] {
        for (int t = 0; t < Threads * Slots; ++t) {
            ASSERT_NE(table[t]->child, nullptr);
            EXPECT_EQ(table[t]->child->v, table[t]->tag);
        }
    });
    table.clear();
    table.shrink_to_fit();
    collector::clear_stack(SIZE_MAX);
    full_cycle();
    EXPECT_EQ(Payload::alive.load(), 0);
}
