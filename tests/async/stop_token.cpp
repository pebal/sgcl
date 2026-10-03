//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

    template<class C>
    concept closable = requires(const C& c) { c.close(); };

    template<class C>
    concept sendable = requires(const C& c) { c.try_send(); c.send(); };

    // A function that only listens takes the receiving end
    bool stopped_now(sgcl::async::receive_channel<void> stop) {
        return stop.closed();
    }
}

// The token's channel is the receiving end of the stop's channel: the
// program may wait on it and select on it, but not close it or send on
// it, which would read as a stop that stopped nothing (the children, the
// deadline)
TEST(StopToken_Test, TheTokensChannelOnlyReceives) {
    sgcl::async::stop_source src;
    auto stop = src.token().channel();
    static_assert(std::is_same_v<decltype(stop), sgcl::async::receive_channel<void>>);
    static_assert(!closable<decltype(stop)>);
    static_assert(!sendable<decltype(stop)>);
    static_assert(closable<sgcl::async::channel<void>> && sendable<sgcl::async::channel<void>>);
    static_assert(sgcl::req::handle<sgcl::async::receive_channel<int>> && sgcl::req::handle<sgcl::async::receive_channel<void>>);
    sgcl::async::stop_source child(src.token());
    EXPECT_FALSE(stop.closed());
    EXPECT_FALSE(stop.try_receive());
    EXPECT_FALSE(stopped_now(stop));
    EXPECT_TRUE(stop == src.token().channel());   // the same channel, the stop's
    src.request_stop();
    EXPECT_TRUE(stop.closed());
    EXPECT_TRUE(stopped_now(stop));
    EXPECT_FALSE(stop.receive().wait());           // closed: at once, nothing
    EXPECT_TRUE(child.stop_requested());
    sgcl::async::channel<void> own(1);             // a channel converts to its receiving end
    EXPECT_FALSE(stopped_now(own));
    sgcl::async::receive_channel<void> end = own;
    EXPECT_TRUE(end == own);
    EXPECT_TRUE(own.try_send());
    EXPECT_TRUE(end.try_receive());
    sgcl::async::channel<int> numbers(2);
    sgcl::async::receive_channel<int> in = numbers;
    numbers.try_send(1);
    numbers.try_send(2);
    numbers.close();
    int sum = 0;
    for (int v : in) {
        sum += v;
    }
    EXPECT_EQ(sum, 3);
    EXPECT_EQ(in.capacity(), 2u);
    EXPECT_TRUE(in.empty());
}

TEST(StopToken_Test, ATokenIsACaseOfASelect) {
    sgcl::async::stop_source src;
    sgcl::async::stop_token tok = src.token();
    EXPECT_TRUE(tok.stop_possible());
    EXPECT_FALSE(tok.stop_requested());
    sgcl::async::channel<int> jobs(4);
    auto t = sgcl::async::spawn([](sgcl::async::channel<int>& jobs, sgcl::async::stop_token tok) -> sgcl::async::task<int> {
        int n = 0;
        bool running = true;
        while (running) {
            co_await sgcl::async::select(jobs.on_receive([&](int) { ++n; }), tok.on_stop([&] { running = false; }));
        }
        co_return n;
    }(jobs, tok));
    jobs.send(1).wait();
    jobs.send(2).wait();
    std::this_thread::sleep_for(20ms);
    src.request_stop();
    EXPECT_EQ(t.wait(), 2);
    EXPECT_TRUE(tok.stop_requested());
    EXPECT_TRUE(src.stop_requested());
    // a thread's select on the token, already stopped: served at once
    bool stopped = false;
    EXPECT_EQ(sgcl::async::select(jobs.on_receive([](int) {}), tok.on_stop([&] { stopped = true; })).wait(), 1u);
    EXPECT_TRUE(stopped);
    sgcl::async::scheduler::stop();
}

