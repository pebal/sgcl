//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of tasks and what runs them (DESIGN 408): an empty task
// and generator (made so, moved from, assigned to themselves), the
// composition of no tasks, a group with no children, an executor and a
// strand with nothing queued. Covered elsewhere: an empty blocking_task
// (Blocking_Tests.AHandleIsMadeOnlyBySpawnBlocking), when_any of nothing
// (When_Test.WhenAnyRethrowsTheWinners), a task dropped or assigned over
// (Unhandled_Tests), an unset task_local (TaskLocal_Tests.UnsetOutsideATask).
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Counted {
        Counted() noexcept {
            ++alive;
        }
        ~Counted() {
            --alive;
        }
        inline static std::atomic<int> alive = {0};
    };

    sgcl::async::task<int> holding(int v) {
        Counted c;
        co_return v;
    }

    sgcl::async::generator<int> two() {
        co_yield 1;
        co_yield 2;
    }
}

// An empty task is done and holds nothing: its destroy does nothing; a
// task moved from is empty, the one moved to runs; a move-assignment to
// itself keeps the task, and one over a task that never started
// destroys that task's frame
TEST(TaskBoundary_Tests, AnEmptyTaskAndOneMovedFromOrToItself) {
    sgcl::async::task<int> empty;
    EXPECT_TRUE(empty.done());
    empty.destroy();
    EXPECT_TRUE(empty.done());
    sgcl::async::task<> nothing;
    EXPECT_TRUE(nothing.done());
    nothing.destroy();

    int before = Counted::alive.load();
    sgcl::async::task<int> a = holding(1);
    sgcl::async::task<int> b = std::move(a);
    EXPECT_TRUE(a.done());                                    // moved from: empty
    EXPECT_FALSE(b.done());
    auto& same = b;
    b = std::move(same);                                      // to itself: kept
    EXPECT_FALSE(b.done());
    EXPECT_EQ(b.wait(), 1);
    a = std::move(empty);                                     // empty over empty
    EXPECT_TRUE(a.done());

    sgcl::async::task<int> unstarted = holding(2);
    {
        sgcl::async::task<int> started = holding(3);
        EXPECT_EQ(started.wait(), 3);
        unstarted = std::move(started);                       // the never-started one destroyed with its frame
    }
    EXPECT_TRUE(unstarted.done());
    EXPECT_EQ(unstarted.result(), 3);
    unstarted.destroy();
    EXPECT_TRUE(unstarted.done());
    EXPECT_EQ(Counted::alive.load(), before);
    sgcl::async::scheduler::stop();
}

// An empty generator, made so or moved from, is done and gives nothing
// at once; one moved to gives the values
TEST(TaskBoundary_Tests, AnEmptyGeneratorAndOneMovedFrom) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        sgcl::async::generator<int> empty;
        int sum = empty.done() ? 100 : 0;
        if (!(co_await empty.next())) {
            sum += 100;
        }
        sgcl::async::generator<int> g = two();
        sgcl::async::generator<int> moved = std::move(g);
        if (g.done() && !(co_await g.next())) {
            sum += 100;
        }
        while (auto v = co_await moved.next()) {
            sum += *v;
        }
        if (moved.done() && !(co_await moved.next())) {      // past the end: nothing again
            sum += 1000;
        }
        co_return sum;
    }());
    EXPECT_EQ(t.wait(), 1303);
    sgcl::async::scheduler::stop();
}

// when_all of no tasks gives nothing at once; of one, its result
TEST(TaskBoundary_Tests, WhenAllOfNothingAndOfOne) {
    sgcl::vector<sgcl::async::task<int>> none;
    EXPECT_TRUE(sgcl::async::when_all(std::move(none)).wait().empty());
    sgcl::vector<sgcl::async::task<>> no_voids;
    sgcl::async::when_all(std::move(no_voids)).wait();
    sgcl::vector<sgcl::async::task<int>> one;
    one.push_back(holding(4));
    auto r = sgcl::async::when_all(std::move(one)).wait();
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0], 4);
    sgcl::vector<sgcl::async::task<int>> single;
    single.push_back(holding(5));
    EXPECT_EQ(sgcl::async::when_any(std::move(single)).wait(), 0u);
    sgcl::async::scheduler::stop();
}

// A group with no children: nothing counted, its wait returns at once on
// a thread and in a task, a stop requested stops nothing and is seen
TEST(TaskBoundary_Tests, AGroupWithNoChildren) {
    {
        sgcl::async::task_group g;
        EXPECT_EQ(g.count(), 0u);
        EXPECT_FALSE(g.stop_requested());
        g.wait();
        g.wait();                                             // twice: nothing to wait for either time
        g.request_stop();
        g.request_stop();
        EXPECT_TRUE(g.stop_requested());
        EXPECT_TRUE(g.token().stop_requested());
        g.wait();
    }
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        sgcl::async::task_group g;
        co_await g;
        co_return (int)g.count() + 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::stop_source parent;
    parent.request_stop();
    sgcl::async::task_group under(parent.token());            // made under a stopped parent: stopped at once
    EXPECT_TRUE(under.stop_requested());
    under.wait();
    sgcl::async::scheduler::stop();
}

// An executor with nothing queued: a poll runs nothing and returns, a
// stop before the run makes the run return at once, and nothing runs it
// after; a strand with nothing is not busy
TEST(TaskBoundary_Tests, AnExecutorAndAStrandWithNothing) {
    sgcl::async::executor ex;
    EXPECT_FALSE(ex.running());
    EXPECT_EQ(ex.poll(), 0u);
    ex.stop();
    ex.stop();                                                // twice: one stop
    ex.run();                                                 // returns at once
    EXPECT_FALSE(ex.running());
    EXPECT_EQ(ex.poll(), 0u);
    sgcl::async::strand s;
    EXPECT_FALSE(s.busy());
    EXPECT_EQ(ex.run(holding(6)), 6);                         // a run after the stop that was spent
}
