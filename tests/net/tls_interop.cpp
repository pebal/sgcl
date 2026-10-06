//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The TLS client (sgcl/net/tls.h) against two servers of other hands, run
// here as child processes on a port of this machine's choice:
//
//   - OpenSSL's s_server (/opt/homebrew/opt/openssl@3/bin/openssl, or
//     $SGCL_OPENSSL): every cipher suite × every group of v1 × a leaf of
//     each kind (Ed25519, ECDSA P-256, RSA: rsa_pss_rsae), the server given
//     one group only, so that P-256 and P-384 take a HelloRetryRequest;
//     lines echoed reversed (-rev), a line of 100 KB across records both
//     ways; the close_notify of close() and async_close() seen by the
//     server (-msg); the certificate checks (a wrong name, an unknown
//     authority, an IP address);
//   - Go's crypto/tls (tools/tls_oracle.go, built here with `go build`):
//     the same matrix, the cipher suite chosen by the client's offer
//     (Go's TLS 1.3 suites are not configurable), ALPN;
//   - resumption and client certificates both ways with each: Go's tickets
//     resumed by our client and ours by Go's ClientSessionCache, s_server's
//     by our client and ours by s_client's -sess_out/-sess_in; a client
//     certificate of ours verified by Go (RequireAndVerifyClientCert) and by
//     s_server (-Verify), Go's and s_client's (-cert) by our server, a
//     missing one refused with certificate_required, and a resumed session
//     that keeps the client's chain.
//
// A test whose program is not on this machine is skipped with the reason.
// The certificates are tests/net/tls_testdata (tools/tls_testdata.sh).
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/io/exec.h"
#include "sgcl/net/tls.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>

namespace tls = sgcl::net::tls;

namespace {
    std::string testdata() {
        return (source_root() / "tests/net/tls_testdata/").string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    std::string openssl_path() {
        if (const char* e = std::getenv("SGCL_OPENSSL")) {
            return e;
        }
        const char* p = "/opt/homebrew/opt/openssl@3/bin/openssl";
        return ::access(p, X_OK) == 0 ? p : "";
    }

    std::string go_path() {
        for (const char* p : {"/opt/homebrew/bin/go", "/usr/local/go/bin/go", "/usr/local/bin/go"}) {
            if (::access(p, X_OK) == 0) {
                return p;
            }
        }
        return "";
    }

    std::string scratch() {
        static int n = 0;
        auto d = std::filesystem::temp_directory_path() / ("sgcl_tls_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        std::filesystem::create_directories(d);
        return d.string();
    }

    uint16_t free_port() {
        auto l = net::tcp::listen("127.0.0.1:0");
        EXPECT_TRUE(l.has_value());
        uint16_t p = l->local_endpoint().port();
        (void)l->close();
        return p;
    }

    // A child whose output goes to a file, read as it grows
    struct Child {
        io::command cmd;
        std::string log;
        bool started = false;

        Child(const std::string& program, sgcl::vector<sgcl::string> args)
        : cmd(sgcl::string(program), std::move(args)) {
        }

        bool start() {
            log = scratch() + "/out.log";
            auto f = io::create(sgcl::string(log));
            if (!f) {
                return false;
            }
            cmd.out = *f;
            cmd.err = *f;
            started = (bool)cmd.start();
            (void)f->close();
            return started;
        }

        // The output once it holds `what` (empty: nothing found in time)
        std::string wait_for(const std::string& what, int ms = 10000) {
            for (int i = 0; i < ms / 10; ++i) {
                std::string s = slurp(log);
                if (s.find(what) != std::string::npos) {
                    return s;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return "";
        }

        void finish() {
            if (started) {
                (void)cmd.process.kill();
                (void)cmd.wait();
                started = false;
            }
        }

        ~Child() {
            finish();
        }
    };

    tls::config trusted() {
        tls::config c;
        c.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata() + "ca.pem")));
        c.server_name = sgcl::string("localhost");
        return c;
    }

    const char* cipher_name(tls::cipher c) {
        switch (c) {
            case tls::cipher::aes_128_gcm_sha256: return "TLS_AES_128_GCM_SHA256";
            case tls::cipher::aes_256_gcm_sha384: return "TLS_AES_256_GCM_SHA384";
            case tls::cipher::chacha20_poly1305_sha256: return "TLS_CHACHA20_POLY1305_SHA256";
        }
        return "";
    }

    const char* group_name(tls::group g) {
        switch (g) {
            case tls::group::x25519_mlkem768: return "X25519MLKEM768";
            case tls::group::x25519: return "X25519";
            case tls::group::secp256r1: return "P-256";
            case tls::group::secp384r1: return "P-384";
            case tls::group::secp521r1: return "P-521";
        }
        return "";
    }

    constexpr tls::cipher ciphers[] = {tls::cipher::aes_128_gcm_sha256, tls::cipher::aes_256_gcm_sha384, tls::cipher::chacha20_poly1305_sha256};
    constexpr tls::group groups[] = {tls::group::x25519_mlkem768, tls::group::x25519, tls::group::secp256r1, tls::group::secp384r1};
    constexpr const char* leaves[] = {"ed25519", "ecdsa", "rsa"};

    // An s_server for one connection, the group and the leaf given
    struct SServer : Child {
        uint16_t port;

        SServer(const std::string& leaf, const char* group, uint16_t p = free_port())
        : Child(openssl_path(), {sgcl::string("s_server"), sgcl::string("-accept"), sgcl::string("127.0.0.1:" + std::to_string(p)),
                                 sgcl::string("-cert"), sgcl::string(testdata() + leaf + ".pem"), sgcl::string("-key"), sgcl::string(testdata() + leaf + ".key"),
                                 sgcl::string("-tls1_3"), sgcl::string("-groups"), sgcl::string(group), sgcl::string("-naccept"), sgcl::string("1"),
                                 sgcl::string("-rev"), sgcl::string("-msg")})
        , port(p) {
        }

        std::string address() const {
            return "127.0.0.1:" + std::to_string(port);
        }
    };

    // A line written and its reversal read back
    void echo(const net::connection& c, const std::string& line) {
        auto w = c.write(sgcl::string(line + "\n"));
        ASSERT_TRUE(w.has_value()) << std::string(w.error().message().view());
        auto r = c.read_line();
        ASSERT_TRUE(r.has_value()) << std::string(r.error().message().view());
        ASSERT_TRUE(r->has_value());
        std::string back(r->value().view());
        std::string expected(line.rbegin(), line.rend());
        EXPECT_EQ(back.size(), expected.size());
        EXPECT_TRUE(back == expected);
    }
}

TEST(TlsInterop, OpenSslMatrix) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL at /opt/homebrew/opt/openssl@3/bin/openssl ($SGCL_OPENSSL)";
    }
    size_t n = 0;
    for (auto c : ciphers) {
        for (auto g : groups) {
            for (const char* leaf : leaves) {
                SCOPED_TRACE(std::string(cipher_name(c)) + " " + group_name(g) + " " + leaf);
                SServer s(leaf, group_name(g));
                ASSERT_TRUE(s.start());
                ASSERT_FALSE(s.wait_for("ACCEPT").empty());
                tls::config cfg = trusted();
                cfg.ciphers = {c};
                // every group offered, the shares of the first two: the
                // server's one group met at once or after a HelloRetryRequest
                auto conn = tls::connect(sgcl::string(s.address()), cfg);
                ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
                auto st = tls::state_of(*conn);
                ASSERT_TRUE(st.has_value());
                EXPECT_EQ(st->cipher, c);
                EXPECT_EQ(st->group, g);
                EXPECT_EQ(std::string(st->server_name.view()), "localhost");
                EXPECT_EQ(st->peer_certificates.size(), 1u);
                echo(*conn, "hello sgcl");
                (void)conn->close();
                ++n;
            }
        }
    }
    EXPECT_EQ(n, 36u);
}

// Records past 2^14 bytes both ways, the async forms, the close_notify
TEST(TlsInterop, OpenSslLongLinesAndClose) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    {
        SServer s("ecdsa", "X25519MLKEM768");
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty());
        auto conn = tls::async_connect(sgcl::string(s.address()), trusted()).wait();
        ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
        conn->set_max_line(1 << 20);
        // (s_server -rev answers at most 16 KB per line: the long lines are Go's)
        echo(*conn, "in a task");
        echo(*conn, "then closed in a task");
        auto closed = conn->async_close().wait();
        EXPECT_TRUE(closed.has_value());
        EXPECT_FALSE(s.wait_for("warning close_notify").empty()) << slurp(s.log);
    }
    {
        // the blocking close: close_notify without waiting
        SServer s("rsa", "X25519");
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty());
        auto t = net::tcp::connect(sgcl::string(s.address()));
        ASSERT_TRUE(t.has_value());
        auto conn = tls::client(*t, trusted());
        ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
        echo(*conn, "blocking");
        EXPECT_TRUE(conn->close().has_value());
        EXPECT_FALSE(s.wait_for("warning close_notify").empty()) << slurp(s.log);
        EXPECT_TRUE(conn->is_closed());
    }
}

