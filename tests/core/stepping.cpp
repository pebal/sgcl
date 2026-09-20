//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

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

    class Stepping
    : public testing::TestWithParam<unsigned> {
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

// ---------------------------------------------------------------------------
// The barriers across the flip. A mutator's barrier reads the epoch
// (Page::reachable_state, Heap::globals.epoch_byte) and stores what it
// derived from it a few instructions later; a thread preempted between
// the read and the store, while the collector flips the epoch and runs its
// passes, would store a stale state or card, and an object reachable only
// through that store was swept (three interleavings, each measured on
// these gates before the region below). The slow path of every barrier
// is a region now (types.h: BarrierRegion): the thread's in_barrier word
// raised before the read of the epoch, a fence between, lowered after the
// last store; the collector, having flipped, waits for every raised word
// before it registers (collector.h: _wait_barriers). A test cannot
// preempt a thread at an instruction, so each scenario executes the
// barrier's own steps by hand, in the order the engine's code has them,
// around the stepper's gates: the region opened and the reads made before
// the flip, then the collector let go towards the registration from a
// second thread, which must not get there while the region is open; the
// stores as the engine's lines write them, with the values read before
// the flip; the region closed, the cycle finished, the object alive. The
// raw pointer of the object at stake is kept inverted (hide) meanwhile: in
// the real interleaving it sits in a register of the preempted thread,
// which the scan never reads.
namespace {
    // The region of this thread, opened and closed by hand as
    // BarrierRegion does it, so that the reads and the stores between can
    // be spread over the gates
    void open_region() {
        auto word = detail::current_thread_ptr->barrier_word;
        word->store(word->load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void close_region() {
        auto word = detail::current_thread_ptr->barrier_word;
        word->store(word->load(std::memory_order_relaxed) - 1, std::memory_order_release);
    }

    // The collector let through the flip towards the roots from another
    // thread; whether it got there within `wait`
    struct Stepper {
        std::thread thread;
        std::atomic<bool> passed = {false};
        void start(collector::stepper& s, collector::stepper::phase to) {
            thread = std::thread([&s, to, this] {
                s.advance_to(to);
                passed.store(true, std::memory_order_release);
            });
        }
        bool arrived(std::chrono::milliseconds wait) {
            auto until = std::chrono::steady_clock::now() + wait;
            while (!passed.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < until) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return passed.load(std::memory_order_acquire);
        }
        void join() {
            thread.join();
        }
    };

    // The release barrier up to its preemption point (pointer.h:
    // store_released; page.h: set_state_releasing, set_state_released):
    // u.release() nulls the unique_ptr, the object goes Releasing with
    // its allocation's parity, `wanted` is read. What is left is the
    // word, the card and the released state with the values read before
    // the flip: release_finish below.
    struct ReleasePrepared {
        uintptr_t hidden;
        detail::Page::Release release;
        detail::State wanted;
    };

    SGCL_NOINLINE ReleasePrepared release_prepare() {
        auto u = make_tracked<Counted>();               // UniqueLock | the parity of this epoch (page.h: unique_state)
        auto raw = u.release();                         // tracked_ptr.h: u nulled, the raw pointer in hand
        ReleasePrepared p;
        p.release = detail::Page::set_state_releasing(raw);   // Releasing | the allocation's parity
        p.wanted = detail::Page::reachable_state();     // the epoch before the flip
        p.hidden = hide(raw);
        return p;
    }

    SGCL_NOINLINE void release_finish(const ReleasePrepared& p, tracked_ptr<Counted>& holder) {
        auto raw = (Counted*)unhide(p.hidden);
        reinterpret_cast<detail::Pointer*>(&holder->next)->store_no_update(raw);   // pointer.h: the word
        detail::Page::mark_card(&holder->next);                                    // pointer.h: the card
        // page.h: set_state_released with the epoch read before the flip
        p.release.state->store((p.release.parity ^ p.wanted) & detail::State::Parity ? p.wanted : detail::State(p.wanted | detail::State::Fresh), std::memory_order_release);
        if (!p.release.page->state_updated.load(std::memory_order_relaxed)) {
            p.release.page->state_updated.store(true, std::memory_order_release);
        }
    }
}

// `holder->next = std::move(u)` with u made in epoch e-1, the thread
// preempted in Releasing with `wanted` read (the old parity), and the
// collector flipping to e: the registration finds the object Releasing
// and queues it as a root; the thread's later stores, the word and a
// state of the old parity, change nothing for a marked object. Young
// cycle, the holder old and marked.
TEST_P(Stepping, ReleasedAcrossTheFlipIntoAnOldObjectYoungCycle) {
    collector::stepper s(false);
    arm(s);
    tracked_ptr holder = make_tracked<Counted>();
    settle(s, true);                                    // holder old and marked; young cycles from here
    Counted::alive = 1;
    auto p = release_prepare();                         // epoch e-1, Releasing, up to the preemption point
    collector::clear_stack();                           // the dead frames of release_prepare
    EXPECT_EQ(s.advance_to(phase::flipped), phase::flipped);   // the flip: epoch e
    EXPECT_EQ(s.advance_to(phase::roots), phase::roots);       // registered (the object queued as a root), the stacks scanned, the dirty pages collected
    release_finish(p, holder);                          // the word, the card, the state of the old parity
    p.hidden = 0;
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2) << "the object released across the flip was swept: holder->next dangles";
    reinterpret_cast<detail::Pointer*>(&holder->next)->store(nullptr);   // a null store reads no target
    holder = nullptr;
    settle(s);
}

// The same in a full cycle, the holder made after the flip: unregistered,
// never traced by this cycle, so the object is reachable through nothing
// the cycle reads but its own state, Releasing at the registration.
TEST_P(Stepping, ReleasedAcrossTheFlipIntoAnUntracedObjectFullCycle) {
    collector::stepper s;
    arm(s);
    settle(s);
    Counted::alive = 0;
    auto p = release_prepare();                         // epoch e-1, Releasing, up to the preemption point
    collector::clear_stack();
    EXPECT_EQ(s.advance_to(phase::flipped), phase::flipped);   // the flip: epoch e
    tracked_ptr<Counted> holder = make_tracked<Counted>();     // made after the flip: unregistered, untraced this cycle
    EXPECT_EQ(s.advance_to(phase::roots), phase::roots);       // the object registered and queued, the stacks scanned
    release_finish(p, holder);
    p.hidden = 0;
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2) << "the object released across the flip was swept: holder->next dangles";
    reinterpret_cast<detail::Pointer*>(&holder->next)->store(nullptr);
    holder = nullptr;
    settle(s);
}

// The ordinary barrier across the flip: thread A copies `y->next = x`
// (pointer.h: store_no_update, then _update -> page.h:
// set_state<Reachable>): the word is stored, `wanted` (the old parity)
// and `old` are read, the region is open, and A is preempted. The
// collector flips (a full cycle) and waits. Once A's state store (the old
// parity, no root) is in and the region closed, the collector registers
// and scans; thread B then moves the pointer, `z->next = y->next; y->next
// = nullptr;`, through the real barrier, which finds the new epoch: x
// gets Reachable of the new parity, and z, made after the flip and never
// traced (the stand-in for an object already black), keeps it. Before the
// region A's store landed after B's and overwrote that state.
TEST_P(Stepping, CopiedAcrossTheFlipAndMovedByAnotherThread) {
    collector::stepper s;
    arm(s);
    tracked_ptr y = make_tracked<Counted>();
    off_frame([&] { y->next = make_tracked<Counted>(); });   // x, held by y only
    settle(s);                                          // y and x old, registered and marked
    Counted::alive = 2;
    // A: the word, then the reads of the barrier, in epoch e-1, the region open
    detail::Page* page;
    unsigned index;
    detail::State wanted, old;
    off_frame([&] {
        auto x = y->next.get();
        reinterpret_cast<detail::Pointer*>(&y->next)->store_no_update(x);   // pointer.h: the word (the same value: the copy)
        page = detail::Page::page_of(x);                // page.h: set_state<Reachable>
        index = page->index_of(x);
        wanted = detail::Page::reachable_state();       // the old parity
        old = page->states()[index].load(std::memory_order_relaxed);
        open_region();                                  // the slow path's region (old != wanted: x's state is of an earlier cycle)
    });
    collector::clear_stack();
    EXPECT_EQ(s.advance_to(phase::flipped), phase::flipped);     // the flip: epoch e, the marks to be cleared
    tracked_ptr<Counted> z = make_tracked<Counted>();            // made after the flip: unregistered, never traced this cycle
    Stepper towards_roots;
    towards_roots.start(s, phase::roots);
    EXPECT_FALSE(towards_roots.arrived(std::chrono::milliseconds(200))) << "the collector registered with a barrier open";
    // A resumes: the state with the value read before the flip, the flag, the card; the region closed
    off_frame([&] {
        auto& state = page->states()[index];
        if (old != wanted) {
            if (old != detail::State(wanted | detail::State::Fresh)) {
                state.store(wanted, std::memory_order_relaxed);   // Reachable of the old parity
            }
        }
        if (!page->state_updated.load(std::memory_order_relaxed)) {
            page->state_updated.store(true, std::memory_order_release);
        }
        detail::Page::mark_card(&y->next);              // pointer.h: _update's card
        close_region();
    });
    towards_roots.join();                               // registered; the stacks scanned: y and z roots (z ignored: unregistered)
    // B: the move through the real barrier, before the marking traces y
    off_frame([&] {
        z->next = y->next;                              // x: Reachable of the new parity (the region's read sees the flip)
        y->next = nullptr;
    });
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 3) << "x, moved into z by B, was swept: z->next dangles";
    reinterpret_cast<detail::Pointer*>(&z->next)->store(nullptr);
    z = nullptr;
    y = nullptr;
    settle(s);
}

