//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of a channel (DESIGN 408): a rendezvous and a capacity
// of one, a capacity past the largest ring, a closed channel and a close
// while tasks wait on both ends, a handle moved from or assigned to
// itself, and an element whose move throws half-way through a send or a
// receive.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    template<class F>
    bool soon(F&& f) {
        auto until = std::chrono::steady_clock::now() + 5s;
        while (!f()) {
            if (std::chrono::steady_clock::now() > until) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    // An element whose move throws once, at the first move of the value
    // armed: the copy and the assignment the same, so that whichever
    // operation the channel makes on that element throws
    struct Fragile {
        int v = 0;

        explicit Fragile(int v) noexcept
        : v(v) {
        }

        Fragile(const Fragile& o)
        : v(o.v) {
            _check(o.v);
        }

        Fragile(Fragile&& o)
        : v(o.v) {
            _check(o.v);
        }

        Fragile& operator=(const Fragile& o) {
            _check(o.v);
            v = o.v;
            return *this;
        }

        Fragile& operator=(Fragile&& o) {
            _check(o.v);
            v = o.v;
            return *this;
        }

        static void arm(int v) noexcept {
            armed.store(v);
        }

        inline static std::atomic<int> armed = {-1};

    private:
        static void _check(int v) {
            int e = v;
            if (armed.compare_exchange_strong(e, -1)) {
                throw std::runtime_error("the move of a fragile element");
            }
        }
    };
    static_assert(!std::is_nothrow_move_constructible_v<Fragile>);

    sgcl::async::task<> receive_one(sgcl::async::channel<Fragile> ch, std::atomic<int>* got) {
        auto v = co_await ch.receive();
        got->store(v ? v->v : 0);
    }
}

// A ring is a power of two of slots, at least the capacity: a capacity
// past the largest power of two, or past the largest array of slots, is
// the length_error the constructor's page promises (std::bit_ceil of it
// has no answer)
TEST(ChannelBoundary_Tests, ACapacityPastTheLargestRingIsALengthError) {
    EXPECT_THROW(sgcl::async::channel<int>(SIZE_MAX), std::length_error);
    EXPECT_THROW(sgcl::async::channel<int>((SIZE_MAX >> 1) + 2), std::length_error);
    EXPECT_THROW(sgcl::async::channel<int>(size_t(1) << 63), std::length_error);
    EXPECT_THROW(sgcl::async::channel<int>(size_t(1) << 60), std::length_error);
    EXPECT_THROW(sgcl::async::channel<void>(SIZE_MAX), std::length_error);
    EXPECT_THROW(sgcl::async::channel<void>(size_t(1) << 63), std::length_error);
    EXPECT_THROW(sgcl::async::broadcast<int>(SIZE_MAX), std::length_error);
    EXPECT_THROW(sgcl::async::broadcast<int>(size_t(1) << 63), std::length_error);
    EXPECT_THROW(sgcl::async::semaphore(1, SIZE_MAX), std::length_error);
    sgcl::async::channel<int> nine(9);                        // past the inline ring: an array of 16
    EXPECT_EQ(nine.capacity(), 9u);
    for (int i = 0; i < 9; ++i) {
        EXPECT_TRUE(nine.try_send(i));
    }
    EXPECT_FALSE(nine.try_send(9));                           // the capacity, not the ring, is the limit
    EXPECT_EQ(nine.size(), 9u);
}

// A rendezvous holds nothing: no try_send without a receiver, no
// try_receive without a sender; closed, every operation ends at once
TEST(ChannelBoundary_Tests, ARendezvousOpenAndClosed) {
    sgcl::async::channel<int> ch;
    EXPECT_EQ(ch.capacity(), 0u);
    EXPECT_EQ(ch.size(), 0u);
    EXPECT_TRUE(ch.empty());
    EXPECT_FALSE(ch.try_send(1));
    EXPECT_FALSE(ch.try_receive());
    EXPECT_FALSE(ch.closed());
    ch.close();
    ch.close();                                               // a second close does nothing
    EXPECT_TRUE(ch.closed());
    EXPECT_FALSE(ch.try_send(1));
    EXPECT_FALSE(ch.send(2).wait());
    EXPECT_FALSE(ch.receive().wait());
    EXPECT_FALSE(ch.try_receive());
    EXPECT_TRUE(ch.begin() == ch.end());                      // a range-for runs no round
    int rounds = 0;
    for ([[maybe_unused]] int v : ch) {
        ++rounds;
    }
    EXPECT_EQ(rounds, 0);
    EXPECT_TRUE(ch.empty());

    sgcl::async::channel<void> signals;
    EXPECT_EQ(signals.capacity(), 0u);
    EXPECT_FALSE(signals.try_send());
    EXPECT_FALSE(signals.try_receive());
    signals.close();
    signals.close();
    EXPECT_FALSE(signals.try_send());
    EXPECT_FALSE(signals.send().wait());
    EXPECT_FALSE(signals.receive().wait());
    EXPECT_TRUE(signals.empty());
}

// A capacity of one: one element, then full; a channel of zero made by
// the explicit constructor is the rendezvous of the default one
TEST(ChannelBoundary_Tests, ACapacityOfOneAndAnExplicitZero) {
    sgcl::async::channel<int> one(1);
    EXPECT_TRUE(one.try_send(1));
    EXPECT_FALSE(one.try_send(2));
    EXPECT_EQ(one.size(), 1u);
    EXPECT_FALSE(one.empty());
    one.close();
    EXPECT_EQ(one.try_receive(), sgcl::optional<int>(1));     // drained after the close
    EXPECT_FALSE(one.try_receive());

    sgcl::async::channel<int> zero(0);
    EXPECT_EQ(zero.capacity(), 0u);
    EXPECT_FALSE(zero.try_send(1));
    std::thread r([&] {
        EXPECT_EQ(zero.receive().wait(), sgcl::optional<int>(5));
    });
    EXPECT_TRUE(soon([&] { return zero.try_send(5); }));     // delivered once the receiver waits
    r.join();
}

// A close while tasks wait on a rendezvous, on both ends: every receiver
// gets nothing, every sender false, and nothing is left waiting
TEST(ChannelBoundary_Tests, ACloseWhileTasksWaitOnARendezvous) {
    sgcl::async::channel<int> in, out;
    std::atomic<int> nothing = 0, refused = 0;
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::channel<int> ch, std::atomic<int>* nothing) -> sgcl::async::task<> {
            if (!co_await ch.receive()) {
                ++*nothing;
            }
        }(in, &nothing)));
        tasks.push_back(sgcl::async::spawn([](sgcl::async::channel<int> ch, std::atomic<int>* refused, int v) -> sgcl::async::task<> {
            if (!co_await ch.send(v)) {
                ++*refused;
            }
        }(out, &refused, i)));
    }
    std::this_thread::sleep_for(20ms);                        // every task suspended on its channel
    EXPECT_FALSE(out.empty());                                // senders hold their elements
    in.close();
    out.close();
    for (auto& t : tasks) {
        t.wait();
    }
    EXPECT_EQ(nothing.load(), 8);
    EXPECT_EQ(refused.load(), 8);
    EXPECT_TRUE(out.empty());
    EXPECT_FALSE(out.try_receive());
    sgcl::async::scheduler::stop();
}

