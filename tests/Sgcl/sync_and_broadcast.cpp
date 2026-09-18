//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// SharedMutex, ConditionVariable and Broadcast: the facades over
// sgcl::shared_mutex, sgcl::condition_variable and sgcl::broadcast
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Item {
        explicit Item(int v)
        : value(v) {
        }

        int value;
    };
}

TEST(Sgcl_SyncAndBroadcast_Tests, SharedMutexReadersAndAWriter) {
    using namespace Sgcl;
    SharedMutex m;
    std::atomic<int> readers = {0}, peak = {0}, violations = {0};
    Event all_in;
    List<Task<>> tasks;
    for (int i : Range(8)) {
        (void)i;
        tasks.Add(Spawn([](SharedMutex& m, std::atomic<int>& readers, std::atomic<int>& peak, Event& all_in) -> Task<> {
            auto lock = co_await m.AsyncScopedLockShared();
            int now = ++readers;
            int p = peak.load();
            while (now > p && !peak.compare_exchange_weak(p, now)) {
            }
            co_await all_in.AsyncWait();
            --readers;
        }(m, readers, peak, all_in)));
    }
    while (readers.load() < 8) {
        ThisThread::SleepFor(1ms);
    }
    EXPECT_FALSE(m.TryLock());
    all_in.Set();
    for (auto& t : tasks) {
        t.Join();
    }
    EXPECT_EQ(peak, 8);
    // the writer, from a task and from a thread; readers excluded
    auto writer = Spawn([](SharedMutex& m, std::atomic<int>& readers, std::atomic<int>& violations) -> Task<> {
        auto lock = co_await m.AsyncScopedLock();
        if (readers.load() != 0 || m.TryLockShared()) {
            ++violations;
        }
        co_await Sleep(5ms);
    }(m, readers, violations));
    writer.Join();
    {
        std::lock_guard lock(m);
        EXPECT_FALSE(m.TryLockShared());
    }
    {
        std::shared_lock lock(m);
        EXPECT_FALSE(m.TryLock());
        EXPECT_TRUE(m.TryLockShared());
        m.UnlockShared();
    }
    m.LockShared();
    m.UnlockShared();
    m.Lock();
    m.Unlock();
    EXPECT_EQ(violations, 0);
    Scheduler::Stop();
}

TEST(Sgcl_SyncAndBroadcast_Tests, ConditionVariableWithAPredicate) {
    using namespace Sgcl;
    Mutex m;
    ConditionVariable cv;
    int ready = 0;                         // guarded by m
    auto waiter = Spawn([](Mutex& m, ConditionVariable& cv, int& ready) -> Task<int> {
        auto guard = co_await m.AsyncScopedLock();
        co_await cv.AsyncWait(guard, [&] { return ready == 2; });
        co_return ready;
    }(m, cv, ready));
    std::thread th([&] {
        std::unique_lock lock(m);
        cv.Wait(lock, [&] { return ready >= 1; });
        ready = 2;
        lock.unlock();
        cv.NotifyAll();
    });
    ThisThread::SleepFor(10ms);
    EXPECT_FALSE(waiter.IsDone());
    {
        std::lock_guard lock(m);
        ready = 1;
    }
    cv.NotifyAll();
    th.join();
    EXPECT_EQ(waiter.Join(), 2);
    cv.NotifyOne();                        // nobody waits
    Scheduler::Stop();
}

TEST(Sgcl_SyncAndBroadcast_Tests, BroadcastToEverySubscriber) {
    using namespace Sgcl;
    Broadcast<Ptr<Item>> bus(16);
    EXPECT_EQ(bus.Capacity(), 16u);
    auto sum = [](Broadcast<Ptr<Item>>::Subscription s) -> Task<int> {
        int sum = 0;
        while (auto v = co_await s.AsyncReceive()) {
            sum += (*v)->value;
        }
        co_return sum;
    };
    List<Task<int>> listeners;
    for (int i : Range(3)) {
        (void)i;
        listeners.Add(Spawn(sum(bus.Subscribe())));
    }
    auto mine = bus.Subscribe();
    EXPECT_EQ(bus.SubscriberCount(), 4u);
    for (int i : Range(1, 11)) {
        EXPECT_TRUE(bus.Send(Make<Item>(i)));
    }
    EXPECT_EQ((*mine.TryReceive())->value, 1);
    EXPECT_EQ((*mine.Receive())->value, 2);
    EXPECT_EQ(Select(mine.OnReceive([](Ptr<Item> v) { EXPECT_EQ(v->value, 3); })), 0u);
    bus.Close();
    EXPECT_FALSE(bus.Send(Make<Item>(11)));
    EXPECT_TRUE(bus.IsClosed());
    for (auto& t : listeners) {
        EXPECT_EQ(t.Join(), 55);
    }
    int rest = 0;
    while (auto v = mine.Receive()) {
        rest += (*v)->value;
    }
    EXPECT_EQ(rest, 4 + 5 + 6 + 7 + 8 + 9 + 10);
    EXPECT_EQ(mine.Lagged(), 0u);
    EXPECT_TRUE(mine.IsClosed());
    Scheduler::Stop();
}

TEST(Sgcl_SyncAndBroadcast_Tests, BroadcastLag) {
    using namespace Sgcl;
    Broadcast<int> bus(4);
    auto s = bus.Subscribe();
    for (int i : Range(10)) {
        bus.Send(i);
    }
    EXPECT_EQ(*s.Receive(), 6);
    EXPECT_EQ(s.Lagged(), 6u);
    EXPECT_EQ(*s.Receive(), 7);
    EXPECT_EQ(s.Lagged(), 0u);
    Broadcast<int>::Subscription moved = std::move(s);
    EXPECT_FALSE(s);
    EXPECT_TRUE(moved);
    EXPECT_EQ(*moved.TryReceive(), 8);
}
