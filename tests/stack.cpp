//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>
#include <random>
#include <thread>

// Roots on the stack are found by scanning the used part of every registered
// thread's stack; these tests cover the cases the scan must get right.
namespace {
    struct Payload {
        int v;
        Payload(int x) : v(x) { ++alive; }
        ~Payload() { v = -1; --alive; }
        inline static sgcl::atomic<int> alive = {0};
    };

    struct Holder {
        tracked_ptr<Payload> ptr;
    };

    // A root in a frame `depth` calls deep, held across a full collection.
    SGCL_NOINLINE int hold_deep(int depth, const tracked_ptr<Holder>& holder) {
        if (depth == 0) {
            tracked_ptr<Payload> local = holder->ptr;
            holder->ptr = nullptr;
            for (int i = 0; i < 3; ++i) {
                collector::force_collect(true);
            }
            return local->v;
        }
        volatile char pad[256] = {};   // a frame of some size, like real ones
        (void)pad;
        return hold_deep(depth - 1, holder);
    }
}

// A thread that never allocates still holds roots: copying a pointer out of a
// shared object onto its stack registers the thread, so the object survives
// after the shared reference is gone.
TEST(Stack_Tests, RootInThreadThatNeverAllocates) {
    tracked_ptr<Holder> holder = make_tracked<Holder>();
    holder->ptr = make_tracked<Payload>(42);
    sgcl::atomic<int> phase = {0};
    int seen = 0;
    std::thread worker([&] {
        tracked_ptr<Payload> local = holder->ptr;   // the thread's first contact with the library
        phase.store(1);
        while (phase.load() != 2) {
            std::this_thread::yield();
        }
        seen = local->v;
        phase.store(3);
    });
    while (phase.load() != 1) {
        std::this_thread::yield();
    }
    holder->ptr = nullptr;
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Payload::alive.load(), 1);
    phase.store(2);
    worker.join();
    EXPECT_EQ(seen, 42);
}

// A local whose address never escapes: the optimizer would keep it in a
// register, where no scan can see it, if the constructor did not force it
// into memory. Fails at -O2 without that, so it must stay in every build.
TEST(Stack_Tests, RootThatNeverEscapes) {
    tracked_ptr<Payload> p = make_tracked<Payload>(21);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(p->v, 21);
    tracked_ptr<Holder> h = make_tracked<Holder>();
    h->ptr = make_tracked<Payload>(22);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(h->ptr->v, 22);
}

TEST(Stack_Tests, RootInDeepFrameSurvives) {
    tracked_ptr<Holder> holder = make_tracked<Holder>();
    holder->ptr = make_tracked<Payload>(7);
    EXPECT_EQ(hold_deep(200, holder), 7);
}

// A root that existed only in a frame that has returned is gone once the
// stack below the caller is cleared, which the counting functions do.
TEST(Stack_Tests, DeadFrameDoesNotRetain) {
    const size_t before = collector::get_live_object_count();
    off_frame([] {
        tracked_ptr<Payload> local = make_tracked<Payload>(1);
        Payload* raw = local.get();
        EXPECT_EQ(raw->v, 1);
    });
    EXPECT_EQ(collector::get_live_object_count(), before);
    EXPECT_EQ(Payload::alive.load(), 0);
}

// Words that look like pointers but are not roots of anything: addresses of
// free pages, of the reserved but unused range, of the middle of objects, of
// the last bytes of a page. None of them may crash the scan or mark the
// wrong thing.
TEST(Stack_Tests, ArbitraryWordsAreHarmless) {
    auto& heap = sgcl::detail::Heap::instance();
    std::mt19937_64 rng(11);
    tracked_ptr<Payload> keep = make_tracked<Payload>(5);
    volatile uintptr_t words[4096];
    auto first = (uintptr_t)keep.get() & ~(uintptr_t)(sgcl::config::PageSize - 1);
    for (auto& w : words) {
        switch (rng() % 4) {
        case 0: w = first + rng() % sgcl::config::PageSize; break;                    // somewhere in a live page
        case 1: w = first + sgcl::config::PageSize - 1 - rng() % 16; break;          // the page's last bytes
        case 2: w = first + (rng() % heap.reserved_bytes()); break;                  // anywhere in the reservation
        default: w = rng(); break;                                                   // noise
        }
    }
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(keep->v, 5);
    EXPECT_GE(Payload::alive.load(), 1);
}

