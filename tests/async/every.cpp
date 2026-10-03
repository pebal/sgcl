//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async::every: f called every d until the source it returns is stopped.
// Under a manual clock: the number of calls an advance makes, a stop
// before the first tick, from inside f and through a copy of the source,
// a parent's stop and an empty parent, a slow coroutine f (the ticks it
// misses held as one, never a backlog), a period of zero or less and of
// duration::max() (never called), an f that throws (its exception to
// on_unhandled, no call after it); in real time, a closure's captures
// living as long as the loop.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    namespace async = sgcl::async;
    using Steady = std::chrono::steady_clock;

    template<class F>
    bool soon(F&& f) {
        auto until = Steady::now() + 5s;
        while (!f()) {
            if (Steady::now() > until) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    std::atomic<int> unhandled{0};

    void record(std::exception_ptr) {
        unhandled.fetch_add(1);
    }
}

// One call per period the clock passes; none after the stop
TEST(Every_Tests, OneCallPerPeriodUntilTheStop) {
    async::manual_clock clock;
    clock.install();
    std::atomic<int> calls{0};
    auto s = async::every(1s, [&] { calls.fetch_add(1); });
    EXPECT_FALSE(s.stop_requested());
    clock.advance(999ms);
    EXPECT_EQ(calls.load(), 0);   // the first call after a whole period, as tick's first signal
    clock.advance(1ms);
    EXPECT_EQ(calls.load(), 1);
    clock.advance(1s);
    clock.advance(1s);
    EXPECT_EQ(calls.load(), 3);
    s.request_stop();
    clock.advance(1s);   // the loop sees the stop: the tick of this advance and every later one is not a call
    clock.advance(5s);
    EXPECT_EQ(calls.load(), 3);
    s.request_stop();    // a second stop does nothing
    clock.uninstall();
    async::scheduler::stop();
}

// A stop before the first tick, a stop through a copy of the source, a
// stop from inside f: no call after it
TEST(Every_Tests, StopsBeforeTheFirstTickThroughACopyAndFromInside) {
    async::manual_clock clock;
    clock.install();
    std::atomic<int> early{0};
    auto first = async::every(1s, [&] { early.fetch_add(1); });
    first.request_stop();
    clock.advance(3s);
    EXPECT_EQ(early.load(), 0);

    std::atomic<int> copied{0};
    auto second = async::every(1s, [&] { copied.fetch_add(1); });
    auto copy = second;
    clock.advance(1s);
    copy.request_stop();
    EXPECT_TRUE(second.stop_requested());
    clock.advance(3s);
    EXPECT_EQ(copied.load(), 1);

    std::atomic<int> inside{0};
    sgcl::tracked_ptr holder = sgcl::make_tracked<async::stop_source>();   // managed: the closure lies in the loop's frame
    *holder = async::every(1s, [&inside, holder] {
        inside.fetch_add(1);
        holder->request_stop();
    });
    clock.advance(1s);
    clock.advance(1s);
    clock.advance(1s);
    EXPECT_EQ(inside.load(), 1);
    clock.uninstall();
    async::scheduler::stop();
}

// The source is the child of a parent token: the parent's stop ends the
// calls, the child's own stop leaves the parent; an empty parent token is
// no parent
TEST(Every_Tests, AParentsStopEndsTheCalls) {
    async::manual_clock clock;
    clock.install();
    async::stop_source parent;
    std::atomic<int> a{0}, b{0}, c{0};
    auto sa = async::every(1s, [&] { a.fetch_add(1); }, parent.token());
    auto sb = async::every(1s, [&] { b.fetch_add(1); }, parent.token());
    auto sc = async::every(1s, [&] { c.fetch_add(1); }, async::stop_token());
    clock.advance(1s);
    sb.request_stop();
    EXPECT_FALSE(parent.stop_requested());
    clock.advance(1s);
    parent.request_stop();
    EXPECT_TRUE(sa.stop_requested());
    EXPECT_FALSE(sc.stop_requested());
    clock.advance(2s);
    EXPECT_EQ(a.load(), 2);
    EXPECT_EQ(b.load(), 1);
    EXPECT_EQ(c.load(), 4);
    sc.request_stop();
    clock.advance(1s);
    EXPECT_EQ(c.load(), 4);
    clock.uninstall();
    async::scheduler::stop();
}

// A coroutine f is awaited before the next tick; the ticks that come
// while it waits are held as one: one more call after it, not a backlog
TEST(Every_Tests, ASlowCoroutineMissesTicksWithoutABacklog) {
    async::manual_clock clock;
    clock.install();
    async::channel<void> gate(1);
    std::atomic<int> started{0}, finished{0};
    auto s = async::every(1s, [&]() -> async::task<> {
        started.fetch_add(1);
        co_await gate.receive();
        finished.fetch_add(1);
    });
    clock.advance(1s);
    EXPECT_EQ(started.load(), 1);
    clock.advance(5s);   // five ticks while the call waits
    EXPECT_EQ(started.load(), 1);
    EXPECT_TRUE(gate.try_send());
    EXPECT_TRUE(soon([&] { return started.load() == 2; }));   // the one tick held
    EXPECT_EQ(finished.load(), 1);
    EXPECT_TRUE(gate.try_send());
    EXPECT_TRUE(soon([&] { return finished.load() == 2; }));
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(started.load(), 2);   // no tick left behind it
    s.request_stop();
    clock.advance(1s);
    EXPECT_EQ(started.load(), 2);
    clock.uninstall();
    async::scheduler::stop();
}

// A period of zero or less never ticks, as tick's; one of duration::max()
// never comes; the loop still ends at the stop
TEST(Every_Tests, APeriodOfZeroOrLessOrOfMaxNeverCalls) {
    async::manual_clock clock;
    clock.install();
    std::atomic<int> calls{0};
    async::stop_source sources[] = {
        async::every(sgcl::duration::zero(), [&] { calls.fetch_add(1); }),
        async::every(-1ms, [&] { calls.fetch_add(1); }),
        async::every(sgcl::duration::min(), [&] { calls.fetch_add(1); }),
        async::every(sgcl::duration::max(), [&] { calls.fetch_add(1); }),
    };
    clock.advance(24h);
    EXPECT_EQ(calls.load(), 0);
    for (auto& s : sources) {
        s.request_stop();
    }
    clock.advance(1s);
    EXPECT_EQ(calls.load(), 0);
    clock.uninstall();
    async::scheduler::stop();
}

// An f that throws ends its loop: the exception goes to on_unhandled, as
// a go'ed task's does, once; no call after it
TEST(Every_Tests, AThrowingCallEndsTheLoop) {
    auto previous = async::on_unhandled(&record);
    unhandled = 0;
    async::manual_clock clock;
    clock.install();
    std::atomic<int> calls{0};
    auto s = async::every(1s, [&] {
        calls.fetch_add(1);
        throw std::runtime_error("every");
    });
    clock.advance(1s);
    EXPECT_TRUE(soon([] { return unhandled.load() == 1; }));
    clock.advance(3s);
    EXPECT_EQ(calls.load(), 1);
    EXPECT_EQ(unhandled.load(), 1);
    EXPECT_FALSE(s.stop_requested());   // the loop ended by itself, the source as it was
    clock.uninstall();
    async::scheduler::stop();
    async::on_unhandled(previous);
}

// In real time: a closure with captures of its own, kept in the loop's
// frame, called again and again; after the stop, at most a call already
// started ends
TEST(Every_Tests, InRealTimeWithAClosuresCaptures) {
    auto calls = std::make_shared<std::atomic<int>>(0);
    sgcl::string name("tick");
    auto s = async::every(2ms, [calls, name] {
        if (name == "tick") {
            calls->fetch_add(1);
        }
    });
    EXPECT_TRUE(soon([&] { return calls->load() >= 5; }));
    s.request_stop();
    std::this_thread::sleep_for(10ms);
    int after = calls->load();
    std::this_thread::sleep_for(30ms);
    EXPECT_EQ(calls->load(), after);
    async::scheduler::stop();
}
