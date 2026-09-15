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
    weak_ptr weak_kept = gone.watch(kept, [&](tracked_ptr<Texture> t) { released.push_back(t->id); }).weak();
    weak_ptr<Texture> weak_dropped;
    off_frame([&] {
        tracked_ptr dropped = make_tracked<Texture>(2);
        weak_dropped = gone.watch(dropped, [&](tracked_ptr<Texture> t) { released.push_back(t->id); }).weak();
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
        weak = gone.watch(object, [](tracked_ptr<Texture>) {}).weak();
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

TEST(ExpiryQueue_Tests, TheEntryAndItsWeakPointer) {
    expiry_queue<Texture> gone;
    tracked_ptr object = make_tracked<Texture>(5);
    auto e = gone.watch(object, [](tracked_ptr<Texture>) {});
    ASSERT_TRUE(e);
    EXPECT_FALSE(e.expired());
    weak_ptr w = e.weak();                                  // the ordinary weak pointer, sharing the entry's cell
    ASSERT_TRUE(w.lock());
    EXPECT_EQ(w.lock()->id, 5);
    auto none = gone.watch(tracked_ptr<Texture>(), [](tracked_ptr<Texture>) {});   // a null object: no entry
    EXPECT_FALSE(none);
    EXPECT_TRUE(none.weak().expired());
    EXPECT_FALSE(none.cancel());
    EXPECT_EQ(gone.size(), 1u);
}

// A cancelled entry: the object is no longer kept, the function is not
// called, the entry leaves with the next drain; a second cancel, or one
// after the drain, withdraws nothing
TEST(ExpiryQueue_Tests, CancelWithdrawsTheEntry) {
    settle();
    const int before = Texture::alive.load();
    expiry_queue<Texture> gone;
    int calls = 0;
    expiry_queue<Texture>::entry kept_entry, dropped_entry;
    off_frame([&] {
        tracked_ptr a = make_tracked<Texture>(9);
        tracked_ptr b = make_tracked<Texture>(10);
        kept_entry = gone.watch(a, [&](tracked_ptr<Texture>) { ++calls; });
        dropped_entry = gone.watch(b, [&](tracked_ptr<Texture>) { ++calls; });
    });
    EXPECT_TRUE(dropped_entry.cancel());                    // released by hand: no finalizer
    EXPECT_FALSE(dropped_entry.cancel());                   // already
    settle();
    EXPECT_EQ(Texture::alive.load(), before + 1);           // b died with the cycle, a is kept for the queue
    EXPECT_TRUE(kept_entry.expired());
    EXPECT_EQ(gone.size(), 2u);                             // the cancelled entry counts until a drain
    EXPECT_EQ(gone.drain(), 1u);                            // a's function; b's entry dropped without one
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(gone.empty());
    EXPECT_FALSE(kept_entry.cancel());                      // drained: nothing to withdraw
    settle();
    EXPECT_EQ(Texture::alive.load(), before);
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
            w1 = gone.watch(a, [&](tracked_ptr<Texture>) { ++calls; }).weak();
            w2 = gone.watch(b, [&](tracked_ptr<Texture>) { ++calls; }).weak();
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
            w2 = gone.watch(c, [&](tracked_ptr<Texture>) { ++calls; }).weak();
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

TEST(ExpiryQueue_Tests, TheFunctionMayCaptureTrackedPointers) {
    settle();
    const int before = Texture::alive.load();
    struct Log {
        vector<int> ids;
    };
    expiry_queue<Texture> gone;
    tracked_ptr log = make_tracked<Log>();
    off_frame([&] {
        tracked_ptr object = make_tracked<Texture>(10);
        gone.watch(object, [log](tracked_ptr<Texture> t) { log->ids.push_back(t->id); });   // a closure with a pointer: in a managed object, followed
    });
    tracked_ptr<Log> only_the_closure = log;
    log = nullptr;
    only_the_closure = nullptr;                 // the closure alone keeps the log now
    settle();
    EXPECT_EQ(gone.drain(), 1u);
    EXPECT_EQ(Texture::alive.load(), before + 1);   // alive one last time, until the next cycle
    settle();
    EXPECT_EQ(Texture::alive.load(), before);
    tracked_ptr<Log> kept_by_queue;
    off_frame([&] {
        tracked_ptr another = make_tracked<Log>();
        tracked_ptr object = make_tracked<Texture>(11);
        gone.watch(object, [another](tracked_ptr<Texture> t) { another->ids.push_back(t->id); });
        kept_by_queue = another;
    });
    EXPECT_EQ(kept_by_queue->ids.size(), 0u);
    settle();
    EXPECT_EQ(gone.drain(), 1u);
    EXPECT_EQ(kept_by_queue->ids.size(), 1u);
    EXPECT_EQ(kept_by_queue->ids[0], 11);
}
