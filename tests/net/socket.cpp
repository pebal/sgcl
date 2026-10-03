//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: TCP, unix and UDP sockets on this machine. Echo of 1 B to 16 MB in
// both forms, half-close, a close from another task or thread ending a
// read and an accept, deadlines on the manual clock, the pause of accept
// when the descriptors run out, a write to a closed peer (SIGPIPE), a unix
// socket's file, a datagram cut to the buffer.
#include "tests/types.h"
#include "sgcl/net/net.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    using Steady = std::chrono::steady_clock;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    sgcl::string address_of(const net::listener& l) {
        return l.local_endpoint().to_string();
    }

    // A connected pair over the loopback: the client's end, the server's end
    pair<net::connection, net::connection> tcp_pair() {
        auto l = tcp::listen("127.0.0.1:0");
        EXPECT_TRUE(l) << l.error().message();
        auto client = tcp::connect(l->local_endpoint());
        EXPECT_TRUE(client) << client.error().message();
        auto server = l->accept();
        EXPECT_TRUE(server) << server.error().message();
        l->close();
        return pair<net::connection, net::connection>(*client, *server);
    }

    // How an operation on another thread ended, in plain words: a result
    // holds strings, which live on the stack of the thread that made them
    struct Outcome {
        std::atomic<bool> ended = false;
        std::atomic<bool> ok = false;
        std::atomic<bool> closed = false;
        std::atomic<bool> timeout = false;

        template<class R>
        void set(const R& r) {
            ok = (bool)r;
            closed = !r && r.error().is_closed();
            timeout = !r && r.error().is_timeout();
            ended = true;
        }
    };

    vector<byte> pattern(size_t n, unsigned seed) {
        vector<byte> v;
        v.resize(n);
        uint32_t x = seed * 2654435761u + 1;
        for (size_t i = 0; i < n; ++i) {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            v[i] = byte(x);
        }
        return v;
    }

    task<> echo(net::connection c) {
        (void)co_await c.async_copy_to(c);
        (void)c.close();
    }

    task<> echo_server(net::listener l, size_t connections) {
        for (size_t i = 0; i < connections; ++i) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            go(echo(*c));
        }
    }

    task<size_t> write_then_close_write(net::connection c, vector<byte> data) {
        auto w = co_await c.async_write(data.as_slice());
        (void)c.close_write();
        co_return w ? *w : 0;
    }

    // The client's side of an echo, in a task: the writer spawned, the
    // reader here, everything back compared
    task<bool> async_round_trip(net::connection c, size_t n, unsigned seed) {
        auto data = pattern(n, seed);
        auto writer = spawn(write_then_close_write(c, data));
        auto back = co_await c.async_read_all();
        size_t written = co_await writer;
        (void)c.close();
        co_return back && written == n && back->size() == n && std::equal(back->begin(), back->end(), data.begin());
    }
}

TEST(NetSocket_Tests, EchoFromOneByteToSixteenMegabytes) {
    const size_t sizes[] = {1, 2, 3, 7, 8191, 8192, 8193, 65539, 1 << 20, 16 << 20};
    auto l = tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l) << l.error().message();
    auto server = spawn(echo_server(*l, 2 * std::size(sizes)));
    unsigned seed = 1;
    for (size_t n : sizes) {
        // async on both sides
        auto c = tcp::connect(address_of(*l));
        ASSERT_TRUE(c) << c.error().message();
        EXPECT_TRUE(spawn(async_round_trip(*c, n, seed++)).wait()) << n;
        // blocking on this side: a thread writes, this one reads
        auto d = tcp::connect(address_of(*l));
        ASSERT_TRUE(d) << d.error().message();
        auto data = pattern(n, seed++);
        std::thread writer([&] {
            auto w = d->write(data.as_slice());
            EXPECT_TRUE(w && *w == n);
            d->close_write();
        });
        auto back = d->read_all();
        writer.join();
        ASSERT_TRUE(back) << back.error().message();
        ASSERT_EQ(back->size(), n);
        EXPECT_TRUE(std::equal(back->begin(), back->end(), data.begin())) << n;
        d->close();
    }
    server.wait();
    l->close();
}

TEST(NetSocket_Tests, ReadFullAndPiecesAcrossWrites) {
    auto p = tcp_pair();
    net::connection a = p.first, b = p.second;
    std::thread writer([&] {
        const char* pieces[] = {"a", "bc", "def", "ghijklm"};   // written in pieces of 1, 2, 3, 7
        for (auto p : pieces) {
            a.write(sgcl::string(p));
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    byte buf[13];
    auto r = b.read_full(buf);
    writer.join();
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 13u);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(buf), 13), "abcdefghijklm");
    a.close();
    auto end = b.read(buf);   // the end of the stream is a read of 0
    ASSERT_TRUE(end);
    EXPECT_EQ(*end, 0u);
    auto none = b.read_full(buf);   // the end before the first byte: 0, as Go's ReadFull gives io.EOF
    ASSERT_TRUE(none);
    EXPECT_EQ(*none, 0u);
    b.close();
}

