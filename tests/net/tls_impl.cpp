//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
// server's head takes, and read_line's buffer above the connection; the
// alerts of a failed handshake as each side reads them (before the server's
// hello, a chain the client refuses, a record the client cannot open).
#include "tests/types.h"
#include "tests/source_root.h"

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
        return (source_root() / "tests/net/tls_testdata" / name).string();
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

// An alert in the clear before the server's hello: the error names the
// alert as the peer's, and only close_notify and protocol_version (what a
// server without TLS 1.3, an AWS load balancer among them, answers a hello
// of 1.3 alone with) add the guess that the server has no TLS 1.3
TEST(TlsImpl_Tests, AnAlertBeforeTheServersHello) {
    // close_notify, protocol_version, handshake_failure, no_application_protocol
    for (uint8_t description : {uint8_t(0), uint8_t(70), uint8_t(40), uint8_t(120)}) {
        auto l = sgcl::net::tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l);
        sgcl::net::listener listener = *l;
        std::thread server([&listener, description] {   // by reference: no tracked handle in a std::thread's state
            auto c = listener.accept();
            if (!c) {
                return;
            }
            std::byte hello[512];
            (void)c->read(hello);   // the ClientHello, or its start
            const uint8_t level = description == 0 ? 1 : 2;
            const uint8_t record[] = {0x15, 0x03, 0x03, 0x00, 0x02, level, description};
            (void)c->write(sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(record), sizeof(record)));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            (void)c->close();
        });
        auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(listener.local_endpoint().port())));
        server.join();
        (void)listener.close();
        ASSERT_FALSE(c) << int(description);
        const std::string m(c.error().message().view());
        const char* want = description == 0 ? "remote error: tls: close notify, before the server's hello (no TLS 1.3?)"
                         : description == 70 ? "remote error: tls: protocol version not supported, before the server's hello (no TLS 1.3?)"
                         : description == 40 ? "remote error: tls: handshake failure, before the server's hello"
                                             : "remote error: tls: no application protocol, before the server's hello";
        EXPECT_NE(m.find(want), std::string::npos) << m;
        if (description == 40 || description == 120) {
            EXPECT_EQ(m.find("TLS 1.3"), std::string::npos) << m;
        }
        EXPECT_TRUE(tls::is_remote(c.error()));
        ASSERT_TRUE(tls::alert_of(c.error()));
        EXPECT_EQ(int(*tls::alert_of(c.error())), int(description));
        EXPECT_FALSE(tls::certificate_reason(c.error()));
    }
}

namespace {
    // A handshake of our client and our server over the loopback, each
    // side's result
    struct Outcome {
        expected<net::connection, io::error> client, server;
    };

    Outcome handshake(const tls::config& ccfg, const tls::config& scfg) {
        auto l = net::tcp::listen("127.0.0.1:0");
        EXPECT_TRUE(l.has_value());
        // the server's result made in a managed object, not in this thread's stack
        auto served = make_tracked<expected<net::connection, io::error>>(unexpected(io::error(io::errc::closed, "accept", "")));
        std::thread t([&] {
            auto a = l->accept();
            if (a) {
                *served = tls::server(*a, scfg);
            }
        });
        auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(l->local_endpoint().port())), ccfg);
        t.join();
        (void)l->close();
        return Outcome{c, *served};
    }

    tls::config server_config() {
        tls::config c;
        c.identities = {tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        return c;
    }

    tls::config client_config(const char* name) {
        tls::config c;
        c.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
        c.server_name = sgcl::string(name);
        return c;
    }
}

// ALPN with nothing in common: the server refuses before its hello with
// no_application_protocol, and the client's error says that, not "no TLS 1.3"
TEST(TlsImpl_Tests, AnAlpnMismatchReadsAsWhatItIs) {
    auto ccfg = client_config("localhost");
    ccfg.alpn = {sgcl::string("http/1.1")};
    auto scfg = server_config();
    scfg.alpn = {sgcl::string("h2")};
    auto o = handshake(ccfg, scfg);
    ASSERT_FALSE(o.client);
    ASSERT_FALSE(o.server);
    const std::string m(o.client.error().message().view());
    EXPECT_NE(m.find("remote error: tls: no application protocol"), std::string::npos) << m;
    EXPECT_EQ(m.find("TLS 1.3"), std::string::npos) << m;
    EXPECT_TRUE(tls::is_remote(o.client.error()));
    EXPECT_EQ(tls::alert_of(o.client.error()), tls::alert::no_application_protocol);
    EXPECT_EQ(tls::alert_of(o.server.error()), tls::alert::no_application_protocol);
    EXPECT_FALSE(tls::is_remote(o.server.error()));
}

