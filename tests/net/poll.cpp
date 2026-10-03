//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: the descriptors' waits on the reactor (async/reactor.h: PollSlot;
// io/detail/descriptor.h). A descriptor is registered once, edge-triggered,
// and a wait parks in its slot's word: an edge that comes between a call's
// EAGAIN and the park must be kept and taken by the park, a close must end
// every waiter of both directions, a read and a write wait on one number
// at once, several waiters in one direction (tasks accepting on one
// listener) all get their turn, and a stop of the reactor ends a parked
// wait and a later one registers again. Each wait that could hang is under
// a watchdog that closes the descriptor, so that a lost wake fails the
// test rather than stopping the suite. Run under the thread and address
// sanitizers as well.
#include "tests/types.h"
#include "sgcl/net/net.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    using Descriptor = net::detail::Descriptor;
    using WaitResult = net::detail::WaitResult;
    using PollSlot = sgcl::async::detail::PollSlot;

    struct Holder {
        explicit Holder(int fd)
        : d(fd) {
        }

        Descriptor d;
    };

    // A socket pair, both ends non-blocking; the second end is the
    // descriptor's (it closes it), the first the test's
    struct Pair {
        int peer = -1;
        tracked_ptr<Holder> h;

        Pair() {
            int p[2];
            [[maybe_unused]] int r = ::socketpair(AF_UNIX, SOCK_STREAM, 0, p);
            assert(r == 0);
            net::detail::prepare_socket(p[0]);
            net::detail::prepare_socket(p[1]);
            peer = p[0];
            h = make_tracked<Holder>(p[1]);
        }

        ~Pair() {
            h->d.close();
            ::close(peer);
        }

        int fd() const {
            return h->d.fd();
        }
    };

    PollSlot& slot_of(int fd) {
        return *sgcl::async::detail::reactor_instance().slot(fd);
    }

    // Spins until the direction's word has every bit of `bits`; false after two seconds
    bool until(int fd, int dir, uint32_t bits) {
        auto end = std::chrono::steady_clock::now() + 2s;
        while ((slot_of(fd).state[dir].load() & bits) != bits) {
            if (std::chrono::steady_clock::now() > end) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    // Calls f once `after` has passed, unless disarmed first (the destructor)
    struct Watchdog {
        Watchdog(std::chrono::milliseconds after, std::function<void()> f)
        : _t([this, after, f = std::move(f)] {
            std::unique_lock lock(_m);
            if (!_cv.wait_for(lock, after, [this] { return _off; })) {
                fired = true;
                f();
            }
        }) {
        }

        ~Watchdog() {
            {
                std::lock_guard lock(_m);
                _off = true;
            }
            _cv.notify_one();
            _t.join();
        }

        std::atomic<bool> fired = {false};

    private:
        std::mutex _m;
        std::condition_variable _cv;
        bool _off = false;
        std::thread _t;
    };

    ssize_t recv1(int fd) {
        char c;
        return ::recv(fd, &c, 1, 0);
    }

    void send1(int fd) {
        char c = 'x';
        [[maybe_unused]] auto n = ::send(fd, &c, 1, 0);
    }

    // The send buffer of fd filled: a write on it now answers EAGAIN
    void fill(int fd) {
        std::vector<char> block(65536, 'f');
        while (::send(fd, block.data(), block.size(), 0) > 0) {
        }
        while (::send(fd, block.data(), 1, 0) > 0) {
        }
    }

    void drain(int fd) {
        std::vector<char> block(65536);
        while (::recv(fd, block.data(), block.size(), 0) > 0) {
        }
    }

    // One wait of a task, under an operation's hold as the modules make it
    task<WaitResult> wait_in_task(tracked_ptr<Holder> h, int dir) {
        net::detail::Operation op(h->d);
        if (!op) {
            co_return WaitResult::closed;
        }
        co_return co_await h->d.async_wait(dir);
    }
}

// The window between a call's EAGAIN and the park, both halves of it,
// made to happen rather than hoped for: the edge comes (and the reactor
// has handled it, the word says Ready) before the wait claims the
// direction, and the wait answers at once; the edge comes after the claim
// and before the publication, and the publication takes it and gives the
// claim up. An edge-triggered registration signals once: a lost Ready
// would leave the wait asleep for good (the watchdog's close ends it, as
// closed).
TEST(NetPoll_Tests, AnEdgeBetweenTheTryAndTheParkIsKept) {
    Pair p;
    int fd = p.fd();
    send1(p.peer);
    ASSERT_EQ(p.h->d.wait(Descriptor::Read), WaitResult::ready);   // the first wait registers the descriptor
    ASSERT_EQ(recv1(fd), 1);
    for (int round = 0; round < 200; ++round) {
        ASSERT_EQ(recv1(fd), -1);                                  // EAGAIN: the call tried
        send1(p.peer);                                             // the edge, before the wait
        ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Ready));
        {
            Watchdog dog(2s, [&] { p.h->d.close(); });
            EXPECT_EQ(p.h->d.wait(Descriptor::Read), WaitResult::ready);
            ASSERT_FALSE(dog.fired);
        }
        EXPECT_EQ(slot_of(fd).state[Descriptor::Read].load(), 0u);   // taken by the wait
        ASSERT_EQ(recv1(fd), 1);
    }
    for (int round = 0; round < 200; ++round) {
        ASSERT_EQ(recv1(fd), -1);
        auto& s = slot_of(fd);
        ASSERT_EQ(s.claim(Descriptor::Read), PollSlot::Claimed::ours);
        send1(p.peer);                                             // the edge, between the claim and the publication
        ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Ready | PollSlot::Claim));
        EXPECT_FALSE(s.publish(Descriptor::Read));                 // not parked: the readiness taken, the call tried again
        EXPECT_EQ(s.state[Descriptor::Read].load(), 0u);
        ASSERT_EQ(recv1(fd), 1);
    }
}

