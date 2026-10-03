//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/throwing.h"
#include "tests/concurrent/together.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

TEST(ConcurrentStack_Test, PushPopOrder) {
    sgcl::concurrent::stack<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_FALSE(s.try_pop());
    s.push(1);
    int two = 2;
    s.push(two);
    s.emplace(3);
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(*s.try_pop(), 3);
    EXPECT_EQ(*s.try_pop(), 2);
    EXPECT_EQ(s.pop(), 1);
    EXPECT_TRUE(s.empty());
    EXPECT_FALSE(s.try_pop());
}

TEST(ConcurrentStack_Test, EmplaceAndClear) {
    sgcl::concurrent::stack<std::string> s;
    s.emplace(3, 'x');
    s.emplace("abc");
    EXPECT_EQ(*s.try_pop(), "abc");
    EXPECT_EQ(*s.try_pop(), "xxx");
    for (int i = 0; i < 10; ++i) {
        s.push(std::to_string(i));
    }
    EXPECT_EQ(s.size(), 10u);
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(ConcurrentStack_Test, ElementDestroyedByPop) {
    const size_t before = Int::counter;
    sgcl::concurrent::stack<Int> s;
    s.push(Int(1));
    s.emplace(2);
    s.push(Int(3));
    EXPECT_EQ(Int::counter, before + 3);
    {
        auto v = s.try_pop();
        EXPECT_EQ(*v, 3);
        EXPECT_EQ(Int::counter, before + 3);   // moved out of the node, alive in v
    }
    EXPECT_EQ(Int::counter, before + 2);   // and gone with it
    s.clear();
    EXPECT_EQ(Int::counter, before);
}

TEST(ConcurrentStack_Test, NodesAndObjectsReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::concurrent::stack<tracked_ptr<Baz>> s;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            s.push(make_tracked<Baz>(i));
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 200u);   // 100 nodes, 100 Baz
    tracked_ptr<Baz> kept;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            auto v = s.try_pop();
            ASSERT_TRUE(v);
            EXPECT_EQ((*v)->value, 99 - i);
            if (i == 0) {
                kept = *v;
            }
        }
        EXPECT_TRUE(s.empty());
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);   // the one Baz still held
    kept = nullptr;
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(ConcurrentStack_Test, StackInsideManagedObject) {
    struct Holder {
        sgcl::concurrent::stack<int> s;
    };
    tracked_ptr h = make_tracked<Holder>();
    h->s.push(1);
    h->s.push(2);
    EXPECT_EQ(*h->s.try_pop(), 2);
    EXPECT_EQ(h->s.size(), 1u);
}

