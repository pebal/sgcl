//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

    task<int> number(int n, int ms) {
        co_await sgcl::async::sleep(std::chrono::milliseconds(ms));
        co_return n;
    }

    task<std::string> text_task(int ms) {
        co_await sgcl::async::sleep(std::chrono::milliseconds(ms));
        co_return "text";
    }

    task<> nothing(int ms, sgcl::atomic<int>& finished) {
        co_await sgcl::async::sleep(std::chrono::milliseconds(ms));
        ++finished;
    }

    task<int> failing() {
        throw std::runtime_error("failing");
        co_return 0;
    }

    // A task that works until told to stop: how it ended is its result
    task<std::string> obedient(sgcl::async::stop_token tok, int ms) {
        size_t which = co_await sgcl::async::select(
            tok.on_stop([] {}),
            sgcl::async::timeout(std::chrono::milliseconds(ms), [] {}));
        co_return which == 0 ? "stopped" : "finished";
    }

    task<std::string> obedient_into(sgcl::async::stop_token tok, int ms, sgcl::atomic<int>& stopped) {
        auto how = co_await obedient(tok, ms);
        if (how == "stopped") {
            ++stopped;
        }
        co_return how;
    }

    // A task of nothing that works until told to stop: counted when it ends, however it ended
    task<> obedient_nothing(sgcl::async::stop_token tok, int ms, sgcl::atomic<int>& finished) {
        co_await obedient(tok, ms);
        ++finished;
    }
}

TEST(Timeout_Tests, ATaskThatFinishesInTime) {
    auto r = sgcl::async::with_timeout(number(7, 5), 200ms).wait();    // join starts the timeout, which starts the task
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 7);
    auto s = sgcl::async::with_timeout(sgcl::async::spawn(text_task(5)), 200ms).wait();   // a task spawned already
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(*s, "text");
    sgcl::atomic<int> finished = {0};
    EXPECT_TRUE(sgcl::async::with_timeout(nothing(5, finished), 200ms).wait());   // a task of nothing: whether it finished
    EXPECT_EQ(finished.load(), 1);
    // from a task
    auto t = sgcl::async::spawn([]() -> task<int> {
        auto r = co_await sgcl::async::with_timeout(number(8, 2), 200ms);
        co_return r ? *r : -1;
    }());
    EXPECT_EQ(t.wait(), 8);
    sgcl::async::scheduler::stop();
}

TEST(Timeout_Tests, ATaskThatDoesNot) {
    auto t0 = Clock::now();
    auto r = sgcl::async::with_timeout(number(7, 200), 20ms).wait();
    EXPECT_FALSE(r.has_value());
    auto took = Clock::now() - t0;
    EXPECT_GE(took, 20ms);
    EXPECT_LT(took, 150ms);                                // the deadline, not the task
    sgcl::atomic<int> finished = {0};
    EXPECT_FALSE(sgcl::async::with_timeout(nothing(200, finished), 20ms).wait());
    EXPECT_EQ(finished.load(), 0);                         // the loser runs on, unseen
    std::this_thread::sleep_for(300ms);
    EXPECT_EQ(finished.load(), 1);                         // and finishes on its own
    sgcl::async::scheduler::stop();
}

