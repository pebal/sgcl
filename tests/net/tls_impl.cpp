//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The TLS connection's own paths (sgcl/net/tls/detail/impl.h), our client
// and our server over the loopback: a close from another task ending a
// read and a write in progress (the semantics of connection.md, the
// transport's raw operations called with no lock of its own); a write begun
// without waiting (start_write, as the http server writes) whose record the
// socket takes in part, its tail sent before anything else and before the
// close_notify of async_close; the read without a frame (try_read) the http
// server's head takes, and read_line's buffer above the connection.
#include "tests/types.h"

#include "sgcl/net/tls.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace tls = sgcl::net::tls;

namespace {
    std::string testdata(const std::string& name) {
        std::string f = __FILE__;
        return f.substr(0, f.rfind('/')) + "/tls_testdata/" + name;
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    // Two ends of a TLS connection over the loopback, ours on both sides
    struct Pair {
        net::connection client, server;
    };

    Pair tls_pair() {
        auto l = net::tcp::listen("127.0.0.1:0");
        EXPECT_TRUE(l.has_value());
        tls::config scfg;
        scfg.identities = {tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        std::atomic<bool> ok = false;
        net::connection accepted;
        std::thread t([&] {
            auto a = l->accept();
            if (!a) {
                return;
            }
            auto s = tls::server(*a, scfg);
            if (s) {
                accepted = *s;
                ok = true;
            }
        });
        tls::config ccfg;
        ccfg.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
        ccfg.server_name = sgcl::string("localhost");
        auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(l->local_endpoint().port())), ccfg);
        t.join();
        (void)l->close();
        EXPECT_TRUE(c.has_value());
        EXPECT_TRUE(ok.load());
        return Pair{c ? *c : net::connection(), accepted};
    }

    std::vector<byte> pattern(size_t n) {
        std::vector<byte> v(n);
        for (size_t i = 0; i < n; ++i) {
            v[i] = byte(uint8_t(i * 131 + 7));
        }
        return v;
    }
}

// close() from another task ends an awaited read and an awaited write in
// progress with io::errc::closed: nothing hangs, nothing is used after the
// close (the transport's descriptor goes back when the last one lets go)
TEST(TlsImpl_Tests, CloseEndsAReadAndAWriteInProgress) {
    for (int round = 0; round < 20; ++round) {
        auto p = tls_pair();
        ASSERT_TRUE(p.client && p.server);
        net::connection c = p.client;
        // the read waits (the server sends nothing); the write fills the
        // socket (the server reads nothing) and waits
        auto big = std::make_shared<std::vector<byte>>(pattern(8 << 20));
        auto reading = async::spawn([](net::connection c) -> async::task<expected<size_t, io::error>> {
            byte buf[4096];
            co_return co_await c.async_read(buf);
        }(c));
        auto writing = async::spawn([](net::connection c, std::shared_ptr<std::vector<byte>> data) -> async::task<expected<size_t, io::error>> {
            co_return co_await c.async_write(slice<const byte>(data->data(), data->size()));
        }(c, big));
        std::this_thread::sleep_for(std::chrono::milliseconds(round % 5 * 3 + 1));
        auto closing = async::spawn([](net::connection c) -> async::task<expected<void, io::error>> {
            co_return co_await c.async_close();
        }(c));
        auto r = reading.wait();
        auto w = writing.wait();
        (void)closing.wait();
        ASSERT_FALSE(r.has_value());
        EXPECT_TRUE(r.error().is_closed()) << std::string(r.error().message().view());
        ASSERT_FALSE(w.has_value());
        EXPECT_TRUE(w.error().is_closed() || w.error().is_timeout()) << std::string(w.error().message().view());
        EXPECT_TRUE(c.is_closed());
        EXPECT_FALSE(c.write(sgcl::string("after\n")).has_value());
        (void)p.server.close();
    }
}

