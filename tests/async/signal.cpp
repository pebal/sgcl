//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// sgcl::async::signals: the signals of the process as a channel, received by a
// task, a thread, a select; reset and ignore
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    void (*disposition(int n))(int) {
        struct sigaction sa = {};
        ::sigaction(n, nullptr, &sa);
        return sa.sa_handler;
    }
}

TEST(Signal_Test, ReceivedByAThread) {
    auto ch = sgcl::async::signals({SIGUSR1});
    EXPECT_NE(disposition(SIGUSR1), SIG_DFL);   // the handler installed
    ::raise(SIGUSR1);
    auto n = ch->receive().wait();
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, SIGUSR1);
    EXPECT_FALSE(ch->try_receive());            // one delivery, one element
    sgcl::async::reset_signals({SIGUSR1});
    EXPECT_EQ(disposition(SIGUSR1), SIG_DFL);   // the disposition given back
}

TEST(Signal_Test, ReceivedByATask) {
    auto ch = sgcl::async::signals({SIGUSR1, SIGUSR2});
    auto t = sgcl::async::spawn([](sgcl::tracked_ptr<sgcl::async::channel<int>> ch) -> sgcl::async::task<int> {
        auto a = co_await ch->receive();   // no thread held while the process waits for the signal
        auto b = co_await ch->receive();
        co_return (a ? *a : 0) * 100 + (b ? *b : 0);
    }(ch));
    std::this_thread::sleep_for(10ms);           // the task at its wait (not needed for the delivery: the channel holds one)
    ::raise(SIGUSR2);
    std::this_thread::sleep_for(10ms);
    ::raise(SIGUSR1);
    EXPECT_EQ(t.wait(), SIGUSR2 * 100 + SIGUSR1);
    sgcl::async::reset_signals({SIGUSR1, SIGUSR2});
    sgcl::async::scheduler::stop();
}

TEST(Signal_Test, InASelect) {
    auto sig = sgcl::async::signals({SIGUSR1});
    sgcl::async::channel<int> data(1);
    int got = -1;
    // the data first
    data.send(7).wait();
    EXPECT_EQ(sgcl::async::select(data.on_receive([&](int v) { got = v; }), sig->on_receive([&](int n) { got = -n; })).wait(), 0u);
    EXPECT_EQ(got, 7);
    // the signal, in a task: the shutdown idiom
    auto t = sgcl::async::spawn([](sgcl::tracked_ptr<sgcl::async::channel<int>> sig, sgcl::async::channel<int>& data) -> sgcl::async::task<int> {
        int seen = 0;
        bool running = true;
        while (running) {
            co_await sgcl::async::select(
                data.on_receive([&](int) { ++seen; }),
                sig->on_receive([&](int) { running = false; })
            );
        }
        co_return seen;
    }(sig, data));
    data.send(1).wait();
    data.send(2).wait();
    std::this_thread::sleep_for(10ms);
    ::raise(SIGUSR1);
    EXPECT_EQ(t.wait(), 2);
    sgcl::async::reset_signals({SIGUSR1});
    sgcl::async::scheduler::stop();
}

TEST(Signal_Test, TwoChannelsForOneSignalBothDelivered) {
    auto a = sgcl::async::signals({SIGUSR1});
    auto b = sgcl::async::signals({SIGUSR1, SIGUSR2});
    ::raise(SIGUSR1);
    EXPECT_EQ(a->receive().wait(), sgcl::optional<int>(SIGUSR1));
    EXPECT_EQ(b->receive().wait(), sgcl::optional<int>(SIGUSR1));
    ::raise(SIGUSR2);
    EXPECT_EQ(b->receive().wait(), sgcl::optional<int>(SIGUSR2));
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(a->try_receive());              // not registered for SIGUSR2
    sgcl::async::reset_signals();                       // every number
    EXPECT_EQ(disposition(SIGUSR1), SIG_DFL);
    EXPECT_EQ(disposition(SIGUSR2), SIG_DFL);
}

TEST(Signal_Test, ResetGivesTheDispositionBack) {
    ::signal(SIGUSR1, SIG_IGN);                  // the program's own disposition, before the registration
    auto ch = sgcl::async::signals({SIGUSR1});
    EXPECT_NE(disposition(SIGUSR1), SIG_IGN);
    ::raise(SIGUSR1);
    EXPECT_TRUE(ch->receive().wait());
    sgcl::async::reset_signals({SIGUSR1});
    EXPECT_EQ(disposition(SIGUSR1), SIG_IGN);    // what it was
    ::raise(SIGUSR1);                            // ignored: nothing on the channel
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(ch->try_receive());
    EXPECT_FALSE(ch->closed());                  // forgotten, not closed
    ::signal(SIGUSR1, SIG_DFL);
    // ignore, then reset: the disposition from before the first registration
    auto again = sgcl::async::signals({SIGUSR1});
    sgcl::async::ignore_signals({SIGUSR1});
    EXPECT_EQ(disposition(SIGUSR1), SIG_IGN);
    ::raise(SIGUSR1);
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(again->try_receive());
    sgcl::async::reset_signals({SIGUSR1});
    EXPECT_EQ(disposition(SIGUSR1), SIG_DFL);
}

TEST(Signal_Test, ABurstIsCoalesced) {
    auto one = sgcl::async::signals({SIGUSR1});         // the channel holds one: a burst is one delivery for a receiver that was not there
    auto four = sgcl::async::signals({SIGUSR1}, 4);     // four held
    for (int i : sgcl::range(10)) {
        (void)i;
        ::raise(SIGUSR1);
    }
    std::this_thread::sleep_for(20ms);
    int a = 0, b = 0;
    while (one->try_receive()) {
        ++a;
    }
    while (four->try_receive()) {
        ++b;
    }
    EXPECT_EQ(a, 1);
    EXPECT_GE(b, 1);
    EXPECT_LE(b, 4);
    sgcl::async::reset_signals({SIGUSR1});
}