// A close while tasks wait on a channel of signals and on a full buffer
TEST(ChannelBoundary_Tests, ACloseWhileTasksWaitOnSignalsAndAFullBuffer) {
    sgcl::async::channel<void> signals;
    sgcl::async::channel<int> full(1);
    EXPECT_TRUE(full.try_send(0));
    std::atomic<int> ended = 0;
    std::vector<sgcl::async::task<>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::channel<void> ch, std::atomic<int>* ended) -> sgcl::async::task<> {
            if (!co_await ch.receive()) {
                ++*ended;
            }
        }(signals, &ended)));
        tasks.push_back(sgcl::async::spawn([](sgcl::async::channel<int> ch, std::atomic<int>* ended) -> sgcl::async::task<> {
            if (!co_await ch.send(1)) {
                ++*ended;
            }
        }(full, &ended)));
    }
    std::this_thread::sleep_for(20ms);
    signals.close();
    full.close();
    for (auto& t : tasks) {
        t.wait();
    }
    EXPECT_EQ(ended.load(), 8);
    EXPECT_EQ(full.try_receive(), sgcl::optional<int>(0));    // what was in the buffer before the close
    EXPECT_FALSE(full.try_receive());
    sgcl::async::scheduler::stop();
}

// A handle moved from is the same channel still (its word copied), and
// an assignment of a handle to itself changes nothing; the same for the
// receiving end
TEST(ChannelBoundary_Tests, AHandleMovedFromOrAssignedToItself) {
    sgcl::async::channel<int> a(2);
    sgcl::async::channel<int> b = std::move(a);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a.try_send(1));
    EXPECT_EQ(b.try_receive(), sgcl::optional<int>(1));
    sgcl::async::channel<int> c;
    c = std::move(b);
    EXPECT_TRUE(b == a);
    EXPECT_TRUE(c == a);
    auto& same = c;
    c = same;
    c = std::move(same);
    EXPECT_TRUE(c == a);
    EXPECT_EQ(c.capacity(), 2u);
    EXPECT_TRUE(c.try_send(2));
    EXPECT_EQ(a.try_receive(), sgcl::optional<int>(2));

    sgcl::async::receive_channel<int> r = a;
    sgcl::async::receive_channel<int> s = std::move(r);
    EXPECT_TRUE(r == s);
    EXPECT_TRUE(r == a);
    auto& alias = s;
    s = alias;
    s = std::move(alias);
    EXPECT_TRUE(s == a);
    EXPECT_TRUE(a.try_send(3));
    EXPECT_EQ(r.try_receive(), sgcl::optional<int>(3));
    a.close();
    EXPECT_TRUE(s.closed());
    EXPECT_FALSE(s.receive().wait());

    sgcl::async::channel<void> v(1);
    sgcl::async::channel<void> w = std::move(v);
    EXPECT_TRUE(v == w);
    EXPECT_TRUE(v.try_send());
    EXPECT_TRUE(w.try_receive());
}