// A write begun without waiting (as the http server writes a response):
// the records the socket takes whole go at once, a record taken in part
// keeps its tail, and the rest of the write sends the tail first and never
// seals that plaintext again; the peer reads the data whole and in order
TEST(TlsImpl_Tests, AStartedWriteKeepsTheTailOfARecord) {
    auto p = tls_pair();
    ASSERT_TRUE(p.client && p.server);
    auto data = pattern(6 << 20);   // more than the socket's buffers
    auto& impl = net::detail::ConnectionAccess::impl(p.client);
    auto started = impl.start_write(slice<const byte>(data.data(), data.size()));
    ASSERT_TRUE(started.rest.has_value());   // the socket did not take it all
    std::vector<byte> got;
    std::thread reader([&] {
        byte buf[65536];
        while (got.size() < data.size()) {
            auto n = p.server.read(buf);
            if (!n || *n == 0) {
                break;
            }
            got.insert(got.end(), buf, buf + *n);
        }
    });
    auto rest = started.rest->wait();
    ASSERT_TRUE(rest.has_value()) << std::string(rest.error().message().view());
    EXPECT_EQ(*rest, data.size());
    reader.join();
    ASSERT_EQ(got.size(), data.size());
    EXPECT_TRUE(got == data);
    // and the connection goes on: a line each way
    ASSERT_TRUE(p.client.write(sgcl::string("after\n")).has_value());
    auto line = p.server.read_line();
    ASSERT_TRUE(line && *line);
    EXPECT_EQ(std::string((*line)->view()), "after");
}

// async_close with the tail of a record not yet sent: the tail first, then
// the close_notify, so the peer reads a prefix of the data in whole records
// and then the end of the stream (not a record cut, not a bad record)
TEST(TlsImpl_Tests, AsyncCloseSendsTheTailThenCloseNotify) {
    auto p = tls_pair();
    ASSERT_TRUE(p.client && p.server);
    auto data = pattern(6 << 20);
    auto& impl = net::detail::ConnectionAccess::impl(p.client);
    auto started = impl.start_write(slice<const byte>(data.data(), data.size()));
    ASSERT_TRUE(started.rest.has_value());
    // the rest of the write is dropped unawaited: only the tail of the last
    // record the socket took in part remains, queued
    started.rest.reset();
    std::vector<byte> got;
    optional<io::error> end;
    std::thread reader([&] {
        byte buf[65536];
        for (;;) {
            auto n = p.server.read(buf);
            if (!n) {
                end = n.error();
                break;
            }
            if (*n == 0) {
                break;   // close_notify
            }
            got.insert(got.end(), buf, buf + *n);
        }
    });
    auto closed = p.client.async_close().wait();
    EXPECT_TRUE(closed.has_value());
    reader.join();
    EXPECT_FALSE(end.has_value()) << std::string(end ? end->message().view() : std::string_view());
    ASSERT_GT(got.size(), 0u);
    ASSERT_LE(got.size(), data.size());
    EXPECT_TRUE(std::equal(got.begin(), got.end(), data.begin()));   // a prefix, in order
    EXPECT_EQ(got.size() % tls::detail::MaxPlaintext, 0u);          // whole records
}

// The read without a frame (ConnImpl::try_read, the http server's head):
// the plaintext held first, a record read ahead, nullopt with no whole
// record; and read_line's buffer above the connection, the plaintext
// straight into it
TEST(TlsImpl_Tests, TryReadAndReadLine) {
    auto p = tls_pair();
    ASSERT_TRUE(p.client && p.server);
    auto& impl = net::detail::ConnectionAccess::impl(p.server);
    byte buf[4];
    bool slow = false;
    auto none = impl.try_read(buf, slow);   // nothing sent yet
    ASSERT_TRUE(none.has_value());
    EXPECT_FALSE(slow);
    EXPECT_FALSE(none->has_value());
    ASSERT_TRUE(p.client.write(sgcl::string("abcdefgh")).has_value());
    std::string got;
    for (int i = 0; i < 1000 && got.size() < 8; ++i) {
        auto r = impl.try_read(buf, slow);
        ASSERT_TRUE(r.has_value());
        ASSERT_FALSE(slow);
        if (*r) {
            got.append(reinterpret_cast<const char*>(buf), **r);   // 4 of the record, then the 4 held
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    EXPECT_EQ(got, "abcdefgh");
    ASSERT_TRUE(p.client.write(sgcl::string("one\ntwo\n")).has_value());
    auto one = p.server.read_line();
    auto two = p.server.read_line();
    ASSERT_TRUE(one && *one && two && *two);
    EXPECT_EQ(std::string((*one)->view()), "one");
    EXPECT_EQ(std::string((*two)->view()), "two");
    // the line buffer above the connection: try_read now takes the slow way
    auto after = impl.try_read(buf, slow);
    EXPECT_TRUE(slow);
    (void)after;
}
