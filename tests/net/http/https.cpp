//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// https: the module's client and server over TLS 1.3 (net/tls.h) — each
// other on the loopback (by name and by address, the pool, a certificate
// for another name, redirects from http to https and back), the client
// against Go's net/http serving TLS (go_peer, built with go), and curl
// against the server when curl is on the PATH. The certificates are
// tests/net/tls_testdata (tools/tls_testdata.sh): a test CA and a leaf for
// localhost, 127.0.0.1 and ::1.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <atomic>
#include <sys/socket.h>
#include <array>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    net::tls::config server_tls() {
        net::tls::config c;
        c.identities = {net::tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        c.alpn = {sgcl::string("http/1.1")};
        return c;
    }

    crypto::x509::certificate_pool test_roots() {
        return crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
    }

    net::http::server basic() {
        net::http::server s;
        s.route("GET /hello", [](net::http::request, net::http::response_writer w) {
            w.set_header("Content-Type", "text/plain");
            w.write("hello\n");
        });
        s.route("POST /echo", [](net::http::request req, net::http::response_writer w) -> async::task<> {
            auto body = co_await req.async_text();
            if (!body) {
                w.error(net::http::status::bad_request);
                co_return;
            }
            w.write(*body);
        });
        s.route("GET /big", [](net::http::request, net::http::response_writer w) {
            w.write(sgcl::string(std::string(300000, 'b')));
        });
        return s;
    }

    // A server on a port of the loopback, serving on the scheduler: over
    // TLS (tls::listen) or plain
    struct Running {
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        uint16_t port = 0;

        Running(net::http::server s, bool secure)
        : server(s) {
            if (secure) {
                auto l = net::tls::listen("127.0.0.1:0", server_tls());
                EXPECT_TRUE(l.has_value());
                listener = *l;
            } else {
                listener = *net::tcp::listen("127.0.0.1:0");
            }
            port = listener.local_endpoint().port();
            serving = async::spawn(server.async_serve(listener));
        }

        std::string at(const std::string& host, const std::string& path, const char* scheme = "https") const {
            return std::string(scheme) + "://" + host + ":" + std::to_string(port) + path;
        }

        ~Running() {
            server.close();
            (void)serving.wait();
        }
    };

    net::http::client trusting() {
        net::http::client c;
        c.tls.roots = test_roots();
        return c;
    }
}

// Our client and our server: by name and by address, several requests on
// the pool's connection, a body larger than a record both ways
TEST(HttpHttps_Tests, OurClientOurServer) {
    Running r(basic(), true);
    auto c = trusting();
    for (const char* host : {"localhost", "127.0.0.1", "[::1]"}) {
        SCOPED_TRACE(host);
        if (std::string(host) == "[::1]" && !net::tcp::connect(sgcl::string("[::1]:" + std::to_string(r.port)))) {
            continue;   // the listener is IPv4
        }
        for (int i = 0; i < 3; ++i) {
            auto res = c.get(sgcl::string(r.at(host, "/hello")));
            ASSERT_TRUE(res) << text(res.error().message());
            EXPECT_EQ(res->status(), 200);
            EXPECT_EQ(*res->text(), "hello\n");
        }
    }
    std::string big(200000, 'x');
    auto echo = c.post(sgcl::string(r.at("localhost", "/echo")), "text/plain", sgcl::string(big));
    ASSERT_TRUE(echo) << text(echo.error().message());
    EXPECT_EQ(echo->text()->size(), big.size());
    auto large = c.get(sgcl::string(r.at("localhost", "/big")));
    ASSERT_TRUE(large);
    EXPECT_EQ(large->text()->size(), 300000u);
}