TEST(NetSocket_Tests, HalfClose) {
    auto [client, server] = tcp_pair();
    ASSERT_TRUE(client.write("question"));
    ASSERT_TRUE(client.close_write());
    auto asked = server.read_all_text();   // to the end: the client closed its writing half
    ASSERT_TRUE(asked);
    EXPECT_EQ(text(*asked), "question");
    ASSERT_TRUE(server.write("answer"));   // the other half still open
    server.close();
    auto answer = client.read_all_text();
    ASSERT_TRUE(answer);
    EXPECT_EQ(text(*answer), "answer");
    auto again = client.write("more");     // this side's writing half is closed
    EXPECT_FALSE(again);
    client.close();
    EXPECT_TRUE(client.is_closed());
    EXPECT_TRUE(client.close());           // a second close: nothing, no error
    auto after = client.read_all();
    ASSERT_FALSE(after);
    EXPECT_TRUE(after.error().is_closed());
}

TEST(NetSocket_Tests, LinesAndTheirBound) {
    auto [client, server] = tcp_pair();
    ASSERT_TRUE(client.write("first\r\nsecond\nthird"));
    client.close_write();
    auto l1 = server.read_line();
    ASSERT_TRUE(l1 && *l1);
    EXPECT_EQ(text(**l1), "first");
    byte b[3];
    auto r = server.read(b);   // read takes from the line buffer first
    ASSERT_TRUE(r);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(b), *r), std::string_view("sec").substr(0, *r));
    auto rest = server.read_all_text();
    ASSERT_TRUE(rest);
    EXPECT_EQ(text(sgcl::string(std::string_view(reinterpret_cast<const char*>(b), *r))) + text(*rest), "second\nthird");
    server.close();
    client.close();

    auto [c2, s2] = tcp_pair();
    s2.set_max_line(16);
    ASSERT_TRUE(c2.write(sgcl::string(std::string(40, 'x') + "\n")));
    auto long_line = s2.read_line();
    ASSERT_FALSE(long_line);
    EXPECT_EQ(long_line.error().code(), io::errc::line_too_long);
    c2.close();
    s2.close();
}

TEST(NetSocket_Tests, AsyncLinesAndStreamView) {
    auto [client, server] = tcp_pair();
    auto t = spawn([](net::connection c) -> task<std::string> {
        std::string out;
        while (auto line = co_await c.async_read_line()) {
            if (!*line) {
                break;
            }
            out += text(**line) + "|";
        }
        co_return out;
    }(server));
    ASSERT_TRUE(client.write("one\ntwo\nthree\n"));
    client.close();
    EXPECT_EQ(t.wait(), "one|two|three|");
    // the io view: a buffered_reader over it, as over any stream
    auto [c2, s2] = tcp_pair();
    ASSERT_TRUE(c2.write("x\ny\n"));
    c2.close();
    io::buffered_reader r(s2);
    auto x = r.read_line();
    ASSERT_TRUE(x && *x);
    EXPECT_EQ(std::string_view(**x), "x");
    io::writer s = s2;                // the connection as a stream: its close is the connection's
    EXPECT_FALSE(s2.is_closed());
    (void)s.close();
    EXPECT_TRUE(s2.is_closed());   // the same connection
}

TEST(NetSocket_Tests, WritesFromTwoTasksLandWhole) {
    auto [client, server] = tcp_pair();
    const size_t n = 1 << 20;
    auto writer = [](net::connection c, char ch, size_t n) -> task<bool> {
        std::string s(n, ch);
        co_return (bool)co_await c.async_write(sgcl::string(s));
    };
    auto a = spawn(writer(client, 'a', n));
    auto b = spawn(writer(client, 'b', n));
    auto reader = spawn([](net::connection c, size_t total) -> task<std::string> {
        std::string out;
        vector<byte> buf;
        buf.resize(65536);
        while (out.size() < total) {
            auto r = co_await c.async_read(buf.as_slice());
            if (!r || *r == 0) {
                break;
            }
            out.append(reinterpret_cast<const char*>(buf.data()), *r);
        }
        co_return out;
    }(server, 2 * n));
    EXPECT_TRUE(a.wait());
    EXPECT_TRUE(b.wait());
    auto got = reader.wait();
    ASSERT_EQ(got.size(), 2 * n);
    size_t runs = 1;
    for (size_t i = 1; i < got.size(); ++i) {
        runs += got[i] != got[i - 1];
    }
    EXPECT_EQ(runs, 2u);   // one write whole, then the other
    client.close();
    server.close();
}

