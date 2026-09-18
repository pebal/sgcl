//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The executor: a task on the thread that runs it, resumed there after
// every wait; run(task), poll from a loop of the program's own, stop and
// run again, an executor gone; on(ex) and on_workers round trips; the
// strand: tasks one at a time, in order, a wait leaving the strand.
#include "tests/types.h"

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

    task<std::thread::id> where() {
        co_return std::this_thread::get_id();
    }

    // The thread after every kind of wait: a sleep (the timer thread wakes
    // it), a channel receive (a thread serves it), a task on the workers
    // (a worker finishes it), an event (a thread sets it), a yield
    task<int> waits(sgcl::channel<int>& ch, sgcl::event& ev, std::vector<std::thread::id>& ids) {
        ids.push_back(std::this_thread::get_id());
        co_await sgcl::sleep(1ms);
        ids.push_back(std::this_thread::get_id());
        auto v = co_await ch.async_receive();
        ids.push_back(std::this_thread::get_id());
        auto sq = co_await sgcl::spawn(square(*v));
        ids.push_back(std::this_thread::get_id());
        co_await ev.async_wait();
        ids.push_back(std::this_thread::get_id());
        co_await sgcl::yield();
        ids.push_back(std::this_thread::get_id());
        auto sub = co_await where();   // an awaited task runs where its awaiter does
        ids.push_back(sub);
        co_return sq;
    }

    task<> round_trips(sgcl::executor& ex, std::vector<std::thread::id>& ids, std::vector<bool>& on_worker) {
        ids.push_back(std::this_thread::get_id());
        on_worker.push_back(sgcl::scheduler::on_worker());
        co_await sgcl::on(ex);
        ids.push_back(std::this_thread::get_id());
        on_worker.push_back(sgcl::scheduler::on_worker());
        co_await sgcl::on(ex);            // there already: no hop
        ids.push_back(std::this_thread::get_id());
        co_await sgcl::on_workers();
        ids.push_back(std::this_thread::get_id());
        on_worker.push_back(sgcl::scheduler::on_worker());
        co_await sgcl::on_workers();      // there already
        on_worker.push_back(sgcl::scheduler::on_worker());
        co_await sgcl::on(ex);
        ids.push_back(std::this_thread::get_id());
        on_worker.push_back(sgcl::scheduler::on_worker());
    }

    task<> yields(int n, sgcl::atomic<int>& steps, std::thread::id expected, sgcl::atomic<int>& wrong) {
        for (int i = 0; i < n; ++i) {
            if (std::this_thread::get_id() != expected) {
                ++wrong;
            }
            co_await sgcl::yield();
            ++steps;
        }
    }

    task<> stopper(sgcl::executor& ex, sgcl::atomic<int>& steps) {
        ++steps;
        ex.stop();
        co_await sgcl::yield();   // queued again: run when the executor runs again
        ++steps;
    }

    task<> holds(tracked_ptr<Node> node, sgcl::channel<void>& go, sgcl::atomic<int>& seen) {
        co_await go.async_receive();
        seen = node->value;
    }

    // The strand: a plain int, no lock, a yield now and then
    task<> incrementer(int& counter, sgcl::atomic<int>& inside, sgcl::atomic<int>& overlaps, int n) {
        for (int i = 0; i < n; ++i) {
            if (inside.fetch_add(1) != 0) {
                ++overlaps;
            }
            ++counter;
            inside.fetch_sub(1);
            if (i % 1000 == 999) {
                co_await sgcl::yield();
            }
        }
    }

    task<> ordered(int index, std::vector<int>& order) {
        order.push_back(index);
        co_await sgcl::yield();
        order.push_back(index);
    }

    task<> waits_on_strand(sgcl::channel<void>& go, sgcl::atomic<int>& phase) {
        phase = 1;
        co_await go.async_receive();   // leaves the strand to the next task
        phase = 3;
    }

    task<> runs_meanwhile(sgcl::atomic<int>& phase, sgcl::atomic<int>& seen) {
        seen = phase.load();
        co_return;
    }

    task<> to_strand(sgcl::strand& s, sgcl::atomic<int>& state) {
        co_await sgcl::on(s);
        state = (sgcl::scheduler::on_worker() ? 1 : 0) + (s.busy() ? 2 : 0);
        co_await sgcl::on_workers();
    }
}

