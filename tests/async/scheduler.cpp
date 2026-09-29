//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The scheduler: tasks spawned on a pool of workers, joined from threads,
// awaited from tasks, detached; the channel as the way they talk.
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
#include <coroutine>
#include <random>
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

namespace {
    // A task that holds its worker until all n of its kind are running at
    // once (a barrier of n, spun on, never a wait the scheduler sees): with
    // n the number of workers, every worker must run one at the same time,
    // so every sleeper must have been woken. False when the barrier was not
    // reached in five seconds (a lost wake: the test fails instead of
    // hanging)
    task<bool> barrier_task(std::atomic<int>* arrived, int n) {
        arrived->fetch_add(1);
        auto end = std::chrono::steady_clock::now() + 5s;
        while (arrived->load() < n) {
            if (std::chrono::steady_clock::now() > end) {
                co_return false;
            }
        }
        co_return true;
    }
}

// The wakes the spinners' cap and the conditional cascade must still give
// (scheduler.h: _find_work): n = the workers tasks enqueued at once, each
// holding its worker at a barrier of n, from a thread that is no worker
// (the global queue) and from a task on a worker (its ring, taken by
// thieves); every round with the workers asleep first
TEST(Scheduler_Tests, AsManyBarrierTasksAsWorkersAllRunAtOnce) {
    int n = (int)sgcl::async::scheduler::workers();
    for (int round = 0; round < 20; ++round) {
        std::this_thread::sleep_for(2ms);                 // the workers asleep
        std::atomic<int> arrived = {0};
        std::vector<task<bool>> ts;
        for (int i = 0; i < n; ++i) {
            ts.push_back(spawn(barrier_task(&arrived, n)));
        }
        for (auto& t : ts) {
            EXPECT_TRUE(t.wait());
        }
        std::this_thread::sleep_for(2ms);
        std::atomic<int> arrived2 = {0};
        auto from_task = spawn([](std::atomic<int>* a, int n) -> task<int> {
            std::vector<task<bool>> inner;
            for (int i = 0; i < n - 1; ++i) {             // onto this worker's ring; this task is the n-th
                inner.push_back(spawn(barrier_task(a, n)));
            }
            bool mine = co_await barrier_task(a, n);
            int ok = mine ? 1 : 0;
            for (auto& t : inner) {
                ok += (co_await t) ? 1 : 0;
            }
            co_return ok;
        }(&arrived2, n));
        EXPECT_EQ(from_task.wait(), n);
    }
    sgcl::async::scheduler::stop();
}

// Work left in a busy worker's ring while another worker spins: the spinner
// takes something else (a task from outside, a steady trickle of them), and
// the busy worker's second task must not wait for the busy one to finish.
// A long task (20 ms of computation) spawns two short ones onto its own
// ring; each records how long it waited to start; the worst of them, over
// rounds with the trickle keeping a spinner about, must be well under the
// long task's 20 ms
TEST(Scheduler_Tests, WorkInABusyWorkersRingIsTakenWhileItRuns) {
    std::atomic<bool> trickle = {true};
    std::thread outside([&] {                             // tasks from outside now and then: someone is spinning when the long task pushes
        while (trickle.load()) {
            spawn([]() -> task<> { co_return; }()).wait();
            std::this_thread::sleep_for(20us);
        }
    });
    int64_t worst = 0;
    for (int round = 0; round < 30; ++round) {
        auto waited = spawn([]() -> task<int64_t> {
            auto short_task = [](std::chrono::steady_clock::time_point queued) -> task<int64_t> {
                co_return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - queued).count();
            };
            auto begin = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - begin < 2ms) {
            }
            auto now = std::chrono::steady_clock::now();
            auto a = spawn(short_task(now));
            auto b = spawn(short_task(now));
            while (std::chrono::steady_clock::now() - begin < 20ms) {   // this worker busy: the two wait in its ring for a thief
            }
            int64_t wa = co_await a;
            int64_t wb = co_await b;
            co_return std::max(wa, wb);
        }()).wait();
        worst = std::max(worst, waited);
    }
    trickle = false;
    outside.join();
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
    worst = 0;   // the thread sanitizer stops the process for tens of milliseconds at a time: no latency to assert there
