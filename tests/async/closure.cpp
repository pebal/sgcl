//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

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
    auto t = sgcl::async::spawn([big, n]() -> sgcl::async::task<std::string> {
        co_await sgcl::async::sleep(2ms);   // a real suspension: the statement below is over by now
        co_return big.substr(0, 3) + std::to_string(n);
    });
    // the closure was a temporary of the statement above; the task runs on its copy
    EXPECT_EQ(t.wait(), "vvv7");
    sgcl::async::scheduler::stop();
}

TEST(Closure_Tests, CapturesByReferenceToAnOuterScope) {
    std::atomic<int> sum = 0;
    sgcl::async::event done;
    sgcl::async::go([&sum, &done]() -> sgcl::async::task<> {
        for (int i = 1; i <= 10; ++i) {
            co_await sgcl::async::yield();
            sum += i;
        }
        done.set();
    });
    done.wait();
    EXPECT_EQ(sum.load(), 55);
    sgcl::async::scheduler::stop();
}

TEST(Closure_Tests, ManagedCapturesAreRooted) {
    sgcl::tracked_ptr p = make_tracked<Int>(41);
    auto t = sgcl::async::spawn([p]() -> sgcl::async::task<int> {
        co_await sgcl::async::sleep(2ms);
        sgcl::collector::force_collect(true);   // the capture in the frame keeps the object
        co_return *p + 1;
    });
    p = nullptr;
    EXPECT_EQ(t.wait(), 42);
    sgcl::async::scheduler::stop();
}

TEST(Closure_Tests, OnExecutorStrandAndGroup) {
    sgcl::async::executor ex;
    int x = 5;
    auto a = sgcl::async::spawn([x]() -> sgcl::async::task<int> { co_return x * 2; }, ex);
    sgcl::async::strand st;
    auto b = st.spawn([x]() -> sgcl::async::task<int> {
        co_await sgcl::async::yield();
        co_return x * 3;
    });
    std::atomic<int> c = 0;
    sgcl::async::go([&c, x]() -> sgcl::async::task<> { c = x * 4; co_return; }, st);
    ex.run_until(a);
    EXPECT_EQ(a.result(), 10);
    EXPECT_EQ(b.wait(), 15);
    auto g = sgcl::async::spawn([x, &c]() -> sgcl::async::task<int> {
        sgcl::async::task_group group;
        group.go([x, &c]() -> sgcl::async::task<> {
            co_await sgcl::async::sleep(1ms);
            c += x;
        });
        co_await group;
        co_return c.load();
    });
    EXPECT_EQ(g.wait(), 25);
    sgcl::async::scheduler::stop();
}