// A KeyUpdate from the server that asks for one back (s_server's "K"
// command): the client's read takes the new keys and answers; data flows
// both ways after it
TEST(TlsInterop, OpenSslKeyUpdate) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    uint16_t port = free_port();
    Child s(openssl_path(), {sgcl::string("s_server"), sgcl::string("-accept"), sgcl::string("127.0.0.1:" + std::to_string(port)),
                             sgcl::string("-cert"), sgcl::string(testdata() + "ecdsa.pem"), sgcl::string("-key"), sgcl::string(testdata() + "ecdsa.key"),
                             sgcl::string("-tls1_3"), sgcl::string("-naccept"), sgcl::string("1"), sgcl::string("-msg")});
    auto in = s.cmd.stdin_pipe();
    ASSERT_TRUE(in.has_value());
    ASSERT_TRUE(s.start());
    ASSERT_FALSE(s.wait_for("ACCEPT").empty());
    auto conn = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(port)), trusted());
    ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
    ASSERT_TRUE(conn->write(sgcl::string("before\n")).has_value());
    ASSERT_FALSE(s.wait_for("before").empty());
    ASSERT_TRUE(in->write(sgcl::string("K\n")).has_value());   // KeyUpdate, update_requested
    ASSERT_FALSE(s.wait_for("KeyUpdate").empty());
    ASSERT_TRUE(in->write(sgcl::string("from the server\n")).has_value());
    auto line = conn->read_line();   // the KeyUpdate taken on the way, answered
    ASSERT_TRUE(line.has_value()) << std::string(line.error().message().view());
    ASSERT_TRUE(line->has_value());
    EXPECT_EQ(std::string(line->value().view()), "from the server");
    ASSERT_TRUE(conn->write(sgcl::string("after the update\n")).has_value());
    std::string log = s.wait_for("after the update");
    ASSERT_FALSE(log.empty()) << slurp(s.log);
    // the server sent one KeyUpdate and read one back
    size_t sent = log.find(">>> TLS 1.3, Handshake [length 0005], KeyUpdate");
    size_t got = log.find("<<< TLS 1.3, Handshake [length 0005], KeyUpdate");
    EXPECT_NE(sent, std::string::npos) << log;
    EXPECT_NE(got, std::string::npos) << log;
    (void)conn->close();
    (void)in->close();
}

// The server's chain: a name it does not hold, an authority not trusted,
// an address it holds, no check at all
TEST(TlsInterop, OpenSslCertificateChecks) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    struct Case {
        const char* name;
        bool trusted;
        bool insecure;
        optional<crypto::x509::reason> reason;
    };
    for (auto k : {Case{"wrong.example", true, false, crypto::x509::reason::hostname_mismatch},
                   Case{"localhost", false, false, crypto::x509::reason::unknown_authority},
                   Case{"127.0.0.1", true, false, nullopt},
                   Case{"", false, true, nullopt}}) {
        SCOPED_TRACE(k.name);
        SServer s("ecdsa", "X25519");
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty());
        tls::config cfg = trusted();
        if (!k.trusted) {
            cfg.roots = crypto::x509::certificate_pool();
        }
        cfg.server_name = sgcl::string(k.name);
        cfg.insecure_skip_verify = k.insecure;
        auto conn = tls::connect(sgcl::string(s.address()), cfg);
        if (k.reason) {
            ASSERT_FALSE(conn.has_value());
            EXPECT_EQ(tls::certificate_reason(conn.error()), k.reason);
            EXPECT_EQ(conn.error().code().category(), tls::category());
            EXPECT_EQ(std::string(conn.error().message().view()).find("handshake tls "), 0u) << std::string(conn.error().message().view());
            EXPECT_FALSE(s.wait_for("Alert").empty());   // the server was told
        } else {
            ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
            echo(*conn, "checked");
        }
    }
    // the errors of the category
    io::error remote(std::error_code(256 + 42, tls::category()), "read", "x");
    EXPECT_TRUE(tls::is_remote(remote));
    EXPECT_EQ(tls::alert_of(remote), tls::alert::bad_certificate);
    EXPECT_EQ(std::string(remote.message().view()), "read x: remote error: tls: bad certificate");
    io::error local(tls::alert::decode_error, "handshake", "x");
    EXPECT_EQ(local.code(), tls::alert::decode_error);
    EXPECT_FALSE(tls::is_remote(local));
    EXPECT_EQ(std::string(local.message().view()), "handshake x: tls: error decoding message");
}

namespace {
    // tools/tls_oracle.go built once
    std::string go_oracle() {
        static std::string built = [] {
            if (go_path().empty()) {
                return std::string();
            }
            std::string src = (source_root() / "tools/tls_oracle.go").string();
            std::string bin = scratch() + "/tls_oracle";
            io::command b(sgcl::string(go_path()), sgcl::string("build"), sgcl::string("-o"), sgcl::string(bin), sgcl::string(src));
            auto r = b.combined_output();
            return r.has_value() ? bin : std::string();
        }();
        return built;
    }

    struct GoServer : Child {
        GoServer(const std::string& leaf, int curve, const std::string& alpn = "")
        : Child(go_oracle(), {sgcl::string("-cert"), sgcl::string(testdata() + leaf + ".pem"), sgcl::string("-key"), sgcl::string(testdata() + leaf + ".key"),
                              sgcl::string("-curves"), sgcl::string(std::to_string(curve)), sgcl::string("-alpn"), sgcl::string(alpn)}) {
        }

        std::string address() {
            std::string s = wait_for("LISTEN ");
            auto at = s.find("LISTEN ");
            if (at == std::string::npos) {
                return "";
            }
            return "127.0.0.1:" + s.substr(at + 7, s.find('\n', at) - at - 7);
        }
    };
}

TEST(TlsInterop, GoMatrix) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty()) << "go build tools/tls_oracle.go failed";
    size_t n = 0;
    for (auto c : ciphers) {
        for (auto g : groups) {
            for (const char* leaf : leaves) {
                SCOPED_TRACE(std::string(cipher_name(c)) + " " + group_name(g) + " " + leaf);
                GoServer s(leaf, int(g));
                ASSERT_TRUE(s.start());
                std::string address = s.address();
                ASSERT_FALSE(address.empty()) << slurp(s.log);
                tls::config cfg = trusted();
                cfg.ciphers = {c};
                auto conn = tls::connect(sgcl::string(address), cfg);
                ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
                auto st = tls::state_of(*conn);
                ASSERT_TRUE(st.has_value());
                EXPECT_EQ(st->cipher, c);
                EXPECT_EQ(st->group, g);
                std::string state = s.wait_for("STATE ");
                std::string expected = "STATE " + std::to_string(int(c)) + " " + std::to_string(int(g)) + " ";
                EXPECT_NE(state.find(expected), std::string::npos) << state;
                echo(*conn, "hello go");
                EXPECT_TRUE(conn->async_close().wait().has_value());
                EXPECT_FALSE(s.wait_for("CLOSE_NOTIFY").empty()) << slurp(s.log);
                ++n;
            }
        }
    }
    EXPECT_EQ(n, 36u);
}

// Lines of 100 KB and 1 MB: records of 2^14 bytes both ways, a read
// buffer smaller than a record (the unmanaged path) and larger (straight
// into the reader's buffer)
TEST(TlsInterop, GoLongLines) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    GoServer s("rsa", int(tls::group::x25519_mlkem768));
    ASSERT_TRUE(s.start());
    std::string address = s.address();
    ASSERT_FALSE(address.empty());
    auto conn = tls::connect(sgcl::string(address), trusted());
    ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
    conn->set_max_line(2 << 20);
    for (size_t size : {size_t(100000), size_t(1) << 20}) {
        std::string line;
        for (size_t i = 0; i < size; ++i) {
            line += char('a' + i % 26);
        }
        echo(*conn, line);
    }
    // raw reads of a record's size and more
    ASSERT_TRUE(conn->write(sgcl::string(std::string(40000, 'x') + "\n")).has_value());
    std::vector<uint8_t> got;
    std::vector<uint8_t> buf(70000);
    while (got.size() < 40001) {
        auto n = conn->read(slice<byte>(reinterpret_cast<byte*>(buf.data()), got.empty() ? 100 : buf.size()));
        ASSERT_TRUE(n.has_value());
        ASSERT_GT(*n, 0u);
        got.insert(got.end(), buf.begin(), buf.begin() + long(*n));
    }
    EXPECT_EQ(got.size(), 40001u);
    EXPECT_EQ(got.back(), '\n');
    EXPECT_TRUE(conn->async_close().wait().has_value());
    EXPECT_FALSE(s.wait_for("CLOSE_NOTIFY").empty()) << slurp(s.log);
}