// The same window hit at random: a byte there and back a hundred
// thousand times between two tasks, and between a task and a thread,
// every read after an EAGAIN; one lost edge stops the exchange, and the
// watchdog's close ends it with an error. 30 s, 90 s under the thread
// sanitizer: 100 000 round trips take ~25 s there on an idle machine, and
// its stops of the whole process push a loaded run past 30 s
namespace {
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
    constexpr auto RoundTripWatchdog = std::chrono::seconds(90);
#else
    constexpr auto RoundTripWatchdog = std::chrono::seconds(30);
#endif
#else
    constexpr auto RoundTripWatchdog = std::chrono::seconds(30);
#endif
}

TEST(NetPoll_Tests, SingleBytesThereAndBackLoseNoWake) {
    for (int mode = 0; mode < 2; ++mode) {
        const long rounds = mode == 0 ? 100000 : 20000;
        int p[2];
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, p), 0);
        net::detail::prepare_socket(p[0]);
        net::detail::prepare_socket(p[1]);
        auto wrap = [](int fd) {
            return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(make_tracked<net::detail::SocketConn>(fd, false, net::endpoint(), net::endpoint(), sgcl::string("pair"))));
        };
        net::connection a = wrap(p[0]), b = wrap(p[1]);
        auto echo = [](net::connection c, long n) -> task<long> {
            byte x[1];
            long done = 0;
            for (; done < n; ++done) {
                auto r = co_await c.async_read(x);
                if (!r || *r != 1 || !co_await c.async_write(slice<const byte>(x, 1))) {
                    break;
                }
            }
            co_return done;
        };
        auto echoing = spawn(echo(b, rounds));
        Watchdog dog(RoundTripWatchdog, [&] { a.close(); b.close(); });
        long done = 0;
        byte x[1] = {byte('p')};
        if (mode == 0) {
            auto ping = [](net::connection c, long n) -> task<long> {
                byte x[1] = {byte('p')};
                long done = 0;
                for (; done < n; ++done) {
                    if (!co_await c.async_write(slice<const byte>(x, 1))) {
                        break;
                    }
                    auto r = co_await c.async_read(x);
                    if (!r || *r != 1) {
                        break;
                    }
                }
                co_return done;
            };
            done = spawn(ping(a, rounds)).wait();
        } else {
            for (; done < rounds; ++done) {   // the blocking form, a thread parked in the slot
                if (!a.write(slice<const byte>(x, 1))) {
                    break;
                }
                auto r = a.read(x);
                if (!r || *r != 1) {
                    break;
                }
            }
        }
        EXPECT_EQ(done, rounds);
        EXPECT_EQ(echoing.wait(), rounds);
        EXPECT_FALSE(dog.fired);
        a.close();
        b.close();
    }
}

