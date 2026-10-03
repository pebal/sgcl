//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/throwing.h"
#include "tests/concurrent/together.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    // An element whose constructor throws for one value
    struct Throwing {
        explicit Throwing(int v)
        : value(v) {
            if (v == 13) {
                throw std::runtime_error("13");
            }
        }

        int value;
    };
}

TEST(ConcurrentBoundedQueue_Test, PushPopOrder) {
    sgcl::concurrent::bounded_queue<int> q(4);
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.full());
    EXPECT_EQ(q.size(), 0u);
    EXPECT_EQ(q.capacity(), 4u);
    EXPECT_FALSE(q.try_pop());
    EXPECT_TRUE(q.try_push(1));
    int two = 2;
    EXPECT_TRUE(q.try_push(two));
    EXPECT_TRUE(q.try_emplace(3));
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

TEST(ConcurrentBoundedQueue_Test, FullAndCapacityRounding) {
    sgcl::concurrent::bounded_queue<int> q(5);   // rounded up to 8
    EXPECT_EQ(q.capacity(), 8u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(q.try_push(i));
    }
    EXPECT_TRUE(q.full());
    EXPECT_EQ(q.size(), 8u);
    EXPECT_FALSE(q.try_push(8));
    EXPECT_FALSE(q.try_emplace(8));
    EXPECT_EQ(*q.try_pop(), 0);
    EXPECT_FALSE(q.full());
    EXPECT_TRUE(q.try_push(8));   // the cell freed, taken on the next lap
    for (int i = 1; i <= 8; ++i) {
        EXPECT_EQ(*q.try_pop(), i);
    }
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(sgcl::concurrent::bounded_queue<int>(2).capacity(), 2u);
    EXPECT_EQ(sgcl::concurrent::bounded_queue<int>(1).capacity(), 2u);   // at least two: the ring's minimum
    EXPECT_EQ(sgcl::concurrent::bounded_queue<int>(0).capacity(), 2u);
    EXPECT_EQ(sgcl::concurrent::bounded_queue<int>(1000).capacity(), 1024u);
    sgcl::concurrent::bounded_queue<int> two(2);
    EXPECT_TRUE(two.try_push(1));
    EXPECT_TRUE(two.try_push(2));
    EXPECT_FALSE(two.try_push(3));
    EXPECT_EQ(two.pop(), 1);
    EXPECT_TRUE(two.try_push(3));
    EXPECT_EQ(two.pop(), 2);
    EXPECT_EQ(two.pop(), 3);
    EXPECT_TRUE(two.empty());
}

TEST(ConcurrentBoundedQueue_Test, ManyLaps) {
    sgcl::concurrent::bounded_queue<int> q(4);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(q.try_push(i));
        if (i % 3 == 2) {
            EXPECT_EQ(*q.try_pop(), i - 2);
            EXPECT_EQ(*q.try_pop(), i - 1);
            EXPECT_EQ(*q.try_pop(), i);
        }
    }
    EXPECT_EQ(*q.try_pop(), 999);
    EXPECT_TRUE(q.empty());
}

TEST(ConcurrentBoundedQueue_Test, MoveOnlyElements) {
    sgcl::concurrent::bounded_queue<std::unique_ptr<int>> q(2);
    EXPECT_TRUE(q.try_push(std::make_unique<int>(1)));
    EXPECT_TRUE(q.try_emplace(new int(2)));
    EXPECT_FALSE(q.try_push(std::make_unique<int>(3)));
    auto a = q.try_pop();
    ASSERT_TRUE(a && *a);
    EXPECT_EQ(**a, 1);
    q.push(std::make_unique<int>(3));
    EXPECT_EQ(*q.pop(), 2);
    EXPECT_EQ(**q.try_pop(), 3);
}

TEST(ConcurrentBoundedQueue_Test, StringsEmplaced) {
    sgcl::concurrent::bounded_queue<std::string> q(4);
    EXPECT_TRUE(q.try_emplace(3, 'x'));
    EXPECT_TRUE(q.try_emplace("abc"));
    std::string s = "def";
    EXPECT_TRUE(q.try_push(s));
    EXPECT_EQ(s, "def");
    EXPECT_EQ(*q.try_pop(), "xxx");
    EXPECT_EQ(*q.try_pop(), "abc");
    EXPECT_EQ(q.pop(), "def");
}

