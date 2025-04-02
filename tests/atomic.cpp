//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

TEST(Atomic_Test, LoadStore) {
    sgcl::atomic<sgcl::tracked_ptr<int>> atomicPtr;
    sgcl::tracked_ptr sp = sgcl::make_tracked<int>(42);
    atomicPtr.store(sp);
    auto loaded = atomicPtr.load();
    ASSERT_TRUE(loaded);
    EXPECT_EQ(*loaded, 42);
}

TEST(Atomic_Test, AssignmentOperator) {
    sgcl::atomic<sgcl::tracked_ptr<int>> atomicPtr;
    sgcl::tracked_ptr sp = sgcl::make_tracked<int>(42);
    atomicPtr = sp;
    sgcl::tracked_ptr loaded = atomicPtr.load();
    ASSERT_TRUE(loaded);
    EXPECT_EQ(*loaded, 42);
}

TEST(Atomic_Test, CompareExchangeStrong) {
    sgcl::atomic<sgcl::tracked_ptr<int>> atomicPtr;
    sgcl::tracked_ptr sp1 = sgcl::make_tracked<int>(100);
    atomicPtr.store(sp1);

    sgcl::tracked_ptr expected = sp1;
    sgcl::tracked_ptr sp2 = sgcl::make_tracked<int>(200);
    bool exchanged = atomicPtr.compare_exchange_strong(expected, sp2);
    EXPECT_TRUE(exchanged);
    auto loaded = atomicPtr.load();
    EXPECT_EQ(*loaded, 200);

    expected = sp1;
    exchanged = atomicPtr.compare_exchange_strong(expected, sp1);
    EXPECT_FALSE(exchanged);
    EXPECT_EQ(expected, sp2);
}

TEST(Atomic_Test, CompareExchangeWeak) {
    sgcl::atomic<sgcl::tracked_ptr<int>> atomicPtr;
    sgcl::tracked_ptr sp1 = sgcl::make_tracked<int>(300);
    atomicPtr.store(sp1);

    sgcl::tracked_ptr expected = sp1;
    sgcl::tracked_ptr sp2 = sgcl::make_tracked<int>(400);
    while(!atomicPtr.compare_exchange_weak(expected, sp2));
    sgcl::tracked_ptr loaded = atomicPtr.load();
    EXPECT_EQ(*loaded, 400);
}

TEST(Atomic_Test, IsLockFree) {
    sgcl::atomic<sgcl::tracked_ptr<int>> atomicPtr;
    bool lockFree = atomicPtr.is_lock_free();
    EXPECT_TRUE(lockFree == true);
}

TEST(Atomic_Test, WaitNotifyOne) {
    sgcl::atomic<sgcl::tracked_ptr<int>> atomicPtr;
    sgcl::tracked_ptr sp1 = sgcl::make_tracked<int>(1);
    sgcl::tracked_ptr sp2 = sgcl::make_tracked<int>(2);
    atomicPtr.store(sp1);

    std::atomic<bool> threadWoke{false};

    std::thread t([&]{
        atomicPtr.wait(sp1);
        threadWoke.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(threadWoke.load());

    atomicPtr.store(sp2);
    atomicPtr.notify_one();

    t.join();
    EXPECT_TRUE(threadWoke.load());
}

TEST(Atomic_Test, WaitNotifyAll) {
    sgcl::atomic<sgcl::tracked_ptr<int>> atomicPtr;
    sgcl::tracked_ptr sp1 = sgcl::make_tracked<int>(1);
    sgcl::tracked_ptr sp2 = sgcl::make_tracked<int>(2);
    atomicPtr.store(sp1);

    std::atomic<int> wakeCount{0};

    auto waitFunc = [&]{
        atomicPtr.wait(sp1);
        wakeCount.fetch_add(1);
    };

    std::thread t1(waitFunc);
    std::thread t2(waitFunc);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(wakeCount.load(), 0);

    atomicPtr.store(sp2);
    atomicPtr.notify_all();

    t1.join();
    t2.join();

    EXPECT_EQ(wakeCount.load(), 2);
}
