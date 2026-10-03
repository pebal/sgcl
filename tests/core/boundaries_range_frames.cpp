//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of range, counting_iterator, managed_frame and frame_ptr
// (DESIGN 408): ranges at the limits of their integer type (the whole of a
// 64-bit type is 2^64 - 1 numbers, past a signed difference), empty and
// reversed ends, ranges of one; an empty and a moved-from frame_ptr at
// every member, a move into itself, a frame past one page and of a size
// that is not a whole number of words, a coroutine destroyed where it
// waits and one released.
#include "tests/types.h"

#include <array>
#include <coroutine>
#include <cstdint>
#include <list>
#include <utility>
#include <vector>

// range and counting_iterator

TEST(RangeBoundaries_Tests, EmptyAndReversedEnds) {
    range<counting_iterator<int>> d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
    EXPECT_TRUE(range(0).empty());
    EXPECT_TRUE(range(-5).empty());
    EXPECT_EQ(*range(-5).begin(), 0);
    EXPECT_TRUE(range(INT_MIN).empty());
    auto reversed = range(5, 2);
    EXPECT_TRUE(reversed.empty());
    EXPECT_EQ(*reversed.begin(), 5);   // empty at first, not a descent
    EXPECT_EQ(reversed.size(), 0u);
    EXPECT_TRUE(range(INT_MAX, INT_MIN).empty());
    EXPECT_TRUE(range(7, 7).empty());
    EXPECT_FALSE(range(7, 7).contains(7));
    EXPECT_EQ(range(7, 7).index_of(7), size_t(-1));
    EXPECT_FALSE(range(0).find_if([](int) { return true; }));
    EXPECT_TRUE(range(0).all([](int) { return false; }));
    EXPECT_FALSE(range(0).exists([](int) { return true; }));
    EXPECT_EQ(range(0).count_of([](int) { return true; }), 0u);
    std::vector<int> none;
    range empty_vector(none.begin(), none.end());
    EXPECT_TRUE(empty_vector.empty());
    EXPECT_EQ(empty_vector.size(), 0u);
    std::list<int> empty_list;
    EXPECT_EQ(range(empty_list.begin(), empty_list.end()).size(), 0u);
}

TEST(RangeBoundaries_Tests, OneElement) {
    auto one = range(41, 42);
    EXPECT_EQ(one.size(), 1u);
    EXPECT_EQ(one.front(), 41);
    EXPECT_EQ(one.min(), 41);
    EXPECT_EQ(one.max(), 41);
    EXPECT_EQ(one.index_of(41), 0u);
    EXPECT_EQ(one.last_index_of(41), 0u);
    EXPECT_TRUE(range(1) == range(0, 1));
    EXPECT_TRUE(range(1) < range(0, 2));
}

TEST(RangeBoundaries_Tests, AtTheLimitsOfTheType) {
    // The last numbers of a type: the range stops before last, nothing steps past it
    auto top = range(INT_MAX - 3, INT_MAX);
    EXPECT_EQ(top.size(), 3u);
    EXPECT_EQ(top.max(), INT_MAX - 1);
    EXPECT_EQ(top.begin()[2], INT_MAX - 1);
    int count = 0;
    for (int i : top) {
        EXPECT_LT(i, INT_MAX);
        ++count;
    }
    EXPECT_EQ(count, 3);
    auto bottom = range(INT64_MIN, INT64_MIN + 2);
    EXPECT_EQ(bottom.size(), 2u);
    EXPECT_EQ(bottom.front(), INT64_MIN);
    EXPECT_EQ(bottom.min(), INT64_MIN);
    auto small = range(uint8_t(250), uint8_t(255));
    EXPECT_EQ(small.size(), 5u);
    EXPECT_EQ(small.max(), 254);
    auto chars = range(char(-128), char(127));
    EXPECT_EQ(chars.size(), 255u);
    auto bytes_from_zero = range(uint8_t(255));
    EXPECT_EQ(bytes_from_zero.size(), 255u);
}