TEST(StopToken_Test, ADeadlineIsATimerThatStops) {
    sgcl::async::stop_source src;
    auto t0 = Clock::now();                                // before the timer is armed: its deadline counts from a later moment
    src.stop_after(20ms);
    sgcl::async::stop_source child(src.token());                  // stopped by the parent's deadline too
    sgcl::async::select(src.token().on_stop([] {})).wait();
    EXPECT_GE(Clock::now() - t0, 20ms);
    EXPECT_TRUE(src.stop_requested());                     // requested before the case was served, not after
    for (int i = 0; i < 5000 && !child.stop_requested(); ++i) {   // the children after the parent (Go's order): a moment later on the timer's thread
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_TRUE(child.stop_requested());
    sgcl::async::scheduler::stop();
}

TEST(StopToken_Test, AwaitingTheStop) {
    sgcl::async::stop_source src;
    auto w = sgcl::async::spawn([](sgcl::async::stop_token tok) -> sgcl::async::task<int> {
        co_await tok.stopped();
        co_return 7;
    }(src.token()));
    std::this_thread::sleep_for(10ms);
    src.request_stop();
    EXPECT_EQ(w.wait(), 7);
    sgcl::async::scheduler::stop();
}

TEST(StopToken_Test, ChildrenStopWithTheParent) {
    sgcl::async::stop_source parent;
    sgcl::async::stop_source child(parent.token());
    sgcl::async::stop_source grandchild(child.token());
    sgcl::async::stop_source sibling(parent.token());
    sibling.request_stop();                          // a child on its own: the parent unaffected
    EXPECT_TRUE(sibling.stop_requested());
    EXPECT_FALSE(parent.stop_requested());
    EXPECT_FALSE(child.stop_requested());
    parent.request_stop();
    EXPECT_TRUE(child.stop_requested());
    EXPECT_TRUE(grandchild.stop_requested());
    sgcl::async::stop_source late(parent.token());          // made after the stop: stopped at once
    EXPECT_TRUE(late.stop_requested());
    sgcl::async::stop_token none;
    EXPECT_FALSE(none.stop_possible());
    EXPECT_FALSE(none.stop_requested());
}

TEST(StopToken_Test, ChildrenGoneAreNoBurden) {
    sgcl::async::stop_source parent;
    for (int i = 0; i < 1000; ++i) {
        sgcl::async::stop_source child(parent.token());     // each registered, each gone
    }
    sgcl::collector::force_collect(true);
    parent.request_stop();                           // the weak entries: nothing to stop
    EXPECT_TRUE(parent.stop_requested());
}

// A stop by hand cancels the deadline (Go's cancel() stops the timer of a
// WithDeadline): the timer swept out of its heap with the other cancelled
// ones, and the source's state no longer held by it, where it used to stay
// until the deadline (an hour here; for ever with stop_after(forever))
TEST(StopToken_Test, AStopByHandCancelsTheDeadline) {
    const auto before = sgcl::async::detail::timers_instance().shard_sizes();
    for (int i = 0; i < 5000; ++i) {
        sgcl::async::stop_source src;
        src.stop_after(1h);
        src.request_stop();
    }
    auto after = sgcl::async::detail::timers_instance().shard_sizes();
    for (size_t i = 0; i < after.size(); ++i) {   // a shard sweeps its cancelled once they outnumber its live timers past 64 (timeout.cpp: expect_swept)
        EXPECT_LE(after[i], std::max<size_t>(66, 2 * before[i] + 3)) << "shard " << i << ", before " << before[i];
    }
    const size_t live = live_objects_named("StopState");
    off_frame([] {
        sgcl::async::stop_source src;
        src.stop_after(1h);
        src.stop_at(sgcl::clock::now() + 2h);   // a later deadline beside it: cancelled too, or never armed
        sgcl::async::stop_source child(src.token());
        child.stop_after(3h);                   // a child's deadline, cancelled by the parent's stop
        src.request_stop();
        EXPECT_TRUE(child.stop_requested());
    });
    sgcl::collector::clear_stack();
    EXPECT_EQ(live_objects_named("StopState"), live);   // the states of both gone: no timer holds them
}

// A deadline armed after the stop, or racing it: never left armed
TEST(StopToken_Test, ADeadlineAfterTheStopIsNotArmed) {
    const size_t live = live_objects_named("StopState");
    off_frame([] {
        sgcl::async::stop_source src;
        src.request_stop();
        src.stop_after(1h);
    });
    std::atomic<int> go = {0};
    off_frame([&] {
        for (int i = 0; i < 200; ++i) {
            sgcl::async::stop_source src;
            go.store(0);
            std::thread t([&] {
                go.fetch_add(1);
                while (go.load() < 2) {
                }
                src.request_stop();
            });
            go.fetch_add(1);
            while (go.load() < 2) {
            }
            src.stop_after(1h);
            t.join();
            EXPECT_TRUE(src.stop_requested());
        }
    });
    sgcl::collector::clear_stack();
    EXPECT_EQ(live_objects_named("StopState"), live);
}

TEST(StopToken_Test, ManyTasksStoppedAtOnce) {
    sgcl::async::stop_source src;
    std::vector<sgcl::async::task<int>> tasks;
    for (int i = 0; i < 100; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::stop_token tok, int i) -> sgcl::async::task<int> {
            co_await tok.stopped();
            co_return i;
        }(src.token(), i)));
    }
    std::this_thread::sleep_for(20ms);
    src.request_stop();
    long sum = 0;
    for (auto& t : tasks) {
        sum += t.wait();
    }
    EXPECT_EQ(sum, 99L * 100 / 2);
    sgcl::async::scheduler::stop();
}

// stopped() waits for the stop and gives nothing, in both forms
TEST(StopToken_Test, StoppedGivesNothing) {
    sgcl::async::stop_source src;
    auto tok = src.token();
    static_assert(std::is_void_v<decltype(tok.stopped().wait())>);
    std::thread stopper([&] { std::this_thread::sleep_for(10ms); src.request_stop(); });
    tok.stopped().wait();                                  // a thread: returns once the stop came
    stopper.join();
    EXPECT_TRUE(tok.stop_requested());
    auto t = sgcl::async::spawn([](sgcl::async::stop_token tok) -> sgcl::async::task<> {
        co_await tok.stopped();                            // stopped already: at once
    }(tok));
    t.wait();
    sgcl::async::scheduler::stop();
}