// close() while another thread's write is in progress: no close_notify
// (the record is the writer's), the transport closed, the write ended
TEST(TlsInterop, CloseDuringAWrite) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    GoServer s("ecdsa", int(tls::group::x25519));
    ASSERT_TRUE(s.start());
    std::string address = s.address();
    ASSERT_FALSE(address.empty());
    auto conn = tls::connect(sgcl::string(address), trusted());
    ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
    net::connection c = *conn;
    sgcl::string big(std::string(64 << 20, 'z'));   // one line the server buffers without answering
    std::atomic<bool> ended = false;
    std::thread writer([&] {
        (void)c.write(big);
        ended = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_TRUE(c.close().has_value());
    writer.join();
    EXPECT_TRUE(ended.load());
    EXPECT_TRUE(c.is_closed());
    EXPECT_FALSE(c.write(sgcl::string("after\n")).has_value());
}

TEST(TlsInterop, GoAlpn) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    GoServer s("ed25519", int(tls::group::x25519_mlkem768), "h2,http/1.1");
    ASSERT_TRUE(s.start());
    std::string address = s.address();
    ASSERT_FALSE(address.empty());
    tls::config cfg = trusted();
    cfg.alpn = {sgcl::string("http/1.1"), sgcl::string("h2")};
    auto conn = tls::connect(sgcl::string(address), cfg);
    ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
    auto st = tls::state_of(*conn);
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(std::string(st->alpn.view()), "h2");   // the server's preference among the offered
    EXPECT_NE(s.wait_for("STATE ").find(" h2"), std::string::npos);
    echo(*conn, "alpn");
    (void)conn->close();
}

// A connection without TLS has no state; one over a pair in memory whose
// other end does not speak TLS fails within the handshake's timeout
TEST(TlsInterop, NoTlsState) {
    auto [a, b] = net::connection::in_memory();
    EXPECT_FALSE(tls::state_of(a).has_value());
    tls::config cfg = trusted();
    cfg.handshake_timeout = 200 * sgcl::millisecond;
    auto r = tls::client(a, cfg);
    ASSERT_FALSE(r.has_value());
    EXPECT_TRUE(r.error().is_timeout()) << std::string(r.error().message().view());
    EXPECT_TRUE(a.is_closed());
}

// --- the server ----------------------------------------------------------------

namespace {
    tls::identity identity_of(const std::string& leaf) {
        return tls::identity(sgcl::string(slurp(testdata() + leaf + ".pem")), sgcl::string(slurp(testdata() + leaf + ".key")));
    }

    tls::config server_config(const std::string& leaf) {
        tls::config c;
        c.identities = {identity_of(leaf)};
        return c;
    }

    // One connection served on a thread: accepted, the server's handshake
    // (server()), the first line read and answered reversed, then read on
    // to the end (the client's close_notify)
    // (std types only: it is written on the server's thread, and lives on
    // the test's stack, where a tracked word of another thread may not be)
    struct Seen {
        tls::cipher cipher;
        tls::group group;
        std::string server_name, alpn;
        bool resumed = false;
        std::string peer;   // the client's leaf's common name, empty: none
    };

    struct Served {
        optional<Seen> state;
        std::string line, error;
        bool eof = false;
    };

    void serve_one(const net::listener& l, const tls::config& cfg, Served& out) {
        auto t = l.accept();
        if (!t) {
            out.error = std::string(t.error().message().view());
            return;
        }
        auto c = tls::server(*t, cfg);
        if (!c) {
            out.error = std::string(c.error().message().view());
            return;
        }
        if (auto st = tls::state_of(*c)) {
            out.state = Seen{st->cipher, st->group, std::string(st->server_name.view()), std::string(st->alpn.view()), st->resumed,
                             st->peer_certificates.empty() ? std::string() : std::string(st->peer_certificates[0].subject().common_name().view())};
        }
        auto line = c->read_line();
        if (!line || !line->has_value()) {
            out.error = line ? "no line" : std::string(line.error().message().view());
            return;
        }
        out.line = std::string(line->value().view());
        std::string back(out.line.rbegin(), out.line.rend());
        (void)c->write(sgcl::string(back + "\n"));
        auto end = c->read_line();
        out.eof = end.has_value() && !end->has_value();
        if (!end) {
            out.error = std::string(end.error().message().view());
        }
        (void)c->close();
    }

    std::string address_of(const net::listener& l) {
        return "127.0.0.1:" + std::to_string(l.local_endpoint().port());
    }
}

// Our client and our server over a socket: every suite × group × identity,
// the server given one of each (P-256 and P-384 after a HelloRetryRequest)
TEST(TlsInterop, OurClientOurServer) {
    size_t n = 0;
    for (auto c : ciphers) {
        for (auto g : groups) {
            for (const char* leaf : leaves) {
                SCOPED_TRACE(std::string(cipher_name(c)) + " " + group_name(g) + " " + leaf);
                auto l = net::tcp::listen("127.0.0.1:0");
                ASSERT_TRUE(l.has_value());
                tls::config scfg = server_config(leaf);
                scfg.ciphers = {c};
                scfg.groups = {g};
                Served served;
                std::thread server([&] { serve_one(*l, scfg, served); });
                auto conn = tls::connect(sgcl::string(address_of(*l)), trusted());
                if (conn) {
                    echo(*conn, "ours both ways");
                    auto st = tls::state_of(*conn);
                    ASSERT_TRUE(st.has_value());
                    EXPECT_EQ(st->cipher, c);
                    EXPECT_EQ(st->group, g);
                    (void)conn->async_close().wait();
                }
                server.join();
                (void)l->close();
                ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view()) << " / server: " << served.error;
                EXPECT_TRUE(served.error.empty()) << served.error;
                ASSERT_TRUE(served.state.has_value());
                EXPECT_EQ(served.state->cipher, c);
                EXPECT_EQ(served.state->group, g);
                EXPECT_EQ(served.state->server_name, "localhost");
                EXPECT_EQ(served.line, "ours both ways");
                EXPECT_TRUE(served.eof);   // the client's close_notify
                ++n;
            }
        }
    }
    EXPECT_EQ(n, 36u);
}

namespace {
    // s_client against a server of ours
    sgcl::vector<sgcl::string> s_client_args(const std::string& address, const std::vector<std::string>& extra) {
        sgcl::vector<sgcl::string> a = {sgcl::string("s_client"), sgcl::string("-connect"), sgcl::string(address), sgcl::string("-tls1_3"),
                                        sgcl::string("-CAfile"), sgcl::string(testdata() + "ca.pem"), sgcl::string("-servername"), sgcl::string("localhost"),
                                        sgcl::string("-verify_return_error"), sgcl::string("-msg")};
        for (auto& e : extra) {
            a.push_back(sgcl::string(e));
        }
        return a;
    }
}

TEST(TlsInterop, OpenSslClientMatrix) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    size_t n = 0;
    for (auto c : ciphers) {
        for (auto g : groups) {
            for (const char* leaf : leaves) {
                SCOPED_TRACE(std::string(cipher_name(c)) + " " + group_name(g) + " " + leaf);
                auto l = net::tcp::listen("127.0.0.1:0");
                ASSERT_TRUE(l.has_value());
                Served served;
                tls::config scfg = server_config(leaf);
                std::thread server([&] { serve_one(*l, scfg, served); });
                Child s(openssl_path(), s_client_args(address_of(*l), {"-ciphersuites", cipher_name(c), "-groups", group_name(g)}));
                auto in = s.cmd.stdin_pipe();
                ASSERT_TRUE(in.has_value());
                ASSERT_TRUE(s.start());
                ASSERT_TRUE(in->write(sgcl::string("hello from openssl\n")).has_value());
                std::string out = s.wait_for("lssnepo morf olleh");
                (void)in->close();   // the end of its input: s_client closes (close_notify)
                server.join();
                (void)l->close();
                EXPECT_FALSE(out.empty()) << slurp(s.log);
                EXPECT_NE(out.find("Verification: OK"), std::string::npos);
                EXPECT_TRUE(served.error.empty()) << served.error;
                ASSERT_TRUE(served.state.has_value());
                EXPECT_EQ(served.state->cipher, c);
                EXPECT_EQ(served.state->group, g);
                EXPECT_EQ(served.line, "hello from openssl");
                EXPECT_TRUE(served.eof);
                ++n;
            }
        }
    }
    EXPECT_EQ(n, 36u);
}

// A HelloRetryRequest to s_client (its first share X25519, the server's
// one group P-256), ALPN, and a KeyUpdate from s_client answered
TEST(TlsInterop, OpenSslClientRetryAlpnKeyUpdate) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l.has_value());
    tls::config scfg = server_config("ecdsa");
    scfg.groups = {tls::group::secp256r1};
    scfg.alpn = {sgcl::string("http/1.1"), sgcl::string("h2")};
    Served served;
    std::thread server([&] { serve_one(*l, scfg, served); });
    Child s(openssl_path(), s_client_args(address_of(*l), {"-groups", "X25519:P-256", "-alpn", "h2,http/1.1"}));
    auto in = s.cmd.stdin_pipe();
    ASSERT_TRUE(in.has_value());
    ASSERT_TRUE(s.start());
    ASSERT_FALSE(s.wait_for("Verification: OK").empty()) << slurp(s.log);
    ASSERT_TRUE(in->write(sgcl::string("K\n")).has_value());   // KeyUpdate, update_requested
    ASSERT_FALSE(s.wait_for("KeyUpdate").empty());
    ASSERT_TRUE(in->write(sgcl::string("after the update\n")).has_value());
    std::string out = s.wait_for("etadpu eht retfa");
    (void)in->close();
    server.join();
    (void)l->close();
    ASSERT_FALSE(out.empty()) << slurp(s.log);
    EXPECT_NE(out.find("ALPN protocol: http/1.1"), std::string::npos) << out;
    // s_client sent a KeyUpdate and got one back
    EXPECT_NE(out.find(">>> TLS 1.3, Handshake [length 0005], KeyUpdate"), std::string::npos) << out;
    EXPECT_NE(out.find("<<< TLS 1.3, Handshake [length 0005], KeyUpdate"), std::string::npos) << out;
    EXPECT_TRUE(served.error.empty()) << served.error;
    ASSERT_TRUE(served.state.has_value());
    EXPECT_EQ(served.state->group, tls::group::secp256r1);
    EXPECT_EQ(served.state->alpn, "http/1.1");
    EXPECT_EQ(served.line, "after the update");
    EXPECT_TRUE(served.eof);
}