// The card stamped late. x is made in cycle e, after the flip
// (Reachable|Fresh of e's parity, unregistered); y is old and marked. A
// stores `y->next = x` in cycle e: the word, the state (skipped: Fresh of
// the current parity; the flag raised), then the card (page.h: mark_card
// -> heap.h): the region open, the epoch byte e read, and A is preempted.
// Cycle e ends (x unregistered: kept). The young cycle e+1 flips and
// waits; A stamps e and closes the region; the collector registers x,
// scans, and the dirty pass takes y's page (a card of e or e+1): y is
// traced again and x marked. Before the region the stamp landed after the
// dirty pass, and the cycle after took the page for clean.
TEST_P(Stepping, CardStampedAfterTheDirtyPagesOfTheNextCycle) {
    collector::stepper s(false);
    arm(s);
    tracked_ptr y = make_tracked<Counted>();
    settle(s, true);                                    // y old and marked; young cycles from here
    Counted::alive = 1;
    EXPECT_EQ(s.advance_to(phase::registered), phase::registered);   // cycle e, past the flip and the wait for the barriers
    uint8_t epoch_byte;
    off_frame([&] {
        tracked_ptr<Counted> x = make_tracked<Counted>();      // after the flip: Reachable|Fresh|parity(e), unregistered
        reinterpret_cast<detail::Pointer*>(&y->next)->store_no_update(x.get());   // pointer.h: the word
        detail::Page::set_state<detail::State::Reachable>(x.get());              // pointer.h: _update -> page.h: skipped (Fresh of this parity), the flag
        open_region();                                                           // heap.h: mark_card's region
        epoch_byte = detail::Heap::globals.epoch_byte.load(std::memory_order_relaxed);   // the epoch read, then preempted
        x = nullptr;
    });
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();                                   // cycle e ends: x unregistered, kept
    EXPECT_EQ(s.advance_to(phase::flipped), phase::flipped);   // cycle e+1: the flip
    Stepper towards_roots;
    towards_roots.start(s, phase::roots);
    EXPECT_FALSE(towards_roots.arrived(std::chrono::milliseconds(200))) << "the collector registered with a barrier open";
    off_frame([&] {                                     // A resumes: heap.h: the card with the epoch read before the flip; the region closed
        auto& card = detail::Heap::card_of(&y->next);
        if (card.load(std::memory_order_relaxed) != epoch_byte) {
            card.store(epoch_byte, std::memory_order_release);
        }
        close_region();
    });
    towards_roots.join();                               // registered (x now), scanned, the dirty pages collected: y's among them
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, 2) << "x, stored into an old object with its card stamped late, was swept: y->next dangles";
    reinterpret_cast<detail::Pointer*>(&y->next)->store(nullptr);
    y = nullptr;
    settle(s);
}

