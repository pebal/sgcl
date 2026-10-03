//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    sgcl::async::generator<int> tens(sgcl::async::channel<int>& in) {
        while (auto v = co_await in.receive()) {   // waits between yields
            co_yield *v * 10;
        }
    }

    sgcl::async::generator<int> slow(int n) {
        for (int i = 0; i < n; ++i) {
            co_await sgcl::async::sleep(1ms);
            co_yield i;
        }
    }

    sgcl::async::generator<int> failing() {
        co_yield 1;
        throw std::runtime_error("failing");
    }

    sgcl::async::generator<sgcl::tracked_ptr<Int>> objects(int n) {
        for (int i = 0; i < n; ++i) {
            co_await sgcl::async::yield();
            co_yield sgcl::make_tracked<Int>(i);
        }
    }

    struct Owned {
        explicit Owned(int v) : value(v) { ++alive; }
        ~Owned() { --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    // Yields values that can only be moved, waiting before each
    sgcl::async::generator<std::unique_ptr<Owned>> owned(int n) {
        for (int i = 0; i < n; ++i) {
            co_await sgcl::async::yield();
            co_yield std::make_unique<Owned>(i);
        }
    }
}

TEST(AsyncGenerator_Test, YieldsBetweenWaits) {
    sgcl::async::channel<int> in(2);
    auto t = sgcl::async::spawn([](sgcl::async::channel<int>& in) -> sgcl::async::task<int> {
        auto g = tens(in);
        int sum = 0;
        while (auto v = co_await g.next()) {
            sum += *v;
        }
        co_return sum;
    }(in));
    for (int i = 1; i <= 4; ++i) {
        std::this_thread::sleep_for(2ms);
        in.send(i).wait();
    }
    in.close();
    EXPECT_EQ(t.wait(), 100);
    sgcl::async::scheduler::stop();
}

TEST(AsyncGenerator_Test, TheEndAndAnException) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
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
    EXPECT_EQ(t.wait(), 301);
    sgcl::async::scheduler::stop();
}

TEST(AsyncGenerator_Test, TheValuesAreHeldWhileYielded) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        auto g = objects(50);
        int sum = 0;
        while (auto v = co_await g.next()) {
            sgcl::collector::force_collect(true);   // the yielded object is held by the promise until taken
            sum += (int)**v;
        }
        co_return sum;
    }());
    EXPECT_EQ(t.wait(), 49 * 50 / 2);
    sgcl::async::scheduler::stop();
}

TEST(AsyncGenerator_Test, AYieldedUniquePtrIsMovedToTheConsumer) {
    std::vector<std::unique_ptr<Owned>> taken;
    int alive_after_drop = -1;
    auto t = sgcl::async::spawn([](std::vector<std::unique_ptr<Owned>>& taken, int& alive_after_drop) -> sgcl::async::task<> {
        {
            auto g = owned(5);
            for (int i = 0; i < 2; ++i) {
                auto v = co_await g.next();
                taken.push_back(std::move(*v));
            }
        }                                                    // dropped midway: 2, 3, 4 never made
        alive_after_drop = Owned::alive.load();
        auto g = owned(3);
        while (auto v = co_await g.next()) {
            taken.push_back(std::move(*v));
        }
    }(taken, alive_after_drop));
    t.wait();
    EXPECT_EQ(alive_after_drop, 2);                          // nothing left behind in the dropped frame
    ASSERT_EQ(taken.size(), 5u);
    const int expected[] = {0, 1, 0, 1, 2};
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(taken[i]->value, expected[i]);
    }
    EXPECT_EQ(Owned::alive.load(), 5);                       // the moved values outlive their generators
    taken.clear();
    EXPECT_EQ(Owned::alive.load(), 0);
    sgcl::async::scheduler::stop();
}

TEST(AsyncGenerator_Test, SchedulerStatistics) {
    auto before = sgcl::async::scheduler::get_statistics();
    EXPECT_EQ(before.workers, 0u);                           // not started
    sgcl::async::channel<void> gate;
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::channel<void>& gate) -> sgcl::async::task<> {
            co_await gate.receive();
        }(gate)));
    }
    auto st = sgcl::async::scheduler::get_statistics();
    EXPECT_EQ(st.workers, sgcl::async::scheduler::workers());
    EXPECT_LE(st.spinning + st.sleeping, st.workers);
    gate.close();
    for (auto& t : tasks) {
        t.wait();
    }
    sgcl::async::scheduler::stop();
    EXPECT_EQ(sgcl::async::scheduler::get_statistics().workers, 0u);
}

// The generator runs as part of its consumer: on the consumer's executor
// and under its task-locals, also after a wait of its own (the frame
// resumed by the scheduler is the generator's, whose header must then
// say what the consumer's does), and the consumer resumed by the yield
// after such a wait is still on its own thread and still sees its locals
namespace {
    sgcl::async::task_local<int> request_id;

    sgcl::async::task<int> consumes(sgcl::async::channel<int>& in, std::vector<std::thread::id>& ids, std::vector<bool>& seen, std::vector<bool>& seen_inside) {
        co_await request_id.set(7);
        auto g = [](sgcl::async::channel<int>& in, std::vector<bool>& seen_inside) -> sgcl::async::generator<int> {
            while (auto v = co_await in.receive()) {   // a wait of the generator's own
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
    sgcl::async::executor ex;
    sgcl::async::channel<int> in;
    std::vector<std::thread::id> ids;
    std::vector<bool> seen, seen_inside;
    std::thread other([&] {
        for (int i = 1; i <= 4; ++i) {
            std::this_thread::sleep_for(2ms);                // the generator waits every time
            in.send(i).wait();
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
    sgcl::async::scheduler::stop();
}
