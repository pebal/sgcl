//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;
}

TEST(StopToken_Test, ATokenIsACaseOfASelect) {
    sgcl::stop_source src;
    sgcl::stop_token tok = src.token();
    EXPECT_TRUE(tok.stop_possible());
    EXPECT_FALSE(tok.stop_requested());
    sgcl::channel<int> jobs(4);
    auto t = sgcl::spawn([](sgcl::channel<int>& jobs, sgcl::stop_token tok) -> sgcl::task<int> {
        int n = 0;
        bool running = true;
        while (running) {
            co_await sgcl::async_select(jobs.on_receive([&](int) { ++n; }), tok.on_stop([&] { running = false; }));
        }
        co_return n;
    }(jobs, tok));
    jobs.send(1);
    jobs.send(2);
    std::this_thread::sleep_for(20ms);
    src.request_stop();
    EXPECT_EQ(t.join(), 2);
    EXPECT_TRUE(tok.stop_requested());
    EXPECT_TRUE(src.stop_requested());
    // a thread's select on the token, already stopped: served at once
    bool stopped = false;
    EXPECT_EQ(sgcl::select(jobs.on_receive([](int) {}), tok.on_stop([&] { stopped = true; })), 1u);
    EXPECT_TRUE(stopped);
    sgcl::scheduler::stop();
}

TEST(StopToken_Test, ADeadlineIsATimerThatStops) {
    sgcl::stop_source src;
    auto t0 = Clock::now();                                // before the timer is armed: its deadline counts from a later moment
    src.stop_after(20ms);
    sgcl::stop_source child(src.token());                  // stopped by the parent's deadline too
    sgcl::select(src.token().on_stop([] {}));
    EXPECT_GE(Clock::now() - t0, 20ms);
    EXPECT_TRUE(src.stop_requested());                     // requested before the case was served, not after
    for (int i = 0; i < 5000 && !child.stop_requested(); ++i) {   // the children after the parent (Go's order): a moment later on the timer's thread
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_TRUE(child.stop_requested());
    sgcl::scheduler::stop();
}

TEST(StopToken_Test, AwaitingTheStop) {
    sgcl::stop_source src;
    auto w = sgcl::spawn([](sgcl::stop_token tok) -> sgcl::task<int> {
        co_await tok.stopped();
        co_return 7;
    }(src.token()));
    std::this_thread::sleep_for(10ms);
    src.request_stop();
    EXPECT_EQ(w.join(), 7);
    sgcl::scheduler::stop();
}

TEST(StopToken_Test, ChildrenStopWithTheParent) {
    sgcl::stop_source parent;
    sgcl::stop_source child(parent.token());
    sgcl::stop_source grandchild(child.token());
    sgcl::stop_source sibling(parent.token());
    sibling.request_stop();                          // a child on its own: the parent unaffected
    EXPECT_TRUE(sibling.stop_requested());
    EXPECT_FALSE(parent.stop_requested());
    EXPECT_FALSE(child.stop_requested());
    parent.request_stop();
    EXPECT_TRUE(child.stop_requested());
    EXPECT_TRUE(grandchild.stop_requested());
    sgcl::stop_source late(parent.token());          // made after the stop: stopped at once
    EXPECT_TRUE(late.stop_requested());
    sgcl::stop_token none;
    EXPECT_FALSE(none.stop_possible());
    EXPECT_FALSE(none.stop_requested());
}

TEST(StopToken_Test, ChildrenGoneAreNoBurden) {
    sgcl::stop_source parent;
    for (int i = 0; i < 1000; ++i) {
        sgcl::stop_source child(parent.token());     // each registered, each gone
    }
    sgcl::collector::force_collect(true);
    parent.request_stop();                           // the weak entries: nothing to stop
    EXPECT_TRUE(parent.stop_requested());
}

TEST(StopToken_Test, ManyTasksStoppedAtOnce) {
    sgcl::stop_source src;
    std::vector<sgcl::task<int>> tasks;
    for (int i = 0; i < 100; ++i) {
        tasks.push_back(sgcl::spawn([](sgcl::stop_token tok, int i) -> sgcl::task<int> {
            co_await tok.stopped();
            co_return i;
        }(src.token(), i)));
    }
    std::this_thread::sleep_for(20ms);
    src.request_stop();
    long sum = 0;
    for (auto& t : tasks) {
        sum += t.join();
    }
    EXPECT_EQ(sum, 99L * 100 / 2);
    sgcl::scheduler::stop();
}