// A close from another thread with waiters parked in both directions, one
// in each record and two behind it on the overflow list for the read: all
// five end, closed, and nothing hangs. Then the number is given to a new
// socket pair: its descriptor registers again (the slot cleared by the
// close, a new generation) and its wait is woken by its own data.
TEST(NetPoll_Tests, ACloseEndsEveryWaiterOfBothDirections) {
    for (int round = 0; round < 50; ++round) {
        int reused;
        {
            Pair p;
            int fd = p.fd();
            reused = fd;
            fill(fd);
            std::vector<task<WaitResult>> readers, writers;
            readers.push_back(spawn(wait_in_task(p.h, Descriptor::Read)));
            ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Parked));
            readers.push_back(spawn(wait_in_task(p.h, Descriptor::Read)));
            readers.push_back(spawn(wait_in_task(p.h, Descriptor::Read)));
            ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Parked | PollSlot::Overflow));
            writers.push_back(spawn(wait_in_task(p.h, Descriptor::Write)));
            ASSERT_TRUE(until(fd, Descriptor::Write, PollSlot::Parked));
            writers.push_back(spawn(wait_in_task(p.h, Descriptor::Write)));
            std::this_thread::sleep_for(round % 4 == 0 ? 5ms : 0ms);   // the second writer parked, or still on its way
            std::thread([&] { p.h->d.close(); }).join();
            Watchdog dog(5s, [] { std::abort(); });                    // a waiter the close missed would hang here
            for (auto& t : readers) {
                EXPECT_EQ(t.wait(), WaitResult::closed);
            }
            for (auto& t : writers) {
                EXPECT_EQ(t.wait(), WaitResult::closed);
            }
            EXPECT_EQ(slot_of(fd).state[Descriptor::Read].load(), 0u);
            EXPECT_EQ(slot_of(fd).state[Descriptor::Write].load(), 0u);
        }
        collector::force_collect(true);   // optional: the closed descriptors and the waiters collected under the next round
        int q[2];
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, q), 0);
        net::detail::prepare_socket(q[0]);
        net::detail::prepare_socket(q[1]);
        int mine = q[1] == reused ? 1 : q[0] == reused ? 0 : 1;   // the end that took the number, if one did
        tracked_ptr<Holder> h = make_tracked<Holder>(q[mine]);
        auto w = spawn(wait_in_task(h, Descriptor::Read));
        std::this_thread::sleep_for(1ms);
        send1(q[1 - mine]);
        {
            Watchdog dog(2s, [&] { h->d.close(); });
            EXPECT_EQ(w.wait(), WaitResult::ready);
        }
        h->d.close();
        ::close(q[1 - mine]);
    }
}

