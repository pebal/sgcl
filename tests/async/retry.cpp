//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async::retry under a manual clock: a value at the first attempt, errors
// retried after the policy's waits (no jitter: exact), the attempts spent
// (the last error returned), a predicate that refuses, max_elapsed, the
// cap of a wait, an unlimited count, a stop before and during a wait, a
// coroutine function, an exception that is not retried, the blocking form
// on a thread; the waits of each jitter within their bounds, and a policy
// out of its ranges (a multiplier below one, a negative wait).
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    namespace async = sgcl::async;

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

    // Fails `failures` times with the attempt's number, then gives 42
    struct Flaky {
        int failures;
        std::atomic<int>* calls;

        expected<int, int> operator()() const {
            int n = ++*calls;
            if (n <= failures) {
                return unexpected(n);
            }
            return 42;
        }
    };

    async::retry_policy exact(size_t attempts, duration initial = 100ms) {
        async::retry_policy p;
        p.attempts = attempts;
        p.initial = initial;
        p.jitter = async::jitter::none;
        return p;
    }

    async::task<expected<int, int>> in_task(Flaky f, async::retry_policy p, async::stop_token stop) {
        co_return co_await async::retry(f, p, stop);
    }
}

// A value at the first attempt: one call, no wait
TEST(Retry_Test, AValueAtOnce) {
    Clocked c;
    std::atomic<int> calls{0};
    auto r = async::retry(Flaky{0, &calls}).wait();
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 42);
    EXPECT_EQ(calls.load(), 1);
    auto t = async::spawn(in_task(Flaky{0, &calls}, {}, {}));
    auto rt = t.wait();
    ASSERT_TRUE(rt);
    EXPECT_EQ(calls.load(), 2);
}

// Errors retried after the policy's waits: 100 ms, then 200 ms
TEST(Retry_Test, TheWaitsOfThePolicy) {
    Clocked c;
    std::atomic<int> calls{0};
    auto t = async::spawn(in_task(Flaky{2, &calls}, exact(5), {}));
    c.clock.advance(99ms);
    EXPECT_EQ(calls.load(), 1);
    c.clock.advance(1ms);
    EXPECT_EQ(calls.load(), 2);
    c.clock.advance(199ms);
    EXPECT_EQ(calls.load(), 2);
    c.clock.advance(1ms);
    auto r = t.wait();
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 42);
    EXPECT_EQ(calls.load(), 3);
}

// The attempts spent: the last error returned
TEST(Retry_Test, TheAttemptsSpent) {
    Clocked c;
    std::atomic<int> calls{0};
    auto t = async::spawn(in_task(Flaky{100, &calls}, exact(3), {}));
    c.clock.advance(100ms);
    c.clock.advance(200ms);
    auto r = t.wait();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error(), 3);
    EXPECT_EQ(calls.load(), 3);
    calls = 0;
    auto once = async::retry(Flaky{100, &calls}, exact(1)).wait();   // one attempt: a call
    ASSERT_FALSE(once);
    EXPECT_EQ(calls.load(), 1);
}

namespace {
    async::task<expected<int, int>> with_predicate(Flaky f, async::retry_policy p) {
        co_return co_await async::retry(f, p, [](const expected<int, int>& r) { return !r && r.error() < 2; });
    }
}

// A predicate that refuses ends the retry: the first error retried, the
// second not
TEST(Retry_Test, APredicateThatRefuses) {
    Clocked c;
    std::atomic<int> calls{0};
    auto t = async::spawn(with_predicate(Flaky{100, &calls}, exact(10)));
    c.clock.advance(100ms);
    auto r = t.wait();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error(), 2);
    EXPECT_EQ(calls.load(), 2);
}

namespace {
    struct Response {
        int status;
    };

    async::task<expected<Response, int>> retry_on_status(std::atomic<int>* calls) {
        co_return co_await async::retry([calls]() -> expected<Response, int> {
            int n = ++*calls;
            return Response{n < 3 ? 503 : 200};   // a value either way
        }, exact(5, 10ms), [](const expected<Response, int>& r) { return !r || r->status == 503; });
    }
}

// The predicate sees the whole result: a value retried too (a 503); f of
// any result with a predicate
TEST(Retry_Test, APredicateOnTheValue) {
    Clocked c;
    std::atomic<int> calls{0};
    auto t = async::spawn(retry_on_status(&calls));
    c.clock.advance(10ms);
    c.clock.advance(20ms);
    auto r = t.wait();
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status, 200);
    EXPECT_EQ(calls.load(), 3);
    int tries = 0;
    int last = async::retry([&] { return ++tries; }, exact(4, duration::zero()), [](int v) { return v < 3; }).wait();
    EXPECT_EQ(last, 3);   // a plain int with a predicate
    EXPECT_EQ(tries, 3);
    calls = 0;
    auto never = async::retry(Flaky{0, &calls}, exact(3, duration::zero()), [](const expected<int, int>&) { return false; }).wait();
    ASSERT_TRUE(never);
    EXPECT_EQ(calls.load(), 1);
}