TEST(RangeBoundaries_Tests, TheWholeOfA64BitType) {
    // 2^64 - 1 numbers: their difference does not fit a signed 64-bit
    // number; size() still counts them, and an iterator moved by the
    // difference lands on the other end (the subtraction overflowed before)
    auto all = range(INT64_MIN, INT64_MAX);
    EXPECT_EQ(all.size(), SIZE_MAX);
    EXPECT_FALSE(all.empty());
    EXPECT_EQ(*(all.begin() + (all.end() - all.begin())), INT64_MAX);
    EXPECT_EQ(*(all.end() - (all.end() - all.begin())), INT64_MIN);
    EXPECT_EQ(all.begin()[all.end() - all.begin()], INT64_MAX);
    auto it = all.begin();
    it += all.end() - all.begin();
    EXPECT_TRUE(it == all.end());
    it -= all.end() - all.begin();
    EXPECT_TRUE(it == all.begin());
    auto u = range(uint64_t(0), UINT64_MAX);
    EXPECT_EQ(u.size(), SIZE_MAX);
    auto half = range(uint64_t(1), uint64_t(1) << 63);
    EXPECT_EQ(half.size(), (size_t(1) << 63) - 1);
    auto past_half = range(uint64_t(0), (uint64_t(1) << 63) + 5);
    EXPECT_EQ(past_half.size(), (size_t(1) << 63) + 5);
    EXPECT_TRUE(all.contains(INT64_MIN));   // found at once: the first number
    EXPECT_EQ(all.front(), INT64_MIN);
}

TEST(RangeBoundaries_Tests, CountingIteratorSteps) {
    counting_iterator<int> z;
    EXPECT_EQ(*z, 0);
    counting_iterator<int> a(5);
    EXPECT_EQ(*(a + 0), 5);
    EXPECT_EQ(*(a - 0), 5);
    EXPECT_EQ(a - a, 0);
    EXPECT_EQ(*(3 + a), 8);
    EXPECT_EQ(a[-5], 0);
    EXPECT_EQ(*a++, 5);
    EXPECT_EQ(*a--, 6);
    EXPECT_EQ(*++a, 6);
    EXPECT_EQ(*--a, 5);
    EXPECT_TRUE(counting_iterator<int>(1) < counting_iterator<int>(2));
    counting_iterator<uint64_t> m(UINT64_MAX);
    EXPECT_EQ(m - counting_iterator<uint64_t>(0), -1);   // 2^64 - 1 apart: wraps in the ptrdiff_t
    EXPECT_EQ(*(counting_iterator<uint64_t>(0) + (m - counting_iterator<uint64_t>(0))), UINT64_MAX);
}

TEST(RangeBoundaries_Tests, OfAPairAndOfAContainer) {
    std::vector<int> v = {3, 1, 2};
    range r(std::pair(v.begin(), v.end()));
    EXPECT_EQ(r.size(), 3u);
    r.sort();
    EXPECT_EQ(v[0], 1);
    range(v.begin(), v.begin()).sort();   // an empty piece
    range(v.begin() + 1, v.begin() + 2).reverse();   // a piece of one
    EXPECT_EQ(v[1], 2);
    r.fill(0);
    EXPECT_EQ(r.count_of([](int x) { return x == 0; }), 3u);
}

// managed_frame and frame_ptr

namespace {
    // A coroutine on a managed frame, suspended at its start and where it
    // awaits; the promise counts its destruction
    struct Task {
        struct promise_type : managed_frame {
            int* destroyed = nullptr;

            ~promise_type() {
                if (destroyed) {
                    ++*destroyed;
                }
            }

            Task get_return_object() {
                return Task{frame_ptr<promise_type>(std::coroutine_handle<promise_type>::from_promise(*this))};
            }
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept {}
        };
        frame_ptr<promise_type> frame;
    };

    Task waits(int value, int* seen) {
        tracked_ptr<Int> local = make_tracked<Int>(value);   // held by the frame alone across the suspension
        co_await std::suspend_always{};
        *seen = *local;
    }

