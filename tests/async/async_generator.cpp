//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    sgcl::async_generator<int> tens(sgcl::channel<int>& in) {
        while (auto v = co_await in.async_receive()) {   // waits between yields
            co_yield *v * 10;
        }
    }

    sgcl::async_generator<int> slow(int n) {
        for (int i = 0; i < n; ++i) {
            co_await sgcl::sleep(1ms);
            co_yield i;
        }
    }

    sgcl::async_generator<int> failing() {
        co_yield 1;
        throw std::runtime_error("failing");
    }

    sgcl::async_generator<sgcl::tracked_ptr<Int>> objects(int n) {
        for (int i = 0; i < n; ++i) {
            co_await sgcl::yield();
            co_yield sgcl::make_tracked<Int>(i);
        }
    }
}

TEST(AsyncGenerator_Test, YieldsBetweenWaits) {
    sgcl::channel<int> in(2);
    auto t = sgcl::spawn([](sgcl::channel<int>& in) -> sgcl::task<int> {
        auto g = tens(in);
        int sum = 0;
        while (auto v = co_await g.next()) {
            sum += *v;
        }
        co_return sum;
    }(in));
    for (int i = 1; i <= 4; ++i) {
        std::this_thread::sleep_for(2ms);
        in.send(i);
    }
    in.close();
    EXPECT_EQ(t.join(), 100);
    sgcl::scheduler::stop();
}

TEST(AsyncGenerator_Test, TheEndAndAnException) {
    auto t = sgcl::spawn([]() -> sgcl::task<int> {
        auto g = slow(3);
        int count = 0;
        while (auto v = co_await g.next()) {
            ++count;
        }
        if (!g.done() || co_await g.next()) {   // past the end: nothing, again
            co_return -1;
        }
        auto f = failing();
        int first = *co_await f.next();
        try {
            co_await f.next();
        } catch (const std::runtime_error&) {
            co_return count * 100 + first;
        }
        co_return -2;
    }());
    EXPECT_EQ(t.join(), 301);
    sgcl::scheduler::stop();
}

TEST(AsyncGenerator_Test, TheValuesAreHeldWhileYielded) {
    auto t = sgcl::spawn([]() -> sgcl::task<int> {
        auto g = objects(50);
        int sum = 0;
        while (auto v = co_await g.next()) {
            sgcl::collector::force_collect(true);   // the yielded object is held by the promise until taken
            sum += (int)**v;
        }
        co_return sum;
    }());
    EXPECT_EQ(t.join(), 49 * 50 / 2);
    sgcl::scheduler::stop();
}

TEST(AsyncGenerator_Test, SchedulerStatistics) {
    auto before = sgcl::scheduler::get_statistics();
    EXPECT_EQ(before.workers, 0u);                           // not started
    sgcl::channel<void> gate;
    std::vector<sgcl::task<>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(sgcl::spawn([](sgcl::channel<void>& gate) -> sgcl::task<> {
            co_await gate.async_receive();
        }(gate)));
    }
    auto st = sgcl::scheduler::get_statistics();
    EXPECT_EQ(st.workers, sgcl::scheduler::workers());
    EXPECT_LE(st.spinning + st.sleeping, st.workers);
    gate.close();
    for (auto& t : tasks) {
        t.join();
    }
    sgcl::scheduler::stop();
    EXPECT_EQ(sgcl::scheduler::get_statistics().workers, 0u);
}

// The generator runs as part of its consumer: on the consumer's executor
// and under its task-locals, also after a wait of its own (the frame
// resumed by the scheduler is the generator's, whose header must then
// say what the consumer's does), and the consumer resumed by the yield
// after such a wait is still on its own thread and still sees its locals
namespace {
    sgcl::task_local<int> request_id;

    sgcl::task<int> consumes(sgcl::channel<int>& in, std::vector<std::thread::id>& ids, std::vector<bool>& seen, std::vector<bool>& seen_inside) {
        co_await request_id.set(7);
        auto g = [](sgcl::channel<int>& in, std::vector<bool>& seen_inside) -> sgcl::async_generator<int> {
            while (auto v = co_await in.async_receive()) {   // a wait of the generator's own
                seen_inside.push_back(request_id.get() == 7);
                co_yield *v * 10;
            }
        }(in, seen_inside);
        int sum = 0;
        while (auto v = co_await g.next()) {
            ids.push_back(std::this_thread::get_id());
            seen.push_back(request_id.get() == 7);
            sum += *v;
        }
        co_return sum;
    }
}

TEST(AsyncGenerator_Test, RunsAsPartOfItsConsumer) {
    sgcl::executor ex;
    sgcl::channel<int> in;
    std::vector<std::thread::id> ids;
    std::vector<bool> seen, seen_inside;
    std::thread other([&] {
        for (int i = 1; i <= 4; ++i) {
            std::this_thread::sleep_for(2ms);                // the generator waits every time
            in.send(i);
        }
        in.close();
    });
    EXPECT_EQ(ex.run(consumes(in, ids, seen, seen_inside)), 100);
    other.join();
    ASSERT_EQ(ids.size(), 4u);
    for (auto id : ids) {
        EXPECT_EQ(id, std::this_thread::get_id());           // the consumer back on its executor after the generator's wait
    }
    ASSERT_EQ(seen.size(), 4u);
    ASSERT_EQ(seen_inside.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(seen[i]);                                // the consumer's task-local still there
        EXPECT_TRUE(seen_inside[i]);                         // and the generator under it
    }
    sgcl::scheduler::stop();
}