// The server's certificate checked: an authority not trusted, a name it
// does not hold (set in the client's tls settings)
TEST(HttpHttps_Tests, CertificateChecks) {
    Running r(basic(), true);
    net::http::client untrusting;
    untrusting.tls.roots = crypto::x509::certificate_pool();
    auto a = untrusting.get(sgcl::string(r.at("localhost", "/hello")));
    ASSERT_FALSE(a);
    EXPECT_EQ(net::tls::certificate_reason(a.error()), crypto::x509::reason::unknown_authority) << text(a.error().message());
    auto c = trusting();
    c.tls.server_name = sgcl::string("wrong.example");
    auto b = c.get(sgcl::string(r.at("localhost", "/hello")));
    ASSERT_FALSE(b);
    EXPECT_EQ(net::tls::certificate_reason(b.error()), crypto::x509::reason::hostname_mismatch) << text(b.error().message());
    // plain http to a TLS listener: the handshake refused, the request fails
    auto plain = trusting().get(sgcl::string(r.at("localhost", "/hello", "http")));
    EXPECT_FALSE(plain);
}

// Redirects between the schemes, by the client's usual rule
TEST(HttpHttps_Tests, RedirectsBetweenSchemes) {
    Running secure(basic(), true);
    net::http::server p = basic();
    std::string to_secure = secure.at("localhost", "/hello");
    p.route("GET /secure", [to_secure](net::http::request, net::http::response_writer w) {
        w.redirect(sgcl::string(to_secure));
    });
    Running plain(p, false);
    net::http::server s2 = basic();
    std::string to_plain = plain.at("127.0.0.1", "/hello", "http");
    s2.route("GET /plain", [to_plain](net::http::request, net::http::response_writer w) {
        w.redirect(sgcl::string(to_plain), net::http::status::moved_permanently);
    });
    Running secure2(s2, true);
    auto c = trusting();
    auto up = c.get(sgcl::string(plain.at("127.0.0.1", "/secure", "http")));
    ASSERT_TRUE(up) << text(up.error().message());
    EXPECT_EQ(*up->text(), "hello\n");
    EXPECT_EQ(up->url().scheme(), "https");
    auto down = c.get(sgcl::string(secure2.at("localhost", "/plain")));
    ASSERT_TRUE(down) << text(down.error().message());
    EXPECT_EQ(*down->text(), "hello\n");
    EXPECT_EQ(down->url().scheme(), "http");
}

namespace {
    const std::string& peer() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto src = source_root() / "tests/net/http/go_peer/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_http_go_peer_tls";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }
}

// Our client against Go's net/http over TLS
TEST(HttpHttps_Tests, OurClientAgainstGosTlsServer) {
    if (peer().empty()) {
        GTEST_SKIP() << "no go to build the peer with";
    }
    FILE* p = popen(("'" + peer() + "' server '" + testdata("ecdsa.pem") + "' '" + testdata("ecdsa.key") + "'").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof(line), p));
    int port = std::atoi(line + 5);
    ASSERT_GT(port, 0);
    std::string base = "https://localhost:" + std::to_string(port);
    auto c = trusting();
    auto hello = c.get(sgcl::string(base + "/hello"));
    ASSERT_TRUE(hello) << text(hello.error().message());
    EXPECT_EQ(*hello->text(), "hello from go\n");
    auto stream = c.get(sgcl::string(base + "/stream"));
    ASSERT_TRUE(stream);
    EXPECT_EQ(*stream->text(), "part 0\npart 1\npart 2\n");
    std::string body(100000, 'q');
    auto echo = c.post(sgcl::string(base + "/echo"), "text/plain", sgcl::string(body));
    ASSERT_TRUE(echo);
    EXPECT_EQ(echo->header("X-Length"), "100000");
    EXPECT_EQ(text(*echo->text()), body);   // read to its end: the connection goes back to the pool
    auto redirected = c.get(sgcl::string(base + "/redirect/3"));
    ASSERT_TRUE(redirected);
    EXPECT_EQ(*redirected->text(), "hello from go\n");
    auto conns = c.get(sgcl::string(base + "/conns"));
    ASSERT_TRUE(conns);
    EXPECT_EQ(*conns->text(), "1");   // one TLS connection, from the pool each time
    (void)c.get(sgcl::string(base + "/quit"));
    pclose(p);
}

