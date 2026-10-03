//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// thread: std::thread's interface, the callable and the arguments in a
// managed node of their own, followed by the collector.
#include "tests/types.h"

using namespace sgcl::async;

#include <latch>
#include <stdexcept>
#include <thread>
#include <utility>

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

// Types of the numbering test below and of nothing else: their first
// allocation in the process is the one the test races. Of different
// sizes, so that an object made by another type's allocator shows in its
// size as well as in its type.
namespace {
    template<int N>
    struct Fresh {
        Fresh(int t, int i) : thread(t), index(i) {}
        int thread;
        int index;
        char pad[16 * (N + 1)] = {};
        tracked_ptr<Fresh> next;
    };

    template<int N>
    SGCL_NOINLINE bool make_and_check_fresh(int t, int count) {
        tracked_ptr<Fresh<N>> head;
        for (int i = 0; i < count; ++i) {
            tracked_ptr<Fresh<N>> p = make_tracked<Fresh<N>>(t, i);
            p->next = head;
            head = p;
        }
        collector::force_collect(false);
        int i = count;
        for (auto p = head; p; p = p->next) {
            --i;
            if (p->thread != t || p->index != i || p.type() != typeid(Fresh<N>)
                || sgcl::detail::Pointer::object_size(p.get()) != sizeof(Fresh<N>)) {
                return false;
            }
        }
        return i == 0;
    }

    template<int... N>
    bool make_and_check_fresh_all(int t, int count, std::integer_sequence<int, N...>) {
        return (make_and_check_fresh<N>(t, count) & ...);
    }
}

// Threads that allocate a type never allocated before at the same moment
// race to number it (thread.h: _slot_index, a CAS from 0): one number
// wins, every thread gets an allocator of that type's own pool in that
// slot. Eight types, eight threads released together; every
// object is checked for its values, its type and its size after a cycle.
TEST(Thread_Tests, ThreadsNumberANewTypeAtOnce) {
    constexpr int Threads = 8;
    constexpr int Count = 3000;   // more objects than one page holds of every one of them
    // the heap and the collector made before the race (run alone, the
    // test would otherwise also race the heap's construction against the
    // debug assertion of tracked_ptr's constructor, which reads its range)
    (void)make_tracked<int>();
    std::latch go(Threads);
    std::atomic<int> failed = {0};
    std::vector<std::thread> workers;
    for (int t = 0; t < Threads; ++t) {
        workers.emplace_back([&, t] {
            go.arrive_and_wait();
            if (!make_and_check_fresh_all(t, Count, std::make_integer_sequence<int, 8>())) {
                failed.fetch_add(1);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    EXPECT_EQ(failed.load(), 0);
}
