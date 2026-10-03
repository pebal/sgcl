//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the synchronization of tasks (DESIGN 408): every
// handle moved from and assigned to itself, an empty stop_token, the
// first and the last permit of a semaphore, a wait group at zero, a once
// and a condition variable nobody waits on, a promise whose value cannot
// be copied or moved in. Covered elsewhere: an empty guard (Sync_Test.
// AnEmptyGuardHasNoMutex), a closed semaphore and a negative add
// (Sync_Test.AWaitGroupsNegativeAddReleasesAndAClosedSemaphoreOpens), a
// fresh group as a case (Sync_Test.AFreshWaitGroupIsDoneAsACase), a once
// that throws (Sync_Test.AOnceThatThrowsIsDoneAndGivesItsException), the
// second setter of a promise (Promise_Tests.TheFirstSetterWins).
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    // A value whose copy or move throws when asked to
    struct Brittle {
        int v = 0;

        explicit Brittle(int v) noexcept
        : v(v) {
        }

        Brittle(const Brittle& o)
        : v(o.v) {
            if (fail_copy) {
                throw std::runtime_error("copy");
            }
        }

        Brittle(Brittle&& o)
        : v(o.v) {
            if (moves_to_fail > 0 && --moves_to_fail == 0) {
                throw std::runtime_error("move");
            }
        }

        inline static bool fail_copy = false;
        inline static int moves_to_fail = 0;   // the move that throws, counted from 1; 0: none
    };

    template<class H>
    void moved_and_self_assigned(H& a) {
        H b = std::move(a);
        EXPECT_TRUE(a == b);                                  // a move of the word is a copy: the same object
        H c = b;
        c = std::move(a);
        EXPECT_TRUE(c == b);
        auto& same = c;
        c = same;
        c = std::move(same);
        EXPECT_TRUE(c == b);
    }
}

// Every handle moved from stands for its object still, and an assignment
// to itself changes nothing: each works through the old name
TEST(SyncBoundary_Tests, HandlesMovedFromAndAssignedToThemselves) {
    sgcl::async::event e;
    moved_and_self_assigned(e);
    e.set();
    EXPECT_TRUE(e.is_set());

    sgcl::async::mutex m;
    moved_and_self_assigned(m);
    EXPECT_TRUE(m.try_lock());
    EXPECT_FALSE(m.try_lock());
    m.unlock();

    sgcl::async::wait_group g;
    moved_and_self_assigned(g);
    g.add(2);
    EXPECT_EQ(g.count(), 2);
    g.add(-2);

    sgcl::async::promise<int> p;
    moved_and_self_assigned(p);
    p.set_value(3);
    EXPECT_EQ(p.result(), 3);

    sgcl::async::promise<> q;
    moved_and_self_assigned(q);
    q.set_value();
    EXPECT_TRUE(q.done());

    sgcl::async::stop_source src;
    sgcl::async::stop_source moved = std::move(src);
    sgcl::async::stop_token t = src.token();
    EXPECT_TRUE(t == moved.token());
    moved_and_self_assigned(t);
    src.request_stop();
    EXPECT_TRUE(moved.stop_requested());
    EXPECT_TRUE(t.stop_requested());
}

// A token with no source: never stopped, equal to every other empty one
// and to none with a source; a source made under it is a source of its
// own; a deadline of it is no deadline (Timeout_Tests.ATokenAsTheDeadline)
TEST(SyncBoundary_Tests, AnEmptyStopToken) {
    sgcl::async::stop_token none, other;
    EXPECT_TRUE(none == other);
    sgcl::async::stop_token moved = std::move(none);
    EXPECT_TRUE(moved == other);
    EXPECT_FALSE(moved.stop_possible());
    sgcl::async::stop_source root(none);
    EXPECT_FALSE(root.token() == none);
    EXPECT_TRUE(root.token().stop_possible());
    EXPECT_FALSE(root.stop_requested());
    root.request_stop();
    root.request_stop();                                      // a second stop does nothing
    EXPECT_TRUE(root.stop_requested());
    EXPECT_FALSE(none.stop_requested());
}

