//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    // The won races' timers swept, shard by shard: each shard below
    // max(66, 2 x what it held before + 3). A shard sweeps its cancelled
    // once they outnumber its live timers past 64 (timer.h: _push), and its
    // live ones are what it held before at most (other tests' deadlines an
    // hour away, which a run of the whole program leaves behind: the fixed
    // bound of 64 failed there, at 65 to 100) and the one race in flight
    void expect_swept(const std::vector<size_t>& before) {
        auto after = sgcl::async::detail::timers_instance().shard_sizes();
        for (size_t i = 0; i < after.size(); ++i) {
            EXPECT_LE(after[i], std::max<size_t>(66, 2 * before[i] + 3)) << "shard " << i << ", before " << before[i];
        }
    }
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;
}

TEST(Timer_Test, SleepSuspendsATask) {
    auto t0 = Clock::now();
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_await sgcl::async::sleep(30ms);
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    auto elapsed = Clock::now() - t0;
    EXPECT_GE(elapsed, 30ms);
    EXPECT_LT(elapsed, 2s);
    // a sleep of nothing does not suspend
    auto u = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        co_await sgcl::async::sleep(0ms);
        co_return 2;
    }());
    EXPECT_EQ(u.wait(), 2);
    sgcl::async::scheduler::stop();
}

TEST(Timer_Test, ManySleepersInOrder) {
    std::atomic<int> order = {0};
    std::vector<sgcl::async::task<int>> tasks;
    for (int i = 4; i >= 0; --i) {
        tasks.push_back(sgcl::async::spawn([](int i, std::atomic<int>& order) -> sgcl::async::task<int> {
            co_await sgcl::async::sleep(std::chrono::milliseconds(10 + 10 * i));
            co_return order++;
        }(i, order)));
    }
    for (int i = 4, k = 0; i >= 0; --i, ++k) {
        EXPECT_EQ(tasks[k].wait(), i);   // the shortest sleep woke first
    }
    sgcl::async::scheduler::stop();
}

TEST(Timer_Test, AfterIsOneSignalThenClosed) {
    auto t0 = Clock::now();
    sgcl::async::event ev = sgcl::async::after(20ms);
    ev.wait();
    EXPECT_TRUE(ev.is_set());
    EXPECT_GE(Clock::now() - t0, 20ms);
    ev.wait();                        // set: a wait after it does not wait
    EXPECT_TRUE(ev.is_set());
    sgcl::async::scheduler::stop();
}

TEST(Timer_Test, TimeoutInASelect) {
    sgcl::async::channel<int> never;
    bool timed = false;
    auto t0 = Clock::now();
    EXPECT_EQ(sgcl::async::select(never.on_receive([](int) {}), sgcl::async::timeout(20ms, [&] { timed = true; })).wait(), 1u);
    EXPECT_TRUE(timed);
    EXPECT_GE(Clock::now() - t0, 20ms);
    // the data wins when it comes first
    sgcl::async::channel<int> data(1);
    data.send(5).wait();
    int got = 0;
    EXPECT_EQ(sgcl::async::select(data.on_receive([&](int v) { got = v; }), sgcl::async::timeout(30ms, [] {})).wait(), 0u);
    EXPECT_EQ(got, 5);
    // in a task
    auto t = sgcl::async::spawn([](sgcl::async::channel<int>& never) -> sgcl::async::task<size_t> {
        co_return co_await sgcl::async::select(never.on_receive([](int) {}), sgcl::async::timeout(10ms, [] {}));
    }(never));
    EXPECT_EQ(t.wait(), 1u);
    std::this_thread::sleep_for(40ms);   // the timer of the timeout that lost fires and lets go of its channel
    sgcl::async::scheduler::stop();
}