TEST(NetSocket_Tests, CloseFromAnotherTaskEndsARead) {
    auto [client, server] = tcp_pair();
    auto reader = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte b[16];
        co_return co_await c.async_read(b);
    }(server));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));   // the read waiting on the reactor
    EXPECT_FALSE(reader.done());
    ASSERT_TRUE(server.close());
    auto r = reader.wait();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_closed()) << r.error().message();
    client.close();
}

TEST(NetSocket_Tests, CloseFromAnotherThreadEndsABlockingRead) {
    auto p = tcp_pair();
    net::connection client = p.first, server = p.second;
    Outcome out;
    std::thread reader([&] {
        byte b[16];
        out.set(server.read(b));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(out.ended);
    server.close();
    reader.join();
    EXPECT_FALSE(out.ok);
    EXPECT_TRUE(out.closed);
    client.close();
}

TEST(NetSocket_Tests, CloseEndsAnAccept) {
    auto l = tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto accepting = spawn(l->async_accept());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(accepting.done());
    ASSERT_TRUE(l->close());
    auto r = accepting.wait();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_closed());
    // the blocking form, from a thread
    auto l2 = tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l2);
    Outcome out;
    net::listener second = *l2;
    std::thread t([&] { out.set(second.accept()); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    l2->close();
    t.join();
    EXPECT_FALSE(out.ok);
    EXPECT_TRUE(out.closed);
    EXPECT_TRUE(l2->is_closed());
    auto after = l2->accept();
    ASSERT_FALSE(after);
    EXPECT_TRUE(after.error().is_closed());
}

TEST(NetSocket_Tests, DeadlinesOnTheManualClock) {
    auto p = tcp_pair();
    net::connection client = p.first, server = p.second;
    sgcl::async::manual_clock clock;
    clock.install();
    // a read waiting past its deadline
    server.set_read_deadline(clock.now() + 5s);
    EXPECT_EQ(server.read_deadline(), clock.now() + 5s);   // the getters (DESIGN 328)
    EXPECT_EQ(server.write_deadline(), time_point());
    auto reader = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte b[16];
        co_return co_await c.async_read(b);
    }(server));
    clock.advance(4s);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(reader.done());
    clock.advance(1s);
    auto r = reader.wait();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_timeout()) << r.error().message();
    // past the deadline nothing is read, though the data is there
    ASSERT_TRUE(client.write("data"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    byte b[16];
    auto late = server.read(b);
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_timeout());
    // removed: the data is read
    server.set_read_deadline(time_point());
    auto now = server.read(b);
    ASSERT_TRUE(now);
    EXPECT_EQ(*now, 4u);
    // a deadline moved while a read waits: extended, then removed, then the data comes
    server.set_read_deadline(clock.now() + 1s);
    auto waiting = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte b[16];
        co_return co_await c.async_read(b);
    }(server));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    server.set_read_deadline(clock.now() + 10s);
    clock.advance(2s);                          // past the first deadline, not the second
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(waiting.done());
    server.set_read_deadline(time_point());
    clock.advance(20s);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(waiting.done());
    ASSERT_TRUE(client.write("late"));
    auto got = waiting.wait();
    ASSERT_TRUE(got) << got.error().message();
    EXPECT_EQ(*got, 4u);
    // a deadline in the past set from another task ends a read at once (Go's idiom to unblock one)
    auto blocked = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte b[16];
        co_return co_await c.async_read(b);
    }(server));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    server.set_read_deadline(clock.now());
    auto cut = blocked.wait();
    ASSERT_FALSE(cut);
    EXPECT_TRUE(cut.error().is_timeout());
    // the blocking form on a thread, the same clock
    server.set_read_deadline(clock.now() + 3s);
    Outcome out;
    std::thread t([&] {
        byte b[16];
        out.set(server.read(b));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    clock.advance(2s);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(out.ended);
    clock.advance(1s);
    t.join();
    EXPECT_FALSE(out.ok);
    EXPECT_TRUE(out.timeout);
    // a write deadline: the peer reads nothing, the buffers fill, the write waits and times out
    client.set_write_deadline(clock.now() + 1s);
    auto flood = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        std::string big(64 << 20, 'z');
        co_return co_await c.async_write(sgcl::string(big));
    }(client));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    clock.advance(1s);
    auto w = flood.wait();
    ASSERT_FALSE(w);
    EXPECT_TRUE(w.error().is_timeout());
    clock.uninstall();
    client.close();
    server.close();
    sgcl::async::scheduler::stop();
}