// A state set after the flip and before the registration lowered the
// page's flag is of the current parity, right for its cycle, and was
// never retired: the retire after the sweep takes the pages the
// registration flagged and the other parity, and the next cycle's
// registration finds the page's flag down when nothing has been stored
// on it since. Two cycles later the parity comes round, the state reads
// as current in the pass over every page of a full cycle, and the object
// is a root once more: a dead object kept one full cycle too long. An
// object of its own type here, alone on its page, so that no other
// store raises the flag.
namespace {
    struct Quiet {
        static inline sgcl::atomic<int> alive = 0;
        Quiet() { ++alive; }
        ~Quiet() { --alive; }
        long value = 1;
    };
}

TEST_P(Stepping, AStateSetAfterTheFlipIsRetiredBeforeItsParityComesRound) {
    collector::stepper s;
    arm(s);
    Quiet::alive = 0;
    static root_ptr<Quiet> keep;                        // the root: a cell, not a word of this frame, which the scan would read
    off_frame([&] { keep = make_tracked<Quiet>(); });
    collector::clear_stack(SIZE_MAX);
    settle(s);                                          // the object old, registered, its state retired to Used
    EXPECT_EQ(s.advance_to(phase::flipped), phase::flipped);   // cycle e, parity p: after the flip, before the registration
    off_frame([&] { tracked_ptr<Quiet> y = keep; });    // the barrier: Reachable|p on the object, the page's flag raised
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();                                   // registered with the flag up
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();                                   // cycle e+1, parity ~p: the object held by the root, its state stale, the page quiet
    off_frame([&] { keep = nullptr; });                 // nothing holds it now
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();                                   // cycle e+2, parity p: its state reads as current unless it was retired
    EXPECT_EQ(Quiet::alive, 0) << "the object, dead since before the cycle, survived it on a state set two cycles ago";
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Quiet::alive, 0);
}

