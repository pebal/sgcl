//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// SleepUntil, At, the aligned Tick, Clock, ManualClock and Signals: the
// facades over sgcl::sleep_until, at, tick, clock, manual_clock, signals
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    using Steady = std::chrono::steady_clock;
}

TEST(Sgcl_TimeAndSignal_Tests, SleepUntilAndAt) {
    using namespace Sgcl;
    auto t0 = Steady::now();
    Task t = Spawn([]() -> Task<int> {
        co_await SleepUntil(Clock::Now() + 20ms);
        co_return 1;
    }());
    EXPECT_EQ(t.Join(), 1);
    EXPECT_GE(Steady::now() - t0, 20ms);
    Task past = Spawn([]() -> Task<int> {
        co_await SleepUntil(Clock::Now() - 1s);   // a point that has passed: no suspension
        co_return 2;
    }());
    EXPECT_EQ(past.Join(), 2);
    auto t1 = Steady::now();
    SleepUntil(Clock::Now() + 10ms).Wait();       // a thread blocks
    Sleep(5ms).Wait();
    EXPECT_GE(Steady::now() - t1, 15ms);
    Ptr at = At(Clock::Now() + 10ms);
    EXPECT_TRUE(at->Receive());
    EXPECT_FALSE(at->Receive());                  // closed
    Channel<int> never;
    bool timed = false;
    EXPECT_EQ(Select(never.OnReceive([](int) {}), Timeout(Clock::Now() + 5ms, [&] { timed = true; })), 1u);
    EXPECT_TRUE(timed);
    Scheduler::Stop();
}

TEST(Sgcl_TimeAndSignal_Tests, ManualClock) {
    using namespace Sgcl;
    ManualClock clock;
    EXPECT_FALSE(clock.IsInstalled());
    clock.Install();
    EXPECT_TRUE(clock.IsInstalled());
    EXPECT_EQ(Clock::Now(), clock.Now());
    std::atomic<bool> woke = {false};
    Task t = Spawn([](std::atomic<bool>& woke) -> Task<int> {
        co_await Sleep(30s);
        woke = true;
        co_return 30;
    }(woke));
    auto w0 = Steady::now();
    clock.Advance(29s);
    EXPECT_FALSE(woke);
    clock.Advance(1s);
    EXPECT_TRUE(woke);
    EXPECT_EQ(t.Join(), 30);
    EXPECT_LT(Steady::now() - w0, 1s);
    // an aligned tick, a deadline
    TimePoint start = clock.Now();
    Ptr tick = Tick(1s, start + 500ms);
    clock.Advance(499ms);
    EXPECT_FALSE(tick->TryReceive());
    clock.AdvanceTo(start + 500ms);
    EXPECT_TRUE(tick->TryReceive());
    tick->Close();
    StopSource src;
    src.StopAfter(1h);
    clock.Advance(1h);
    EXPECT_TRUE(src.IsStopRequested());
    EXPECT_TRUE(clock.Inner().installed());
    clock.Uninstall();
    EXPECT_FALSE(clock.IsInstalled());
    EXPECT_LT(Clock::Now(), start + 1h);          // the steady clock again
    Scheduler::Stop();
}

TEST(Sgcl_TimeAndSignal_Tests, Signals) {
    using namespace Sgcl;
    Ptr sig = Signals({SIGUSR1, SIGUSR2});
    ::raise(SIGUSR2);
    EXPECT_EQ(sig->Receive(), Optional<int>(SIGUSR2));
    Task t = Spawn([](Ptr<Channel<int>> sig) -> Task<int> {
        Optional<int> n = co_await sig->AsyncReceive();
        co_return n ? *n : 0;
    }(sig));
    std::this_thread::sleep_for(10ms);
    ::raise(SIGUSR1);
    EXPECT_EQ(t.Join(), SIGUSR1);
    IgnoreSignals({SIGUSR2});
    ::raise(SIGUSR2);                             // ignored
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(sig->TryReceive());
    ResetSignals();
    struct sigaction sa = {};
    ::sigaction(SIGUSR1, nullptr, &sa);
    EXPECT_EQ(sa.sa_handler, SIG_DFL);
    ::sigaction(SIGUSR2, nullptr, &sa);
    EXPECT_EQ(sa.sa_handler, SIG_DFL);
    Scheduler::Stop();
}