TEST(TlsInterop, GoClientMatrix) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    size_t n = 0;
    for (auto c : ciphers) {
        for (auto g : groups) {
            for (const char* leaf : leaves) {
                SCOPED_TRACE(std::string(cipher_name(c)) + " " + group_name(g) + " " + leaf);
                auto l = net::tcp::listen("127.0.0.1:0");
                ASSERT_TRUE(l.has_value());
                tls::config scfg = server_config(leaf);
                scfg.ciphers = {c};   // Go's TLS 1.3 suites are not configurable: the server's choice
                scfg.groups = {g};    // Go's shares are X25519MLKEM768 and X25519: P-256, P-384 by HelloRetryRequest
                Served served;
                std::thread server([&] { serve_one(*l, scfg, served); });
                Child go(go_oracle(), {sgcl::string("-connect"), sgcl::string(address_of(*l)), sgcl::string("-ca"), sgcl::string(testdata() + "ca.pem")});
                ASSERT_TRUE(go.start());
                std::string out = go.wait_for("CLOSED");
                server.join();
                (void)l->close();
                EXPECT_FALSE(out.empty()) << slurp(go.log) << " / server: " << served.error;
                std::string expected = "STATE " + std::to_string(int(c)) + " " + std::to_string(int(g)) + " ";
                EXPECT_NE(out.find(expected), std::string::npos) << out;
                EXPECT_NE(out.find("GOT og olleh"), std::string::npos) << out;
                EXPECT_TRUE(served.error.empty()) << served.error;
                EXPECT_EQ(served.line, "hello go");
                EXPECT_TRUE(served.eof);
                ++n;
            }
        }
    }
    EXPECT_EQ(n, 36u);
}

TEST(TlsInterop, GoClientAlpn) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l.has_value());
    tls::config scfg = server_config("ed25519");
    scfg.alpn = {sgcl::string("http/1.1"), sgcl::string("h2")};
    Served served;
    std::thread server([&] { serve_one(*l, scfg, served); });
    Child go(go_oracle(), {sgcl::string("-connect"), sgcl::string(address_of(*l)), sgcl::string("-ca"), sgcl::string(testdata() + "ca.pem"), sgcl::string("-alpn"), sgcl::string("h2,http/1.1")});
    ASSERT_TRUE(go.start());
    std::string out = go.wait_for("CLOSED");
    server.join();
    (void)l->close();
    EXPECT_NE(out.find(" http/1.1"), std::string::npos) << out;   // the server's first the client offers
    ASSERT_TRUE(served.state.has_value());
    EXPECT_EQ(served.state->alpn, "http/1.1");
}

// tls::listen: accept gives connections whose handshake is done; a client
// that never speaks holds nobody up and is dropped after the timeout; a
// client that fails its handshake is dropped too; close ends the accepts
TEST(TlsInterop, Listener) {
    tls::config scfg = server_config("ecdsa");
    scfg.handshake_timeout = 300 * sgcl::millisecond;
    auto l = tls::listen("127.0.0.1:0", scfg);
    ASSERT_TRUE(l.has_value()) << std::string(l.error().message().view());
    const std::string address = address_of(*l);
    // one that connects and says nothing, one whose client refuses the
    // server (no roots), then two that complete
    auto silent = net::tcp::connect(sgcl::string(address));
    ASSERT_TRUE(silent.has_value());
    std::thread refused([&] {
        tls::config bad = trusted();
        bad.roots = crypto::x509::certificate_pool();
        auto r = tls::connect(sgcl::string(address), bad);
        EXPECT_FALSE(r.has_value());
    });
    refused.join();
    std::thread clients([&] {
        for (int i = 0; i < 2; ++i) {
            auto c = tls::connect(sgcl::string(address), trusted());
            ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
            echo(*c, "through the listener " + std::to_string(i));
            (void)c->close();
        }
    });
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 2; ++i) {
        auto c = l->accept();
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        auto st = tls::state_of(*c);
        ASSERT_TRUE(st.has_value());   // a TLS connection, its handshake done
        auto line = c->read_line();
        ASSERT_TRUE(line.has_value() && line->has_value());
        std::string text(line->value().view());
        ASSERT_TRUE(c->write(sgcl::string(std::string(text.rbegin(), text.rend()) + "\n")).has_value());
    }
    clients.join();
    // the silent one did not hold the others up
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));
    // the silent one: dropped by the server after the timeout (its end reads 0)
    byte b[1];
    silent->set_read_deadline(sgcl::clock::now() + 5 * sgcl::second);
    auto r = silent->read(b);
    EXPECT_TRUE(r.has_value() && *r == 0);
    (void)silent->close();
    // the accepts end with the close
    std::thread closer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        (void)l->close();
    });
    auto after = l->accept();
    closer.join();
    ASSERT_FALSE(after.has_value());
    EXPECT_TRUE(after.error().is_closed());
    EXPECT_TRUE(l->is_closed());
}

// A task's listener: async_listen, async_accept, s_client as the peer
TEST(TlsInterop, ListenerInATaskWithOpenSsl) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    auto l = tls::async_listen(sgcl::string("127.0.0.1:0"), server_config("rsa")).wait();
    ASSERT_TRUE(l.has_value());
    Child s(openssl_path(), s_client_args(address_of(*l), {}));
    auto in = s.cmd.stdin_pipe();
    ASSERT_TRUE(in.has_value());
    ASSERT_TRUE(s.start());
    auto c = l->async_accept().wait();
    ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
    ASSERT_TRUE(in->write(sgcl::string("a task's\n")).has_value());
    auto line = c->async_read_line().wait();
    ASSERT_TRUE(line.has_value() && line->has_value());
    EXPECT_EQ(std::string(line->value().view()), "a task's");
    ASSERT_TRUE(c->async_write(sgcl::string("s'ksat a\n")).wait().has_value());
    EXPECT_FALSE(s.wait_for("s'ksat a").empty()) << slurp(s.log);
    (void)in->close();
    auto end = c->async_read_line().wait();
    EXPECT_TRUE(end.has_value() && !end->has_value());   // s_client's close_notify
    (void)c->close();
    (void)l->close();
}

namespace {
    sgcl::async::task<expected<size_t, io::error>> copy_file_in_task(net::connection c, io::file from) {
        co_return co_await io::async_copy(c, from);
    }
}

// io::copy of a file to a TLS connection (the connection's read_from):
// blocks read from the file, sealed into records where they lie, many
// times the socket's buffers so the writes wait; the other side reads the
// file's bytes from the position on, and the position moves to the end.
// With a send buffer of 4 KB and a reader that pauses, the socket takes a
// batch of records in part again and again: the rest of each write goes
// on from the record it cut, and no record goes twice
TEST(TlsInterop, CopyOfAFileOverTls) {
    const std::string path = (std::filesystem::temp_directory_path() / ("sgcl_tls_file_" + std::to_string(::getpid()))).string();
    std::string content(20 << 20, '\0');
    uint32_t x = 12345;
    for (auto& ch : content) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        ch = char(x);
    }
    {
        std::ofstream out(path, std::ios::binary);
        out.write(content.data(), std::streamsize(content.size()));
    }
    for (int mode = 0; mode < 4; ++mode) {
        const bool in_task = mode & 1;
        const bool small = mode & 2;
        SCOPED_TRACE(std::string(in_task ? "async_copy" : "copy") + (small ? ", a send buffer of 4 KB" : ""));
        auto l = net::tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l.has_value());
        tls::config scfg = server_config("ecdsa");
        std::string error;
        size_t sent = 0;
        uint64_t position = 0;
        std::thread server([&] {
            auto t = l->accept();
            if (!t) {
                error = "accept";
                return;
            }
            auto c = tls::server(*t, scfg);
            if (!c) {
                error = std::string(c.error().message().view());
                return;
            }
            if (small) {
                const int fd = static_cast<net::detail::SocketConn&>(net::detail::ConnectionAccess::impl(*t)).fd();
                const int bytes = 4096;
                ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes));
            }
            auto f = io::open(sgcl::string(path));
            if (!f || !f->seek(777)) {
                error = "open";
                return;
            }
            auto r = in_task ? sgcl::async::spawn(copy_file_in_task(*c, *f)).wait() : io::copy(*c, *f);
            if (!r) {
                error = std::string(r.error().message().view());
            } else {
                sent = *r;
            }
            position = f->seek(0, io::seek_from::current).value_or(0);
            (void)c->close();
        });
        auto conn = tls::connect(sgcl::string(address_of(*l)), trusted());
        std::string got;
        if (conn) {
            // read in pieces of 4 KB, a pause of 1 ms after each 256 KB
            std::array<std::byte, 4096> piece;
            size_t since = 0;
            for (;;) {
                auto n = conn->read(slice<byte>(piece.data(), piece.size()));
                if (!n || *n == 0) {
                    break;
                }
                got.append(reinterpret_cast<const char*>(piece.data()), *n);
                since += *n;
                if (small && since >= (256 << 10)) {
                    since = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            (void)conn->close();
        }
        server.join();
        (void)l->close();
        ASSERT_TRUE(conn.has_value());
        EXPECT_TRUE(error.empty()) << error;
        EXPECT_EQ(sent, content.size() - 777);
        EXPECT_EQ(position, content.size());
        ASSERT_EQ(got.size(), content.size() - 777);
        EXPECT_TRUE(got == content.substr(777));
    }
    std::filesystem::remove(path);
}