// accept failing with EMFILE pauses and tries again rather than spin or
// fail. What happens to the connection it could not take differs: Linux
// leaves it in the backlog, macOS drops it (the next accept is EAGAIN), so
// the test connects once more after the descriptors come back, and the
// accept returns one of the two.
TEST(NetSocket_Tests, AcceptPausesWhenDescriptorsRunOut) {
    auto l = tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto warm = spawn(l->async_accept());   // the reactor and the timers started while there are descriptors
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto first = tcp::connect(l->local_endpoint());
    ASSERT_TRUE(first);
    auto warmed = warm.wait();
    ASSERT_TRUE(warmed);
    sgcl::async::sleep(1ms).wait();
    auto pending = tcp::connect(l->local_endpoint());   // in the backlog
    ASSERT_TRUE(pending);
    rlimit saved;
    ASSERT_EQ(::getrlimit(RLIMIT_NOFILE, &saved), 0);
    int probe = ::dup(0);
    ASSERT_GE(probe, 0);
    ::close(probe);
    rlimit low = saved;
    low.rlim_cur = rlim_t(probe + 64);
    ASSERT_EQ(::setrlimit(RLIMIT_NOFILE, &low), 0);
    std::vector<int> filler;
    for (;;) {
        int fd = ::dup(0);
        if (fd < 0) {
            break;
        }
        filler.push_back(fd);
    }
    ASSERT_EQ(errno, EMFILE);
    auto accepting = spawn(l->async_accept());
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    EXPECT_FALSE(accepting.done());         // EMFILE: pausing, not an error
    for (int fd : filler) {
        ::close(fd);
    }
    ::setrlimit(RLIMIT_NOFILE, &saved);
    auto late = tcp::connect(l->local_endpoint());
    ASSERT_TRUE(late);
    auto r = accepting.wait();
    ASSERT_TRUE(r) << r.error().message();
    net::connection client = r->remote_endpoint() == pending->local_endpoint() ? *pending : *late;
    EXPECT_TRUE(r->remote_endpoint() == pending->local_endpoint() || r->remote_endpoint() == late->local_endpoint());
    ASSERT_TRUE(client.write("ok"));
    byte b[2];
    auto got = r->read_full(b);
    ASSERT_TRUE(got);
    EXPECT_EQ(*got, 2u);
    r->close();
    late->close();
    pending->close();
    first->close();
    warmed->close();
    l->close();
}

TEST(NetSocket_Tests, WriteToAClosedPeerIsAnErrorNotASignal) {
    ASSERT_EQ(std::signal(SIGPIPE, SIG_DFL), SIG_DFL);   // the default action kills the process: the test is that it does not come
    auto [client, server] = tcp_pair();
    server.close();
    std::string chunk(65536, 'p');
    expected<size_t, io::error> w;
    for (int i = 0; i < 1000 && (w = client.write(sgcl::string(chunk))); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_FALSE(w);
    auto code = w.error().code();
    EXPECT_TRUE(code == std::errc::broken_pipe || code == std::errc::connection_reset) << w.error().message();
    client.close();
}

TEST(NetSocket_Tests, ErrorsOfConnectAndListen) {
    auto l = tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto port = l->local_endpoint().port();
    l->close();   // nothing listens there now
    auto refused = tcp::connect(net::endpoint(ip_address::loopback_v4(), port));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), std::errc::connection_refused);
    EXPECT_EQ(text(refused.error().message()), "dial tcp 127.0.0.1:" + std::to_string(port) + ": " + std::system_category().message(ECONNREFUSED));
    auto by_name = tcp::connect(sgcl::string("127.0.0.1:") + sgcl::to_string(port));
    ASSERT_FALSE(by_name);
    EXPECT_EQ(by_name.error().code(), std::errc::connection_refused);
    for (auto bad : {"no-port", "1.2.3.4:", "1.2.3.4:99999", "::1:80", "[::1]80", "host:http"}) {
        auto r = tcp::connect(bad);
        ASSERT_FALSE(r) << bad;
        EXPECT_EQ(r.error().code(), net::errc::invalid_address) << bad;
        auto s = tcp::listen(bad);
        ASSERT_FALSE(s) << bad;
        EXPECT_EQ(s.error().code(), net::errc::invalid_address) << bad;
    }
    auto unknown = tcp::connect("nothing.invalid:80");
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error().code(), net::errc::host_not_found);
    auto taken = tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(taken);
    auto again = tcp::listen(address_of(*taken));
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code(), std::errc::address_in_use);
    taken->close();
}

TEST(NetSocket_Tests, DualStackAndReusePort) {
    auto l = tcp::listen(":0");
    ASSERT_TRUE(l) << l.error().message();
    auto port = l->local_endpoint().port();
    EXPECT_EQ(l->local_endpoint().address(), ip_address::any_v6());
    auto server = spawn(echo_server(*l, 3));
    for (auto host : {"127.0.0.1", "[::1]", ""}) {   // both families on one socket; no host is this machine
        auto c = tcp::connect(sgcl::string(host) + ":" + sgcl::to_string(port));
        ASSERT_TRUE(c) << host << ": " << c.error().message();
        EXPECT_TRUE(c->remote_endpoint().address().is_loopback());
        EXPECT_TRUE(spawn(async_round_trip(*c, 100, 7)).wait()) << host;
    }
    server.wait();
    l->close();
    auto a = tcp::listen("127.0.0.1:0", net::reuse_port);
    ASSERT_TRUE(a);
    auto b = tcp::listen(address_of(*a), net::reuse_port);
    ASSERT_TRUE(b) << b.error().message();
    a->close();
    b->close();
    auto named = tcp::listen("localhost:0");   // a name: its first IPv4 address
    ASSERT_TRUE(named) << named.error().message();
    EXPECT_EQ(named->local_endpoint().address(), ip_address::loopback_v4());
    named->close();
    auto async_named = spawn(tcp::async_listen("localhost:0"));
    auto al = async_named.wait();
    ASSERT_TRUE(al);
    EXPECT_TRUE(al->local_endpoint().address().is_loopback());
    al->close();
}

