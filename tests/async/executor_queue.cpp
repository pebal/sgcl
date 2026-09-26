//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The queue of an executor and a strand under load (scheduler.h:
// ExecutorQueue, an intrusive list through the frames' headers): many
// threads posting into one executor, many tasks hopping onto it, the
// strand's order for each producer, stop and run again with frames
// queued, frames kept alive by the queue alone through collections, an
// executor dropped with frames on it, tasks moving between two executors,
// a strand and the workers.
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Held {
        explicit Held(int v) : value(v) { ++alive; }
        ~Held() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // Waits for a count a detached task keeps, a minute at most
    void wait_for(const sgcl::atomic<int>& count, int value) {
        for (int i = 0; i < 60000 && count.load() < value; ++i) {
            std::this_thread::sleep_for(1ms);
        }
    }

    task<> counted(sgcl::async::executor& ex, std::thread::id thread, sgcl::atomic<int>& ran, sgcl::atomic<int>& wrong, int total) {
        if (std::this_thread::get_id() != thread) {
            ++wrong;
        }
        if (++ran == total) {
            ex.stop();
        }
        co_return;
    }

    task<> hops(sgcl::async::executor& ex, std::thread::id thread, int n, sgcl::atomic<int>& wrong) {
        for (int i = 0; i < n; ++i) {
            co_await sgcl::async::on(ex);
            if (std::this_thread::get_id() != thread) {
                ++wrong;
            }
            co_await sgcl::async::on_workers();
            if (!sgcl::async::scheduler::on_worker()) {
                ++wrong;
            }
        }
    }

    template<class Executor>
    task<> hoppers(Executor& ex, std::thread::id thread, int tasks, int n, sgcl::atomic<int>& wrong) {
        std::vector<task<>> ts;
        for (int k = 0; k < tasks; ++k) {
            ts.push_back(sgcl::async::spawn(hops(ex, thread, n, wrong)));
        }
        for (auto& t : ts) {
            co_await t;
        }
    }

    task<> strand_hops(sgcl::async::strand& s, int n, int& counter, sgcl::atomic<int>& inside, sgcl::atomic<int>& overlaps, sgcl::atomic<int>& wrong) {
        for (int i = 0; i < n; ++i) {
            co_await sgcl::async::on(s);
            if (!sgcl::async::scheduler::on_worker()) {
                ++wrong;
            }
            if (inside.fetch_add(1) != 0) {
                ++overlaps;
            }
            ++counter;
            inside.fetch_sub(1);
            co_await sgcl::async::on_workers();
        }
    }

    // A strand's task: its producer's sequence written down, on the
    // strand, with a plain vector and nothing to guard it
    task<> recorded(std::vector<int>& seen, int i, sgcl::atomic<int>& inside, sgcl::atomic<int>& overlaps, sgcl::atomic<int>& done) {
        if (inside.fetch_add(1) != 0) {
            ++overlaps;
        }
        seen.push_back(i);
        inside.fetch_sub(1);
        ++done;
        co_return;
    }

    task<> bump(sgcl::atomic<int>& ran) {
        ++ran;
        co_return;
    }

    task<> checks(tracked_ptr<Held> held, int expected, sgcl::atomic<int>& bad, sgcl::atomic<int>& ran) {
        if (held->value != expected) {
            ++bad;
        }
        ++ran;
        co_return;
    }

    // Round the four places: two executors, a strand, the workers
    task<> travels(sgcl::async::executor& a, std::thread::id ta, sgcl::async::executor& b, std::thread::id tb, sgcl::async::strand& s, int n, sgcl::atomic<int>& wrong) {
        for (int i = 0; i < n; ++i) {
            co_await sgcl::async::on(a);
            if (std::this_thread::get_id() != ta) {
                ++wrong;
            }
            co_await sgcl::async::yield();
            if (std::this_thread::get_id() != ta) {
                ++wrong;
            }
            co_await sgcl::async::on(b);
            if (std::this_thread::get_id() != tb) {
                ++wrong;
            }
            co_await sgcl::async::on(s);
            if (!sgcl::async::scheduler::on_worker()) {
                ++wrong;
            }
            co_await sgcl::async::yield();   // back through the strand's queue
            co_await sgcl::async::on_workers();
            if (!sgcl::async::scheduler::on_worker()) {
                ++wrong;
            }
        }
    }
}

