//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// TaskGroup, Timeout, WithDeadline, TimedOut: the facades over
// sgcl::task_group and sgcl/async/timeout.h
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    Task<> Child(int ms, Atomic<int>& finished) {
        co_await Sleep(std::chrono::milliseconds(ms));
        ++finished;
    }

    Task<> Obedient(StopToken tok, Atomic<int>& stopped, Atomic<int>& finished) {
        co_await tok.Stopped();
        if (tok.IsStopRequested()) {
            ++stopped;
        }
        ++finished;
    }

    Task<> Failing(int ms, Atomic<int>& finished) {
        co_await Sleep(std::chrono::milliseconds(ms));
        ++finished;
        throw std::runtime_error("failing");
    }

    Task<int> Number(int n, int ms) {
        co_await Sleep(std::chrono::milliseconds(ms));
        co_return n;
    }

    Task<String> HowItEnded(StopToken tok, int ms) {
        size_t which = co_await AsyncSelect(tok.OnStop([] {}), Timeout(std::chrono::milliseconds(ms), [] {}));
        co_return which == 0 ? "stopped" : "finished";
    }
}

TEST(Sgcl_TaskGroup_Tests, ChildrenAllFinish) {
    using namespace Sgcl;
    Atomic<int> finished = {0};
    TaskGroup g;
    EXPECT_FALSE(g.IsStopRequested());
    EXPECT_TRUE(g.Token().IsStopPossible());
    for (int i : Range(8)) {
        g.Spawn(Child(i, finished));
    }
    EXPECT_LE(g.Count(), 8u);
    g.Wait();
    EXPECT_EQ(finished.Load(), 8);
    EXPECT_EQ(g.Count(), 0u);
    g.Spawn(Number(1, 1));   // a task of a value: its result dropped
    g.Wait();
    Scheduler::Stop();
}

TEST(Sgcl_TaskGroup_Tests, OneThrowingStopsTheOthers) {
    using namespace Sgcl;
    Atomic<int> stopped = {0}, finished = {0};
    TaskGroup g;
    for ([[maybe_unused]] int i : Range(7)) {
        g.Spawn(Obedient(g.Token(), stopped, finished));
    }
    g.Spawn(Failing(10, finished));
    EXPECT_THROW(g.Wait(), std::runtime_error);
    EXPECT_EQ(finished.Load(), 8);
    EXPECT_EQ(stopped.Load(), 7);
    EXPECT_TRUE(g.IsStopRequested());
    Scheduler::Stop();
}

TEST(Sgcl_TaskGroup_Tests, TheThreeWaysAndANestedGroup) {
    using namespace Sgcl;
    Atomic<int> finished = {0};
    TaskGroup g;
    for ([[maybe_unused]] int i : Range(4)) {
        g.Spawn(Child(5, finished));
    }
    auto t = Spawn([](TaskGroup& g) -> Task<int> {
        co_await g.AsyncWait();
        co_return g.Count() == 0 ? 1 : 0;
    }(g));
    EXPECT_EQ(t.Join(), 1);
    TaskGroup h;
    for ([[maybe_unused]] int i : Range(4)) {
        h.Spawn(Child(5, finished));
    }
    bool done = false;
    Channel<int> never;
    EXPECT_EQ(Select(never.OnReceive([](int) {}), h.OnDone([&] { done = true; })), 1u);
    EXPECT_TRUE(done);
    h.Wait();
    EXPECT_EQ(finished.Load(), 8);
    // a nested group under a stopped parent
    StopSource parent;
    Atomic<int> stopped = {0}, inner_finished = {0};
    TaskGroup outer(parent.Token());
    outer.Spawn([](StopToken tok, Atomic<int>& stopped, Atomic<int>& finished) -> Task<> {
        TaskGroup inner(tok);
        for ([[maybe_unused]] int i : Range(3)) {
            inner.Spawn(Obedient(inner.Token(), stopped, finished));
        }
        co_await inner.AsyncWait();
    }(outer.Token(), stopped, inner_finished));
    std::this_thread::sleep_for(10ms);
    parent.RequestStop();
    outer.Wait();
    EXPECT_EQ(stopped.Load(), 3);
    EXPECT_EQ(inner_finished.Load(), 3);
    Scheduler::Stop();
}

TEST(Sgcl_Timeout_Tests, TimeoutAndWithDeadline) {
    using namespace Sgcl;
    Optional<int> r = Timeout(Number(7, 5), 200ms).Join();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 7);
    EXPECT_EQ(Timeout(Number(7, 200), 20ms).Join(), None);
    Atomic<int> finished = {0};
    EXPECT_TRUE(Timeout(Child(5, finished), 200ms).Join());        // a Task<>: whether it finished
    EXPECT_FALSE(Timeout(Child(200, finished), 20ms).Join());
    EXPECT_EQ(WithDeadline(Number(3, 5), 200ms).Join(), 3);
    EXPECT_THROW(WithDeadline(Number(3, 200), 20ms).Join(), TimedOut);
    EXPECT_THROW(WithDeadline(Number(3, 200), 20ms).Join(), std::runtime_error);
    // the loser stopped through its source
    StopSource src;
    EXPECT_EQ(Timeout(HowItEnded(src.Token(), 500), 20ms, src).Join(), None);
    EXPECT_TRUE(src.IsStopRequested());
    StopSource src2;
    EXPECT_THROW(WithDeadline(HowItEnded(src2.Token(), 500), 20ms, src2).Join(), TimedOut);
    EXPECT_TRUE(src2.IsStopRequested());
    // a token as the deadline
    StopSource src3;
    src3.StopAfter(20ms);
    EXPECT_THROW(WithDeadline(Number(3, 500), src3.Token()).Join(), TimedOut);
    StopSource src4;
    src4.StopAfter(200ms);
    EXPECT_EQ(WithDeadline(Number(4, 5), src4.Token()).Join(), 4);
    // from a task, and a zero timeout
    auto t = Spawn([]() -> Task<int> {
        auto a = co_await Timeout(Number(8, 2), 200ms);
        auto b = co_await Timeout(Number(9, 50), 0ms);
        co_return (a ? *a : -1) + (b ? *b : 0);
    }());
    EXPECT_EQ(t.Join(), 8);
    std::this_thread::sleep_for(300ms);                              // the losers finish
    Scheduler::Stop();
}
