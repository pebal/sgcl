//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async::rate_limiter: the token bucket of golang.org/x/time/rate under a
// manual clock. The bucket full at the start, the refill, allow(n) at its
// limits (zero, the burst, past it, in debt), reserve and cancel by Go's
// rules (the tokens a later reservation counts on kept, nothing after the
// time), wait in a task and on a thread, a stop before and during the wait,
// a deadline that would pass first, a parent's deadline, set_limit and
// set_burst keeping the tokens of the moment, the modes inf and zero
// (and less, and NaN), a rate fine enough to need fractions of a
// nanosecond, a rate of one a day, a clock that runs for months (the new
// bases), copies and equality; on threads, many takers at once giving out
// exactly the burst, waits in many tasks served in order, a change of the
// limit under load.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    namespace async = sgcl::async;
    using reason = async::rate_error::reason;

    struct Clocked {
        async::manual_clock clock;

        Clocked() {
            clock.install();
        }

        ~Clocked() {
            clock.uninstall();
            async::scheduler::stop();
        }
    };

    async::task<expected<void, async::rate_error>> wait_in_task(async::rate_limiter lim, size_t n, async::stop_token stop) {
        co_return co_await lim.acquire(n, stop);
    }
}

// Full at the start: the burst at once, then nothing until the refill
TEST(RateLimiter_Test, FullAtTheStartThenTheRefill) {
    Clocked c;
    async::rate_limiter lim(10.0, 3);
    EXPECT_EQ(lim.limit(), 10.0);
    EXPECT_EQ(lim.burst(), 3u);
    EXPECT_DOUBLE_EQ(lim.tokens(), 3.0);
    EXPECT_TRUE(lim.allow());
    EXPECT_TRUE(lim.allow());
    EXPECT_TRUE(lim.allow());
    EXPECT_FALSE(lim.allow());
    EXPECT_NEAR(lim.tokens(), 0.0, 1e-9);
    c.clock.advance(99ms);
    EXPECT_FALSE(lim.allow());
    EXPECT_NEAR(lim.tokens(), 0.99, 1e-6);
    c.clock.advance(1ms);
    EXPECT_TRUE(lim.allow());
    EXPECT_FALSE(lim.allow());
    c.clock.advance(10s);   // never more than the burst
    EXPECT_DOUBLE_EQ(lim.tokens(), 3.0);
    EXPECT_TRUE(lim.allow(3));
    EXPECT_FALSE(lim.allow());
}

// allow(n) at its limits: zero, the burst, past it, more than any count
TEST(RateLimiter_Test, AllowAtItsLimits) {
    Clocked c;
    async::rate_limiter lim(1.0, 5);
    EXPECT_FALSE(lim.allow(6));                           // past the burst: never
    EXPECT_FALSE(lim.allow(std::numeric_limits<size_t>::max()));
    EXPECT_DOUBLE_EQ(lim.tokens(), 5.0);                  // nothing taken by a refusal
    EXPECT_TRUE(lim.allow(0));
    EXPECT_TRUE(lim.allow(5));
    EXPECT_TRUE(lim.allow(0));                            // zero of zero
    EXPECT_FALSE(lim.allow(1));
    auto r = lim.reserve(2);                              // in debt
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE(lim.allow(0));                           // as Go: a bucket in debt allows not even zero
    EXPECT_NEAR(lim.tokens(), -2.0, 1e-9);
}

// One token every interval: Go's rate.Every
TEST(RateLimiter_Test, AnInterval) {
    Clocked c;
    async::rate_limiter lim(250ms, 1);
    EXPECT_DOUBLE_EQ(lim.limit(), 4.0);
    EXPECT_TRUE(lim.allow());
    EXPECT_FALSE(lim.allow());
    c.clock.advance(250ms);
    EXPECT_TRUE(lim.allow());
    async::rate_limiter none(0s, 1);                      // an interval of zero or less: inf
    EXPECT_TRUE(std::isinf(none.limit()));
    async::rate_limiter neg(-1s, 0);
    EXPECT_TRUE(std::isinf(neg.limit()));
}