INSTANTIATE_TEST_SUITE_P(Helpers, Stepping, testing::Values(0u, 2u), [](const testing::TestParamInfo<unsigned>& info) {
    return info.param ? "TwoHelpers" : "Alone";
});

// A stepper made right after the previous one (collector.h: step_end,
// step_begin, _gate). Two things went wrong here once: step_end lowered
// _stepping and returned before the collector had woken and re-read the
// predicate of its gate, and step_begin, taking the mutex first, re-armed
// the gates under it (the collector stayed in the old gate, step_begin
// waited for a Start gate that never came); and with the collector
// asleep between the two, step_begin's wake, made without the collector's
// mutex, was lost between the predicate and the block, for the long sleep
// time. step_end waits for the collector to leave its gate now, and the
// wake takes the mutex. Nothing lies between the destructor and the
// constructor here; a watchdog ends the process after five seconds, since
// a collector stuck at a gate cannot be released: RUN THIS TEST ALONE
// (--gtest_filter=StepperReuse.*).
TEST(StepperReuse, ASecondStepperRightAfterTheFirstHangs) {
    sgcl::atomic<bool> made = false;
    std::thread watchdog([&] {                          // made before the steppers: nothing between the two below
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!made && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!made) {
            std::fprintf(stderr, "[  FAILED  ] StepperReuse.ASecondStepperRightAfterTheFirstHangs: the second stepper's constructor did not return within 5 s: the collector is stuck in the first stepper's last gate, step_begin waits for a Start gate that never comes\n");
            std::fflush(stderr);
            std::_Exit(1);
        }
    });
    {
        collector::stepper first;
        first.finish_cycle();                           // the collector at the Released gate
    }                                                   // step_end: notified, not yet awake
    collector::stepper second;                          // step_begin at once: hangs on the current engine
    made = true;
    watchdog.join();
    EXPECT_EQ(second.current(), phase::start);
}
