//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::tls at its boundaries (DESIGN 408), over pairs in memory: every
// alert's number in a handshake, either side reading it; the records of a
// write at 2^14 and 2^14 + 1 bytes, and a peer's record past 2^14; a peer
// that ends the stream part way through a record, in the handshake and
// after it; the handshake's timeout at zero, below it and at the maximum;
// a config moved from, the ALPN protocol's length at 255 and 256. The
// record layer's own limits are tls_record.cpp's (ProtectedRecordErrors,
// PlaintextRecordErrors); the alerts of a failed handshake as the other
// side words them, tls_impl.cpp's; a close during a read or a write,
// TlsImpl_Tests.CloseEndsAReadAndAWriteInProgress. A thread takes a
// connection by reference: a tracked handle is never copied into a
// std::thread's state.
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/net/tls.h"

#include <atomic>
#include <fstream>
#include <functional>
#include <latch>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace tls = sgcl::net::tls;

namespace {
    using bytes_t = std::vector<uint8_t>;

    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    tls::config server_config() {
        tls::config c;
        c.identities = {tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        return c;
    }

    tls::config client_config() {
        tls::config c;
        c.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
        c.server_name = sgcl::string("localhost");
        return c;
    }

    // One record from a raw end: its header and its body, or empty at the end
    bytes_t read_record(const net::connection& c) {
        bytes_t r(5);
        auto h = c.read_full(slice<byte>(reinterpret_cast<byte*>(r.data()), 5));
        if (!h || *h != 5) {
            return {};
        }
        size_t n = size_t(r[3]) << 8 | r[4];
        r.resize(5 + n);
        auto b = c.read_full(slice<byte>(reinterpret_cast<byte*>(r.data() + 5), n));
        if (!b || *b != n) {
            return {};
        }
        return r;
    }

    bool write_bytes(const net::connection& c, const bytes_t& b) {
        return bool(c.write(slice<const byte>(reinterpret_cast<const byte*>(b.data()), b.size())));
    }

    // Everything to the end of a raw end, dropped
    void drain(const net::connection& c) {
        byte b[4096];
        for (;;) {
            auto n = c.read(b);
            if (!n || *n == 0) {
                return;
            }
        }
    }

    // A client and a server of ours over two pairs in memory, a relay
    // between them that passes the records whole and keeps their sizes,
    // each way; `cut`, when set, is called for every record from the
    // server and may end the relay part way through it
    struct Relay {
        std::mutex m;
        std::vector<std::pair<uint8_t, size_t>> to_client, to_server;   // (type, length) of every record
        std::function<size_t(uint8_t type, size_t length)> cut;         // the bytes of the record to pass (all: SIZE_MAX)
        std::atomic<bool> was_cut = false;
    };

    struct Handshaken {
        expected<net::connection, io::error> client, server;
        net::connection client_end, server_end;   // the relay's own ends
    };

    void pump(net::connection from, net::connection to, Relay& relay, bool towards_client) {
        for (;;) {
            auto r = read_record(from);
            if (r.empty()) {
                break;
            }
            size_t pass = r.size();
            if (towards_client && relay.cut) {
                pass = std::min(pass, relay.cut(r[0], r.size() - 5));
            }
            {
                std::lock_guard lock(relay.m);
                (towards_client ? relay.to_client : relay.to_server).emplace_back(r[0], r.size() - 5);
            }
            if (pass < r.size()) {
                r.resize(pass);
                (void)write_bytes(to, r);
                relay.was_cut = true;
                (void)to.close();
                (void)from.close();
                return;
            }
            if (!write_bytes(to, r)) {
                break;
            }
        }
        (void)to.close_write();
    }

    // A pump on a thread of its own, its ends copied onto the thread's stack
    // before the caller goes on: a tracked handle lives on a stack or in a
    // managed object, never in a std::thread's state
    std::thread start_pump(const net::connection& from, const net::connection& to, Relay& relay, bool towards_client) {
        std::latch copied(1);
        std::thread t([&] {
            net::connection f = from;
            net::connection o = to;
            Relay& r = relay;
            const bool towards = towards_client;
            copied.count_down();
            pump(f, o, r, towards);
        });
        copied.wait();
        return t;
    }

    Handshaken handshake_through(Relay& relay, tls::config ccfg = client_config(), tls::config scfg = server_config(), std::thread* threads = nullptr) {
        auto [c, cr] = net::connection::in_memory();
        auto [s, sr] = net::connection::in_memory();
        threads[0] = start_pump(cr, sr, relay, false);
        threads[1] = start_pump(sr, cr, relay, true);
        Handshaken h{unexpected(io::error(io::errc::closed, "", "")), unexpected(io::error(io::errc::closed, "", "")), cr, sr};
        // the server's result made in a managed object, not in this thread's stack
        auto served = make_tracked<expected<net::connection, io::error>>(unexpected(io::error(io::errc::closed, "", "")));
        std::thread server([&] { *served = tls::server(s, scfg); });
        h.client = tls::client(c, ccfg);
        server.join();
        h.server = *served;
        return h;
    }
}

// Every number an alert may carry (the 27 of RFC 8446 §6 and the 229 it
// does not name), at either level and at levels that are neither, as the
// first answer to a client's hello and as the first record a server reads:
// the handshake fails with that alert, the peer's (is_remote), worded by
// its name or by its number, the client's said to come before the
// server's hello; nothing waits for more
TEST(TlsBounds, EveryAlertInAHandshake) {
    for (int d = 0; d < 256; ++d) {
        const uint8_t level = uint8_t(d % 4);   // 0, 1 (warning), 2 (fatal), 3: the level is not read
        {
            auto [c, raw] = net::connection::in_memory();
            std::thread peer([&raw, d, level] {
                auto hello = read_record(raw);
                if (!hello.empty()) {
                    (void)write_bytes(raw, {21, 3, 3, 0, 2, level, uint8_t(d)});
                }
                drain(raw);
            });
            auto r = tls::client(c, client_config());
            (void)c.close();
            peer.join();
            ASSERT_FALSE(r) << d;
            ASSERT_TRUE(tls::alert_of(r.error())) << d << ": " << text(r.error().message());
            EXPECT_EQ(int(*tls::alert_of(r.error())), d);
            EXPECT_TRUE(tls::is_remote(r.error())) << d;
            EXPECT_FALSE(tls::certificate_reason(r.error())) << d;
            const std::string m = text(r.error().message());
            EXPECT_NE(m.find("remote error: tls: "), std::string::npos) << m;
            EXPECT_NE(m.find("before the server's hello"), std::string::npos) << m;
            if (!sgcl::net::tls::detail::alert_text(d)) {
                EXPECT_NE(m.find("alert(" + std::to_string(d) + ")"), std::string::npos) << m;
            }
            EXPECT_TRUE(c.is_closed());   // a handshake that fails closes the transport
        }
        {
            auto [s, raw] = net::connection::in_memory();
            std::thread peer([&raw, d, level] {
                (void)write_bytes(raw, {21, 3, 3, 0, 2, level, uint8_t(d)});
                drain(raw);
            });
            auto r = tls::server(s, server_config());
            (void)s.close();
            peer.join();
            ASSERT_FALSE(r) << d;
            ASSERT_TRUE(tls::alert_of(r.error())) << d << ": " << text(r.error().message());
            EXPECT_EQ(int(*tls::alert_of(r.error())), d);
            EXPECT_TRUE(tls::is_remote(r.error())) << d;
            EXPECT_EQ(text(r.error().message()).find("before the server's hello"), std::string::npos);
        }
    }
    // an alert record that is not two bytes: this side's decode_error
    for (bytes_t bad : {bytes_t{21, 3, 3, 0, 1, 2}, bytes_t{21, 3, 3, 0, 3, 2, 40, 0}, bytes_t{21, 3, 3, 0, 0}}) {
        auto [s, raw] = net::connection::in_memory();
        std::thread peer([&raw, bad] {
            (void)write_bytes(raw, bad);
            drain(raw);
        });
        auto r = tls::server(s, server_config());
        (void)s.close();
        peer.join();
        ASSERT_FALSE(r);
        ASSERT_TRUE(tls::alert_of(r.error())) << text(r.error().message());
        EXPECT_FALSE(tls::is_remote(r.error()));
        EXPECT_TRUE(*tls::alert_of(r.error()) == tls::alert::decode_error || *tls::alert_of(r.error()) == tls::alert::unexpected_message)
            << text(r.error().message());
    }
}

// A write is cut into records of 2^14 bytes at most: 2^14 is one record,
// 2^14 + 1 two, 0 none; what comes out the other side is what went in
TEST(TlsBounds, RecordsOfAWriteAtTheirLimit) {
    Relay relay;
    std::thread pumps[2];
    auto h = handshake_through(relay, client_config(), server_config(), pumps);
    ASSERT_TRUE(h.client) << text(h.client.error().message());
    ASSERT_TRUE(h.server) << text(h.server.error().message());
    auto app_records = [&] {
        std::lock_guard lock(relay.m);
        std::vector<size_t> out;
        for (auto& [type, n] : relay.to_server) {
            if (type == 23) {
                out.push_back(n);
            }
        }
        return out;
    };
    const size_t before = app_records().size();   // the client's Finished went as one
    for (size_t n : {size_t(0), size_t(16384), size_t(16385), size_t(3 * 16384)}) {
        std::vector<byte> data(n);
        for (size_t i = 0; i < n; ++i) {
            data[i] = byte(uint8_t(i * 7 + n));
        }
        std::vector<byte> back(n);
        std::thread reader([&] {
            if (n) {
                auto r = h.server->read_full(back);
                EXPECT_TRUE(r) << n;
            }
        });
        auto w = h.client->write(slice<const byte>(data.data(), n));
        reader.join();
        ASSERT_TRUE(w) << n;
        EXPECT_EQ(*w, n);
        EXPECT_EQ(back, data) << n;
    }
    auto records = app_records();
    std::vector<size_t> sizes(records.begin() + std::ptrdiff_t(before), records.end());
    const size_t full = 16384 + 1 + 16, one = 1 + 1 + 16;   // the content, its type, the tag
    EXPECT_EQ(sizes, (std::vector<size_t>{full, full, one, full, full, full})) << sizes.size();
    (void)h.client->close();
    (void)h.server->close();
    pumps[0].join();
    pumps[1].join();
}

// A record a peer sends before the keys: 2^14 bytes is a record (the
// message in it is what is refused), 2^14 + 1 is record_overflow, this
// side's alert; a ciphertext's length past 2^14 + 256 the same
TEST(TlsBounds, APeersRecordPastTheLimit) {
    for (size_t n : {size_t(16384), size_t(16385), size_t(16384 + 256 + 1), size_t(65535)}) {
        auto [s, raw] = net::connection::in_memory();
        bytes_t rec = {22, 3, 1, uint8_t(n >> 8), uint8_t(n)};
        rec.resize(5 + n, 0);
        rec[5] = 1;   // a ClientHello of n - 4 bytes of zeros
        rec[6] = uint8_t((n - 4) >> 16);
        rec[7] = uint8_t((n - 4) >> 8);
        rec[8] = uint8_t(n - 4);
        // the record written and the answer read at once: a pair in memory
        // holds nothing, and the server answers the header before the body
        std::thread writer([&raw, rec] { (void)write_bytes(raw, rec); });
        std::thread peer([&raw] { drain(raw); });
        auto r = tls::server(s, server_config());
        (void)s.close();
        writer.join();
        peer.join();
        ASSERT_FALSE(r) << n;
        ASSERT_TRUE(tls::alert_of(r.error())) << n << ": " << text(r.error().message());
        EXPECT_FALSE(tls::is_remote(r.error()));
        if (n == 16384) {
            EXPECT_NE(*tls::alert_of(r.error()), tls::alert::record_overflow) << text(r.error().message());
        } else {
            EXPECT_EQ(*tls::alert_of(r.error()), tls::alert::record_overflow) << n << ": " << text(r.error().message());
        }
    }
}

// The stream ending part way through a record is unexpected_eof, in the
// handshake (the server's flight cut) and after it (a record of data cut);
// at a record's boundary without close_notify, after the handshake, it is
// the end of the stream, a read of 0
TEST(TlsBounds, APeerThatClosesMidRecord) {
    for (size_t keep : {size_t(0), size_t(3), size_t(5), size_t(6), size_t(40)}) {   // in the header, at its end, in the body
        Relay relay;
        relay.cut = [keep](uint8_t type, size_t) { return type == 22 ? keep : SIZE_MAX; };   // the ServerHello's record
        std::thread pumps[2];
        auto h = handshake_through(relay, client_config(), server_config(), pumps);
        pumps[0].join();
        pumps[1].join();
        EXPECT_TRUE(relay.was_cut);
        ASSERT_FALSE(h.client) << keep;
        const auto& e = h.client.error();
        if (keep == 0) {
            EXPECT_TRUE(e.is_eof() || e.is_closed()) << keep << ": " << text(e.message());
        } else {
            EXPECT_EQ(e.code(), io::errc::unexpected_eof) << keep << ": " << text(e.message());
        }
        if (h.server) {
            (void)h.server->close();
        }
    }
    {
        Relay relay;
        std::atomic<bool> armed = false;
        relay.cut = [&](uint8_t type, size_t n) { return type == 23 && armed ? 5 + n / 2 : SIZE_MAX; };   // the first record of data, half of it
        std::thread pumps[2];
        auto h = handshake_through(relay, client_config(), server_config(), pumps);
        ASSERT_TRUE(h.client) << text(h.client.error().message());
        ASSERT_TRUE(h.server);
        armed = true;
        std::thread writer([&] { (void)h.server->write("cut in the middle"); });
        byte b[64];
        auto r = h.client->read(b);
        writer.join();
        pumps[0].join();
        pumps[1].join();
        EXPECT_TRUE(relay.was_cut);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), io::errc::unexpected_eof) << text(r.error().message());
        (void)h.client->close();
        (void)h.server->close();
    }
    {
        Relay relay;
        std::thread pumps[2];
        auto h = handshake_through(relay, client_config(), server_config(), pumps);
        ASSERT_TRUE(h.client && h.server);
        (void)h.server_end.close();   // the server's records end at a boundary, no close_notify
        byte b[8];
        auto r = h.client->read(b);
        ASSERT_TRUE(r) << text(r.error().message());
        EXPECT_EQ(*r, 0u);
        (void)h.client->close();
        (void)h.server->close();
        pumps[0].join();
        pumps[1].join();
    }
}

