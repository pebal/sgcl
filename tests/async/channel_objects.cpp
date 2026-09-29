//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async: the managed objects a channel is made of (channel.h), counted
// among the live objects: a thousand of each kind held in a managed vector,
// the vector's buffer the one object beside them. A ring of MinSlots small
// slots lies in the state (a rendezvous, a signal's channel, any capacity
// up to 8); a larger ring, or a ring of large elements, is an array of its
// own. The lists of waiters keep their first nodes in the state when the
// state was made linked (a channel, an event, a mutex, a promise, a stop
// source, a timer, the reactor's waits): one object for a small channel.
// A state off the managed heap (a semaphore's, a field of an object on a
// stack) takes its lists' first nodes as before.
#include "tests/types.h"
#include "sgcl/async/async.h"
#include "sgcl/core/vector.h"

#include <array>
#include <vector>

using namespace sgcl;

namespace {
    // live objects per handle of the kind make() makes, 1000 of them
    template<class F>
    double objects_per(F make) {
        const size_t n = 1000;
        collector::force_collect(true);
        const size_t before = collector::get_live_object_count();
        sgcl::vector<decltype(make())> held;
        off_frame([&] {
            held.reserve(n);
            for (size_t i = 0; i < n; ++i) {
                held.push_back(make());
            }
        });
        const size_t after = collector::get_live_object_count();
        const double per = double(after - before - 1) / double(n);   // the vector's buffer: one
        held = {};
        return per;
    }
}

TEST(ChannelObjects_Tests, ASmallChannelIsOneObject) {
    EXPECT_DOUBLE_EQ(objects_per([] { return async::channel<void>(1); }), 1.0);
    EXPECT_DOUBLE_EQ(objects_per([] { return async::channel<void>(); }), 1.0);   // a rendezvous
    EXPECT_DOUBLE_EQ(objects_per([] { return async::channel<int>(8); }), 1.0);
    // a ring past MinSlots, or of large elements: an array of its own
    EXPECT_DOUBLE_EQ(objects_per([] { return async::channel<int>(9); }), 2.0);
    EXPECT_DOUBLE_EQ(objects_per([] { return async::channel<std::array<char, 100>>(1); }), 2.0);
}

TEST(ChannelObjects_Tests, TheHandlesOverAChannel) {
    EXPECT_DOUBLE_EQ(objects_per([] { return async::event(); }), 1.0);
    EXPECT_DOUBLE_EQ(objects_per([] { return async::mutex(); }), 1.0);
    EXPECT_DOUBLE_EQ(objects_per([] { return async::promise<int>(); }), 1.0);
    EXPECT_DOUBLE_EQ(objects_per([] { return async::stop_source(); }), 1.0);   // its signal and its children's queue in it
    EXPECT_DOUBLE_EQ(objects_per([] { return async::wait_group(); }), 2.0);    // its state and the round's channel
}

TEST(ChannelObjects_Tests, AStateOffTheHeapTakesItsOwnFirstNodes) {
    // semaphores on a stack: their channels' lists take a node each, the
    // rings lie in them
    collector::force_collect(true);
    const size_t before = collector::get_live_object_count();
    async::semaphore a(1), b(1), c(1), d(1);
    const size_t after = collector::get_live_object_count();
    EXPECT_EQ(after - before, 8u);
    for (async::semaphore* s : {&a, &b, &c, &d}) {
        EXPECT_TRUE(s->try_acquire());
        EXPECT_FALSE(s->try_acquire());
        s->release();
    }
}

TEST(ChannelObjects_Tests, AnInlineRingStillWorks) {
    // the ring of eight slots, laps past its end: order and capacity hold
    async::channel<int> ch(3);
    for (int lap = 0; lap < 20; ++lap) {
        EXPECT_TRUE(ch.try_send(3 * lap));
        EXPECT_TRUE(ch.try_send(3 * lap + 1));
        EXPECT_TRUE(ch.try_send(3 * lap + 2));
        EXPECT_FALSE(ch.try_send(-1));   // full at its capacity, not at the ring's
        EXPECT_EQ(ch.size(), 3u);
        for (int k = 0; k < 3; ++k) {
            auto v = ch.try_receive();
            ASSERT_TRUE(v);
            EXPECT_EQ(*v, 3 * lap + k);
        }
        EXPECT_FALSE(ch.try_receive());
    }
    async::channel<void> sig(1);
    EXPECT_TRUE(sig.try_send());
    EXPECT_FALSE(sig.try_send());   // one held
    EXPECT_TRUE(sig.try_receive());
    EXPECT_FALSE(sig.try_receive());
    sig.close();
    EXPECT_FALSE(sig.receive().wait());
}

TEST(ChannelObjects_Tests, TheFirstNodesInTheStateUnderLoad) {
    // many senders and receivers through lists whose first nodes are the
    // state's: every element once, the old first nodes holding nothing
    async::channel<int> ch;   // a rendezvous: every element through the lists
    const int producers = 4, each = 2000;
    std::vector<async::task<>> tasks;
    std::atomic<long> sum{0};
    std::atomic<int> got{0};
    for (int p = 0; p < producers; ++p) {
        tasks.push_back([](async::channel<int> c, int base, int n) -> async::task<> {
            for (int i = 0; i < n; ++i) {
                (void)co_await c.send(base + i);
            }
        }(ch, p * each, each));
        (void)tasks.back().spawn();   // held in tasks, waited below
    }
    for (int r = 0; r < producers; ++r) {
        tasks.push_back([](async::channel<int> c, std::atomic<long>* s, std::atomic<int>* g, int n) -> async::task<> {
            for (int i = 0; i < n; ++i) {
                auto v = co_await c.receive();
                if (v) {
                    s->fetch_add(*v);
                    g->fetch_add(1);
                }
            }
        }(ch, &sum, &got, each));
        (void)tasks.back().spawn();   // held in tasks, waited below
    }
    for (auto& t : tasks) {
        t.wait();
    }
    const long n = long(producers) * each;
    EXPECT_EQ(got.load(), int(n));
    EXPECT_EQ(sum.load(), n * (n - 1) / 2);
}