// inf allows everything, whatever the burst; zero (or less, or NaN) the
// burst once
TEST(RateLimiter_Test, InfAndZero) {
    Clocked c;
    async::rate_limiter all(async::rate_limiter::inf, 0);
    for (int i : range(1000)) {
        (void)i;
        EXPECT_TRUE(all.allow(1000));
    }
    auto r = all.reserve(1'000'000);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(r.delay(), duration::zero());
    EXPECT_TRUE(all.acquire(5).wait());
    for (double z : {0.0, -3.0, std::nan("")}) {
        async::rate_limiter once(z, 2);
        EXPECT_EQ(once.limit(), 0.0);
        EXPECT_TRUE(once.allow());
        auto two = once.reserve(2);
        EXPECT_FALSE(two.ok());                           // too few left: no debt at a limit of zero
        EXPECT_TRUE(once.allow());
        EXPECT_FALSE(once.allow());
        c.clock.advance(24h);
        EXPECT_FALSE(once.allow());                       // never refilled
        auto w = once.acquire().wait();
        ASSERT_FALSE(w);
        EXPECT_EQ(w.error().why(), reason::deadline);     // never comes
    }
}

// A burst of zero allows nothing at a finite rate
TEST(RateLimiter_Test, ABurstOfZero) {
    Clocked c;
    async::rate_limiter lim(1000.0, 0);
    EXPECT_FALSE(lim.allow());
    EXPECT_TRUE(lim.allow(0));
    EXPECT_FALSE(lim.reserve().ok());
    auto w = lim.acquire().wait();
    ASSERT_FALSE(w);
    EXPECT_EQ(w.error().why(), reason::burst);
    EXPECT_EQ(w.error().message(), "more tokens than the burst");
}

// A reservation: its time, its delay, the debt it leaves
TEST(RateLimiter_Test, AReservationSaysWhen) {
    Clocked c;
    async::rate_limiter lim(10.0, 2);
    time_point t0 = clock::now();
    auto a = lim.reserve(2);
    ASSERT_TRUE(a.ok());
    EXPECT_EQ(a.time(), t0);                              // there now
    EXPECT_EQ(a.delay(), duration::zero());
    auto b = lim.reserve();
    ASSERT_TRUE(b.ok());
    EXPECT_EQ(b.time(), t0 + 100ms);
    EXPECT_EQ(b.delay(), duration(100ms));
    auto d = lim.reserve(2);
    EXPECT_EQ(d.time(), t0 + 300ms);
    c.clock.advance(50ms);
    EXPECT_EQ(b.delay(), duration(50ms));
    c.clock.advance(1s);
    EXPECT_EQ(b.delay(), duration::zero());
    async::rate_limiter::reservation none;
    EXPECT_FALSE(none.ok());
    EXPECT_EQ(none.time(), time_point::max());
    EXPECT_EQ(none.delay(), duration::max());
    none.cancel();                                        // nothing to give back
    auto past = lim.reserve(3);                           // past the burst
    EXPECT_FALSE(past.ok());
}

// cancel by Go's rules: the tokens back before the time, the ones a later
// reservation counts on kept, nothing after the time, nothing twice
TEST(RateLimiter_Test, CancelGivesBackWhatNobodyCountsOn) {
    Clocked c;
    async::rate_limiter lim(10.0, 1);
    EXPECT_TRUE(lim.allow());
    auto a = lim.reserve();                               // at +100 ms
    EXPECT_NEAR(lim.tokens(), -1.0, 1e-9);
    a.cancel();
    EXPECT_FALSE(a.ok());
    EXPECT_NEAR(lim.tokens(), 0.0, 1e-9);
    a.cancel();                                           // twice: nothing
    EXPECT_NEAR(lim.tokens(), 0.0, 1e-9);

    auto b = lim.reserve();                               // +100 ms
    auto later = lim.reserve();                           // +200 ms, counts on b's place
    EXPECT_NEAR(lim.tokens(), -2.0, 1e-9);
    b.cancel();                                           // later took one token after it: nothing back
    EXPECT_NEAR(lim.tokens(), -2.0, 1e-9);
    auto big = lim.reserve(1);
    later.cancel();                                       // big counts on it: nothing back
    EXPECT_NEAR(lim.tokens(), -3.0, 1e-9);
    big.cancel();                                         // the last: all back
    EXPECT_NEAR(lim.tokens(), -2.0, 1e-9);

    auto due = lim.reserve();
    c.clock.advance(1s);                                  // its time has come
    double before = lim.tokens();
    due.cancel();
    EXPECT_DOUBLE_EQ(lim.tokens(), before);
}

