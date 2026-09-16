//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Coroutines whose frames live on the managed heap: tracked pointers in a
// suspended frame are roots.
#include "types.h"

#include <coroutine>
#include <stdexcept>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        tracked_ptr<Node> next;
        inline static sgcl::atomic<int> alive = {0};
    };

    // A node made in the coroutine and held across a suspension
    task<int> hold(int v) {
        tracked_ptr<Node> local = make_tracked<Node>(v);
        co_await std::suspend_always{};
        co_return local->value;
    }

    // A node passed in: the parameter copy lives in the frame
    task<int> hold_parameter(tracked_ptr<Node> node) {
        co_await std::suspend_always{};
        co_return node->value;
    }

    // Yields nodes, keeping a chain of the yielded ones in a local
    generator<tracked_ptr<Node>> nodes(int count) {
        tracked_ptr<Node> chain;
        for (int i = 0; i < count; ++i) {
            tracked_ptr<Node> n = make_tracked<Node>(i);
            n->next = chain;
            chain = n;
            co_yield n;
        }
    }

    task<int> throws() {
        tracked_ptr<Node> local = make_tracked<Node>(1);
        co_await std::suspend_always{};
        throw std::runtime_error("from the coroutine");
    }

    task<void> nothing() {
        co_return;
    }

    SGCL_NOINLINE task<int> make_resumed(int v) {
        task<int> t = hold(v);
        t.resume();
        return t;                        // moved out: the frame goes with the task
    }

    void settle() {
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Coroutine_Tests, ALocalOfASuspendedFrameIsARoot) {
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    task<int> t = hold(7);
    t.resume();                          // to the suspension: the node exists only in the frame
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    t.resume();
    EXPECT_TRUE(t.done());
    EXPECT_EQ(t.result(), 7);
    t.destroy();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Coroutine_Tests, AParameterOfASuspendedFrameIsARoot) {
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    task<int> t;
    off_frame([&] {
        tracked_ptr<Node> node = make_tracked<Node>(42);
        t = hold_parameter(node);
        t.resume();
    });
    settle();                            // the caller's copy is gone: the frame's copy keeps the node
    EXPECT_EQ(Node::alive.load(), before + 1);
    t.resume();
    EXPECT_EQ(t.result(), 42);
}

TEST(Coroutine_Tests, TheFrameGoesWithTheTask) {
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    const size_t objects = collector::get_live_object_count();
    off_frame([&] {
        task<int> t = hold(1);
        t.resume();
        settle();
        EXPECT_EQ(Node::alive.load(), before + 1);
    });                                  // the task destroys the coroutine: its local is gone
    settle();
    EXPECT_EQ(Node::alive.load(), before);
    EXPECT_EQ(collector::get_live_object_count(), objects);   // the frame too
}

TEST(Coroutine_Tests, AGeneratorHoldsItsChainAcrossYields) {
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    int seen = 0;
    off_frame([&] {
        for (auto& n : nodes(5)) {
            collector::force_collect(true);
            EXPECT_EQ(n->value, seen);
            ++seen;
            // the chain of the earlier nodes hangs from the local in the frame
            int length = 0;
            for (auto p = n; p; p = p->next) {
                ++length;
            }
            EXPECT_EQ(length, seen);
        }
    });
    EXPECT_EQ(seen, 5);
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Coroutine_Tests, AFrameOutlivesTheFunctionThatMadeIt) {
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    task<int> t = make_resumed(9);
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    t.resume();
    EXPECT_EQ(t.result(), 9);
}

TEST(Coroutine_Tests, AnExceptionComesOutOfResult) {
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    {
        task<int> t = throws();
        t.resume();
        settle();
        EXPECT_EQ(Node::alive.load(), before + 1);
        t.resume();
        EXPECT_TRUE(t.done());
        EXPECT_THROW(t.result(), std::runtime_error);
    }
    settle();
    EXPECT_EQ(Node::alive.load(), before);
    task<void> n = nothing();
    n.resume();
    EXPECT_TRUE(n.done());
    EXPECT_NO_THROW(n.result());
}

TEST(Coroutine_Tests, ManySuspendedFramesWithCycles) {
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    sgcl::vector<task<int>> tasks;        // tasks hold tracked pointers: a managed container
    off_frame([&] {
        for (int i = 0; i < 1000; ++i) {
            tasks.push_back(hold(i));
            tasks.back().resume();
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1000);
    tasks.resize(500);                   // half destroyed
    settle();
    EXPECT_EQ(Node::alive.load(), before + 500);
    int sum = 0;
    for (auto& t : tasks) {
        t.resume();
        sum += t.result();
    }
    EXPECT_EQ(sum, 499 * 500 / 2);
    tasks.clear();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

// A frame in a managed object: a coroutine owned by another object
TEST(Coroutine_Tests, AFrameInsideAManagedObject) {
    settle();                            // the garbage of the previous test, before the baseline
    struct Owner {
        task<int> t;
    };
    const int before = Node::alive.load();
    tracked_ptr<Owner> owner = make_tracked<Owner>();
    off_frame([&] {
        owner->t = hold(3);
        owner->t.resume();
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    owner->t.resume();
    EXPECT_EQ(owner->t.result(), 3);
    owner = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}