// Our client against Go's net/http over TLS 1.2 alone (a server without
// 1.3, as some load balancers are): HTTP/2 by ALPN over 1.2 (RFC 7540 §9.2:
// ECDHE and an AEAD) and HTTP/1.1, transparently; the client's next
// connection resuming the session (its cache, no code of the user's)
TEST(HttpHttps_Tests, OurClientAgainstGosTls12Server) {
    if (peer().empty()) {
        GTEST_SKIP() << "no go to build the peer with";
    }
    FILE* p = popen(("GO_PEER_TLS12=1 '" + peer() + "' server '" + testdata("ecdsa.pem") + "' '" + testdata("ecdsa.key") + "'").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof(line), p));
    int port = std::atoi(line + 5);
    ASSERT_GT(port, 0);
    std::string base = "https://localhost:" + std::to_string(port);
    for (bool http2 : {true, false}) {
        SCOPED_TRACE(http2);
        auto c = trusting();
        c.http2 = http2;
        auto hello = c.get(sgcl::string(base + "/hello"));
        ASSERT_TRUE(hello) << text(hello.error().message());
        EXPECT_EQ(*hello->text(), "hello from go\n");
        EXPECT_EQ(text(hello->proto()), http2 ? "HTTP/2.0" : "HTTP/1.1");
        std::string body(100000, 'q');
        auto echo = c.post(sgcl::string(base + "/echo"), "text/plain", sgcl::string(body));
        ASSERT_TRUE(echo);
        EXPECT_EQ(text(*echo->text()), body);
    }
    // a new connection of the client resumes the session of the one before, by ticket
    for (bool http2 : {true, false}) {
        SCOPED_TRACE(http2);
        auto c = trusting();
        c.http2 = http2;
        auto first = c.get(sgcl::string(base + "/tls"));
        ASSERT_TRUE(first) << text(first.error().message());
        EXPECT_EQ(text(*first->text()), "resumed=false version=303");
        c.close_idle_connections();
        auto second = c.get(sgcl::string(base + "/tls"));
        ASSERT_TRUE(second) << text(second.error().message());
        EXPECT_EQ(text(*second->text()), "resumed=true version=303");
    }
    // a client that requires 1.3: refused
    auto strict = trusting();
    strict.tls.min_version = net::tls::version::tls13;
    auto refused = strict.get(sgcl::string(base + "/hello"));
    ASSERT_FALSE(refused);
    EXPECT_NE(text(refused.error().message()).find("protocol version"), std::string::npos) << text(refused.error().message());
    (void)trusting().get(sgcl::string(base + "/quit"));
    pclose(p);
}

// curl against our server, when curl is on the PATH
TEST(HttpHttps_Tests, CurlAgainstOurServer) {
    if (std::system("command -v curl > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no curl on the PATH";
    }
    Running r(basic(), true);
    std::string cmd = "curl -s --max-time 10 --cacert '" + testdata("ca.pem") + "' " + r.at("localhost", "/hello") + " 2>&1";
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    ASSERT_TRUE(p);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
        out.append(buf, n);
    }
    pclose(p);
    EXPECT_EQ(out, "hello\n");
    std::string post = "curl -s --max-time 10 --cacert '" + testdata("ca.pem") + "' --data-binary 'posted by curl' " + r.at("127.0.0.1", "/echo") + " 2>&1";
    out.clear();
    p = popen(post.c_str(), "r");
    ASSERT_TRUE(p);
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
        out.append(buf, n);
    }
    pclose(p);
    EXPECT_EQ(out, "posted by curl");
}

