//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// sgcl::clock, sleep_until, at, the aligned tick, and the manual clock of
// a test: time moved by advance, every timer due fired with no real waiting
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    using Steady = std::chrono::steady_clock;
}

TEST(Clock_Test, TheSteadyClockUnlessAManualOneIsInstalled) {
    auto a = Steady::now();
    auto b = sgcl::clock::now();
    auto c = Steady::now();
    EXPECT_LE(a, b);
    EXPECT_LE(b, c);
    {
        sgcl::async::manual_clock clock;
        EXPECT_FALSE(clock.installed());
        clock.install();
        EXPECT_TRUE(clock.installed());
        auto t = sgcl::clock::now();
        EXPECT_EQ(t, clock.now());
        std::this_thread::sleep_for(2ms);
        EXPECT_EQ(sgcl::clock::now(), t);         // time does not move on its own
        clock.advance(1h);
        EXPECT_EQ(sgcl::clock::now(), t + 1h);    // only by an advance
        EXPECT_GE(t, c);                          // it started from the steady clock's now
    }
    EXPECT_LT(sgcl::clock::now(), c + 1h);        // uninstalled by the destructor: the steady clock again
}

TEST(Clock_Test, SleepUntilAPointInThePastReturnsAtOnce) {
    auto t0 = Steady::now();
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_await sgcl::async::sleep_until(sgcl::clock::now() - 1s);
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    EXPECT_LT(Steady::now() - t0, 500ms);
    sgcl::async::sleep_until(sgcl::clock::now() - 1s).wait();   // a thread's, the same
    sgcl::async::sleep(-1s).wait();
    EXPECT_LT(Steady::now() - t0, 500ms);
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, SleepUntilAPointInTheFutureWaits) {
    auto t0 = Steady::now();
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_await sgcl::async::sleep_until(sgcl::clock::now() + 30ms);
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    EXPECT_GE(Steady::now() - t0, 30ms);
    auto t1 = Steady::now();
    sgcl::async::sleep_until(sgcl::clock::now() + 20ms).wait();   // a thread blocks
    EXPECT_GE(Steady::now() - t1, 20ms);
    auto t2 = Steady::now();
    sgcl::async::sleep(10ms).wait();
    EXPECT_GE(Steady::now() - t2, 10ms);
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, AtIsOneSignalAtThePointThenClosed) {
    auto t0 = Steady::now();
    auto ch = sgcl::async::at(sgcl::clock::now() + 20ms);
    EXPECT_TRUE(ch->receive().wait());
    EXPECT_GE(Steady::now() - t0, 20ms);
    EXPECT_FALSE(ch->receive().wait());   // closed
    auto past = sgcl::async::at(sgcl::clock::now() - 1s);   // a point that has passed: at once
    EXPECT_TRUE(past->receive().wait());
    EXPECT_FALSE(past->receive().wait());
    // a timeout case at a point
    sgcl::async::channel<int> never;
    bool timed = false;
    EXPECT_EQ(sgcl::async::select(never.on_receive([](int) {}), sgcl::async::timeout(sgcl::clock::now() + 10ms, [&] { timed = true; })).wait(), 1u);
    EXPECT_TRUE(timed);
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, TickAlignedToAPoint) {
    sgcl::async::manual_clock clock;
    clock.install();
    auto start = clock.now();
    auto tk = sgcl::async::tick(1s, start + 500ms);   // the first tick at +500 ms, then every second
    clock.advance(499ms);
    EXPECT_FALSE(tk->try_receive());
    clock.advance(1ms);
    EXPECT_TRUE(tk->try_receive());
    EXPECT_FALSE(tk->try_receive());
    clock.advance(1s);
    EXPECT_TRUE(tk->try_receive());              // at +1500 ms
    tk->close();
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, ASleepOfThirtySecondsCompletesOnAdvance) {
    sgcl::async::manual_clock clock;
    clock.install();
    std::atomic<bool> woke = {false};
    auto t = sgcl::async::spawn([](std::atomic<bool>& woke) -> sgcl::async::task<int> {
        co_await sgcl::async::sleep(30s);
        woke = true;
        co_return 30;
    }(woke));
    auto w0 = Steady::now();
    clock.advance(29s);
    EXPECT_FALSE(woke);                          // not yet
    clock.advance(1s);
    EXPECT_TRUE(woke);                           // advance returned with the task run to its end
    EXPECT_EQ(t.wait(), 30);
    EXPECT_LT(Steady::now() - w0, 1s);           // the whole thing in wall time: milliseconds at the most
    // a chain of sleeps: each advance runs the task to its next sleep before it returns
    std::atomic<int> laps = {0};
    auto u = sgcl::async::spawn([](std::atomic<int>& laps) -> sgcl::async::task<> {
        for (int i : sgcl::range(3)) {
            (void)i;
            co_await sgcl::async::sleep(10min);
            ++laps;
        }
    }(laps));
    for (int i : sgcl::range(3)) {
        clock.advance(10min);
        EXPECT_EQ(laps, i + 1);
    }
    u.wait();
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, TickFiresPerAdvance) {
    sgcl::async::manual_clock clock;
    clock.install();
    auto tk = sgcl::async::tick(1s);
    int n = 0;
    for (int i : sgcl::range(5)) {              // one advance per period: one tick each
        (void)i;
        clock.advance(1s);
        if (tk->try_receive()) {
            ++n;
        }
    }
    EXPECT_EQ(n, 5);
    EXPECT_FALSE(tk->try_receive());
    clock.advance(3s);                           // three periods in one advance, nobody receiving in between:
    EXPECT_TRUE(tk->try_receive());              // coalesced to the one the channel holds, as a slow receiver's are
    EXPECT_FALSE(tk->try_receive());
    clock.advance(999ms);
    EXPECT_FALSE(tk->try_receive());             // the period is kept: the next is at +4 s, not 1 s after the last receive
    clock.advance(1ms);
    EXPECT_TRUE(tk->try_receive());
    tk->close();
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, AfterOrderedAcrossDeadlinesInOneAdvance) {
    sgcl::async::manual_clock clock;
    clock.install();
    std::atomic<int> done[5] = {};
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 4; i >= 0; --i) {              // armed in reverse order of their deadlines: 5 s first, 1 s last
        tasks.push_back(sgcl::async::spawn([](int i, std::atomic<int>* done) -> sgcl::async::task<> {
            auto ch = sgcl::async::after(std::chrono::seconds(1 + i));
            co_await ch->receive();
            done[i] = 1;
        }(i, done)));
    }
    clock.advance(2500ms);                       // the deadlines at 1 and 2 s passed in one step, the rest not
    EXPECT_TRUE(done[0] && done[1]);
    EXPECT_FALSE(done[2] || done[3] || done[4]);
    clock.advance(1500ms);                       // 4 s: two more
    EXPECT_TRUE(done[2] && done[3]);
    EXPECT_FALSE(done[4]);
    clock.advance(10s);
    EXPECT_TRUE(done[4]);
    for (auto& t : tasks) {
        t.wait();
    }
    // the same for a thread receiving on the channels: the earlier first
    auto a = sgcl::async::after(2s);
    auto b = sgcl::async::after(1s);
    clock.advance(1s);
    EXPECT_TRUE(b->try_receive());
    EXPECT_FALSE(a->try_receive());
    clock.advance(1s);
    EXPECT_TRUE(a->try_receive());
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, TimeoutCaseFiredByTheClock) {
    sgcl::async::manual_clock clock;
    clock.install();
    sgcl::async::channel<int> never;
    std::atomic<bool> done = {false};
    auto t = sgcl::async::spawn([](sgcl::async::channel<int>& never, std::atomic<bool>& done) -> sgcl::async::task<size_t> {
        auto which = co_await sgcl::async::select(never.on_receive([](int) {}), sgcl::async::timeout(1h, [] {}));
        done = true;
        co_return which;
    }(never, done));
    clock.advance(59min);
    EXPECT_FALSE(done);                          // the task still waits
    clock.advance(1min);
    EXPECT_TRUE(done);
    EXPECT_EQ(t.wait(), 1u);                     // the timeout won
    // a thread's select: its case armed here, served when the clock reaches the deadline
    auto deadline = sgcl::async::timeout(2h, [] {});
    std::atomic<size_t> which = {9};
    std::thread th([&] {
        which = sgcl::async::select(never.on_receive([](int) {}), std::move(deadline)).wait();
    });
    clock.advance(2h);
    th.join();
    EXPECT_EQ(which, 1u);
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, StopAfterThroughTheManualClock) {
    sgcl::async::manual_clock clock;
    clock.install();
    sgcl::async::stop_source source;
    source.stop_after(24h);
    auto token = source.token();
    auto t = sgcl::async::spawn([](sgcl::async::stop_token token) -> sgcl::async::task<bool> {
        co_await token.stopped();
        co_return token.stop_requested();
    }(token));
    clock.advance(23h);
    EXPECT_FALSE(token.stop_requested());
    clock.advance(1h);
    EXPECT_TRUE(token.stop_requested());
    EXPECT_TRUE(t.wait());
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, UninstallRestoresRealTime) {
    auto real = Steady::now();
    {
        sgcl::async::manual_clock clock;
        clock.install();
        clock.advance(1h);
        EXPECT_GE(sgcl::clock::now(), real + 1h);
        clock.uninstall();
        EXPECT_FALSE(clock.installed());
        EXPECT_LT(sgcl::clock::now(), real + 1h);
        clock.uninstall();                       // twice: nothing
    }
    // a timer armed under the real clock after that fires in real time
    auto t0 = Steady::now();
    EXPECT_TRUE(sgcl::async::after(10ms)->receive().wait());
    EXPECT_GE(Steady::now() - t0, 10ms);
    // and a sleep in a task
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_await sgcl::async::sleep(10ms);
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

TEST(Clock_Test, AdvanceWithNoTimerThread) {
    sgcl::async::scheduler::stop();                     // the timer thread joined, if it ran
    sgcl::async::manual_clock clock;
    clock.install();
    clock.advance(1s);                           // nothing to wait for
    auto t = clock.now();
    clock.advance_to(t + 2s);
    EXPECT_EQ(clock.now(), t + 2s);
    EXPECT_TRUE(sgcl::async::at(t + 1s)->receive().wait());    // a point that has passed under the manual clock: at once
}

// A cache's time to live runs on the library's clock, so the manual clock
// moves it on without a sleep
TEST(Clock_Test, ACachesTimeToLiveFollowsTheManualClock) {
    sgcl::async::manual_clock clock;
    clock.install();
    sgcl::concurrent::cache<int, int> c(10, 30ms);
    c.put(1, 10);
    EXPECT_EQ(c.get(1), 10);
    clock.advance(20ms);
    EXPECT_EQ(c.get(1), 10);                               // 20 ms: still fresh
    clock.advance(20ms);
    EXPECT_FALSE(c.get(1));                                // 40 ms since the put: stale, and no wall time passed
}
