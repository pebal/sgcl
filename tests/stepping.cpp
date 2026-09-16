//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

// The collector one gate at a time (collector::stepper): the races of the
// engine as scenarios, the mutator's stores placed in the window between
// two phases of a cycle where a stress test would have to land by luck.
// The objects count their destructors, so that alive or dead is a fact of
// the sweep, not of a scan of this frame; what must not be found on the
// frame is made and dropped in off_frame and the frames below are zeroed.
// Every scenario runs twice: with the collector's thread alone, and with
// two helpers forced on every pass (the parallel marking, sweep, stack
// scan and states pass on a heap of a few objects).
namespace {
    using phase = collector::stepper::phase;

    class Stepping : public testing::TestWithParam<unsigned> {
    protected:
        void arm(collector::stepper& s) {
            s.helpers(GetParam());
        }
    };

    struct Counted {
        static inline sgcl::atomic<int> alive = 0;
        Counted() { ++alive; }
        ~Counted() { --alive; }
        int value = 7;
        tracked_ptr<Counted> next;
    };

    // Two full cycles with nothing of the test's in flight, so that the
    // objects of the previous test are gone (a young cycle would leave
    // them marked) and the marks of the survivors are set; the cycles
    // after are of the kind the stepper was made with
    void settle(collector::stepper& s, bool young = false) {
        s.full(true);
        collector::clear_stack(SIZE_MAX);
        s.finish_cycle();
        collector::clear_stack(SIZE_MAX);
        s.finish_cycle();
        s.full(!young);
    }
}

TEST_P(Stepping, TheGatesOfACycleInOrder) {
    collector::stepper s;
    arm(s);
    EXPECT_EQ(s.current(), phase::start);
    EXPECT_EQ(s.step(), phase::flipped);
    EXPECT_EQ(s.step(), phase::registered);
    EXPECT_EQ(s.step(), phase::roots);
    EXPECT_EQ(s.step(), phase::marked);
    EXPECT_EQ(s.step(), phase::swept);
    EXPECT_EQ(s.step(), phase::released);
    EXPECT_EQ(s.step(), phase::start);          // the next cycle, waiting
    EXPECT_EQ(s.advance_to(phase::swept), phase::swept);
    s.finish_cycle();
    EXPECT_EQ(s.current(), phase::released);
}

TEST_P(Stepping, GarbageDiesAtTheSweepAndNotBefore) {
    collector::stepper s;
    arm(s);
    settle(s);
    Counted::alive = 0;
    off_frame([] { tracked_ptr<Counted> t = make_tracked<Counted>(); });   // made and dropped: garbage from the start
    collector::clear_stack(SIZE_MAX);
    EXPECT_EQ(Counted::alive, 1);
    s.advance_to(phase::marked);
    EXPECT_EQ(Counted::alive, 1);                       // marking destroys nothing
    s.step();
    EXPECT_EQ(s.current(), phase::swept);
    EXPECT_EQ(Counted::alive, 0);                       // the sweep ran its destructor
    s.finish_cycle();
}

// An object made after the flip (unregistered this cycle) and released
// from its unique_ptr into a member of an old, marked object: not swept
// this cycle, since it is not registered; registered by the next, and
// traced through the card the store stamped on the old object's page.
// The marks are sticky through the young cycles: what died shows at the
// next full cycle.
TEST_P(Stepping, ReleasedAfterTheFlipIntoAnOldObject) {
    collector::stepper s(false);
    arm(s);                        // young cycles
    tracked_ptr holder = make_tracked<Counted>();
    settle(s, true);                                    // holder is old and marked
    Counted::alive = 1;
    EXPECT_EQ(s.advance_to(phase::flipped), phase::flipped);
    auto u = make_tracked<Counted>();                   // made after the flip: unregistered this cycle
    s.advance_to(phase::registered);
    off_frame([&] { holder->next = std::move(u); });    // released into an old object: the state, the card
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2);                       // not swept: made after the flip
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2);                       // registered and traced through the card
    off_frame([&] { holder->next = nullptr; });
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2);                       // dropped, but marked: a young cycle leaves it
    settle(s);
    EXPECT_EQ(Counted::alive, 1);                       // a full cycle: gone
}