TEST(ExecutorQueue_Tests, ManyThreadsPostIntoOneExecutor) {
    constexpr int Threads = 8;
    constexpr int PerThread = 20000;
    constexpr int Total = Threads * PerThread;
    sgcl::async::executor ex;
    sgcl::atomic<int> ran = {0};
    sgcl::atomic<int> wrong = {0};
    auto me = std::this_thread::get_id();
    std::vector<std::thread> producers;
    for (int k = 0; k < Threads; ++k) {
        producers.emplace_back([&] {
            for (int i = 0; i < PerThread; ++i) {
                ex.go(counted(ex, me, ran, wrong, Total));
            }
        });
    }
    ex.run();                            // until the last task stops it
    for (auto& p : producers) {
        p.join();
    }
    EXPECT_EQ(ran.load(), Total);
    EXPECT_EQ(wrong.load(), 0);
    EXPECT_EQ(ex.poll(), 0u);            // nothing left, nothing run twice
    sgcl::async::scheduler::stop();
}

TEST(ExecutorQueue_Tests, ManyTasksHopOntoOneExecutor) {
    sgcl::async::executor ex;
    sgcl::atomic<int> wrong = {0};
    ex.run(hoppers(ex, std::this_thread::get_id(), 16, 2000, wrong));
    EXPECT_EQ(wrong.load(), 0);
    sgcl::async::scheduler::stop();
}

