//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ConcurrentPriorityQueue: the facade over sgcl::concurrent_priority_queue
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {
    struct Job {
        int priority;
        int id;
    };

    struct ByPriority {
        bool operator()(const Job& a, const Job& b) const noexcept {
            return a.priority < b.priority;
        }
    };

    struct Item {
        explicit Item(int v = 0)
        : value(v) {
        }

        int value;
    };

    struct ByValue {
        bool operator()(const Ptr<Item>& a, const Ptr<Item>& b) const noexcept {
            return a->value < b->value;
        }
    };
}

TEST(Sgcl_PriorityQueue_Tests, DequeueInOrder) {
    using namespace Sgcl;
    ConcurrentPriorityQueue<int> q;
    EXPECT_TRUE(q.IsEmpty());
    EXPECT_EQ(q.Count(), 0u);
    EXPECT_FALSE(q.TryDequeue());
    EXPECT_FALSE(q.TryPeek());
    q.Enqueue(5);
    int two = 2;
    q.Enqueue(two);
    q.Emplace(9);
    q.Enqueue(1);
    EXPECT_EQ(q.Count(), 4u);
    EXPECT_EQ(*q.TryPeek(), 1);
    EXPECT_EQ(q.Dequeue(), 1);
    EXPECT_EQ(*q.TryDequeue(), 2);
    EXPECT_EQ(*q.TryPeek(), 5);
    EXPECT_EQ(q.Count(), 2u);
    q.Clear();
    EXPECT_TRUE(q.IsEmpty());
    EXPECT_FALSE(q.TryDequeue());
    q.Inner().push(3);   // the sgcl class inside
    EXPECT_EQ(q.Dequeue(), 3);
}

TEST(Sgcl_PriorityQueue_Tests, ConstructorsComparerAndDuplicates) {
    using namespace Sgcl;
    ConcurrentPriorityQueue<int, std::greater<int>> g = {3, 9, 1};
    EXPECT_EQ(g.Dequeue(), 9);
    EXPECT_EQ(g.Dequeue(), 3);
    EXPECT_EQ(g.Dequeue(), 1);
    EXPECT_TRUE(g.Comparer()(2, 1));
    List<String> names = {"bob", "alice", "carol"};
    ConcurrentPriorityQueue<String> s(begin(names), end(names));
    EXPECT_EQ(s.Dequeue(), "alice");
    EXPECT_EQ(s.Dequeue(), "bob");
    EXPECT_EQ(s.Dequeue(), "carol");
    ConcurrentPriorityQueue<Job, ByPriority> tasks;
    tasks.Enqueue({1, 1});
    tasks.Enqueue({1, 2});
    tasks.Enqueue({0, 3});
    tasks.Enqueue({1, 4});
    std::vector<int> ids;
    while (auto t = tasks.TryDequeue()) {
        ids.push_back(t->id);
    }
    EXPECT_EQ(ids, (std::vector<int>{3, 1, 2, 4}));   // equal priorities in the order they came
}

TEST(Sgcl_PriorityQueue_Tests, PtrElementsInsideManagedObject) {
    using namespace Sgcl;
    struct Holder {
        ConcurrentPriorityQueue<Ptr<Item>, ByValue> q;
    };
    Ptr h = Make<Holder>();
    h->q.Enqueue(Make<Item>(8));
    h->q.Enqueue(Make<Item>(4));
    h->q.Emplace(Make<Item>(6));
    collector::force_collect(true);
    EXPECT_EQ((*h->q.TryPeek())->value, 4);
    EXPECT_EQ(h->q.Dequeue()->value, 4);
    EXPECT_EQ((*h->q.TryDequeue())->value, 6);
    EXPECT_EQ(h->q.Count(), 1u);
    EXPECT_EQ(h->q.Dequeue()->value, 8);
    EXPECT_TRUE(h->q.IsEmpty());
}

TEST(Sgcl_PriorityQueue_Tests, DequeueWaitsForEnqueue) {
    using namespace Sgcl;
    off_frame([&] {
        ConcurrentPriorityQueue<int> q;
        Atomic<int> got = {0};
        Thread consumer([&] {
            got = q.Dequeue();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_EQ(got.Load(), 0);
        q.Enqueue(42);
        consumer.Join();
        EXPECT_EQ(got.Load(), 42);
        List<Thread> producers;
        for (int t : Range(4)) {
            producers.Emplace([&, t] {
                for (int i : Range(1000)) {
                    q.Enqueue(t * 1000 + i);
                }
            });
        }
        for (auto& p : producers) {
            p.Join();
        }
        EXPECT_EQ(q.Count(), 4000u);
        int last = -1, disorder = 0;
        for (int i : Range(4000)) {
            int v = q.Dequeue();
            disorder += v <= last;
            last = v;
        }
        EXPECT_EQ(disorder, 0);
        EXPECT_TRUE(q.IsEmpty());
    });
}
