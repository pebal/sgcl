//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string testdata(const std::string& name) {
        return (std::filesystem::path(__FILE__).parent_path().parent_path() / "tls_testdata" / name).string();
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
            auto src = std::filesystem::path(__FILE__).parent_path() / "go_peer" / "main.go";
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