// A read and a write waiting on one descriptor at once: the peer's byte
// wakes the read and leaves the write waiting; the peer draining the
// buffer wakes the write
TEST(NetPoll_Tests, AReadAndAWriteWaitOnOneDescriptor) {
    for (int round = 0; round < 50; ++round) {
        Pair p;
        int fd = p.fd();
        fill(fd);
        Watchdog dog(5s, [&] { p.h->d.close(); });
        auto reader = spawn(wait_in_task(p.h, Descriptor::Read));
        auto writer = spawn(wait_in_task(p.h, Descriptor::Write));
        ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Parked));
        ASSERT_TRUE(until(fd, Descriptor::Write, PollSlot::Parked));
        send1(p.peer);
        EXPECT_EQ(reader.wait(), WaitResult::ready);
        std::this_thread::sleep_for(2ms);
        EXPECT_FALSE(writer.done());
        drain(p.peer);
        EXPECT_EQ(writer.wait(), WaitResult::ready);
        EXPECT_FALSE(dog.fired);
    }
}

// Several tasks waiting in one direction: eight accepting on one listener
// (the second and later on the slot's overflow list), eight connections
// dialled; every accept completes. And eight tasks reading one byte each
// from one descriptor, the bytes sent one at a time.
TEST(NetPoll_Tests, EveryWaiterInOneDirectionGetsItsTurn) {
    for (int round = 0; round < 20; ++round) {
        auto l = tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l);
        net::listener listener = *l;
        std::vector<task<bool>> accepting;
        for (int i = 0; i < 8; ++i) {
            accepting.push_back(spawn([](net::listener l) -> task<bool> {
                auto c = co_await l.async_accept();
                co_return (bool)c;
            }(listener)));
        }
        std::this_thread::sleep_for(round % 2 ? 2ms : 0ms);
        Watchdog dog(10s, [&] { listener.close(); });
        sgcl::vector<net::connection> clients;
        for (int i = 0; i < 8; ++i) {
            auto c = tcp::connect(listener.local_endpoint());
            ASSERT_TRUE(c);
            clients.push_back(*c);
        }
        for (auto& t : accepting) {
            EXPECT_TRUE(t.wait());
        }
        EXPECT_FALSE(dog.fired);
        listener.close();
    }
    for (int round = 0; round < 20; ++round) {
        Pair p;
        int fd = p.fd();
        auto read_one = [](tracked_ptr<Holder> h) -> task<bool> {
            net::detail::Operation op(h->d);
            for (;;) {
                char c;
                if (::recv(h->d.fd(), &c, 1, 0) == 1) {
                    co_return true;
                }
                if (errno != EAGAIN) {
                    co_return false;
                }
                if (co_await h->d.async_wait(Descriptor::Read) != WaitResult::ready) {
                    co_return false;
                }
            }
        };
        std::vector<task<bool>> readers;
        for (int i = 0; i < 8; ++i) {
            readers.push_back(spawn(read_one(p.h)));
        }
        Watchdog dog(10s, [&] { p.h->d.close(); });
        for (int i = 0; i < 8; ++i) {
            send1(p.peer);
            if (round % 2) {
                std::this_thread::sleep_for(100us);
            }
        }
        for (auto& t : readers) {
            EXPECT_TRUE(t.wait());
        }
        EXPECT_FALSE(dog.fired);
        EXPECT_EQ(recv1(fd), -1);   // every byte read by one of them
    }
}

// The reactor stopped under a parked wait: the wait ends, cancelled; the
// next wait starts the reactor again, registers the descriptor again in
// the new queue, and is woken by the data
TEST(NetPoll_Tests, AStopOfTheReactorEndsAParkedWaitAndTheNextRegistersAgain) {
    for (int round = 0; round < 20; ++round) {
        Pair p;
        int fd = p.fd();
        auto w = spawn(wait_in_task(p.h, Descriptor::Read));
        ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Parked));
        sgcl::async::detail::reactor_instance().stop();
        {
            Watchdog dog(5s, [&] { p.h->d.close(); });
            EXPECT_EQ(w.wait(), WaitResult::cancelled);
        }
        auto again = spawn(wait_in_task(p.h, Descriptor::Read));
        ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Parked));
        send1(p.peer);
        Watchdog dog(5s, [&] { p.h->d.close(); });
        EXPECT_EQ(again.wait(), WaitResult::ready);
    }
}

