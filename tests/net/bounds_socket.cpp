//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net's sockets at their boundaries (DESIGN 408): connection, listener,
// tcp, udp and unix_domain. What socket.cpp, memory.cpp, write.cpp and
// race.cpp hold already (an echo of 1 B to 16 MB, a close from another
// task or thread ending a read, an accept and a write's rest, deadlines on
// the manual clock and at the clock's end, a write to a closed peer, the
// line bound, a datagram cut or into an empty buffer, a zone past any
// index) is not repeated; here the ports 0 and 65535, the scope ids, the
// deadlines in the past and at the maximum on every transport, a close
// during a blocking write, an end part way, a handle moved from, and the
// limits of a unix path and of a datagram.
#include "tests/types.h"
#include "sgcl/net/net.h"

#include <atomic>
#include <chrono>
#include <string>
#include <sys/un.h>
#include <thread>

using namespace sgcl;
using namespace sgcl::async;

namespace {
    using namespace std::chrono_literals;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    pair<net::connection, net::connection> tcp_pair(const char* at = "127.0.0.1:0") {
        auto l = net::tcp::listen(at);
        EXPECT_TRUE(l) << l.error().message();
        auto client = net::tcp::connect(l->local_endpoint());
        EXPECT_TRUE(client) << client.error().message();
        auto server = l->accept();
        EXPECT_TRUE(server) << server.error().message();
        (void)l->close();
        return pair<net::connection, net::connection>(*client, *server);
    }

    // The transports a connection has: a TCP pair, a unix pair and the pair in memory
    template<class F>
    void on_every_transport(F f) {
        {
            auto [a, b] = tcp_pair();
            f(a, b, "tcp");
        }
        {
            auto dir = io::make_temp_dir({}, "sgcl-net-*");
            ASSERT_TRUE(dir);
            auto path = io::path::join(*dir, "b.sock");
            auto l = net::unix_domain::listen(path);
            ASSERT_TRUE(l) << l.error().message();
            auto a = net::unix_domain::connect(path);
            ASSERT_TRUE(a) << a.error().message();
            auto b = l->accept();
            ASSERT_TRUE(b);
            (void)l->close();
            f(*a, *b, "unix");
            (void)io::remove_all(*dir);
        }
        {
            auto [a, b] = net::connection::in_memory();
            f(a, b, "memory");
        }
    }
}

// The ends of a port: 0 to listen is the system's choice, 0 to connect is
// an error of the system's, never a crash or a wait; 65535 listened on,
// connected to and reported
TEST(NetSocketBounds_Tests, PortsZeroAndTheLast) {
    auto zero = net::tcp::connect(net::endpoint(net::ip_address::loopback_v4(), 0));
    ASSERT_FALSE(zero);
    EXPECT_EQ(text(zero.error().op()), "dial tcp");
    auto by_name = net::tcp::connect("127.0.0.1:0");
    ASSERT_FALSE(by_name);
    auto l = net::tcp::listen("127.0.0.1:65535");
    if (!l) {
        EXPECT_EQ(l.error().code(), std::errc::address_in_use) << l.error().message();   // another program has it
    } else {
        EXPECT_EQ(l->local_endpoint().port(), 65535);
        auto c = net::tcp::connect("127.0.0.1:65535");
        ASSERT_TRUE(c) << c.error().message();
        auto s = l->accept();
        ASSERT_TRUE(s);
        EXPECT_EQ(c->remote_endpoint().port(), 65535);
        EXPECT_EQ(s->local_endpoint().port(), 65535);
        (void)c->close();
        (void)s->close();
        (void)l->close();
    }
    auto u = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(u);
    EXPECT_NE(u->local_endpoint().port(), 0);
    byte one[1] = {};
    (void)u->send_to(one, net::endpoint(net::ip_address::loopback_v4(), 0));   // the system's to take or refuse (macOS takes it)
    (void)u->close();
}