// A head larger than the TLS layer's small record block (4 KB): its
// record comes in two reads, the second already in the socket when the
// first is taken. The server's head is read without waiting (try_fill over
// TLS try_raw_read), which must go on reading until the socket would
// block before it waits: the socket's readiness is edge-triggered, and a
// wait after a partial read would wait for nothing (found by h2spec 4.2 on
// the HTTP/2 reader, which reads so too)
TEST(HttpHttps_Tests, HeadLargerThanASmallRecordBlock) {
    auto s = basic();
    s.read_header_timeout = std::chrono::seconds(5);
    Running r(s, true);
    auto c = trusting();
    for (size_t size : {3000u, 6000u, 12000u, 16000u}) {
        SCOPED_TRACE(size);
        net::http::request req("GET", sgcl::string(r.at("localhost", "/hello")));
        req.set_header("X-Big", sgcl::string(std::string(size, 'v')));
        auto began = std::chrono::steady_clock::now();
        auto res = c.send(req);
        ASSERT_TRUE(res) << text(res.error().message());
        EXPECT_EQ(res->status(), 200);
        EXPECT_EQ(*res->text(), "hello\n");
        EXPECT_LT(std::chrono::steady_clock::now() - began, std::chrono::seconds(2));
    }
}

// A body written whole goes out as the head and its blocks in one write
// (ConnImpl::start_write_parts): sendmsg over plain TCP, records sealed
// from the blocks over TLS. The bytes arrive as they were written, in
// order, over both, whatever the size: under a record, a record and a
// half, and 8 MB, which the socket takes in many waits (the rest of the
// write, from the middle of a batch of records and of a block); several
// clients at once, so that the writes of the connections interleave
TEST(HttpHttps_Tests, ABodysBlocksGoOutAsTheyAre) {
    auto pattern = [](size_t n) {
        std::string s(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            s[i] = char('a' + (i * 7 + i / 8191) % 26);
        }
        return s;
    };
    auto serving = [&] {   // a server of its own for each run (the one before closed)
        net::http::server s;
        s.route("GET /size", [&](net::http::request req, net::http::response_writer w) {
            size_t n = std::strtoull(std::string(req.url().query().view()).c_str(), nullptr, 10);
            w.write(sgcl::string(pattern(n)));
        });
        return s;
    };
    for (bool secure : {false, true}) {
        SCOPED_TRACE(secure ? "https" : "http");
        Running r(serving(), secure);
        const char* scheme = secure ? "https" : "http";
        for (size_t n : {size_t(100), size_t(9000), size_t(16384), size_t(24600), size_t(65536), size_t(8 << 20)}) {
            auto c = trusting();
            auto res = c.get(sgcl::string(r.at("127.0.0.1", "/size?" + std::to_string(n), scheme)));
            ASSERT_TRUE(res) << text(res.error().message());
            auto body = res->text();
            ASSERT_TRUE(body);
            EXPECT_EQ(body->size(), n);
            EXPECT_TRUE(body->view() == pattern(n)) << n;
        }
        std::vector<std::thread> clients;
        std::atomic<int> good = 0;
        for (int t = 0; t < 4; ++t) {
            clients.emplace_back([&] {
                auto c = trusting();
                for (int i = 0; i < 3; ++i) {
                    auto res = c.get(sgcl::string(r.at("127.0.0.1", "/size?" + std::to_string(3 << 20), scheme)));
                    if (!res) {
                        continue;
                    }
                    auto body = res->text();   // read once: the body is the response's
                    if (body && body->view() == pattern(3 << 20)) {
                        ++good;
                    }
                }
            });
        }
        for (auto& t : clients) {
            t.join();
        }
        EXPECT_EQ(good.load(), 12);
    }
}

