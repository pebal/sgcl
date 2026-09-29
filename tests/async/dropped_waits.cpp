//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A task destroyed while it waits: for every waiting primitive, a task that
// waits on it and is let go of in three ways, and the wait's end after
// that. The frame's locals must be destroyed once, whatever the way:
//   - dropped: spawned, its object dropped (a started task let go of runs
//     on to its end, as a detached one: DESIGN 302), then the wait served;
//   - a timeout's loser: with_timeout over it, the deadline first (the race
//     holds the loser, which runs on);
//   - a select's losing case: the primitive's case beside a timeout that
//     comes first (the select cancels the other cases).
// A Guard in the frame counts its destructions; a second one is the
// coroutine resumed after it was destroyed, as every dropped case showed
// while a dropped task was destroyed where it waited (DESIGN 300, fixed by
// 302). The primitives: the blocking pool
// (spawn_blocking), a channel's receive and send, a select, an event, a
// timer (sleep), the reactor (readable), the signals; the resolver's wait
// is net's (tests/net: dropped_dns.cpp).
#include "tests/types.h"
#include "sgcl/async/async.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <string>
#include <thread>
#include <unistd.h>

using namespace sgcl::async;
using namespace std::chrono_literals;

namespace {
    std::atomic<int> destroyed = 0;   // the Guard's destructions
    std::atomic<int> resumed = 0;     // the code after the wait run
    std::atomic<bool> entered = false;

    struct Guard {
        ~Guard() {
            ++destroyed;
        }
    };

    void settle() {
        std::this_thread::sleep_for(100ms);
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
        std::this_thread::sleep_for(20ms);
    }

    void wait_entered() {
        for (int i = 0; i < 2000 && !entered; ++i) {
            std::this_thread::sleep_for(1ms);
        }
        std::this_thread::sleep_for(20ms);   // past the wait's registration
    }

    // The dropped case: made, spawned, dropped while it waits; then the
    // wait served. The locals destroyed once (at the drop, or at the end of
    // a task that ran on)
    void dropped(const std::function<task<>()>& make, const std::function<void()>& serve) {
        destroyed = 0;
        resumed = 0;
        entered = false;
        {
            auto t = spawn(make());
            wait_entered();
            ASSERT_TRUE(entered);
        }
        serve();
        settle();
        EXPECT_EQ(destroyed.load(), 1) << "the locals destroyed " << destroyed.load() << " times (resumed after the drop: " << resumed.load() << ")";
    }

    // The timeout's loser: the deadline first, then the wait served
    void timeout_loser(const std::function<task<>()>& make, const std::function<void()>& serve) {
        destroyed = 0;
        resumed = 0;
        entered = false;
        auto r = with_timeout(make(), 20ms).wait();
        EXPECT_FALSE(r.has_value());
        serve();
        settle();
        EXPECT_EQ(destroyed.load(), 1);
        EXPECT_EQ(resumed.load(), 1);   // the loser ran on to its end
    }

    // The primitives' waits, each a task with a Guard in its frame
    std::atomic<bool> pool_release = false;

    task<> on_pool() {
        Guard g;
        entered = true;
        (void)co_await spawn_blocking([] {
            while (!pool_release) {
                std::this_thread::sleep_for(1ms);
            }
            return 0;
        });
        ++resumed;
    }

    task<> on_receive(channel<int> ch) {
        Guard g;
        entered = true;
        (void)co_await ch.receive();
        ++resumed;
    }

    task<> on_send(channel<int> ch) {
        Guard g;
        entered = true;
        (void)co_await ch.send(1);
        ++resumed;
    }

    task<> on_select(channel<int> a, channel<int> b) {
        Guard g;
        entered = true;
        (void)co_await select(a.on_receive([](optional<int>) {}), b.on_receive([](optional<int>) {}));
        ++resumed;
    }

    task<> on_event(event e) {
        Guard g;
        entered = true;
        co_await e;
        ++resumed;
    }

    task<> on_sleep() {
        Guard g;
        entered = true;
        co_await sgcl::async::sleep(60ms);
        ++resumed;
    }

    task<> on_readable(int fd) {
        Guard g;
        entered = true;
        co_await readable(fd);
        ++resumed;
    }

    task<> on_signal(channel<int> ch) {
        Guard g;
        entered = true;
        (void)co_await ch.receive();
        ++resumed;
    }

