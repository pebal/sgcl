//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async::singleflight: one call per key at a time. Under a manual clock
// (whose advance first waits for every task to reach its wait, so that
// every caller has joined): many callers of a key, one call and one result
// for all; keys apart run apart; a call after the end runs again; what f
// throws reaches every caller; forget during a call; a void result, an
// expected, a plain function and a coroutine function; the blocking form
// on threads; the empty key; a caller of the key with another result type
// (run alone; release builds, the debug ones assert); copies and
// equality; and many threads and tasks on few keys, every result the one
// of a call that was in flight.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

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

    std::atomic<int> calls{0};

    async::task<int> slow_value(int v) {
        calls.fetch_add(1);
        co_await async::sleep(1s);
        co_return v;
    }

    async::task<int> ask(async::singleflight flights, string key, int v) {
        co_return co_await flights.run(key, [v] { return slow_value(v); });
    }
}

// Many callers of one key: one call, its result for all
TEST(Singleflight_Test, ManyCallersOneCall) {
    Clocked c;
    calls = 0;
    async::singleflight flights;
    vector<async::task<int>> ts;
    for (int i : range(20)) {
        ts.push_back(async::spawn(ask(flights, "k", 100 + i)));
    }
    c.clock.advance(1s);   // every caller joined before the time moves
    int first = ts[0].wait();
    for (auto& t : ts) {
        EXPECT_EQ(t.wait(), first);
    }
    EXPECT_EQ(calls.load(), 1);
    // after the end, the next caller runs again
    auto again = async::spawn(ask(flights, "k", 7));
    c.clock.advance(1s);
    EXPECT_EQ(again.wait(), 7);
    EXPECT_EQ(calls.load(), 2);
}

// Keys apart run apart
TEST(Singleflight_Test, KeysApart) {
    Clocked c;
    calls = 0;
    async::singleflight flights;
    auto a = async::spawn(ask(flights, "a", 1));
    auto b = async::spawn(ask(flights, "b", 2));
    auto e = async::spawn(ask(flights, "", 3));   // the empty key is a key
    auto a2 = async::spawn(ask(flights, "a", 4));
    c.clock.advance(1s);
    EXPECT_EQ(a.wait(), a2.wait());
    EXPECT_EQ(b.wait(), 2);
    EXPECT_EQ(e.wait(), 3);
    EXPECT_EQ(calls.load(), 3);
}

namespace {
    async::task<int> failing() {
        co_await async::sleep(1s);
        throw std::runtime_error("down");
    }

    async::task<string> ask_failing(async::singleflight flights) {
        try {
            co_await flights.run("x", [] { return failing(); });
        } catch (const std::runtime_error& e) {
            co_return string(e.what());
        }
        co_return string("no throw");
    }
}

// What f throws reaches every caller of the call
TEST(Singleflight_Test, AnExceptionForEveryCaller) {
    Clocked c;
    async::singleflight flights;
    vector<async::task<string>> ts;
    for (int i : range(5)) {
        (void)i;
        ts.push_back(async::spawn(ask_failing(flights)));
    }
    c.clock.advance(1s);
    for (auto& t : ts) {
        EXPECT_EQ(t.wait(), "down");
    }
    // the key is free again
    int v = flights.run("x", [] { return 5; }).wait();
    EXPECT_EQ(v, 5);
}

// forget during a call: the next caller starts a new call, the old one's
// callers get the old result, and its end does not erase the new call
TEST(Singleflight_Test, ForgetDuringACall) {
    Clocked c;
    calls = 0;
    async::singleflight flights;
    auto old1 = async::spawn(ask(flights, "k", 1));
    auto old2 = async::spawn(ask(flights, "k", 2));
    c.clock.advance(500ms);
    flights.forget("k");
    auto new1 = async::spawn(ask(flights, "k", 3));
    c.clock.advance(500ms);   // the old call ends; the new one is half way
    EXPECT_EQ(old1.wait(), old2.wait());
    EXPECT_EQ(calls.load(), 2);
    auto new2 = async::spawn(ask(flights, "k", 4));   // joins the new call: the old end left it in place
    c.clock.advance(500ms);
    EXPECT_EQ(new1.wait(), 3);
    EXPECT_EQ(new2.wait(), 3);
    EXPECT_EQ(calls.load(), 2);
    flights.forget("absent");   // nothing to forget
    flights.forget("k");
}

namespace {
    std::atomic<int> voids{0};

    async::task<> slow_void() {
        voids.fetch_add(1);
        co_await async::sleep(1s);
    }

    async::task<> ask_void(async::singleflight flights) {
        co_await flights.run("v", [] { return slow_void(); });
    }

    async::task<expected<int, string>> ask_expected(async::singleflight flights, bool ok) {
        co_return co_await flights.run("e", [ok]() -> expected<int, string> {
            if (ok) {
                return 9;
            }
            return unexpected(string("no"));
        });
    }
}