TEST(ConcurrentStack_Test, MixedPushPopManyThreads) {
    const int threads = 8;
    const long n = 20000;
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent::stack<tracked_ptr<Baz>> s;
        sgcl::atomic<long> popped = {0};
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                long sum = 0;
                for (long i = 0; i < n; ++i) {
                    s.push(make_tracked<Baz>(int(t * n + i)));
                    if (auto v = s.try_pop()) {
                        sum += (*v)->value;
                        ++popped;
                    }
                    if (t == 0 && i % 5000 == 0) {
                        collector::force_collect();
                    }
                }
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (sum < 0) {
                    ADD_FAILURE();
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        // every thread pushed n and popped at most n: what is left is the difference
        EXPECT_EQ(s.size() + size_t(popped.load()), size_t(threads * n));
        s.clear();
        EXPECT_TRUE(s.empty());
    });
    // a cycle forced during the run may be half done: two full ones, so
    // that no sticky mark of a young cycle is left on a popped node
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(ConcurrentStack_Test, ProducersAndBlockingConsumers) {
    const int pairs = 4;
    const int n = 20000;
    std::vector<sgcl::atomic<int>> seen(size_t(pairs * n));
    off_frame([&] {
        sgcl::concurrent::stack<tracked_ptr<Baz>> s;
        std::vector<std::thread> ws;
        for (int t = 0; t < pairs; ++t) {
            ws.emplace_back([&, t] {
                for (int i = 0; i < n; ++i) {
                    s.emplace(make_tracked<Baz>(t * n + i));
                }
            });
            ws.emplace_back([&] {
                for (int i = 0; i < n; ++i) {
                    tracked_ptr<Baz> v = s.pop();   // blocks while the stack is empty
                    ++seen[size_t(v->value)];
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_TRUE(s.empty());
    });
    for (auto& c : seen) {
        ASSERT_EQ(c.load(), 1);   // every value popped exactly once
    }
}

// Boundaries (DESIGN 408)

// An empty stack and a stack of one: the pops of nothing, clear of
// nothing, the one element out, the stack usable after clear; a null
// tracked_ptr is an element like any other
TEST(ConcurrentStack_Test, EmptyAndOneElement) {
    sgcl::concurrent::stack<int> s;
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_FALSE(s.try_pop());
    s.push(1);
    EXPECT_EQ(s.size(), 1u);
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_FALSE(s.try_pop());
    s.emplace(2);
    EXPECT_EQ(s.pop(), 2);
    EXPECT_TRUE(s.empty());
    sgcl::concurrent::stack<tracked_ptr<Baz>> p;
    p.push(nullptr);
    EXPECT_FALSE(p.empty());
    auto null = p.try_pop();
    ASSERT_TRUE(null);
    EXPECT_EQ(*null, nullptr);
    EXPECT_TRUE(p.empty());
}

// Many threads at the empty stack: every try_pop finds nothing, and the
// one element pushed into it is taken by exactly one of them
TEST(ConcurrentStack_Test, ThreadsAtTheLastElement) {
    sgcl::concurrent::stack<int> s;
    for (int round = 0; round < together::Rounds; ++round) {
        std::atomic<int> got = {0}, empty = {0};
        together::run(4, [&](int) {
            empty += !s.try_pop();
        });
        EXPECT_EQ(empty.load(), 4);
        s.push(round);
        together::run(4, [&](int) {
            if (auto v = s.try_pop()) {
                EXPECT_EQ(*v, round);
                ++got;
            }
        });
        EXPECT_EQ(got.load(), 1);
        EXPECT_TRUE(s.empty());
        s.push(round);   // the last element: a pop against a clear, one of them has it
        together::run(2, [&](int i) {
            if (i == 0) {
                got += s.try_pop().has_value();
            } else {
                s.clear();
            }
        });
        EXPECT_TRUE(s.empty());
    }
}

// clear while threads wait in pop: it takes nothing from them and wakes
// none; each pushed element afterwards goes to one waiter
TEST(ConcurrentStack_Test, ClearWhileOthersWait) {
    const int waiters = 3;
    sgcl::concurrent::stack<int> s;
    std::atomic<int> sum = {0}, done = {0};
    std::vector<std::thread> ts;
    for (int i = 0; i < waiters; ++i) {
        ts.emplace_back([&] {
            sum += s.pop();
            ++done;
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));   // the waiters in pop (a later arrival only spins less)
    s.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(done.load(), 0);
    for (int i = 1; i <= waiters; ++i) {
        s.push(i);
    }
    for (auto& t : ts) {
        t.join();
    }
    EXPECT_EQ(sum.load(), 6);
    EXPECT_TRUE(s.empty());
}

// An element whose move throws: the pop takes it off and loses it (the
// node is the collector's), the stack otherwise intact; a push whose copy
// throws links nothing (try_pop.md, pop.md, push.md)
TEST(ConcurrentStack_Test, AMoveThatThrows) {
    using throwing::Val;
    throwing::Disarm disarm;
    sgcl::concurrent::stack<Val> s;
    s.push(Val(1));
    s.push(Val(2));
    s.push(Val(3));
    throwing::countdown.move = 1;
    EXPECT_THROW(s.try_pop(), throwing::Error);
    EXPECT_EQ(s.size(), 2u);
    throwing::countdown.move = 2;   // the move out of the node, then the one out of the optional into pop's value
    EXPECT_THROW(s.pop(), throwing::Error);
    EXPECT_EQ(s.size(), 1u);
    Val v(4);
    throwing::countdown.copy = 1;
    EXPECT_THROW(s.push(v), throwing::Error);
    throwing::countdown.construct = 1;
    EXPECT_THROW(s.emplace(5), throwing::Error);
    throwing::countdown = {};
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.try_pop()->v, 1);
    EXPECT_FALSE(s.try_pop());
}
