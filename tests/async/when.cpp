//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    sgcl::task<int> number(int n, int ms) {
        co_await sgcl::sleep(std::chrono::milliseconds(ms));
        co_return n;
    }

    sgcl::task<std::string> text() {
        co_return "text";
    }

    sgcl::task<> nothing() {
        co_return;
    }

    sgcl::task<int> failing() {
        throw std::runtime_error("failing");
        co_return 0;
    }

    sgcl::task<int> failing_after(int ms) {
        co_await sgcl::sleep(std::chrono::milliseconds(ms));
        throw std::runtime_error("failing");
    }
}

TEST(When_Test, WhenAllGivesATupleOfTheResults) {
    auto [n, s] = sgcl::when_all(sgcl::spawn(number(1, 5)), sgcl::spawn(text())).join();   // join starts the when_all
    EXPECT_EQ(n, 1);
    EXPECT_EQ(s, "text");
    sgcl::when_all(sgcl::spawn(nothing()), sgcl::spawn(nothing())).join();
    // tasks nobody spawned: started by the wait for them
    auto [a, b] = sgcl::when_all(number(2, 1), number(3, 1)).join();
    EXPECT_EQ(a + b, 5);
    sgcl::scheduler::stop();
}

TEST(When_Test, WhenAllOverARange) {
    sgcl::vector<sgcl::task<int>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(sgcl::spawn(number(i, 8 - i)));   // the later ones finish first
    }
    auto results = sgcl::when_all(std::move(tasks)).join();
    ASSERT_EQ(results.size(), 8u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(results[i], i);                          // in the order given, not of finishing
    }
    sgcl::vector<sgcl::task<>> nothings;
    nothings.push_back(nothing());
    nothings.push_back(nothing());
    sgcl::when_all(std::move(nothings)).join();
    sgcl::scheduler::stop();
}

TEST(When_Test, WhenAllRethrows) {
    EXPECT_THROW(sgcl::when_all(sgcl::spawn(failing()), sgcl::spawn(number(1, 1))).join(), std::runtime_error);
    sgcl::scheduler::stop();
}

TEST(When_Test, WhenAnyIsTheFirstToFinish) {
    EXPECT_EQ(sgcl::when_any(sgcl::spawn(number(1, 60)), sgcl::spawn(number(2, 5)), sgcl::spawn(number(3, 80))).join(), 1u);
    sgcl::vector<sgcl::task<int>> tasks;
    tasks.push_back(number(1, 40));
    tasks.push_back(number(2, 3));
    EXPECT_EQ(sgcl::when_any(std::move(tasks)).join(), 1u);
    // from a task
    auto t = sgcl::spawn([]() -> sgcl::task<size_t> {
        co_return co_await sgcl::when_any(number(1, 30), number(2, 2));
    }());
    EXPECT_EQ(t.join(), 1u);
    std::this_thread::sleep_for(100ms);                     // the losers finish and are collected
    sgcl::scheduler::stop();
    sgcl::collector::force_collect(true);
}

TEST(When_Test, ATaskStartsOnItsFirstWait) {
    auto t = number(4, 1);
    EXPECT_FALSE(t.done());
    EXPECT_EQ(t.join(), 4);                                 // never spawned: join started it
    auto u = sgcl::spawn([]() -> sgcl::task<int> {
        co_return co_await number(5, 1);                    // co_await started it
    }());
    EXPECT_EQ(u.join(), 5);
    sgcl::scheduler::stop();
}

TEST(When_Test, WhenAnyRethrowsTheWinners) {
    EXPECT_THROW(sgcl::when_any(sgcl::spawn(failing()), sgcl::spawn(number(1, 50))).join(), std::runtime_error);   // the first to finish threw: its exception
    EXPECT_EQ(sgcl::when_any(sgcl::spawn(number(2, 1)), sgcl::spawn(failing_after(30))).join(), 0u);              // a loser's is dropped with it
    sgcl::vector<sgcl::task<int>> none;
    EXPECT_EQ(sgcl::when_any(std::move(none)).join(), SIZE_MAX);                                                   // nothing can finish first
    std::this_thread::sleep_for(100ms);
    sgcl::scheduler::stop();
}
