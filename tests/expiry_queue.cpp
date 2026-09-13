//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// expiry_queue: an object found unreachable is kept for the queue, whose
// function gets it alive one last time.
#include "types.h"

#include <set>

namespace {
    struct Texture {
        explicit Texture(int id) : id(id) { ++alive; }
        ~Texture() { id = -1; --alive; }
        int id;
        inline static std::atomic<int> alive = {0};
    };

    void settle() {
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(ExpiryQueue_Tests, TheFunctionGetsTheObjectAliveOnceNothingElseReachesIt) {
    settle();
    const int before = Texture::alive.load();
    std::vector<int> released;
    expiry_queue<Texture> gone;
    tracked_ptr kept = make_tracked<Texture>(1);
    weak_ptr weak_kept = gone.watch(kept, [&](tracked_ptr<Texture> t) { released.push_back(t->id); });
    weak_ptr<Texture> weak_dropped;
    off_frame([&] {
        tracked_ptr dropped = make_tracked<Texture>(2);
        weak_dropped = gone.watch(dropped, [&](tracked_ptr<Texture> t) { released.push_back(t->id); });
    });
    EXPECT_EQ(gone.size(), 2u);
    EXPECT_EQ(gone.drain(), 0u);          // nothing found unreachable yet
    settle();
    EXPECT_EQ(Texture::alive.load(), before + 2);   // the dropped one is kept for the queue
    EXPECT_FALSE(weak_dropped.expired());           // alive until drained
    off_frame([&] {                                 // the locked pointers stay out of this frame
        ASSERT_TRUE(weak_dropped.lock());
        EXPECT_EQ(weak_dropped.lock()->id, 2);
    });
    EXPECT_EQ(gone.drain(), 1u);
    ASSERT_EQ(released.size(), 1u);
    EXPECT_EQ(released[0], 2);            // the object, with its data
    EXPECT_EQ(gone.size(), 1u);
    settle();
    EXPECT_EQ(Texture::alive.load(), before + 1);   // dead after the drain
    EXPECT_TRUE(weak_dropped.expired());
    EXPECT_EQ(gone.drain(), 0u);
    kept = nullptr;
    settle();
    EXPECT_EQ(gone.drain(), 1u);
    EXPECT_EQ(released.back(), 1);
    EXPECT_TRUE(gone.empty());
    settle();
    EXPECT_EQ(Texture::alive.load(), before);
}

TEST(ExpiryQueue_Tests, TheFunctionMayKeepTheObject) {
    settle();
    const int before = Texture::alive.load();
    expiry_queue<Texture> gone;
    tracked_ptr<Texture> revived;
    off_frame([&] {
        tracked_ptr object = make_tracked<Texture>(3);
        gone.watch(object, [&](tracked_ptr<Texture> t) { revived = t; });   // back to life
    });
    settle();
    int deaths = 0;
    off_frame([&] {                                 // every use of the pointer off this frame: no raw copy left behind
        EXPECT_EQ(gone.drain(), 1u);
        ASSERT_TRUE(revived);
        EXPECT_EQ(revived->id, 3);
    });
    settle();
    EXPECT_EQ(Texture::alive.load(), before + 1);   // back to life
    off_frame([&] {
        gone.watch(revived, [&](tracked_ptr<Texture>) { ++deaths; });   // watched again: the second death is reported too
        revived = nullptr;
    });
    settle();
    EXPECT_EQ(gone.drain(), 1u);
    EXPECT_EQ(deaths, 1);
    settle();
    EXPECT_EQ(Texture::alive.load(), before);
}

TEST(ExpiryQueue_Tests, AnObjectWaitsForTheDrain) {
    settle();
    const int before = Texture::alive.load();
    expiry_queue<Texture> gone;
    weak_ptr<Texture> weak;
    off_frame([&] {
        tracked_ptr object = make_tracked<Texture>(4);
        weak = gone.watch(object, [](tracked_ptr<Texture>) {});
    });
    for (int i = 0; i < 5; ++i) {
        settle();                         // many cycles, no drain: kept through all of them
        EXPECT_EQ(Texture::alive.load(), before + 1);
        ASSERT_TRUE(weak.lock());
    }
    gone.drain();
    settle();
    EXPECT_EQ(Texture::alive.load(), before);
}

TEST(ExpiryQueue_Tests, TheWeakPointerIsTheOrdinaryOne) {
    expiry_queue<Texture> gone;
    tracked_ptr object = make_tracked<Texture>(5);
    weak_ptr w = gone.watch(object, [](tracked_ptr<Texture>) {});
    ASSERT_TRUE(w.lock());
    EXPECT_EQ(w.lock()->id, 5);
    weak_ptr none = gone.watch(tracked_ptr<Texture>(), [](tracked_ptr<Texture>) {});   // a null object: no entry
    EXPECT_TRUE(none.expired());
    EXPECT_EQ(gone.size(), 1u);
}

TEST(ExpiryQueue_Tests, DrainsByItselfEverySoManyWatches) {
    settle();
    const int before = Texture::alive.load();
    std::set<int> released;
    expiry_queue<Texture> gone;
    off_frame([&] {
        for (int round = 0; round < 5; ++round) {
            for (int i = 0; i < 100; ++i) {
                tracked_ptr texture = make_tracked<Texture>(round * 100 + i);
                gone.watch(texture, [&released](tracked_ptr<Texture> t) { released.insert(t->id); });
            }
            settle();
        }
    });
    // the queue drained on its own along the way: the entries did not pile up
    EXPECT_LT(gone.size(), 300u);
    EXPECT_GT(released.size(), 200u);
    gone.drain();
    EXPECT_EQ(released.size(), 500u);
    EXPECT_TRUE(gone.empty());
    settle();
    EXPECT_EQ(Texture::alive.load(), before);
}

TEST(ExpiryQueue_Tests, ClearAndDestructionLetTheObjectsGo) {
    settle();
    const int before = Texture::alive.load();
    int calls = 0;
    weak_ptr<Texture> w1, w2;
    {
        expiry_queue<Texture> gone;
        off_frame([&] {
            tracked_ptr a = make_tracked<Texture>(6);
            tracked_ptr b = make_tracked<Texture>(7);
            w1 = gone.watch(a, [&](tracked_ptr<Texture>) { ++calls; });
            w2 = gone.watch(b, [&](tracked_ptr<Texture>) { ++calls; });
        });
        settle();
        EXPECT_EQ(Texture::alive.load(), before + 2);
        gone.clear();                     // no call, no longer kept
        EXPECT_TRUE(gone.empty());
        settle();
        EXPECT_EQ(Texture::alive.load(), before);
        EXPECT_TRUE(w1.expired());
        off_frame([&] {
            tracked_ptr c = make_tracked<Texture>(8);
            w2 = gone.watch(c, [&](tracked_ptr<Texture>) { ++calls; });
        });
        settle();
        EXPECT_EQ(Texture::alive.load(), before + 1);
    }                                     // the queue destroyed: its entry released
    settle();
    EXPECT_EQ(Texture::alive.load(), before);
    EXPECT_TRUE(w2.expired());
    EXPECT_EQ(calls, 0);
}

TEST(ExpiryQueue_Tests, InsideAManagedObject) {
    settle();
    struct Owner {
        expiry_queue<Texture> gone;
        int released = 0;
    };
    tracked_ptr owner = make_tracked<Owner>();
    off_frame([&] {
        tracked_ptr object = make_tracked<Texture>(9);
        owner->gone.watch(object, [o = owner.get()](tracked_ptr<Texture> t) { o->released = t->id; });   // a raw pointer: the owner outlives its queue
    });
    settle();
    EXPECT_EQ(owner->gone.drain(), 1u);
    EXPECT_EQ(owner->released, 9);
}