// A timeout of zero or less is ETIMEDOUT at once, no attempt made; the
// largest is none (LimitsAtTheClocksEnd)
TEST(NetSocketBounds_Tests, ConnectTimeoutsOfZeroAndLess) {
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto at = l->local_endpoint().to_string();
    for (auto t : {duration(), duration(-1s), duration::min()}) {
        auto c = net::tcp::connect(at, t);
        ASSERT_FALSE(c);
        EXPECT_TRUE(c.error().is_timeout()) << c.error().message();
        auto a = spawn(net::tcp::async_connect(at, t)).wait();
        ASSERT_FALSE(a);
        EXPECT_TRUE(a.error().is_timeout()) << a.error().message();
    }
    auto named = net::tcp::connect("localhost:" + sgcl::to_string(l->local_endpoint().port()), duration());
    ASSERT_FALSE(named);
    EXPECT_TRUE(named.error().is_timeout()) << named.error().message();   // the lookup's own check
    (void)l->close();
    scheduler::stop();
}

// Scope ids: a zone of a number is that interface's index (0 and past
// 2^32 - 1 are none: invalid_address, never another interface), a name
// the interface's; an address with its zone over the loopback (macOS's
// lo0 has fe80::1) round-trips, the ends reporting the zone by name
TEST(NetSocketBounds_Tests, ScopeIds) {
    for (auto zone : {"0", "4294967296", "000000000000000", "no-such-if"}) {
        auto a = net::ip_address("fe80::1").with_zone(zone);
        auto c = net::tcp::connect(net::endpoint(a, 80));
        ASSERT_FALSE(c) << zone;
        EXPECT_EQ(c.error().code(), net::errc::invalid_address) << zone << ": " << c.error().message();
        auto l = net::tcp::listen(net::endpoint(a, 0).to_string());
        ASSERT_FALSE(l) << zone;
        EXPECT_EQ(l.error().code(), net::errc::invalid_address) << zone;
    }
    auto l = net::tcp::listen("[fe80::1%lo0]:0");
    if (!l) {
        GTEST_SKIP() << "no fe80::1 on lo0: " << l.error().message();
    }
    EXPECT_EQ(text(l->local_endpoint().address().zone()), "lo0");
    for (auto zone : {"lo0", "1"}) {   // lo0's index is 1 on macOS
        auto c = net::tcp::connect(net::endpoint(net::ip_address("fe80::1").with_zone(zone), l->local_endpoint().port()));
        ASSERT_TRUE(c) << zone << ": " << c.error().message();
        auto s = l->accept();
        ASSERT_TRUE(s);
        EXPECT_EQ(text(c->remote_endpoint().address().zone()), "lo0") << zone;
        EXPECT_EQ(text(s->remote_endpoint().address().zone()), "lo0") << zone;
        ASSERT_TRUE(c->write("z"));
        byte b[1];
        EXPECT_EQ(*s->read_full(b), 1u);
        (void)c->close();
        (void)s->close();
    }
    // an IPv4-mapped address dialed is the IPv4 peer, as accepted on a socket of both families
    auto v4l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(v4l);
    auto mapped = net::tcp::connect(net::endpoint(net::ip_address("::ffff:127.0.0.1"), v4l->local_endpoint().port()));
    ASSERT_TRUE(mapped) << mapped.error().message();
    EXPECT_EQ(mapped->remote_endpoint(), v4l->local_endpoint());
    (void)mapped->close();
    (void)v4l->close();
    (void)l->close();
    // a datagram to a zone that names no interface: invalid_address, as a connect's
    auto u = net::udp::bind("[fe80::1%lo0]:0");
    ASSERT_TRUE(u) << u.error().message();
    byte one[1] = {};
    auto bad = u->send_to(one, net::endpoint(net::ip_address("fe80::1%no-such-if"), u->local_endpoint().port()));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::invalid_address) << bad.error().message();
    auto bad_async = spawn(u->async_send_to(one, net::endpoint(net::ip_address("fe80::1%0"), 9))).wait();
    ASSERT_FALSE(bad_async);
    EXPECT_EQ(bad_async.error().code(), net::errc::invalid_address) << bad_async.error().message();
    auto v4 = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(v4);
    auto other = v4->send_to(one, u->local_endpoint());               // an IPv6 address to an IPv4 socket
    ASSERT_FALSE(other);
    EXPECT_EQ(other.error().code(), std::errc::address_family_not_supported);
    ASSERT_TRUE(u->send_to(one, u->local_endpoint()));                // to itself, through its zone
    byte got[4];
    auto d = u->receive_from(got);
    ASSERT_TRUE(d);
    EXPECT_EQ(text(d->from.address().zone()), "lo0");
    (void)u->close();
    (void)v4->close();
}

