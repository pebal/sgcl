//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// on_unhandled: the exception of a task let go of (DESIGN 302) that nobody
// reads goes to the handler, once. The three ways a task is let go of — go(),
// detach(), the object dropped after the start — with a handler of the
// test's; the exceptions somebody took (wait, co_await) and those of a
// race's loser (when_any, with_timeout), which are never the handler's; and
// the default, which prints one line and ends the program (death tests).
#include "tests/types.h"
#include "sgcl/async/async.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

using namespace sgcl::async;
using namespace std::chrono_literals;

namespace unhandled_test {
    struct custom_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };
}

namespace {
    std::atomic<int> calls = 0;
    std::mutex what_lock;
    std::string last_what;

    void record(std::exception_ptr e) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& x) {
            std::lock_guard l(what_lock);
            last_what = x.what();
        } catch (...) {
            std::lock_guard l(what_lock);
            last_what = "?";
        }
        ++calls;
    }

    std::string what() {
        std::lock_guard l(what_lock);
        return last_what;
    }

    // Until the handler was called n times, or two seconds passed
    bool wait_calls(int n) {
        for (int i = 0; i < 2000 && calls < n; ++i) {
            std::this_thread::sleep_for(1ms);
        }
        return calls >= n;
    }

    // Past every end still to come, and every race object collected
    void settle() {
        std::this_thread::sleep_for(100ms);
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
        std::this_thread::sleep_for(20ms);
    }

    task<> thrower(const char* message) {
        throw std::runtime_error(message);
        co_return;
    }

    task<> late_thrower(std::chrono::milliseconds d, const char* message) {
        co_await sgcl::async::sleep(d);
        throw std::runtime_error(message);
    }

    task<int> late_number(std::chrono::milliseconds d, int n) {
        co_await sgcl::async::sleep(d);
        co_return n;
    }

    task<int> late_int_thrower(std::chrono::milliseconds d) {
        co_await sgcl::async::sleep(d);
        throw std::runtime_error("loser");
    }

    struct Unhandled_Tests : ::testing::Test {
        void SetUp() override {
            calls = 0;
            last_what.clear();
            _previous = on_unhandled(&record);
        }

        void TearDown() override {
            settle();
            on_unhandled(_previous);
        }

        void (*_previous)(std::exception_ptr) = nullptr;
    };
}

TEST_F(Unhandled_Tests, ReplacedHandlerIsReturned) {
    auto mine = on_unhandled(nullptr);   // the default back
    EXPECT_EQ(mine, &record);
    EXPECT_EQ(on_unhandled(&record), &sgcl::async::detail::unhandled_default);
}

TEST_F(Unhandled_Tests, Go) {
    go(thrower("go"));
    ASSERT_TRUE(wait_calls(1));
    EXPECT_EQ(what(), "go");
    settle();
    EXPECT_EQ(calls, 1);
}

TEST_F(Unhandled_Tests, GoOfAFunction) {
    go([]() -> task<> {
        co_await sgcl::async::sleep(5ms);
        throw std::runtime_error("go f");
    });
    ASSERT_TRUE(wait_calls(1));
    EXPECT_EQ(what(), "go f");
}