// max_elapsed: no attempt that would start past it (1 s, then 2 s: the
// third would start at 3 s, past 2.5)
TEST(Retry_Test, MaxElapsed) {
    Clocked c;
    std::atomic<int> calls{0};
    auto p = exact(0, 1s);
    p.max_elapsed = 2500ms;
    auto t = async::spawn(in_task(Flaky{100, &calls}, p, {}));
    c.clock.advance(1s);
    auto r = t.wait();
    ASSERT_FALSE(r);
    EXPECT_EQ(calls.load(), 2);
}

// The cap of a wait, and an unlimited count: 1 s, 5 s, 5 s, ...
TEST(Retry_Test, TheCapAndNoLimit) {
    Clocked c;
    std::atomic<int> calls{0};
    auto p = exact(0, 1s);
    p.multiplier = 10;
    p.max_delay = 5s;
    auto t = async::spawn(in_task(Flaky{5, &calls}, p, {}));
    c.clock.advance(1s);
    EXPECT_EQ(calls.load(), 2);
    c.clock.advance(4999ms);
    EXPECT_EQ(calls.load(), 2);
    c.clock.advance(1ms);
    EXPECT_EQ(calls.load(), 3);
    for (int i : range(3)) {
        (void)i;
        c.clock.advance(5s);
    }
    auto r = t.wait();
    ASSERT_TRUE(r);
    EXPECT_EQ(calls.load(), 6);
}

// A stop before the first attempt: the attempt runs, nothing after it; a
// stop during a wait: the last error, no attempt after it
TEST(Retry_Test, AStop) {
    Clocked c;
    std::atomic<int> calls{0};
    {
        async::stop_source s;
        s.request_stop();
        auto t = async::spawn(in_task(Flaky{100, &calls}, exact(10), s.token()));
        auto r = t.wait();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error(), 1);
        EXPECT_EQ(calls.load(), 1);
    }
    calls = 0;
    {
        async::stop_source s;
        auto t = async::spawn(in_task(Flaky{100, &calls}, exact(10), s.token()));
        c.clock.advance(100ms);
        EXPECT_EQ(calls.load(), 2);
        c.clock.advance(50ms);
        s.request_stop();
        auto r = t.wait();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error(), 2);
        c.clock.advance(1s);
        EXPECT_EQ(calls.load(), 2);
    }
    calls = 0;
    {
        async::stop_source s;   // a token that never stops: the waits are the policy's
        auto t = async::spawn(in_task(Flaky{1, &calls}, exact(10), s.token()));
        c.clock.advance(100ms);
        auto r = t.wait();
        ASSERT_TRUE(r);
    }
}

namespace {
    async::task<expected<string, string>> fetch(std::atomic<int>* calls) {
        co_await async::yield();
        if (++*calls < 3) {
            co_return unexpected(string("busy"));
        }
        co_return string("done");
    }

    async::task<expected<string, string>> retry_fetch(std::atomic<int>* calls, async::retry_policy p) {
        co_return co_await async::retry([calls] { return fetch(calls); }, p);
    }
}

// A coroutine function: its task awaited at each attempt
TEST(Retry_Test, ACoroutineFunction) {
    Clocked c;
    std::atomic<int> calls{0};
    auto t = async::spawn(retry_fetch(&calls, exact(5, 10ms)));
    c.clock.advance(10ms);
    c.clock.advance(20ms);
    auto r = t.wait();
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, "done");
    EXPECT_EQ(calls.load(), 3);
}

namespace {
    async::task<string> throwing_retry() {
        int calls = 0;
        try {
            co_await async::retry([&calls]() -> expected<int, int> {
                if (++calls == 2) {
                    throw std::runtime_error("broken");
                }
                return unexpected(calls);
            }, exact(5, 1ms));
        } catch (const std::runtime_error& e) {
            co_return string(e.what()) + " after " + to_string(calls);
        }
        co_return string("no throw");
    }
}

// An exception is not retried: it comes out
TEST(Retry_Test, AnExceptionComesOut) {
    Clocked c;
    auto t = async::spawn(throwing_retry());
    c.clock.advance(1ms);
    EXPECT_EQ(t.wait(), "broken after 2");
}