// write(file): the file from its position is the body. Alone, it goes after
// the head by sendfile (http) or sealed from its blocks (https), its length
// the Content-Length, a connection kept for the next request; with bytes
// before or after it, or over HTTP/2 (h2c), its bytes are taken into the
// body as written ones. A file larger than the socket's buffers
TEST(HttpHttps_Tests, AFileWrittenAsTheBody) {
    const std::string path = (std::filesystem::temp_directory_path() / ("sgcl_http_file_" + std::to_string(::getpid()))).string();
    std::string content(5 << 20, '\0');
    for (size_t i = 0; i < content.size(); ++i) {
        content[i] = char('a' + (i * 13 + i / 4093) % 26);
    }
    {
        std::ofstream out(path, std::ios::binary);
        out.write(content.data(), std::streamsize(content.size()));
    }
    auto serving = [&] {   // a server of its own for each run (the one before closed)
        net::http::server s;
        s.h2c = true;
        s.route("GET /file", [&](net::http::request req, net::http::response_writer w) {
            auto f = io::open(sgcl::string(path));
            if (!f) {
                w.error(net::http::status::internal_server_error);
                return;
            }
            (void)f->seek(int64_t(std::strtoull(std::string(req.url().query().view()).c_str(), nullptr, 10)));
            w.set_header("Content-Type", "application/octet-stream");
            w.write(*f);
        });
        s.route("GET /mixed", [&](net::http::request, net::http::response_writer w) {
            auto f = io::open(sgcl::string(path));
            if (!f) {
                w.error(net::http::status::internal_server_error);
                return;
            }
            w.write("<").write(*f).write(">");
        });
        return s;
    };
    for (bool secure : {false, true}) {
        SCOPED_TRACE(secure ? "https" : "http");
        Running r(serving(), secure);
        const char* scheme = secure ? "https" : "http";
        auto c = trusting();
        for (size_t off : {size_t(0), size_t(1), size_t(4 << 20), content.size()}) {
            auto res = c.get(sgcl::string(r.at("127.0.0.1", "/file?" + std::to_string(off), scheme)));
            ASSERT_TRUE(res) << text(res.error().message());
            EXPECT_EQ(res->status(), 200);
            EXPECT_EQ(std::string(res->header("Content-Length").view()), std::to_string(content.size() - off));
            EXPECT_EQ(res->header("Content-Type"), "application/octet-stream");
            auto body = res->text();
            ASSERT_TRUE(body);
            ASSERT_EQ(body->size(), content.size() - off) << off;
            EXPECT_TRUE(body->view() == std::string_view(content).substr(off)) << off;
        }
        auto mixed = c.get(sgcl::string(r.at("127.0.0.1", "/mixed", scheme)));
        ASSERT_TRUE(mixed) << text(mixed.error().message());
        auto body = mixed->text();
        ASSERT_TRUE(body);
        EXPECT_TRUE(body->view() == "<" + content + ">");
        if (!secure) {
            net::http::client h2;
            h2.h2c = true;
            auto res = h2.get(sgcl::string(r.at("127.0.0.1", "/file?7", "http")));
            ASSERT_TRUE(res) << text(res.error().message());
            EXPECT_EQ(res->proto(), "HTTP/2.0");
            auto b = res->text();
            ASSERT_TRUE(b);
            EXPECT_TRUE(b->view() == std::string_view(content).substr(7));
            h2.close_idle_connections();   // GOAWAY: the server's end closes now, not when the client's is collected
        }
        c.close_idle_connections();   // the kept connections closed: no server connection outlives the test
    }
    std::filesystem::remove(path);
}

namespace {
    // The pieces of the flushed responses: sizes across the inline bytes,
    // a block and its edge, several blocks
    const size_t flushed_sizes[] = {1, 70, 8192 + 5, 20000, 64, 33000, 3, 9000};

    std::string flushed_piece(size_t k) {
        std::string s(flushed_sizes[k], '\0');
        for (size_t i = 0; i < s.size(); ++i) {
            s[i] = char('A' + (i * 7 + k * 13 + i / 8191) % 50);
        }
        return s;
    }

    std::string flushed_all() {
        std::string s;
        for (size_t k = 0; k < std::size(flushed_sizes); ++k) {
            s += flushed_piece(k);
        }
        return s;
    }