// A deadline already passed fails a read and a write at once and takes
// and gives nothing, though the data is there and the peer reads; one at
// time_point::max() is none: the operations go on, the getters give it back
TEST(NetSocketBounds_Tests, DeadlinesPastAndAtTheMaximum) {
    on_every_transport([](net::connection a, net::connection b, const char* what) {
        auto feeder = std::thread([&] { (void)a.write("data"); });
        b.set_read_deadline(sgcl::clock::now() - 1h);
        std::this_thread::sleep_for(10ms);
        byte buf[8];
        auto r = b.read(buf);
        ASSERT_FALSE(r) << what;
        EXPECT_TRUE(r.error().is_timeout()) << what;
        auto ar = spawn(b.async_read(buf)).wait();
        ASSERT_FALSE(ar) << what;
        EXPECT_TRUE(ar.error().is_timeout()) << what;
        b.set_read_deadline(time_point::max());
        EXPECT_EQ(b.read_deadline(), time_point::max()) << what;
        auto later = b.read_full(slice<byte>(buf).first(4));
        feeder.join();
        ASSERT_TRUE(later) << what << ": " << later.error().message();
        EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(buf), 4), "data") << what;

        a.set_write_deadline(sgcl::clock::now() - 1ns);
        auto w = a.write("nothing");
        ASSERT_FALSE(w) << what;
        EXPECT_TRUE(w.error().is_timeout()) << what;
        auto aw = spawn(a.async_write("nothing")).wait();
        ASSERT_FALSE(aw) << what;
        EXPECT_TRUE(aw.error().is_timeout()) << what;
        a.set_deadline(time_point::max());
        EXPECT_EQ(a.write_deadline(), time_point::max()) << what;
        EXPECT_EQ(a.read_deadline(), time_point::max()) << what;
        auto reader = spawn(b.async_read(buf));
        ASSERT_TRUE(a.write("x")) << what;
        auto got = reader.wait();
        ASSERT_TRUE(got) << what;
        EXPECT_EQ(*got, 1u) << what;                                  // nothing of "nothing" went
        // a read waiting under the maximum ends by a close, not by its timer
        auto waiting = spawn(b.async_read(buf));
        b.set_read_deadline(time_point::max());
        std::this_thread::sleep_for(10ms);
        EXPECT_FALSE(waiting.done()) << what;
        (void)b.close();
        auto cut = waiting.wait();
        ASSERT_FALSE(cut) << what;
        EXPECT_TRUE(cut.error().is_closed()) << what;
        (void)a.close();
    });
    scheduler::stop();
}

// A blocking write that waits on a full buffer, closed from another thread:
// io::errc::closed, the thread back; a read on a thread the same way (a
// task's are race.cpp's and write.cpp's). The results stay on the threads'
// own stacks and plain facts come back: an error made from another thread
// in the main thread's stack would put a string's tracked word there
TEST(NetSocketBounds_Tests, CloseDuringABlockingWriteOrRead) {
    on_every_transport([](net::connection a, net::connection b, const char* what) {
        std::atomic<bool> done = false;
        bool write_failed = false;
        bool write_closed = false;
        std::string write_message;
        std::thread writer([&] {
            std::string big(64 << 20, 'w');
            auto w = a.write(sgcl::string(big));
            if (!w) {
                write_failed = true;
                write_closed = w.error().is_closed();
                auto m = w.error().message();
                write_message.assign(m.data(), m.size());
            }
            done = true;
        });
        std::this_thread::sleep_for(30ms);
        EXPECT_FALSE(done) << what;
        (void)a.close();
        writer.join();
        ASSERT_TRUE(write_failed) << what;
        EXPECT_TRUE(write_closed) << what << ": " << write_message;
        bool read_ended = false;
        std::thread reader([&] {
            byte buf[4];
            for (;;) {   // the bytes the write left in the buffers first, then the end or the close
                auto r = b.read(buf);
                if (!r || *r == 0) {
                    read_ended = !r ? r.error().is_closed() : *r == 0;
                    break;
                }
            }
        });
        std::this_thread::sleep_for(10ms);
        (void)b.close();
        reader.join();
        EXPECT_TRUE(read_ended) << what;
    });
}