// The handshake's timeout: zero or less has passed before it starts
// (ETIMEDOUT, the transport closed, nothing sent); the maximum is none, the
// handshake done and no deadline left on the transport
TEST(TlsBounds, HandshakeTimeouts) {
    for (auto t : {duration(), duration(-1 * sgcl::second), duration::min()}) {
        auto [c, raw] = net::connection::in_memory();
        auto ccfg = client_config();
        ccfg.handshake_timeout = t;
        std::atomic<bool> got = false;
        std::thread peer([&raw, &got] {
            byte b[1];
            auto n = raw.read(b);
            got = n && *n > 0;
        });
        auto r = tls::client(c, ccfg);
        ASSERT_FALSE(r);
        EXPECT_TRUE(r.error().is_timeout()) << text(r.error().message());
        EXPECT_TRUE(c.is_closed());
        peer.join();
        EXPECT_FALSE(got);
        auto [s, raw2] = net::connection::in_memory();
        auto scfg = server_config();
        scfg.handshake_timeout = t;
        auto sr = spawn(tls::async_server(s, scfg)).wait();
        ASSERT_FALSE(sr);
        EXPECT_TRUE(sr.error().is_timeout()) << text(sr.error().message());
        (void)raw2.close();
        auto l = tls::listen("127.0.0.1:0", scfg);   // a listener drops each handshake: none ready
        ASSERT_TRUE(l);
        (void)l->close();
    }
    Relay relay;
    std::thread pumps[2];
    auto ccfg = client_config();
    auto scfg = server_config();
    ccfg.handshake_timeout = duration::max();
    scfg.handshake_timeout = duration::max();
    auto h = handshake_through(relay, ccfg, scfg, pumps);
    ASSERT_TRUE(h.client) << text(h.client.error().message());
    ASSERT_TRUE(h.server) << text(h.server.error().message());
    EXPECT_EQ(h.client->read_deadline(), time_point());
    EXPECT_EQ(h.client->write_deadline(), time_point());
    (void)h.client->close();
    (void)h.server->close();
    pumps[0].join();
    pumps[1].join();
    sgcl::async::scheduler::stop();
}

