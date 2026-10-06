//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

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

namespace {
    // A frame with a large local buffer written only at its low end (next to
    // the stack pointer): the stack probes of a big frame (___chkstk_darwin,
    // -fstack-clash-protection) only read the pages they cross, so those pages
    // stay out of the resident set, a hole between the caller's frames and the
    // deeper ones. The scan must look below it.
    SGCL_NOINLINE int hold_below_a_hole(const tracked_ptr<Holder>& holder) {
        char hole[96 * 1024];
        hole[0] = 1;
        asm volatile("" :: "r"(hole) : "memory");
        return hold_deep(0, holder);
    }
}

// A root in a frame below a hole of pages never written (a big buffer used at
// one end), on a thread of its own whose stack the collector has not seen yet.
// A scan that took the first unused page for the end of the stack (a growth-
// only query, measured unsound on macOS) freed the object under the frame.
TEST(Stack_Tests, RootBelowAnUnwrittenBufferSurvives) {
    for (int round = 0; round < 8; ++round) {
        tracked_ptr<Holder> holder = make_tracked<Holder>();
        holder->ptr = make_tracked<Payload>(9);
        int seen = 0;
        std::thread worker([&] { seen = hold_below_a_hole(holder); });
        worker.join();
        ASSERT_EQ(seen, 9) << "round " << round;
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
    auto first = (uintptr_t)keep.get() & ~(uintptr_t)(sgcl::config::page_size - 1);
    for (auto& w : words) {
        switch (rng() % 4) {
        case 0: w = first + rng() % sgcl::config::page_size; break;                    // somewhere in a live page
        case 1: w = first + sgcl::config::page_size - 1 - rng() % 16; break;          // the page's last bytes
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
        auto p = new tracked_ptr<Payload>();   // lint-handles: ok the death test of this very rule
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
// down its stack, together above config::stack_scan_threshold. The scan then
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
    constexpr int Threads = 20;   // 20 x ~300 KB used: above config::stack_scan_threshold (4 MB)
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

// A stack scanned from the stack pointer a thread recorded at its allocation
// (config.h: SGCL_STACK_SP): the frames above the record are scanned, and a
// root in a frame made before the cycles and kept through them, while the
// thread allocates below it, survives every one of them.
namespace {
    struct Garbage {
        char bytes[256];
    };

    SGCL_NOINLINE int allocate_below(int depth, sgcl::atomic<bool>& stop) {
        volatile char pad[512];
        sgcl::detail::os::escape((const void*)pad);
        pad[0] = (char)depth;
        if (depth > 0) {
            return allocate_below(depth - 1, stop) + (pad[0] == (char)depth ? 0 : 1);
        }
        int n = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            for (int i = 0; i < 1000; ++i) {
                tracked_ptr<Garbage> g = make_tracked<Garbage>();
                g->bytes[0] = (char)i;
            }
            ++n;
        }
        return n - n;
    }
}

TEST(Stack_Tests, RootAboveTheRecordedPointerSurvivesWhileTheThreadAllocates) {
    auto& c = sgcl::detail::collector_instance();
    const auto hits_before = c.stack_sp_hits();
    sgcl::atomic<bool> stop = {false};
    sgcl::atomic<int> phase = {0};
    int seen = 0;
    std::thread worker([&] {
        tracked_ptr<Payload> local = make_tracked<Payload>(77);   // made before the cycles, kept through them
        phase.store(1);
        int r = allocate_below(16, stop);
        seen = local->v + r;
    });
    while (phase.load() != 1) {
        std::this_thread::yield();
    }
    for (int i = 0; i < 40; ++i) {
        collector::force_collect(i % 2 == 0);
    }
    stop.store(true);
    worker.join();
    EXPECT_EQ(seen, 77);
    if constexpr(sgcl::config::stack_sp != 0) {
        EXPECT_GT(c.stack_sp_hits(), hits_before);   // the scan from the record was taken
    }
}

// A thread that records its stack pointer near the top of its stack, then
// goes deep, holds a root there and waits, allocating nothing, while the
// other threads run cycle after cycle. Its record is never current again,
// so every cycle queries its whole stack: a tag that comes round (four bits
// of the epoch: -DSGCL_STACK_SP_TAG_MASK=0xF) took the old record for the
// sixteenth cycle's, scanned only above the shallow pointer and freed the
// object under the deep frame.
namespace {
    struct Watched {
        int round;
        Watched(int r) : round(r) {}
        ~Watched() { gone[round].store(true); }
        inline static std::atomic<bool> gone[3] = {};
    };

    struct WatchedHolder {
        tracked_ptr<Watched> ptr;
    };

    SGCL_NOINLINE int wait_deep(int depth, const tracked_ptr<WatchedHolder>& holder, sgcl::atomic<int>& phase) {
        volatile char pad[256];
        sgcl::detail::os::escape((const void*)pad);
        pad[0] = (char)depth;
        if (depth == 0) {
            tracked_ptr<Watched> local = holder->ptr;   // no allocation: no new record
            holder->ptr = nullptr;
            phase.store(2);
            while (phase.load() != 3) {
                std::this_thread::yield();
            }
            return local->round;
        }
        return wait_deep(depth - 1, holder, phase) + (pad[0] == (char)depth ? 0 : 1000);
    }
}

TEST(Stack_Tests, IdleThreadDeepInItsStackKeepsItsRoots) {
    for (int round = 0; round < 3; ++round) {
        tracked_ptr<WatchedHolder> holder = make_tracked<WatchedHolder>();
        holder->ptr = make_tracked<Watched>(round);
        sgcl::atomic<int> phase = {0};
        int seen = -1;
        std::thread worker([&] {
            for (int i = 0; i < 100000; ++i) {   // pages taken here, at the top of the stack: a record for this cycle
                tracked_ptr<Garbage> g = make_tracked<Garbage>();
            }
            seen = wait_deep(200, holder, phase);
        });
        while (phase.load() != 2) {
            std::this_thread::yield();
        }
        for (int i = 0; i < 40; ++i) {       // more than sixteen cycles, the thread waiting
            collector::force_collect(true);
        }
        EXPECT_FALSE(Watched::gone[round].load()) << "round " << round;
        phase.store(3);
        worker.join();
        EXPECT_EQ(seen, round) << "round " << round;
    }
}

// Many threads alternating deep recursion, with a root in every few frames,
// and bursts of allocation at the bottom, while another thread forces cycles:
// every root is checked on the way back up.
namespace {
    struct Stamp {
        int v;
        Stamp(int x) : v(x) {}
        ~Stamp() { v = -1; }
    };

    SGCL_NOINLINE int descend(int depth, int id, unsigned& seed) {
        volatile char pad[128];
        sgcl::detail::os::escape((const void*)pad);
        pad[0] = (char)depth;
        tracked_ptr<Stamp> here;
        seed = seed * 1664525u + 1013904223u;
        if ((seed >> 28) < 4) {
            here = make_tracked<Stamp>(id * 100000 + depth);   // allocates at this depth: a record from here
        }
        int bad = 0;
        if (depth > 0) {
            bad += descend(depth - 1, id, seed);
        } else {
            for (int i = 0; i < 2000; ++i) {
                tracked_ptr<Garbage> g = make_tracked<Garbage>();
                g->bytes[1] = (char)i;
            }
        }
        if (here && here->v != id * 100000 + depth) {
            ++bad;
        }
        return bad + (pad[0] == (char)depth ? 0 : 1);
    }
}

TEST(Stack_Tests, DeepRecursionAndAllocationOnManyThreads) {
    sgcl::atomic<bool> stop = {false};
    std::thread collector_thread([&] {
        bool full = false;
        while (!stop.load()) {
            collector::force_collect(full = !full);
        }
    });
    constexpr int Threads = 12;
    std::vector<int> bad(Threads, 0);
    std::vector<std::thread> workers;
    for (int t = 0; t < Threads; ++t) {
        workers.emplace_back([&, t] {
            unsigned seed = 12345u + t;
            for (int round = 0; round < 150; ++round) {
                seed = seed * 1664525u + 1013904223u;
                bad[t] += descend(int(seed >> 24) % 300, t + 1, seed);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    stop.store(true);
    collector_thread.join();
    for (int t = 0; t < Threads; ++t) {
        EXPECT_EQ(bad[t], 0) << "thread " << t;
    }
}