// A send whose element's move into the buffer throws: the exception
// comes out of the send, the slot it reserved is passed over, and the
// channel goes on in its order
TEST(ChannelBoundary_Tests, AMoveThatThrowsIntoTheBuffer) {
    sgcl::async::channel<Fragile> ch(4);
    EXPECT_TRUE(ch.try_send(Fragile(1)));
    Fragile::arm(2);
    EXPECT_THROW(ch.try_send(Fragile(2)), std::runtime_error);
    EXPECT_TRUE(ch.try_send(Fragile(3)));
    EXPECT_EQ(ch.try_receive()->v, 1);
    EXPECT_EQ(ch.try_receive()->v, 3);
    EXPECT_FALSE(ch.try_receive());
    for (int i = 10; i < 50; ++i) {                           // round the ring several times
        ASSERT_TRUE(ch.try_send(Fragile(i)));
        auto got = ch.try_receive();
        ASSERT_TRUE(got);
        EXPECT_EQ(got->v, i);
    }
    EXPECT_TRUE(ch.empty());
}

// A receive whose element's move out of the buffer throws: the
// exception comes out of the receive, that element is lost, and its slot
// is free for the sends after it
TEST(ChannelBoundary_Tests, AMoveThatThrowsOutOfTheBuffer) {
    sgcl::async::channel<Fragile> ch(2);
    EXPECT_TRUE(ch.try_send(Fragile(1)));
    EXPECT_TRUE(ch.try_send(Fragile(2)));
    Fragile::arm(1);
    EXPECT_THROW((void)ch.try_receive(), std::runtime_error);
    EXPECT_EQ(ch.try_receive()->v, 2);
    EXPECT_FALSE(ch.try_receive());
    for (int i = 10; i < 50; ++i) {
        ASSERT_TRUE(ch.try_send(Fragile(i)));
        ASSERT_TRUE(ch.try_send(Fragile(i + 100)));
        EXPECT_EQ(ch.try_receive()->v, i);
        EXPECT_EQ(ch.try_receive()->v, i + 100);
    }
}

// A send to a waiting receiver whose element's move throws: the
// exception comes out of the send, and the receiver waits on, served by
// the next send
TEST(ChannelBoundary_Tests, AMoveThatThrowsIntoAWaitingReceiver) {
    sgcl::async::channel<Fragile> ch;
    std::atomic<int> got = -1;
    sgcl::async::go(receive_one(ch, &got));
    std::this_thread::sleep_for(20ms);                        // suspended on the receive
    Fragile::arm(7);
    bool threw = false;
    ASSERT_TRUE(soon([&] {
        try {
            return ch.try_send(Fragile(7));
        } catch (const std::runtime_error&) {
            threw = true;
            return true;
        }
    }));
    EXPECT_TRUE(threw);
    EXPECT_EQ(got.load(), -1);                                // the receiver still waits
    EXPECT_TRUE(soon([&] { return ch.try_send(Fragile(8)); }));
    EXPECT_TRUE(soon([&] { return got.load() == 8; }));
    sgcl::async::scheduler::stop();
}

// A receive that moves a waiting sender's element into the buffer, and
// that move throws: the receive gives what it took, the sender's send
// throws, and the channel goes on
TEST(ChannelBoundary_Tests, AMoveThatThrowsFromAWaitingSender) {
    sgcl::async::channel<Fragile> ch(1);
    EXPECT_TRUE(ch.try_send(Fragile(1)));
    std::atomic<int> outcome = 0;   // 1 delivered, 2 refused, 3 threw
    std::thread sender([&] {
        try {
            outcome = ch.send(Fragile(2)).wait() ? 1 : 2;
        } catch (const std::runtime_error&) {
            outcome = 3;
        }
    });
    ASSERT_TRUE(soon([&] { return !ch.empty() && ch.size() == 1; }));
    std::this_thread::sleep_for(20ms);                        // the sender waits with its element
    Fragile::arm(2);
    auto first = ch.receive().wait();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->v, 1);
    sender.join();
    EXPECT_EQ(outcome.load(), 3);
    EXPECT_FALSE(ch.try_receive());                           // the element whose move threw is not there
    EXPECT_TRUE(ch.try_send(Fragile(3)));
    EXPECT_EQ(ch.try_receive()->v, 3);
    EXPECT_TRUE(ch.empty());
}