// A config moved from has no cipher suite and no group (its vectors are
// empty): refused with EINVAL before anything is sent, the transport left
// as it was; an identity moved from is the same identity (a handle); the
// ALPN protocol's length at 255 is taken, 0 and 256 refused
TEST(TlsBounds, ConfigsAndIdentitiesAtTheirEdges) {
    auto ccfg = client_config();
    auto moved = std::move(ccfg);
    auto [c, raw] = net::connection::in_memory();
    auto r = tls::client(c, ccfg);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::errc::invalid_argument) << text(r.error().message());
    EXPECT_FALSE(c.is_closed());
    auto scfg = server_config();
    auto id = scfg.identities[0];
    auto id_moved = std::move(scfg.identities[0]);
    EXPECT_EQ(id.certificates().size(), id_moved.certificates().size());
    EXPECT_EQ(&scfg.identities[0].certificates(), &id.certificates());   // the same state
    auto sm = std::move(scfg);
    auto sr = tls::server(c, scfg);
    ASSERT_FALSE(sr);
    EXPECT_EQ(sr.error().code(), std::errc::invalid_argument);
    EXPECT_FALSE(c.is_closed());
    auto lr = tls::listen("127.0.0.1:0", scfg);
    ASSERT_FALSE(lr);
    EXPECT_EQ(lr.error().code(), std::errc::invalid_argument);
    (void)c.close();
    (void)raw.close();
    // ALPN at its edges
    for (size_t n : {size_t(0), size_t(256)}) {
        auto a = client_config();
        a.alpn = {sgcl::string(std::string(n, 'p'))};
        auto [x, y] = net::connection::in_memory();
        auto ar = tls::client(x, a);
        ASSERT_FALSE(ar) << n;
        EXPECT_EQ(ar.error().code(), std::errc::invalid_argument) << n;
        auto b = server_config();
        b.alpn = a.alpn;
        auto br = tls::server(x, b);
        ASSERT_FALSE(br) << n;
        EXPECT_EQ(br.error().code(), std::errc::invalid_argument) << n;
        (void)x.close();
        (void)y.close();
    }
    Relay relay;
    std::thread pumps[2];
    auto a = client_config();
    auto b = server_config();
    const sgcl::string longest(std::string(255, 'p'));
    a.alpn = {longest};
    b.alpn = {sgcl::string("h2"), longest};
    auto h = handshake_through(relay, a, b, pumps);
    ASSERT_TRUE(h.client) << text(h.client.error().message());
    ASSERT_TRUE(h.server) << text(h.server.error().message());
    EXPECT_EQ(tls::state_of(*h.client)->alpn, longest);
    EXPECT_EQ(tls::state_of(*h.server)->alpn, longest);
    EXPECT_FALSE(tls::state_of(net::connection()));
    EXPECT_FALSE(tls::state_of(h.client_end));
    // the empty read and write of a TLS connection: 0, nothing on the wire
    byte none[1];
    auto e = h.client->read(slice<byte>(none).first(0));
    ASSERT_TRUE(e);
    EXPECT_EQ(*e, 0u);
    auto w = h.client->write(slice<const byte>());
    ASSERT_TRUE(w);
    EXPECT_EQ(*w, 0u);
    (void)h.client->close();
    (void)h.server->close();
    pumps[0].join();
    pumps[1].join();
    // an identity of nothing, of a key that is not the leaf's
    EXPECT_FALSE(tls::identity::from_pem(sgcl::string(), slice<const byte>()));
    EXPECT_FALSE(tls::identity::from_pem(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string()));
    EXPECT_FALSE(tls::identity::from_pem(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("rsa.key")))));
    EXPECT_THROW(tls::identity(sgcl::string(), sgcl::string()), std::invalid_argument);
}

