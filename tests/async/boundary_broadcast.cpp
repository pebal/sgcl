//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of a broadcast (DESIGN 408): a ring of one, no
// subscriber, a subscription empty, moved from and assigned to itself,
// a subscription made after the close, a value whose copy throws. A
// capacity past the largest ring is ChannelBoundary_Tests'; a lap of a
// ring of eight is Broadcast_Test.ASlowSubscriberIsLapped.
#include "tests/types.h"

#include <stdexcept>
#include <utility>

namespace {
    // A value whose copy throws when asked to, once
    struct Touchy {
        int v = 0;

        explicit Touchy(int v) noexcept
        : v(v) {
        }

        Touchy(const Touchy& o)
        : v(o.v) {
            if (std::exchange(fail, false)) {
                throw std::runtime_error("copy");
            }
        }

        Touchy(Touchy&&) noexcept = default;
        Touchy& operator=(Touchy&&) noexcept = default;

        inline static bool fail = false;
    };
}

// A ring of one (a capacity of zero rounds up to it): every send laps a
// subscription that has not received, which gets the last value and the
// count of the ones it lost
TEST(BroadcastBoundary_Tests, ARingOfOne) {
    sgcl::async::broadcast<int> b(0);
    EXPECT_EQ(b.capacity(), 1u);
    auto s = b.subscribe();
    EXPECT_TRUE(b.send(1));
    EXPECT_EQ(s.try_receive(), sgcl::optional<int>(1));
    EXPECT_EQ(s.lagged(), 0u);
    for (int i = 2; i <= 5; ++i) {
        EXPECT_TRUE(b.send(i));
    }
    EXPECT_EQ(s.try_receive(), sgcl::optional<int>(5));
    EXPECT_EQ(s.lagged(), 3u);
    EXPECT_FALSE(s.try_receive());
}

// A value sent with nobody subscribed is dropped, and a subscription made
// after it starts at the next one; the count of the subscriptions follows
// their making and their end
TEST(BroadcastBoundary_Tests, NoSubscriber) {
    sgcl::async::broadcast<int> b(4);
    EXPECT_EQ(b.subscribers(), 0u);
    EXPECT_TRUE(b.send(1));
    auto s = b.subscribe();
    EXPECT_EQ(b.subscribers(), 1u);
    EXPECT_FALSE(s.try_receive());
    EXPECT_TRUE(b.send(2));
    EXPECT_EQ(s.try_receive(), sgcl::optional<int>(2));
    {
        auto t = b.subscribe();
        EXPECT_EQ(b.subscribers(), 2u);
    }
    EXPECT_EQ(b.subscribers(), 1u);
}

// A subscription made empty, moved from or assigned over: empty and false;
// one moved to keeps the cursor; a move-assignment to itself keeps it
// subscribed; an assignment over a subscription counts it off
TEST(BroadcastBoundary_Tests, ASubscriptionEmptyMovedFromAndAssignedToItself) {
    sgcl::async::broadcast<int>::subscription empty;
    EXPECT_FALSE(empty);
    sgcl::async::broadcast<int> b(4);
    auto s = b.subscribe();
    EXPECT_TRUE(s);
    EXPECT_TRUE(b.send(1));
    auto t = std::move(s);
    EXPECT_FALSE(s);
    EXPECT_TRUE(t);
    auto& same = t;
    t = std::move(same);
    EXPECT_TRUE(t);
    EXPECT_EQ(b.subscribers(), 1u);
    EXPECT_EQ(t.try_receive(), sgcl::optional<int>(1));
    s = b.subscribe();
    EXPECT_EQ(b.subscribers(), 2u);
    t = std::move(empty);                                     // the subscription assigned over: counted off
    EXPECT_FALSE(t);
    EXPECT_EQ(b.subscribers(), 1u);
    t = std::move(s);
    EXPECT_TRUE(t);
    EXPECT_EQ(b.subscribers(), 1u);
}

// After the close: a send is refused, a second close does nothing, a
// subscription made then is closed and receives nothing at once, one made
// before receives what was sent, then nothing
TEST(BroadcastBoundary_Tests, AfterTheClose) {
    sgcl::async::broadcast<int> b(4);
    auto before = b.subscribe();
    EXPECT_TRUE(b.send(7));
    b.close();
    b.close();
    EXPECT_TRUE(b.closed());
    EXPECT_FALSE(b.send(8));
    auto after = b.subscribe();
    EXPECT_TRUE(after.closed());
    EXPECT_FALSE(after.receive().wait());
    EXPECT_FALSE(after.try_receive());
    EXPECT_EQ(before.receive().wait(), sgcl::optional<int>(7));
    EXPECT_FALSE(before.receive().wait());
    EXPECT_TRUE(before.closed());
}

// A receive whose copy of the value throws leaves the cursor where it
// was: the next receive gives the same value
TEST(BroadcastBoundary_Tests, AValueWhoseCopyThrows) {
    sgcl::async::broadcast<Touchy> b(2);
    auto s = b.subscribe();
    EXPECT_TRUE(b.send(Touchy(1)));
    EXPECT_TRUE(b.send(Touchy(2)));
    Touchy::fail = true;
    EXPECT_THROW((void)s.try_receive(), std::runtime_error);
    auto v = s.try_receive();
    ASSERT_TRUE(v);
    EXPECT_EQ(v->v, 1);
    EXPECT_EQ(s.receive().wait()->v, 2);
    Touchy one(3);
    Touchy::fail = true;
    EXPECT_THROW(b.send(one), std::runtime_error);            // the copy into the send: nothing sent
    EXPECT_FALSE(s.try_receive());
    EXPECT_TRUE(b.send(one));
    EXPECT_EQ(s.try_receive()->v, 3);
}