// --- resumption and client certificates ------------------------------------------

namespace {
    // A Go server of these arguments, its address once it listens
    std::string go_address(Child& go) {
        std::string s = go.wait_for("LISTEN ");
        auto at = s.find("LISTEN ");
        if (at == std::string::npos) {
            return "";
        }
        return "127.0.0.1:" + s.substr(at + 7, s.find('\n', at) - at - 7);
    }

    sgcl::vector<sgcl::string> args(std::initializer_list<std::string> a) {
        sgcl::vector<sgcl::string> v;
        for (auto& x : a) {
            v.push_back(sgcl::string(x));
        }
        return v;
    }

    size_t count(const std::string& text, const std::string& what) {
        size_t n = 0;
        for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) {
            ++n;
        }
        return n;
    }
}

TEST(TlsInterop, GoServerResumesOurClient) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    Child go(go_oracle(), args({"-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key", "-n", "2"}));
    ASSERT_TRUE(go.start());
    const std::string address = go_address(go);
    ASSERT_FALSE(address.empty()) << slurp(go.log);
    tls::config cfg = trusted();
    cfg.session_cache = tls::session_cache();
    for (int i = 0; i < 2; ++i) {
        auto c = tls::connect(sgcl::string(address), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        EXPECT_EQ(tls::state_of(*c)->resumed, i == 1);
        echo(*c, "hello go");   // the ticket comes before the answer
        EXPECT_TRUE(c->async_close().wait().has_value());
        if (i == 0) {
            EXPECT_EQ(cfg.session_cache->size(), 1u);
            ASSERT_FALSE(go.wait_for("CLOSE_NOTIFY").empty()) << slurp(go.log);
        }
    }
    std::string out = go.wait_for("resumed=true");
    EXPECT_NE(out.find("resumed=false"), std::string::npos) << out;
    EXPECT_NE(out.find("resumed=true"), std::string::npos) << out;
}

TEST(TlsInterop, GoClientResumesOurServer) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l.has_value());
    tls::config scfg = server_config("ecdsa");
    Served first, second;
    std::thread server([&] {
        serve_one(*l, scfg, first);
        serve_one(*l, scfg, second);
    });
    Child go(go_oracle(), args({"-connect", address_of(*l), "-ca", testdata() + "ca.pem", "-n", "2"}));
    ASSERT_TRUE(go.start());
    std::string out = go.wait_for("CLOSED");
    server.join();
    (void)l->close();
    EXPECT_EQ(count(out, "GOT og olleh"), 2u) << out;
    EXPECT_NE(out.find("resumed=false"), std::string::npos) << out;
    EXPECT_NE(out.find("resumed=true"), std::string::npos) << out;
    ASSERT_TRUE(first.state && second.state) << first.error << " / " << second.error;
    EXPECT_FALSE(first.state->resumed);
    EXPECT_TRUE(second.state->resumed);
}

TEST(TlsInterop, GoServerAsksOurClientForACertificate) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    for (const char* kind : {"client_ecdsa", "client_ed25519", "client_rsa", ""}) {
        SCOPED_TRACE(kind);
        Child go(go_oracle(), args({"-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key", "-clientauth", "require", "-clientca", testdata() + "ca.pem"}));
        ASSERT_TRUE(go.start());
        const std::string address = go_address(go);
        ASSERT_FALSE(address.empty()) << slurp(go.log);
        tls::config cfg = trusted();
        if (*kind) {
            cfg.identities = {identity_of(kind)};
        }
        auto c = tls::connect(sgcl::string(address), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        if (*kind) {
            echo(*c, "hello go");
            std::string out = go.wait_for("STATE ");
            EXPECT_NE(out.find(std::string("peer=sgcl_test_client_") + (kind + 7)), std::string::npos) << out;
        } else {
            // the handshake is ours before Go reads the empty Certificate: the first read has the alert
            (void)c->write(sgcl::string("hello go\n"));
            auto r = c->read_line();
            ASSERT_FALSE(r.has_value());
            EXPECT_EQ(std::string(r.error().message().view()).find("remote error: tls: certificate required") != std::string::npos, true)
                << std::string(r.error().message().view());
            EXPECT_FALSE(go.wait_for("ERROR").empty()) << slurp(go.log);
        }
        (void)c->close();
    }
}

TEST(TlsInterop, GoClientCertificateToOurServer) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l.has_value());
    tls::config scfg = server_config("ecdsa");
    scfg.client_auth = tls::client_auth::require;
    scfg.client_roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata() + "ca.pem")));
    Served first, second;
    std::thread server([&] {
        serve_one(*l, scfg, first);
        serve_one(*l, scfg, second);
    });
    Child go(go_oracle(), args({"-connect", address_of(*l), "-ca", testdata() + "ca.pem", "-cert", testdata() + "client_rsa.pem", "-key", testdata() + "client_rsa.key", "-n", "2"}));
    ASSERT_TRUE(go.start());
    std::string out = go.wait_for("CLOSED");
    server.join();
    (void)l->close();
    EXPECT_EQ(count(out, "GOT og olleh"), 2u) << out;
    ASSERT_TRUE(first.state && second.state) << first.error << " / " << second.error;
    EXPECT_EQ(first.state->peer, "sgcl test client rsa");
    EXPECT_FALSE(first.state->resumed);
    // the session resumed keeps the client's chain
    EXPECT_TRUE(second.state->resumed);
    EXPECT_EQ(second.state->peer, "sgcl test client rsa");
}

TEST(TlsInterop, OpenSslServerResumesOurClient) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    const uint16_t port = free_port();
    Child s(openssl_path(), args({"s_server", "-accept", "127.0.0.1:" + std::to_string(port), "-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key",
                                  "-tls1_3", "-naccept", "2", "-rev"}));
    ASSERT_TRUE(s.start());
    ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
    tls::config cfg = trusted();
    cfg.session_cache = tls::session_cache();
    for (int i = 0; i < 2; ++i) {
        auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(port)), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        EXPECT_EQ(tls::state_of(*c)->resumed, i == 1);
        echo(*c, "hello openssl");
        if (i == 0) {
            EXPECT_EQ(cfg.session_cache->size(), 2u);   // s_server sends two tickets
        }
        EXPECT_TRUE(c->async_close().wait().has_value());
    }
}

TEST(TlsInterop, OpenSslClientResumesOurServer) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l.has_value());
    tls::config scfg = server_config("ecdsa");
    const std::string session = scratch() + "/session.pem";
    for (int i = 0; i < 2; ++i) {
        SCOPED_TRACE(i);
        Served served;
        std::thread server([&] { serve_one(*l, scfg, served); });
        Child s(openssl_path(), s_client_args(address_of(*l), {i == 0 ? "-sess_out" : "-sess_in", session}));
        auto in = s.cmd.stdin_pipe();
        ASSERT_TRUE(in.has_value());
        ASSERT_TRUE(s.start());
        ASSERT_TRUE(in->write(sgcl::string("hello from openssl\n")).has_value());
        std::string out = s.wait_for("lssnepo morf olleh");
        (void)in->close();
        server.join();
        s.finish();
        out = slurp(s.log);
        ASSERT_TRUE(served.state.has_value()) << served.error << out;
        EXPECT_EQ(served.state->resumed, i == 1);
        EXPECT_NE(out.find(i == 0 ? "New, TLSv1.3" : "Reused, TLSv1.3"), std::string::npos) << out;
    }
    (void)l->close();
}