TEST(Timeout_Tests, TheLoserIsStoppedThroughItsSource) {
    sgcl::atomic<int> stopped = {0};
    sgcl::async::stop_source src;
    auto t0 = Clock::now();
    auto r = sgcl::async::with_timeout(obedient_into(src.token(), 500, stopped), 20ms, src).wait();
    EXPECT_FALSE(r.has_value());
    EXPECT_LT(Clock::now() - t0, 400ms);
    EXPECT_TRUE(src.stop_requested());                     // the timeout stopped the source
    for (int i = 0; i < 5000 && stopped.load() == 0; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(stopped.load(), 1);                          // the task saw it and left
    // in time: the source untouched
    sgcl::async::stop_source src2;
    auto s = sgcl::async::with_timeout(obedient(src2.token(), 5), 200ms, src2).wait();
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(*s, "finished");
    EXPECT_FALSE(src2.stop_requested());
    // a child source under the caller's token: the way to write it
    sgcl::async::stop_source parent;
    sgcl::async::stop_source child(parent.token());
    EXPECT_FALSE(sgcl::async::with_timeout(obedient(child.token(), 500), 20ms, child).wait().has_value());
    EXPECT_TRUE(child.stop_requested());
    EXPECT_FALSE(parent.stop_requested());
    sgcl::async::scheduler::stop();
}

TEST(Timeout_Tests, WithTimeoutGivesTimedOut) {
    EXPECT_EQ(sgcl::async::with_timeout(number(3, 5), 200ms).result(), 3);
    EXPECT_EQ(sgcl::async::with_timeout(number(3, 500), 20ms).wait().error(), sgcl::async::timed_out());
    auto late = sgcl::async::with_timeout(text_task(500), 20ms).wait();
    ASSERT_FALSE(late);                                    // an error, not an exception (DESIGN 220)
    EXPECT_EQ(late.error().message(), "timed out");
    // the task's own exception comes through
    EXPECT_THROW(sgcl::async::with_timeout(failing(), 200ms).wait(), std::runtime_error);
    EXPECT_THROW(sgcl::async::with_timeout(failing(), 200ms).wait(), std::runtime_error);
    // with the loser stopped
    sgcl::async::stop_source src;
    EXPECT_EQ(sgcl::async::with_timeout(obedient(src.token(), 500), 20ms, src).wait().error(), sgcl::async::timed_out());
    EXPECT_TRUE(src.stop_requested());
    // a task of nothing
    sgcl::atomic<int> finished = {0};
    sgcl::async::with_timeout(nothing(5, finished), 200ms).wait();
    EXPECT_EQ(finished.load(), 1);
    // the loser stopped through its source and waited for: a task left sleeping past the
    // test would write to `finished`, a word of this stack, from a later test
    sgcl::async::stop_source src2;
    EXPECT_EQ(sgcl::async::with_timeout(obedient_nothing(src2.token(), 500, finished), 20ms, src2).wait().error(), sgcl::async::timed_out());
    EXPECT_TRUE(src2.stop_requested());
    for (int i = 0; i < 5000 && finished.load() < 2; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(finished.load(), 2);
    sgcl::async::scheduler::stop();
}

// The deadline as a point of the module's clock, as Go's WithDeadline
// takes it, beside with_timeout's time; and a source stopped at a point
TEST(Timeout_Tests, ADeadlineAtAPoint) {
    auto t0 = Clock::now();
    EXPECT_EQ(sgcl::async::with_deadline(number(3, 500), sgcl::clock::now() + 20ms).wait().error(), sgcl::async::timed_out());
    EXPECT_GE(Clock::now() - t0, 20ms);
    EXPECT_EQ(sgcl::async::with_deadline(number(4, 5), sgcl::clock::now() + 200ms).result(), 4);
    sgcl::async::stop_source loser;
    EXPECT_EQ(sgcl::async::with_deadline(obedient(loser.token(), 500), sgcl::clock::now() + 20ms, loser).wait().error(), sgcl::async::timed_out());
    EXPECT_TRUE(loser.token().stop_requested());
    EXPECT_EQ(sgcl::async::with_deadline(number(1, 50), sgcl::clock::now() - 1s).wait().error(), sgcl::async::timed_out());   // a point past: at once
    sgcl::async::stop_source at;
    at.stop_at(sgcl::clock::now() + 20ms);
    EXPECT_EQ(sgcl::async::with_deadline(number(3, 500), at.token()).wait().error(), sgcl::async::stopped());
    EXPECT_TRUE(at.token().stop_requested());
    sgcl::async::scheduler::stop();
}

TEST(Timeout_Tests, ATokenAsTheDeadline) {
    sgcl::async::stop_source src;
    src.stop_after(20ms);                                  // the deadline on the source
    auto t0 = Clock::now();
    EXPECT_EQ(sgcl::async::with_deadline(number(3, 500), src.token()).wait().error(), sgcl::async::stopped());
    EXPECT_GE(Clock::now() - t0, 20ms);
    EXPECT_LT(Clock::now() - t0, 400ms);
    // the task given the same token stops itself
    sgcl::async::stop_source src2;
    src2.stop_after(20ms);
    EXPECT_EQ(sgcl::async::with_deadline(obedient(src2.token(), 500), src2.token()).wait().error(), sgcl::async::stopped());
    // in time
    sgcl::async::stop_source src3;
    src3.stop_after(200ms);
    EXPECT_EQ(sgcl::async::with_deadline(number(4, 5), src3.token()).wait(), 4);
    // a stop by hand, from another thread
    sgcl::async::stop_source src4;
    std::thread stopper([&] { std::this_thread::sleep_for(20ms); src4.request_stop(); });
    auto by_hand = sgcl::async::with_deadline(number(3, 500), src4.token()).wait();
    ASSERT_FALSE(by_hand);
    EXPECT_EQ(by_hand.error().message(), "stopped");        // a stop, not a timeout: the library cannot tell a deadline from a hand
    stopper.join();
    // an empty token: no deadline
    EXPECT_EQ(sgcl::async::with_deadline(number(5, 5), sgcl::async::stop_token()).wait(), 5);
    sgcl::async::scheduler::stop();
}

TEST(Timeout_Tests, ATimeoutOfZero) {
    EXPECT_FALSE(sgcl::async::with_timeout(number(1, 50), 0ms).wait().has_value());   // the task needs 50 ms: the deadline is first
    auto t = sgcl::async::spawn(number(2, 0));
    t.wait();                                                             // done already
    auto r = sgcl::async::with_timeout(std::move(t), 0ms).wait();                     // a result that is there is a result
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 2);
    EXPECT_EQ(sgcl::async::with_timeout(number(1, 50), 0ms).wait().error(), sgcl::async::timed_out());
    sgcl::async::scheduler::stop();
}

TEST(Timeout_Tests, ManyAtOnce) {
    // a thousand timeouts at once, half of them met, half not. The margin
    // of the met half is wide on purpose: under the thread sanitizer the
    // whole process stops for 65 to 100 ms now and then (a program with
    // no sgcl in it does too, every two million or so synchronisations),
    // and a stop between the timer's arming and the task's first step
    // passed a 50 ms deadline for a task of 1 ms once in thirty runs.
    // The lost half is stopped at the deadline, so its length costs nothing
    sgcl::atomic<int> met = {0}, missed = {0}, stopped = {0};
    sgcl::async::task_group g;
    for (int i = 0; i < 1000; ++i) {
        g.go([](int i, sgcl::atomic<int>& met, sgcl::atomic<int>& missed, sgcl::atomic<int>& stopped) -> task<> {
            sgcl::async::stop_source src;
            auto r = co_await sgcl::async::with_timeout(obedient_into(src.token(), i % 2 ? 1 : 3000, stopped), 300ms, src);
            if (r) {
                ++met;
            } else {
                ++missed;
            }
        }(i, met, missed, stopped));
    }
    g.wait();
    EXPECT_EQ(met.load(), 500);
    EXPECT_EQ(missed.load(), 500);
    for (int i = 0; i < 5000 && stopped.load() < 500; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(stopped.load(), 500);                        // every loser saw its stop
    sgcl::async::scheduler::stop();
    sgcl::collector::force_collect(true);                  // the thousand frames and their timers reclaimed: once dodged an exit hang under TSan, not seen since the tests stopped leaving tasks behind
}


TEST(Timeout_Tests, AWonRaceCancelsItsTimer) {
    // a race the task wins leaves no timer behind: cancelled, and swept
    // out of the heap once the cancelled are half of it
    auto t = sgcl::async::spawn([]() -> task<> {
        for (int i = 0; i < 5000; ++i) {
            auto r = co_await sgcl::async::with_timeout([](int v) -> task<int> { co_return v; }(i), 1h);
            EXPECT_EQ(*r, i);
        }
    }());
    t.wait();
    EXPECT_LT(sgcl::async::detail::timers_instance().size(), 200u);
    sgcl::async::scheduler::stop();
}