TEST(NetSocket_Tests, OptionsAndAddresses) {
    auto [client, server] = tcp_pair();
    EXPECT_EQ(client.remote_endpoint(), server.local_endpoint());
    EXPECT_EQ(client.local_endpoint(), server.remote_endpoint());
    EXPECT_TRUE(client.path().empty());
    EXPECT_TRUE(client.set_no_delay(false));
    EXPECT_TRUE(client.set_no_delay(true));
    EXPECT_TRUE(client.set_keep_alive(30s));
    EXPECT_TRUE(client.set_keep_alive(0s));
    net::connection copy = client;   // a handle: the same connection
    EXPECT_EQ(copy, client);
    EXPECT_NE(copy, server);
    copy.close();
    EXPECT_TRUE(client.is_closed());
    auto closed = client.set_no_delay(true);
    ASSERT_FALSE(closed);
    EXPECT_TRUE(closed.error().is_closed());
    server.close();
}

TEST(NetSocket_Tests, UnixDomainWithItsFile) {
    auto dir = io::make_temp_dir({}, "sgcl-net-*");
    ASSERT_TRUE(dir);
    auto path = io::path::join(*dir, "s.sock");
    auto l = unix_domain::listen(path);
    ASSERT_TRUE(l) << l.error().message();
    EXPECT_EQ(l->path(), path);
    EXPECT_TRUE(io::exists(path));
    auto twice = unix_domain::listen(path);   // something is at the path already
    ASSERT_FALSE(twice);
    EXPECT_EQ(twice.error().code(), std::errc::address_in_use);
    auto server = spawn(echo_server(*l, 2));
    auto c = unix_domain::connect(path);
    ASSERT_TRUE(c) << c.error().message();
    EXPECT_EQ(c->path(), path);
    EXPECT_FALSE(c->local_endpoint().is_valid());
    EXPECT_TRUE(spawn(async_round_trip(*c, 1 << 20, 3)).wait());
    auto d = spawn(unix_domain::async_connect(path)).wait();
    ASSERT_TRUE(d);
    EXPECT_TRUE(spawn(async_round_trip(*d, 5, 4)).wait());
    EXPECT_FALSE(d->set_no_delay(true));   // TCP only
    server.wait();
    ASSERT_TRUE(l->close());
    EXPECT_FALSE(io::exists(path));       // the listener's close removed its file
    auto gone = unix_domain::connect(path);
    ASSERT_FALSE(gone);
    EXPECT_TRUE(gone.error().is_not_found());
    auto too_long = unix_domain::listen(io::path::join(*dir, sgcl::string(std::string(200, 'x'))));
    ASSERT_FALSE(too_long);
    EXPECT_EQ(too_long.error().code(), net::errc::invalid_address);
    io::remove_all(*dir);
}

