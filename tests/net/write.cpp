//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: a write begun without a frame (connection.h: ConnImpl::start_write),
// what the HTTP server sends its responses with. What the socket takes at
// once ends the write there; a task goes on with the rest only when the
// socket would wait, holding the write lock to its end. Through the same
// checks as every write: a deadline passed ends it (at once, or in the
// rest), a close ends it, and another task's write never lands between the
// two parts of one.
#include "tests/types.h"
#include "sgcl/net/net.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    std::pair<net::connection, net::connection> pair_of_ends() {
        int p[2];
        [[maybe_unused]] int r = ::socketpair(AF_UNIX, SOCK_STREAM, 0, p);
        assert(r == 0);
        auto wrap = [](int fd) {
            net::detail::prepare_socket(fd);
            return net::connection(tracked_ptr<net::detail::ConnImpl>(make_tracked<net::detail::SocketConn>(fd, false, net::endpoint(), net::endpoint(), sgcl::string("pair"))));
        };
        return {wrap(p[0]), wrap(p[1])};
    }

    net::detail::ConnImpl& impl(const net::connection& c) {
        return net::detail::ConnectionAccess::impl(c);
    }

    // The peer's buffer filled: a write now would wait
    void fill(const net::connection& c) {
        std::vector<char> block(65536, 'f');
        for (;;) {
            auto s = impl(c).start_write(slice<const byte>(reinterpret_cast<const byte*>(block.data()), block.size()));
            if (s.rest) {
                s.rest.reset();   // never started: a task not awaited is dropped with nothing done
                break;
            }
            if (!s.done) {
                break;
            }
        }
    }
}

// Taken whole at once: the result there, no task
TEST(NetWrite_Tests, AWriteTheSocketTakesAtOnceHasNoTask) {
    auto [a, b] = pair_of_ends();
    std::string text = "hello";
    auto s = impl(a).start_write(slice<const byte>(reinterpret_cast<const byte*>(text.data()), text.size()));
    EXPECT_FALSE(s.rest);
    ASSERT_TRUE(s.done);
    EXPECT_EQ(*s.done, text.size());
    byte got[5];
    auto r = b.read(got);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 5u);
    a.close();
    b.close();
}

// A full buffer and a deadline: past the deadline the write fails at once
// (the check before the send); a deadline ahead, the rest waits and fails
// with the timeout when it passes
TEST(NetWrite_Tests, AFullBufferAndADeadline) {
    auto [a, b] = pair_of_ends();
    fill(a);
    std::string text = "x";
    slice<const byte> one(reinterpret_cast<const byte*>(text.data()), 1);
    a.set_write_deadline(sgcl::clock::now() - 1ms);
    auto past = impl(a).start_write(one);
    EXPECT_FALSE(past.rest);
    ASSERT_FALSE(past.done);
    EXPECT_TRUE(past.done.error().is_timeout());
    a.set_write_deadline(sgcl::clock::now() + 30ms);
    auto ahead = impl(a).start_write(one);
    ASSERT_TRUE(ahead.rest);
    auto t0 = std::chrono::steady_clock::now();
    auto r = spawn(std::move(*ahead.rest)).wait();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_timeout());
    EXPECT_GE(std::chrono::steady_clock::now() - t0, 20ms);
    a.close();
    b.close();
}

// A close: before the write, it fails at once; during the rest (a full
// buffer), the rest ends closed
TEST(NetWrite_Tests, AWriteAndAClose) {
    {
        auto [a, b] = pair_of_ends();
        fill(a);
        std::string text = "x";
        slice<const byte> one(reinterpret_cast<const byte*>(text.data()), 1);
        auto s = impl(a).start_write(one);
        ASSERT_TRUE(s.rest);
        auto rest = spawn(std::move(*s.rest));
        std::this_thread::sleep_for(5ms);
        std::thread([&] { a.close(); }).join();
        auto r = rest.wait();
        ASSERT_FALSE(r);
        EXPECT_TRUE(r.error().is_closed());
        b.close();
    }
    {
        auto [a, b] = pair_of_ends();
        a.close();
        std::string text = "x";
        auto s = impl(a).start_write(slice<const byte>(reinterpret_cast<const byte*>(text.data()), 1));
        EXPECT_FALSE(s.rest);
        ASSERT_FALSE(s.done);
        EXPECT_TRUE(s.done.error().is_closed());
        b.close();
    }
}

// A write larger than the socket's buffer, its first part taken at once and
// the rest in a task, with another task's write queued meanwhile: the other
// write lands after the whole of the first (the rest holds the write lock)
TEST(NetWrite_Tests, TheRestHoldsTheLockAgainstAnotherWrite) {
    auto [a, b] = pair_of_ends();
    std::vector<char> big(4 << 20, 'A');
    std::vector<char> small(1000, 'B');
    auto s = impl(a).start_write(slice<const byte>(reinterpret_cast<const byte*>(big.data()), big.size()));
    ASSERT_TRUE(s.rest);                                           // four megabytes do not fit at once
    auto other = spawn([](net::connection c, std::vector<char>* bytes) -> task<bool> {
        auto r = co_await c.async_write(slice<const byte>(reinterpret_cast<const byte*>(bytes->data()), bytes->size()));
        co_return (bool)r;
    }(a, &small));
    auto rest = spawn(std::move(*s.rest));
    std::vector<char> got;
    std::thread reader([&] {
        std::vector<char> block(65536);
        while (got.size() < big.size() + small.size()) {
            auto r = b.read(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
            if (!r || *r == 0) {
                break;
            }
            got.insert(got.end(), block.begin(), block.begin() + long(*r));
        }
    });
    auto first = rest.wait();
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, big.size());
    EXPECT_TRUE(other.wait());
    reader.join();
    ASSERT_EQ(got.size(), big.size() + small.size());
    size_t as = 0;
    while (as < got.size() && got[as] == 'A') {
        ++as;
    }
    EXPECT_EQ(as, big.size());                                     // the A's whole, then the B's
    a.close();
    b.close();
}