TEST(TlsInterop, OpenSslServerVerifiesOurClientCertificate) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    for (const char* kind : {"client_ecdsa", ""}) {
        SCOPED_TRACE(kind);
        const uint16_t port = free_port();
        Child s(openssl_path(), args({"s_server", "-accept", "127.0.0.1:" + std::to_string(port), "-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key",
                                      "-tls1_3", "-naccept", "1", "-rev", "-Verify", "1", "-CAfile", testdata() + "ca.pem", "-verify_return_error"}));
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
        tls::config cfg = trusted();
        if (*kind) {
            cfg.identities = {identity_of(kind)};
        }
        auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(port)), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        if (*kind) {
            echo(*c, "hello openssl");
            std::string out = slurp(s.log);
            EXPECT_NE(out.find("CN=sgcl test client ecdsa"), std::string::npos) << out;
        } else {
            (void)c->write(sgcl::string("hello\n"));
            auto r = c->read_line();
            ASSERT_FALSE(r.has_value());
            EXPECT_NE(std::string(r.error().message().view()).find("remote error: tls: certificate required"), std::string::npos) << std::string(r.error().message().view());
        }
        (void)c->close();
    }
}

TEST(TlsInterop, OpenSslClientCertificateToOurServer) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    for (const char* kind : {"client_ed25519", "client_rsa"}) {
        SCOPED_TRACE(kind);
        auto l = net::tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l.has_value());
        tls::config scfg = server_config("ecdsa");
        scfg.client_auth = tls::client_auth::require;
        scfg.client_roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata() + "ca.pem")));
        Served served;
        std::thread server([&] { serve_one(*l, scfg, served); });
        Child s(openssl_path(), s_client_args(address_of(*l), {"-cert", testdata() + kind + ".pem", "-key", testdata() + kind + ".key"}));
        auto in = s.cmd.stdin_pipe();
        ASSERT_TRUE(in.has_value());
        ASSERT_TRUE(s.start());
        ASSERT_TRUE(in->write(sgcl::string("hello from openssl\n")).has_value());
        std::string out = s.wait_for("lssnepo morf olleh");
        (void)in->close();
        server.join();
        (void)l->close();
        EXPECT_FALSE(out.empty()) << slurp(s.log);
        ASSERT_TRUE(served.state.has_value()) << served.error;
        EXPECT_EQ(served.state->peer, std::string("sgcl test client ") + (kind + 7));
    }
}

// --- TLS 1.2: the client against servers without 1.3 ---------------------------------
//
// Go's crypto/tls with MaxVersion 1.2 (tools/tls_oracle.go -tls12) and
// OpenSSL's s_server -tls1_2: every suite of each kind of leaf × every
// curve, the state's version and suite, lines both ways and close_notify;
// the signature schemes of 1.2 (PKCS #1 v1.5, ECDSA with SHA-384 on P-256);
// ALPN h2 over 1.2 (RFC 7540 §9.2: ECDHE and an AEAD, which every suite
// here is); client certificates; a client that requires 1.3 refused; and
// our server, 1.3 alone, refusing a 1.2 client.

namespace {
    // The 1.2 suites of a leaf: ECDHE_ECDSA for ECDSA and Ed25519, ECDHE_RSA for RSA
    std::vector<tls::cipher> suites12_of(const std::string& leaf) {
        if (leaf == "rsa") {
            return {tls::cipher::ecdhe_rsa_aes_128_gcm_sha256, tls::cipher::ecdhe_rsa_aes_256_gcm_sha384, tls::cipher::ecdhe_rsa_chacha20_poly1305_sha256};
        }
        return {tls::cipher::ecdhe_ecdsa_aes_128_gcm_sha256, tls::cipher::ecdhe_ecdsa_aes_256_gcm_sha384, tls::cipher::ecdhe_ecdsa_chacha20_poly1305_sha256};
    }

    const char* openssl_name12(tls::cipher c) {
        switch (c) {
            case tls::cipher::ecdhe_ecdsa_aes_128_gcm_sha256: return "ECDHE-ECDSA-AES128-GCM-SHA256";
            case tls::cipher::ecdhe_ecdsa_aes_256_gcm_sha384: return "ECDHE-ECDSA-AES256-GCM-SHA384";
            case tls::cipher::ecdhe_ecdsa_chacha20_poly1305_sha256: return "ECDHE-ECDSA-CHACHA20-POLY1305";
            case tls::cipher::ecdhe_rsa_aes_128_gcm_sha256: return "ECDHE-RSA-AES128-GCM-SHA256";
            case tls::cipher::ecdhe_rsa_aes_256_gcm_sha384: return "ECDHE-RSA-AES256-GCM-SHA384";
            case tls::cipher::ecdhe_rsa_chacha20_poly1305_sha256: return "ECDHE-RSA-CHACHA20-POLY1305";
            default: return "";
        }
    }

    constexpr tls::group groups12[] = {tls::group::x25519, tls::group::secp256r1, tls::group::secp384r1};

    // An s_server of TLS 1.2 alone for one connection, the extra arguments given
    struct SServer12 : Child {
        uint16_t port;

        SServer12(const std::string& leaf, std::vector<std::string> extra, uint16_t p = free_port())
        : Child(openssl_path(), [&] {
            sgcl::vector<sgcl::string> a = {sgcl::string("s_server"), sgcl::string("-accept"), sgcl::string("127.0.0.1:" + std::to_string(p)),
                                            sgcl::string("-cert"), sgcl::string(testdata() + leaf + ".pem"), sgcl::string("-key"), sgcl::string(testdata() + leaf + ".key"),
                                            sgcl::string("-tls1_2"), sgcl::string("-naccept"), sgcl::string("1"), sgcl::string("-rev")};
            for (auto& e : extra) {
                a.push_back(sgcl::string(e));
            }
            return a;
        }())
        , port(p) {
        }

        std::string address() const {
            return "127.0.0.1:" + std::to_string(port);
        }
    };
}

TEST(TlsInterop, Tls12GoMatrix) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    size_t n = 0;
    for (const char* leaf : leaves) {
        for (auto c : suites12_of(leaf)) {
            for (auto g : groups12) {
                SCOPED_TRACE(std::string(leaf) + " " + std::to_string(int(c)) + " " + group_name(g));
                Child go(go_oracle(), {sgcl::string("-cert"), sgcl::string(testdata() + leaf + ".pem"), sgcl::string("-key"), sgcl::string(testdata() + leaf + ".key"),
                                       sgcl::string("-tls12"), sgcl::string("-ciphers"), sgcl::string(std::to_string(int(c))), sgcl::string("-curves"), sgcl::string(std::to_string(int(g)))});
                ASSERT_TRUE(go.start());
                std::string address = go_address(go);
                ASSERT_FALSE(address.empty()) << slurp(go.log);
                auto conn = tls::connect(sgcl::string(address), trusted());
                ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
                auto st = tls::state_of(*conn);
                EXPECT_EQ(st->version, tls::version::tls12);
                EXPECT_EQ(st->cipher, c);
                EXPECT_EQ(st->group, g);
                echo(*conn, "hello go 1.2");
                conn->set_max_line(1 << 20);
                echo(*conn, std::string(200000, 'x') + "y");   // records of 2^14 bytes both ways
                EXPECT_TRUE(conn->async_close().wait().has_value());
                std::string out = go.wait_for("CLOSE_NOTIFY");
                EXPECT_NE(out.find("STATE " + std::to_string(int(c)) + " " + std::to_string(int(g)) + " "), std::string::npos) << out;
                EXPECT_NE(out.find("version=303"), std::string::npos) << out;
                ++n;
            }
        }
    }
    EXPECT_EQ(n, 27u);
}

TEST(TlsInterop, Tls12OpenSslMatrix) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    size_t n = 0;
    for (const char* leaf : leaves) {
        for (auto c : suites12_of(leaf)) {
            for (auto g : groups12) {
                SCOPED_TRACE(std::string(leaf) + " " + openssl_name12(c) + " " + group_name(g));
                SServer12 s(leaf, {"-cipher", openssl_name12(c), "-groups", group_name(g)});
                ASSERT_TRUE(s.start());
                ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
                auto conn = tls::connect(sgcl::string(s.address()), trusted());
                ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
                auto st = tls::state_of(*conn);
                EXPECT_EQ(st->version, tls::version::tls12);
                EXPECT_EQ(st->cipher, c);
                EXPECT_EQ(st->group, g);
                echo(*conn, "hello openssl 1.2");
                echo(*conn, std::string(16000, 'x') + "y");   // s_server -rev answers 16 KB a line at most
                (void)conn->close();
                ++n;
            }
        }
    }
    EXPECT_EQ(n, 27u);
}

// The schemes of 1.2 OpenSSL signs its ServerKeyExchange with when told:
// PKCS #1 v1.5, PSS, and ECDSA with SHA-384 on a P-256 key
TEST(TlsInterop, Tls12SignatureSchemes) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    struct Case {
        const char *leaf, *sigalgs;
    };
    for (Case c : {Case{"rsa", "RSA+SHA256"}, Case{"rsa", "RSA+SHA512"}, Case{"rsa", "rsa_pss_rsae_sha384"}, Case{"ecdsa", "ECDSA+SHA384"}, Case{"ecdsa", "ECDSA+SHA256"}, Case{"ed25519", "ed25519"}}) {
        SCOPED_TRACE(std::string(c.leaf) + " " + c.sigalgs);
        SServer12 s(c.leaf, {"-sigalgs", c.sigalgs});
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
        auto conn = tls::connect(sgcl::string(s.address()), trusted());
        ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
        EXPECT_EQ(tls::state_of(*conn)->version, tls::version::tls12);
        echo(*conn, "schemes");
        (void)conn->close();
    }
}