// A client that refuses the server's chain says why to the server: its
// alert goes under the handshake keys the server reads with since its hello,
// so the server's error is the client's alert (bad_certificate for a name
// the certificate does not hold, unknown_ca for an authority the client does
// not know), never "unexpected message"
TEST(TlsImpl_Tests, TheServerSeesWhyTheClientRefusedItsChain) {
    {
        auto o = handshake(client_config("wrong.test"), server_config());
        ASSERT_FALSE(o.client);
        ASSERT_FALSE(o.server);
        EXPECT_EQ(tls::certificate_reason(o.client.error()), crypto::x509::reason::hostname_mismatch);
        const std::string m(o.server.error().message().view());
        EXPECT_NE(m.find("remote error: tls: bad certificate"), std::string::npos) << m;
        EXPECT_TRUE(tls::is_remote(o.server.error()));
        EXPECT_EQ(tls::alert_of(o.server.error()), tls::alert::bad_certificate);
    }
    {
        auto ccfg = client_config("localhost");
        ccfg.roots = crypto::x509::certificate_pool();
        auto o = handshake(ccfg, server_config());
        ASSERT_FALSE(o.client);
        ASSERT_FALSE(o.server);
        EXPECT_EQ(tls::certificate_reason(o.client.error()), crypto::x509::reason::unknown_authority);
        const std::string m(o.server.error().message().view());
        EXPECT_NE(m.find("remote error: tls: unknown certificate authority"), std::string::npos) << m;
        EXPECT_EQ(tls::alert_of(o.server.error()), tls::alert::unknown_ca);
    }
}

// A record the client cannot open after the server's hello (a failure of the
// record layer, not of the handshake machine): the client's bad_record_mac
// goes under its handshake keys, a change_cipher_spec first, as the
// machine's own alerts do, so the server reads it as the client's alert and
// not as "unexpected message". A relay between the two flips the last byte
// (the tag) of the server's first encrypted record, its EncryptedExtensions.
TEST(TlsImpl_Tests, TheServerSeesTheClientsRecordFailure) {
    auto front = net::tcp::listen("127.0.0.1:0");
    auto back = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(front);
    ASSERT_TRUE(back);
    // the server's result made in a managed object, not in this thread's stack
    auto served = make_tracked<expected<net::connection, io::error>>(unexpected(io::error(io::errc::closed, "accept", "")));
    std::thread server([&] {
        auto a = back->accept();
        if (a) {
            *served = tls::server(*a, server_config());
        }
    });
    std::atomic<bool> flipped = false;
    std::thread relay([&] {
        auto from_client = front->accept();
        if (!from_client) {
            return;
        }
        auto to_server = net::tcp::connect(back->local_endpoint());
        if (!to_server) {
            (void)from_client->close();
            return;
        }
        net::connection c = *from_client, s = *to_server;
        std::thread upstream([&c, &s] {
            std::byte b[4096];
            for (;;) {
                auto n = c.read(b);
                if (!n || *n == 0 || !s.write(sgcl::slice<const std::byte>(b, *n))) {
                    break;
                }
            }
            (void)s.close_write();
        });
        // downstream, record by record: the first of type application_data
        // (23) after the hello gets its last byte flipped
        std::vector<std::byte> pending;
        std::byte b[4096];
        for (;;) {
            auto n = s.read(b);
            if (!n || *n == 0) {
                break;
            }
            pending.insert(pending.end(), b, b + *n);
            size_t whole = 0;
            while (pending.size() - whole >= 5) {
                const size_t len = size_t(uint8_t(pending[whole + 3])) << 8 | uint8_t(pending[whole + 4]);
                if (pending.size() - whole < 5 + len) {
                    break;
                }
                if (uint8_t(pending[whole]) == 23 && !flipped.exchange(true)) {
                    pending[whole + 5 + len - 1] ^= std::byte{1};
                }
                whole += 5 + len;
            }
            if (whole > 0 && !c.write(sgcl::slice<const std::byte>(pending.data(), whole))) {
                break;
            }
            pending.erase(pending.begin(), pending.begin() + std::ptrdiff_t(whole));
        }
        // the server is done (it has read the client's alert): the client's
        // end closed ends the upstream read
        (void)c.close();
        upstream.join();
        (void)s.close();
    });
    auto ccfg = client_config("localhost");
    auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(front->local_endpoint().port())), ccfg);
    if (c) {
        (void)c->close();
    }
    relay.join();
    server.join();
    (void)front->close();
    (void)back->close();
    EXPECT_TRUE(flipped.load());
    ASSERT_FALSE(c);
    EXPECT_EQ(tls::alert_of(c.error()), tls::alert::bad_record_mac);
    EXPECT_FALSE(tls::is_remote(c.error()));
    ASSERT_FALSE(*served);
    const std::string m(served->error().message().view());
    EXPECT_NE(m.find("remote error: tls: bad record MAC"), std::string::npos) << m;
    EXPECT_TRUE(tls::is_remote(served->error()));
    EXPECT_EQ(tls::alert_of(served->error()), tls::alert::bad_record_mac);
}