// A void result; an expected passed on as it is; a plain function
TEST(Singleflight_Test, VoidExpectedAndPlainFunctions) {
    Clocked c;
    voids = 0;
    async::singleflight flights;
    auto a = async::spawn(ask_void(flights));
    auto b = async::spawn(ask_void(flights));
    c.clock.advance(1s);
    a.wait();
    b.wait();
    EXPECT_EQ(voids.load(), 1);
    auto good = async::spawn(ask_expected(flights, true)).wait();
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, 9);
    auto bad = async::spawn(ask_expected(flights, false)).wait();
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error(), "no");
    int plain = 0;
    flights.run("p", [&] { ++plain; }).wait();
    EXPECT_EQ(plain, 1);
    string s = flights.run("s", [] { return string("text"); }).wait();
    EXPECT_EQ(s, "text");
}

// The blocking form on threads: one call while it runs, its result for all
TEST(Singleflight_Test, ThreadsShareACall) {
    async::singleflight flights;
    std::atomic<int> runs{0};
    std::atomic<bool> inside{false}, release{false};
    std::atomic<int> sum{0};
    auto body = [&] {
        runs.fetch_add(1);
        inside = true;
        while (!release) {
            std::this_thread::yield();
        }
        return 11;
    };
    std::thread first([&] {
        async::singleflight mine = flights;
        sum += mine.run("t", body).wait();
    });
    while (!inside) {
        std::this_thread::yield();
    }
    std::vector<std::thread> rest;
    std::atomic<int> started{0};
    for (int i : range(6)) {
        (void)i;
        rest.emplace_back([&] {
            async::singleflight mine = flights;
            started.fetch_add(1);
            sum += mine.run("t", body).wait();
        });
    }
    while (started < 6) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(50ms);   // the six joined (the call is in flight until the release)
    release = true;
    first.join();
    for (auto& t : rest) {
        t.join();
    }
    EXPECT_EQ(sum.load(), 77);
    EXPECT_EQ(runs.load(), 1);
}

// A coroutine function from a thread: its task waited for there
TEST(Singleflight_Test, ACoroutineFunctionFromAThread) {
    async::singleflight flights;
    int v = flights.run("c", [] { return []() -> async::task<int> { co_return 3; }(); }).wait();
    EXPECT_EQ(v, 3);
}

#ifdef NDEBUG
// A caller of the key with another result type runs alone
TEST(Singleflight_Test, AnotherTypeRunsAlone) {
    Clocked c;
    calls = 0;
    async::singleflight flights;
    auto t = async::spawn(ask(flights, "k", 1));
    c.clock.advance(100ms);
    string other = flights.run("k", [] { return string("alone"); }).wait();
    EXPECT_EQ(other, "alone");
    c.clock.advance(900ms);
    EXPECT_EQ(t.wait(), 1);
}
#endif

// Copies share the table; == is identity
TEST(Singleflight_Test, CopiesShareTheTable) {
    Clocked c;
    calls = 0;
    async::singleflight a;
    async::singleflight b = a;
    async::singleflight other;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == other);
    auto x = async::spawn(ask(a, "k", 1));
    auto y = async::spawn(ask(b, "k", 2));
    auto z = async::spawn(ask(other, "k", 3));
    c.clock.advance(1s);
    EXPECT_EQ(x.wait(), y.wait());
    EXPECT_EQ(z.wait(), 3);
    EXPECT_EQ(calls.load(), 2);
}

namespace {
    std::atomic<long> stress_calls{0};

    async::task<long> stress_task(async::singleflight flights, int key, int n) {
        long got = 0;
        for (int i : range(n)) {
            (void)i;
            got += co_await flights.run(to_string(key), [key] {
                stress_calls.fetch_add(1);
                return (long)key;
            });
        }
        co_return got;
    }
}

// Threads and tasks on few keys at once: every result the key's, f run
// at most once per call
TEST(Singleflight_Test, ManyCallersManyKeys) {
    stress_calls = 0;
    async::singleflight flights;
    std::atomic<long> total{0};
    std::vector<std::thread> ts;
    for (int t : range(4)) {
        ts.emplace_back([&, t] {
            async::singleflight mine = flights;
            long got = 0;
            for (int i : range(2000)) {
                int key = (t + i) % 3;
                got += mine.run(to_string(key), [key] {
                    stress_calls.fetch_add(1);
                    return (long)key;
                }).wait();
            }
            total += got;
        });
    }
    vector<async::task<long>> tasks;
    for (int k : range(8)) {
        tasks.push_back(async::spawn(stress_task(flights, k % 3, 2000)));
    }
    long want = 0;
    for (int t : range(4)) {
        for (int i : range(2000)) {
            want += (t + i) % 3;
        }
    }
    for (int k : range(8)) {
        want += (long)(k % 3) * 2000;
    }
    for (auto& t : ts) {
        t.join();
    }
    for (auto& t : tasks) {
        total += t.wait();
    }
    EXPECT_EQ(total.load(), want);
    EXPECT_LE(stress_calls.load(), 4 * 2000 + 8 * 2000);
    EXPECT_GT(stress_calls.load(), 0);
    async::scheduler::stop();
}