// A descriptor the reactor cannot watch: a wait on it fails with the
// reason, never answers ready, which would have the operation try its
// call again forever (EAGAIN, a wait answered at once, EAGAIN...). A
// registration the kernel refuses (here a number closed under the
// descriptor: EBADF), in both forms of the wait; and a number past the
// reactor's table, which no test can open (four million), made so by the
// test hook that lowers the table's limit: a read of a connection, in
// both forms, and of a pipe, fail with io::errc::unsupported. A watchdog
// closes them after two seconds: a loop would end as closed instead.
TEST(NetPoll_Tests, AWaitTheReactorCannotMakeFailsRatherThanLoops) {
    {
        int p[2];
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, p), 0);
        struct Unowned {
            explicit Unowned(int fd)
            : d(fd, false) {
            }

            Descriptor d;
        };
        tracked_ptr<Unowned> u = make_tracked<Unowned>(p[1]);
        ::close(p[1]);                                             // closed under the descriptor: the kernel refuses to register it
        EXPECT_EQ(u->d.wait(Descriptor::Read), WaitResult::failed);
        EXPECT_EQ(u->d.wait_failure(), std::error_code(EBADF, std::system_category()));
        auto t = spawn([](tracked_ptr<Unowned> u) -> task<WaitResult> {
            co_return co_await u->d.async_wait(Descriptor::Write);
        }(u));
        EXPECT_EQ(t.wait(), WaitResult::failed);
        EXPECT_EQ(u->d.wait_failure(), std::error_code(EBADF, std::system_category()));
        ::close(p[0]);
    }
    struct Limit {   // the table's limit lowered to the number, and given back
        explicit Limit(int fd) {
            sgcl::async::detail::poll_number_limit.store(unsigned(fd));
        }

        ~Limit() {
            sgcl::async::detail::poll_number_limit.store(sgcl::async::detail::PollChunkSize * sgcl::async::detail::PollChunkCount);
        }
    };
    {
        int p[2];
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, p), 0);
        net::detail::prepare_socket(p[0]);
        net::detail::prepare_socket(p[1]);
        net::connection c = net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(make_tracked<net::detail::SocketConn>(p[1], false, net::endpoint(), net::endpoint(), sgcl::string("pair"))));
        Limit limit(p[1]);
        Watchdog dog(2s, [&] { c.close(); });
        byte b[1];
        auto blocking = c.read(b);                                 // nothing to read: the call answers EAGAIN, the wait fails
        ASSERT_FALSE(blocking);
        EXPECT_EQ(blocking.error().code(), io::errc::unsupported);
        EXPECT_NE(std::string(blocking.error().message().c_str()).find("past the reactor's table"), std::string::npos);
        auto awaited = spawn(c.async_read(b)).wait();
        ASSERT_FALSE(awaited);
        EXPECT_EQ(awaited.error().code(), io::errc::unsupported);
        EXPECT_FALSE(dog.fired);
        c.close();
        ::close(p[0]);
    }
    {
        int p[2];
        ASSERT_EQ(::pipe(p), 0);
        ::fcntl(p[0], F_SETFL, ::fcntl(p[0], F_GETFL) | O_NONBLOCK);
        auto f = io::from_fd(p[0], "pipe");
        ASSERT_TRUE(f.is_nonblocking());
        Limit limit(p[0]);
        Watchdog dog(2s, [&] { (void)f.close(); });
        byte b[1];
        auto r = spawn(f.async_read(b)).wait();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), io::errc::unsupported);
        EXPECT_FALSE(dog.fired);
        (void)f.close();
        ::close(p[1]);
    }
}