TEST(Executor_Tests, RunReturnsTheResult) {
    sgcl::executor ex;
    EXPECT_EQ(ex.run(square(7)), 49);
    EXPECT_THROW(ex.run(throwing()), std::runtime_error);
    auto id = ex.run(where());
    EXPECT_EQ(id, std::this_thread::get_id());
    EXPECT_FALSE(ex.running());
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, ResumedOnTheExecutorsThreadAfterEveryWait) {
    sgcl::executor ex;
    sgcl::channel<int> ch;
    sgcl::event ev;
    std::vector<std::thread::id> ids;
    std::thread other([&] {
        ch.send(6);
        std::this_thread::sleep_for(2ms);
        ev.set();
    });
    EXPECT_EQ(ex.run(waits(ch, ev, ids)), 36);
    other.join();
    ASSERT_EQ(ids.size(), 7u);
    for (auto id : ids) {
        EXPECT_EQ(id, std::this_thread::get_id());
    }
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, OnAndOnWorkersRoundTrips) {
    sgcl::executor ex;
    std::vector<std::thread::id> ids;
    std::vector<bool> on_worker;
    auto t = sgcl::spawn(round_trips(ex, ids, on_worker));   // starts on the workers
    ex.run_until(t);
    EXPECT_TRUE(t.done());
    ASSERT_EQ(ids.size(), 5u);
    auto me = std::this_thread::get_id();
    EXPECT_NE(ids[0], me);
    EXPECT_EQ(ids[1], me);
    EXPECT_EQ(ids[2], me);
    EXPECT_NE(ids[3], me);
    EXPECT_EQ(ids[4], me);
    ASSERT_EQ(on_worker.size(), 5u);
    EXPECT_TRUE(on_worker[0]);
    EXPECT_FALSE(on_worker[1]);
    EXPECT_TRUE(on_worker[2]);
    EXPECT_TRUE(on_worker[3]);
    EXPECT_FALSE(on_worker[4]);
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, PollFromALoopOfOnesOwn) {
    sgcl::executor ex;
    sgcl::atomic<int> steps = {0};
    sgcl::atomic<int> wrong = {0};
    EXPECT_EQ(ex.poll(), 0u);
    std::vector<task<>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(ex.spawn(yields(10, steps, std::this_thread::get_id(), wrong)));
    }
    EXPECT_EQ(steps.load(), 0);          // nothing runs until the thread polls
    size_t passes = 0, ran = 0;
    while (steps.load() < 40) {
        ran += ex.poll();                // one pass: each task to its next yield
        ++passes;
    }
    EXPECT_EQ(passes, 11u);              // 4 tasks, 10 yields each, 4 resumes per pass: to the first yield, then a step per pass
    EXPECT_EQ(ran, 44u);
    for (auto& t : tasks) {
        EXPECT_TRUE(t.done());
    }
    EXPECT_EQ(wrong.load(), 0);
    EXPECT_EQ(ex.poll(), 0u);
}

TEST(Executor_Tests, StopReturnsFromRunAndTheTasksStay) {
    sgcl::executor ex;
    sgcl::atomic<int> steps = {0};
    auto t = ex.spawn(stopper(ex, steps));
    ex.run();                            // returns at the stop, the task queued again by its yield
    EXPECT_EQ(steps.load(), 1);
    EXPECT_FALSE(t.done());
    ex.run_until(t);                     // the next run resumes it
    EXPECT_EQ(steps.load(), 2);
    EXPECT_TRUE(t.done());
    ex.stop();                           // with no run in progress: the next run returns at once
    ex.run();
    EXPECT_EQ(ex.run(square(3)), 9);     // and the one after runs
}

