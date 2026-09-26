//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "sgcl/core/deque.h"
#include "sgcl/core/queue.h"
#include "sgcl/core/stack.h"
#include "tests/types.h"

#include <functional>
#include <queue>
#include <random>
#include <stack>
#include <string>
#include <vector>

namespace {
    struct MoveOnly {
        int value;

        explicit MoveOnly(int v) noexcept
        : value(v) {
        }

        MoveOnly(MoveOnly&& other) noexcept
        : value(other.value) {
            other.value = -1;
        }

        MoveOnly& operator=(MoveOnly&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;

        bool operator<(const MoveOnly& other) const noexcept {
            return value < other.value;
        }
    };

    struct ByValue {
        bool operator()(const Int& a, const Int& b) const noexcept {
            return int(a) < int(b);
        }
    };

    template<class Adapter>
    std::vector<int> drain(Adapter a) {
        std::vector<int> values;
        while (!a.empty()) {
            if constexpr (requires { a.top(); }) {
                values.push_back(int(a.top()));
            } else {
                values.push_back(int(a.front()));
            }
            a.pop();
        }
        return values;
    }
}

TEST(Adapters_Test, StackBasic) {
    sgcl::stack<Int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    s.push(1);
    Int two(2);
    s.push(two);
    auto& top = s.emplace(3);
    EXPECT_EQ(top, 3);
    EXPECT_EQ(&top, &s.top());
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.top(), 3);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(collector::get_live_object_count(), 2u);   // the deque's map and block
    s.top() = 4;
    s.pop();
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(s.top(), 2);
    const auto& cs = s;
    EXPECT_EQ(cs.top(), 2);
    EXPECT_EQ(cs.size(), 2u);
    s.pop();
    s.pop();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(Int::counter, 1u);
    for (int i = 0; i < 10000; ++i) {
        s.push(i);
    }
    std::vector<int> expected;
    for (int i = 9999; i >= 0; --i) {
        expected.push_back(i);
    }
    EXPECT_EQ(drain(s), expected);
}

TEST(Adapters_Test, StackOtherContainers) {
    sgcl::stack<int, sgcl::vector<int>> v;
    sgcl::stack<int, sgcl::list<int>> l;
    for (int i = 0; i < 5; ++i) {
        v.push(i);
        l.push(i);
    }
    EXPECT_EQ(v.top(), 4);
    EXPECT_EQ(l.top(), 4);
    v.pop();
    l.pop();
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(l.size(), 4u);
    std::vector<int> expected = {3, 2, 1, 0};
    EXPECT_EQ(drain(v), expected);
    EXPECT_EQ(drain(l), expected);
}

TEST(Adapters_Test, StackConstructors) {
    sgcl::deque<int> d({1, 2, 3});
    sgcl::stack<int> from_copy(d);
    EXPECT_EQ(from_copy.top(), 3);
    EXPECT_EQ(d.size(), 3u);
    sgcl::stack<int> from_move(std::move(d));
    EXPECT_EQ(from_move.size(), 3u);
    EXPECT_TRUE(d.empty());
    std::vector<int> range = {4, 5, 6};
    sgcl::stack<int> from_range(range.begin(), range.end());
    EXPECT_EQ(from_range.top(), 6);
    sgcl::stack<int> copy(from_range);
    EXPECT_EQ(copy, from_range);
    sgcl::stack<int> moved(std::move(copy));
    EXPECT_EQ(moved, from_range);
    EXPECT_TRUE(copy.empty());
    off_frame([&] {   // the buffers dropped by the assignments must not linger in this frame
        copy = moved;
        EXPECT_EQ(copy, from_range);
        moved = std::move(copy);
        EXPECT_EQ(moved, from_range);
    });
    EXPECT_EQ(collector::get_live_object_count(), 8u);   // four stacks of three, a map and a block each
}

TEST(Adapters_Test, StackComparisonsAndSwap) {
    std::vector<int> range = {1, 2, 3};
    sgcl::stack<int> a(range.begin(), range.end());
    sgcl::stack<int> b(range.begin(), range.end());
    sgcl::stack<int> c(range.begin(), range.end() - 1);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(c < a);
    EXPECT_TRUE(a > c);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(c <= a);
    EXPECT_TRUE(a >= c);
    EXPECT_EQ(a <=> b, std::strong_ordering::equal);
    EXPECT_EQ(c <=> a, std::strong_ordering::less);
    a.swap(c);
    EXPECT_EQ(a.size(), 2u);
    EXPECT_EQ(c.size(), 3u);
    swap(a, c);
    EXPECT_EQ(a, b);
}