#endif
#endif
    // the worst of the 30 rounds, not a median: under a millisecond when
    // taken, eighteen when left to the busy worker; the bound is 5 ms, since
    // 1 ms failed once in a hundred runs of the program under a parallel
    // build (1067 us), which a missing steal would exceed by far
    EXPECT_LT(worst, 5000) << "a short task waited " << worst << " us";
    sgcl::async::scheduler::stop();
}

// The same with every other worker asleep and nobody looking: a busy
// worker's push is the only thing that happens, so the push itself must
// wake a sleeper (one when nobody looks). The workers left idle before
// each round, long past their spin
TEST(Scheduler_Tests, ABusyWorkersPushWakesASleeperWhenNobodyLooks) {
    int64_t worst = 0;
    for (int round = 0; round < 30; ++round) {
        std::this_thread::sleep_for(2ms);   // every worker past its spin, asleep
        auto waited = spawn([]() -> task<int64_t> {
            auto short_task = [](std::chrono::steady_clock::time_point queued) -> task<int64_t> {
                co_return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - queued).count();
            };
            auto begin = std::chrono::steady_clock::now();
            auto a = spawn(short_task(begin));
            while (std::chrono::steady_clock::now() - begin < 20ms) {   // this worker busy: the task waits in its ring for a sleeper
            }
            co_return co_await a;
        }()).wait();
        worst = std::max(worst, waited);
    }
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
    worst = 0;   // the thread sanitizer stops the process for tens of milliseconds at a time: no latency to assert there
#endif
#endif
    EXPECT_LT(worst, 5000) << "a task pushed by a busy worker waited " << worst << " us";   // twenty when nobody wakes for it
    sgcl::async::scheduler::stop();
}

// One worker looking (right after a short task) and three tasks pushed
// from a thread at once that wait for one another: a push that sees the
// looker wakes nobody, the looker takes one and holds it, so the other two
// need a wake from somewhere else (the finder's look at what still waits).
// Every round with a 2 s bound on the meeting
TEST(Scheduler_Tests, OneLookerAndThreeTasksThatWaitForOneAnother) {
    for (int round = 0; round < 20; ++round) {
        std::this_thread::sleep_for(2ms);
        spawn([]() -> task<> { co_return; }()).wait();   // its worker now looks for its window
        std::atomic<int> arrived = {0};
        std::vector<task<bool>> ts;
        for (int i = 0; i < 3; ++i) {
            ts.push_back(spawn([](std::atomic<int>& arrived) -> task<bool> {
                arrived.fetch_add(1);
                auto end = std::chrono::steady_clock::now() + 2s;
                while (arrived.load() < 3) {
                    if (std::chrono::steady_clock::now() > end) {
                        co_return false;
                    }
                }
                co_return true;
            }(arrived)));
        }
        for (auto& t : ts) {
            ASSERT_TRUE(t.wait()) << "round " << round << ": " << arrived.load() << " of 3 met";
        }
    }
    sgcl::async::scheduler::stop();
}

// Cold pushes from many threads at once, every worker asleep: each push
// that finds nobody looking wakes a sleeper; whatever the interleaving,
// every task runs soon (40 rounds, 2 s a round)
TEST(Scheduler_Tests, ColdPushesFromManyThreadsAllRun) {
    constexpr int Threads = 8, Tasks = 4, Rounds = 40;
    for (int round = 0; round < Rounds; ++round) {
        std::this_thread::sleep_for(2ms);   // every worker past its spin, asleep
        std::atomic<int> ran = {0};
        std::atomic<bool> go = {false};
        std::vector<std::thread> pushers;
        for (int t = 0; t < Threads; ++t) {
            pushers.emplace_back([&] {
                while (!go.load()) {
                }
                for (int i = 0; i < Tasks; ++i) {
                    sgcl::async::go([](std::atomic<int>& ran) -> task<> {
                        ran.fetch_add(1);
                        co_return;
                    }(ran));
                }
            });
        }
        go = true;
        for (auto& p : pushers) {
            p.join();
        }
        auto until = std::chrono::steady_clock::now() + 2s;
        while (ran.load() != Threads * Tasks && std::chrono::steady_clock::now() < until) {
            std::this_thread::yield();
        }
        ASSERT_EQ(ran.load(), Threads * Tasks) << "round " << round;
    }
    sgcl::async::scheduler::stop();
}