TEST(TlsInterop, Tls12AlpnH2) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    Child go(go_oracle(), {sgcl::string("-cert"), sgcl::string(testdata() + "ecdsa.pem"), sgcl::string("-key"), sgcl::string(testdata() + "ecdsa.key"),
                           sgcl::string("-tls12"), sgcl::string("-alpn"), sgcl::string("h2,http/1.1")});
    ASSERT_TRUE(go.start());
    std::string address = go_address(go);
    ASSERT_FALSE(address.empty()) << slurp(go.log);
    tls::config cfg = trusted();
    cfg.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
    auto conn = tls::connect(sgcl::string(address), cfg);
    ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
    EXPECT_EQ(tls::state_of(*conn)->alpn, "h2");
    EXPECT_EQ(tls::state_of(*conn)->version, tls::version::tls12);
    echo(*conn, "h2 over 1.2");
    (void)conn->async_close().wait();
}

TEST(TlsInterop, Tls12ClientCertificates) {
    if (go_path().empty() || openssl_path().empty()) {
        GTEST_SKIP() << "no go or no OpenSSL";
    }
    ASSERT_FALSE(go_oracle().empty());
    for (const char* kind : {"client_ecdsa", "client_ed25519", "client_rsa"}) {
        SCOPED_TRACE(kind);
        Child go(go_oracle(), args({"-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key", "-tls12", "-clientauth", "require", "-clientca", testdata() + "ca.pem"}));
        ASSERT_TRUE(go.start());
        const std::string address = go_address(go);
        ASSERT_FALSE(address.empty()) << slurp(go.log);
        tls::config cfg = trusted();
        cfg.identities = {identity_of(kind)};
        auto c = tls::connect(sgcl::string(address), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        echo(*c, "hello go");
        std::string out = go.wait_for("STATE ");
        EXPECT_NE(out.find(std::string("peer=sgcl_test_client_") + (kind + 7)), std::string::npos) << out;
        EXPECT_NE(out.find("version=303"), std::string::npos) << out;
        (void)c->close();
    }
    // OpenSSL asking for PKCS #1 v1.5 alone from an RSA client, and ECDSA
    for (const char* kind : {"client_rsa", "client_ecdsa"}) {
        SCOPED_TRACE(kind);
        SServer12 s("ecdsa", {"-Verify", "1", "-CAfile", testdata() + "ca.pem", "-verify_return_error", "-client_sigalgs", "RSA+SHA256:ECDSA+SHA256"});
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
        tls::config cfg = trusted();
        cfg.identities = {identity_of(kind)};
        auto c = tls::connect(sgcl::string(s.address()), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        echo(*c, "hello openssl");
        std::string out = slurp(s.log);
        EXPECT_NE(out.find(std::string("CN=sgcl test client ") + (kind + 7)), std::string::npos) << out;
        (void)c->close();
    }
    // none sent where one is required: the server's alert at the end of the handshake
    Child go(go_oracle(), args({"-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key", "-tls12", "-clientauth", "require", "-clientca", testdata() + "ca.pem"}));
    ASSERT_TRUE(go.start());
    const std::string address = go_address(go);
    auto c = tls::connect(sgcl::string(address), trusted());
    EXPECT_FALSE(c.has_value());   // in 1.2 the server's Finished never comes
    if (!c) {
        EXPECT_TRUE(tls::is_remote(c.error())) << std::string(c.error().message().view());
    }
}

// min_version 1.3 against a server of 1.2: the server's alert before its
// hello; max_version 1.2 against a server of both: 1.2
TEST(TlsInterop, Tls12VersionSettings) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    {
        Child go(go_oracle(), args({"-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key", "-tls12"}));
        ASSERT_TRUE(go.start());
        const std::string address = go_address(go);
        tls::config cfg = trusted();
        cfg.min_version = tls::version::tls13;
        auto c = tls::connect(sgcl::string(address), cfg);
        ASSERT_FALSE(c.has_value());
        EXPECT_NE(std::string(c.error().message().view()).find("protocol version"), std::string::npos) << std::string(c.error().message().view());
    }
    {
        // the oracle's server without -tls12 takes 1.3 alone (MinVersion 1.3)
        Child go(go_oracle(), args({"-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key"}));
        ASSERT_TRUE(go.start());
        const std::string address = go_address(go);
        tls::config cfg = trusted();
        cfg.max_version = tls::version::tls12;
        auto c = tls::connect(sgcl::string(address), cfg);
        ASSERT_FALSE(c.has_value());
        EXPECT_NE(std::string(c.error().message().view()).find("protocol version"), std::string::npos) << std::string(c.error().message().view());
    }
    // OpenSSL of both versions, the client limited to 1.2: 1.2 (its sentinel, sent, is not checked by a client of 1.2 alone)
    if (!openssl_path().empty()) {
        const uint16_t port = free_port();
        Child s(openssl_path(), args({"s_server", "-accept", "127.0.0.1:" + std::to_string(port), "-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key", "-naccept", "1", "-rev"}));
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty());
        tls::config cfg = trusted();
        cfg.max_version = tls::version::tls12;
        auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(port)), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        EXPECT_EQ(tls::state_of(*c)->version, tls::version::tls12);
        echo(*c, "both");
        (void)c->close();
    }
}

// Our server speaks 1.3 alone: Go's client of 1.2 is refused with
// protocol_version. (The downgrade sentinel, which a real server sends only
// when something between strips 1.3 from the hello, is tested at the level
// of messages, tls12_client.cpp.)
TEST(TlsInterop, Tls12ClientToOurServer) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l.has_value());
    tls::config scfg = server_config("ecdsa");
    Served served;
    std::thread server([&] { serve_one(*l, scfg, served); });
    Child go(go_oracle(), args({"-connect", address_of(*l), "-ca", testdata() + "ca.pem", "-tls12"}));
    ASSERT_TRUE(go.start());
    std::string out = go.wait_for("ERROR");
    server.join();
    (void)l->close();
    EXPECT_NE(out.find("protocol version"), std::string::npos) << out;
    EXPECT_NE(served.error.find("protocol version"), std::string::npos) << served.error;
}

// --- TLS 1.2 resumption: the client's session_cache against servers of 1.2 -----------
//
// Go's crypto/tls of 1.2 alone resumes by ticket (it has no session-id
// cache); OpenSSL's s_server -tls1_2 by ticket and, with -no_ticket, by
// session id. Each: the first connection a full handshake, the next ones
// resumed (the session kept again after each), the state's version,
// resumed and the peer's chain (the session's), lines both ways. A server
// that does not know the session (a new s_server on the same port): a full
// handshake, the session replaced.

namespace {
    // The certificates of a state as DER, for comparing two connections'
    std::vector<std::string> ders(const crypto::x509::chain& chain) {
        std::vector<std::string> out;
        for (auto& c : chain) {
            auto d = c.raw();
            out.emplace_back(reinterpret_cast<const char*>(d.data()), d.size());
        }
        return out;
    }

    // An s_server of TLS 1.2 alone at a port, for `n` connections one after another
    struct SServer12At : Child {
        SServer12At(uint16_t port, int n, const std::vector<std::string>& extra)
        : Child(openssl_path(), [&] {
            sgcl::vector<sgcl::string> a = args({"s_server", "-accept", "127.0.0.1:" + std::to_string(port), "-cert", testdata() + "ecdsa.pem", "-key",
                                                 testdata() + "ecdsa.key", "-tls1_2", "-naccept", std::to_string(n), "-rev"});
            for (auto& e : extra) {
                a.push_back(sgcl::string(e));
            }
            return a;
        }()) {
        }
    };
}

TEST(TlsInterop, Tls12GoServerResumesByTicket) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty());
    Child go(go_oracle(), args({"-cert", testdata() + "ecdsa.pem", "-key", testdata() + "ecdsa.key", "-tls12", "-n", "3"}));
    ASSERT_TRUE(go.start());
    const std::string address = go_address(go);
    ASSERT_FALSE(address.empty()) << slurp(go.log);
    tls::config cfg = trusted();
    cfg.session_cache = tls::session_cache();
    cfg.groups = {tls::group::secp256r1};   // the resumed state's group is the session's
    std::vector<std::string> first;
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        auto c = tls::connect(sgcl::string(address), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        auto st = tls::state_of(*c);
        EXPECT_EQ(st->version, tls::version::tls12);
        EXPECT_EQ(st->resumed, i > 0);
        EXPECT_EQ(st->group, tls::group::secp256r1);
        if (i == 0) {
            first = ders(st->peer_certificates);
            ASSERT_FALSE(first.empty());
        } else {
            EXPECT_EQ(ders(st->peer_certificates), first);   // the session's
        }
        EXPECT_EQ(cfg.session_cache->size(), 1u);   // the ticket in the handshake, kept again after a resumption
        echo(*c, "hello go 1.2");
        EXPECT_TRUE(c->async_close().wait().has_value());
    }
    std::string out = go.wait_for("resumed=true", 10000);
    for (int i = 0; i < 100 && count(out, "CLOSE_NOTIFY") < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        out = slurp(go.log);
    }
    EXPECT_EQ(count(out, "resumed=false"), 1u) << out;
    EXPECT_EQ(count(out, "resumed=true"), 2u) << out;
    EXPECT_EQ(count(out, "version=303"), 3u) << out;
}

