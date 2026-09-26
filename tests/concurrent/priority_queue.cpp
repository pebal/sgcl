//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {
    // A job with a priority and an identity: what tells equal priorities
    // apart in the tests of the FIFO order among equals
    struct Job {
        int priority;
        int id;
    };

    struct ByPriority {
        bool operator()(const Job& a, const Job& b) const noexcept {
            return a.priority < b.priority;
        }
    };

    struct ByValue {
        bool operator()(const tracked_ptr<Baz>& a, const tracked_ptr<Baz>& b) const noexcept {
            return a->value < b->value;
        }
    };

    template<class Q>
    std::vector<int> drain(Q& q) {
        std::vector<int> out;
        while (auto v = q.try_pop()) {
            out.push_back(int(*v));
        }
        return out;
    }
}

TEST(ConcurrentPriorityQueue_Test, PopsInOrder) {
    sgcl::concurrent::priority_queue<int> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
    EXPECT_FALSE(q.try_pop());
    EXPECT_FALSE(q.try_top());
    q.push(5);
    int two = 2;
    q.push(two);
    q.emplace(9);
    q.push(1);
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 4u);
    EXPECT_EQ(*q.try_top(), 1);
    EXPECT_EQ(*q.try_pop(), 1);
    EXPECT_EQ(*q.try_pop(), 2);
    EXPECT_EQ(q.pop(), 5);
    EXPECT_EQ(*q.try_top(), 9);
    EXPECT_EQ(q.pop(), 9);
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.try_pop());
    q.push(4);   // usable again after running empty
    EXPECT_EQ(*q.try_pop(), 4);
}

TEST(ConcurrentPriorityQueue_Test, ConstructorsAndTypes) {
    sgcl::concurrent::priority_queue<int> a = {3, 1, 2};
    EXPECT_EQ(drain(a), (std::vector<int>{1, 2, 3}));
    std::vector<int> v = {4, 6, 5};
    sgcl::concurrent::priority_queue<int> b(v.begin(), v.end());
    EXPECT_EQ(drain(b), (std::vector<int>{4, 5, 6}));
    sgcl::concurrent::priority_queue<std::string> s;
    s.emplace(3, 'x');
    s.emplace("abc");
    s.push("b");
    EXPECT_EQ(*s.try_pop(), "abc");
    EXPECT_EQ(*s.try_pop(), "b");
    EXPECT_EQ(*s.try_pop(), "xxx");
    static_assert(std::is_same_v<sgcl::concurrent::priority_queue<int>::value_type, int>);
    static_assert(std::is_same_v<sgcl::concurrent::priority_queue<int>::value_compare, std::less<int>>);
}

TEST(ConcurrentPriorityQueue_Test, CustomComparator) {
    sgcl::concurrent::priority_queue<int, std::greater<int>> q = {3, 9, 1, 7};
    EXPECT_EQ(*q.try_top(), 9);
    EXPECT_EQ(drain(q), (std::vector<int>{9, 7, 3, 1}));
    auto longer = [](const std::string& a, const std::string& b) { return a.size() > b.size(); };
    sgcl::concurrent::priority_queue<std::string, decltype(longer)> l(longer);
    l.push("a");
    l.push("ccc");
    l.push("bb");
    EXPECT_EQ(*l.try_pop(), "ccc");
    EXPECT_EQ(*l.try_pop(), "bb");
    EXPECT_EQ(*l.try_pop(), "a");
    EXPECT_TRUE(l.value_comp()("xx", "x"));
}

// Equal elements are told apart by the number their push took: they
// come out in the order they went in, from any number of threads
TEST(ConcurrentPriorityQueue_Test, DuplicatesFifo) {
    sgcl::concurrent::priority_queue<Job, ByPriority> q;
    q.push({1, 1});
    q.push({1, 2});
    q.push({0, 3});
    q.push({1, 4});
    q.push({0, 5});
    EXPECT_EQ(q.size(), 5u);
    std::vector<int> ids;
    while (auto j = q.try_pop()) {
        ids.push_back(j->id);
    }
    EXPECT_EQ(ids, (std::vector<int>{3, 5, 1, 2, 4}));
    sgcl::concurrent::priority_queue<int> same;
    for (int i = 0; i < 1000; ++i) {
        same.push(7);
    }
    EXPECT_EQ(same.size(), 1000u);
    EXPECT_EQ(drain(same).size(), 1000u);
    EXPECT_TRUE(same.empty());
}

