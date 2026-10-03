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

TEST(SpscQueue_Test, PushPopOrder) {
    sgcl::concurrent::spsc_queue<int> q(4);
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

TEST(SpscQueue_Test, FullAndCapacityRounding) {
    sgcl::concurrent::spsc_queue<int> q(5);   // rounded up to 8
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
    EXPECT_EQ(sgcl::concurrent::spsc_queue<int>(1).capacity(), 1u);
    EXPECT_EQ(sgcl::concurrent::spsc_queue<int>(0).capacity(), 1u);
    EXPECT_EQ(sgcl::concurrent::spsc_queue<int>(1000).capacity(), 1024u);
    sgcl::concurrent::spsc_queue<int> one(1);
    EXPECT_TRUE(one.try_push(1));
    EXPECT_FALSE(one.try_push(2));
    EXPECT_EQ(one.pop(), 1);
    EXPECT_TRUE(one.try_push(2));
    EXPECT_EQ(one.pop(), 2);
}

TEST(SpscQueue_Test, ManyLaps) {
    sgcl::concurrent::spsc_queue<int> q(4);
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

TEST(SpscQueue_Test, MoveOnlyElements) {
    sgcl::concurrent::spsc_queue<std::unique_ptr<int>> q(2);
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

TEST(SpscQueue_Test, StringsEmplaced) {
    sgcl::concurrent::spsc_queue<std::string> q(4);
    EXPECT_TRUE(q.try_emplace(3, 'x'));
    EXPECT_TRUE(q.try_emplace("abc"));
    std::string s = "def";
    EXPECT_TRUE(q.try_push(s));
    EXPECT_EQ(s, "def");
    EXPECT_EQ(*q.try_pop(), "xxx");
    EXPECT_EQ(*q.try_pop(), "abc");
    EXPECT_EQ(q.pop(), "def");
}

TEST(SpscQueue_Test, ElementDestroyedByPopAndByDestructor) {
    const size_t before = Int::counter;
    {
        sgcl::concurrent::spsc_queue<Int> q(4);
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

// A constructor that throws leaves the queue as it was
TEST(SpscQueue_Test, ConstructorThrows) {
    sgcl::concurrent::spsc_queue<Throwing> q(4);
    EXPECT_TRUE(q.try_emplace(1));
    EXPECT_THROW(q.try_emplace(13), std::runtime_error);
    EXPECT_EQ(q.size(), 1u);
    EXPECT_TRUE(q.try_emplace(2));
    EXPECT_EQ(q.try_pop()->value, 1);
    EXPECT_EQ(q.pop().value, 2);
    EXPECT_TRUE(q.empty());
}

TEST(SpscQueue_Test, ElementsHoldingTrackedPtrs) {
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent::spsc_queue<tracked_ptr<Baz>> q(16);
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

TEST(SpscQueue_Test, QueueInsideManagedObject) {
    struct Holder {
        sgcl::concurrent::spsc_queue<tracked_ptr<Baz>> q{8};
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

TEST(SpscQueue_Test, BlockingPushAndPopBetweenThreads) {
    const int n = 50000;
    off_frame([&] {
        sgcl::concurrent::spsc_queue<int> q(8);
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

// One producer, one consumer, both spinning on the try operations, the
// elements managed objects, cycles forced meanwhile: every value once
// and in order, nothing lost and nothing kept. The elements have no
// destructor: one with a destructor is destroyed on the collector's
// thread, ordered after its last use by the cycle that found it
// unreferenced, which the thread sanitizer cannot see (tracked_ptr.h:
// the destructor).
TEST(SpscQueue_Test, ProducerConsumerStress) {
    struct Value {
        int value;
    };
    const int n = 200000;
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent::spsc_queue<tracked_ptr<Value>> q(32);
        std::thread producer([&] {
            for (int i = 0; i < n; ++i) {
                tracked_ptr<Value> v = make_tracked<Value>(i);
                if (i % 2) {
                    q.push(std::move(v));
                } else {
                    while (!q.try_push(v)) {
                        std::this_thread::yield();
                    }
                }
                if (i % 20000 == 0) {
                    collector::force_collect();
                }
            }
        });
        for (int i = 0; i < n; ++i) {
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
            ASSERT_EQ(v->value, i);
        }
        producer.join();
        EXPECT_TRUE(q.empty());
        EXPECT_FALSE(q.try_pop());
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), before);
}

// Boundaries (DESIGN 408)

// A capacity past the largest ring a buffer can hold (its rounding up was
// undefined past the largest power of two, and the bytes of the buffer
// wrapped around) is a buffer no memory gives: the program ends as at
// any refused managed allocation
TEST(SpscQueue_Test, ACapacityNoMemoryHoldsEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    EXPECT_DEATH(sgcl::concurrent::spsc_queue<int>{SIZE_MAX}, "sgcl: out of managed memory");
    EXPECT_DEATH(sgcl::concurrent::spsc_queue<int>{(size_t(1) << 63) + 1}, "sgcl: out of managed memory");
    EXPECT_DEATH(sgcl::concurrent::spsc_queue<int>{size_t(1) << 62}, "sgcl: out of managed memory");   // 2^66 bytes: wrapped to 0
}

// An element whose move throws: try_pop leaves it in its cell and takes
// it again next time; pop loses it when the move out of the optional
// throws (try_pop.md, pop.md); a push whose copy throws leaves the queue
// as it was (try_push.md, push.md)
TEST(SpscQueue_Test, ACopyOrAMoveThatThrows) {
    using throwing::Val;
    throwing::Disarm disarm;
    sgcl::concurrent::spsc_queue<Val> q(2);
    Val one(1);
    EXPECT_TRUE(q.try_push(one));
    throwing::countdown.copy = 1;
    EXPECT_THROW(q.try_push(one), throwing::Error);
    throwing::countdown.copy = 1;
    EXPECT_THROW(q.push(one), throwing::Error);
    EXPECT_EQ(q.size(), 1u);
    EXPECT_TRUE(q.try_push(Val(2)));
    EXPECT_TRUE(q.full());
    throwing::countdown.move = 1;
    EXPECT_THROW(q.try_pop(), throwing::Error);
    EXPECT_EQ(q.size(), 2u);   // still in its cell
    EXPECT_EQ(q.try_pop()->v, 1);
    throwing::countdown.move = 1;
    EXPECT_THROW(q.pop(), throwing::Error);   // the move out of the cell: the element stays
    EXPECT_EQ(q.size(), 1u);
    throwing::countdown = {};
    EXPECT_EQ(q.pop().v, 2);
    EXPECT_TRUE(q.empty());
}

// A ring of one cell between a producer and a consumer that both wait:
// every push and every pop at a boundary, every value once and in order
TEST(SpscQueue_Test, ARingOfOneBetweenWaitingSides) {
    const int n = 20000;
    sgcl::concurrent::spsc_queue<int> q(0);   // rounded up to one cell
    ASSERT_EQ(q.capacity(), 1u);
    together::run(2, [&](int i) {
        if (i == 0) {
            for (int k = 0; k < n; ++k) {
                q.push(k);
            }
        } else {
            for (int k = 0; k < n; ++k) {
                ASSERT_EQ(q.pop(), k);
            }
        }
    });
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.try_pop());
}

// A third thread reading size, empty and full while the two sides run:
// a value from 0 to the capacity, never past it
TEST(SpscQueue_Test, SizeFromAThirdThreadStaysInBounds) {
    const int n = 20000;
    sgcl::concurrent::spsc_queue<int> q(4);
    std::atomic<bool> stop = {false}, out = {false};
    together::run(3, [&](int i) {
        if (i == 0) {
            for (int k = 0; k < n; ++k) {
                q.push(k);
            }
        } else if (i == 1) {
            for (int k = 0; k < n; ++k) {
                (void)q.pop();
            }
            stop = true;
        } else {
            while (!stop) {
                if (q.size() > q.capacity()) {
                    out = true;
                }
            }
        }
    });
    EXPECT_FALSE(out.load());
}