// A stream that ends part way: read_full says how far it got, read_line
// gives the last line without its newline, read_all what came; an empty
// buffer reads nothing and waits for nothing; an empty write is 0
TEST(NetSocketBounds_Tests, AnEndPartWayAndNothing) {
    on_every_transport([](net::connection a, net::connection b, const char* what) {
        byte none[1];
        auto zero = b.read(slice<byte>(none).first(0));   // nothing there yet: no wait
        ASSERT_TRUE(zero) << what;
        EXPECT_EQ(*zero, 0u) << what;
        auto empty_write = a.write(slice<const byte>());
        ASSERT_TRUE(empty_write) << what;
        EXPECT_EQ(*empty_write, 0u) << what;
        auto feeder = std::thread([&] {
            (void)a.write("abc");
            (void)a.close_write();
        });
        byte buf[8];
        auto r = b.read_full(buf);
        feeder.join();
        ASSERT_FALSE(r) << what;
        EXPECT_TRUE(r.error().is_eof()) << what;
        EXPECT_EQ(r.error().count(), 3u) << what;
        EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(buf), 3), "abc") << what;
        auto after = b.read_full(slice<byte>(buf).first(0));
        ASSERT_TRUE(after) << what;
        EXPECT_EQ(*after, 0u) << what;
        (void)a.close();
        (void)b.close();
    });
    on_every_transport([](net::connection a, net::connection b, const char* what) {
        auto feeder = std::thread([&] {
            (void)a.write("one\ntwo");
            (void)a.close_write();
        });
        auto one = b.read_line();
        auto two = b.read_line();
        auto end = b.read_line();
        feeder.join();
        ASSERT_TRUE(one && *one && two && *two && end) << what;
        EXPECT_EQ(text(**one), "one") << what;
        EXPECT_EQ(text(**two), "two") << what;
        EXPECT_FALSE(*end) << what;
        (void)a.close();
        (void)b.close();
    });
}

// The line bound at its edge: a line of exactly max_line bytes is a line
// (its "\n" not counted, a "\r" before it counted: the buffered reader's
// rule), one more byte is line_too_long; a bound of 0 is none, a line past
// the default 64 KB read whole
TEST(NetSocketBounds_Tests, TheLineBoundAtItsEdge) {
    on_every_transport([](net::connection a, net::connection b, const char* what) {
        b.set_max_line(8);
        EXPECT_EQ(b.max_line(), 8u) << what;
        auto feeder = std::thread([&] {
            (void)a.write("12345678\n1234567\r\n12345678\r\n");
        });
        auto fits = b.read_line();
        ASSERT_TRUE(fits && *fits) << what;
        EXPECT_EQ(text(**fits), "12345678") << what;
        auto with_cr = b.read_line();
        ASSERT_TRUE(with_cr && *with_cr) << what;
        EXPECT_EQ(text(**with_cr), "1234567") << what;
        auto over = b.read_line();
        ASSERT_FALSE(over) << what;
        EXPECT_EQ(over.error().code(), io::errc::line_too_long) << what;
        feeder.join();
        (void)a.close();
        (void)b.close();
    });
    on_every_transport([](net::connection a, net::connection b, const char* what) {
        b.set_max_line(0);
        EXPECT_EQ(b.max_line(), 0u) << what;
        std::string line(100 << 10, 'y');
        auto feeder = std::thread([&] {
            (void)a.write(sgcl::string(line + "\n"));
            (void)a.close_write();
        });
        auto whole = b.read_line();
        ASSERT_TRUE(whole && *whole) << what << ": " << (whole ? std::string() : text(whole.error().message()));
        EXPECT_EQ((*whole)->size(), line.size()) << what;
        feeder.join();
        (void)a.close();
        (void)b.close();
    });
}

