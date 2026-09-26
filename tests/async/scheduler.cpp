//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The scheduler: tasks spawned on a pool of workers, joined from threads,
// awaited from tasks, detached; the channel as the way they talk.
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
#include <coroutine>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    task<int> square(int v) {
        co_return v * v;
    }

    task<> throwing() {
        throw std::runtime_error("boom");
        co_return;
    }

    // A task that awaits others: the awaits suspend it, no thread held
    task<int> sum_of_squares(int n) {
        int sum = 0;
        for (int i = 1; i <= n; ++i) {
            sum += co_await sgcl::async::spawn(square(i));
        }
        co_return sum;
    }

    // Many tasks on one channel, each receiving one element
    task<> receiver(sgcl::async::channel<int>& ch, sgcl::atomic<long>& sum, sgcl::atomic<int>& done) {
        if (auto v = co_await ch.receive()) {
            sum += *v;
        }
        ++done;
    }

    task<> on_worker_check(sgcl::atomic<int>& seen) {
        seen = sgcl::async::scheduler::on_worker() ? 1 : 2;
        co_return;
    }

    task<int> yielder(int n) {
        int steps = 0;
        for (int i = 0; i < n; ++i) {
            co_await sgcl::async::yield();
            ++steps;
        }
        co_return steps;
    }

    task<> holds_node(tracked_ptr<Node> node, sgcl::async::channel<void>& go, sgcl::atomic<int>& seen) {
        co_await go.receive();   // suspended with the node in the frame
        seen = node->value;
    }
}

TEST(Scheduler_Tests, SpawnAndJoin) {
    auto t = sgcl::async::spawn(square(7));
    EXPECT_EQ(t.wait(), 49);
    EXPECT_TRUE(t.done());
    EXPECT_EQ(t.result(), 49);        // again, after
    EXPECT_GE(sgcl::async::scheduler::workers(), 1u);
    EXPECT_FALSE(sgcl::async::scheduler::on_worker());
    sgcl::async::scheduler::stop();
}

TEST(Scheduler_Tests, AnExceptionComesOutOfJoin) {
    auto t = sgcl::async::spawn(throwing());
    EXPECT_THROW(t.wait(), std::runtime_error);
    EXPECT_TRUE(t.done());
    sgcl::async::scheduler::stop();
}

TEST(Scheduler_Tests, ATaskAwaitsTasks) {
    auto t = sgcl::async::spawn(sum_of_squares(100));
    EXPECT_EQ(t.wait(), 338350);
    auto seen_task = [](sgcl::atomic<int>& seen) -> task<> {
        auto inner = sgcl::async::spawn(on_worker_check(seen));
        co_await inner;               // the same task twice: done already the second time
        co_await inner;
    };
    sgcl::atomic<int> seen = {0};
    sgcl::async::spawn(seen_task(seen)).wait();
    EXPECT_EQ(seen.load(), 1);        // the inner task ran on a worker
    auto rethrown = []() -> task<int> {
        co_await sgcl::async::spawn(throwing());
        co_return 1;
    };
    auto r = sgcl::async::spawn(rethrown());
    EXPECT_THROW(r.wait(), std::runtime_error);
    sgcl::async::scheduler::stop();
}

TEST(Scheduler_Tests, DetachedTasksRunAndAreReclaimed) {
    settle();
    const int before = Node::alive.load();
    sgcl::atomic<int> ran = {0};
    auto job = [](sgcl::atomic<int>& ran, int v) -> task<> {
        tracked_ptr<Node> n = make_tracked<Node>(v);
        co_await sgcl::async::yield();
        ran += n->value;
    };
    off_frame([&] {
        for (int i = 1; i <= 100; ++i) {
            sgcl::async::go(job(ran, i));   // the task object let go of: the frame lives while it runs
        }
    });
    auto until = std::chrono::steady_clock::now() + 5s;
    while (ran.load() != 5050 && std::chrono::steady_clock::now() < until) {
        std::this_thread::yield();
    }
    EXPECT_EQ(ran.load(), 5050);
    sgcl::async::scheduler::stop();
    settle();
    EXPECT_EQ(Node::alive.load(), before);   // the frames, and the nodes in them, gone with the tasks
}