TEST(ConcurrentPriorityQueue_Test, DuplicatesFifoPerProducerManyThreads) {
    const int producers = 4;
    const int n = 5000;
    std::vector<std::vector<int>> popped(producers * 3);   // per producer and priority
    off_frame([&] {
        sgcl::concurrent::priority_queue<Job, ByPriority> q;   // three priorities, each producer's ids in order
        std::vector<std::thread> ws;
        for (int t = 0; t < producers; ++t) {
            ws.emplace_back([&, t] {
                for (int i = 0; i < n; ++i) {
                    q.push({i % 3, t * n + i});
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_EQ(q.size(), size_t(producers * n));
        int priority = 0;
        while (auto j = q.try_pop()) {
            EXPECT_GE(j->priority, priority);
            priority = j->priority;
            popped[size_t(j->id / n * 3 + j->priority)].push_back(j->id);
        }
    });
    for (auto& ids : popped) {   // one producer's equal jobs in the order it pushed them
        EXPECT_EQ(ids.size(), size_t(n / 3 + (ids.front() % n < n % 3)));
        EXPECT_TRUE(std::is_sorted(ids.begin(), ids.end()));
    }
}

TEST(ConcurrentPriorityQueue_Test, TryTopCopiesAndLeavesTheElement) {
    sgcl::concurrent::priority_queue<std::string> q = {"b", "a"};
    auto top = q.try_top();
    ASSERT_TRUE(top);
    EXPECT_EQ(*top, "a");
    EXPECT_EQ(q.size(), 2u);
    EXPECT_EQ(*q.try_top(), "a");
    EXPECT_EQ(*q.try_pop(), "a");
    EXPECT_EQ(*q.try_top(), "b");
    q.clear();
    EXPECT_FALSE(q.try_top());
}

TEST(ConcurrentPriorityQueue_Test, Clear) {
    sgcl::concurrent::priority_queue<int> q;
    for (int i = 0; i < 100; ++i) {
        q.push(i % 10);
    }
    EXPECT_EQ(q.size(), 100u);
    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
    EXPECT_FALSE(q.try_pop());
    q.push(1);
    EXPECT_EQ(q.pop(), 1);
}

// An element is moved out at the pop and destroyed then, as
// std::priority_queue's is; clear destroys the rest at once
TEST(ConcurrentPriorityQueue_Test, ElementMovedOutAtThePop) {
    const size_t before = Int::counter;
    sgcl::concurrent::priority_queue<Int> q;
    q.push(Int(3));
    q.emplace(1);
    q.push(Int(2));
    EXPECT_EQ(Int::counter, before + 3);
    {
        auto v = q.try_pop();
        EXPECT_EQ(*v, 1);
        EXPECT_EQ(Int::counter, before + 3);       // the element in v now, gone from the heap
    }
    EXPECT_EQ(Int::counter, before + 2);
    q.clear();
    EXPECT_EQ(Int::counter, before);
    EXPECT_TRUE(q.empty());
}

// Elements holding tracked_ptrs: traced in the heap's buffer, kept alive
// by the queue and let go at the pop
TEST(ConcurrentPriorityQueue_Test, ObjectsHeldByTheHeapAndReleasedAtThePop) {
    const size_t before = collector::get_live_object_count();
    sgcl::concurrent::priority_queue<tracked_ptr<Baz>, ByValue> q;
    off_frame([&] {
        for (int i = 99; i >= 0; --i) {
            q.push(make_tracked<Baz>(i));
        }
    });
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before + 101u);   // the heap's buffer, 100 Baz
    EXPECT_EQ(q.size(), 100u);
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
        collector::force_collect(true);
        collector::force_collect(true);
        EXPECT_EQ(collector::get_live_object_count(), before + 2u);   // the buffer, the Baz still held
    });
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);   // the buffer
}

// A move-only element goes in and comes out as it is: the heap moves
// elements, nothing is copied (try_top alone needs a copy, and is not
// there for such a T)
TEST(ConcurrentPriorityQueue_Test, MoveOnlyElements) {
    struct Payload {
        explicit Payload(int v)
        : value(std::make_unique<int>(v)) {
        }

        std::unique_ptr<int> value;
    };
    auto by_value = [](const Payload& a, const Payload& b) { return *a.value < *b.value; };
    sgcl::concurrent::priority_queue<Payload, decltype(by_value)> q(by_value);
    q.push(Payload(3));
    q.emplace(1);
    q.push(Payload(2));
    EXPECT_EQ(*(*q.try_pop()).value, 1);
    EXPECT_EQ(*q.pop().value, 2);
    EXPECT_EQ(*q.pop().value, 3);
    EXPECT_TRUE(q.empty());
    auto by_ptr = [](const tracked_ptr<Payload>& a, const tracked_ptr<Payload>& b) { return *a->value < *b->value; };
    sgcl::concurrent::priority_queue<tracked_ptr<Payload>, decltype(by_ptr)> p(by_ptr);   // or behind a tracked_ptr, one word
    p.push(make_tracked<Payload>(3));
    p.push(make_tracked<Payload>(1));
    EXPECT_EQ(*(*p.try_top())->value, 1);
    EXPECT_EQ(*p.pop()->value, 1);
    EXPECT_EQ(*p.pop()->value, 3);
}

