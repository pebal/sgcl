//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// thread: std::thread's interface, the callable and the arguments in a
// managed node of their own, followed by the collector.
#include "tests/types.h"

using namespace sgcl::async;

#include <latch>
#include <stdexcept>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    struct Counted {
        explicit Counted(tracked_ptr<Node> n) : node(std::move(n)) { ++alive; }
        Counted(const Counted& o) : node(o.node) { ++alive; }
        Counted(Counted&& o) noexcept : node(std::move(o.node)) { ++alive; }
        ~Counted() { --alive; }
        void operator()(int* out, std::latch* go) const { go->wait(); *out = node->value; }
        tracked_ptr<Node> node;
        inline static sgcl::atomic<int> alive = {0};
    };

    // A callable whose copy throws
    struct Throwing {
        Throwing() = default;
        Throwing(const Throwing&) { throw std::runtime_error("copy"); }
        void operator()() const {}
        tracked_ptr<Node> node;
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

// The captured pointer is the object's only reference once the frame
// that made it is gone; the collector runs while the thread has not
// touched it yet, and the thread finds it alive: the closure is in a
// managed node, traced, held by the root that travels with std::thread.
TEST(Thread_Tests, ACapturedTrackedPtrKeepsItsObject) {
    std::latch collected(1);
    int seen = 0;
    thread t;
    off_frame([&] {
        tracked_ptr node = make_tracked<Node>(7);
        t = thread([=, &collected, &seen] {                  // by value: the pointer goes with the closure
            collected.wait();
            seen = node->value;
        });
    });
    settle();                                                 // nothing on this stack points at the node now
    EXPECT_EQ(Node::alive.load(), 1);
    collected.count_down();
    t.join();
    EXPECT_EQ(seen, 7);
    settle();
    EXPECT_EQ(Node::alive.load(), 0);                          // the closure died with the call, the node with it
}

// The arguments are copied to the node as the callable is, and the
// callable is called with them as rvalues, once
TEST(Thread_Tests, ArgumentsGoToTheNodeToo) {
    std::latch go(1);
    int out = 0;
    thread t;
    off_frame([&] {
        tracked_ptr node = make_tracked<Node>(3);
        t = thread(Counted(node), &out, &go);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), 1);                          // held by the callable in the node, the thread waiting
    go.count_down();
    t.join();
    EXPECT_EQ(out, 3);
    settle();
    EXPECT_EQ(Counted::alive.load(), 0);
    EXPECT_EQ(Node::alive.load(), 0);
}

// A callable whose copy throws leaves no thread and no object behind
TEST(Thread_Tests, ACopyThatThrowsLeavesNothingBehind) {
    off_frame([] {
        Throwing f;
        f.node = make_tracked<Node>(1);
        EXPECT_THROW(thread{f}, std::runtime_error);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), 0);
}

TEST(Thread_Tests, TheInterfaceOfStdThread) {
    EXPECT_GT(thread::hardware_concurrency(), 0u);
    thread none;
    EXPECT_FALSE(none.joinable());
    EXPECT_EQ(none.get_id(), thread::id());

    std::latch go(1);
    thread::id inside;
    thread t([&] { go.wait(); inside = this_thread::get_id(); });
    EXPECT_TRUE(t.joinable());
    EXPECT_NE(t.get_id(), thread::id());
    auto id = t.get_id();

    thread moved = std::move(t);                              // the thread moves, the source is empty
    EXPECT_FALSE(t.joinable());
    EXPECT_EQ(moved.get_id(), id);
    none.swap(moved);
    EXPECT_TRUE(none.joinable());
    EXPECT_FALSE(moved.joinable());
    go.count_down();
    none.join();
    EXPECT_FALSE(none.joinable());
    EXPECT_EQ(inside, id);

    std::latch done(1);
    thread d([&] { done.count_down(); });
    d.detach();
    EXPECT_FALSE(d.joinable());
    done.wait();
}

// The threads of a program kept in a vector on the stack; each holds a
// pointer to a shared object by value and adds to it
TEST(Thread_Tests, KeptInAVectorSharingAnObject) {
    struct Sum {
        sgcl::atomic<int> total = {0};
    };
    int result = 0;
    off_frame([&] {
        tracked_ptr sum = make_tracked<Sum>();
        vector<thread> workers;
        for (int t : range(4)) {
            workers.emplace_back([=] {
                for (int i : range(1000)) {
                    sum->total += t * 1000 + i;
                }
            });
        }
        for (auto& w : workers) {
            w.join();
        }
        result = sum->total.load();
    });
    EXPECT_EQ(result, 4 * 999 * 1000 / 2 + 1000 * (0 + 1 + 2 + 3) * 1000);
}