TEST_F(Unhandled_Tests, DetachOfAFinishedTask) {
    auto t = spawn(thrower("finished"));
    for (int i = 0; i < 2000 && !t.done(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(t.done());
    EXPECT_EQ(calls, 0);   // held: its exception may still be read
    t.detach();
    EXPECT_EQ(calls, 1);   // on this thread, in detach
    EXPECT_EQ(what(), "finished");
}

TEST_F(Unhandled_Tests, DetachOfARunningTask) {
    auto t = spawn(late_thrower(20ms, "running"));
    t.detach();
    EXPECT_EQ(calls, 0);
    ASSERT_TRUE(wait_calls(1));   // at its end, on the worker
    EXPECT_EQ(what(), "running");
}

TEST_F(Unhandled_Tests, DroppedWhileRunning) {
    {
        auto t = spawn(late_thrower(20ms, "dropped"));
    }
    EXPECT_EQ(calls, 0);
    ASSERT_TRUE(wait_calls(1));
    EXPECT_EQ(what(), "dropped");
}

TEST_F(Unhandled_Tests, DroppedAfterItsEnd) {
    {
        auto t = spawn(thrower("ended"));
        for (int i = 0; i < 2000 && !t.done(); ++i) {
            std::this_thread::sleep_for(1ms);
        }
        ASSERT_TRUE(t.done());
        EXPECT_EQ(calls, 0);
    }
    EXPECT_EQ(calls, 1);   // on this thread, in the destructor
    EXPECT_EQ(what(), "ended");
}

TEST_F(Unhandled_Tests, AssignedOverAfterItsEnd) {
    auto t = spawn(thrower("assigned over"));
    for (int i = 0; i < 2000 && !t.done(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    t = task<>();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(what(), "assigned over");
}

TEST_F(Unhandled_Tests, NeverStartedIsNotReported) {
    {
        auto t = thrower("never");
    }
    settle();
    EXPECT_EQ(calls, 0);
}

TEST_F(Unhandled_Tests, TakenByWaitIsNotReported) {
    {
        auto t = spawn(thrower("taken"));
        EXPECT_THROW(t.wait(), std::runtime_error);
    }
    settle();
    EXPECT_EQ(calls, 0);
}

TEST_F(Unhandled_Tests, TakenByCoAwaitIsNotReported) {
    auto outer = []() -> task<int> {
        try {
            co_await late_thrower(5ms, "awaited");
        } catch (const std::runtime_error&) {
            co_return 1;
        }
        co_return 0;
    };
    EXPECT_EQ(spawn(outer()).wait(), 1);
    settle();
    EXPECT_EQ(calls, 0);
}

TEST_F(Unhandled_Tests, TimeoutLoserIsNotReported) {
    auto r = with_timeout(late_int_thrower(40ms), 5ms).wait();
    EXPECT_FALSE(r.has_value());
    settle();   // the loser threw, the race collected
    EXPECT_EQ(calls, 0);
}

TEST_F(Unhandled_Tests, WhenAnyLoserIsNotReported) {
    EXPECT_EQ(when_any(late_number(5ms, 1), late_int_thrower(40ms)).wait(), 0u);
    settle();
    EXPECT_EQ(calls, 0);
}

// The default: one line on stderr — the type, what(), where the task ended —
// then std::terminate. Each statement runs in a child of its own
// (threadsafe: the child execs the test binary again, so the scheduler's
// workers are its own)
using Unhandled_DeathTest = Unhandled_Tests;

TEST_F(Unhandled_DeathTest, DefaultOnAWorker) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH({
        on_unhandled(nullptr);
        go([]() -> task<> {
            co_await sgcl::async::sleep(1ms);
            throw unhandled_test::custom_error("connection reset");
        });
        std::this_thread::sleep_for(5s);
    }, "sgcl::async: unhandled exception in a detached task on worker [0-9]+: unhandled_test::custom_error: connection reset\n");
}

TEST_F(Unhandled_DeathTest, DefaultNamesTheStandardType) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH({
        on_unhandled(nullptr);
        go(late_thrower(1ms, "boom"));
        std::this_thread::sleep_for(5s);
    }, "sgcl::async: unhandled exception in a detached task on worker [0-9]+: std::runtime_error: boom\n");
}

TEST_F(Unhandled_DeathTest, DefaultOnAThread) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH({
        on_unhandled(nullptr);
        auto t = spawn(thrower("detached here"));
        while (!t.done()) {
            std::this_thread::sleep_for(1ms);
        }
        t.detach();   // ended already: reported on this thread
    }, "sgcl::async: unhandled exception in a detached task on thread [0-9]+: std::runtime_error: detached here\n");
}

TEST_F(Unhandled_DeathTest, DefaultOfANonStandardException) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH({
        on_unhandled(nullptr);
        go([]() -> task<> {
            co_await sgcl::async::sleep(1ms);
            throw 42;
        });
        std::this_thread::sleep_for(5s);
    }, "sgcl::async: unhandled exception in a detached task on worker [0-9]+: unknown exception\n");
}