TEST(ConcurrentPriorityQueue_Test, QueueInsideManagedObject) {
    struct Holder {
        sgcl::concurrent::priority_queue<int> q;
        sgcl::concurrent::priority_queue<tracked_ptr<Baz>, ByValue> p;
    };
    tracked_ptr h = make_tracked<Holder>();
    h->q.push(2);
    h->q.push(1);
    h->p.push(make_tracked<Baz>(8));
    h->p.push(make_tracked<Baz>(4));
    EXPECT_EQ(*h->q.try_pop(), 1);
    EXPECT_EQ(h->q.size(), 1u);
    EXPECT_EQ((*h->p.try_pop())->value, 4);
    collector::force_collect(true);
    EXPECT_EQ((*h->p.try_top())->value, 8);
}

// pop blocks on an empty queue and is woken by a push from another
// thread, and by every push when several consumers wait
TEST(ConcurrentPriorityQueue_Test, BlockingPopWokenByPush) {
    off_frame([&] {
        sgcl::concurrent::priority_queue<int> q;
        sgcl::atomic<int> got = {0};
        std::thread consumer([&] {
            got = q.pop();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_EQ(got.load(), 0);   // still waiting
        q.push(42);
        consumer.join();
        EXPECT_EQ(got.load(), 42);
        EXPECT_TRUE(q.empty());

        const int consumers = 4;
        sgcl::atomic<int> sum = {0};
        std::vector<std::thread> ws;
        for (int t = 0; t < consumers; ++t) {
            ws.emplace_back([&] {
                sum += q.pop();
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        for (int i = 1; i <= consumers; ++i) {
            q.push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_EQ(sum.load(), 10);
        EXPECT_TRUE(q.empty());
    });
}

// Producers push random values while consumers pop: every value comes
// out exactly once, the collector runs meanwhile, nothing is left
TEST(ConcurrentPriorityQueue_Test, ProducersAndConsumersManyThreads) {
    const int producers = 4;
    const int consumers = 4;
    const int n = 20000;
    std::vector<sgcl::atomic<int>> seen(size_t(producers * n));
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent::priority_queue<tracked_ptr<Baz>, ByValue> q;
        sgcl::atomic<long> popped = {0};
        std::vector<std::thread> ws;
        for (int t = 0; t < producers; ++t) {
            ws.emplace_back([&, t] {
                std::vector<int> values(static_cast<size_t>(n));
                for (int i = 0; i < n; ++i) {
                    values[size_t(i)] = t * n + i;
                }
                std::shuffle(values.begin(), values.end(), std::mt19937(unsigned(t + 1)));
                for (int i = 0; i < n; ++i) {
                    q.push(make_tracked<Baz>(values[size_t(i)]));
                    if (t == 0 && i % 5000 == 0) {
                        collector::force_collect();
                    }
                }
            });
        }
        for (int t = 0; t < consumers; ++t) {
            ws.emplace_back([&] {
                while (popped.load() < long(producers * n)) {
                    if (auto v = q.try_pop()) {
                        ++seen[size_t((*v)->value)];
                        ++popped;
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_TRUE(q.empty());
        EXPECT_EQ(q.size(), 0u);
    });
    for (auto& c : seen) {
        ASSERT_EQ(c.load(), 1);
    }
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before);
}

// With the pushes done, the least element only grows: the values each
// consumer pops, blocking, are increasing, and every value is popped once
TEST(ConcurrentPriorityQueue_Test, ConcurrentPopsInOrder) {
    const int threads = 8;
    const int n = 10000;
    std::vector<sgcl::atomic<int>> seen(size_t(threads * n));
    sgcl::atomic<bool> out_of_order = {false};
    off_frame([&] {
        sgcl::concurrent::priority_queue<int> q;
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                std::mt19937 rng(unsigned(t + 7));
                std::vector<int> values(static_cast<size_t>(n));
                for (int i = 0; i < n; ++i) {
                    values[size_t(i)] = t * n + i;
                }
                std::shuffle(values.begin(), values.end(), rng);
                for (int v : values) {
                    q.push(v);
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        ws.clear();
        EXPECT_EQ(q.size(), size_t(threads * n));
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                int last = -1;
                for (int i = 0; i < n; ++i) {
                    int v = q.pop();
                    if (v <= last) {
                        out_of_order = true;
                    }
                    last = v;
                    ++seen[size_t(v)];
                    if (t == 0 && i % 2500 == 0) {
                        collector::force_collect();
                    }
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
