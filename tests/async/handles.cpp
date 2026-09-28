//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The primitives of async as handles: one word, a tracked_ptr to a state
// the copies share, made by the constructor. A task takes them by value;
// a state nobody holds is garbage, a task waiting on it with it; a global
// holds one through rooted<H>.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <type_traits>

namespace {
    using namespace std::chrono_literals;

    template<class H>
    constexpr bool IsHandle = sizeof(H) == sizeof(sgcl::tracked_ptr<void>) && std::is_default_constructible_v<H> && std::is_copy_constructible_v<H> && std::is_copy_assignable_v<H>;

    static_assert(IsHandle<sgcl::async::channel<int>>);
    static_assert(IsHandle<sgcl::async::channel<void>>);
    static_assert(IsHandle<sgcl::async::event>);
    static_assert(IsHandle<sgcl::async::mutex>);
    static_assert(IsHandle<sgcl::async::wait_group>);
    static_assert(IsHandle<sgcl::async::promise<int>>);
    static_assert(IsHandle<sgcl::async::promise<>>);
    static_assert(sizeof(sgcl::async::mutex::guard) == sizeof(sgcl::tracked_ptr<void>));   // one word, as the pointer it replaced

    // Each takes part in the atomics by its word (core/detail/handle_word.h)
    static_assert(sgcl::req::handle<sgcl::async::channel<int>>);
    static_assert(sgcl::req::handle<sgcl::async::channel<void>>);
    static_assert(sgcl::req::handle<sgcl::async::event>);
    static_assert(sgcl::req::handle<sgcl::async::mutex>);
    static_assert(sgcl::req::handle<sgcl::async::wait_group>);
    static_assert(sgcl::req::handle<sgcl::async::promise<int>>);
    static_assert(sgcl::req::handle<sgcl::async::promise<>>);

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // The worker of the example in the docs: the channels by value, from a
    // caller that returns before the task ends
    sgcl::async::task<> doubler(sgcl::async::channel<int> jobs, sgcl::async::channel<int> results) {
        for (int v : jobs) {
            co_await results.send(2 * v);
        }
        results.close();
    }

    SGCL_NOINLINE sgcl::async::channel<int> start_doubler(sgcl::async::channel<int> jobs) {
        sgcl::async::channel<int> results(4);
        sgcl::async::go(doubler(jobs, results));
        return results;                                       // the caller's frame gone, the task still running
    }

    // What a frame keeps: counted, so that a frame collected shows
    struct Probe {
        Probe() { ++alive; }
        ~Probe() { --alive; }
        inline static std::atomic<int> alive = {0};
    };

    sgcl::async::task<> wait_forever(sgcl::async::channel<int> ch, std::atomic<bool>* waiting) {
        sgcl::tracked_ptr<Probe> held = sgcl::make_tracked<Probe>();
        waiting->store(true);
        (void)co_await ch.receive();
        held = nullptr;
    }

    // A global: static memory, which the collector does not trace
    std::optional<rooted<sgcl::async::mutex>> global_lock;
}

TEST(AsyncHandles_Test, ACopySharesTheChannel) {
    sgcl::async::channel<int> a(2);
    sgcl::async::channel<int> b = a;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == sgcl::async::channel<int>(2));
    EXPECT_TRUE(a.try_send(7));
    EXPECT_EQ(b.size(), 1u);
    EXPECT_EQ(b.try_receive(), sgcl::optional<int>(7));
    b.close();
    EXPECT_TRUE(a.closed());
}

TEST(AsyncHandles_Test, ATaskTakesTheChannelsByValue) {
    sgcl::async::channel<int> jobs(4);
    sgcl::async::channel<int> results = start_doubler(jobs);
    for (int i = 1; i <= 3; ++i) {
        EXPECT_TRUE(jobs.send(i).wait());
    }
    jobs.close();
    int sum = 0;
    for (int v : results) {
        sum += v;
    }
    EXPECT_EQ(sum, 12);
    sgcl::async::scheduler::stop();
}

TEST(AsyncHandles_Test, ACopySharesTheOthers) {
    sgcl::async::event e;
    sgcl::async::event f = e;
    f.set();
    EXPECT_TRUE(e.is_set());
    EXPECT_TRUE(e == f);

    sgcl::async::mutex m;
    sgcl::async::mutex n = m;
    EXPECT_TRUE(n.try_lock());
    EXPECT_FALSE(m.try_lock());
    m.unlock();
    {
        auto guard = n.scoped_lock().wait();
        EXPECT_TRUE(guard.owner() == m);
        EXPECT_FALSE(m.try_lock());
    }
    EXPECT_TRUE(m.try_lock());
    m.unlock();

    sgcl::async::wait_group g;
    sgcl::async::wait_group h = g;
    g.add(2);
    h.done();
    EXPECT_EQ(g.count(), 1);
    h.done();
    g.wait();

    sgcl::async::promise<int> p;
    sgcl::async::promise<int> q = p;
    std::thread setter([&] { q.set_value(42); });
    EXPECT_EQ(p.wait(), 42);
    setter.join();
    EXPECT_TRUE(q.done());
}