namespace {
    // Both ends of a socket pair as connections (the second's number in
    // *fd, when asked)
    std::pair<net::connection, net::connection> conn_pair(int* fd = nullptr) {
        int p[2];
        [[maybe_unused]] int r = ::socketpair(AF_UNIX, SOCK_STREAM, 0, p);
        assert(r == 0);
        auto wrap = [](int fd) {
            net::detail::prepare_socket(fd);
            return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(make_tracked<net::detail::SocketConn>(fd, false, net::endpoint(), net::endpoint(), sgcl::string("pair"))));
        };
        if (fd) {
            *fd = p[1];
        }
        return {wrap(p[0]), wrap(p[1])};
    }
}

// A descriptor keeps one timer per direction (descriptor.h: _arm): a
// deadline moved later is served by the armed timer, which arms the rest
// when it fires early; one moved earlier gets a timer of its own at the
// next wait. A deadline moved earlier while a read waits: the read times
// out at the new one. Moved later while it waits: the read outlives the
// old one, the early fire of the armed timer arming the rest, and takes the
// data that comes after.
TEST(NetPoll_Tests, ADeadlineMovedWhileAReadWaits) {
    for (int round = 0; round < 10; ++round) {
        auto [a, b] = conn_pair();
        b.set_read_deadline(sgcl::clock::now() + 1h);
        auto t0 = std::chrono::steady_clock::now();
        auto reading = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
            byte x[1];
            co_return co_await c.async_read(x);
        }(b));
        std::this_thread::sleep_for(5ms);
        b.set_read_deadline(sgcl::clock::now() + 20ms);           // earlier than the armed timer
        auto r = reading.wait();
        auto took = std::chrono::steady_clock::now() - t0;
        ASSERT_FALSE(r);
        EXPECT_TRUE(r.error().is_timeout());
        EXPECT_GE(took, 20ms);
        EXPECT_LT(took, 2s);

        b.set_read_deadline(sgcl::clock::now() + 20ms);
        auto later = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
            byte x[1];
            co_return co_await c.async_read(x);
        }(b));
        std::this_thread::sleep_for(5ms);
        b.set_read_deadline(sgcl::clock::now() + 1h);             // later than the armed timer: it fires at 20 ms and arms the rest
        std::this_thread::sleep_for(50ms);
        EXPECT_FALSE(later.done());
        byte one[1] = {byte('x')};
        ASSERT_TRUE(a.write(slice<const byte>(one, 1)));
        auto got = later.wait();
        ASSERT_TRUE(got);
        EXPECT_EQ(*got, 1u);
        a.close();
        b.close();
    }
}

// The deadline and the data racing: a deadline a few hundred microseconds
// away and a byte sent about then, a thousand times, each read ending in
// the byte or in a timeout, never anything else and never a hang; and after
// each, a read under a deadline moved an hour away, the byte sent a moment
// later: whatever the timer of the race did as it fired, it must not end
// that read (a stale fire takes the deadline it finds, not the one it was
// armed for). One timer serves the thousand rounds' deadlines moved later,
// not a timer per wait: the heaps grow by a handful, not a thousand.
TEST(NetPoll_Tests, ATimerFiringAsAWaitEndsLeavesTheNextWaitAlone) {
    auto [a, b] = conn_pair();
    size_t before = sgcl::async::detail::timers_instance().size();
    int timeouts = 0;
    byte one[1] = {byte('x')};
    for (int round = 0; round < 1000; ++round) {
        auto gap = std::chrono::microseconds(50 * (round % 8));
        b.set_read_deadline(sgcl::clock::now() + gap);
        std::thread sender([&] {
            std::this_thread::sleep_for(std::chrono::microseconds(50 * ((round * 3) % 8)));
            (void)a.write(slice<const byte>(one, 1));
        });
        byte x[1];
        auto r = b.read(x);
        if (!r) {
            ASSERT_TRUE(r.error().is_timeout()) << r.error().message().c_str();
            ++timeouts;
        }
        sender.join();
        b.set_read_deadline(sgcl::clock::now() + 1h);
        if (!r) {
            auto again = b.read(x);                               // the byte of the round, there or on its way
            ASSERT_TRUE(again);
        }
        std::thread late([&] {
            std::this_thread::sleep_for(200us);
            (void)a.write(slice<const byte>(one, 1));
        });
        auto next = b.read(x);
        late.join();
        ASSERT_TRUE(next) << next.error().message().c_str();
    }
    EXPECT_GT(timeouts, 0);                                        // the race was run, on both sides of it
    EXPECT_LT(timeouts, 1000);
    EXPECT_LT(sgcl::async::detail::timers_instance().size(), before + 64);   // a timer per wait would have added a thousand, less the sweeps
    a.close();
    b.close();
}

