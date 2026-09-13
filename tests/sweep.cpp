//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>

// Enough garbage for the sweep to run on the helper threads
// (config::SweepPageThreshold pages): every object is destroyed exactly once,
// a destructor sees its pointer to a peer dying in the same sweep as null and
// its pointer to a survivor intact, whichever thread sweeps which page.
namespace {
    struct Survivor {
        int v = 7;
    };

    // The destructor reads its tracked_ptr members through if_alive() only:
    // the peer dies in the same sweep, on any thread, so it reads as null;
    // the survivor is live and readable.
    struct Peer {
        tracked_ptr<Peer> peer;
        tracked_ptr<Survivor> survivor;
        char pad[40];
        inline static std::atomic<long> destroyed = {0};
        inline static std::atomic<long> peer_not_null = {0};
        inline static std::atomic<long> survivor_bad = {0};
        ~Peer() {
            destroyed.fetch_add(1, std::memory_order_relaxed);
            if (peer.if_alive()) {
                peer_not_null.fetch_add(1, std::memory_order_relaxed);
            }
            auto s = survivor.if_alive();
            if (!s || s->v != 7) {
                survivor_bad.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };
}

TEST(Sweep_Tests, ParallelSweepDestroysEachObjectOnce) {
    constexpr long Count = 2'000'000;   // 64-byte objects: ~2000 pages
    Peer::destroyed = Peer::peer_not_null = Peer::survivor_bad = 0;
    tracked_ptr<Survivor> survivor = make_tracked<Survivor>();
    off_frame([&] {
        sgcl::vector<tracked_ptr<Peer>> all;
        all.reserve(Count);
        for (long i = 0; i < Count; ++i) {
            all.push_back(make_tracked<Peer>());
            all.back()->survivor = survivor;
        }
        for (long i = 0; i < Count; ++i) {
            all[i]->peer = all[(i * 7919 + 13) % Count];   // peers across pages
        }
    });
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Peer::destroyed.load(), Count);
    EXPECT_EQ(Peer::peer_not_null.load(), 0);
    EXPECT_EQ(Peer::survivor_bad.load(), 0);
    EXPECT_EQ(survivor->v, 7);
}

// The same with a destructor that allocates and copies pointers on the
// sweeping thread: helper threads register themselves and the allocations
// they make are ordinary managed objects.
namespace {
    struct Allocating {
        tracked_ptr<Survivor> made;
        inline static std::atomic<long> destroyed = {0};
        ~Allocating() {
            tracked_ptr<Survivor> local = make_tracked<Survivor>();
            made = local;
            destroyed.fetch_add(local->v == 7, std::memory_order_relaxed);
        }
    };
}

TEST(Sweep_Tests, DestructorsMayAllocateOnHelperThreads) {
    constexpr long Count = 1'000'000;
    Allocating::destroyed = 0;
    off_frame([] {
        sgcl::vector<tracked_ptr<Allocating>> all(Count);
        for (auto& a : all) {
            a = make_tracked<Allocating>();
        }
    });
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Allocating::destroyed.load(), Count);
}