// A task that waits on a channel nobody else holds: the channel, the
// waiter and the frame are one garbage cycle, collected. A handle holding
// a root (a root_ptr in the word) would keep the three for good.
TEST(AsyncHandles_Test, ATaskWaitingOnAnUnheldChannelIsCollected) {
    settle();
    size_t channels = live_objects_named("12ChannelStateIiE");
    std::atomic<bool> waiting = false;
    off_frame([&] {
        sgcl::async::go(wait_forever(sgcl::async::channel<int>(), &waiting));
    });
    for (int i = 0; i < 2000 && !waiting.load(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(waiting.load());
    std::this_thread::sleep_for(20ms);                        // suspended on the receive
    for (int i = 0; i < 16; ++i) {                            // the workers' stacks written over by other tasks
        sgcl::async::spawn([]() -> sgcl::async::task<int> { co_return 1; }()).wait();
    }
    settle();
    EXPECT_EQ(Probe::alive.load(), 0);                        // the frame collected: its local with it
    EXPECT_EQ(live_objects_named("12ChannelStateIiE"), channels);
    sgcl::async::scheduler::stop();
}

// A guard in a task's frame through a wait and a collection: the frame is
// scanned conservatively, so the guard's word keeps the mutex's state after
// the last handle is gone, and the guard's end unlocks it
TEST(AsyncHandles_Test, AGuardInATaskFrameThroughACollection) {
    settle();
    size_t states = live_objects_named("10MutexState");
    sgcl::async::event proceed;
    std::atomic<bool> locked = false;
    auto t = sgcl::async::spawn([](sgcl::async::event proceed, std::atomic<bool>* locked) -> sgcl::async::task<bool> {
        std::optional<sgcl::async::mutex::guard> held;
        {
            sgcl::async::mutex m;                             // the only handle, gone at the end of the block
            held.emplace(co_await m.scoped_lock());
        }
        locked->store(true);
        co_await proceed;                                     // suspended holding the guard alone
        sgcl::async::mutex owner = held->owner();
        bool was_locked = !owner.try_lock();
        held.reset();                                         // the guard's end: unlocked
        bool unlocked = owner.try_lock();
        owner.unlock();
        co_return was_locked && unlocked;
    }(proceed, &locked));
    for (int i = 0; i < 2000 && !locked.load(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(locked.load());
    std::this_thread::sleep_for(10ms);
    settle();
    EXPECT_EQ(live_objects_named("10MutexState"), states + 1);   // kept by the guard's word in the frame
    proceed.set();
    EXPECT_TRUE(t.wait());
    sgcl::async::scheduler::stop();
}

// A global holds a primitive through rooted<H>: one state, alive through
// collections, the same state as a copy of the handle
TEST(AsyncHandles_Test, AGlobalRootedMutex) {
    settle();
    size_t states = live_objects_named("10MutexState");
    off_frame([] {
        global_lock.emplace(std::in_place);
    });
    settle();
    EXPECT_EQ(live_objects_named("10MutexState"), states + 1);
    off_frame([] {
        sgcl::async::mutex copy = **global_lock;
        EXPECT_TRUE(copy == **global_lock);
        (*global_lock)->lock();
        EXPECT_FALSE(copy.try_lock());
        copy.unlock();
    });
    global_lock.reset();
    settle();
    EXPECT_EQ(live_objects_named("10MutexState"), states);
}

// An atomic of a handle: load, store and exchange by identity, the object
// shared, nothing copied
TEST(AsyncHandles_Test, AnAtomicHandleByIdentity) {
    sgcl::async::channel<int> one(1), two(2);
    sgcl::atomic<sgcl::async::channel<int>> current;
    current.store(one);
    EXPECT_TRUE(current.load() == one);
    EXPECT_TRUE(current.exchange(two) == one);
    EXPECT_TRUE(current.load() == two);
    EXPECT_TRUE(current.load().try_send(5));
    EXPECT_EQ(two.try_receive(), sgcl::optional<int>(5));

    sgcl::async::mutex m;
    sgcl::atomic<sgcl::async::mutex> lock;
    lock.store(m);
    EXPECT_TRUE(lock.load().try_lock());
    EXPECT_FALSE(m.try_lock());
    m.unlock();
}
