//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <memory>
#include <vector>

// The collector one gate at a time (collector::stepper): the races of the
// engine as scenarios, the mutator's stores placed in the window between
// two phases of a cycle where a stress test would have to land by luck.
// The objects count their destructors, so that alive or dead is a fact of
// the sweep, not of a scan of this frame; what must not be found on the
// frame is made and dropped in off_frame and the frames below are zeroed.
namespace {
    using phase = collector::stepper::phase;

    struct Counted {
        static inline std::atomic<int> alive = 0;
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

TEST(Stepping_Tests, TheGatesOfACycleInOrder) {
    collector::stepper s;
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

TEST(Stepping_Tests, GarbageDiesAtTheSweepAndNotBefore) {
    collector::stepper s;
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
TEST(Stepping_Tests, ReleasedAfterTheFlipIntoAnOldObject) {
    collector::stepper s(false);                        // young cycles
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
TEST(Stepping_Tests, StoredIntoAnOldObjectAfterItsPageWasTraced) {
    collector::stepper s(false);
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
TEST(Stepping_Tests, LockBeforeAndAfterTheWeakPhase) {
    collector::stepper s;
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

// The cells of gc::tracked_ptrs in unmanaged memory: a block let go of by
// its allocator is freed by the cycle whose registration finds every cell
// of it given back, not by the cycle in flight when the last cell goes.
TEST(Stepping_Tests, ABlockOfCellsGoesWithTheNextRegistration) {
    collector::stepper s;
    settle(s);
    auto live0 = collector::get_statistics().live_objects;
    auto cells = std::make_unique<std::vector<gc::tracked_ptr<Counted>>>();
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