TEST(ExecutorQueue_Tests, ManyTasksHopOntoOneStrand) {
    sgcl::async::strand s;
    int counter = 0;
    sgcl::atomic<int> inside = {0};
    sgcl::atomic<int> overlaps = {0};
    sgcl::atomic<int> wrong = {0};
    auto driver = [](sgcl::async::strand& s, int& counter, sgcl::atomic<int>& inside, sgcl::atomic<int>& overlaps, sgcl::atomic<int>& wrong) -> task<> {
        std::vector<task<>> ts;
        for (int k = 0; k < 16; ++k) {
            ts.push_back(sgcl::async::spawn(strand_hops(s, 2000, counter, inside, overlaps, wrong)));
        }
        for (auto& t : ts) {
            co_await t;
        }
    };
    sgcl::async::spawn(driver(s, counter, inside, overlaps, wrong)).wait();
    EXPECT_EQ(counter, 16 * 2000);       // a plain int, one task at a time
    EXPECT_EQ(overlaps.load(), 0);
    EXPECT_EQ(wrong.load(), 0);
    for (int i = 0; i < 1000 && s.busy(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_FALSE(s.busy());
    sgcl::async::scheduler::stop();
}

TEST(ExecutorQueue_Tests, StrandKeepsEachProducersOrder) {
    constexpr int Threads = 8;
    constexpr int PerThread = 5000;
    sgcl::async::strand s;
    std::vector<std::vector<int>> seen(Threads);
    sgcl::atomic<int> inside = {0};
    sgcl::atomic<int> overlaps = {0};
    sgcl::atomic<int> done = {0};
    std::vector<std::thread> producers;
    for (int k = 0; k < Threads; ++k) {
        producers.emplace_back([&, k] {
            for (int i = 0; i < PerThread; ++i) {
                s.go(recorded(seen[k], i, inside, overlaps, done));
            }
        });
    }
    for (auto& p : producers) {
        p.join();
    }
    wait_for(done, Threads * PerThread);
    ASSERT_EQ(done.load(), Threads * PerThread);
    EXPECT_EQ(overlaps.load(), 0);
    for (int k = 0; k < Threads; ++k) {
        ASSERT_EQ(seen[k].size(), size_t(PerThread));
        int out = 0;
        for (int i = 0; i < PerThread; ++i) {
            out += seen[k][i] != i;
        }
        EXPECT_EQ(out, 0) << "producer " << k;   // one thread's pushes in its order
    }
    for (int i = 0; i < 1000 && s.busy(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_FALSE(s.busy());
    sgcl::async::scheduler::stop();
}

TEST(ExecutorQueue_Tests, StopAndRunAgainWhileThreadsPost) {
    constexpr int Threads = 4;
    constexpr int PerThread = 10000;
    constexpr int Total = Threads * PerThread;
    sgcl::async::executor ex;
    sgcl::atomic<int> ran = {0};
    std::atomic<bool> finished = {false};
    std::vector<std::thread> producers;
    for (int k = 0; k < Threads; ++k) {
        producers.emplace_back([&] {
            for (int i = 0; i < PerThread; ++i) {
                ex.go(bump(ran));
            }
        });
    }
    std::thread stopper([&] {            // a stop now and then, from outside, until everything ran
        while (!finished.load()) {
            ex.stop();
            std::this_thread::sleep_for(50us);
        }
    });
    int runs = 0;
    while (ran.load() < Total) {
        if (++runs % 2) {
            ex.run();                    // to the next stop, the frames left queued
        } else {
            ex.poll();                   // or one pass
        }
    }
    finished = true;
    stopper.join();
    for (auto& p : producers) {
        p.join();
    }
    EXPECT_EQ(ran.load(), Total);        // each once: no frame lost across a stop, none run twice
    EXPECT_EQ(ex.poll(), 0u);
    sgcl::async::scheduler::stop();
}

TEST(ExecutorQueue_Tests, QueuedFramesAreHeldByTheQueueAlone) {
    constexpr int N = 2000;
    sgcl::async::executor ex;
    sgcl::atomic<int> bad = {0};
    sgcl::atomic<int> ran = {0};
    off_frame([&] {
        for (int i = 0; i < N; ++i) {
            ex.go(checks(make_tracked<Held>(i), i, bad, ran));   // detached: the queue's links are all that holds the frames
        }
    });
    settle();
    EXPECT_EQ(Held::alive.load(), N);
    EXPECT_EQ(ex.poll(), size_t(N));
    EXPECT_EQ(ran.load(), N);
    EXPECT_EQ(bad.load(), 0);
    settle();
    EXPECT_EQ(Held::alive.load(), 0);    // and let go of once taken: a frame run holds nothing of the queue
    sgcl::async::scheduler::stop();
}

TEST(ExecutorQueue_Tests, AnExecutorDroppedWithFramesQueued) {
    constexpr int N = 2000;
    sgcl::atomic<int> bad = {0};
    sgcl::atomic<int> ran = {0};
    off_frame([&] {
        sgcl::async::executor ex;
        for (int i = 0; i < N; ++i) {
            ex.go(checks(make_tracked<Held>(i), i, bad, ran));
        }
        ex.poll();                       // the first N run, the next N left on the queue
        EXPECT_EQ(ran.load(), N);
        for (int i = 0; i < N; ++i) {
            ex.go(checks(make_tracked<Held>(i), i, bad, ran));
        }
    });
    settle();                            // the queue, its stub and every frame on it: nothing holds them
    EXPECT_EQ(Held::alive.load(), 0);
    EXPECT_EQ(ran.load(), N);
    EXPECT_EQ(bad.load(), 0);
    sgcl::async::scheduler::stop();
}

TEST(ExecutorQueue_Tests, TasksTravelBetweenTwoExecutorsAStrandAndTheWorkers) {
    sgcl::async::executor a;
    sgcl::async::executor b;
    sgcl::async::strand s;
    sgcl::atomic<int> wrong = {0};
    std::thread::id tb;
    std::atomic<bool> up = {false};
    std::thread other([&] {
        tb = std::this_thread::get_id();
        up = true;
        b.run();
    });
    while (!up.load()) {
        std::this_thread::yield();
    }
    auto driver = [](sgcl::async::executor& a, std::thread::id ta, sgcl::async::executor& b, std::thread::id tb, sgcl::async::strand& s, sgcl::atomic<int>& wrong) -> task<> {
        std::vector<task<>> ts;
        for (int k = 0; k < 8; ++k) {
            ts.push_back(sgcl::async::spawn(travels(a, ta, b, tb, s, 500, wrong)));
        }
        for (auto& t : ts) {
            co_await t;
        }
    };
    a.run(driver(a, std::this_thread::get_id(), b, tb, s, wrong));
    b.stop();
    other.join();
    EXPECT_EQ(wrong.load(), 0);
    sgcl::async::scheduler::stop();
}