TEST(Timer_Test, TickUntilClosed) {
    auto t0 = Clock::now();
    auto tk = sgcl::async::tick(5ms);
    int n = 0;
    while (n < 5) {
        if (tk.receive().wait()) {
            ++n;
        }
    }
    EXPECT_GE(Clock::now() - t0, 25ms);
    tk.close();                       // the timer sees the close and lets go
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(tk.receive().wait());
    sgcl::async::scheduler::stop();
    sgcl::collector::force_collect(true);
}

TEST(Timer_Test, TheTimersStopWithTheScheduler) {
    sgcl::async::event ev = sgcl::async::after(10ms);
    ev.wait();
    EXPECT_TRUE(ev.is_set());
    sgcl::async::scheduler::stop();           // the timer thread joined
    auto again = sgcl::async::after(10ms);    // and started again by the next timer
    again.wait();
    EXPECT_TRUE(again.is_set());
    sgcl::async::scheduler::stop();
}

// A timeout case gone before its time cancels its timer, and the heap is
// swept of the cancelled: a select with a long timeout served by its
// channel in a loop keeps a bounded heap, not a timer per iteration
// until the deadline (0.75 KB each, measured, for an hour)
TEST(Timer_Test, ATimeoutCaseGoneCancelsItsTimer) {
    const auto before = sgcl::async::detail::timers_instance().shard_sizes();
    sgcl::async::channel<int> ch(1);
    auto t = sgcl::async::spawn([](sgcl::async::channel<int>& ch) -> sgcl::async::task<> {
        for (int i = 0; i < 5000; ++i) {
            co_await sgcl::async::select(ch.on_receive([](int) {}), sgcl::async::timeout(1h, [] {}));
        }
    }(ch));
    for (int i = 0; i < 5000; ++i) {
        ch.send(i).wait();
    }
    t.wait();
    expect_swept(before);
    sgcl::async::scheduler::stop();
}

// The timers live in shards, one per worker and one for every other thread,
// and one thread fires them (timer.h: Timers). An add from a thread that is
// no worker, made exactly between the timer thread's pass over the heaps and
// its sleep (the test hook), with its point earlier than anything the pass
// saw: the thread must not sleep past it. Then a thousand rounds of the same
// without the hook, from a plain thread, while the timer thread goes to
// sleep and wakes on the others; under the thread sanitizer as well.
TEST(Timer_Test, AnAddBetweenThePassAndTheSleepWakesTheThread) {
    auto far = after(1h);                                        // the thread asleep towards an hour
    std::this_thread::sleep_for(5ms);
    static std::atomic<int> fired = {0};
    static std::atomic<bool> woke = {false};
    static std::atomic<bool> armed = {false};
    fired = 0;
    woke = false;
    sgcl::async::detail::timers_test_hook.store([] {
        if (woke.load() && armed.exchange(false)) {              // the pass that fired the wake below: what it left is the hour
            std::thread([] {                                     // a thread that is no worker: the shared shard
                sgcl::async::detail::add_timer(sgcl::clock::now() + 2ms, tracked_ptr<void>(), [](void*) { fired.fetch_add(1); });
            }).join();
        }
    });
    armed = true;
    sgcl::async::detail::add_timer(sgcl::clock::now() + 1ms, tracked_ptr<void>(), [](void*) { woke = true; });
    auto end = Clock::now() + 2s;
    while (fired.load() == 0 && Clock::now() < end) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_FALSE(armed.load());                                  // the hook did add, after the pass
    EXPECT_EQ(fired.load(), 1);                                  // not an hour later
    sgcl::async::detail::timers_test_hook.store(nullptr);
    fired = 0;
    const int rounds = 1000;
    for (int i = 0; i < rounds; ++i) {
        std::thread([] {
            sgcl::async::detail::add_timer(sgcl::clock::now() + std::chrono::microseconds(50), tracked_ptr<void>(), [](void*) { fired.fetch_add(1); });
        }).join();
        if (i % 16 == 0) {
            std::this_thread::sleep_for(100us);                  // the timer thread asleep again, now and then
        }
    }
    end = Clock::now() + 5s;
    while (fired.load() < rounds && Clock::now() < end) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(fired.load(), rounds);
    far.set();
    sgcl::async::scheduler::stop();
}

