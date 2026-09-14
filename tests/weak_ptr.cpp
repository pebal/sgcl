//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// weak_ptr: a pointer the collector clears instead of following.
#include "types.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace {
    struct Node {
        explicit Node(int v) : value(v) {}
        ~Node() { value = -1; }
        int value;
        tracked_ptr<Node> next;
        weak_ptr<Node> back;
    };
}

TEST(WeakGcTrackedPtr_Tests, LocksWhileTheObjectIsStronglyHeld) {
    tracked_ptr<Node> strong = make_tracked<Node>(7);
    weak_ptr<Node> weak = strong;
    EXPECT_FALSE(weak.expired());
    collector::force_collect(true);
    collector::force_collect(true);
    auto p = weak.lock();
    ASSERT_TRUE(p);
    EXPECT_EQ(p.get(), strong.get());
    EXPECT_EQ(p->value, 7);
}

TEST(WeakGcTrackedPtr_Tests, ClearedOnceTheObjectIsUnreachable) {
    weak_ptr<Node> weak;
    off_frame([&] {
        tracked_ptr<Node> strong = make_tracked<Node>(7);
        weak = strong;
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
}

TEST(WeakGcTrackedPtr_Tests, EmptyAndNullBehaveAsExpired) {
    weak_ptr<Node> empty;
    EXPECT_TRUE(empty.expired());
    EXPECT_FALSE(empty.lock());
    weak_ptr<Node> null = nullptr;
    EXPECT_TRUE(null.expired());
    tracked_ptr<Node> none;
    weak_ptr<Node> from_null = none;
    EXPECT_TRUE(from_null.expired());
    EXPECT_FALSE(from_null.lock());
}

TEST(WeakGcTrackedPtr_Tests, CopiesShareTheCellAndAssignmentReplacesIt) {
    tracked_ptr<Node> a = make_tracked<Node>(1);
    tracked_ptr<Node> b = make_tracked<Node>(2);
    weak_ptr<Node> w = a;
    weak_ptr<Node> copy = w;
    EXPECT_EQ(copy.lock().get(), a.get());
    w = b;                               // a new cell: the copy still sees a
    EXPECT_EQ(w.lock().get(), b.get());
    EXPECT_EQ(copy.lock().get(), a.get());
    w.reset();
    EXPECT_TRUE(w.expired());
    EXPECT_EQ(copy.lock().get(), a.get());
    weak_ptr<Node> moved = std::move(copy);
    EXPECT_EQ(moved.lock().get(), a.get());
    swap(moved, w);
    EXPECT_EQ(w.lock().get(), a.get());
    EXPECT_TRUE(moved.expired());
}

TEST(WeakGcTrackedPtr_Tests, ConvertsToABaseClass) {
    tracked_ptr<Baz> baz = make_tracked<Baz>();
    baz->value = 5;
    weak_ptr<Bar> base = baz;
    ASSERT_TRUE(base.lock());
    EXPECT_EQ(base.lock()->get_value(), 5);
    weak_ptr<Bar> from_weak = weak_ptr<Baz>(baz);
    EXPECT_EQ(from_weak.lock().get(), baz.get());
}

TEST(WeakGcTrackedPtr_Tests, InsideAManagedObject) {
    tracked_ptr<Node> head = make_tracked<Node>(1);
    head->next = make_tracked<Node>(2);
    head->next->back = head;             // a back pointer that keeps nothing
    tracked_ptr<Node> tail = head->next;
    collector::force_collect(true);
    off_frame([&] {   // the locked pointer is a temporary: off this frame
        EXPECT_EQ(tail->back.lock().get(), head.get());
    });
    head = nullptr;
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_TRUE(tail->back.expired());   // the head died, the tail lives
    EXPECT_EQ(tail->value, 2);
}

TEST(WeakGcTrackedPtr_Tests, WeakPointersDoNotKeepACycle) {
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        tracked_ptr<Node> a = make_tracked<Node>(1);
        tracked_ptr<Node> b = make_tracked<Node>(2);
        a->next = b;
        b->back = a;
        a->back = b;
    });
    EXPECT_EQ(collector::get_live_object_count(), before);   // nodes and cells
}

// A cell made during a cycle, from a strong pointer dropped in the same
// cycle: cleared all the same, never a pointer into a reused slot.
TEST(WeakGcTrackedPtr_Tests, ATargetThatDiesInTheCycleTheCellWasMadeIn) {
    for (int round = 0; round < 50; ++round) {
        weak_ptr<Node> weak;
        off_frame([&] {
            tracked_ptr<Node> strong = make_tracked<Node>(round);
            weak = strong;
            collector::force_collect(false);
        });
        collector::force_collect(true);
        collector::force_collect(true);
        EXPECT_TRUE(weak.expired());
        EXPECT_FALSE(weak.lock());
    }
}

// Threads lock a weak pointer while the owner drops and replaces its
// object: every lock that succeeds hands out a live object. The weak
// pointer is published under a mutex (rule 6: a tracked_ptr shared
// between threads needs synchronization); the race under test is between
// lock() and the collector clearing the cell.
TEST(WeakGcTrackedPtr_Tests, LockRacesWithTheClearing) {
    struct Shared {
        std::mutex mutex;
        weak_ptr<Node> weak;
        std::atomic<bool> stop = {false};
    };
    tracked_ptr<Shared> shared = make_tracked<Shared>();
    std::atomic<size_t> locked = {0};
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        // a raw pointer: this thread's tracked_ptr keeps the object, and a
        // lambda copied into a std::thread is no place for a tracked_ptr
        readers.emplace_back([&locked, shared = shared.get()] {
            while (!shared->stop.load(std::memory_order_relaxed)) {
                weak_ptr<Node> weak;
                {
                    std::lock_guard<std::mutex> guard(shared->mutex);
                    weak = shared->weak;
                }
                if (auto p = weak.lock()) {
                    EXPECT_GE(p->value, 0);   // never a destroyed object
                    ++locked;
                }
            }
        });
    }
    for (int round = 0; round < 200; ++round) {
        tracked_ptr<Node> strong = make_tracked<Node>(round);
        {
            std::lock_guard<std::mutex> guard(shared->mutex);
            shared->weak = strong;
        }
        strong = nullptr;
        collector::force_collect(round % 8 == 0);
    }
    shared->stop = true;
    for (auto& r : readers) {
        r.join();
    }
    EXPECT_GT(locked.load(), 0u);
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_TRUE(shared->weak.expired());
}