    // A chunked body decoded; empty and `ok` false when it is not one
    std::string dechunk(const std::string& body, bool& ok) {
        std::string out;
        size_t at = 0;
        ok = false;
        for (;;) {
            size_t line = body.find("\r\n", at);
            if (line == std::string::npos) {
                return out;
            }
            const size_t n = std::strtoull(body.c_str() + at, nullptr, 16);
            at = line + 2;
            if (n == 0) {
                ok = body.compare(at, std::string::npos, "\r\n") == 0;
                return out;
            }
            if (at + n + 2 > body.size() || body.compare(at + n, 2, "\r\n") != 0) {
                return out;
            }
            out.append(body, at, n);
            at += n + 2;
        }
    }
}

// Flushed responses written as pieces (the size line, the body's blocks
// where they lie, the CRLF) under a reader that takes little at a time: a
// receive buffer of 4 KB and pauses, so the server's writes are cut short
// again and again. Chunked, with the handler's Content-Length, and past
// it (the rest dropped, the connection closed); over TCP and TLS
TEST(HttpHttps_Tests, FlushedPiecesUnderPartialWrites) {
    auto serving = [] {   // a server of its own for each run (the one before closed)
        net::http::server s;
        auto route = [&s](const char* path, int length) {   // length: -1 none, 0 exact, 1 short by 1000
            s.route(path, [length](net::http::request, net::http::response_writer w) -> async::task<> {
                if (length >= 0) {
                    const size_t total = flushed_all().size() - (length ? 1000 : 0);
                    w.set_header("Content-Length", sgcl::string(std::to_string(total)));
                }
                for (size_t k = 0; k < std::size(flushed_sizes); ++k) {
                    w.write(sgcl::string(flushed_piece(k)));
                    if (!co_await w.async_flush()) {
                        co_return;
                    }
                }
            });
        };
        route("GET /chunked", -1);
        route("GET /declared", 0);
        route("GET /over", 1);
        return s;
    };
    for (bool secure : {false, true}) {
        SCOPED_TRACE(secure ? "https" : "http");
        Running r(serving(), secure);
        for (const char* path : {"/chunked", "/declared", "/over"}) {
            SCOPED_TRACE(path);
            auto tcp = net::tcp::connect(sgcl::string("127.0.0.1:" + std::to_string(r.port)));
            ASSERT_TRUE(tcp);
            const int fd = static_cast<net::detail::SocketConn&>(net::detail::ConnectionAccess::impl(*tcp)).fd();
            const int bytes = 4096;
            ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof(bytes));
            net::connection c = *tcp;
            if (secure) {
                net::tls::config cfg;
                cfg.roots = test_roots();
                cfg.server_name = sgcl::string("localhost");
                auto t = net::tls::client(*tcp, cfg);
                ASSERT_TRUE(t) << text(t.error().message());
                c = *t;
            }
            c.set_read_deadline(std::chrono::steady_clock::now() + 20s);   // a response that never ends fails, not hangs
            ASSERT_TRUE(c.write(sgcl::string(std::string("GET ") + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n")));
            std::string got;
            std::array<std::byte, 4096> piece;
            size_t since = 0;
            for (;;) {
                auto n = c.read(slice<byte>(piece.data(), piece.size()));
                if (!n || *n == 0) {
                    break;
                }
                got.append(reinterpret_cast<const char*>(piece.data()), *n);
                since += *n;
                if (since >= (16 << 10)) {
                    since = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                // a kept connection: the response is whole at its end
                const size_t head = got.find("\r\n\r\n");
                if (head != std::string::npos && std::string(path) != "/over") {
                    const std::string body = got.substr(head + 4);
                    if (std::string(path) == "/chunked" ? body.size() >= 5 && body.compare(body.size() - 5, 5, "0\r\n\r\n") == 0
                                                         : body.size() >= flushed_all().size()) {
                        break;
                    }
                }
            }
            (void)c.close();
            const size_t head = got.find("\r\n\r\n");
            ASSERT_NE(head, std::string::npos) << got.substr(0, 200);
            const std::string head_text = got.substr(0, head);
            const std::string body = got.substr(head + 4);
            const std::string all = flushed_all();
            if (std::string(path) == "/chunked") {
                EXPECT_NE(head_text.find("Transfer-Encoding: chunked"), std::string::npos) << head_text;
                bool ok = false;
                const std::string plain = dechunk(body, ok);
                EXPECT_TRUE(ok) << "not a chunked body of " << body.size() << " bytes";
                ASSERT_EQ(plain.size(), all.size());
                EXPECT_TRUE(plain == all);
            } else if (std::string(path) == "/declared") {
                EXPECT_NE(head_text.find("Content-Length: " + std::to_string(all.size())), std::string::npos) << head_text;
                ASSERT_EQ(body.size(), all.size());
                EXPECT_TRUE(body == all);
            } else {
                EXPECT_NE(head_text.find("Content-Length: " + std::to_string(all.size() - 1000)), std::string::npos) << head_text;
                ASSERT_EQ(body.size(), all.size() - 1000);   // the rest dropped, the connection closed after it
                EXPECT_TRUE(body == all.substr(0, all.size() - 1000));
            }
        }
    }
}

namespace {
    std::string posted_pattern(size_t n, unsigned seed) {
        std::string s(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            s[i] = char((i * 31 + seed * 7 + i / 8191) % 251);
        }
        return s;
    }
}

// The client's request bodies written from where they lie: the head and a
// body in memory as one write, a stream's chunks each as its size line,
// its bytes and the CRLF. A server that reads slowly (4 KB at a time, a
// pause after each 64 KB) fills the socket's buffers, so the client's
// writes are cut short again and again; the server checks every byte.
// A string, a vector, a short body in the head, a stream chunked and one
// of a known length; over TCP and TLS
TEST(HttpHttps_Tests, RequestBodiesUnderPartialWrites) {
    auto serving = [] {   // a server of its own for each run (the one before closed)
        net::http::server s;
        s.max_body_bytes = 16 << 20;
        s.route("POST /check", [](net::http::request r, net::http::response_writer w) -> async::task<> {
            std::string q(r.url().query().view());
            const size_t n = std::strtoull(q.c_str(), nullptr, 10);
            const unsigned seed = unsigned(std::strtoul(q.c_str() + q.find('.') + 1, nullptr, 10));
            auto body = r.body();
            tracked_ptr block = make_tracked<io::detail::CopyBlock>();   // read 4 KB of it at a time
            std::string got;
            size_t since = 0;
            for (;;) {
                auto k = co_await body.async_read(slice<byte>(block, block->data(), 4096));
                if (!k || *k == 0) {
                    break;
                }
                got.append(reinterpret_cast<const char*>(block->data()), *k);
                since += *k;
                if (since >= (64 << 10)) {
                    since = 0;
                    co_await async::after(std::chrono::milliseconds(1));
                }
            }
            w.write(got == posted_pattern(n, seed) ? "ok\n" : sgcl::string("bad " + std::to_string(got.size()) + "\n"));
        });
        return s;
    };
    for (bool secure : {false, true}) {
        SCOPED_TRACE(secure ? "https" : "http");
        Running r(serving(), secure);
        const char* scheme = secure ? "https" : "http";
        auto c = trusting();
        auto url = [&](size_t n, unsigned seed) {
            return sgcl::string(r.at("localhost", "/check?" + std::to_string(n) + "." + std::to_string(seed), scheme));
        };
        auto check = [&](const net::http::request& req, const char* what) {
            SCOPED_TRACE(what);
            auto res = c.send(req);
            ASSERT_TRUE(res) << text(res.error().message());
            EXPECT_EQ(res->status(), 200);
            auto t = res->text();
            ASSERT_TRUE(t);
            EXPECT_EQ(text(*t), "ok\n");
        };
        {
            net::http::request req("POST", url(4 << 20, 1));
            req.set_body(sgcl::string(posted_pattern(4 << 20, 1)));
            check(req, "a string of 4 MB");
        }
        {
            const std::string p = posted_pattern(3 << 20, 2);
            vector<byte> v;
            v.resize(p.size());
            std::memcpy(v.data(), p.data(), p.size());
            net::http::request req("POST", url(p.size(), 2));
            req.set_body(std::move(v));
            check(req, "a vector of 3 MB");
        }
        {
            net::http::request req("POST", url(100, 3));
            req.set_body(sgcl::string(posted_pattern(100, 3)));
            check(req, "a short body, in the head");
        }
        {
            net::http::request req("POST", url(2 << 20, 4));
            req.set_body(io::reader(make_tracked<io::buffer>(sgcl::string(posted_pattern(2 << 20, 4)))));
            check(req, "a stream, chunked");
        }
        {
            net::http::request req("POST", url(2 << 20, 5));
            req.set_body(io::reader(make_tracked<io::buffer>(sgcl::string(posted_pattern(2 << 20, 5)))), uint64_t(2 << 20));
            check(req, "a stream of a known length");
        }
        c.close_idle_connections();
    }
}

// A server that answers each request and then ends the TLS connection
// (close_notify), without Connection: close, as a server whose keep-alive
// ran out does: the next request on the pooled connection goes again on a
// fresh one when it can be sent again (a body in memory), as Go's
// Transport does; a stream body is not sent twice
TEST(HttpHttps_Tests, APooledConnectionTheServerClosedIsDialedAgain) {
    auto l = net::tls::listen("127.0.0.1:0", server_tls());
    ASSERT_TRUE(l);
    net::listener listener = *l;
    const uint16_t port = listener.local_endpoint().port();
    std::atomic<int> accepted{0};
    std::thread serving([&listener, &accepted] {   // the handle stays on this stack: a thread's closure is not managed memory
        for (;;) {
            auto c = listener.accept();
            if (!c) {
                return;
            }
            ++accepted;
            size_t length = 0;
            for (;;) {
                auto line = c->read_line();
                if (!line || !*line || (*line)->empty()) {
                    break;
                }
                std::string h = text(**line);
                for (auto& ch : h) {
                    ch = char(std::tolower(static_cast<unsigned char>(ch)));
                }
                if (h.rfind("content-length:", 0) == 0) {
                    length = std::stoul(h.substr(15));
                }
            }
            std::string body(length, '\0');
            if (length) {
                (void)c->read_full(sgcl::slice<byte>(reinterpret_cast<byte*>(body.data()), body.size()));
            }
            std::string reply = "answer " + std::to_string(accepted.load()) + ":" + body;
            (void)c->write(sgcl::string("HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(reply.size()) + "\r\n\r\n" + reply));
            (void)c->close();   // close_notify, then the transport closed
        }
    });
    auto c = trusting();
    const std::string base = "https://localhost:" + std::to_string(port);
    auto first = c.get(sgcl::string(base + "/"));
    ASSERT_TRUE(first) << text(first.error().message());
    EXPECT_EQ(*first->text(), "answer 1:");
    std::this_thread::sleep_for(50ms);   // the close_notify is there before the next request
    auto second = c.post(sgcl::string(base + "/"), "text/plain", "abc");   // a body in memory
    ASSERT_TRUE(second) << text(second.error().message());
    EXPECT_EQ(*second->text(), "answer 2:abc");
    EXPECT_EQ(accepted.load(), 2);
    std::this_thread::sleep_for(50ms);
    auto post = net::http::request("POST", sgcl::string(base + "/"));
    post.set_body(io::reader(make_tracked<io::buffer>("stream")));
    auto streamed = c.send(post);   // on the closed pooled connection; a stream is not sent twice
    if (streamed) {
        EXPECT_EQ(*streamed->text(), "answer 3:stream");   // the pool found the connection closed before using it
    }
    listener.close();
    serving.join();
}
