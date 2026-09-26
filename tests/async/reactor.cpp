//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <thread>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Pipe {
        int fd[2];
        Pipe() {
            [[maybe_unused]] int r = ::pipe(fd);
            assert(r == 0);
        }
        ~Pipe() {
            ::close(fd[0]);
            ::close(fd[1]);
        }
    };
}

TEST(Reactor_Test, ReadableWakesATaskWhenDataComes) {
    Pipe p;
    auto t = sgcl::async::spawn([](int fd) -> sgcl::async::task<int> {
        co_await sgcl::async::readable(fd)->receive();       // suspended, no thread held
        char c = 0;
        [[maybe_unused]] auto n = ::read(fd, &c, 1);
        co_return c;
    }(p.fd[0]));
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(t.done());
    [[maybe_unused]] auto n = ::write(p.fd[1], "x", 1);
    EXPECT_EQ(t.wait(), 'x');
    sgcl::async::scheduler::stop();
}

TEST(Reactor_Test, WritableIsAtOnceOnAPipeWithRoom) {
    Pipe p;
    EXPECT_TRUE(sgcl::async::writable(p.fd[1])->receive().wait());
    // and readable once the end of the stream comes
    ::close(p.fd[1]);
    p.fd[1] = ::dup(p.fd[0]);                                 // the destructor closes two descriptors
    EXPECT_TRUE(sgcl::async::readable(p.fd[0])->receive().wait());          // readable: the end of the stream
    sgcl::async::scheduler::stop();
}

TEST(Reactor_Test, AWaitBoundedByATimeoutAndCancelledByAToken) {
    Pipe p;
    bool timed = false;
    EXPECT_EQ(sgcl::async::select(sgcl::async::readable(p.fd[0])->on_receive([] {}), sgcl::async::timeout(20ms, [&] { timed = true; })).wait(), 1u);
    EXPECT_TRUE(timed);
    sgcl::async::stop_source src;
    auto t = sgcl::async::spawn([](int fd, sgcl::async::stop_token tok) -> sgcl::async::task<int> {
        co_return (int)co_await sgcl::async::select(sgcl::async::readable(fd)->on_receive([] {}), tok.on_stop([] {}));
    }(p.fd[0], src.token()));
    std::this_thread::sleep_for(10ms);
    src.request_stop();
    EXPECT_EQ(t.wait(), 1);
    [[maybe_unused]] auto n = ::write(p.fd[1], "y", 1);        // the registrations left behind fire and are dropped
    std::this_thread::sleep_for(10ms);
    sgcl::async::scheduler::stop();                                   // or are ended by the stop
}

TEST(Reactor_Test, ManyTasksOnManyPipes) {
    constexpr int N = 32;
    std::vector<Pipe> pipes(N);
    std::vector<sgcl::async::task<int>> tasks;
    for (int i = 0; i < N; ++i) {
        tasks.push_back(sgcl::async::spawn([](int fd) -> sgcl::async::task<int> {
            co_await sgcl::async::readable(fd)->receive();
            char c = 0;
            [[maybe_unused]] auto n = ::read(fd, &c, 1);
            co_return c;
        }(pipes[i].fd[0])));
    }
    for (int i = N - 1; i >= 0; --i) {
        char c = char('a' + i % 26);
        [[maybe_unused]] auto n = ::write(pipes[i].fd[1], &c, 1);
    }
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(tasks[i].wait(), 'a' + i % 26);
    }
    sgcl::async::scheduler::stop();
}

// Two waits on one descriptor in one direction are both signalled by its
// readiness: the kernel keeps one entry per descriptor and filter, and a
// second registration would have replaced the first (never signalled)
TEST(Reactor_Test, TwoWaitsOnOneDescriptorAreBothSignalled) {
    Pipe p;
    auto waiter = [](int fd) -> sgcl::async::task<bool> {
        co_return co_await sgcl::async::readable(fd)->receive();   // true: signalled, not closed with nothing
    };
    auto a = sgcl::async::spawn(waiter(p.fd[0]));
    auto b = sgcl::async::spawn(waiter(p.fd[0]));
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(a.done());
    EXPECT_FALSE(b.done());
    [[maybe_unused]] auto n = ::write(p.fd[1], "x", 1);
    EXPECT_TRUE(a.wait());
    EXPECT_TRUE(b.wait());
    sgcl::async::scheduler::stop();
}

