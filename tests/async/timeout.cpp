//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

    task<int> number(int n, int ms) {
        co_await sgcl::sleep(std::chrono::milliseconds(ms));
        co_return n;
    }

    task<std::string> text(int ms) {
        co_await sgcl::sleep(std::chrono::milliseconds(ms));
        co_return "text";
    }

    task<> nothing(int ms, sgcl::atomic<int>& finished) {
        co_await sgcl::sleep(std::chrono::milliseconds(ms));
        ++finished;
    }

    task<int> failing() {
        throw std::runtime_error("failing");
        co_return 0;
    }

    // A task that works until told to stop: how it ended is its result
    task<std::string> obedient(sgcl::stop_token tok, int ms) {
        size_t which = co_await sgcl::async_select(
            tok.on_stop([] {}),
            sgcl::timeout(std::chrono::milliseconds(ms), [] {}));
        co_return which == 0 ? "stopped" : "finished";
    }

    task<std::string> obedient_into(sgcl::stop_token tok, int ms, sgcl::atomic<int>& stopped) {
        auto how = co_await obedient(tok, ms);
        if (how == "stopped") {
            ++stopped;
        }
        co_return how;
    }

    // A task of nothing that works until told to stop: counted when it ends, however it ended
    task<> obedient_nothing(sgcl::stop_token tok, int ms, sgcl::atomic<int>& finished) {
        co_await obedient(tok, ms);
        ++finished;
    }
}

TEST(Timeout_Tests, ATaskThatFinishesInTime) {
    auto r = sgcl::timeout(number(7, 5), 200ms).join();    // join starts the timeout, which starts the task
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 7);
    auto s = sgcl::timeout(sgcl::spawn(text(5)), 200ms).join();   // a task spawned already
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(*s, "text");
    sgcl::atomic<int> finished = {0};
    EXPECT_TRUE(sgcl::timeout(nothing(5, finished), 200ms).join());   // a task of nothing: whether it finished
    EXPECT_EQ(finished.load(), 1);
    // from a task
    auto t = sgcl::spawn([]() -> task<int> {
        auto r = co_await sgcl::timeout(number(8, 2), 200ms);
        co_return r ? *r : -1;
    }());
    EXPECT_EQ(t.join(), 8);
    sgcl::scheduler::stop();
}

TEST(Timeout_Tests, ATaskThatDoesNot) {
    auto t0 = Clock::now();
    auto r = sgcl::timeout(number(7, 200), 20ms).join();
    EXPECT_FALSE(r.has_value());
    auto took = Clock::now() - t0;
    EXPECT_GE(took, 20ms);
    EXPECT_LT(took, 150ms);                                // the deadline, not the task
    sgcl::atomic<int> finished = {0};
    EXPECT_FALSE(sgcl::timeout(nothing(200, finished), 20ms).join());
    EXPECT_EQ(finished.load(), 0);                         // the loser runs on, unseen
    std::this_thread::sleep_for(300ms);
    EXPECT_EQ(finished.load(), 1);                         // and finishes on its own
    sgcl::scheduler::stop();
}

