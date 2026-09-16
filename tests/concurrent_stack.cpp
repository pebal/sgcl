//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

TEST(ConcurrentStack_Test, PushPopOrder) {
    sgcl::concurrent_stack<int> s;
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
    sgcl::concurrent_stack<std::string> s;
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
    sgcl::concurrent_stack<Int> s;
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
    sgcl::concurrent_stack<tracked_ptr<Baz>> s;
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
        sgcl::concurrent_stack<int> s;
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
        sgcl::concurrent_stack<tracked_ptr<Baz>> s;
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
        sgcl::concurrent_stack<tracked_ptr<Baz>> s;
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