    Task big(int* seen) {
        std::array<int64_t, 20000> words{};   // 160 KB of locals: a frame past one page
        tracked_ptr<Int> local = make_tracked<Int>(9);
        words[0] = 1;
        words.back() = 2;
        co_await std::suspend_always{};
        *seen = int(words[0] + words.back()) + *local;
    }

    Task odd(int* seen) {
        char bytes[13] = {};   // a frame whose size is likely not a whole number of words
        bytes[12] = 7;
        tracked_ptr<Int> local = make_tracked<Int>(1);
        co_await std::suspend_always{};
        *seen = bytes[12] + *local;
    }
}

TEST(FrameBoundaries_Tests, EmptyAtEveryMember) {
    frame_ptr<Task::promise_type> f;
    EXPECT_FALSE(f);
    EXPECT_TRUE(f.done());
    EXPECT_FALSE(f.handle());
    f.destroy();
    f.destroy();
    EXPECT_FALSE(f.release());
    frame_ptr<Task::promise_type> g(std::move(f));
    EXPECT_FALSE(g);
    f = std::move(g);
    f = std::move(f);
    EXPECT_FALSE(f);
    EXPECT_TRUE(f.done());
}

TEST(FrameBoundaries_Tests, MovedFromAndItself) {
    int seen = 0;
    int destroyed = 0;
    Task t = waits(5, &seen);
    t.frame.promise().destroyed = &destroyed;
    t.frame = std::move(t.frame);   // a move into itself does nothing
    ASSERT_TRUE(t.frame);
    EXPECT_FALSE(t.frame.done());
    frame_ptr<Task::promise_type> moved(std::move(t.frame));
    EXPECT_FALSE(t.frame);   // empty after a move
    EXPECT_TRUE(t.frame.done());
    t.frame.destroy();   // nothing: the coroutine is the other's
    EXPECT_EQ(destroyed, 0);
    moved.resume();
    collector::force_collect(true);
    moved.resume();
    EXPECT_TRUE(moved.done());
    EXPECT_EQ(seen, 5);
    moved.destroy();
    EXPECT_EQ(destroyed, 1);
    EXPECT_FALSE(moved);
    moved.destroy();
    EXPECT_EQ(destroyed, 1);
}

TEST(FrameBoundaries_Tests, AssignedOverASuspendedCoroutine) {
    int seen = 0;
    int destroyed = 0;
    Task a = waits(1, &seen);
    a.frame.promise().destroyed = &destroyed;
    a.frame.resume();   // suspended in the middle, its local alive
    Task b = waits(2, &seen);
    a.frame = std::move(b.frame);   // the old coroutine destroyed where it waits
    EXPECT_EQ(destroyed, 1);
    EXPECT_FALSE(b.frame);
    a.frame.resume();
    a.frame.resume();
    EXPECT_EQ(seen, 2);
}

TEST(FrameBoundaries_Tests, FramesOfEverySize) {
    int seen = 0;
    {
        Task t = big(&seen);
        t.frame.resume();
        collector::force_collect(true);
        t.frame.resume();
        EXPECT_EQ(seen, 12);
    }
    {
        Task t = odd(&seen);
        t.frame.resume();
        collector::force_collect(true);
        t.frame.resume();
        EXPECT_EQ(seen, 8);
    }
    {
        Task t = waits(3, &seen);   // destroyed before it ever ran
    }
    collector::force_collect(true);
}

TEST(FrameBoundaries_Tests, Released) {
    int seen = 0;
    int destroyed = 0;
    Task t = waits(4, &seen);
    t.frame.promise().destroyed = &destroyed;
    auto h = t.frame.release();
    EXPECT_FALSE(t.frame);
    EXPECT_TRUE(t.frame.done());
    ASSERT_TRUE(h);
    h.resume();   // the coroutine goes on: nothing has collected it in between
    h.resume();
    EXPECT_TRUE(h.done());
    EXPECT_EQ(seen, 4);
    h.destroy();   // its own end, as a detached coroutine sees to
    EXPECT_EQ(destroyed, 1);
    collector::force_collect(true);
}