// The codes of the category at their ends: an alert's code and the
// peer's, a reason past the last, a value no alert has
TEST(TlsBounds, TheCategorysCodes) {
    io::error mine(tls::make_error_code(tls::alert::no_application_protocol), "x", "");
    EXPECT_EQ(tls::alert_of(mine), tls::alert::no_application_protocol);
    EXPECT_FALSE(tls::is_remote(mine));
    EXPECT_EQ(mine.code(), tls::alert::no_application_protocol);
    io::error theirs(error_code(256 + 255, tls::category()), "x", "");
    EXPECT_EQ(int(*tls::alert_of(theirs)), 255);
    EXPECT_TRUE(tls::is_remote(theirs));
    EXPECT_EQ(text(theirs.message()), "x: remote error: tls: alert(255)");
    io::error reason(error_code(1023, tls::category()), "x", "");
    EXPECT_FALSE(tls::alert_of(reason));
    EXPECT_FALSE(tls::is_remote(reason));
    ASSERT_TRUE(tls::certificate_reason(reason));
    EXPECT_EQ(text(reason.message()), "x: tls: certificate does not verify");
    io::error other(std::make_error_code(std::errc::timed_out), "x", "");
    EXPECT_FALSE(tls::alert_of(other));
    EXPECT_FALSE(tls::is_remote(other));
    EXPECT_FALSE(tls::certificate_reason(other));
}