// A semaphore made with more permits than its maximum keeps the maximum;
// the last permit taken, try_acquire fails, and a release gives it back
TEST(SyncBoundary_Tests, TheFirstAndTheLastPermit) {
    sgcl::async::semaphore over(5, 2);
    EXPECT_EQ(over.available(), 2u);
    EXPECT_TRUE(over.try_acquire());
    EXPECT_TRUE(over.try_acquire());
    EXPECT_FALSE(over.try_acquire());
    EXPECT_EQ(over.available(), 0u);
    over.release();
    over.release();
    over.release();                                           // past the maximum: lost
    EXPECT_EQ(over.available(), 2u);

    sgcl::async::semaphore one(1);
    one.acquire().wait();
    EXPECT_FALSE(one.try_acquire());
    std::thread releaser([&] {
        std::this_thread::sleep_for(10ms);
        one.release();
    });
    one.acquire().wait();                                     // waits for the release
    releaser.join();
    EXPECT_EQ(one.available(), 0u);
}

// A wait group at zero: its wait returns at once, on a thread and in a
// task; an add of zero changes nothing
TEST(SyncBoundary_Tests, AWaitGroupAtZero) {
    sgcl::async::wait_group g;
    g.wait();
    g.add(0);
    EXPECT_EQ(g.count(), 0);
    g.wait();
    auto t = sgcl::async::spawn([](sgcl::async::wait_group g) -> sgcl::async::task<int> {
        co_await g;
        co_return 1;
    }(g));
    EXPECT_EQ(t.wait(), 1);
    g.add();
    g.done();
    g.wait();                                                 // a round over: zero again
    sgcl::async::scheduler::stop();
}

// A once never called is not called(); a once called by a thread and
// then by a task runs the function once
TEST(SyncBoundary_Tests, AOnceFreshAndCalledFromBothSides) {
    sgcl::async::once o;
    EXPECT_FALSE(o.called());
    int runs = 0;
    o.call([&] { ++runs; }).wait();
    auto t = sgcl::async::spawn([](sgcl::async::once& o, int& runs) -> sgcl::async::task<> {
        co_await o.call([&] { ++runs; });
    }(o, runs));
    t.wait();
    EXPECT_EQ(runs, 1);
    EXPECT_TRUE(o.called());
    sgcl::async::scheduler::stop();
}

// A notify with nobody waiting is lost, and a wait whose predicate holds
// does not wait at all
TEST(SyncBoundary_Tests, AConditionVariableNobodyWaitsOn) {
    sgcl::async::condition_variable cv;
    cv.notify_one();
    cv.notify_all();
    sgcl::async::mutex m;
    auto g = m.scoped_lock().wait();
    cv.wait(g, [] { return true; }).wait();
    ASSERT_TRUE(g.owner());                                   // the mutex still held by the guard
    EXPECT_FALSE(m.try_lock());
}

// An event set twice is set once; a wait and a case after the set are
// served at once
TEST(SyncBoundary_Tests, AnEventSetTwice) {
    sgcl::async::event e;
    EXPECT_FALSE(e.is_set());
    e.set();
    e.set();
    EXPECT_TRUE(e.is_set());
    e.wait();
    bool served = false;
    EXPECT_EQ(sgcl::async::select(e.on_set([&] { served = true; })).wait(), 0u);
    EXPECT_TRUE(served);
}

// A value whose copy into the promise throws leaves the promise unset,
// the exception the setter's; one whose move into the promise throws sets
// the promise with that exception, so that no waiter waits for good
TEST(SyncBoundary_Tests, APromiseWhoseValueCannotBeCopiedOrMovedIn) {
    sgcl::async::promise<Brittle> p;
    Brittle b(1);
    Brittle::fail_copy = true;
    EXPECT_THROW(p.set_value(b), std::runtime_error);
    Brittle::fail_copy = false;
    EXPECT_FALSE(p.done());
    p.set_value(Brittle(2));
    EXPECT_EQ(p.wait().v, 2);

    sgcl::async::promise<Brittle> q;
    std::thread waiter([&] {
        EXPECT_THROW(q.wait(), std::runtime_error);
    });
    std::this_thread::sleep_for(10ms);
    Brittle::moves_to_fail = 2;                               // the argument's move into the call, then the move into the state
    q.set_value(Brittle(3));                                  // the move into the state throws: the waiters get it
    EXPECT_EQ(Brittle::moves_to_fail, 0);
    waiter.join();
    EXPECT_TRUE(q.done());
    EXPECT_THROW(q.result(), std::runtime_error);
}
