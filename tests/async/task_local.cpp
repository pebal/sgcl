//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Task-local values: set by a task, read from any function it calls,
// inherited by the tasks it starts and not by its siblings, unset outside
// a task; a managed value kept alive by the chain.
#include "tests/types.h"

#include <chrono>
#include <string>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    sgcl::task_local<int> request_id;
    sgcl::task_local<std::string> user;
    sgcl::task_local<tracked_ptr<Node>> current_node;

    // A plain function the task calls: the value is the running task's
    int read_id() {
        return request_id.get_or(-1);
    }

    task<int> reader() {
        co_return read_id();
    }

    task<int> reader_after_wait() {
        co_await sgcl::sleep(1ms);
        co_return read_id();
    }

    task<int> child_sets_and_reads(int v, sgcl::channel<void>& done) {
        co_await request_id.set(v);
        int r = read_id();
        co_await done.async_send();
        co_return r;
    }

    task<> sends_id(sgcl::channel<int>& out) {
        co_await out.async_send(read_id());
    }

    // The parent sets 7, the first child sets 9, the parent and the second
    // child still see 7, a detached one too. The channels are the test's,
    // not the frame's: a channel outlives the tasks that use it
    task<> family(sgcl::channel<void>& done, sgcl::channel<int>& out, sgcl::atomic<int>& parent_before, sgcl::atomic<int>& child, sgcl::atomic<int>& parent_after, sgcl::atomic<int>& sibling, sgcl::atomic<int>& detached) {
        co_await request_id.set(7);
        parent_before = read_id();
        auto c = sgcl::spawn(child_sets_and_reads(9, done));
        co_await done.async_receive();   // the child has set its value
        child = co_await c;
        parent_after = read_id();
        sibling = co_await sgcl::spawn(reader());
        sgcl::go(sends_id(out));
        detached = *co_await out.async_receive();
    }

    task<int> with_value(int v) {
        int r = co_await request_id.with(v, reader());
        co_return r + (request_id.is_set() ? 1000 : 0);   // the value was the wrapper's, not this task's
    }

    task<int> node_value() {
        co_await current_node.set(make_tracked<Node>(41));
        co_await sgcl::sleep(1ms);
        collector::force_collect(true);   // the node is held by the chain, not by any local
        co_return current_node.get().value()->value;
    }
}

TEST(TaskLocal_Tests, UnsetOutsideATask) {
    EXPECT_FALSE(request_id.get());
    EXPECT_EQ(request_id.get_or(5), 5);
    EXPECT_FALSE(request_id.is_set());
    EXPECT_EQ(read_id(), -1);
    EXPECT_FALSE(user.get());
}

TEST(TaskLocal_Tests, SetAndReadFromAFunction) {
    auto t = [](sgcl::atomic<int>& before, sgcl::atomic<int>& after, sgcl::atomic<int>& later) -> task<> {
        before = read_id();
        co_await request_id.set(7);
        after = read_id();
        co_await request_id.set(8);   // set again: the newest
        co_await sgcl::sleep(1ms);
        later = read_id();
        co_await user.set("ann");
        EXPECT_EQ(*user.get(), "ann");
    };
    sgcl::atomic<int> before = {0}, after = {0}, later = {0};
    sgcl::spawn(t(before, after, later)).join();
    EXPECT_EQ(before.load(), -1);
    EXPECT_EQ(after.load(), 7);
    EXPECT_EQ(later.load(), 8);
    EXPECT_FALSE(request_id.is_set());   // this thread: outside a task
    sgcl::scheduler::stop();
}

TEST(TaskLocal_Tests, InheritedByChildrenNotBySiblings) {
    sgcl::atomic<int> parent_before = {0}, child = {0}, parent_after = {0}, sibling = {0}, detached = {0};
    sgcl::channel<void> done;
    sgcl::channel<int> out;
    sgcl::spawn(family(done, out, parent_before, child, parent_after, sibling, detached)).join();
    EXPECT_EQ(parent_before.load(), 7);
    EXPECT_EQ(child.load(), 9);
    EXPECT_EQ(parent_after.load(), 7);
    EXPECT_EQ(sibling.load(), 7);
    EXPECT_EQ(detached.load(), 7);
    sgcl::scheduler::stop();
}

TEST(TaskLocal_Tests, WithRunsATaskWithTheValue) {
    EXPECT_EQ(sgcl::spawn(with_value(3)).join(), 3);
    auto inherits_through_wait = []() -> task<int> {
        co_return co_await request_id.with(4, reader_after_wait());
    };
    EXPECT_EQ(sgcl::spawn(inherits_through_wait()).join(), 4);
    sgcl::scheduler::stop();
}

TEST(TaskLocal_Tests, AManagedValueLivesWithTheChain) {
    EXPECT_EQ(sgcl::spawn(node_value()).join(), 41);
    sgcl::scheduler::stop();
}

TEST(TaskLocal_Tests, OnAnExecutorAndByHand) {
    sgcl::executor ex;
    auto t = []() -> task<int> {
        co_await request_id.set(11);
        co_await sgcl::on_workers();
        int a = read_id();
        co_await sgcl::yield();
        co_return a * 100 + read_id();
    };
    EXPECT_EQ(ex.run(t()), 1111);
    auto by_hand = []() -> task<> {
        co_await request_id.set(12);
        EXPECT_EQ(read_id(), 12);
    };
    auto h = by_hand();
    h.resume();
    EXPECT_TRUE(h.done());
    EXPECT_EQ(read_id(), -1);
    sgcl::scheduler::stop();
}