TEST(NetSocket_Tests, UdpDatagramsAndTruncation) {
    auto server = udp::bind("127.0.0.1:0");
    ASSERT_TRUE(server) << server.error().message();
    auto client = udp::connect(server->local_endpoint().to_string());
    ASSERT_TRUE(client) << client.error().message();
    EXPECT_EQ(client->remote_endpoint(), server->local_endpoint());
    auto data = pattern(100, 9);
    ASSERT_EQ(*client->send(data.as_slice()), 100u);
    byte small[10];
    auto d = server->receive_from(small);   // a datagram longer than the buffer: cut, and said so
    ASSERT_TRUE(d) << d.error().message();
    EXPECT_EQ(d->size, 10u);
    EXPECT_TRUE(d->truncated);
    EXPECT_EQ(d->from, client->local_endpoint());
    EXPECT_TRUE(std::equal(small, small + 10, data.begin()));
    auto sent = server->send_to(data.as_slice().first(20), d->from);
    ASSERT_TRUE(sent) << sent.error().message();
    ASSERT_EQ(*sent, 20u);
    byte back[64];
    auto n = client->receive(back);
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, 20u);
    // async forms; a whole datagram is not cut
    auto t = spawn([](net::udp::socket s) -> task<expected<net::udp::datagram, io::error>> {
        byte b[64];
        co_return co_await s.async_receive_from(b);
    }(*server));
    ASSERT_TRUE(spawn(client->async_send(data.as_slice().first(30))).wait());
    auto got = t.wait();
    ASSERT_TRUE(got);
    EXPECT_EQ(got->size, 30u);
    EXPECT_FALSE(got->truncated);
    auto awaited = spawn(udp::async_connect(server->local_endpoint().to_string())).wait();   // the connect of a task
    ASSERT_TRUE(awaited) << awaited.error().message();
    EXPECT_EQ(awaited->remote_endpoint(), server->local_endpoint());
    // a deadline, and a close that ends a receive
    server->set_read_deadline(sgcl::clock::now() - 1s);
    EXPECT_NE(server->read_deadline(), time_point());
    EXPECT_EQ(server->write_deadline(), time_point());
    auto late = server->receive_from(back);
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_timeout());
    server->set_read_deadline(time_point());
    auto waiting = spawn(server->async_receive_from(back));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    server->close();
    auto cut = waiting.wait();
    ASSERT_FALSE(cut);
    EXPECT_TRUE(cut.error().is_closed());
    // dual stack: an IPv4 peer of a socket bound to every address
    auto any = udp::bind(":0");
    ASSERT_TRUE(any);
    auto v4 = udp::connect(sgcl::string("127.0.0.1:") + sgcl::to_string(any->local_endpoint().port()));
    ASSERT_TRUE(v4);
    ASSERT_TRUE(v4->send(data.as_slice().first(5)));
    auto from4 = any->receive_from(back);
    ASSERT_TRUE(from4);
    EXPECT_TRUE(from4->from.address().is_v4());   // reported unmapped
    ASSERT_TRUE(any->send_to(data.as_slice().first(5), from4->from));   // and answered through the mapping
    EXPECT_EQ(*v4->receive(back), 5u);
    any->close();
    v4->close();
    client->close();
}

// A limit or a deadline at the clock's end is no limit: now + duration::max()
// saturates at time_point::max(), where a chrono sum would wrap into the past
// and time out at once
TEST(NetSocket_Tests, LimitsAtTheClocksEnd) {
    auto l = tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto server = spawn(echo_server(*l, 3));
    auto a = tcp::connect(address_of(*l), duration::max());
    ASSERT_TRUE(a) << a.error().message();
    auto b = tcp::connect(address_of(*l), duration(std::chrono::hours(24 * 365 * 300)));
    ASSERT_TRUE(b) << b.error().message();
    auto c = spawn(tcp::async_connect(address_of(*l), duration::max())).wait();
    ASSERT_TRUE(c) << c.error().message();
    a->set_deadline(sgcl::clock::now() + duration::max());
    ASSERT_TRUE(a->write("far"));
    byte got[3];
    auto r = a->read_full(got);
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(*r, 3u);
    a->close();
    b->close();
    c->close();
    server.wait();
    l->close();
}

// An empty buffer takes the next datagram and says whether it had bytes;
// macOS alone would answer with a datagram of nothing and keep the real one
TEST(NetSocket_Tests, UdpIntoAnEmptyBuffer) {
    auto server = udp::bind("127.0.0.1:0");
    ASSERT_TRUE(server);
    auto client = udp::connect(server->local_endpoint().to_string());
    ASSERT_TRUE(client);
    byte big[100] = {};
    ASSERT_TRUE(client->send(slice<const byte>(big).first(0)));   // a datagram of nothing
    ASSERT_TRUE(client->send(big));
    ASSERT_TRUE(client->send(slice<const byte>(big).first(2)));
    slice<byte> none;
    auto empty = server->receive_from(none);
    ASSERT_TRUE(empty) << empty.error().message();
    EXPECT_EQ(empty->size, 0u);
    EXPECT_FALSE(empty->truncated);
    auto cut = server->receive_from(none);
    ASSERT_TRUE(cut);
    EXPECT_EQ(cut->size, 0u);
    EXPECT_TRUE(cut->truncated);           // the hundred bytes, taken and cut to nothing
    byte buf[100];
    auto next = server->receive_from(buf);
    ASSERT_TRUE(next);
    EXPECT_EQ(next->size, 2u);             // the one after it: the hundred are not read twice
    server->close();
    client->close();
}

TEST(NetSocket_Tests, ZonesAtTheSocket) {
    auto huge = tcp::connect(net::endpoint(*ip_address::parse("fe80::1%99999999999"), 80));   // past any interface index, not wrapped to another
    ASSERT_FALSE(huge);
    EXPECT_EQ(huge.error().code(), net::errc::invalid_address);
}