// The blocking form on a thread, in real time
TEST(Retry_Test, OnAThread) {
    std::atomic<int> calls{0};
    auto start = std::chrono::steady_clock::now();
    auto r = async::retry(Flaky{2, &calls}, exact(5, 2ms)).wait();
    auto took = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(r);
    EXPECT_EQ(calls.load(), 3);
    EXPECT_GE(took, 6ms);   // 2 + 4
    calls = 0;
    async::stop_source s;
    std::thread stopper([&] {
        std::this_thread::sleep_for(20ms);
        s.request_stop();
    });
    auto stopped = async::retry(Flaky{1000, &calls}, exact(0, 5ms), s.token()).wait();
    stopper.join();
    ASSERT_FALSE(stopped);
    EXPECT_GE(calls.load(), 1);
    EXPECT_LE(calls.load(), 6);
    async::scheduler::stop();
}

// Each jitter's waits within its bounds
TEST(Retry_Test, TheJitters) {
    async::retry_policy p;
    p.initial = 100ms;
    p.max_delay = 1s;
    p.multiplier = 2;
    for (int n : range(1, 8)) {
        double v = std::min(100.0 * std::pow(2.0, n - 1), 1000.0);
        for (double u : {0.0, 0.25, 0.5, 0.999999}) {
            p.jitter = async::jitter::none;
            EXPECT_NEAR(async::detail::retry_delay(p, n, 0ms, u).nanoseconds() / 1e6, v, 1e-6);
            p.jitter = async::jitter::full;
            EXPECT_NEAR(async::detail::retry_delay(p, n, 0ms, u).nanoseconds() / 1e6, u * v, 1e-6);
            p.jitter = async::jitter::equal;
            EXPECT_NEAR(async::detail::retry_delay(p, n, 0ms, u).nanoseconds() / 1e6, v / 2 + u * v / 2, 1e-6);
        }
    }
    p.jitter = async::jitter::decorrelated;
    duration prev = p.initial;
    for (int n : range(1, 50)) {
        duration d = async::detail::retry_delay(p, n, prev, async::detail::retry_uniform());
        EXPECT_GE(d, p.initial);
        EXPECT_LE(d, p.max_delay);
        EXPECT_LE(d.nanoseconds(), 3 * prev.nanoseconds());
        prev = d;
    }
    // the retry's own sequence (multiplied as it goes) is the formula's
    async::retry_policy q;
    q.initial = 3ms;
    q.max_delay = 7s;
    q.multiplier = 1.7;
    q.attempts = 0;
    q.jitter = async::jitter::none;
    async::detail::RetryState st(q);
    for (size_t n = 1; n < 60; ++n, ++st.n) {
        auto d = st.next(q);
        ASSERT_TRUE(d);
        EXPECT_NEAR((double)d->nanoseconds(), (double)async::detail::retry_delay(q, n, 0ms, 0).nanoseconds(), 1e-6 * (double)d->nanoseconds() + 1);
    }
    q.jitter = async::jitter::decorrelated;
    async::detail::RetryState dst(q);
    duration last = q.initial;
    for (int n : range(1, 60)) {
        (void)n;
        auto d = dst.next(q);
        ASSERT_TRUE(d);
        EXPECT_GE(*d, q.initial);
        EXPECT_LE(*d, q.max_delay);
        EXPECT_LE(d->nanoseconds(), 3 * last.nanoseconds() + 1);
        last = *d;
        ++dst.n;
    }
    double sum = 0;   // the draws spread over [0, 1)
    for (int i : range(10000)) {
        (void)i;
        double u = async::detail::retry_uniform();
        EXPECT_GE(u, 0.0);
        EXPECT_LT(u, 1.0);
        sum += u;
    }
    EXPECT_NEAR(sum / 10000, 0.5, 0.02);
}

// A policy out of its ranges: a multiplier below one is one, a negative
// wait is zero, a huge count of waits stays at the cap
TEST(Retry_Test, APolicyOutOfRange) {
    async::retry_policy p;
    p.jitter = async::jitter::none;
    p.multiplier = 0.5;
    EXPECT_EQ(async::detail::retry_delay(p, 5, 0ms, 0.5), p.initial);
    p.initial = -5s;
    EXPECT_EQ(async::detail::retry_delay(p, 5, 0ms, 0.5), duration::zero());
    p.initial = 1s;
    p.multiplier = 2;
    EXPECT_EQ(async::detail::retry_delay(p, 100000, 0ms, 0.5), p.max_delay);
    p.max_delay = -1s;
    EXPECT_EQ(async::detail::retry_delay(p, 3, 0ms, 0.5), duration::zero());
}