// close() from one thread while a read runs on another (http::server::close
// over a connection its task reads): the close_notify close() writes fails
// (the peer has gone), and close() must not record that failure where the
// read side reads its state without the record's lock; the read side keeps
// reading what it reads (the end of the stream here). A data race of the
// two was TSan's finding in HttpHttps_Tests.AFileWrittenAsTheBody; under
// TSan this test shows it, elsewhere it checks that the read ends as it should
TEST(TlsImpl_Tests, CloseWhileAReadRunsOnAnotherThread) {
    for (int round = 0; round < 5; ++round) {
        auto p = tls_pair();
        ASSERT_TRUE(p.client && p.server);
        net::connection server = p.server;
        std::atomic<bool> stop = false;
        std::atomic<int> ends = 0;
        std::thread reader([&server, &stop, &ends] {
            byte buf[256];
            while (!stop.load()) {
                auto n = server.read(buf);
                if (!n || *n == 0) {
                    ends.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
        (void)p.client.close();                                      // close_notify, then the socket closed
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        (void)server.write(sgcl::string("into a closed socket\n"));  // the peer answers with a reset
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        (void)server.close();                                        // its close_notify fails: nothing recorded for the reader
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        stop = true;
        reader.join();
        EXPECT_GT(ends.load(), 0);
    }
}

// close_write() from one thread while a read runs on another: the peer has
// sent its close_notify and gone, so the read side is at the end of the
// stream, and the close_notify close_write writes fails (a reset). That
// failure is the write side's: the reader keeps reading the end (never the
// write's error), and the writes after it fail. The write side's record of
// the failure and the read side's check of its state once shared a field,
// written under the record's lock and read without it (TSan's finding);
// under TSan this test shows a race of the two, elsewhere it checks that
// each direction keeps its own outcome
TEST(TlsImpl_Tests, CloseWriteFailsWhileAReadRuns) {
    for (int round = 0; round < 3; ++round) {
        auto p = tls_pair();
        ASSERT_TRUE(p.client && p.server);
        net::connection server = p.server;
        std::atomic<bool> stop = false;
        std::atomic<int> ends = 0;
        std::atomic<int> others = 0;
        std::thread reader([&server, &stop, &ends, &others] {
            byte buf[256];
            while (!stop.load()) {
                auto n = server.read(buf);
                if (n && *n == 0) {
                    ends.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else {
                    others.fetch_add(1);
                }
            }
        });
        (void)p.client.close();                                      // close_notify, then the socket closed
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        (void)server.write(sgcl::string("into a closed socket\n"));  // the peer answers with a reset
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        auto cw = server.close_write();                              // its close_notify fails: the write side's alone
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        stop = true;
        reader.join();
        EXPECT_FALSE(cw.has_value());
        EXPECT_GT(ends.load(), 0);
        EXPECT_EQ(others.load(), 0);
        byte buf[16];
        auto r = server.read(buf);
        ASSERT_TRUE(r.has_value()) << std::string(r.error().message().view());
        EXPECT_EQ(*r, 0u);
        EXPECT_FALSE(server.write(sgcl::string("after\n")).has_value());
        (void)server.close();
    }
}

// A read that fails on a record (bad_record_mac) while writes run on another
// thread: the reader gets its own error, the alert goes out, and every write
// after it fails with the read's error. The read side records its failure
// first and then hands the alert over through an atomic the write side takes
// under the record's lock (where it copies the error to its own field); a
// relay between the two flips the tag of the client's records once told to
TEST(TlsImpl_Tests, ReadFailsWhileAWriteRuns) {
    auto front = net::tcp::listen("127.0.0.1:0");
    auto back = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(front);
    ASSERT_TRUE(back);
    // the server's result made in a managed object, not in this thread's stack
    auto served = make_tracked<expected<net::connection, io::error>>(unexpected(io::error(io::errc::closed, "accept", "")));
    std::thread accept_t([&] {
        auto a = back->accept();
        if (a) {
            *served = tls::server(*a, server_config());
        }
    });
    std::atomic<bool> corrupt = false;
    std::thread relay([&] {
        auto from_client = front->accept();
        if (!from_client) {
            return;
        }
        auto to_server = net::tcp::connect(back->local_endpoint());
        if (!to_server) {
            (void)from_client->close();
            return;
        }
        net::connection c = *from_client, s = *to_server;
        std::thread down([&c, &s] {
            std::byte b[16384];
            for (;;) {
                auto n = s.read(b);
                if (!n || *n == 0 || !c.write(sgcl::slice<const std::byte>(b, *n))) {
                    break;
                }
            }
            (void)c.close();
        });
        std::byte b[16384];
        for (;;) {
            auto n = c.read(b);
            if (!n || *n == 0) {
                break;
            }
            if (corrupt.load()) {
                b[*n - 1] ^= std::byte{1};   // the tag of the last record
            }
            if (!s.write(sgcl::slice<const std::byte>(b, *n))) {
                break;
            }
        }
        (void)s.close();
        down.join();
    });
    auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(front->local_endpoint().port())), client_config("localhost"));
    accept_t.join();
    ASSERT_TRUE(c);
    ASSERT_TRUE(*served);
    net::connection client = *c;
    net::connection server = **served;
    std::atomic<bool> stop = false;
    std::thread drain([&client, &stop] {   // the client reads what the server writes
        byte buf[16384];
        while (!stop.load()) {
            auto n = client.read(buf);
            if (!n || *n == 0) {
                break;
            }
        }
    });
    std::atomic<bool> read_failed = false;
    std::atomic<int> writes_after = 0;
    std::string after_error;   // the writer's, read after it is joined
    std::thread writer([&server, &stop, &read_failed, &writes_after, &after_error] {
        sgcl::string chunk("0123456789abcdef0123456789abcdef");
        while (!stop.load()) {
            const bool after = read_failed.load();
            auto w = server.write(chunk);
            if (after) {
                writes_after.fetch_add(1);
                if (!w && after_error.empty()) {
                    after_error = std::string(w.error().message().view());
                } else if (w) {
                    after_error = "a write succeeded after the read failed";
                }
            }
            if (!w) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    corrupt = true;
    (void)client.write(sgcl::string("this record arrives broken"));
    byte buf[256];
    auto r = server.read(buf);
    read_failed = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    stop = true;
    writer.join();
    ASSERT_FALSE(r.has_value());
    const std::string m(r.error().message().view());
    EXPECT_NE(m.find("bad record MAC"), std::string::npos) << m;
    EXPECT_EQ(tls::alert_of(r.error()), tls::alert::bad_record_mac);
    EXPECT_FALSE(tls::is_remote(r.error()));
    EXPECT_GT(writes_after.load(), 0);
    EXPECT_EQ(after_error, m);
    // on this thread too, and the read again: the same error
    auto w = server.write(sgcl::string("after"));
    ASSERT_FALSE(w.has_value());
    EXPECT_EQ(std::string(w.error().message().view()), m);
    auto again = server.read(buf);
    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(std::string(again.error().message().view()), m);
    (void)server.close();
    (void)client.close();
    drain.join();
    (void)front->close();
    (void)back->close();
    relay.join();
}