// A stream made of a connection binds the connection, not the handle: it
// reads and writes on after the handles, locals of a scope, are gone (it
// held the handle's address before, a stack address with no owner)
TEST(NetSocket_Tests, AStreamOutlivesTheHandle) {
    static_assert(sizeof(net::connection) == sizeof(sgcl::tracked_ptr<void>));
    io::reader r;
    io::writer w;
    {
        auto [c, s] = tcp_pair();
        r = io::reader(s);
        w = io::writer(c);
    }
    std::string noise(64, 'n');   // the stack where the handles were, written over
    sgcl::collector::force_collect(true);
    ASSERT_TRUE(w.write(std::string("still here")));
    ASSERT_TRUE(w.close());
    EXPECT_EQ(std::string_view(*r.read_all_text()), "still here");
    EXPECT_TRUE(r.close());
    EXPECT_FALSE(io::reader(net::connection()));
}

// A connection held where a tracked word may not lie (a std container here,
// a global alike): rooted<net::connection>, the handle in a managed object
// under a root; a copy of the rooted shares it, the state lives through
// collections with the stack that made it written over
// A connection takes part in the atomics by its word (core/detail/handle_word.h):
// load, store and exchange by identity
static_assert(sgcl::req::handle<net::connection>);
static_assert(sgcl::req::handle<net::listener>);
static_assert(sgcl::req::handle<net::udp::socket>);
// The handles are made by the module: no public constructor takes the object
// inside, a type of detail
static_assert(!std::is_constructible_v<net::connection, const sgcl::tracked_ptr<net::detail::ConnImpl>&>);
static_assert(!std::is_constructible_v<net::listener, const sgcl::tracked_ptr<net::detail::ListenerImpl>&>);
static_assert(!std::is_constructible_v<net::udp::socket, const sgcl::tracked_ptr<net::detail::UdpImpl>&>);

TEST(NetSocket_Tests, AnAtomicUdpSocketByIdentity) {
    auto a = net::udp::bind("127.0.0.1:0");
    auto b = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(a && b);
    sgcl::atomic<net::udp::socket> current;
    current.store(*a);
    EXPECT_TRUE(current.load() == *a);
    EXPECT_TRUE(current.exchange(*b) == *a);
    EXPECT_TRUE(current.load() == *b);
    ASSERT_TRUE(a->send_to(sgcl::as_bytes(sgcl::string("x").as_slice()), b->local_endpoint()));
    std::byte buffer[8];
    auto got = current.load().receive_from(buffer);
    ASSERT_TRUE(got);
    EXPECT_EQ(got->size, 1u);
    EXPECT_EQ(got->from, a->local_endpoint());
    EXPECT_TRUE(a->close());
    EXPECT_TRUE(b->close());
}

TEST(NetSocket_Tests, AnAtomicConnectionByIdentity) {
    auto [c, s] = tcp_pair();
    sgcl::atomic<net::connection> current;
    current.store(c);
    EXPECT_TRUE(current.load() == c);
    EXPECT_TRUE(current.exchange(s) == c);
    EXPECT_TRUE(current.load() == s);
    ASSERT_TRUE(c.write(sgcl::string("x")));
    ASSERT_TRUE(c.close());
    auto got = current.load().read_all_text();
    ASSERT_TRUE(got);
    EXPECT_EQ(text(got.value()), "x");
    EXPECT_TRUE(s.close());
}

TEST(NetSocket_Tests, ARootedConnection) {
    static_assert(!std::is_convertible_v<rooted<net::connection>, net::connection>);
    std::vector<rooted<net::connection>> ends;
    off_frame([&] {
        auto [c, s] = tcp_pair();
        ends.emplace_back(c);
        ends.emplace_back(s);
        EXPECT_TRUE(*ends[0] == c);
    });
    std::string noise(64, 'n');
    collector::clear_stack();
    collector::force_collect(true);
    rooted<net::connection> client = ends[0];
    EXPECT_EQ(client.get(), ends[0].get());
    ASSERT_TRUE(client->write(sgcl::string("through the root")));
    ASSERT_TRUE(client->close());
    auto got = ends[1]->read_all_text();
    ASSERT_TRUE(got);
    EXPECT_EQ(text(got.value()), "through the root");
    EXPECT_TRUE(ends[1]->close());
}

namespace {
    // A file of n pattern bytes in the temporary directory, removed with the value
    struct TempFile {
        std::string path;
        std::vector<unsigned char> data;

        TempFile(size_t n, unsigned seed)
        : path((std::filesystem::temp_directory_path() / ("sgcl_send_file_" + std::to_string(::getpid()) + "_" + std::to_string(seed))).string()) {
            auto p = pattern(n, seed);
            data.assign(reinterpret_cast<const unsigned char*>(p.data()), reinterpret_cast<const unsigned char*>(p.data()) + p.size());
            EXPECT_TRUE(io::write_file(sgcl::string(path), p.as_slice()));
        }

        ~TempFile() {
            std::filesystem::remove(path);
        }

        bool same(const vector<byte>& got, size_t from) const {
            return got.size() == data.size() - from && std::memcmp(got.data(), data.data() + from, got.size()) == 0;
        }
    };

    task<expected<vector<byte>, io::error>> read_to_end(net::connection c) {
        co_return co_await c.async_read_all();
    }