    task<> send_one(channel<int> ch) {
        (void)co_await ch.send(7);
    }

    task<> receive_one(channel<int> ch) {
        (void)co_await ch.receive();
    }

    // Serves a channel's receiver: a send, which may find nobody (the
    // receiver gone) and waits then until the close
    void serve_receiver(channel<int> ch) {
        auto s = spawn(send_one(ch));
        std::this_thread::sleep_for(50ms);
        ch.close();
        s.wait();
    }

    void serve_sender(channel<int> ch) {
        auto r = spawn(receive_one(ch));
        std::this_thread::sleep_for(50ms);
        ch.close();
        r.wait();
    }
}

// --- dropped: the task runs on to its end, its locals destroyed there, once

TEST(DroppedWaits_Tests, Pool) {
    pool_release = false;
    dropped([] { return on_pool(); }, [] { pool_release = true; });
}

TEST(DroppedWaits_Tests, ChannelReceive) {
    channel<int> ch(0);
    dropped([ch] { return on_receive(ch); }, [ch] { serve_receiver(ch); });
}

TEST(DroppedWaits_Tests, ChannelSend) {
    channel<int> ch(0);
    dropped([ch] { return on_send(ch); }, [ch] { serve_sender(ch); });
}

TEST(DroppedWaits_Tests, Select) {
    channel<int> a(0), b(0);
    dropped([a, b] { return on_select(a, b); }, [a, b] {
        serve_receiver(a);
        b.close();
    });
}

TEST(DroppedWaits_Tests, Event) {
    event e;
    dropped([e] { return on_event(e); }, [e] { e.set(); });
}

TEST(DroppedWaits_Tests, Timer) {
    dropped([] { return on_sleep(); }, [] { std::this_thread::sleep_for(100ms); });
}

TEST(DroppedWaits_Tests, Reactor) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    const int w = fds[1];
    dropped([fd = fds[0]] { return on_readable(fd); }, [w] { (void)::write(w, "x", 1); });
    ::close(fds[0]);
    ::close(fds[1]);
}

TEST(DroppedWaits_Tests, Signal) {
    auto ch = signals({SIGUSR2});
    dropped([ch] { return on_signal(ch); }, [] { ::kill(::getpid(), SIGUSR2); });
    reset_signals({SIGUSR2});
}

// --- a timeout's loser: runs on to its end (the race holds it)

TEST(DroppedWaits_Tests, TimeoutLoserPool) {
    pool_release = false;
    timeout_loser([] { return on_pool(); }, [] { pool_release = true; });
}

TEST(DroppedWaits_Tests, TimeoutLoserChannel) {
    channel<int> ch(0);
    timeout_loser([ch] { return on_receive(ch); }, [ch] { serve_receiver(ch); });
}

TEST(DroppedWaits_Tests, TimeoutLoserEvent) {
    event e;
    timeout_loser([e] { return on_event(e); }, [e] { e.set(); });
}

TEST(DroppedWaits_Tests, TimeoutLoserTimer) {
    timeout_loser([] { return on_sleep(); }, [] { std::this_thread::sleep_for(100ms); });
}

TEST(DroppedWaits_Tests, TimeoutLoserReactor) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    const int w = fds[1];
    timeout_loser([fd = fds[0]] { return on_readable(fd); }, [w] { (void)::write(w, "x", 1); });
    ::close(fds[0]);
    ::close(fds[1]);
}

// --- a select's losing case: cancelled by the select, the task goes on

TEST(DroppedWaits_Tests, SelectLoserChannelAndEvent) {
    destroyed = 0;
    resumed = 0;
    channel<int> ch(0);
    event e;
    auto run = [](channel<int> c, event ev) -> task<size_t> {
        Guard g;
        size_t which = co_await select(c.on_receive([](optional<int>) {}), ev.on_set([] {}), timeout(20ms, [] {}));
        ++resumed;
        co_return which;
    };
    EXPECT_EQ(spawn(run(ch, e)).wait(), 2u);   // the timeout served; the channel's and the event's cases cancelled
    serve_receiver(ch);                        // a send that finds the cancelled case: nobody takes it
    e.set();
    settle();
    EXPECT_EQ(destroyed.load(), 1);
    EXPECT_EQ(resumed.load(), 1);
}