TEST(Executor_Tests, AnExecutorGoneLeavesItsTasksSuspended) {
    sgcl::channel<void> go;
    sgcl::atomic<int> seen = {0};
    off_frame([&] {
        sgcl::executor ex;
        ex.go(holds(make_tracked<Node>(5), go, seen));
        ex.poll();                       // to the receive
        EXPECT_EQ(Node::alive.load(), 1);
    });
    go.send();                           // the wake lands on the queue nobody runs
    std::this_thread::sleep_for(5ms);
    EXPECT_EQ(seen.load(), 0);
    settle();                            // the frame, the queue and the node: a cycle nothing holds
    EXPECT_EQ(Node::alive.load(), 0);
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, SpawnedFromAWorkerAndAwaitedThere) {
    sgcl::executor ex;
    std::vector<std::thread::id> ids;
    auto outer = [](sgcl::executor& ex, std::vector<std::thread::id>& ids) -> task<> {
        ids.push_back(co_await sgcl::spawn(where(), ex));   // started on the executor from a worker
        ids.push_back(co_await sgcl::spawn(where()));       // on the workers
        ids.push_back(co_await where());                    // where the awaiter is: a worker
    };
    auto t = sgcl::spawn(outer(ex, ids));
    ex.run_until(t);
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], std::this_thread::get_id());
    EXPECT_NE(ids[1], std::this_thread::get_id());
    EXPECT_NE(ids[2], std::this_thread::get_id());
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, StrandRunsOneTaskAtATime) {
    sgcl::strand s;
    int counter = 0;
    sgcl::atomic<int> inside = {0};
    sgcl::atomic<int> overlaps = {0};
    std::vector<task<>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(s.spawn(incrementer(counter, inside, overlaps, 100000)));
    }
    for (auto& t : tasks) {
        t.join();
    }
    EXPECT_EQ(counter, 800000);
    EXPECT_EQ(overlaps.load(), 0);
    for (int i = 0; i < 1000 && s.busy(); ++i) {   // the last run is counted until the worker's epilogue
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_FALSE(s.busy());
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, StrandKeepsTheOrder) {
    sgcl::strand s;
    std::vector<int> order;
    // Spawned from a task on the strand, so that they queue behind it in
    // the order of the spawns (spawned from this thread, the first would
    // run and yield while the others are still being spawned, and its
    // yield would be an arrival among theirs)
    auto spawner = [](sgcl::strand& s, std::vector<int>& order) -> task<> {
        co_await sgcl::on(s);
        std::vector<task<>> tasks;
        for (int i = 0; i < 8; ++i) {
            tasks.push_back(s.spawn(ordered(i, order)));
        }
        for (auto& t : tasks) {
            co_await t;
        }
    };
    sgcl::spawn(spawner(s, order)).join();
    ASSERT_EQ(order.size(), 16u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(order[i], i);          // the first runs, in the order of the spawns
        EXPECT_EQ(order[8 + i], i);      // the second, in the order of the yields
    }
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, AWaitLeavesTheStrandToTheNext) {
    sgcl::strand s;
    sgcl::channel<void> go;
    sgcl::atomic<int> phase = {0};
    sgcl::atomic<int> seen = {0};
    auto a = s.spawn(waits_on_strand(go, phase));
    auto b = s.spawn(runs_meanwhile(phase, seen));
    b.join();
    EXPECT_EQ(seen.load(), 1);           // b ran while a waited: a waits off the strand (b's own run may still be counted here: busy() not asserted)
    go.send();
    a.join();
    EXPECT_EQ(phase.load(), 3);
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, OnStrandFromAWorker) {
    sgcl::strand s;
    sgcl::atomic<int> state = {0};
    sgcl::spawn(to_strand(s, state)).join();
    EXPECT_EQ(state.load(), 3);          // on a worker, and the strand busy with it
    sgcl::scheduler::stop();
}

TEST(Executor_Tests, StrandFromAnExecutorAndBack) {
    sgcl::executor ex;
    sgcl::strand s;
    std::vector<std::thread::id> ids;
    auto t = [](sgcl::strand& s, sgcl::executor& ex, std::vector<std::thread::id>& ids) -> task<> {
        ids.push_back(std::this_thread::get_id());
        co_await sgcl::on(s);
        ids.push_back(std::this_thread::get_id());
        co_await sgcl::sleep(1ms);       // woken back onto the strand, on a worker
        ids.push_back(std::this_thread::get_id());
        co_await sgcl::on(ex);
        ids.push_back(std::this_thread::get_id());
    };
    ex.run(t(s, ex, ids));
    ASSERT_EQ(ids.size(), 4u);
    EXPECT_EQ(ids[0], std::this_thread::get_id());
    EXPECT_NE(ids[1], std::this_thread::get_id());
    EXPECT_NE(ids[2], std::this_thread::get_id());
    EXPECT_EQ(ids[3], std::this_thread::get_id());
    sgcl::scheduler::stop();
}