// Handles moved from are the same connection, listener and socket (the
// move of a tracked word copies it, as tracked_ptr's); a default handle is
// none and equal to another default one; a second close does nothing
TEST(NetSocketBounds_Tests, HandlesMovedFromAndDefault) {
    auto [a, b] = tcp_pair();
    auto moved = std::move(a);
    EXPECT_TRUE(bool(a));
    EXPECT_EQ(a, moved);
    ASSERT_TRUE(a.write("m"));
    byte buf[1];
    EXPECT_EQ(*b.read_full(buf), 1u);
    EXPECT_FALSE(bool(net::connection()));
    EXPECT_EQ(net::connection(), net::connection());
    EXPECT_NE(net::connection(), a);
    ASSERT_TRUE(a.close());
    EXPECT_TRUE(moved.is_closed());
    EXPECT_TRUE(moved.close());
    (void)b.close();

    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto lm = std::move(*l);
    EXPECT_TRUE(bool(*l));
    EXPECT_EQ(*l, lm);
    EXPECT_FALSE(bool(net::listener()));
    auto port = lm.local_endpoint();
    ASSERT_TRUE(l->close());
    EXPECT_TRUE(lm.close());                                         // a second close: nothing
    EXPECT_TRUE(lm.is_closed());
    EXPECT_EQ(lm.local_endpoint(), port);                             // kept after the close
    auto after = lm.accept();
    ASSERT_FALSE(after);
    EXPECT_TRUE(after.error().is_closed());
    auto async_after = spawn(lm.async_accept()).wait();
    ASSERT_FALSE(async_after);
    EXPECT_TRUE(async_after.error().is_closed());

    auto u = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(u);
    auto um = std::move(*u);
    EXPECT_EQ(*u, um);
    EXPECT_FALSE(bool(net::udp::socket()));
    ASSERT_TRUE(u->close());
    EXPECT_TRUE(um.close());
    byte d[4];
    auto r = um.receive_from(d);
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_closed());
    auto s = um.send_to(slice<const byte>(d), um.local_endpoint());
    ASSERT_FALSE(s);
    EXPECT_TRUE(s.error().is_closed());
    scheduler::stop();
}

// Keep-alive at its ends: zero and less turn the probes off, the longest
// duration is clamped to the system's largest number of seconds
TEST(NetSocketBounds_Tests, KeepAliveAtItsEnds) {
    auto [a, b] = tcp_pair();
    EXPECT_TRUE(a.set_keep_alive(duration::max()));
    EXPECT_TRUE(a.set_keep_alive(1ns));                               // rounded up to a second
    EXPECT_TRUE(a.set_keep_alive(-1s));
    EXPECT_TRUE(a.set_keep_alive(duration::min()));
    (void)a.close();
    (void)b.close();
}

// A unix socket's path: 103 bytes on macOS (107 on Linux) is the longest,
// one more is invalid_address, as the empty path and one with a NUL; the
// listener's second close removes nothing (the file is someone else's by
// then)
TEST(NetSocketBounds_Tests, UnixPathsAtTheirLimit) {
    auto dir = io::make_temp_dir("/tmp", "u");
    ASSERT_TRUE(dir);
    sockaddr_un probe;
    const size_t longest = sizeof(probe.sun_path) - 1;
    auto name = [&](size_t total) {
        return io::path::join(*dir, sgcl::string(std::string(total - dir->size() - 1, 'p')));
    };
    auto at = name(longest);
    ASSERT_EQ(at.size(), longest);
    auto l = net::unix_domain::listen(at);
    ASSERT_TRUE(l) << l.error().message();
    auto c = net::unix_domain::connect(at);
    ASSERT_TRUE(c) << c.error().message();
    EXPECT_EQ(c->path(), at);
    (void)c->close();
    for (auto bad : {name(longest + 1), sgcl::string(), sgcl::string(std::string_view("a\0b", 3))}) {
        auto lb = net::unix_domain::listen(bad);
        ASSERT_FALSE(lb);
        EXPECT_EQ(lb.error().code(), net::errc::invalid_address);
        auto cb = net::unix_domain::connect(bad);
        ASSERT_FALSE(cb);
        EXPECT_EQ(cb.error().code(), net::errc::invalid_address);
        auto ab = spawn(net::unix_domain::async_connect(bad)).wait();
        ASSERT_FALSE(ab);
        EXPECT_EQ(ab.error().code(), net::errc::invalid_address);
    }
    ASSERT_TRUE(l->close());
    EXPECT_FALSE(io::exists(at));
    ASSERT_TRUE(io::write_file(at, "someone else's"));
    EXPECT_TRUE(l->close());
    EXPECT_TRUE(io::exists(at));                                      // not removed twice
    (void)io::remove_all(*dir);
    scheduler::stop();
}