namespace sgcl::async::detail {
    // The ring's operations on rings of the test's own (Scheduler is a friend)
    struct SchedulerRingAccess {
        using Local = Scheduler::Local;
        using Frame = Scheduler::Frame;

        static void push(Scheduler& s, Local& l, Frame f) {
            s._push_local(l, std::move(f));
        }

        static Frame pop(Local& l) {
            return Scheduler::_pop_local(l);
        }

        static Frame steal(Scheduler& s, Local& victim, Local& mine) {
            return s._steal(victim, mine);
        }

        // what _find_work does before a thief's first look
        static void clear(Local& l) {
            Scheduler::_clear_taken(l, l.head.load(std::memory_order_acquire));
        }

        // the ring of worker i of a scheduler, and how many rings it has
        static Local& local(Scheduler& s, unsigned i) {
            return *s._locals[i];
        }

        static size_t rings(Scheduler& s) {
            return s._locals.size();
        }
    };
}

namespace {
    using Ring = sgcl::async::detail::SchedulerRingAccess;

    size_t held_slots(const Ring::Local& l) {
        size_t n = 0;
        for (auto& s : l.slots) {
            n += s != nullptr;
        }
        return n;
    }
}

// The ring under its owner and eight thieves, a million frames: the owner
// pushes bursts and pops some, the thieves steal halves into rings of their
// own and pop those empty, clearing them before each steal as _find_work
// does. Every frame is taken exactly once (a count per frame: none lost to
// a slot nulled under it, none taken twice), and once drained no ring
// holds a frame: the slots taken were nulled, the owner's by its batches
// and its last clear, a thief's copies left by a failed exchange by the
// thief. Run under the thread sanitizer as well: the thieves' reads of the
// slots against the owner's nulls
TEST(Scheduler_Tests, TheRingLosesNoFrameAndHoldsNoneTaken) {
    const uint32_t frames = 1'000'000;
    constexpr int thieves = 8;
    sgcl::async::detail::Scheduler s;   // never started: its rings' operations only
    tracked_ptr<Ring::Local> owner = make_tracked<Ring::Local>();
    std::vector<std::atomic<uint8_t>> taken(frames);
    auto count = [&](const Ring::Frame& f) {
        auto id = uint32_t(uintptr_t(f->word) >> 1);
        taken[id].fetch_add(1, std::memory_order_relaxed);
    };
    std::atomic<bool> done = {false};
    std::atomic<uint64_t> stolen = {0};
    std::atomic<size_t> left_in_thieves = {0};
    std::vector<std::thread> threads;
    for (int t = 0; t < thieves; ++t) {
        threads.emplace_back([&] {
            tracked_ptr<Ring::Local> mine = make_tracked<Ring::Local>();
            uint64_t got = 0;
            for (;;) {
                bool finished = done.load(std::memory_order_acquire);
                Ring::clear(*mine);
                if (auto f = Ring::steal(s, *owner, *mine)) {
                    count(f);
                    ++got;
                    while (auto g = Ring::pop(*mine)) {
                        count(g);
                        ++got;
                    }
                } else if (finished) {
                    break;
                }
            }
            Ring::clear(*mine);
            left_in_thieves.fetch_add(held_slots(*mine));
            stolen.fetch_add(got);
        });
    }
    std::mt19937 rng(20260927);
    uint32_t next = 0;
    while (next < frames) {
        uint32_t burst = 1 + rng() % 32;
        for (uint32_t i = 0; i < burst && next < frames; ++i) {
            if (owner->tail.load() - owner->head.load() >= Ring::Local::Size) {
                break;   // full: the scheduler would spill to the global queue, which this one does not have
            }
            tracked_ptr<sgcl::detail::FrameWord> f = make_tracked<sgcl::detail::FrameWord>();
            f->word = (void*)(uintptr_t(next++) << 1 | 1);   // an odd word: never an address
            Ring::push(s, *owner, f);
        }
        uint32_t pops = rng() % 32;
        for (uint32_t i = 0; i < pops; ++i) {
            auto f = Ring::pop(*owner);
            if (!f) {
                break;
            }
            count(f);
        }
    }
    while (auto f = Ring::pop(*owner)) {
        count(f);
    }
    done.store(true, std::memory_order_release);
    for (auto& t : threads) {
        t.join();
    }
    size_t lost = 0, twice = 0;
    for (auto& c : taken) {
        lost += c.load() == 0;
        twice += c.load() > 1;
    }
    EXPECT_EQ(lost, 0u);
    EXPECT_EQ(twice, 0u);
    EXPECT_GT(stolen.load(), 0u);   // the thieves took part
    Ring::clear(*owner);
    EXPECT_EQ(held_slots(*owner), 0u);
    EXPECT_EQ(left_in_thieves.load(), 0u);
}