TEST(Adapters_Test, QueueBasic) {
    sgcl::queue<Int> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
    q.push(1);
    Int two(2);
    q.push(two);
    auto& back = q.emplace(3);
    EXPECT_EQ(back, 3);
    EXPECT_EQ(&back, &q.back());
    EXPECT_EQ(q.size(), 3u);
    EXPECT_EQ(q.front(), 1);
    EXPECT_EQ(q.back(), 3);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    q.front() = 0;
    q.back() = 4;
    const auto& cq = q;
    EXPECT_EQ(cq.front(), 0);
    EXPECT_EQ(cq.back(), 4);
    q.pop();
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(q.front(), 2);
    std::vector<int> expected = {2, 4};
    EXPECT_EQ(drain(q), expected);
    q.pop();
    q.pop();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(Int::counter, 1u);
    for (int i = 0; i < 10000; ++i) {   // a moving window over the deque
        q.push(i);
        if (i >= 100) {
            EXPECT_EQ(q.front(), i - 100);
            q.pop();
        }
    }
    EXPECT_EQ(q.size(), 100u);
    EXPECT_EQ(Int::counter, 101u);
}

TEST(Adapters_Test, QueueOtherContainersAndConstructors) {
    sgcl::queue<int, sgcl::list<int>> l;
    for (int i = 0; i < 5; ++i) {
        l.push(i);
    }
    std::vector<int> expected = {0, 1, 2, 3, 4};
    EXPECT_EQ(drain(l), expected);

    sgcl::deque<int> d({1, 2, 3});
    sgcl::queue<int> from_copy(d);
    EXPECT_EQ(from_copy.front(), 1);
    EXPECT_EQ(d.size(), 3u);
    sgcl::queue<int> from_move(std::move(d));
    EXPECT_EQ(from_move.back(), 3);
    EXPECT_TRUE(d.empty());
    sgcl::queue<int> from_range(expected.begin(), expected.end());
    EXPECT_EQ(drain(from_range), expected);
    sgcl::queue<int> copy(from_range);
    EXPECT_EQ(copy, from_range);
    sgcl::queue<int> moved(std::move(copy));
    EXPECT_EQ(moved, from_range);
    EXPECT_TRUE(copy.empty());
    copy = moved;
    moved = std::move(copy);
    EXPECT_EQ(moved, from_range);
}

TEST(Adapters_Test, QueueComparisonsAndSwap) {
    std::vector<int> range = {1, 2, 3};
    sgcl::queue<int> a(range.begin(), range.end());
    sgcl::queue<int> b(range.begin(), range.end());
    sgcl::queue<int> c(range.begin() + 1, range.end());
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(c > a);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(c >= a);
    EXPECT_EQ(a <=> c, std::strong_ordering::less);
    a.swap(c);
    EXPECT_EQ(a.size(), 2u);
    swap(a, c);
    EXPECT_EQ(a, b);
}

TEST(Adapters_Test, PriorityQueueBasic) {
    sgcl::priority_queue<Int> pq;
    EXPECT_TRUE(pq.empty());
    EXPECT_EQ(pq.size(), 0u);
    std::mt19937 rng(9);
    std::vector<int> values;
    for (int i = 0; i < 2000; ++i) {
        int v = int(rng() % 1000);
        values.push_back(v);
        if (i % 3 == 0) {
            pq.push(v);
        } else if (i % 3 == 1) {
            Int value(v);
            pq.push(value);
        } else {
            pq.emplace(v);
        }
    }
    EXPECT_EQ(pq.size(), 2000u);
    // sgcl::vector leaves the elements of an outgrown buffer to the
    // collector: the count runs a cycle first
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the vector's buffer
    EXPECT_EQ(Int::counter, 2000u);
    std::sort(values.begin(), values.end(), std::greater<>());
    EXPECT_EQ(pq.top(), values.front());
    std::vector<int> drained;
    while (!pq.empty()) {
        drained.push_back(pq.top());
        pq.pop();
    }
    EXPECT_EQ(drained, values);
    EXPECT_EQ(Int::counter, 0u);
}