// Threads that hold roots and exit while the collector scans: the exit
// handshake keeps the scan off a stack that is going away.
TEST(Stack_Tests, ThreadExitDuringScan) {
    sgcl::atomic<bool> stop = {false};
    std::thread collector_thread([&] {
        while (!stop.load()) {
            collector::force_collect(true);
        }
    });
    for (int round = 0; round < 200; ++round) {
        std::thread workers[8];
        for (auto& w : workers) {
            w = std::thread([] {
                tracked_ptr<Payload> local = make_tracked<Payload>(3);
                EXPECT_EQ(local->v, 3);
            });
        }
        for (auto& w : workers) {
            w.join();
        }
    }
    stop.store(true);
    collector_thread.join();
}

#ifndef NDEBUG
// The placement rule is checked in debug builds: a tracked_ptr on the
// unmanaged heap is a bug, not a slow path.
TEST(Stack_Tests, UnmanagedHeapIsRejectedInDebug) {
    EXPECT_DEATH({
        auto p = new tracked_ptr<Payload>();
        (void)p;
    }, "stack or inside a managed object");
}
#endif

// Objects created while a cycle is marking are not registered in that cycle;
// the objects they point to must survive that cycle (the barrier's state) and
// the next one (the young object is registered and traced then), whatever the
// timing of the page's object_created flag against the collector's rounds.
TEST(Stack_Tests, YoungObjectKeepsItsChildAcrossCycles) {
    struct Link {
        tracked_ptr<Payload> child;
    };
    sgcl::atomic<bool> stop = {false};
    std::thread collector_thread([&] {
        while (!stop.load()) {
            collector::force_collect(true);
        }
    });
    sgcl::vector<tracked_ptr<Link>> keep;
    for (int round = 0; round < 20000; ++round) {
        tracked_ptr<Payload> y = make_tracked<Payload>(round);
        tracked_ptr<Link> x = make_tracked<Link>();
        x->child = y;
        y = nullptr;                       // x.child is the only path to y
        keep.push_back(x);
        if (keep.size() == 64) {
            for (auto& k : keep) {
                ASSERT_EQ(k->child->v, int(&k - &keep[0]) + round - 63);
            }
            keep.clear();
        }
    }
    stop.store(true);
    collector_thread.join();
}

// Big stacks: threads deep in recursion, each holding roots several megabytes
// down its stack, together above config::StackScanThreshold. The scan then
// runs on the helper threads (reading only), and every root must still be
// found.
namespace {
    SGCL_NOINLINE int deep_roots(int depth, int id, sgcl::atomic<int>& ready, sgcl::atomic<bool>& release) {
        volatile char pad[4096];             // 4 KB per frame: 64 frames = 256 KB per thread (std::thread stacks are 512 KB on macOS)
        sgcl::detail::os::escape((const void*)pad);   // keeps the frames real: the optimizer turns such a recursion into a loop otherwise
        for (int i = 0; i < 4096; i += 64) {  // touch every line: the pages must really be used
            pad[i] = (char)depth;
        }
        if (depth == 0) {
            tracked_ptr<Payload> local = make_tracked<Payload>(id);
            ready.fetch_add(1);
            while (!release.load()) {
                std::this_thread::yield();
            }
            return local->v;
        }
        tracked_ptr<Payload> here = make_tracked<Payload>(id * 1000 + depth);
        int below = deep_roots(depth - 1, id, ready, release);
        return below + (here->v == id * 1000 + depth ? 0 : 1000000) + (int)pad[0] - depth;
    }
}

TEST(Stack_Tests, BigStacksAreScannedOnHelpers) {
    constexpr int Threads = 20;   // 20 x ~300 KB used: above config::StackScanThreshold (4 MB)
    constexpr int Depth = 64;
    const auto scans_before = sgcl::detail::collector_instance().parallel_stack_scans();
    sgcl::atomic<int> ready = {0};
    sgcl::atomic<bool> release = {false};
    std::vector<std::thread> workers;
    std::vector<int> results(Threads, -1);
    for (int t = 0; t < Threads; ++t) {
        workers.emplace_back([&, t] { results[t] = deep_roots(Depth, t + 1, ready, release); });
    }
    while (ready.load() < Threads) {
        std::this_thread::yield();
    }
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    EXPECT_GT(sgcl::detail::collector_instance().parallel_stack_scans(), scans_before);
    release.store(true);
    for (auto& w : workers) {
        w.join();
    }
    for (int t = 0; t < Threads; ++t) {
        EXPECT_EQ(results[t], t + 1) << "thread " << t;   // every root along the recursion was intact
    }
}