// A finished task's frame is not held by the ring it was queued on: tasks
// spawned from a task (onto its worker's ring) keep a raw pointer to a node
// across a yield (a word of the frame no destructor nulls, as the frames of
// the HTTP server keep their requests); once they have finished and the
// workers are idle, the nodes are collected. Failed while the ring kept
// the words of the slots taken
TEST(Scheduler_Tests, AFinishedTasksFrameIsNotHeldByTheRing) {
    settle();
    const int before = Node::alive.load();
    sgcl::atomic<int> ran = {0};
    spawn([](sgcl::atomic<int>& ran) -> task<> {
        for (int i = 1; i <= 100; ++i) {
            sgcl::async::go([](sgcl::atomic<int>& ran, int v) -> task<> {
                tracked_ptr<Node> n = make_tracked<Node>(v);
                Node* volatile raw = n.get();   // in the frame, and left there by the end
                co_await sgcl::async::yield();
                ran += raw->value;
            }(ran, i));
        }
        co_return;
    }(ran)).wait();
    auto until = std::chrono::steady_clock::now() + 5s;
    while (ran.load() != 5050 && std::chrono::steady_clock::now() < until) {
        std::this_thread::yield();
    }
    EXPECT_EQ(ran.load(), 5050);
    sgcl::async::scheduler::stop();   // the workers idle, their stacks cleared
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

namespace {
    using Frame = sgcl::async::detail::Scheduler::Frame;
    using Global = sgcl::async::detail::Scheduler::Global;

    // A frame buffer as a coroutine's would be (a header, then one word
    // with the frame's number, odd: never an address), for the queue alone
    Frame numbered_frame(uint32_t id) {
        using namespace sgcl::detail;
        auto words = Maker<FrameWord[]>::make_tracked_data(FrameHeaderWords + 1).release();
        Frame f = sgcl::unique_ptr<FrameWord>(UniquePtr<FrameWord>(words));
        f.get()[FrameHeaderWords].word = (void*)(uintptr_t(id) << 1 | 1);
        return f;
    }

    uint32_t number_of(const Frame& f) {
        return uint32_t(uintptr_t(f.get()[sgcl::detail::FrameHeaderWords].word) >> 1);
    }
}

// The global queue (a concurrent::queue of frames, a node each) under
// eight producers pushing ranges of 1 to 64 new frames (the reactor's
// batches) and eight consumers popping one at a time and pushing a frame
// back a few times (a frame made ready again after its run). Every push is
// taken exactly once (a count per frame against the pushes it was given),
// and the queue ends empty
TEST(Scheduler_Tests, TheGlobalQueueTakesEveryPushOnce) {
    constexpr uint32_t Frames = 200'000;
    constexpr int Producers = 8, Consumers = 8;
    tracked_ptr<Global> q = make_tracked<Global>();
    std::vector<std::atomic<uint8_t>> taken(Frames);
    std::vector<uint8_t> again(Frames);                   // the pushes back each frame is given, written before its first push
    std::mt19937 seeds(20260927);
    for (auto& a : again) {
        a = uint8_t(seeds() % 4);
    }
    std::atomic<uint32_t> made = {0};
    std::atomic<uint32_t> producers_left = {Producers};
    std::atomic<uint64_t> pops = {0};
    uint64_t expected = Frames;
    for (auto a : again) {
        expected += a;
    }
    std::vector<std::thread> threads;
    for (int p = 0; p < Producers; ++p) {
        threads.emplace_back([&, p] {
            std::mt19937 rng(p + 1);
            Frame batch[64];
            for (;;) {
                uint32_t n = 1 + rng() % 64;
                uint32_t first = made.fetch_add(n);
                if (first >= Frames) {
                    break;
                }
                n = std::min(n, Frames - first);
                for (uint32_t i = 0; i < n; ++i) {
                    batch[i] = numbered_frame(first + i);
                }
                q->ready.push_range(batch, batch + n);
                for (uint32_t i = 0; i < n; ++i) {
                    batch[i] = nullptr;
                }
            }
            producers_left.fetch_sub(1);
        });
    }
    for (int c = 0; c < Consumers; ++c) {
        threads.emplace_back([&] {
            for (;;) {
                auto f = q->ready.try_pop();
                if (!f) {
                    if (!producers_left.load() && pops.load() == expected) {
                        break;
                    }
                    continue;
                }
                auto id = number_of(*f);
                auto n = taken[id].fetch_add(1) + 1;
                pops.fetch_add(1);
                if (n <= again[id]) {
                    q->ready.push(*f);   // made ready again: the same frame on the queue once more
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    size_t wrong = 0;
    for (uint32_t i = 0; i < Frames; ++i) {
        wrong += taken[i].load() != 1u + again[i];
    }
    EXPECT_EQ(wrong, 0u);
    EXPECT_EQ(pops.load(), expected);
    EXPECT_TRUE(q->ready.empty());
}

namespace {
    // A coroutine parked by putting its frame where a plain thread takes
    // it: the frame copied into the handoff (a copy of the promise's word:
    // a frame is a buffer, which a tracked_ptr is never made from by its
    // raw address), then the flag set, the thread's acquire its order
    struct Handoff {
        Frame frame;
        std::atomic<bool> full = {false};

        Frame take() {
            while (!full.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            Frame f = frame;
            frame = nullptr;
            full.store(false, std::memory_order_relaxed);
            return f;
        }
    };

    struct ParkHere {
        Handoff& handoff;

        bool await_ready() const noexcept {
            return false;
        }

        template<class P>
        void await_suspend(std::coroutine_handle<P> h) {
            handoff.frame = sgcl::detail::frame_of(h);
            handoff.full.store(true, std::memory_order_release);
        }

        void await_resume() const noexcept {
        }
    };
}

// A wake from off the workers (the reactor's, a timer's, a plain thread's)
// allocates one node of the global queue and nothing more: a task parks
// with its frame in a slot, a plain thread takes the frame and makes it
// ready, and waits for the next park. The node is 32 bytes (its link, the
// frame in an optional, the claim); an intrusive list through the frames,
// which allocated nothing, was measured and dropped (the global queue
// stays on its nodes)
TEST(Scheduler_Tests, AWakeFromOffTheWorkersAllocatesOneNode) {
    constexpr size_t Wakes = 20'000, NodeBytes = 32;
    Handoff handoff;
    std::atomic<bool> stop = {false};
    std::atomic<uint64_t> rounds = {0};
    auto parker = spawn([](Handoff& handoff, std::atomic<bool>& stop, std::atomic<uint64_t>& rounds) -> task<> {
        while (!stop.load()) {
            co_await ParkHere{handoff};
            rounds.fetch_add(1);
        }
    }(handoff, stop, rounds));
    auto wake_once = [&] {
        sgcl::async::detail::enqueue(handoff.take(), false);   // not a worker: the global queue
    };
    size_t bytes = managed_bytes_of(Wakes, wake_once);
    EXPECT_LE(bytes, Wakes * NodeBytes + config::page_size);   // counted in pages: 655 360 for 640 000
    EXPECT_GT(bytes, 0u);
    stop = true;
    wake_once();
    parker.wait();
    EXPECT_GT(rounds.load(), 2 * Wakes);
    sgcl::async::scheduler::stop();
}

// A plain thread and a task in a rendezvous ping-pong, each of the
// thread's sends a wake from off the workers, through the global queue:
// the same frame on it again at every round, never lost, never twice
TEST(Scheduler_Tests, APlainThreadAndATaskPingPongThroughTheGlobalQueue) {
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
    const int n = 100'000;   // the sanitizer's pace
#else
    const int n = 1'000'000;
#endif
#else
    const int n = 1'000'000;
#endif
    sgcl::async::channel<int> ping, pong;
    auto echo = spawn([](sgcl::async::channel<int>& ping, sgcl::async::channel<int>& pong) -> task<long> {
        long sum = 0;
        while (auto v = co_await ping.receive()) {
            sum += *v;
            co_await pong.send(*v + 1);
        }
        co_return sum;
    }(ping, pong));
    long back = 0;
    for (int i = 0; i < n; ++i) {
        ping.send(i).wait();
        auto r = pong.receive().wait();
        ASSERT_TRUE(r);
        ASSERT_EQ(*r, i + 1);
        back += *r;
    }
    ping.close();
    EXPECT_EQ(echo.wait(), long(n) * (n - 1) / 2);
    EXPECT_EQ(back, long(n) * (n + 1) / 2);
    sgcl::async::scheduler::stop();
}

// set_workers at run time: the workers stopped and started again with the
// new number, the tasks run under each; set_worker_spin applies at once
TEST(Scheduler_Tests, SetWorkersAtRunTime) {
    const unsigned before = sgcl::async::scheduler::workers();   // started
    for (unsigned n : {3u, 1u, 5u, 2u}) {
        sgcl::async::scheduler::set_workers(n);
        EXPECT_EQ(sgcl::async::scheduler::workers(), n);
        EXPECT_EQ(sgcl::async::scheduler::get_statistics().workers, n);
        sgcl::vector<task<int>> tasks;   // handles hold tracked words: a managed vector
        for (int i = 0; i < 200; ++i) {
            tasks.push_back(sgcl::async::spawn(square(i)));
        }
        long sum = 0;
        for (auto& t : tasks) {
            sum += t.wait();
        }
        EXPECT_EQ(sum, 2646700);   // the squares of 0..199
    }
    sgcl::async::scheduler::set_workers(2);
    sgcl::async::scheduler::set_workers(2);                    // the same number: nothing
    EXPECT_EQ(sgcl::async::scheduler::workers(), 2u);
    const auto spin = sgcl::async::scheduler::worker_spin();
    sgcl::async::scheduler::set_worker_spin(std::chrono::microseconds(7));
    EXPECT_EQ(sgcl::async::scheduler::worker_spin(), std::chrono::microseconds(7));
    EXPECT_EQ(sgcl::async::spawn(square(6)).wait(), 36);
    sgcl::async::scheduler::set_worker_spin(spin);
    sgcl::async::scheduler::set_workers(before);
    EXPECT_EQ(sgcl::async::scheduler::workers(), before);
    sgcl::async::scheduler::stop();
}

namespace {
    using Frame = sgcl::async::detail::Scheduler::Frame;

    // Suspends its coroutine and hands its frame out, made ready by nobody
    struct HandOut {
        Frame* out;

        bool await_ready() const noexcept {
            return false;
        }

        template<class P>
        void await_suspend(std::coroutine_handle<P> h) {
            *out = sgcl::async::detail::frame_of(h);
        }

        void await_resume() const noexcept {
        }
    };

    task<int> parked(Frame* out, sgcl::atomic<int>* ran, int value) {
        co_await HandOut{out};
        ran->fetch_add(1);
        co_return value;
    }
}

// The tasks in the rings (and the next slots) of workers that a smaller
// set_workers takes away run after the start: the start hands them to the
// global queue. Two parked tasks, put while the scheduler is stopped into
// the ring and the next slot of worker 3 of 4, then two workers
TEST(Scheduler_Tests, TheRingsOfWorkersTakenAwayAreRun) {
    const unsigned before = sgcl::async::scheduler::workers();
    auto& s = sgcl::async::detail::scheduler_instance();
    sgcl::async::scheduler::set_workers(4);
    ASSERT_GE(Ring::rings(s), 4u);
    sgcl::atomic<int> ran = {0};
    Frame a, b;
    task<int> ta = sgcl::async::spawn(parked(&a, &ran, 10));
    task<int> tb = sgcl::async::spawn(parked(&b, &ran, 20));
    auto until = std::chrono::steady_clock::now() + 5s;
    while ((!a || !b) && std::chrono::steady_clock::now() < until) {
        std::this_thread::yield();
    }
    ASSERT_TRUE(a && b);
    sgcl::async::scheduler::stop();
    Ring::Local& gone = Ring::local(s, 3);
    Ring::push(s, gone, std::move(a));
    gone.next = std::move(b);
    a = nullptr;
    b = nullptr;
    sgcl::async::scheduler::set_workers(2);                    // stopped: taken by the next start
    EXPECT_EQ(sgcl::async::scheduler::workers(), 2u);          // the start
    EXPECT_EQ(ta.wait(), 10);
    EXPECT_EQ(tb.wait(), 20);
    EXPECT_EQ(ran.load(), 2);
    EXPECT_EQ(gone.head.load(), gone.tail.load());
    EXPECT_EQ(held_slots(gone), 0u);
    EXPECT_FALSE(gone.next);
    sgcl::async::scheduler::set_workers(4);                    // and back: the ring is a worker's again
    EXPECT_EQ(sgcl::async::spawn(square(9)).wait(), 81);
    sgcl::async::scheduler::set_workers(before);
    sgcl::async::scheduler::stop();
}

// blocking_pool::set_threads: the cap from now on
TEST(Scheduler_Tests, BlockingPoolSetThreads) {
    const unsigned before = sgcl::async::blocking_pool::max_threads();
    sgcl::async::blocking_pool::set_threads(3);
    EXPECT_EQ(sgcl::async::blocking_pool::max_threads(), 3u);
    sgcl::vector<sgcl::async::blocking_task<int>> jobs;
    for (int i = 0; i < 12; ++i) {
        jobs.push_back(sgcl::async::spawn_blocking([i] { return i; }));
    }
    int sum = 0;
    for (auto& j : jobs) {
        sum += j.wait();
    }
    EXPECT_EQ(sum, 66);
    EXPECT_LE(sgcl::async::blocking_pool::get_statistics().threads, 3u);
    sgcl::async::blocking_pool::set_threads(before);
    EXPECT_EQ(sgcl::async::blocking_pool::max_threads(), before);
    sgcl::async::scheduler::stop();
}

// The processor's counter the spins run on (core/detail/ticks.h): never
// backwards, and its rate the steady clock's within 5% over 10 ms
TEST(Scheduler_Tests, TheSpinsCounterIsMonotonicAndAtItsRate) {
    uint64_t last = sgcl::detail::cpu_ticks();
    for (int i = 0; i < 1'000'000; ++i) {
        const uint64_t now = sgcl::detail::cpu_ticks();
        ASSERT_GE(now, last);
        last = now;
    }
    EXPECT_GT(sgcl::detail::ticks_per_microsecond(), 0.0);
    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t c0 = sgcl::detail::cpu_ticks();
    while (std::chrono::steady_clock::now() - t0 < 10ms) {
    }
    const uint64_t c1 = sgcl::detail::cpu_ticks();
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    const double measured = double(c1 - c0) / sgcl::detail::ticks_per_microsecond();
    EXPECT_NEAR(measured / us, 1.0, 0.05) << measured << " us of ticks against " << us << " us";
    EXPECT_EQ(sgcl::detail::ticks_of_microseconds(20), sgcl::detail::ticks_per_second() * 20 / 1'000'000);
}