// A deadline moved later at every wait, as a kept connection's idle
// deadline is: the armed timer serves them all, and a wait under it
// allocates no more than a wait under none (a timer per wait was a managed
// Timer and a root per wait). Blocking reads on this thread, the byte of
// each sent by a helper thread 200 us after the read began waiting.
TEST(NetPoll_Tests, ADeadlineMovedLaterReusesItsTimer) {
    auto [a, b] = conn_pair();
    std::atomic<int> go = {0};
    std::atomic<bool> stop = {false};
    std::thread sender([&] {
        int sent = 0;
        while (!stop.load()) {
            if (go.load() > sent) {
                std::this_thread::sleep_for(200us);
                byte one[1] = {byte('x')};
                (void)a.write(slice<const byte>(one, 1));
                ++sent;
            } else {
                std::this_thread::yield();
            }
        }
    });
    auto round = [&](bool deadline) {
        if (deadline) {
            b.set_read_deadline(sgcl::clock::now() + 1h);
        }
        go.fetch_add(1);
        byte x[1];
        auto r = b.read(x);
        ASSERT_TRUE(r);
    };
    size_t with = managed_bytes_of(3000, [&] { round(true); });
    b.set_read_deadline(time_point());                            // no deadline for the reads that compare
    size_t without = managed_bytes_of(3000, [&] { round(false); });
    EXPECT_LE(with, without + 65536) << "with " << with << " without " << without;   // a page: the granularity of the count
    stop = true;
    sender.join();
    a.close();
    b.close();
}

// A connection abandoned without a close, its idle deadline's timer armed
// (a read under a deadline of two minutes, ended by its data): the timer
// holds the descriptor weakly, so the connection is collected at the next
// cycle and its number closed by the descriptor's destructor, as with no
// timer at all; the timer's point passing (the manual clock moved past it)
// then runs out into nothing
TEST(NetPoll_Tests, AnAbandonedConnectionIsCollectedWithItsTimerArmed) {
    sgcl::async::manual_clock clock;
    clock.install();
    int fd = -1;
    weak_ptr<net::detail::SocketConn> gone;                         // the abandoned end, watched without keeping it
    off_frame([&] {
        int p[2];
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, p), 0);
        net::detail::prepare_socket(p[0]);
        net::detail::prepare_socket(p[1]);
        fd = p[1];
        tracked_ptr<net::detail::SocketConn> end = make_tracked<net::detail::SocketConn>(p[1], false, net::endpoint(), net::endpoint(), sgcl::string("pair"));
        gone = end;
        net::connection b = net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(end));
        net::connection a = net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(make_tracked<net::detail::SocketConn>(p[0], false, net::endpoint(), net::endpoint(), sgcl::string("pair"))));
        end = nullptr;
        b.set_read_deadline(sgcl::clock::now() + 120s);
        bool got = false;
        std::thread reader([&] {                                    // a thread's read: no task frame for the scheduler's queues to keep
            byte x[1];
            got = (bool)b.read(x);
        });
        ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Parked));   // waiting: the timer armed
        byte one[1] = {byte('x')};
        ASSERT_TRUE(a.write(slice<const byte>(one, 1)));
        reader.join();
        ASSERT_TRUE(got);
        a.close();                                                  // the peer closed; b abandoned, its timer armed
    });
    for (int i = 0; i < 3 && !gone.expired(); ++i) {
        collector::clear_stack();
        collector::force_collect(true);
        sgcl::async::scheduler::stop();                             // the workers' dead stacks too
    }
    EXPECT_TRUE(gone.expired());                                    // collected at the cycle, the timer's hold weak
    errno = 0;
    EXPECT_EQ(::fcntl(fd, F_GETFD), -1);                           // closed by the destructor
    EXPECT_EQ(errno, EBADF);
    clock.advance(121s);                                            // past the timer's point: nothing to call
}

