//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <string>

// spawn(f) and go(f) with a lambda that captures: the closure is copied into
// a frame that lives as long as the task, so the captures survive the
// statement that made the temporary closure (which the task of a called
// lambda, spawn(f()), does not: CP.51).

namespace {
    using namespace std::chrono_literals;
}

TEST(Closure_Tests, SpawnCopiesTheClosure) {
    std::string big(1000, 'v');   // past any small-buffer optimization: a dangling closure would read freed memory
    int n = 7;
    auto t = sgcl::spawn([big, n]() -> sgcl::task<std::string> {
        co_await sgcl::sleep(2ms);   // a real suspension: the statement below is over by now
        co_return big.substr(0, 3) + std::to_string(n);
    });
    // the closure was a temporary of the statement above; the task runs on its copy
    EXPECT_EQ(t.join(), "vvv7");
    sgcl::scheduler::stop();
}

TEST(Closure_Tests, CapturesByReferenceToAnOuterScope) {
    std::atomic<int> sum = 0;
    sgcl::event done;
    sgcl::go([&sum, &done]() -> sgcl::task<> {
        for (int i = 1; i <= 10; ++i) {
            co_await sgcl::yield();
            sum += i;
        }
        done.set();
    });
    done.wait();
    EXPECT_EQ(sum.load(), 55);
    sgcl::scheduler::stop();
}

TEST(Closure_Tests, ManagedCapturesAreRooted) {
    sgcl::tracked_ptr p = make_tracked<Int>(41);
    auto t = sgcl::spawn([p]() -> sgcl::task<int> {
        co_await sgcl::sleep(2ms);
        sgcl::collector::force_collect(true);   // the capture in the frame keeps the object
        co_return *p + 1;
    });
    p = nullptr;
    EXPECT_EQ(t.join(), 42);
    sgcl::scheduler::stop();
}

TEST(Closure_Tests, OnExecutorStrandAndGroup) {
    sgcl::executor ex;
    int x = 5;
    auto a = sgcl::spawn([x]() -> sgcl::task<int> { co_return x * 2; }, ex);
    sgcl::strand st;
    auto b = st.spawn([x]() -> sgcl::task<int> {
        co_await sgcl::yield();
        co_return x * 3;
    });
    std::atomic<int> c = 0;
    sgcl::go([&c, x]() -> sgcl::task<> { c = x * 4; co_return; }, st);
    ex.run_until(a);
    EXPECT_EQ(a.result(), 10);
    EXPECT_EQ(b.join(), 15);
    auto g = sgcl::spawn([x, &c]() -> sgcl::task<int> {
        sgcl::task_group group;
        group.spawn([x, &c]() -> sgcl::task<> {
            co_await sgcl::sleep(1ms);
            c += x;
        });
        co_await group.async_wait();
        co_return c.load();
    });
    EXPECT_EQ(g.join(), 25);
    sgcl::scheduler::stop();
}