TEST(ConcurrentBoundedQueue_Test, ElementDestroyedByPopAndByDestructor) {
    const size_t before = Int::counter;
    {
        sgcl::concurrent::bounded_queue<Int> q(4);
        EXPECT_TRUE(q.try_push(Int(1)));
        EXPECT_TRUE(q.try_emplace(2));
        EXPECT_TRUE(q.try_push(Int(3)));
        EXPECT_EQ(Int::counter, before + 3);
        {
            auto v = q.try_pop();
            EXPECT_EQ(*v, 1);
            EXPECT_EQ(Int::counter, before + 3);   // moved out of the cell, alive in v
        }
        EXPECT_EQ(Int::counter, before + 2);
        EXPECT_TRUE(q.try_push(Int(4)));
        EXPECT_TRUE(q.try_push(Int(5)));   // the cell of 1, on the second lap
        EXPECT_EQ(Int::counter, before + 4);
    }
    EXPECT_EQ(Int::counter, before);   // the four left in the ring destroyed with the queue
}

// A constructor that throws in a cell the producer won: the cell is
// published empty and skipped by a consumer, the queue intact
TEST(ConcurrentBoundedQueue_Test, ConstructorThrowsInsideTheRing) {
    sgcl::concurrent::bounded_queue<Throwing> q(4);
    EXPECT_TRUE(q.try_emplace(1));
    EXPECT_THROW(q.try_emplace(13), std::runtime_error);
    EXPECT_TRUE(q.try_emplace(2));
    EXPECT_EQ(q.size(), 3u);   // the empty cell counts until a consumer passes it
    EXPECT_EQ(q.try_pop()->value, 1);
    EXPECT_EQ(q.try_pop()->value, 2);   // the empty cell skipped
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.try_pop());
    for (int lap = 0; lap < 3; ++lap) {   // empty cells on every lap, including a ring of them
        for (int i = 0; i < 4; ++i) {
            EXPECT_THROW(q.try_emplace(13), std::runtime_error);
        }
        EXPECT_TRUE(q.full());
        EXPECT_FALSE(q.try_emplace(1));
        EXPECT_FALSE(q.try_pop());   // four empty cells passed
        EXPECT_TRUE(q.empty());
    }
    std::thread consumer([&] {
        EXPECT_EQ(q.pop().value, 3);   // wakes on the empty cell, waits on, wakes on the element
    });
    EXPECT_THROW(q.try_emplace(13), std::runtime_error);
    EXPECT_TRUE(q.try_emplace(3));
    consumer.join();
}

TEST(ConcurrentBoundedQueue_Test, ElementsHoldingTrackedPtrs) {
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent::bounded_queue<tracked_ptr<Baz>> q(16);
        const size_t with_buffer = collector::get_live_object_count();
        off_frame([&] {
            for (int i = 0; i < 10; ++i) {
                EXPECT_TRUE(q.try_push(make_tracked<Baz>(i)));
            }
        });
        collector::force_collect(true);
        EXPECT_EQ(collector::get_live_object_count(), with_buffer + 10u);   // held by the cells: traced
        off_frame([&] {
            for (int i = 0; i < 10; ++i) {
                auto v = q.try_pop();
                ASSERT_TRUE(v);
                EXPECT_EQ((*v)->value, i);
            }
        });
        collector::force_collect(true);
        EXPECT_EQ(collector::get_live_object_count(), with_buffer);   // popped and dropped: collected
        off_frame([&] {
            for (int i = 0; i < 16; ++i) {
                EXPECT_TRUE(q.try_push(make_tracked<Baz>(i)));   // a full ring, over the lap boundary
            }
        });
        collector::force_collect(true);
        EXPECT_EQ(collector::get_live_object_count(), with_buffer + 16u);
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before);   // the queue and its elements gone
}