    task<expected<size_t, io::error>> copy_in_task(net::connection c, io::file from) {
        co_return co_await io::async_copy(c, from);
    }
}

// io::copy of a file to a TCP connection goes by sendfile (the connection's
// read_from): from the file's position to its end, a file far larger than
// the socket's buffers, so the call is cut short and waits for room again
// and again; the bytes are the file's, the position moves to the end
TEST(NetSocket_Tests, CopyOfAFileToTcpGoesBySendfile) {
    TempFile f(24 << 20, 7);
    for (bool in_task : {false, true}) {
        SCOPED_TRACE(in_task ? "async_copy" : "copy");
        auto [client, server] = tcp_pair();
        auto file = io::open(sgcl::string(f.path));
        ASSERT_TRUE(file);
        ASSERT_TRUE(file->seek(1000));
        auto reader = spawn(read_to_end(server));
        expected<size_t, io::error> sent;
        if (in_task) {
            sent = spawn(copy_in_task(client, *file)).wait();
        } else {
            sent = io::copy(client, *file);
        }
        ASSERT_TRUE(sent) << sent.error().message();
        EXPECT_EQ(*sent, f.data.size() - 1000);
        EXPECT_EQ(*file->seek(0, io::seek_from::current), f.data.size());
        (void)client.close_write();
        auto back = reader.wait();
        ASSERT_TRUE(back);
        EXPECT_TRUE(f.same(*back, 1000));
        client.close();
        server.close();
    }
}

// A file at its end sends nothing; a pipe is no regular file: the copy's
// loop, the bytes the same
TEST(NetSocket_Tests, CopyOfAPipeOrAFileAtItsEnd) {
    TempFile f(1 << 20, 9);
    auto [client, server] = tcp_pair();
    auto file = io::open(sgcl::string(f.path));
    ASSERT_TRUE(file);
    ASSERT_TRUE(file->seek(0, io::seek_from::end));
    auto none = io::copy(client, *file);
    ASSERT_TRUE(none);
    EXPECT_EQ(*none, 0u);
    auto ends = io::pipe();
    ASSERT_TRUE(ends);
    auto reader = spawn(read_to_end(server));
    std::thread writer([&] {
        auto p = pattern(1 << 20, 9);
        (void)ends->write.write(p.as_slice());
        (void)ends->write.close();
    });
    auto sent = io::copy(client, ends->read);
    writer.join();
    ASSERT_TRUE(sent) << sent.error().message();
    EXPECT_EQ(*sent, f.data.size());
    (void)client.close_write();
    auto back = reader.wait();
    ASSERT_TRUE(back);
    EXPECT_TRUE(f.same(*back, 0));
    client.close();
    server.close();
}

// sendfile keeps the write's deadline (the peer reads nothing, the
// buffers fill) and a closed peer is an error, not SIGPIPE
TEST(NetSocket_Tests, SendfileKeepsTheDeadlineAndNoSignal) {
    ASSERT_EQ(std::signal(SIGPIPE, SIG_DFL), SIG_DFL);
    TempFile f(64 << 20, 11);
    {
        auto [client, server] = tcp_pair();
        auto file = io::open(sgcl::string(f.path));
        ASSERT_TRUE(file);
        client.set_write_deadline(std::chrono::steady_clock::now() + 300ms);
        auto sent = io::copy(client, *file);
        ASSERT_FALSE(sent);
        EXPECT_TRUE(sent.error().is_timeout()) << sent.error().message();
        client.close();
        server.close();
    }
    {
        auto [client, server] = tcp_pair();
        server.close();
        auto file = io::open(sgcl::string(f.path));
        ASSERT_TRUE(file);
        auto sent = io::copy(client, *file);
        ASSERT_FALSE(sent);
        auto code = sent.error().code();
        // macOS's sendfile names a reset peer ENOTCONN where send says EPIPE
        EXPECT_TRUE(code == std::errc::broken_pipe || code == std::errc::connection_reset || code == std::errc::not_connected) << sent.error().message();
        client.close();
    }
}

// What cannot fail but by a broken contract is noexcept: the close of a
// connection and of a listener, and the deadlines, as udp::socket's are
TEST(NetSocket_Tests, CloseAndDeadlinesAreNoexcept) {
    const connection c;
    const listener l;
    const sgcl::time_point t;
    static_assert(noexcept(c.close()));
    static_assert(noexcept(c.set_deadline(t)));
    static_assert(noexcept(c.set_read_deadline(t)));
    static_assert(noexcept(c.set_write_deadline(t)));
    static_assert(noexcept(l.close()));
    static_assert(!noexcept(c.close_write()));   // TLS sends its close_notify and waits for the socket
    auto [a, b] = connection::in_memory();
    a.set_deadline(sgcl::clock::now() + std::chrono::seconds(5));
    EXPECT_TRUE(a.close());
    EXPECT_TRUE(b.close());
}
