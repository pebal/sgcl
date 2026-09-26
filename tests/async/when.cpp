//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    sgcl::async::task<int> number(int n, int ms) {
        co_await sgcl::async::sleep(std::chrono::milliseconds(ms));
        co_return n;
    }

    sgcl::async::task<std::string> text_task() {
        co_return "text";
    }

    sgcl::async::task<> nothing() {
        co_return;
    }

    sgcl::async::task<int> failing() {
        throw std::runtime_error("failing");
        co_return 0;
    }

    sgcl::async::task<int> failing_after(int ms) {
        co_await sgcl::async::sleep(std::chrono::milliseconds(ms));
        throw std::runtime_error("failing");
    }
}

TEST(When_Test, WhenAllGivesATupleOfTheResults) {
    auto [n, s] = sgcl::async::when_all(sgcl::async::spawn(number(1, 5)), sgcl::async::spawn(text_task())).wait();   // join starts the when_all
    EXPECT_EQ(n, 1);
    EXPECT_EQ(s, "text");
    sgcl::async::when_all(sgcl::async::spawn(nothing()), sgcl::async::spawn(nothing())).wait();
    // tasks nobody spawned: started by the wait for them
    auto [a, b] = sgcl::async::when_all(number(2, 1), number(3, 1)).wait();
    EXPECT_EQ(a + b, 5);
    sgcl::async::scheduler::stop();
}

TEST(When_Test, WhenAllOverARange) {
    sgcl::vector<sgcl::async::task<int>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(sgcl::async::spawn(number(i, 8 - i)));   // the later ones finish first
    }
    auto results = sgcl::async::when_all(std::move(tasks)).wait();
    ASSERT_EQ(results.size(), 8u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(results[i], i);                          // in the order given, not of finishing
    }
    sgcl::vector<sgcl::async::task<>> nothings;
    nothings.push_back(nothing());
    nothings.push_back(nothing());
    sgcl::async::when_all(std::move(nothings)).wait();
    sgcl::async::scheduler::stop();
}

TEST(When_Test, WhenAllRethrows) {
    EXPECT_THROW(sgcl::async::when_all(sgcl::async::spawn(failing()), sgcl::async::spawn(number(1, 1))).wait(), std::runtime_error);
    sgcl::async::scheduler::stop();
}

TEST(When_Test, WhenAnyIsTheFirstToFinish) {
    EXPECT_EQ(sgcl::async::when_any(sgcl::async::spawn(number(1, 60)), sgcl::async::spawn(number(2, 5)), sgcl::async::spawn(number(3, 80))).wait(), 1u);
    sgcl::vector<sgcl::async::task<int>> tasks;
    tasks.push_back(number(1, 40));
    tasks.push_back(number(2, 3));
    EXPECT_EQ(sgcl::async::when_any(std::move(tasks)).wait(), 1u);
    // from a task
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<size_t> {
        co_return co_await sgcl::async::when_any(number(1, 30), number(2, 2));
    }());
    EXPECT_EQ(t.wait(), 1u);
    std::this_thread::sleep_for(100ms);                     // the losers finish and are collected
    sgcl::async::scheduler::stop();
    sgcl::collector::force_collect(true);
}

TEST(When_Test, ATaskStartsOnItsFirstWait) {
    auto t = number(4, 1);
    EXPECT_FALSE(t.done());
    EXPECT_EQ(t.wait(), 4);                                 // never spawned: join started it
    auto u = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_return co_await number(5, 1);                    // co_await started it
    }());
    EXPECT_EQ(u.wait(), 5);
    sgcl::async::scheduler::stop();
}

TEST(When_Test, WhenAnyRethrowsTheWinners) {
    EXPECT_THROW(sgcl::async::when_any(sgcl::async::spawn(failing()), sgcl::async::spawn(number(1, 50))).wait(), std::runtime_error);   // the first to finish threw: its exception
    EXPECT_EQ(sgcl::async::when_any(sgcl::async::spawn(number(2, 1)), sgcl::async::spawn(failing_after(30))).wait(), 0u);              // a loser's is dropped with it
    sgcl::vector<sgcl::async::task<int>> none;
    EXPECT_EQ(sgcl::async::when_any(std::move(none)).wait(), SIZE_MAX);                                                   // nothing can finish first
    std::this_thread::sleep_for(100ms);
    sgcl::async::scheduler::stop();
}