// A registration cancelled while its descriptor stays open: the kernel
// still holds it, and hands it back when the descriptor becomes ready.
// The reactor's cancel freed the node the kernel named, so the event read
// freed memory (the address sanitizer's report); now the event carries a
// number that no longer names a registration, and is dropped.
TEST(Reactor_Test, AnEventOfACancelledRegistrationIsDropped) {
    Pipe p;
    auto ch = sgcl::async::readable(p.fd[0]);
    sgcl::async::cancel_waits(p.fd[0]);
    EXPECT_FALSE(ch->receive().wait());                               // ended with nothing
    [[maybe_unused]] auto n = ::write(p.fd[1], "x", 1);        // the kernel's entry fires
    std::this_thread::sleep_for(50ms);
    auto again = sgcl::async::readable(p.fd[0]);                      // a new registration on the number: signalled, the stale one not
    EXPECT_TRUE(again->receive().wait());
    sgcl::async::scheduler::stop();
}

// The order a close keeps (cancel_waits, then ::close) while the reactor's
// thread holds events it has taken from the kernel in one batch: an event
// of a registration cancelled meanwhile must not reach the freed node
TEST(Reactor_Test, CancelRacesTheEventsOfABatch) {
    constexpr int N = 64;
    for (int round = 0; round < 200; ++round) {
        int a[N], b[N];
        for (int i = 0; i < N; ++i) {
            int s[2];
            ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, s), 0);
            a[i] = s[0];
            b[i] = s[1];
            (void)sgcl::async::readable(a[i]);
        }
        for (int i = 0; i < N; ++i) {
            [[maybe_unused]] auto n = ::write(b[i], "x", 1);   // the reactor wakes and fires
        }
        for (int i = N - 1; i >= 0; --i) {
            sgcl::async::cancel_waits(a[i]);
            ::close(a[i]);
        }
        for (int i = 0; i < N; ++i) {
            ::close(b[i]);
        }
    }
    sgcl::async::scheduler::stop();
}

// A wait given up (a deadline passed, and its channel closed by the one
// who waited: net's descriptor does so) is dropped by the reactor, and does
// not stay on the descriptor's registration until the descriptor is
// ready: an idle connection read with a short deadline in a loop kept one
// wait and one channel per read. A wait still open is kept and signalled.
namespace {
    struct GivenUp {
        GivenUp() { ++alive; }
        ~GivenUp() { --alive; }
        sgcl::async::channel<void> ch{1};
        inline static std::atomic<int> alive = {0};
    };

    SGCL_NOINLINE void give_up_waits(int fd, int count) {
        for (int i = 0; i < count; ++i) {
            sgcl::tracked_ptr w = sgcl::make_tracked<GivenUp>();
            sgcl::async::detail::reactor_instance().watch(fd, false, w, &w->ch);
            w->ch.close();
        }
    }
}

TEST(Reactor_Test, WaitsGivenUpAreDropped) {
    Pipe p;
    auto live = sgcl::async::readable(p.fd[0]);
    give_up_waits(p.fd[0], 2000);
    collector::clear_stack();
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_LE(GivenUp::alive.load(), 64);                      // was 2000: every one held by the registration
    [[maybe_unused]] auto n = ::write(p.fd[1], "x", 1);
    EXPECT_TRUE(live->receive().wait());                              // the wait still open: signalled
    sgcl::async::scheduler::stop();
}

