//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // A child that works for a while and counts itself off
    task<> child(int ms, sgcl::atomic<int>& finished) {
        co_await sgcl::sleep(std::chrono::milliseconds(ms));
        ++finished;
    }

    // A child that works until told to stop: counts whether it saw the
    // stop, and that it finished
    task<> obedient(sgcl::stop_token tok, sgcl::atomic<int>& stopped, sgcl::atomic<int>& finished) {
        co_await tok.stopped();   // a channel closed by the stop, or by the group's end
        if (tok.stop_requested()) {
            ++stopped;
        }
        ++finished;
    }

    task<> failing(int ms, sgcl::atomic<int>& finished) {
        co_await sgcl::sleep(std::chrono::milliseconds(ms));
        ++finished;
        throw std::runtime_error("failing");
    }

    task<int> number(int n) {
        co_await sgcl::yield();
        co_return n;
    }

    // A child holding a managed object in its frame across a yield
    task<> holder(int v, sgcl::atomic<long>& sum) {
        tracked_ptr node = make_tracked<Node>(v);
        co_await sgcl::yield();
        sum += node->value;
    }
}

TEST(TaskGroup_Tests, EightChildrenAllFinish) {
    sgcl::atomic<int> finished = {0};
    sgcl::task_group g;
    EXPECT_FALSE(g.stop_requested());
    EXPECT_TRUE(g.token().stop_possible());
    for (int i = 0; i < 8; ++i) {
        g.spawn(child(i, finished));
    }
    EXPECT_LE(g.count(), 8u);
    g.wait();                                  // from this thread
    EXPECT_EQ(finished.load(), 8);
    EXPECT_EQ(g.count(), 0u);
    EXPECT_FALSE(g.stop_requested());          // nobody threw
    g.wait();                                  // a second wait: nothing to wait for
    // a task of a value: its result dropped, the child counted
    g.spawn(number(5));
    g.wait();
    sgcl::scheduler::stop();
}

TEST(TaskGroup_Tests, OneThrowingStopsTheOthers) {
    sgcl::atomic<int> stopped = {0}, finished = {0};
    sgcl::task_group g;
    for (int i = 0; i < 7; ++i) {
        g.spawn(obedient(g.token(), stopped, finished));   // waiting for the stop: forever without it
    }
    g.spawn(failing(10, finished));
    EXPECT_THROW(g.wait(), std::runtime_error);            // the exception, once every child has finished
    EXPECT_EQ(finished.load(), 8);                         // every child finished before wait returned
    EXPECT_EQ(stopped.load(), 7);                          // the others saw the stop
    EXPECT_TRUE(g.stop_requested());
    EXPECT_TRUE(g.token().stop_requested());
    EXPECT_THROW(g.wait(), std::runtime_error);            // the same exception again, as errgroup.Wait gives its error again
    sgcl::scheduler::stop();
}

TEST(TaskGroup_Tests, TheFirstExceptionIsTheOneRethrown) {
    sgcl::atomic<int> finished = {0};
    sgcl::task_group g;
    g.spawn(failing(5, finished));
    g.spawn([](sgcl::stop_token tok, sgcl::atomic<int>& finished) -> task<> {
        co_await tok.stopped();
        ++finished;
        throw std::logic_error("second");                  // thrown because of the stop: not the one reported
    }(g.token(), finished));
    try {
        g.wait();
        FAIL() << "wait did not throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "failing");
    }
    EXPECT_EQ(finished.load(), 2);
    sgcl::scheduler::stop();
}

TEST(TaskGroup_Tests, ANestedGroupUnderAStoppedParent) {
    sgcl::stop_source parent;
    parent.request_stop();
    sgcl::task_group g(parent.token());                    // made under a token stopped already: stopped at once
    EXPECT_TRUE(g.stop_requested());
    sgcl::atomic<int> stopped = {0}, finished = {0};
    g.spawn(obedient(g.token(), stopped, finished));       // sees the stop and leaves
    g.wait();
    EXPECT_EQ(stopped.load(), 1);
    EXPECT_EQ(finished.load(), 1);
    // a group inside a child, under the child's token: stopped with its parent group
    sgcl::stop_source outer;
    sgcl::task_group parent_group(outer.token());
    sgcl::atomic<int> inner_stopped = {0}, inner_finished = {0};
    parent_group.spawn([](sgcl::stop_token tok, sgcl::atomic<int>& stopped, sgcl::atomic<int>& finished) -> task<> {
        sgcl::task_group inner(tok);
        for (int i = 0; i < 4; ++i) {
            inner.spawn(obedient(inner.token(), stopped, finished));
        }
        co_await inner.async_wait();                       // from a task: awaited
    }(parent_group.token(), inner_stopped, inner_finished));
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(inner_finished.load(), 0);                   // waiting for the stop
    outer.request_stop();                                  // the root stops: the group, the child's group, its children
    parent_group.wait();
    EXPECT_EQ(inner_stopped.load(), 4);
    EXPECT_EQ(inner_finished.load(), 4);
    sgcl::scheduler::stop();
}

