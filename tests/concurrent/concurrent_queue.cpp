//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

TEST(ConcurrentQueue_Test, PushPopOrder) {
    sgcl::concurrent_queue<int> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
    EXPECT_FALSE(q.try_pop());
    q.push(1);
    int two = 2;
    q.push(two);
    q.emplace(3);
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 3u);
    EXPECT_EQ(*q.try_pop(), 1);
    EXPECT_EQ(*q.try_pop(), 2);
    EXPECT_EQ(q.pop(), 3);
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.try_pop());
    q.push(4);   // usable again after running empty
    EXPECT_EQ(*q.try_pop(), 4);
}

TEST(ConcurrentQueue_Test, EmplaceAndClear) {
    sgcl::concurrent_queue<std::string> q;
    q.emplace(3, 'x');
    q.emplace("abc");
    EXPECT_EQ(*q.try_pop(), "xxx");
    EXPECT_EQ(*q.try_pop(), "abc");
    for (int i = 0; i < 10; ++i) {
        q.push(std::to_string(i));
    }
    EXPECT_EQ(q.size(), 10u);
    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

TEST(ConcurrentQueue_Test, ElementDestroyedByPop) {
    const size_t before = Int::counter;
    sgcl::concurrent_queue<Int> q;
    q.push(Int(1));
    q.emplace(2);
    q.push(Int(3));
    EXPECT_EQ(Int::counter, before + 3);
    {
        auto v = q.try_pop();
        EXPECT_EQ(*v, 1);
        EXPECT_EQ(Int::counter, before + 3);   // moved out of the node, alive in v
    }
    EXPECT_EQ(Int::counter, before + 2);
    q.clear();
    EXPECT_EQ(Int::counter, before);
}

TEST(ConcurrentQueue_Test, NodesAndObjectsReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::concurrent_queue<tracked_ptr<Baz>> q;
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);   // the dummy
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            q.push(make_tracked<Baz>(i));
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 201u);   // the dummy, 100 nodes, 100 Baz
    off_frame([&] {
        tracked_ptr<Baz> kept;
        for (int i = 0; i < 100; ++i) {
            auto v = q.try_pop();
            ASSERT_TRUE(v);
            EXPECT_EQ((*v)->value, i);
            if (i == 0) {
                kept = *v;
            }
        }
        EXPECT_TRUE(q.empty());
        EXPECT_EQ(collector::get_live_object_count(), before + 2u);   // the last node as the dummy, the Baz still held
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);
}

TEST(ConcurrentQueue_Test, QueueInsideManagedObject) {
    struct Holder {
        sgcl::concurrent_queue<int> q;
    };
    tracked_ptr h = make_tracked<Holder>();
    h->q.push(1);
    h->q.push(2);
    EXPECT_EQ(*h->q.try_pop(), 1);
    EXPECT_EQ(h->q.size(), 1u);
}

TEST(ConcurrentQueue_Test, MixedPushPopManyThreads) {
    const int threads = 8;
    const long n = 20000;
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent_queue<tracked_ptr<Baz>> q;
        sgcl::atomic<long> popped = {0};
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                long sum = 0;
                for (long i = 0; i < n; ++i) {
                    q.push(make_tracked<Baz>(int(t * n + i)));
                    if (auto v = q.try_pop()) {
                        sum += (*v)->value;
                        ++popped;
                    }
                    if (t == 0 && i % 5000 == 0) {
                        collector::force_collect();
                    }
                }
                if (sum < 0) {
                    ADD_FAILURE();
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_EQ(q.size() + size_t(popped.load()), size_t(threads * n));
        q.clear();
        EXPECT_TRUE(q.empty());
    });
    // a cycle forced during the run may be half done: two full ones, so
    // that no sticky mark of a young cycle is left on a popped node
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before);
}

// Every producer's elements come out in the order it pushed them, at
// every consumer, and every element comes out once.
TEST(ConcurrentQueue_Test, ProducersAndBlockingConsumersFifo) {
    const int pairs = 4;
    const int n = 20000;
    std::vector<sgcl::atomic<int>> seen(size_t(pairs * n));
    sgcl::atomic<bool> out_of_order = {false};
    off_frame([&] {
        sgcl::concurrent_queue<tracked_ptr<Baz>> q;
        std::vector<std::thread> ws;
        for (int t = 0; t < pairs; ++t) {
            ws.emplace_back([&, t] {
                for (int i = 0; i < n; ++i) {
                    q.emplace(make_tracked<Baz>(t * n + i));
                }
            });
            ws.emplace_back([&] {
                std::vector<int> last(pairs, -1);
                for (int i = 0; i < n; ++i) {
                    tracked_ptr<Baz> v = q.pop();   // blocks while the queue is empty
                    int producer = v->value / n;
                    if (v->value <= last[size_t(producer)]) {
                        out_of_order = true;
                    }
                    last[size_t(producer)] = v->value;
                    ++seen[size_t(v->value)];
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_TRUE(q.empty());
    });
    EXPECT_FALSE(out_of_order.load());
    for (auto& c : seen) {
        ASSERT_EQ(c.load(), 1);
    }
}

TEST(ConcurrentQueue_Test, SingleProducerSingleConsumerExactOrder) {
    const int n = 100000;
    off_frame([&] {
        sgcl::concurrent_queue<int> q;
        std::thread producer([&] {
            for (int i = 0; i < n; ++i) {
                q.push(i);
            }
        });
        int expected = 0;
        while (expected < n) {
            if (auto v = q.try_pop()) {
                ASSERT_EQ(*v, expected);
                ++expected;
            }
        }
        producer.join();
        EXPECT_TRUE(q.empty());
    });
}
