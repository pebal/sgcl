//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

TEST(AtomicRef_Test, LoadStore) {
    sgcl::tracked_ptr<int> atomicPtr;
    sgcl::tracked_ptr sp = sgcl::make_tracked<int>(42);
    sgcl::atomic_ref(atomicPtr).store(sp);
    auto loaded = sgcl::atomic_ref(atomicPtr).load();
    ASSERT_TRUE(loaded);
    EXPECT_EQ(*loaded, 42);
}

TEST(AtomicRef_Test, AssignmentOperator) {
    sgcl::tracked_ptr<int> atomicPtr;
    sgcl::tracked_ptr sp = sgcl::make_tracked<int>(42);
    (sgcl::atomic_ref(atomicPtr)) = sp;
    sgcl::tracked_ptr loaded = sgcl::atomic_ref(atomicPtr).load();
    ASSERT_TRUE(loaded);
    EXPECT_EQ(*loaded, 42);
}

TEST(AtomicRef_Test, CompareExchangeStrong) {
    sgcl::tracked_ptr<int> atomicPtr;
    sgcl::tracked_ptr sp1 = sgcl::make_tracked<int>(100);
    sgcl::atomic_ref(atomicPtr).store(sp1);

    sgcl::tracked_ptr expected = sp1;
    sgcl::tracked_ptr sp2 = sgcl::make_tracked<int>(200);
    bool exchanged = sgcl::atomic_ref(atomicPtr).compare_exchange_strong(expected, sp2);
    EXPECT_TRUE(exchanged);
    auto loaded = sgcl::atomic_ref(atomicPtr).load();
    EXPECT_EQ(*loaded, 200);

    expected = sp1;
    exchanged = sgcl::atomic_ref(atomicPtr).compare_exchange_strong(expected, sp1);
    EXPECT_FALSE(exchanged);
    EXPECT_EQ(expected, sp2);
}

TEST(AtomicRef_Test, CompareExchangeWeak) {
    sgcl::tracked_ptr<int> atomicPtr;
    sgcl::tracked_ptr sp1 = sgcl::make_tracked<int>(300);
    sgcl::atomic_ref(atomicPtr).store(sp1);

    sgcl::tracked_ptr expected = sp1;
    sgcl::tracked_ptr sp2 = sgcl::make_tracked<int>(400);
    while(!sgcl::atomic_ref(atomicPtr).compare_exchange_weak(expected, sp2));
    sgcl::tracked_ptr loaded = sgcl::atomic_ref(atomicPtr).load();
    EXPECT_EQ(*loaded, 400);
}

TEST(AtomicRef_Test, IsLockFree) {
    sgcl::tracked_ptr<int> atomicPtr;
    bool lockFree = sgcl::atomic_ref(atomicPtr).is_lock_free();
    EXPECT_TRUE(lockFree == true);
}

TEST(AtomicRef_Test, WaitNotifyOne) {
    sgcl::tracked_ptr<int> atomicPtr;
    sgcl::tracked_ptr sp1 = sgcl::make_tracked<int>(1);
    sgcl::tracked_ptr sp2 = sgcl::make_tracked<int>(2);
    sgcl::atomic_ref(atomicPtr).store(sp1);

    std::atomic<bool> threadWoke{false};

    std::thread t([&]{
        sgcl::atomic_ref(atomicPtr).wait(sp1);
        threadWoke.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(threadWoke.load());

    sgcl::atomic_ref(atomicPtr).store(sp2);
    sgcl::atomic_ref(atomicPtr).notify_one();

    t.join();
    EXPECT_TRUE(threadWoke.load());
}

TEST(AtomicRef_Test, WaitNotifyAll) {
    sgcl::tracked_ptr<int> atomicPtr;
    sgcl::tracked_ptr sp1 = sgcl::make_tracked<int>(1);
    sgcl::tracked_ptr sp2 = sgcl::make_tracked<int>(2);
    sgcl::atomic_ref(atomicPtr).store(sp1);

    std::atomic<int> wakeCount{0};

    auto waitFunc = [&]{
        sgcl::atomic_ref(atomicPtr).wait(sp1);
        wakeCount.fetch_add(1);
    };

    std::thread t1(waitFunc);
    std::thread t2(waitFunc);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(wakeCount.load(), 0);

    sgcl::atomic_ref(atomicPtr).store(sp2);
    sgcl::atomic_ref(atomicPtr).notify_all();

    t1.join();
    t2.join();

    EXPECT_EQ(wakeCount.load(), 2);
}