TEST(TaskGroup_Tests, TheThreeWaysOfWaiting) {
    sgcl::atomic<int> finished = {0};
    // a thread blocks: wait(); a task co_awaits: async_wait(); a select takes it: on_done(f)
    sgcl::task_group g;
    for (int i = 0; i < 4; ++i) {
        g.spawn(child(5, finished));
    }
    auto t = sgcl::spawn([](sgcl::task_group& g) -> task<int> {
        co_await g.async_wait();
        co_return g.count() == 0 ? 1 : 0;
    }(g));
    EXPECT_EQ(t.join(), 1);
    EXPECT_EQ(finished.load(), 4);
    sgcl::task_group h;
    for (int i = 0; i < 4; ++i) {
        h.spawn(child(5, finished));
    }
    bool done = false;
    sgcl::channel<int> never;
    EXPECT_EQ(sgcl::select(never.on_receive([](int) {}), h.on_done([&] { done = true; })), 1u);
    EXPECT_TRUE(done);
    EXPECT_EQ(finished.load(), 8);
    h.wait();                                              // nothing to wait for; would rethrow
    // an exception awaited from a task
    sgcl::task_group f;
    f.spawn(failing(2, finished));
    auto u = sgcl::spawn([](sgcl::task_group& g) -> task<int> {
        try {
            co_await g.async_wait();
        } catch (const std::runtime_error&) {
            co_return 1;
        }
        co_return 0;
    }(f));
    EXPECT_EQ(u.join(), 1);
    sgcl::scheduler::stop();
}

TEST(TaskGroup_Tests, RequestStopByHand) {
    sgcl::atomic<int> stopped = {0}, finished = {0};
    sgcl::task_group g;
    for (int i = 0; i < 4; ++i) {
        g.spawn(obedient(g.token(), stopped, finished));
    }
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(finished.load(), 0);
    g.request_stop();
    g.wait();                                              // no exception: a stop is not an error
    EXPECT_EQ(stopped.load(), 4);
    EXPECT_EQ(finished.load(), 4);
    sgcl::scheduler::stop();
}

TEST(TaskGroup_Tests, AGroupThatEndsUnwaitedStopsItsChildren) {
    sgcl::atomic<int> stopped = {0}, finished = {0};
    sgcl::stop_token tok;
    {
        sgcl::task_group g;
        tok = g.token();
        for (int i = 0; i < 4; ++i) {
            g.spawn(obedient(g.token(), stopped, finished));
        }
    }                                                      // the scope ends with children running: stopped and let go of
    EXPECT_TRUE(tok.stop_requested());
    for (int i = 0; i < 5000 && finished.load() < 4; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(stopped.load(), 4);
    EXPECT_EQ(finished.load(), 4);
    sgcl::scheduler::stop();
}

TEST(TaskGroup_Tests, AThousandChildren) {
    settle();
    const int before = Node::alive.load();
    sgcl::atomic<long> sum = {0};
    off_frame([&] {
        sgcl::task_group g;
        for (int i = 0; i < 1000; ++i) {
            g.spawn(holder(i, sum));
            if (i % 100 == 0) {
                collector::force_collect(true);            // a cycle while the children run: their frames are roots
            }
        }
        collector::force_collect(true);
        g.wait();
    });
    EXPECT_EQ(sum.load(), 999L * 1000 / 2);
    sgcl::scheduler::stop();                               // the rings' slots let go of
    settle();
    EXPECT_EQ(Node::alive.load(), before);                 // the frames reclaimed, the nodes in them gone
}

namespace {
    sgcl::task_local<int> group_id;
}

TEST(TaskGroup_Tests, ChildrenInheritTheTaskLocalsAndTheExecutor) {
    sgcl::atomic<int> seen = {0};
    sgcl::atomic<bool> on_this_thread = {false};
    std::thread::id me = std::this_thread::get_id();
    sgcl::executor ex;
    ex.run([](sgcl::atomic<int>& seen, sgcl::atomic<bool>& on_this_thread, std::thread::id me) -> sgcl::task<> {
        co_await group_id.set(42);
        sgcl::task_group g;
        g.spawn([](sgcl::atomic<int>& seen, sgcl::atomic<bool>& on_this_thread, std::thread::id me) -> sgcl::task<> {
            co_await sgcl::sleep(1ms);                       // a wait: the child comes back where its parent runs
            seen = group_id.get().value_or(-1);
            on_this_thread = std::this_thread::get_id() == me;
        }(seen, on_this_thread, me));
        co_await g.async_wait();
    }(seen, on_this_thread, me));
    EXPECT_EQ(seen.load(), 42);
    EXPECT_TRUE(on_this_thread.load());
    sgcl::scheduler::stop();
}