// A datagram at the limits: 65507 bytes is the most an IPv4 datagram
// carries, one more is EMSGSIZE (the system may refuse less: macOS's
// net.inet.udp.maxdgram, 9216 by default, is EMSGSIZE as well); a buffer
// of one byte takes the first byte of a long one, cut
TEST(NetSocketBounds_Tests, DatagramsAtTheirLimits) {
    auto server = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(server);
    auto client = net::udp::connect(server->local_endpoint().to_string());
    ASSERT_TRUE(client);
    vector<byte> big(65508);
    auto over = client->send(big);
    ASSERT_FALSE(over);
    EXPECT_EQ(over.error().code(), std::errc::message_size) << over.error().message();
    auto over_to = server->send_to(big, client->local_endpoint());
    ASSERT_FALSE(over_to);
    EXPECT_EQ(over_to.error().code(), std::errc::message_size);
    auto most = client->send(slice<const byte>(big).first(65507));
    if (most) {
        EXPECT_EQ(*most, 65507u);
        byte one[1];
        auto d = server->receive_from(one);
        ASSERT_TRUE(d);
        EXPECT_EQ(d->size, 1u);
        EXPECT_TRUE(d->truncated);
    } else {
        EXPECT_EQ(most.error().code(), std::errc::message_size);       // below the protocol's limit, the system's
    }
    ASSERT_TRUE(client->send(slice<const byte>(big).first(1)));
    byte one[1];
    auto exact = server->receive_from(one);
    ASSERT_TRUE(exact);
    EXPECT_EQ(exact->size, 1u);
    EXPECT_FALSE(exact->truncated);                                   // exactly the buffer: not cut
    // a send with no address from a socket that has no peer: the system's error
    auto lone = server->send(slice<const byte>(big).first(1));
    EXPECT_FALSE(lone);
    (void)server->close();
    (void)client->close();
}

// The resolver's edges: a numeric host with a zone answered as it stands,
// a name past the 253 bytes of DNS refused, the empty address and a zone
// that names no interface to reverse_lookup (invalid_address, not a
// question to the resolver), and a stop that came before the task ran
TEST(NetSocketBounds_Tests, LookupsAtTheirEdges) {
    auto zoned = net::dns::lookup("fe80::1%no-such-if");
    ASSERT_TRUE(zoned);
    EXPECT_EQ(text((*zoned)[0].zone()), "no-such-if");
    std::string longest;
    for (int i : range(63)) {
        (void)i;
        longest += "abc.";
    }
    longest += "x.invalid";
    auto past = net::dns::lookup(sgcl::string(longest));
    ASSERT_FALSE(past);
    for (const auto& a : {net::ip_address(), net::ip_address("fe80::1%no-such-if"), net::ip_address("fe80::1%0")}) {
        auto r = net::dns::reverse_lookup(a);
        ASSERT_FALSE(r) << text(a.to_string());
        EXPECT_EQ(r.error().code(), net::errc::invalid_address) << text(a.to_string()) << ": " << r.error().message();
        auto ar = spawn(net::dns::async_reverse_lookup(a)).wait();
        ASSERT_FALSE(ar);
        EXPECT_EQ(ar.error().code(), net::errc::invalid_address);
    }
    stop_source src;
    src.request_stop();
    auto stopped = spawn(net::dns::async_reverse_lookup(net::ip_address::loopback_v4(), src.token())).wait();
    ASSERT_FALSE(stopped);
    EXPECT_EQ(stopped.error().code(), std::errc::operation_canceled);
    auto stopped_name = spawn(net::dns::async_lookup("localhost", src.token())).wait();
    ASSERT_FALSE(stopped_name);
    EXPECT_EQ(stopped_name.error().code(), std::errc::operation_canceled);
    blocking_pool::wait_idle();
    scheduler::stop();
}

// A datagram socket's deadlines at their ends: one passed fails a send and
// a receive at once, the maximum is none
TEST(NetSocketBounds_Tests, DatagramDeadlinesPastAndAtTheMaximum) {
    auto server = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(server);
    auto client = net::udp::connect(server->local_endpoint().to_string());
    ASSERT_TRUE(client);
    client->set_write_deadline(sgcl::clock::now() - 1ns);
    byte b[4] = {};
    auto late = client->send(b);
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_timeout());
    auto late_async = spawn(client->async_send(b)).wait();
    ASSERT_FALSE(late_async);
    EXPECT_TRUE(late_async.error().is_timeout());
    client->set_deadline(time_point::max());
    server->set_deadline(time_point::max());
    EXPECT_EQ(client->write_deadline(), time_point::max());
    EXPECT_EQ(server->read_deadline(), time_point::max());
    ASSERT_TRUE(client->send(b));
    auto got = server->receive_from(b);
    ASSERT_TRUE(got) << got.error().message();
    EXPECT_EQ(got->size, 4u);
    (void)server->close();
    (void)client->close();
    scheduler::stop();
}