// A reservation moved: the tokens given back once, through the new owner
TEST(RateLimiter_Test, AReservationMoves) {
    Clocked c;
    async::rate_limiter lim(10.0, 1);
    EXPECT_TRUE(lim.allow());
    auto a = lim.reserve();
    async::rate_limiter::reservation b = std::move(a);
    EXPECT_FALSE(a.ok());
    EXPECT_TRUE(b.ok());
    a.cancel();
    EXPECT_NEAR(lim.tokens(), -1.0, 1e-9);
    async::rate_limiter::reservation d;
    d = std::move(b);
    d.cancel();
    EXPECT_NEAR(lim.tokens(), 0.0, 1e-9);
    d = std::move(d);                                     // to itself
    EXPECT_FALSE(d.ok());
}

// wait on a thread and in a task: at once with the tokens there, after the
// refill without them
TEST(RateLimiter_Test, WaitInATaskAndOnAThread) {
    Clocked c;
    async::rate_limiter lim(10.0, 1);
    EXPECT_TRUE(lim.acquire().wait());                       // there
    auto t = async::spawn(wait_in_task(lim, 1, {}));
    c.clock.advance(50ms);
    EXPECT_FALSE(t.done());
    c.clock.advance(50ms);
    auto r = t.wait();
    EXPECT_TRUE(r);
    auto t2 = async::spawn(wait_in_task(lim, 1, {}));
    c.clock.advance(100ms);
    EXPECT_TRUE(t2.wait());
    // on a thread, the clock moved by another
    std::atomic<bool> done{false};
    std::thread w([&] {
        async::rate_limiter mine = lim;
        EXPECT_TRUE(mine.acquire().wait());
        done = true;
    });
    for (int i = 0; i < 1000 && !done; ++i) {
        c.clock.advance(1ms);
        std::this_thread::sleep_for(100us);
    }
    w.join();
    EXPECT_TRUE(done);
}

// A stop before the wait takes nothing; a stop during it gives the tokens
// back; a deadline the tokens would come after fails at once, a parent's
// too
TEST(RateLimiter_Test, StopsAndDeadlines) {
    Clocked c;
    async::rate_limiter lim(10.0, 1);
    EXPECT_TRUE(lim.allow());
    {
        async::stop_source s;
        s.request_stop();
        auto r = lim.acquire(1, s.token()).wait();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().why(), reason::stopped);
        EXPECT_EQ(r.error(), async::rate_error(reason::stopped));
        EXPECT_NEAR(lim.tokens(), 0.0, 1e-9);
    }
    {
        async::stop_source s;
        auto t = async::spawn(wait_in_task(lim, 1, s.token()));
        c.clock.advance(10ms);
        EXPECT_FALSE(t.done());
        EXPECT_NEAR(lim.tokens(), -0.9, 1e-9);           // reserved
        s.request_stop();
        auto r = t.wait();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().why(), reason::stopped);
        EXPECT_NEAR(lim.tokens(), 0.1, 1e-9);            // given back
    }
    {
        async::stop_source s;
        s.stop_after(50ms);                               // the token comes at +90 ms
        auto r = lim.acquire(1, s.token()).wait();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().why(), reason::deadline);
        EXPECT_EQ(r.error().message(), "the tokens would come after the deadline");
        EXPECT_NEAR(lim.tokens(), 0.1, 1e-9);            // nothing taken
    }
    {
        async::stop_source parent;
        parent.stop_after(50ms);
        async::stop_source child(parent.token());
        auto r = lim.acquire(1, child.token()).wait();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().why(), reason::deadline);
    }
    {
        async::stop_source s;
        s.stop_after(1s);                                 // late enough: the wait is served
        auto t = async::spawn(wait_in_task(lim, 1, s.token()));
        c.clock.advance(90ms);
        auto r = t.wait();
        EXPECT_TRUE(r);
    }
    {
        auto r = lim.acquire(2).wait();                      // past the burst
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().why(), reason::burst);
    }
    {
        async::stop_source s;                             // a stop on a thread's wait
        std::atomic<bool> started{false};
        expected<void, async::rate_error> got;
        std::thread w([&] {
            async::rate_limiter mine = lim;
            async::stop_token tok = s.token();
            started = true;
            got = mine.acquire(1, tok).wait();
        });
        while (!started) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(5ms);
        s.request_stop();
        w.join();
        ASSERT_FALSE(got);
        EXPECT_EQ(got.error().why(), reason::stopped);
    }
}