TEST(Timeout_Tests, TheLoserIsStoppedThroughItsSource) {
    sgcl::atomic<int> stopped = {0};
    sgcl::stop_source src;
    auto t0 = Clock::now();
    auto r = sgcl::timeout(obedient_into(src.token(), 500, stopped), 20ms, src).join();
    EXPECT_FALSE(r.has_value());
    EXPECT_LT(Clock::now() - t0, 400ms);
    EXPECT_TRUE(src.stop_requested());                     // the timeout stopped the source
    for (int i = 0; i < 5000 && stopped.load() == 0; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(stopped.load(), 1);                          // the task saw it and left
    // in time: the source untouched
    sgcl::stop_source src2;
    auto s = sgcl::timeout(obedient(src2.token(), 5), 200ms, src2).join();
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(*s, "finished");
    EXPECT_FALSE(src2.stop_requested());
    // a child source under the caller's token: the way to write it
    sgcl::stop_source parent;
    sgcl::stop_source child(parent.token());
    EXPECT_FALSE(sgcl::timeout(obedient(child.token(), 500), 20ms, child).join().has_value());
    EXPECT_TRUE(child.stop_requested());
    EXPECT_FALSE(parent.stop_requested());
    sgcl::scheduler::stop();
}

TEST(Timeout_Tests, WithDeadlineThrowsTimedOut) {
    EXPECT_EQ(sgcl::with_deadline(number(3, 5), 200ms).join(), 3);
    EXPECT_THROW(sgcl::with_deadline(number(3, 500), 20ms).join(), sgcl::timed_out);
    try {
        sgcl::with_deadline(text(500), 20ms).join();
        FAIL() << "no timed_out";
    } catch (const std::runtime_error& e) {                // a runtime_error
        EXPECT_STREQ(e.what(), "timed out");
    }
    // the task's own exception comes through
    EXPECT_THROW(sgcl::with_deadline(failing(), 200ms).join(), std::runtime_error);
    EXPECT_THROW(sgcl::timeout(failing(), 200ms).join(), std::runtime_error);
    // with the loser stopped
    sgcl::stop_source src;
    EXPECT_THROW(sgcl::with_deadline(obedient(src.token(), 500), 20ms, src).join(), sgcl::timed_out);
    EXPECT_TRUE(src.stop_requested());
    // a task of nothing
    sgcl::atomic<int> finished = {0};
    sgcl::with_deadline(nothing(5, finished), 200ms).join();
    EXPECT_EQ(finished.load(), 1);
    // the loser stopped through its source and waited for: a task left sleeping past the
    // test would write to `finished`, a word of this stack, from a later test
    sgcl::stop_source src2;
    EXPECT_THROW(sgcl::with_deadline(obedient_nothing(src2.token(), 500, finished), 20ms, src2).join(), sgcl::timed_out);
    EXPECT_TRUE(src2.stop_requested());
    for (int i = 0; i < 5000 && finished.load() < 2; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(finished.load(), 2);
    sgcl::scheduler::stop();
}

TEST(Timeout_Tests, ATokenAsTheDeadline) {
    sgcl::stop_source src;
    src.stop_after(20ms);                                  // the deadline on the source
    auto t0 = Clock::now();
    EXPECT_THROW(sgcl::with_deadline(number(3, 500), src.token()).join(), sgcl::timed_out);
    EXPECT_GE(Clock::now() - t0, 20ms);
    EXPECT_LT(Clock::now() - t0, 400ms);
    // the task given the same token stops itself
    sgcl::stop_source src2;
    src2.stop_after(20ms);
    EXPECT_THROW(sgcl::with_deadline(obedient(src2.token(), 500), src2.token()).join(), sgcl::timed_out);
    // in time
    sgcl::stop_source src3;
    src3.stop_after(200ms);
    EXPECT_EQ(sgcl::with_deadline(number(4, 5), src3.token()).join(), 4);
    // a stop by hand, from another thread
    sgcl::stop_source src4;
    std::thread stopper([&] { std::this_thread::sleep_for(20ms); src4.request_stop(); });
    EXPECT_THROW(sgcl::with_deadline(number(3, 500), src4.token()).join(), sgcl::timed_out);
    stopper.join();
    // an empty token: no deadline
    EXPECT_EQ(sgcl::with_deadline(number(5, 5), sgcl::stop_token()).join(), 5);
    sgcl::scheduler::stop();
}

TEST(Timeout_Tests, ATimeoutOfZero) {
    EXPECT_FALSE(sgcl::timeout(number(1, 50), 0ms).join().has_value());   // the task needs 50 ms: the deadline is first
    auto t = sgcl::spawn(number(2, 0));
    t.join();                                                             // done already
    auto r = sgcl::timeout(std::move(t), 0ms).join();                     // a result that is there is a result
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 2);
    EXPECT_THROW(sgcl::with_deadline(number(1, 50), 0ms).join(), sgcl::timed_out);
    sgcl::scheduler::stop();
}

TEST(Timeout_Tests, ManyAtOnce) {
    // a thousand timeouts at once, half of them met, half not
    sgcl::atomic<int> met = {0}, missed = {0}, stopped = {0};
    sgcl::task_group g;
    for (int i = 0; i < 1000; ++i) {
        g.spawn([](int i, sgcl::atomic<int>& met, sgcl::atomic<int>& missed, sgcl::atomic<int>& stopped) -> task<> {
            sgcl::stop_source src;
            auto r = co_await sgcl::timeout(obedient_into(src.token(), i % 2 ? 1 : 300, stopped), 50ms, src);
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
    sgcl::scheduler::stop();
    sgcl::collector::force_collect(true);                  // the thousand frames and their timers reclaimed: once dodged an exit hang under TSan, not seen since the tests stopped leaving tasks behind
}


TEST(Timeout_Tests, AWonRaceCancelsItsTimer) {
    // a race the task wins leaves no timer behind: cancelled, and swept
    // out of the heap once the cancelled are half of it
    auto t = sgcl::spawn([]() -> task<> {
        for (int i = 0; i < 5000; ++i) {
            auto r = co_await sgcl::timeout([](int v) -> task<int> { co_return v; }(i), 1h);
            EXPECT_EQ(*r, i);
        }
    }());
    t.join();
    EXPECT_LT(sgcl::detail::timers_instance().size(), 200u);
    sgcl::scheduler::stop();
}