TEST(Adapters_Test, PriorityQueueComparatorsAndContainers) {
    std::vector<int> values = {5, 1, 4, 1, 3, 9, 2};
    sgcl::priority_queue<int, sgcl::vector<int>, std::greater<int>> min_heap;
    for (int v : values) {
        min_heap.push(v);
    }
    std::vector<int> expected = values;
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(drain(min_heap), expected);

    sgcl::priority_queue<int, sgcl::deque<int>> on_deque(values.begin(), values.end());
    std::sort(expected.begin(), expected.end(), std::greater<>());
    EXPECT_EQ(drain(on_deque), expected);
    EXPECT_EQ(on_deque.top(), 9);

    auto comp = [](int a, int b) { return a % 10 < b % 10; };
    sgcl::priority_queue<int, sgcl::deque<int>, decltype(comp)> custom(comp);
    custom.push(21);
    custom.push(15);
    custom.push(39);
    EXPECT_EQ(custom.top(), 39);
    custom.pop();
    EXPECT_EQ(custom.top(), 15);

    sgcl::priority_queue<Int, sgcl::vector<Int>, ByValue> by_value(ByValue(), sgcl::vector<Int>({3, 7, 5}));
    EXPECT_EQ(by_value.top(), 7);
    EXPECT_EQ(Int::counter, 3u);
    sgcl::vector<Int> vec({1, 8});
    sgcl::priority_queue<Int, sgcl::vector<Int>, ByValue> from_copy(ByValue(), vec);
    EXPECT_EQ(from_copy.top(), 8);
    EXPECT_EQ(vec.size(), 2u);
    sgcl::priority_queue<Int, sgcl::vector<Int>, ByValue> with_range(values.begin(), values.end(), ByValue(), vec);
    EXPECT_EQ(with_range.size(), 9u);
    EXPECT_EQ(with_range.top(), 9);
    sgcl::priority_queue<Int, sgcl::vector<Int>, ByValue> with_range_moved(values.begin(), values.end(), ByValue(), std::move(vec));
    EXPECT_EQ(with_range_moved.size(), 9u);
    EXPECT_TRUE(vec.empty());
    sgcl::priority_queue<Int, sgcl::vector<Int>, ByValue> with_comp(values.begin(), values.end(), ByValue());
    EXPECT_EQ(with_comp.top(), 9);
    EXPECT_EQ(drain(with_range), drain(with_range_moved));

    sgcl::priority_queue<int> copy(on_deque.size() ? sgcl::priority_queue<int>() : sgcl::priority_queue<int>());
    copy.push(1);
    copy.push(2);
    sgcl::priority_queue<int> copied(copy);
    EXPECT_EQ(copied.top(), 2);
    EXPECT_EQ(copy.size(), 2u);
    sgcl::priority_queue<int> moved(std::move(copy));
    EXPECT_EQ(moved.size(), 2u);
    EXPECT_TRUE(copy.empty());
    copy = moved;
    moved = std::move(copy);
    EXPECT_EQ(moved.top(), 2);
    copied.swap(moved);
    swap(copied, moved);
    EXPECT_EQ(copied.size(), 2u);
    EXPECT_EQ(moved.size(), 2u);
}

TEST(Adapters_Test, MoveOnlyElements) {
    sgcl::stack<MoveOnly> s;
    s.push(MoveOnly(1));
    s.emplace(2);
    EXPECT_EQ(s.top().value, 2);
    MoveOnly taken = std::move(s.top());
    s.pop();
    EXPECT_EQ(taken.value, 2);
    EXPECT_EQ(s.top().value, 1);

    sgcl::queue<MoveOnly> q;
    q.push(MoveOnly(1));
    q.emplace(2);
    EXPECT_EQ(q.front().value, 1);
    EXPECT_EQ(q.back().value, 2);
    q.pop();
    EXPECT_EQ(q.front().value, 2);

    sgcl::priority_queue<MoveOnly> pq;
    pq.push(MoveOnly(3));
    pq.emplace(7);
    pq.push(MoveOnly(5));
    EXPECT_EQ(pq.top().value, 7);
    pq.pop();
    EXPECT_EQ(pq.top().value, 5);
    pq.pop();
    EXPECT_EQ(pq.top().value, 3);
}