// set_limit and set_burst keep the tokens of the moment
TEST(RateLimiter_Test, ChangesKeepTheTokens) {
    Clocked c;
    async::rate_limiter lim(10.0, 10);
    EXPECT_TRUE(lim.allow(10));
    c.clock.advance(500ms);                               // 5 tokens
    lim.set_limit(100.0);
    EXPECT_EQ(lim.limit(), 100.0);
    EXPECT_NEAR(lim.tokens(), 5.0, 1e-6);
    c.clock.advance(10ms);                                // +1 at the new rate
    EXPECT_NEAR(lim.tokens(), 6.0, 1e-6);
    lim.set_burst(4);                                     // capped
    EXPECT_EQ(lim.burst(), 4u);
    EXPECT_NEAR(lim.tokens(), 4.0, 1e-6);
    EXPECT_FALSE(lim.allow(5));
    EXPECT_TRUE(lim.allow(4));
    lim.set_limit(async::rate_limiter::inf);
    EXPECT_TRUE(lim.allow(1000));
    lim.set_limit(1.0);                                   // from inf: full
    EXPECT_NEAR(lim.tokens(), 4.0, 1e-6);
    EXPECT_TRUE(lim.allow(4));
    lim.set_limit(0.0);                                   // to zero: the tokens left, never more
    c.clock.advance(1h);
    EXPECT_NEAR(lim.tokens(), 0.0, 1e-6);
    EXPECT_FALSE(lim.allow());
    lim.set_limit(2s);                                    // from zero: refilled again
    c.clock.advance(2s);
    EXPECT_TRUE(lim.allow());
    EXPECT_DOUBLE_EQ(lim.limit(), 0.5);
    lim.set_burst(0);
    EXPECT_FALSE(lim.allow());
    lim.set_burst(1000);
    c.clock.advance(2s);
    EXPECT_TRUE(lim.allow());
}

// A smaller burst caps the tokens left at a limit of zero too (the fuzz
// harness found the count left above it)
TEST(RateLimiter_Test, ASmallerBurstAtZero) {
    Clocked c;
    async::rate_limiter lim(0.0, 10);
    EXPECT_TRUE(lim.allow(2));
    lim.set_burst(3);
    EXPECT_DOUBLE_EQ(lim.tokens(), 3.0);
    EXPECT_FALSE(lim.allow(4));
    EXPECT_TRUE(lim.allow(3));
    EXPECT_FALSE(lim.allow());
    lim.set_burst(0);
    EXPECT_FALSE(lim.allow());
}

// A reservation made before a change keeps its time; its cancel after the
// change gives nothing back
TEST(RateLimiter_Test, AReservationAcrossAChange) {
    Clocked c;
    async::rate_limiter lim(10.0, 1);
    EXPECT_TRUE(lim.allow());
    time_point t0 = clock::now();
    auto r = lim.reserve();
    lim.set_limit(1.0);
    EXPECT_EQ(r.time(), t0 + 100ms);
    double before = lim.tokens();
    r.cancel();
    EXPECT_DOUBLE_EQ(lim.tokens(), before);
}

// A rate fine enough to need fractions of a nanosecond: 150 million a
// second keeps its rate (whole nanoseconds would be 6.667 -> 7, 5% slow)
TEST(RateLimiter_Test, FractionsOfANanosecond) {
    Clocked c;
    async::rate_limiter lim(150e6, 150'000'000);
    EXPECT_TRUE(lim.allow(150'000'000));
    c.clock.advance(1s);
    EXPECT_NEAR(lim.tokens(), 150e6, 150e6 * 1e-4);
    c.clock.advance(1s);
    EXPECT_TRUE(lim.allow(150'000'000));
    for (int i : range(3)) {
        (void)i;
        c.clock.advance(100ms);
        EXPECT_TRUE(lim.allow(14'990'000));
    }
    async::rate_limiter fast(1e9, 1000);                  // one a nanosecond
    EXPECT_TRUE(fast.allow(1000));
    c.clock.advance(500ns);
    EXPECT_TRUE(fast.allow(500));
    EXPECT_FALSE(fast.allow(1));
}

// A slow rate and a large burst: the tick widened to whole nanoseconds
TEST(RateLimiter_Test, OneADay) {
    Clocked c;
    async::rate_limiter lim(24h, 1000);
    EXPECT_NEAR(lim.limit(), 1.0 / 86400, 1e-12);
    EXPECT_TRUE(lim.allow(1000));
    c.clock.advance(23h);
    EXPECT_FALSE(lim.allow());
    c.clock.advance(1h);
    EXPECT_TRUE(lim.allow());
    auto r = lim.reserve(3);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.delay(), duration(72h));
}