// A young object stored into an old, already marked object after the
// dirty pages of this cycle were traced: the card stamped by the store
// has the next young cycle retrace the old object.
TEST_P(Stepping, StoredIntoAnOldObjectAfterItsPageWasTraced) {
    collector::stepper s(false);
    arm(s);
    tracked_ptr holder = make_tracked<Counted>();
    settle(s, true);
    Counted::alive = 1;
    s.advance_to(phase::roots);                         // the stacks scanned, the dirty pages traced
    off_frame([&] { holder->next = make_tracked<Counted>(); });   // the only reference: in the old object, after the trace
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2);
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2);                       // found through the card
    off_frame([&] { holder->next = nullptr; });
    settle(s);
    EXPECT_EQ(Counted::alive, 1);
}

// A weak pointer's target held by nothing else: lock() before the marking
// converged returns the object and holds it for the cycle (the state of
// the copy is a root the states pass sees), lock() after the weak phase
// returns null.
TEST_P(Stepping, LockBeforeAndAfterTheWeakPhase) {
    collector::stepper s;
    arm(s);
    weak_ptr<Counted> w;
    settle(s);
    Counted::alive = 0;
    off_frame([&] { w = tracked_ptr<Counted>(make_tracked<Counted>()); });
    collector::clear_stack(SIZE_MAX);
    EXPECT_EQ(Counted::alive, 1);
    s.advance_to(phase::roots);                         // the stacks scanned: the object is on none
    tracked_ptr<Counted> held;
    off_frame([&] { held = w.lock(); });                // locked before the weak phase: held from here on
    EXPECT_NE(held, nullptr);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 1);                       // held through the cycle
    EXPECT_FALSE(w.expired());
    held = nullptr;
    collector::clear_stack(SIZE_MAX);
    s.advance_to(phase::marked);                        // the weak phase of this cycle: the cell cleared
    EXPECT_EQ(w.lock(), nullptr);
    EXPECT_TRUE(w.expired());
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 0);
}

// The cells of root_ptrs in unmanaged memory: a block let go of by
// its allocator is freed by the cycle whose registration finds every cell
// of it given back, not by the cycle in flight when the last cell goes.
TEST_P(Stepping, ABlockOfCellsGoesWithTheNextRegistration) {
    collector::stepper s;
    arm(s);
    settle(s);
    auto live0 = collector::get_statistics().live_objects;
    auto cells = std::make_unique<std::vector<root_ptr<Counted>>>();
    off_frame([&] {
        cells->reserve(detail::CellBlock::Slots);
        for (unsigned i = 0; i < detail::CellBlock::Slots; ++i) {
            cells->emplace_back();                      // a full block, let go of by the allocator
        }
    });
    settle(s);
    EXPECT_EQ(collector::get_statistics().live_objects, live0 + 1);   // the block, a root by state
    s.advance_to(phase::registered);                    // the pass over the blocks is behind
    off_frame([&] { cells.reset(); });                  // every cell given back
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(collector::get_statistics().live_objects, live0 + 1);   // still there: the pass saw it in use
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(collector::get_statistics().live_objects, live0);       // freed by the cycle after
}

// A thread that exits between the registration of the cycle and the scan
// of the stacks: the scan skips it (is_deleted) and its objects, held by
// nothing else, die at the sweep of the same cycle. Before the exit the
// handshake of the thread with a scan in progress is the thread's wait
// for stack_scan to drop (thread.h).
TEST_P(Stepping, AThreadExitingBeforeTheScanFreesItsObjects) {
    collector::stepper s;
    arm(s);
    settle(s);
    Counted::alive = 0;
    sgcl::atomic<bool> go = false;
    sgcl::atomic<bool> ready = false;
    std::thread other([&] {
        tracked_ptr held = make_tracked<Counted>();     // on this thread's stack only
        ready = true;
        while (!go) {
            std::this_thread::yield();
        }
    });
    while (!ready) {
        std::this_thread::yield();
    }
    EXPECT_EQ(Counted::alive, 1);
    s.advance_to(phase::registered);                    // the thread registered for this cycle
    go = true;
    other.join();                                       // gone before the scan
    s.advance_to(phase::swept);
    EXPECT_EQ(Counted::alive, 0);                       // not on any stack the scan read
    s.finish_cycle();
}