// The same abandoned connection read from a task rather than a thread,
// without a deadline and with one: once the task has finished and the
// connection is let go of, it is collected and its number closed. The
// task's frame, finished, names the connection in words no destructor
// nulls; the frame was held by the slot of the worker's ring it had been
// queued on (note 250's aside: collected only from a thread)
TEST(NetPoll_Tests, AnAbandonedConnectionReadFromATaskIsCollected) {
    sgcl::async::manual_clock clock;
    clock.install();
    for (bool deadline : {false, true}) {
        int fd = -1;
        weak_ptr<net::detail::SocketConn> gone;
        off_frame([&] {
            int p[2];
            ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, p), 0);
            net::detail::prepare_socket(p[0]);
            net::detail::prepare_socket(p[1]);
            fd = p[1];
            tracked_ptr<net::detail::SocketConn> end = make_tracked<net::detail::SocketConn>(p[1], false, net::endpoint(), net::endpoint(), sgcl::string("pair"));
            gone = end;
            net::connection b = net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(end));
            net::connection a = net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(make_tracked<net::detail::SocketConn>(p[0], false, net::endpoint(), net::endpoint(), sgcl::string("pair"))));
            end = nullptr;
            if (deadline) {
                b.set_read_deadline(sgcl::clock::now() + 120s);
            }
            auto reading = spawn([](net::connection c) -> task<bool> {
                byte x[1];
                co_return (bool)co_await c.async_read(x);
            }(b));
            ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Parked));
            byte one[1] = {byte('x')};
            ASSERT_TRUE(a.write(slice<const byte>(one, 1)));
            ASSERT_TRUE(reading.wait());
            a.close();                                              // the peer closed; b abandoned
        });
        for (int i = 0; i < 3 && !gone.expired(); ++i) {
            collector::clear_stack();
            collector::force_collect(true);
            sgcl::async::scheduler::stop();                         // the workers idle: their stacks and rings cleared
        }
        EXPECT_TRUE(gone.expired()) << (deadline ? "with a deadline" : "without a deadline");
        errno = 0;
        EXPECT_EQ(::fcntl(fd, F_GETFD), -1);
        EXPECT_EQ(errno, EBADF);
    }
    clock.advance(121s);
}

// A close with the direction's timer armed: the read ends closed, the timer
// is cancelled, and its point passing (the manual clock moved past it)
// wakes nothing and touches nothing
TEST(NetPoll_Tests, ACloseWithTheTimerArmed) {
    sgcl::async::manual_clock clock;
    clock.install();
    for (int round = 0; round < 20; ++round) {
        int fd = -1;
        auto [a, b] = conn_pair(&fd);
        b.set_read_deadline(sgcl::clock::now() + 1s);
        auto reading = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
            byte x[1];
            co_return co_await c.async_read(x);
        }(b));
        ASSERT_TRUE(until(fd, Descriptor::Read, PollSlot::Parked));
        std::thread([&] { b.close(); }).join();
        auto r = reading.wait();
        ASSERT_FALSE(r);
        EXPECT_TRUE(r.error().is_closed());
        clock.advance(2s);                                         // past the cancelled timer's point
        a.close();
    }
}