// A clock that runs for months: new bases on the way, the tokens kept
TEST(RateLimiter_Test, MonthsOfTheClock) {
    Clocked c;
    async::rate_limiter lim(1000.0, 10);
    for (int day : range(200)) {
        (void)day;
        c.clock.advance(24h);
        EXPECT_TRUE(lim.allow(10));
        EXPECT_FALSE(lim.allow());
        c.clock.advance(5ms);
        EXPECT_NEAR(lim.tokens(), 5.0, 1e-3);
        EXPECT_TRUE(lim.allow(5));
    }
    auto r = lim.reserve(10);                             // across a base: the time stays exact
    EXPECT_EQ(r.delay(), duration(10ms));
}

// A reservation too far ahead for the count fails, and a wait reports it
TEST(RateLimiter_Test, TooFarAhead) {
    Clocked c;
    async::rate_limiter lim(1.0, 1);                      // each a second ahead: ~52 days is the count's reach
    long ok = 0;
    for (long i = 0; i < 10'000'000; ++i) {
        if (!lim.reserve().ok()) {
            break;
        }
        ++ok;
    }
    EXPECT_GT(ok, 4'000'000);
    EXPECT_LT(ok, 5'000'000);
    auto w = lim.acquire().wait();
    ASSERT_FALSE(w);
    EXPECT_EQ(w.error().why(), reason::deadline);
}

// Copies share the bucket; == is identity
TEST(RateLimiter_Test, CopiesShareTheBucket) {
    Clocked c;
    async::rate_limiter a(1.0, 2);
    async::rate_limiter b = a;
    async::rate_limiter other(1.0, 2);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == other);
    EXPECT_TRUE(b.allow(2));
    EXPECT_FALSE(a.allow());
    async::rate_limiter moved = std::move(b);
    EXPECT_TRUE(moved == a);
    EXPECT_TRUE(b == a);                                  // a handle moved from stands for the same bucket
}

// Many threads at one bucket with the clock stopped: exactly the burst
TEST(RateLimiter_Test, ManyTakersExactlyTheBurst) {
    Clocked c;
    async::rate_limiter lim(1.0, 10'000);
    std::atomic<long> granted{0};
    std::vector<std::thread> ts;
    for (int t : range(8)) {
        (void)t;
        ts.emplace_back([&] {
            async::rate_limiter mine = lim;
            long k = 0;
            for (int i : range(5000)) {
                (void)i;
                k += mine.allow();
            }
            granted += k;
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    EXPECT_EQ(granted.load(), 10'000);
}

// Many tasks waiting: served one per token, as the clock moves
TEST(RateLimiter_Test, ManyWaitingTasks) {
    Clocked c;
    async::rate_limiter lim(100.0, 1);
    EXPECT_TRUE(lim.allow());
    vector<async::task<expected<void, async::rate_error>>> ts;
    for (int i : range(50)) {
        (void)i;
        ts.push_back(async::spawn(wait_in_task(lim, 1, {})));
    }
    for (int step : range(50)) {
        (void)step;
        c.clock.advance(10ms);
    }
    for (auto& t : ts) {
        EXPECT_TRUE(t.wait());
    }
    EXPECT_NEAR(lim.tokens(), 0.0, 1e-6);
}

// Changes of the limit while threads take: never more than the most any
// rate in force allowed, and every call returns
TEST(RateLimiter_Test, ChangesUnderLoad) {
    async::rate_limiter lim(1e6, 1000);
    std::atomic<bool> stop{false};
    std::atomic<long> granted{0};
    std::vector<std::thread> ts;
    auto t0 = std::chrono::steady_clock::now();
    for (int t : range(4)) {
        (void)t;
        ts.emplace_back([&] {
            async::rate_limiter mine = lim;
            long k = 0;
            while (!stop) {
                k += mine.allow();
            }
            granted += k;
        });
    }
    for (int i = 0; std::chrono::steady_clock::now() - t0 < 50ms; ++i) {
        lim.set_limit(i % 2 ? 1e6 : 5e5);
        if (i % 100 == 0) {
            lim.set_burst(500 + i % 3 * 250);
        }
        std::this_thread::yield();
    }
    stop = true;
    for (auto& t : ts) {
        t.join();
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LE(granted.load(), (long)(1e6 * secs) + 1000 + 10);   // the burst and the most any rate in force refilled
    EXPECT_GT(granted.load(), 0);
}