// A thread that exits after the scan: its object, found on its stack,
// lives through the cycle and dies with the next full one.
TEST_P(Stepping, AThreadExitingAfterTheScanKeepsItsObjectsForTheCycle) {
    collector::stepper s;
    arm(s);
    settle(s);
    Counted::alive = 0;
    sgcl::atomic<bool> go = false;
    sgcl::atomic<bool> ready = false;
    std::thread other([&] {
        tracked_ptr held = make_tracked<Counted>();
        ready = true;
        while (!go) {
            std::this_thread::yield();
        }
    });
    while (!ready) {
        std::this_thread::yield();
    }
    s.advance_to(phase::roots);                         // the stacks scanned: the object is a root
    go = true;
    other.join();
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 1);                       // marked by the scan
    settle(s);
    EXPECT_EQ(Counted::alive, 0);                       // nothing holds it now
}

// An object loaded from an atomic after the stacks were scanned, its only
// other reference dropped at once: the copy made by the load is a root by
// its state (the barrier of the copy) although its frame was scanned
// before it existed, and the states pass of the marking finds it.
TEST_P(Stepping, LoadedAfterTheStackScanIsReachableByItsState) {
    struct Holder { atomic<tracked_ptr<Counted>> slot; };
    collector::stepper s;
    arm(s);
    tracked_ptr holder = make_tracked<Holder>();
    settle(s);
    Counted::alive = 0;
    off_frame([&] { holder->slot = make_tracked<Counted>(); });
    collector::clear_stack(SIZE_MAX);
    s.advance_to(phase::roots);                         // holder is a root; its word not traced yet
    tracked_ptr<Counted> loaded;
    off_frame([&] {
        loaded = holder->slot.load();                   // the copy: a store with the barrier into this frame
        holder->slot = nullptr;                         // the only other reference gone before the tracing
    });
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 1);                       // found by its state
    loaded = nullptr;
    settle(s);
    EXPECT_EQ(Counted::alive, 0);
}

// An object made before the flip (registered by this cycle) and released
// from its unique_ptr into an old, marked object after the dirty pages
// were traced: neither the scan nor the card of this cycle sees it; the
// state of the release, with the current parity and without Fresh, does.
TEST_P(Stepping, ReleasedAfterTheRootsIsReachableByItsState) {
    collector::stepper s(false);
    arm(s);
    tracked_ptr holder = make_tracked<Counted>();
    settle(s, true);
    Counted::alive = 1;
    auto u = make_tracked<Counted>();                   // before the flip: registered by this cycle, unmarked
    s.advance_to(phase::roots);                         // the stacks scanned (u's word is a unique_ptr: a root by its state, not a tracked word), the dirty pages traced
    off_frame([&] { holder->next = std::move(u); });    // the release: the state, and a card the next cycle reads
    collector::clear_stack(SIZE_MAX);
    s.advance_to(phase::swept);
    EXPECT_EQ(Counted::alive, 2);                       // not swept: reachable by its state
    s.finish_cycle();
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2);                       // and by the card since
    off_frame([&] { holder->next = nullptr; });
    settle(s);
    EXPECT_EQ(Counted::alive, 1);
}

// An object watched by an expiry_queue and reached by nothing else: the
// weak phase of the cycle finds it unreachable, keeps it (Expired, marked
// reachable) instead of clearing the cell, and drain() hands it to the
// function alive; a function that keeps it keeps it, otherwise it dies
// with the next cycle.
TEST_P(Stepping, AWatchedObjectIsKeptForTheDrain) {
    collector::stepper s;
    arm(s);
    expiry_queue<Counted> queue;
    settle(s);
    Counted::alive = 0;
    int expired = 0;
    tracked_ptr<Counted> revived;
    off_frame([&] {
        tracked_ptr object = make_tracked<Counted>();
        queue.watch(object, [&](tracked_ptr<Counted> t) { ++expired; revived = t; });   // kept by the function
    });
    collector::clear_stack(SIZE_MAX);
    s.advance_to(phase::marked);                        // the weak phase: unreachable, kept for the queue
    EXPECT_EQ(Counted::alive, 1);
    EXPECT_EQ(expired, 0);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 1);                       // not swept
    EXPECT_EQ(queue.drain(), 1u);                       // the function runs, the object alive
    EXPECT_EQ(expired, 1);
    EXPECT_NE(revived, nullptr);
    settle(s);
    EXPECT_EQ(Counted::alive, 1);                       // kept by `revived`
    revived = nullptr;
    settle(s);
    EXPECT_EQ(Counted::alive, 0);                       // an ordinary object now: gone
}

INSTANTIATE_TEST_SUITE_P(Helpers, Stepping, testing::Values(0u, 2u), [](const testing::TestParamInfo<unsigned>& info) {
    return info.param ? "TwoHelpers" : "Alone";
});
