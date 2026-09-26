//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: the race of a close with an operation in progress (detail/fd.h). A
// task reads a connection a byte at a time, its data all 'A'; another
// thread closes the connection and at once opens new socket pairs with 'B'
// waiting in both directions, so that the number just given back is taken
// again. A read on a stale number would return a 'B'. Ten thousand rounds;
// run under the thread sanitizer as well. The close that leaves the
// ::close to the last operation is what makes it pass: with a ::close made
// by close() itself, the reads see a 'B' (checked by mutation).
#include "tests/types.h"
#include "sgcl/net/net.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <atomic>
#include <fcntl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    // A connection over one end of a socket pair, as the module makes one
    net::connection wrap(int fd) {
        net::detail::prepare_socket(fd);
        return net::connection(tracked_ptr<net::detail::ConnImpl>(make_tracked<net::detail::SocketConn>(fd, false, net::endpoint(), net::endpoint(), sgcl::string("pair"))));
    }

    struct Round {
        std::atomic<size_t> foreign = {0};   // bytes read that were not the connection's
        std::atomic<size_t> read = {0};
    };

    task<> read_until_closed(net::connection c, Round* round) {
        byte b[1];
        for (;;) {
            auto r = co_await c.async_read(b);
            if (!r || *r == 0) {
                co_return;
            }
            round->read.fetch_add(1, std::memory_order_relaxed);
            if (b[0] != byte('A')) {
                round->foreign.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    void fill(int fd, char c, size_t n) {
        std::vector<char> data(n, c);
        size_t done = 0;
        while (done < n) {
            ssize_t w = ::send(fd, data.data() + done, n - done, 0);
            if (w <= 0) {
                break;
            }
            done += size_t(w);
        }
    }
}

TEST(NetRace_Tests, CloseAgainstAReadInProgress) {
    const int rounds = 10000;
    size_t foreign = 0, read = 0, closed_while_reading = 0;
    for (int i = 0; i < rounds; ++i) {
        int a[2];
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, a), 0);
        fill(a[0], 'A', 512);   // enough for the reader to be inside its loop when the close comes
        net::connection c = wrap(a[1]);
        Round round;
        auto reader = spawn(read_until_closed(c, &round));
        for (int spin = 0; spin < (i % 64); ++spin) {
            std::this_thread::yield();
        }
        c.close();
        int fresh[4][2];
        for (auto& p : fresh) {   // the numbers taken again, 'B' waiting at every end
            ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, p), 0);
            fill(p[0], 'B', 64);
            fill(p[1], 'B', 64);
        }
        reader.wait();
        foreign += round.foreign;
        read += round.read;
        closed_while_reading += round.read < 512;
        for (auto& p : fresh) {
            ::close(p[0]);
            ::close(p[1]);
        }
        ::close(a[0]);
    }
    EXPECT_EQ(foreign, 0u);
    EXPECT_GT(closed_while_reading, size_t(rounds / 10));   // the close did land inside the reads, often
    EXPECT_GT(read, 0u);
}

// The mechanism itself, without a race to win: an operation in progress
// keeps the number the descriptor's across a close, no operation starts
// after it, and the last one to end gives the number back
TEST(NetRace_Tests, TheLastOperationCloses) {
    struct Holder {
        explicit Holder(int fd)
        : d(fd) {
        }

        net::detail::Descriptor d;
    };
    int p[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, p), 0);
    tracked_ptr<Holder> h = make_tracked<Holder>(p[1]);
    {
        net::detail::Operation first(h->d);
        ASSERT_TRUE(first);
        net::detail::Operation second(h->d);
        ASSERT_TRUE(second);
        int closed = -1;
        std::thread([&] { closed = h->d.close(); }).join();   // from another thread, while two run
        EXPECT_EQ(closed, 0);
        EXPECT_TRUE(h->d.closing());
        EXPECT_NE(::fcntl(p[1], F_GETFD), -1);                 // still open: the operations hold it
        int other = ::dup(0);
        EXPECT_NE(other, p[1]);                                // so the number is not given to another
        ::close(other);
        net::detail::Operation late(h->d);
        EXPECT_FALSE(late);                                    // nothing starts after the close
        EXPECT_EQ(h->d.close(), 0);                            // a second close: nothing
    }
    errno = 0;
    EXPECT_EQ(::fcntl(p[1], F_GETFD), -1);                     // the last one out closed it
    EXPECT_EQ(errno, EBADF);
    ::close(p[0]);
}

// The same from a thread in the blocking form, the close from a task
TEST(NetRace_Tests, CloseFromATaskAgainstABlockingRead) {
    const int rounds = 2000;
    size_t foreign = 0;
    for (int i = 0; i < rounds; ++i) {
        int a[2];
        ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, a), 0);
        fill(a[0], 'A', 256);
        net::connection c = wrap(a[1]);
        std::atomic<size_t> bad = 0;
        std::thread reader([&] {
            byte b[1];
            for (;;) {
                auto r = c.read(b);
                if (!r || *r == 0) {
                    return;
                }
                if (b[0] != byte('A')) {
                    ++bad;
                }
            }
        });
        spawn([](net::connection c, int spins) -> task<> {
            for (int s = 0; s < spins; ++s) {
                std::this_thread::yield();
            }
            (void)c.close();
            co_return;
        }(c, i % 32)).wait();
        int fresh[4][2];
        for (auto& p : fresh) {
            ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, p), 0);
            fill(p[0], 'B', 64);
            fill(p[1], 'B', 64);
        }
        ::close(a[0]);   // the reader that is still waiting sees the end
        reader.join();
        foreign += bad;
        for (auto& p : fresh) {
            ::close(p[0]);
            ::close(p[1]);
        }
    }
    EXPECT_EQ(foreign, 0u);
}
