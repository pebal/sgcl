//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Coroutines whose frames live on the managed heap: tracked pointers in a
// suspended frame are roots.
#include "tests/types.h"

using namespace sgcl::async;

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
    sgcl::async::detail::cell_allocator.release();   // the block of the cells of the earlier tasks' root_ptrs (root_ptr.h) let go of, before the baseline
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    const size_t objects = collector::get_live_object_count();
    off_frame([&] {
        task<int> t = hold(1);
        t.resume();
        settle();
        EXPECT_EQ(Node::alive.load(), before + 1);
    });                                  // the task destroys the coroutine: its local is gone
    sgcl::async::detail::cell_allocator.release();   // the block of the task's root_ptr cell let go of (root_ptr.h): freed once the cell is back
    settle();
    EXPECT_EQ(Node::alive.load(), before);
    EXPECT_EQ(collector::get_live_object_count(), objects);   // the frame too
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

namespace {
    // A coroutine type of its own over a managed frame: suspended at the
    // start and at the end, a frame_ptr its only member
    struct own_coroutine {
        struct promise_type : managed_frame {
            own_coroutine get_return_object() {
                return own_coroutine{frame_ptr<promise_type>(std::coroutine_handle<promise_type>::from_promise(*this))};
            }
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() { throw; }
        };
        frame_ptr<promise_type> frame;
    };

    own_coroutine count_once(int& n) {
        ++n;
        co_return;
    }
}

// release() gives the handle back, as unique_ptr::release gives the
// pointer: the frame_ptr empty, the coroutine not destroyed
TEST(Coroutine_Tests, AFramePtrsReleaseGivesTheHandleBack) {
    int n = 0;
    auto c = count_once(n);
    auto h = c.frame.release();
    EXPECT_FALSE(c.frame);
    ASSERT_TRUE(h);
    EXPECT_EQ(n, 0);
    h.resume();
    EXPECT_EQ(n, 1);
    EXPECT_TRUE(h.done());
    h.destroy();                         // the coroutine the frame_ptr let go of: its owner's to destroy
}