// A timer from every kind of thread fires: a worker's (its own shard), a
// thread blocked in spawn_blocking's pool, the main thread (the shared
// shard); and a timer added on one thread and cancelled on another does
// not fire
TEST(Timer_Test, TimersFromEveryKindOfThreadAndACancelFromAnother) {
    static std::atomic<int> fired = {0};
    fired = 0;
    auto bump = [](void*) { fired.fetch_add(1); };
    auto from_task = spawn([](void (*f)(void*)) -> task<> {
        sgcl::async::detail::add_timer(sgcl::clock::now() + 5ms, tracked_ptr<void>(), f);
        co_return;
    }(bump));
    from_task.wait();
    spawn_blocking([bump] { sgcl::async::detail::add_timer(sgcl::clock::now() + 5ms, tracked_ptr<void>(), bump); }).wait();
    sgcl::async::detail::add_timer(sgcl::clock::now() + 5ms, tracked_ptr<void>(), bump);
    auto end = Clock::now() + 2s;
    while (fired.load() < 3 && Clock::now() < end) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(fired.load(), 3);
    // added on a worker, cancelled from a plain thread
    fired = 0;
    auto armed = spawn([](void (*f)(void*)) -> task<tracked_ptr<sgcl::async::detail::Timer>> {
        co_return sgcl::async::detail::add_timer(sgcl::clock::now() + 20ms, tracked_ptr<void>(), f);
    }(bump));
    tracked_ptr<sgcl::async::detail::Timer> timer = armed.wait();
    std::thread([&] {
        timer->cancelled.store(true, std::memory_order_release);
        sgcl::async::detail::timer_cancelled(*timer);
    }).join();
    std::this_thread::sleep_for(60ms);
    EXPECT_EQ(fired.load(), 0);
    sgcl::async::scheduler::stop();
}

// The timers' thread asleep after a pass holds nothing of what it fired:
// the dead frames of the pass are cleared before the sleep, so the last
// timer fired and what it kept are the collector's at once — not at the
// thread's next pass, which with no other timer due is never (the words
// of a thread of the library before it parks, as the blocking pool's)
namespace {
    struct Kept {
        Kept() { ++alive; }
        ~Kept() { --alive; }
        inline static std::atomic<int> alive = {0};
    };

    // What a timer's call does on the timer's thread, as deep as a stop
    // that closes a channel and wakes its waiters goes: the pointer it was
    // given left in frames a kilobyte and more below the pass, where the
    // thread's own wait does not reach
    SGCL_NOINLINE void deep(void* p, int n) {
        volatile uintptr_t words[16];
        for (auto& w : words) {
            w = (uintptr_t)p;
        }
        if (n > 0) {
            deep(p, n - 1);
        }
        (void)words[0];
    }
}

TEST(Timer_Test, TheThreadAsleepHoldsNothingOfTheLastTimer) {
    static std::atomic<bool> fired = {false};
    fired = false;
    off_frame([] {
        sgcl::tracked_ptr kept = sgcl::make_tracked<Kept>();
        (void)sgcl::async::detail::add_timer(sgcl::clock::now() + 5ms, kept, [](void* p) {
            deep(p, 6);
            fired = true;
        });
    });
    for (int i = 0; i < 5000 && !fired.load(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(fired.load());
    bool gone = false;
    for (int i = 0; i < 100 && !gone; ++i) {   // the thread reaches its sleep a moment after the call
        std::this_thread::sleep_for(5ms);
        collector::clear_stack();
        collector::force_collect(true);
        gone = Kept::alive.load() == 0;
    }
    EXPECT_TRUE(gone);   // no other timer, no stop(): what the timer kept gone with it
}
