//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The bounded queues of the Sgcl interface: ConcurrentBoundedQueue and
// SpscQueue over their sgcl types, the methods forward, the pointers
// inside are traced.
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <string>
#include <thread>
#include <vector>

namespace {
    struct Item {
        explicit Item(int v = 0) : value(v) { ++alive; }
        Item(const Item& o) : value(o.value) { ++alive; }
        ~Item() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Sgcl_BoundedQueues_Tests, ConcurrentBoundedQueueSingleThread) {
    using namespace Sgcl;
    ConcurrentBoundedQueue<int> q(3);   // rounded up to 4
    EXPECT_EQ(q.Capacity(), 4u);
    EXPECT_TRUE(q.IsEmpty());
    EXPECT_FALSE(q.IsFull());
    EXPECT_TRUE(q.TryEnqueue(1));
    int two = 2;
    EXPECT_TRUE(q.TryEnqueue(two));
    EXPECT_TRUE(q.TryEmplace(3));
    q.Enqueue(4);
    EXPECT_TRUE(q.IsFull());
    EXPECT_EQ(q.Count(), 4u);
    EXPECT_FALSE(q.TryEnqueue(5));
    EXPECT_EQ(q.Dequeue(), 1);
    EXPECT_EQ(*q.TryDequeue(), 2);
    EXPECT_EQ(q.Inner().pop(), 3);
    EXPECT_EQ(q.Dequeue(), 4);
    EXPECT_EQ(q.TryDequeue(), None);
    EXPECT_TRUE(q.IsEmpty());
    ConcurrentBoundedQueue<String> s(2);
    EXPECT_TRUE(s.TryEmplace("abc"));
    EXPECT_EQ(*s.TryDequeue(), "abc");
}

TEST(Sgcl_BoundedQueues_Tests, SpscQueueSingleThread) {
    using namespace Sgcl;
    SpscQueue<int> q(3);   // rounded up to 4
    EXPECT_EQ(q.Capacity(), 4u);
    EXPECT_TRUE(q.IsEmpty());
    EXPECT_FALSE(q.IsFull());
    EXPECT_TRUE(q.TryEnqueue(1));
    int two = 2;
    EXPECT_TRUE(q.TryEnqueue(two));
    EXPECT_TRUE(q.TryEmplace(3));
    q.Enqueue(4);
    EXPECT_TRUE(q.IsFull());
    EXPECT_EQ(q.Count(), 4u);
    EXPECT_FALSE(q.TryEnqueue(5));
    EXPECT_EQ(q.Dequeue(), 1);
    EXPECT_EQ(*q.TryDequeue(), 2);
    EXPECT_EQ(q.Inner().pop(), 3);
    EXPECT_EQ(q.Dequeue(), 4);
    EXPECT_EQ(q.TryDequeue(), None);
    EXPECT_TRUE(q.IsEmpty());
}

TEST(Sgcl_BoundedQueues_Tests, ElementsDestroyedWithTheQueue) {
    using namespace Sgcl;
    const int before = Item::alive.load();
    {
        ConcurrentBoundedQueue<Item> a(4);
        SpscQueue<Item> b(4);
        a.Enqueue(Item(1));
        b.Enqueue(Item(2));
        EXPECT_TRUE(a.TryEmplace(3) && b.TryEmplace(4));
        EXPECT_EQ(Item::alive.load(), before + 4);
        EXPECT_EQ(a.Dequeue().value, 1);
        EXPECT_EQ(b.Dequeue().value, 2);
        EXPECT_EQ(Item::alive.load(), before + 2);
    }
    EXPECT_EQ(Item::alive.load(), before);
}

TEST(Sgcl_BoundedQueues_Tests, PtrsInsideAreTraced) {
    using namespace Sgcl;
    settle();
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        struct Holder {
            ConcurrentBoundedQueue<Ptr<Item>> mpmc{8};
            SpscQueue<Ptr<Item>> spsc{8};
        };
        Ptr h = Make<Holder>();
        for (int i : Range(6)) {
            h->mpmc.Enqueue(Make<Item>(i));
            h->spsc.Enqueue(Make<Item>(i));
        }
        settle();
        EXPECT_EQ(collector::get_live_object_count(), before + 1u + 2u + 12u);   // the holder, two buffers, the items
        for (int i : Range(6)) {
            EXPECT_EQ(h->mpmc.Dequeue()->value, i);
            EXPECT_EQ((*h->spsc.TryDequeue())->value, i);
        }
        settle();
        EXPECT_EQ(collector::get_live_object_count(), before + 3u);
        h->mpmc.Enqueue(Make<Item>(7));   // left in the rings: destroyed with the holder
        h->spsc.Enqueue(Make<Item>(7));
    });
    settle();
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(Sgcl_BoundedQueues_Tests, ProducersAndConsumers) {
    using namespace Sgcl;
    const int producers = 3, consumers = 3, n = 10000;
    std::vector<sgcl::atomic<int>> seen(size_t(producers * n));
    off_frame([&] {
        ConcurrentBoundedQueue<Ptr<Item>> q(16);
        std::vector<std::thread> ws;
        for (int t : Range(producers)) {
            ws.emplace_back([&, t] {
                for (int i : Range(n)) {
                    q.Enqueue(Make<Item>(t * n + i));
                }
            });
        }
        for (int t : Range(consumers)) {
            (void)t;
            ws.emplace_back([&] {
                for (int i : Range(n)) {
                    (void)i;
                    Ptr v = q.Dequeue();
                    ++seen[size_t(v->value)];
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_TRUE(q.IsEmpty());
    });
    for (auto& c : seen) {
        ASSERT_EQ(c.load(), 1);
    }
}

TEST(Sgcl_BoundedQueues_Tests, SpscBetweenTwoThreads) {
    using namespace Sgcl;
    const int n = 100000;
    off_frame([&] {
        SpscQueue<int> q(16);
        std::thread producer([&] {
            for (int i : Range(n)) {
                q.Enqueue(i);
            }
        });
        for (int i : Range(n)) {
            ASSERT_EQ(q.Dequeue(), i);
        }
        producer.join();
        EXPECT_TRUE(q.IsEmpty());
    });
}