TEST(TlsInterop, Tls12OpenSslServerResumes) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    for (bool tickets : {true, false}) {
        SCOPED_TRACE(tickets ? "by ticket" : "by session id (-no_ticket)");
        const uint16_t port = free_port();
        SServer12At s(port, 3, tickets ? std::vector<std::string>{} : std::vector<std::string>{"-no_ticket"});
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
        tls::config cfg = trusted();
        cfg.session_cache = tls::session_cache();
        std::vector<std::string> first;
        for (int i = 0; i < 3; ++i) {
            SCOPED_TRACE(i);
            auto c = tls::connect(sgcl::string("127.0.0.1:" + std::to_string(port)), cfg);
            ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
            auto st = tls::state_of(*c);
            EXPECT_EQ(st->version, tls::version::tls12);
            EXPECT_EQ(st->resumed, i > 0);
            if (i == 0) {
                first = ders(st->peer_certificates);
            } else {
                EXPECT_EQ(ders(st->peer_certificates), first);
            }
            EXPECT_EQ(cfg.session_cache->size(), 1u);
            echo(*c, "hello openssl 1.2");
            (void)c->close();
        }
        // s_server prints the client's signature algorithms for a full handshake alone
        std::string log = slurp(s.log);
        EXPECT_EQ(count(log, "CONNECTION ESTABLISHED"), 3u) << log;
        EXPECT_EQ(count(log, "Signature Algorithms:"), 1u) << log;
    }
}

TEST(TlsInterop, Tls12ResumptionDeclined) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    for (bool tickets : {true, false}) {
        SCOPED_TRACE(tickets ? "by ticket" : "by session id (-no_ticket)");
        const uint16_t port = free_port();
        const std::string address = "127.0.0.1:" + std::to_string(port);
        const std::vector<std::string> extra = tickets ? std::vector<std::string>{} : std::vector<std::string>{"-no_ticket"};
        tls::config cfg = trusted();
        cfg.session_cache = tls::session_cache();
        {
            SServer12At s(port, 1, extra);
            ASSERT_TRUE(s.start());
            ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
            auto c = tls::connect(sgcl::string(address), cfg);
            ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
            EXPECT_FALSE(tls::state_of(*c)->resumed);
            echo(*c, "first");
            (void)c->close();
            s.finish();
        }
        EXPECT_EQ(cfg.session_cache->size(), 1u);
        // another server at the same port: other ticket keys, no session ids of the first's
        SServer12At s(port, 1, extra);
        ASSERT_TRUE(s.start());
        ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
        auto c = tls::connect(sgcl::string(address), cfg);
        ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
        auto st = tls::state_of(*c);
        EXPECT_FALSE(st->resumed);
        EXPECT_FALSE(st->peer_certificates.empty());
        echo(*c, "after the server forgot");
        (void)c->close();
        EXPECT_EQ(cfg.session_cache->size(), 1u);   // the new session in the old one's place
    }
}

// P-521 both ways, against OpenSSL and Go: the group secp521r1 (not in the
// default list: the configs name it) and a leaf of P-521, which signs
// ecdsa_secp521r1_sha512 (1.3's CertificateVerify, 1.2's ServerKeyExchange)
TEST(TlsInterop, P521) {
    const bool openssl = !openssl_path().empty();
    const bool go = !go_path().empty() && !go_oracle().empty();
    if (!openssl && !go) {
        GTEST_SKIP() << "no OpenSSL, no go";
    }
    auto client_config = [] {
        tls::config cfg = trusted();
        cfg.groups = {tls::group::secp521r1};
        return cfg;
    };
    size_t n = 0;
    if (openssl) {
        // our client to s_server, 1.3 and 1.2
        {
            SServer s("p521", "P-521");
            ASSERT_TRUE(s.start());
            ASSERT_FALSE(s.wait_for("ACCEPT").empty());
            auto conn = tls::connect(sgcl::string(s.address()), client_config());
            ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
            auto st = tls::state_of(*conn);
            EXPECT_EQ(st->version, tls::version::tls13);
            EXPECT_EQ(st->group, tls::group::secp521r1);
            echo(*conn, "hello p521");
            (void)conn->close();
            ++n;
        }
        {
            SServer12 s("p521", {"-groups", "P-521", "-sigalgs", "ecdsa_secp521r1_sha512"});
            ASSERT_TRUE(s.start());
            ASSERT_FALSE(s.wait_for("ACCEPT").empty()) << slurp(s.log);
            auto conn = tls::connect(sgcl::string(s.address()), client_config());
            ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
            auto st = tls::state_of(*conn);
            EXPECT_EQ(st->version, tls::version::tls12);
            EXPECT_EQ(st->group, tls::group::secp521r1);
            echo(*conn, "hello p521 1.2");
            (void)conn->close();
            ++n;
        }
        // s_client to our server of a P-521 identity, the group P-521
        {
            auto l = net::tcp::listen("127.0.0.1:0");
            ASSERT_TRUE(l.has_value());
            Served served;
            tls::config scfg = server_config("p521");
            scfg.groups = {tls::group::secp521r1};
            std::thread server([&] { serve_one(*l, scfg, served); });
            Child c(openssl_path(), s_client_args(address_of(*l), {"-groups", "P-521", "-sigalgs", "ecdsa_secp521r1_sha512"}));
            auto in = c.cmd.stdin_pipe();
            ASSERT_TRUE(in.has_value());
            ASSERT_TRUE(c.start());
            ASSERT_TRUE(in->write(sgcl::string("hello from openssl\n")).has_value());
            std::string out = c.wait_for("lssnepo morf olleh");
            (void)in->close();
            server.join();
            (void)l->close();
            EXPECT_FALSE(out.empty()) << slurp(c.log);
            EXPECT_NE(out.find("Verification: OK"), std::string::npos);
            EXPECT_TRUE(served.error.empty()) << served.error;
            ASSERT_TRUE(served.state.has_value());
            EXPECT_EQ(served.state->group, tls::group::secp521r1);
            EXPECT_EQ(served.line, "hello from openssl");
            ++n;
        }
    }
    if (go) {
        // our client to Go's server, 1.3 and 1.2
        {
            GoServer s("p521", int(tls::group::secp521r1));
            ASSERT_TRUE(s.start());
            std::string address = s.address();
            ASSERT_FALSE(address.empty()) << slurp(s.log);
            auto conn = tls::connect(sgcl::string(address), client_config());
            ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
            EXPECT_EQ(tls::state_of(*conn)->group, tls::group::secp521r1);
            echo(*conn, "hello go p521");
            EXPECT_TRUE(conn->async_close().wait().has_value());
            ++n;
        }
        {
            Child g(go_oracle(), {sgcl::string("-cert"), sgcl::string(testdata() + "p521.pem"), sgcl::string("-key"), sgcl::string(testdata() + "p521.key"),
                                  sgcl::string("-tls12"), sgcl::string("-curves"), sgcl::string(std::to_string(int(tls::group::secp521r1)))});
            ASSERT_TRUE(g.start());
            std::string address = go_address(g);
            ASSERT_FALSE(address.empty()) << slurp(g.log);
            auto conn = tls::connect(sgcl::string(address), client_config());
            ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());
            auto st = tls::state_of(*conn);
            EXPECT_EQ(st->version, tls::version::tls12);
            EXPECT_EQ(st->group, tls::group::secp521r1);
            echo(*conn, "hello go p521 1.2");
            (void)conn->close();
            ++n;
        }
        // Go's client to our server
        {
            auto l = net::tcp::listen("127.0.0.1:0");
            ASSERT_TRUE(l.has_value());
            tls::config scfg = server_config("p521");
            scfg.groups = {tls::group::secp521r1};
            Served served;
            std::thread server([&] { serve_one(*l, scfg, served); });
            Child g(go_oracle(), {sgcl::string("-connect"), sgcl::string(address_of(*l)), sgcl::string("-ca"), sgcl::string(testdata() + "ca.pem"),
                                  sgcl::string("-curves"), sgcl::string(std::to_string(int(tls::group::secp521r1)))});
            ASSERT_TRUE(g.start());
            std::string out = g.wait_for("CLOSED");
            server.join();
            (void)l->close();
            EXPECT_FALSE(out.empty()) << slurp(g.log) << " / server: " << served.error;
            EXPECT_NE(out.find("GOT og olleh"), std::string::npos) << out;
            EXPECT_TRUE(served.error.empty()) << served.error;
            ASSERT_TRUE(served.state.has_value());
            EXPECT_EQ(served.state->group, tls::group::secp521r1);
            ++n;
        }
    }
    EXPECT_EQ(n, size_t(openssl) * 3 + size_t(go) * 3);
}