TEST(Adapters_Test, TrackedElements) {
    sgcl::stack<tracked_ptr<Baz>> s;
    sgcl::queue<Foo> q;
    sgcl::priority_queue<Foo, sgcl::deque<Foo>, decltype([](const Foo& a, const Foo& b) { return a.get_value() < b.get_value(); })> pq;
    off_frame([&] {   // the push fast paths leave block pointers in the frame they inline into
        for (int i = 0; i < 100; ++i) {
            s.push(make_tracked<Baz>(i));
            q.emplace(i);
            pq.emplace(i);
        }
    });
    collector::force_collect(true);
    // the stack: a map, a block, 100 Baz; the queue and the priority queue
    // hold Foo, 64 per block: a map, two blocks, 100 Baz each
    EXPECT_EQ(collector::get_live_object_count(), 2u + 100u + 3u + 100u + 3u + 100u);
    off_frame([&] {
        for (int i = 99; i >= 0; --i) {
            ASSERT_EQ(s.top()->value, i);
            s.pop();
            ASSERT_EQ(q.front().ptr->value, 99 - i);
            q.pop();
            ASSERT_EQ(pq.top().ptr->value, i);
            pq.pop();
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // three empty maps
}

TEST(Adapters_Test, AdaptersInsideManagedObject) {
    struct Holder {
        sgcl::stack<Int> s;
        sgcl::queue<Int> q;
        sgcl::priority_queue<Int, sgcl::vector<Int>, ByValue> pq;
        sgcl::stack<tracked_ptr<Holder>> links;
    };
    off_frame([&] {
        tracked_ptr<Holder> a = make_tracked<Holder>();
        tracked_ptr<Holder> b = make_tracked<Holder>();
        for (int i = 0; i < 1000; ++i) {
            a->s.push(i);
            a->q.push(i);
            b->pq.push(i);
        }
        a->links.push(b);
        b->links.push(a);   // a cycle
        collector::force_collect(true);
        EXPECT_EQ(a->s.top(), 999);
        EXPECT_EQ(a->q.front(), 0);
        EXPECT_EQ(b->pq.top(), 999);
        EXPECT_EQ(a->links.top()->pq.size(), 1000u);
        EXPECT_EQ(Int::counter, 3000u);
    });
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
}

TEST(Adapters_Test, StressAgainstStdAdapters) {
    std::mt19937 rng(13);
    sgcl::stack<Int> s;
    std::stack<int> os;
    sgcl::queue<Int> q;
    std::queue<int> oq;
    sgcl::priority_queue<Int, sgcl::deque<Int>, ByValue> pq;   // a deque destroys eagerly, the instance count is exact
    std::priority_queue<int> opq;
    for (int step = 0; step < 30000; ++step) {
        int op = rng() % 100;
        int value = int(rng() % 1000);
        if (op < 20) {
            s.push(value);
            os.push(value);
            q.push(value);
            oq.push(value);
            pq.push(value);
            opq.push(value);
        } else if (op < 35) {
            if (!os.empty()) {
                ASSERT_EQ(s.top(), os.top());
                s.pop();
                os.pop();
            }
        } else if (op < 50) {
            if (!oq.empty()) {
                ASSERT_EQ(q.front(), oq.front());
                ASSERT_EQ(q.back(), oq.back());
                q.pop();
                oq.pop();
            }
        } else if (op < 65) {
            if (!opq.empty()) {
                ASSERT_EQ(pq.top(), opq.top());
                pq.pop();
                opq.pop();
            }
        } else if (op < 80) {
            s.emplace(value);
            os.emplace(value);
            q.emplace(value);
            oq.emplace(value);
            pq.emplace(value);
            opq.emplace(value);
        } else if (op < 85) {
            collector::force_collect();
        } else {
            ASSERT_EQ(s.size(), os.size());
            ASSERT_EQ(q.size(), oq.size());
            ASSERT_EQ(pq.size(), opq.size());
            ASSERT_EQ(Int::counter, os.size() + oq.size() + opq.size());
        }
    }
    while (!os.empty()) {
        ASSERT_EQ(s.top(), os.top());
        s.pop();
        os.pop();
    }
    while (!oq.empty()) {
        ASSERT_EQ(q.front(), oq.front());
        q.pop();
        oq.pop();
    }
    while (!opq.empty()) {
        ASSERT_EQ(pq.top(), opq.top());
        pq.pop();
        opq.pop();
    }
    EXPECT_TRUE(s.empty() && q.empty() && pq.empty());
    EXPECT_EQ(Int::counter, 0u);
}