// The first wait of a reactor made while the descriptors are exhausted:
// kqueue() fails. The reactor was marked running with no queue, and every
// wait after it was signalled at once, forever (a read on an idle
// connection spun); now the wait ends with nothing, and the next one makes
// the queue again.
TEST(Reactor_Test, AQueueThatCannotBeMadeIsMadeByTheNextWait) {
    Pipe p;
    sgcl::async::detail::Reactor reactor;                             // a fresh one: its queue not made yet
    rlimit old;
    ASSERT_EQ(::getrlimit(RLIMIT_NOFILE, &old), 0);
    rlimit low = old;
    low.rlim_cur = 64;
    ASSERT_EQ(::setrlimit(RLIMIT_NOFILE, &low), 0);
    std::vector<int> held;
    for (int fd; (fd = ::dup(p.fd[0])) >= 0;) {
        held.push_back(fd);
    }
    sgcl::tracked_ptr first = sgcl::make_tracked<sgcl::async::channel<void>>(1);
    reactor.watch(p.fd[0], false, first, first.get());
    for (int fd : held) {
        ::close(fd);
    }
    ::setrlimit(RLIMIT_NOFILE, &old);
    EXPECT_FALSE(first->receive().wait());                            // ended with nothing: no queue to wait on
    sgcl::tracked_ptr second = sgcl::make_tracked<sgcl::async::channel<void>>(1);
    reactor.watch(p.fd[0], false, second, second.get());
    std::this_thread::sleep_for(30ms);
    EXPECT_FALSE(second->closed());                            // waiting: nothing to read yet
    [[maybe_unused]] auto n = ::write(p.fd[1], "x", 1);
    EXPECT_TRUE(second->receive().wait());
    reactor.stop();
    sgcl::async::scheduler::stop();
}

// A wait registered between cancel_waits(fd) and ::close(fd) (a reader
// racing the closer): its registration stays in the reactor's table while
// the close drops the kernel's entry. A later wait on the number joined
// that registration and made no entry of its own, so it was never
// signalled, and no wait on the number in that direction ever was again;
// now a wait that joins renews the kernel's entry.
TEST(Reactor_Test, AWaitBetweenCancelAndCloseDoesNotPoisonTheNumber) {
    Pipe p;
    int fd = p.fd[0];
    sgcl::async::cancel_waits(fd);
    auto stale = sgcl::async::readable(fd);                           // the reader, between the closer's two steps
    int q[2];
    ASSERT_EQ(::pipe(q), 0);
    ASSERT_EQ(::dup2(q[0], fd), fd);                           // the close, and the number another pipe's at once
    ::close(q[0]);
    auto fresh = sgcl::async::readable(fd);
    [[maybe_unused]] auto n = ::write(q[1], "x", 1);
    auto waited = [](auto& ch) {
        auto start = std::chrono::steady_clock::now();
        while (!ch->closed() && std::chrono::steady_clock::now() - start < 2s) {
            std::this_thread::sleep_for(1ms);
        }
        return ch->closed();
    };
    EXPECT_TRUE(waited(fresh));                                // signalled: the data is there
    auto later = sgcl::async::readable(fd);
    EXPECT_TRUE(waited(later));
    ::close(q[1]);
    sgcl::async::scheduler::stop();
}

// Two stops at once (two threads calling scheduler::stop(), or the
// reactor's destructor at exit beside a stop): both found the reactor
// running and both joined its thread, and the second join terminated the
// program. Now the second waits for the first and finds it stopped.
TEST(Reactor_Test, TwoStopsAtOnce) {
    Pipe p;
    for (int round = 0; round < 500; ++round) {
        auto w = sgcl::async::readable(p.fd[0]);                      // the reactor started
        std::atomic<int> ready = {0};
        auto stopper = [&] {
            ready.fetch_add(1);
            while (ready.load() < 2) {
            }
            sgcl::async::detail::reactor_instance().stop();
        };
        std::thread a(stopper), b(stopper);
        a.join();
        b.join();
        EXPECT_FALSE(w->receive().wait());                            // ended with nothing by the stop
    }
    sgcl::async::scheduler::stop();
}