TEST(ConcurrentBoundedQueue_Test, QueueInsideManagedObject) {
    struct Holder {
        sgcl::concurrent::bounded_queue<tracked_ptr<Baz>> q{8};
    };
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        tracked_ptr h = make_tracked<Holder>();
        h->q.push(make_tracked<Baz>(1));
        h->q.push(make_tracked<Baz>(2));
        collector::force_collect(true);
        EXPECT_EQ((*h->q.try_pop())->value, 1);
        EXPECT_EQ(h->q.size(), 1u);
        EXPECT_EQ(h->q.pop()->value, 2);
        h->q.push(make_tracked<Baz>(3));   // left in the ring: destroyed with the holder
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(ConcurrentBoundedQueue_Test, BlockingPushAndPopBetweenThreads) {
    const int n = 50000;
    off_frame([&] {
        sgcl::concurrent::bounded_queue<int> q(8);
        std::thread producer([&] {
            for (int i = 0; i < n; ++i) {
                q.push(i);   // waits while the ring is full
            }
        });
        for (int i = 0; i < n; ++i) {
            ASSERT_EQ(q.pop(), i);   // waits while the ring is empty
        }
        producer.join();
        EXPECT_TRUE(q.empty());
    });
}

// Several producers and consumers on a small ring, every value delivered
// exactly once, in each producer's order, with cycles forced meanwhile.
// The elements are managed objects without a destructor: one with a
// destructor is destroyed on the collector's thread, ordered after its
// last use by the cycle that found it unreferenced, which the thread
// sanitizer cannot see (tracked_ptr.h: the destructor).
TEST(ConcurrentBoundedQueue_Test, ProducersAndConsumersStress) {
    struct Value {
        int value;
    };
    const int producers = 4;
    const int consumers = 4;
    const int n = 20000;
    std::vector<sgcl::atomic<int>> seen(size_t(producers * n));
    sgcl::atomic<bool> out_of_order = {false};
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent::bounded_queue<tracked_ptr<Value>> q(64);
        std::vector<std::thread> ws;
        for (int t = 0; t < producers; ++t) {
            ws.emplace_back([&, t] {
                for (int i = 0; i < n; ++i) {
                    if (i % 2) {
                        q.push(make_tracked<Value>(t * n + i));
                    } else {
                        while (!q.try_push(make_tracked<Value>(t * n + i))) {
                            std::this_thread::yield();
                        }
                    }
                    if (t == 0 && i % 5000 == 0) {
                        collector::force_collect();
                    }
                }
            });
        }
        for (int t = 0; t < consumers; ++t) {
            ws.emplace_back([&] {
                std::vector<int> last(size_t(producers), -1);
                for (int i = 0; i < n * producers / consumers; ++i) {
                    tracked_ptr<Value> v;
                    if (i % 2) {
                        v = q.pop();
                    } else {
                        for (;;) {
                            if (auto p = q.try_pop()) {
                                v = *p;
                                break;
                            }
                            std::this_thread::yield();
                        }
                    }
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
        EXPECT_FALSE(q.try_pop());
    });
    EXPECT_FALSE(out_of_order.load());
    for (auto& c : seen) {
        ASSERT_EQ(c.load(), 1);
    }
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before);
}

// Every thread pushes and pops on a ring of two cells: the queue never
// loses a cell to a reserved-and-not-published state
TEST(ConcurrentBoundedQueue_Test, MixedPushPopOnTwoCells) {
    const int threads = 8;
    const int n = 10000;
    off_frame([&] {
        sgcl::concurrent::bounded_queue<int> q(2);
        sgcl::atomic<long> pushed = {0}, popped = {0};
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&] {
                for (int i = 0; i < n; ++i) {
                    if (q.try_push(i)) {
                        ++pushed;
                    }
                    if (q.try_pop()) {
                        ++popped;
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_EQ(size_t(pushed.load()), size_t(popped.load()) + q.size());
        while (q.try_pop()) {
        }
        q.push(-1);   // usable: the ring is intact
        EXPECT_EQ(*q.try_pop(), -1);
    });
}

// Boundaries (DESIGN 408)

// A capacity past the largest ring a buffer can hold (its rounding up was
// undefined past the largest power of two, and the bytes of the buffer
// wrapped around) is a buffer no memory gives: the program ends as at
// any refused managed allocation
TEST(ConcurrentBoundedQueue_Test, ACapacityNoMemoryHoldsEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    EXPECT_DEATH(sgcl::concurrent::bounded_queue<int>{SIZE_MAX}, "sgcl: out of managed memory");
    EXPECT_DEATH(sgcl::concurrent::bounded_queue<int>{(size_t(1) << 63) + 1}, "sgcl: out of managed memory");
    EXPECT_DEATH(sgcl::concurrent::bounded_queue<int>{size_t(1) << 62}, "sgcl: out of managed memory");   // 2^66 bytes: wrapped to 0
}

// Elements whose copy or move throws: a push whose copy throws publishes
// its cell empty, passed over by the consumers (try_push.md); a pop whose
// move throws loses the element, destroyed in its cell, and the ring goes
// on in order (try_pop.md, pop.md)
TEST(ConcurrentBoundedQueue_Test, ACopyOrAMoveThatThrows) {
    using throwing::Val;
    throwing::Disarm disarm;
    sgcl::concurrent::bounded_queue<Val> q(4);
    Val one(1);
    EXPECT_TRUE(q.try_push(one));
    throwing::countdown.copy = 1;
    EXPECT_THROW(q.try_push(one), throwing::Error);
    throwing::countdown.copy = 1;
    EXPECT_THROW(q.push(one), throwing::Error);
    EXPECT_TRUE(q.try_push(Val(2)));
    EXPECT_EQ(q.size(), 4u);   // the two empty cells count until passed
    EXPECT_TRUE(q.full());
    throwing::countdown.move = 1;
    EXPECT_THROW(q.try_pop(), throwing::Error);   // 1, lost
    EXPECT_EQ(q.size(), 3u);
    EXPECT_EQ(q.try_pop()->v, 2);   // the empty cells passed
    EXPECT_TRUE(q.empty());
    for (int i = 3; i < 7; ++i) {
        EXPECT_TRUE(q.try_push(Val(i)));
    }
    throwing::countdown.move = 2;   // the move out of the cell, then the one into pop's value
    EXPECT_THROW(q.pop(), throwing::Error);
    throwing::countdown = {};
    EXPECT_EQ(q.pop().v, 4);
    EXPECT_EQ(q.pop().v, 5);
    EXPECT_EQ(q.pop().v, 6);
    EXPECT_TRUE(q.empty());
}

// Threads at the boundaries of the ring, many rounds: at the empty ring
// every try_pop finds nothing and one element goes to one of them; at the
// full ring every try_push fails, and the one cell a pop frees goes to
// exactly one of them
TEST(ConcurrentBoundedQueue_Test, ThreadsAtTheEmptyAndTheFullRing) {
    sgcl::concurrent::bounded_queue<int> q(2);
    for (int round = 0; round < together::Rounds; ++round) {
        std::atomic<int> got = {0}, refused = {0}, pushed = {0};
        together::run(4, [&](int) {
            got += q.try_pop().has_value();
        });
        EXPECT_EQ(got.load(), 0);
        EXPECT_TRUE(q.try_push(round));
        together::run(4, [&](int) {
            if (auto v = q.try_pop()) {
                EXPECT_EQ(*v, round);
                ++got;
            }
        });
        EXPECT_EQ(got.load(), 1);
        EXPECT_TRUE(q.empty());
        EXPECT_TRUE(q.try_push(1));
        EXPECT_TRUE(q.try_push(2));
        together::run(4, [&](int i) {
            refused += !q.try_emplace(i);
        });
        EXPECT_EQ(refused.load(), 4);
        EXPECT_EQ(*q.try_pop(), 1);
        together::run(4, [&](int i) {
            pushed += q.try_push(10 + i);
        });
        EXPECT_EQ(pushed.load(), 1);
        EXPECT_TRUE(q.full());
        EXPECT_EQ(*q.try_pop(), 2);
        EXPECT_GE(*q.try_pop(), 10);
        EXPECT_TRUE(q.empty());
    }
}

// Producers waiting in push at a full ring of two and consumers waiting
// in pop at the empty one, released by each other: every element once
TEST(ConcurrentBoundedQueue_Test, WaitersAtBothEnds) {
    const int n = 2000;
    sgcl::concurrent::bounded_queue<int> q(1);   // a ring of two: every push and pop at a boundary
    std::vector<std::atomic<int>> seen(size_t(2 * n));
    together::run(4, [&](int i) {
        if (i < 2) {
            for (int k = 0; k < n; ++k) {
                q.push(i * n + k);
            }
        } else {
            for (int k = 0; k < n; ++k) {
                ++seen[size_t(q.pop())];
            }
        }
    });
    for (auto& c : seen) {
        ASSERT_EQ(c.load(), 1);
    }
    EXPECT_TRUE(q.empty());
}