TEST(Scheduler_Tests, YieldGoesRound) {
    auto a = sgcl::async::spawn(yielder(1000));
    auto b = sgcl::async::spawn(yielder(1000));
    EXPECT_EQ(a.wait(), 1000);
    EXPECT_EQ(b.wait(), 1000);
    sgcl::async::scheduler::stop();
}

// A hundred thousand tasks waiting on one channel cost their frames and
// nothing else: no thread, no worker held
TEST(Scheduler_Tests, AHundredThousandTasksWaitOnAChannel) {
    sgcl::async::channel<int> ch;
    sgcl::atomic<long> sum = {0};
    sgcl::atomic<int> done = {0};
    constexpr int N = 100'000;
    std::vector<task<>> tasks;
    tasks.reserve(N);
    for (int i = 0; i < N; ++i) {
        tasks.push_back(sgcl::async::spawn(receiver(ch, sum, done)));
    }
    for (int i = 1; i <= N; ++i) {
        ch.send(i).wait();                   // a rendezvous each: the task is there
    }
    for (auto& t : tasks) {
        t.wait();
    }
    EXPECT_EQ(done.load(), N);
    EXPECT_EQ(sum.load(), long(N) * (N + 1) / 2);
    sgcl::async::scheduler::stop();
}

// close() makes a thousand waiting tasks ready at once
TEST(Scheduler_Tests, CloseWakesAThousandTasks) {
    sgcl::async::channel<int> ch;
    sgcl::atomic<long> sum = {0};
    sgcl::atomic<int> done = {0};
    std::vector<task<>> tasks;
    for (int i = 0; i < 1000; ++i) {
        tasks.push_back(sgcl::async::spawn(receiver(ch, sum, done)));
    }
    ch.close();
    for (auto& t : tasks) {
        t.wait();
    }
    EXPECT_EQ(done.load(), 1000);
    EXPECT_EQ(sum.load(), 0);
    sgcl::async::scheduler::stop();
}

// The frame of a suspended task is a root for its locals and parameters
// wherever it waits, a detached one included
TEST(Scheduler_Tests, ASuspendedDetachedFrameIsARoot) {
    settle();
    const int before = Node::alive.load();
    sgcl::async::channel<void> go;
    sgcl::atomic<int> seen = {0};
    off_frame([&] {
        sgcl::async::go(holds_node(make_tracked<Node>(42), go, seen));
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);   // held by the waiting frame, held by the channel's waiter
    go.send().wait();
    auto until = std::chrono::steady_clock::now() + 5s;
    while (seen.load() != 42 && std::chrono::steady_clock::now() < until) {
        std::this_thread::yield();
    }
    EXPECT_EQ(seen.load(), 42);
    sgcl::async::scheduler::stop();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

// Tasks as producers and consumers of a channel: the pipeline of the
// channel tests, all of it on the scheduler
TEST(Scheduler_Tests, TasksOnBothEndsOfAChannel) {
    sgcl::async::channel<int> ch(8);
    auto producer = [](sgcl::async::channel<int>& ch, int n) -> task<> {
        for (int i = 1; i <= n; ++i) {
            co_await ch.send(i);
        }
    };
    auto consumer = [](sgcl::async::channel<int>& ch) -> task<long> {
        long sum = 0;
        while (auto v = co_await ch.receive()) {
            sum += *v;
        }
        co_return sum;
    };
    std::vector<task<>> producers;
    for (int p = 0; p < 4; ++p) {
        producers.push_back(sgcl::async::spawn(producer(ch, 10000)));
    }
    auto c = sgcl::async::spawn(consumer(ch));
    for (auto& p : producers) {
        p.wait();
    }
    ch.close();
    EXPECT_EQ(c.wait(), 4L * 10000 * 10001 / 2);
    sgcl::async::scheduler::stop();
}

TEST(Scheduler_Tests, StopAndStartAgain) {
    EXPECT_EQ(sgcl::async::spawn(square(3)).wait(), 9);
    sgcl::async::scheduler::stop();
    EXPECT_EQ(sgcl::async::spawn(square(4)).wait(), 16);   // started again by the spawn
    sgcl::async::scheduler::stop();
    sgcl::async::scheduler::stop();                        // twice is nothing
}
