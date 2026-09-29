//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// sgcl::async::run: the entry of a program. The task's value and its
// exception; the stop_token stopped by SIGINT and SIGTERM, the second
// signal ending the process as it would without run (Go's
// signal.NotifyContext), the disposition and the other registrations for
// the signals left as they were
#include "tests/types.h"

#include <csignal>
#include <stdexcept>

namespace {
    void (*disposition(int n))(int) {
        struct sigaction sa = {};
        ::sigaction(n, nullptr, &sa);
        return sa.sa_handler;
    }

    sgcl::async::task<int> answer() {
        co_await sgcl::async::yield();
        co_return 42;
    }

    sgcl::async::task<> fails() {
        co_await sgcl::async::yield();
        throw std::runtime_error("the task's error");
    }

    // The program of the signal tests: raises the signal Ctrl-C would, and
    // ends once its token says stop
    sgcl::async::task<int> until_stopped(sgcl::async::stop_token stop, int sig) {
        std::raise(sig);
        co_await stop.stopped();
        co_return 7;
    }
}

TEST(Run_Test, TheTasksValue) {
    EXPECT_EQ(sgcl::async::run(answer()), 42);
    int seen = 0;
    sgcl::async::run([](int& seen) -> sgcl::async::task<> {
        seen = co_await answer();
    }(seen));
    EXPECT_EQ(seen, 42);
}

TEST(Run_Test, TheTasksException) {
    EXPECT_THROW(sgcl::async::run(fails()), std::runtime_error);
}

TEST(Run_Test, AFunctionWithoutASignal) {
    int n = sgcl::async::run([](sgcl::async::stop_token stop) -> sgcl::async::task<int> {
        EXPECT_FALSE(stop.stop_requested());
        co_return co_await answer();
    });
    EXPECT_EQ(n, 42);
    EXPECT_EQ(disposition(SIGINT), SIG_DFL);   // the registration dropped at the end
    EXPECT_EQ(disposition(SIGTERM), SIG_DFL);
}

TEST(Run_Test, SigintStopsTheToken) {
    EXPECT_EQ(sgcl::async::run([](sgcl::async::stop_token stop) { return until_stopped(stop, SIGINT); }), 7);
    EXPECT_EQ(disposition(SIGINT), SIG_DFL);
}

TEST(Run_Test, SigtermStopsTheToken) {
    EXPECT_EQ(sgcl::async::run([](sgcl::async::stop_token stop) { return until_stopped(stop, SIGTERM); }), 7);
    EXPECT_EQ(disposition(SIGTERM), SIG_DFL);
}

TEST(Run_Test, TheExceptionOfAFunction) {
    EXPECT_THROW(sgcl::async::run([](sgcl::async::stop_token) { return fails(); }), std::runtime_error);
    EXPECT_EQ(disposition(SIGINT), SIG_DFL);   // dropped on the way out of an exception too
}

// The program's own registration for SIGINT is not run's: it keeps
// getting the signal, before, during and after run
TEST(Run_Test, AnotherRegistrationStays) {
    auto mine = sgcl::async::signals({SIGINT});
    EXPECT_EQ(sgcl::async::run([](sgcl::async::stop_token stop) { return until_stopped(stop, SIGINT); }), 7);
    auto during = mine.receive().wait();   // the signal of the run, to both channels
    ASSERT_TRUE(during);
    EXPECT_EQ(*during, SIGINT);
    EXPECT_NE(disposition(SIGINT), SIG_DFL);   // still the module's handler: mine is registered
    std::raise(SIGINT);
    auto after = mine.receive().wait();
    ASSERT_TRUE(after);
    EXPECT_EQ(*after, SIGINT);
    sgcl::async::reset_signals({SIGINT});
    EXPECT_EQ(disposition(SIGINT), SIG_DFL);
}

// The second Ctrl-C: the stop was requested and the program did not end,
// so the signal does what it did before run, SIGINT's default action
TEST(Run_Test, TheSecondSigintEndsTheProcess) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(
        {
            sgcl::async::run([](sgcl::async::stop_token stop) -> sgcl::async::task<> {
                std::raise(SIGINT);
                co_await stop.stopped();
                std::raise(SIGINT);   // the program is still winding down
                for (;;) {
                    co_await sgcl::async::yield();
                }
            });
        },
        testing::KilledBySignal(SIGINT), "");
}
