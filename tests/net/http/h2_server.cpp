//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http/2: the module's server over HTTP/2 (detail/h2/serve.h), with the same
// handlers as HTTP/1.1, against Go's net/http client (tools/h2_oracle.go,
// built with go) and curl with nghttp2: over TLS when ALPN chose "h2", on a
// plain port by prior knowledge (h2c) beside HTTP/1.1; one request, a
// hundred at once on one connection, a body both ways, a response flushed
// in pieces, request::proto(), a hijack refused. Skipped without go or curl.
#include "tests/types.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/http/detail/h2/frame.h"
#include "sgcl/net/http/detail/h2/hpack.h"
#include "sgcl/async/timeout.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string testdata(const std::string& name) {
        return (std::filesystem::path(__FILE__).parent_path().parent_path() / "tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    const std::string& oracle() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
            auto src = root / "tools" / "h2_oracle.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_h2_oracle";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    std::vector<std::string> lines_with(const std::string& out, const std::string& prefix) {
        std::vector<std::string> v;
        std::istringstream in(out);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind(prefix, 0) == 0) {
                v.push_back(line);
            }
        }
        return v;
    }

    std::string field(const std::string& line, const std::string& name) {
        auto at = line.find(name + "=");
        if (at == std::string::npos) {
            return "";
        }
        at += name.size() + 1;
        return line.substr(at, line.find(' ', at) - at);
    }

    net::http::server handlers() {
        net::http::server s;
        s.h2c = true;
        s.route("GET /hello", [](net::http::request r, net::http::response_writer w) {
            w.set_header("Content-Type", "text/plain");
            w.write("hello over " + r.proto() + "\n");
        });
        s.route("POST /echo", [](net::http::request r, net::http::response_writer w) -> async::task<> {
            auto body = co_await r.async_bytes();
            if (!body) {
                w.error(net::http::status::bad_request);
                co_return;
            }
            w.write(*body);
        });
        s.route("GET /stream", [](net::http::request, net::http::response_writer w) -> async::task<> {
            for (int i = 0; i < 3; ++i) {
                w.write(sgcl::string("part " + std::to_string(i) + "\n"));
                auto f = co_await w.async_flush();
                if (!f) {
                    co_return;
                }
            }
        });
        s.route("GET /big", [](net::http::request, net::http::response_writer w) {
            w.write(sgcl::string(std::string(300000, 'b')));
        });
        s.route("GET /slow", [](net::http::request, net::http::response_writer w) -> async::task<> {
            co_await async::after(300ms);
            w.write("slow\n");
        });
        s.route("POST /trailers", [](net::http::request r, net::http::response_writer w) -> async::task<> {
            auto body = co_await r.async_bytes();
            w.write(sgcl::string("body=" + std::to_string(body ? body->size() : 0) + " x-sum=" + std::string(r.trailers().get("x-sum").view()) + "\n"));
        });
        s.route("GET /hijack", [](net::http::request, net::http::response_writer w) {
            auto h = w.hijack();
            w.write(h ? "hijacked\n" : "no hijack\n");
        });
        return s;
    }

    struct Running {
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        uint16_t port = 0;

        Running(net::http::server s, bool secure)
        : server(s) {
            if (secure) {
                net::tls::config c;
                c.identities = {net::tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
                c.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
                auto l = net::tls::listen("127.0.0.1:0", c);
                EXPECT_TRUE(l.has_value());
                listener = *l;
            } else {
                listener = *net::tcp::listen("127.0.0.1:0");
            }
            port = listener.local_endpoint().port();
            serving = async::spawn(server.async_serve(listener));
        }

        ~Running() {
            server.close();
            (void)serving.wait();
        }

        std::string go(const std::string& args, bool secure) const {
            std::string cmd = "'" + oracle() + "' -connect 127.0.0.1:" + std::to_string(port) + " ";
            cmd += secure ? "-ca '" + testdata("ca.pem") + "' -servername localhost " : "-protocols h2c ";
            return run(cmd + args + " 2>&1");
        }
    };
}

// Go's client, h2 by prior knowledge on a plain port: the handler's proto
// is HTTP/2.0, as the response's
TEST(H2Server_Tests, GoClientH2c) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    Running r(handlers(), false);
    auto out = r.go("-path /hello -print", false);
    auto resp = lines_with(out, "RESP 0 ");
    ASSERT_EQ(resp.size(), 1u) << out;
    EXPECT_NE(resp[0].find(" 200 HTTP/2.0 "), std::string::npos) << out;
    EXPECT_NE(out.find("BODY 0 hello over HTTP/2.0"), std::string::npos) << out;
}

// Over TLS, h2 chosen by ALPN
TEST(H2Server_Tests, GoClientTlsAlpn) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    Running r(handlers(), true);
    auto out = r.go("-path /hello -print", true);
    auto resp = lines_with(out, "RESP 0 ");
    ASSERT_EQ(resp.size(), 1u) << out;
    EXPECT_NE(resp[0].find(" 200 HTTP/2.0 h2 "), std::string::npos) << out;
    EXPECT_NE(out.find("BODY 0 hello over HTTP/2.0"), std::string::npos) << out;
}

// A hundred requests at once: one connection, streams side by side (up
// to our MAX_CONCURRENT_STREAMS, 250)
TEST(H2Server_Tests, HundredStreamsOneConnection) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    for (bool secure : {false, true}) {
        SCOPED_TRACE(secure ? "tls" : "h2c");
        Running r(handlers(), secure);
        auto out = r.go("-path /big -n 100", secure);
        auto resp = lines_with(out, "RESP ");
        ASSERT_EQ(resp.size(), 100u) << out;
        for (auto& l : resp) {
            EXPECT_EQ(field(l, "bytes"), "300000") << l;
        }
        EXPECT_NE(out.find("CONNS 1"), std::string::npos) << out;
    }
}

// A body both ways, larger than the windows (65535 at the start)
TEST(H2Server_Tests, BodyBothWays) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    for (bool secure : {false, true}) {
        SCOPED_TRACE(secure ? "tls" : "h2c");
        Running r(handlers(), secure);
        auto out = r.go("-method POST -path /echo -body 1000000 -n 4", secure);
        auto sent = lines_with(out, "SENT ");
        auto resp = lines_with(out, "RESP ");
        ASSERT_EQ(sent.size(), 1u) << out;
        ASSERT_EQ(resp.size(), 4u) << out;
        for (auto& l : resp) {
            EXPECT_EQ(field(l, "bytes"), "1000000") << l;
            EXPECT_EQ(field(l, "crc32"), field(sent[0], "crc32")) << l;
        }
    }
}

// A flush is DATA without END_STREAM: the pieces come as written
TEST(H2Server_Tests, FlushedPieces) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    Running r(handlers(), false);
    auto out = r.go("-path /stream -print", false);
    EXPECT_NE(out.find("BODY 0 part 0\nBODY 0 part 1\nBODY 0 part 2"), std::string::npos) << out;
}

// Hijack refused on a stream: the handler answers without it
TEST(H2Server_Tests, HijackRefused) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    Running r(handlers(), false);
    auto out = r.go("-path /hijack -print", false);
    EXPECT_NE(out.find("BODY 0 no hijack"), std::string::npos) << out;
}

// HTTP/1.1 still on the h2c port; our own client (HTTP/1.1) sees proto 1.1
TEST(H2Server_Tests, Http11BesideH2c) {
    Running r(handlers(), false);
    net::http::client c;
    auto res = c.get(sgcl::string("http://127.0.0.1:" + std::to_string(r.port) + "/hello"));
    ASSERT_TRUE(res) << std::string(res.error().message().view());
    EXPECT_EQ(res->proto(), "HTTP/1.1");
    EXPECT_EQ(*res->text(), "hello over HTTP/1.1\n");
}

// curl with nghttp2: --http2-prior-knowledge on the plain port, --http2
// over TLS
TEST(H2Server_Tests, Curl) {
    if (std::system("command -v curl > /dev/null 2>&1") != 0 || run("curl -V").find("HTTP2") == std::string::npos) {
        GTEST_SKIP() << "no curl with HTTP/2 on the PATH";
    }
    {
        Running r(handlers(), false);
        auto out = run("curl -s --max-time 10 --http2-prior-knowledge -w '%{http_version}' http://127.0.0.1:" + std::to_string(r.port) + "/hello 2>&1");
        EXPECT_EQ(out, "hello over HTTP/2.0\n2");
        std::string post = "curl -s --max-time 10 --http2-prior-knowledge --data-binary 'posted by curl' http://127.0.0.1:" + std::to_string(r.port) + "/echo 2>&1";
        EXPECT_EQ(run(post), "posted by curl");
    }
    {
        Running r(handlers(), true);
        auto out = run("curl -s --max-time 10 --http2 --cacert '" + testdata("ca.pem") + "' -w '%{http_version}' https://localhost:" + std::to_string(r.port) + "/hello 2>&1");
        EXPECT_EQ(out, "hello over HTTP/2.0\n2");
    }
}

// The request's trailers (HEADERS after DATA, END_STREAM with them)
TEST(H2Server_Tests, RequestTrailers) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    Running r(handlers(), false);
    auto out = r.go("-method POST -path /trailers -body 70000 -trailer x-sum=abc -print", false);
    EXPECT_NE(out.find("BODY 0 body=70000 x-sum=abc"), std::string::npos) << out;
}

// shutdown() on a connection with streams open: they finish, the client
// gets every answer, the connection ends with GOAWAY and shutdown returns
TEST(H2Server_Tests, ShutdownGraceful) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    Running r(handlers(), false);
    std::string out;
    std::thread client([&] {
        out = r.go("-path /slow -n 5", false);
    });
    std::this_thread::sleep_for(150ms);
    auto began = std::chrono::steady_clock::now();
    r.server.shutdown();
    auto took = std::chrono::steady_clock::now() - began;
    client.join();
    EXPECT_EQ(lines_with(out, "RESP ").size(), 5u) << out;
    EXPECT_TRUE(lines_with(out, "ERROR").empty()) << out;
    EXPECT_LT(took, 5s);
}

// serve_tls completes the config's ALPN as Go's ListenAndServeTLS does
// (adjustNextProtos): h2 added at the end when on and absent, http/1.1
// added when absent, the order given kept (the server's order is the
// preference)
TEST(H2Server_Tests, ServeTlsAlpnList) {
    auto alpn = [](std::vector<std::string> given, bool http2) {
        net::tls::config c;
        for (auto& p : given) {
            c.alpn.push_back(sgcl::string(p));
        }
        std::string out;
        for (auto& p : net::http::detail::adjusted_alpn(c, http2).alpn) {
            out += std::string(p.view()) + ",";
        }
        return out;
    };
    EXPECT_EQ(alpn({}, true), "h2,http/1.1,");
    EXPECT_EQ(alpn({"http/1.1"}, true), "http/1.1,h2,");
    EXPECT_EQ(alpn({"h2"}, true), "h2,http/1.1,");
    EXPECT_EQ(alpn({"acme-tls/1"}, true), "acme-tls/1,h2,http/1.1,");
    EXPECT_EQ(alpn({"h2", "http/1.1"}, false), "http/1.1,");
    EXPECT_EQ(alpn({}, false), "http/1.1,");
}

// Go's client (offering h2 and http/1.1) against serve_tls: a config
// without ALPN gives h2; {"http/1.1"} stays HTTP/1.1, being first in the
// server's order
TEST(H2Server_Tests, ServeTlsGoClient) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build tools/h2_oracle.go with";
    }
    for (bool only11 : {false, true}) {
        SCOPED_TRACE(only11 ? "alpn {http/1.1}" : "alpn {}");
        net::tls::config c;
        c.identities = {net::tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        if (only11) {
            c.alpn = {sgcl::string("http/1.1")};
        }
        uint16_t port = 0;
        {
            auto probe = net::tcp::listen("127.0.0.1:0");   // a free port, handed to serve_tls
            ASSERT_TRUE(probe.has_value());
            port = probe->local_endpoint().port();
            (void)probe->close();
        }
        net::http::server s = handlers();
        auto serving = async::spawn(s.async_serve_tls(sgcl::string("127.0.0.1:" + std::to_string(port)), c));
        std::string out;
        for (int attempt = 0; attempt < 50; ++attempt) {   // until it listens, at most 5 s
            out = run("'" + oracle() + "' -connect 127.0.0.1:" + std::to_string(port) + " -ca '" + testdata("ca.pem") + "' -servername localhost -path /hello 2>&1");
            if (!lines_with(out, "RESP 0 ").empty()) {
                break;
            }
            std::this_thread::sleep_for(100ms);
        }
        s.close();
        (void)serving.wait();
        auto resp = lines_with(out, "RESP 0 ");
        ASSERT_EQ(resp.size(), 1u) << out;
        if (only11) {
            EXPECT_NE(resp[0].find(" 200 HTTP/1.1 http/1.1 "), std::string::npos) << out;
        } else {
            EXPECT_NE(resp[0].find(" 200 HTTP/2.0 h2 "), std::string::npos) << out;
        }
    }
}

namespace {
    namespace h2 = sgcl::net::http::detail::h2;

    // A client of HTTP/2 by hand over a plain connection (h2c): frames
    // written with FrameWriter and HPACK's Encoder, the server's frames
    // read back one at a time, each read bounded by a deadline
    struct RawClient {
        net::connection c;
        h2::Encoder enc;
        std::string in;

        explicit RawClient(uint16_t port) {
            c = *net::tcp::connect(sgcl::string("127.0.0.1:" + std::to_string(port)));
            std::string out(h2::Preface, h2::PrefaceSize);
            h2::FrameWriter(out).settings(nullptr, 0);
            send(out);
        }

        void send(const std::string& bytes) {
            ASSERT_TRUE(c.write(slice<const byte>(reinterpret_cast<const byte*>(bytes.data()), bytes.size())));
        }

        void request(uint32_t id, const std::string& method, const std::string& path, bool end_stream, const std::string& length = "",
                     bool expect_continue = false) {
            std::string b;
            enc.encode(b, ":method", method);
            enc.encode(b, ":scheme", "http");
            enc.encode(b, ":authority", "localhost");
            enc.encode(b, ":path", path);
            if (!length.empty()) {
                enc.encode(b, "content-length", length);
            }
            if (expect_continue) {
                enc.encode(b, "expect", "100-continue");
            }
            std::string out;
            h2::FrameWriter(out).headers(id, reinterpret_cast<const uint8_t*>(b.data()), b.size(), end_stream, true);
            send(out);
        }

        void data(uint32_t id, size_t n, bool end_stream) {
            std::string body(n, 'd');
            std::string out;
            h2::FrameWriter(out).data(id, reinterpret_cast<const uint8_t*>(body.data()), body.size(), end_stream);
            send(out);
        }

        // The next frame of the server's matching `want`, within the
        // deadline; nullopt past it or at the connection's end
        struct Got {
            h2::FrameType type;
            uint32_t stream;
            uint32_t code;
            std::string payload;
            uint32_t increment = 0;   // WINDOW_UPDATE's
        };

        optional<Got> next(std::function<bool(const Got&)> want, std::chrono::milliseconds within) {
            auto until = sgcl::clock::now() + within;
            for (;;) {
                while (in.size() >= h2::FrameHeaderSize) {
                    auto f = h2::parse_frame(reinterpret_cast<const uint8_t*>(in.data()), in.size(), h2::LargestMaxFrameSize);
                    if (!f.has_value() || f->size == 0) {
                        break;
                    }
                    Got g{f->frame.type(), f->frame.header.stream, f->frame.error_code,
                          std::string(reinterpret_cast<const char*>(f->frame.payload.data()), f->frame.payload.size()), f->frame.increment};
                    in.erase(0, f->size);
                    if (want(g)) {
                        return g;
                    }
                }
                if (sgcl::clock::now() >= until) {
                    return nullopt;
                }
                c.set_read_deadline(until);
                char buf[16384];
                auto r = c.read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof(buf)));
                if (!r || *r == 0) {
                    return nullopt;
                }
                in.append(buf, *r);
            }
        }
    };

    net::http::server timed() {
        net::http::server s;
        s.h2c = true;
        s.route("GET /hello", [](net::http::request r, net::http::response_writer w) {
            w.write("hello over " + r.proto() + "\n");
        });
        // reads nothing of the body for longer than the read_timeout
        s.route("POST /nobody", [](net::http::request, net::http::response_writer w) -> async::task<> {
            co_await async::after(1500ms);
            w.write("late\n");
        });
        // answers before its body, reading none
        s.route("POST /early", [](net::http::request, net::http::response_writer w) {
            w.write("early\n");
        });
        // the body read whole (read_everything, its content-length known)
        s.route("POST /whole", [](net::http::request r, net::http::response_writer w) -> async::task<> {
            auto body = co_await r.async_bytes();
            w.write(sgcl::string("got " + std::to_string(body ? body->size() : 0) + "\n"));
        });
        // answers later than the write_timeout
        s.route("GET /late", [](net::http::request, net::http::response_writer w) -> async::task<> {
            co_await async::after(1500ms);
            w.write("late\n");
        });
        return s;
    }
}

// read_timeout per stream: a request whose body has not come whole by then
// (its handler reads none, the window stays shut) is reset CANCEL; the
// connection lives and serves the next stream
TEST(H2Server_Tests, StreamReadTimeout) {
    auto s = timed();
    s.read_timeout = 300ms;
    Running r(s, false);
    RawClient c(r.port);
    c.request(1, "POST", "/nobody", false);
    c.data(1, 1000, false);
    auto began = std::chrono::steady_clock::now();
    auto rst = c.next([](auto& g) { return g.type == h2::FrameType::rst_stream && g.stream == 1; }, 3000ms);
    ASSERT_TRUE(rst.has_value());
    EXPECT_EQ(rst->code, uint32_t(h2::ErrorCode::cancel));
    auto took = std::chrono::steady_clock::now() - began;
    EXPECT_GE(took, 250ms);
    EXPECT_LT(took, 1400ms);   // before the handler's own end
    c.request(3, "GET", "/hello", true);
    auto body = c.next([](auto& g) { return g.type == h2::FrameType::data && g.stream == 3; }, 3000ms);
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(body->payload, "hello over HTTP/2.0\n");
}

// write_timeout per stream: a response not whole by then is reset
// INTERNAL_ERROR (Go's onWriteTimeout); the connection lives
TEST(H2Server_Tests, StreamWriteTimeout) {
    auto s = timed();
    s.write_timeout = 300ms;
    Running r(s, false);
    RawClient c(r.port);
    c.request(1, "GET", "/late", true);
    auto rst = c.next([](auto& g) { return g.type == h2::FrameType::rst_stream && g.stream == 1; }, 3000ms);
    ASSERT_TRUE(rst.has_value());
    EXPECT_EQ(rst->code, uint32_t(h2::ErrorCode::internal_error));
    c.request(3, "GET", "/hello", true);
    auto body = c.next([](auto& g) { return g.type == h2::FrameType::data && g.stream == 3; }, 3000ms);
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(body->payload, "hello over HTTP/2.0\n");
}

// A body with its content-length read whole: straight into a vector of its
// size, and known ended with its last byte, so the response ends the
// stream with no RST_STREAM after it (a body left unread would get
// NO_ERROR, §8.1)
TEST(H2Server_Tests, BodyWithLengthReadWhole) {
    Running r(timed(), false);
    RawClient c(r.port);
    c.request(1, "POST", "/whole", false, "5000");
    c.data(1, 3000, false);
    c.data(1, 2000, true);
    auto body = c.next([](auto& g) { return g.type == h2::FrameType::data && g.stream == 1; }, 3000ms);
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(body->payload, "got 5000\n");
    c.request(3, "GET", "/hello", true);
    bool reset = false;
    auto next = c.next([&](auto& g) {
        reset = reset || (g.type == h2::FrameType::rst_stream && g.stream == 1);
        return g.type == h2::FrameType::data && g.stream == 3;
    }, 3000ms);
    ASSERT_TRUE(next.has_value());
    EXPECT_FALSE(reset);
}

// A malformed request's body (DATA past its content-length) or trailers
// with a pseudo-field: the stream's PROTOCOL_ERROR (§8.1.1, §8.1)
TEST(H2Server_Tests, MalformedBodyAndTrailers) {
    Running r(timed(), false);
    RawClient c(r.port);
    c.request(1, "POST", "/whole", false, "10");
    c.data(1, 11, true);
    auto rst = c.next([](auto& g) { return g.type == h2::FrameType::rst_stream && g.stream == 1; }, 3000ms);
    ASSERT_TRUE(rst.has_value());
    EXPECT_EQ(rst->code, uint32_t(h2::ErrorCode::protocol_error));
    c.request(3, "POST", "/whole", false);
    c.data(3, 4, false);
    std::string t;
    c.enc.encode(t, ":path", "/x");
    std::string out;
    h2::FrameWriter(out).headers(3, reinterpret_cast<const uint8_t*>(t.data()), t.size(), true, true);
    c.send(out);
    auto rst3 = c.next([](auto& g) { return g.type == h2::FrameType::rst_stream && g.stream == 3; }, 3000ms);
    ASSERT_TRUE(rst3.has_value());
    EXPECT_EQ(rst3->code, uint32_t(h2::ErrorCode::protocol_error));
}

// Response bodies on both sides of the in-place 64 bytes and of a block
// (ByteChunk::Room, 8176): the same bytes over HTTP/1.1 (our client) and
// HTTP/2 (Go's client, CRC32 of what it got)
TEST(H2Server_Tests, BodySizesAcrossBlocks) {
    Running r(handlers(), false);
    net::http::client c;
    for (size_t n : {size_t(0), size_t(1), size_t(64), size_t(65), size_t(8176), size_t(8177), size_t(16352), size_t(70000)}) {
        SCOPED_TRACE(n);
        std::string body(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            body[i] = char(i % 251);
        }
        auto res = c.post(sgcl::string("http://127.0.0.1:" + std::to_string(r.port) + "/echo"), "application/octet-stream", sgcl::string(body));
        ASSERT_TRUE(res) << std::string(res.error().message().view());
        EXPECT_EQ(res->proto(), "HTTP/1.1");
        EXPECT_EQ(std::string(res->text()->view()), body);
        if (!oracle().empty()) {
            auto out = r.go("-method POST -path /echo -body " + std::to_string(n), false);
            auto sent = lines_with(out, "SENT ");
            auto resp = lines_with(out, "RESP 0 ");
            ASSERT_EQ(resp.size(), 1u) << out;
            EXPECT_EQ(field(resp[0], "bytes"), std::to_string(n)) << out;
            if (!sent.empty()) {
                EXPECT_EQ(field(resp[0], "crc32"), field(sent[0], "crc32")) << out;
            }
        }
    }
}

// shutdown() with an HTTP/2 connection kept idle by a client (its streams
// all done, the client holding the connection for the next request):
// closed as an idle connection of HTTP/1.1 is, shutdown returns at once
TEST(H2Server_Tests, ShutdownClosesAnIdleH2Connection) {
    for (int round = 0; round < 3; ++round) {
        net::http::server s = handlers();
        net::listener l = *net::tcp::listen("127.0.0.1:0");
        auto serving = async::spawn(s.async_serve(l));
        net::http::client c;
        c.h2c = true;
        auto res = c.get(sgcl::string("http://127.0.0.1:" + std::to_string(l.local_endpoint().port()) + "/hello"));
        ASSERT_TRUE(res) << std::string(res.error().message().view());
        EXPECT_EQ(res->proto(), "HTTP/2.0");
        EXPECT_EQ(*res->text(), "hello over HTTP/2.0\n");
        std::this_thread::sleep_for(50ms);   // the handler released: the connection idle
        auto began = std::chrono::steady_clock::now();
        auto done = async::with_timeout(s.async_shutdown(), 3s).wait();
        EXPECT_TRUE(done) << "shutdown waited on an idle HTTP/2 connection";
        EXPECT_LT(std::chrono::steady_clock::now() - began, 2s);
        s.close();
        (void)serving.wait();
    }
}

// The connection's window given back for bodies taken (consumed, and for a
// stream that has ended consumed_ended, gathered without the connection's
// lock): 600 requests of 1000 bytes one after another, each read whole by
// its handler; past half the connection's window (1 MB announced) one
// WINDOW_UPDATE on stream 0 with what was taken by then, 524 288 bytes and
// at most one body more; nothing on the streams (each had ended)
TEST(H2Server_Tests, ConnectionWindowGivenBack) {
    Running r(timed(), false);
    RawClient c(r.port);
    std::vector<uint32_t> updates;
    size_t stream_updates = 0;
    for (uint32_t i = 0; i < 600; ++i) {
        const uint32_t id = 1 + 2 * i;
        c.request(id, "POST", "/whole", false, "1000");
        c.data(id, 1000, true);
        auto body = c.next([&](auto& g) {
            if (g.type == h2::FrameType::window_update) {
                if (g.stream == 0) {
                    updates.push_back(g.increment);
                } else {
                    ++stream_updates;
                }
            }
            return g.type == h2::FrameType::data && g.stream == id;
        }, 3000ms);
        ASSERT_TRUE(body.has_value()) << i;
        ASSERT_EQ(body->payload, "got 1000\n");
    }
    ASSERT_EQ(updates.size(), 2u);   // the preface's, then one past half the window
    EXPECT_EQ(updates[0], (1u << 20) - 65535u);
    EXPECT_GE(updates[1], 524288u);
    EXPECT_LT(updates[1], 524288u + 1000u);
    EXPECT_EQ(stream_updates, 0u);
}

// The order of the frames when handlers write their responses themselves
// and the pump writes the rest: 200 requests at once on one connection,
// their responses read back in the order they came, every field block
// decoded by one HPACK decoder (a block out of the encoder's order would
// fail it), every stream's HEADERS before its DATA, each stream ended once
TEST(H2Server_Tests, FramesInOrderAcrossStreams) {
    Running r(timed(), false);
    RawClient c(r.port);
    constexpr uint32_t N = 200;
    for (uint32_t i = 0; i < N; ++i) {
        const uint32_t id = 1 + 2 * i;
        if (i % 2) {
            c.request(id, "GET", "/hello", true);
        } else {
            c.request(id, "POST", "/whole", false, "3000");
            c.data(id, 3000, true);
        }
    }
    h2::Decoder decoder;
    std::vector<int> headed(2 * N + 2, 0), ended(2 * N + 2, 0);
    uint32_t done = 0;
    bool broken = false;
    std::string blocks;
    while (done < N && !broken) {
        auto f = c.next([](auto& g) { return g.type == h2::FrameType::headers || g.type == h2::FrameType::data || g.type == h2::FrameType::continuation; }, 5000ms);
        if (!f) {
            break;
        }
        ASSERT_LT(f->stream, 2 * N + 2);
        if (f->type == h2::FrameType::headers) {
            auto block = decoder.decode(slice<const byte>(reinterpret_cast<const byte*>(f->payload.data()), f->payload.size()), 1 << 20);
            ASSERT_TRUE(block.has_value()) << "stream " << f->stream;
            EXPECT_EQ(std::string(block->fields.get(":status").view()), "200");
            ++headed[f->stream];
        } else if (f->type == h2::FrameType::data) {
            if (!headed[f->stream]) {
                broken = true;   // DATA before its HEADERS
            }
            if (f->payload.size() == 0 || f->payload.back() == '\n') {
                ++ended[f->stream];
                ++done;
            }
        }
    }
    EXPECT_FALSE(broken);
    EXPECT_EQ(done, N);
    for (uint32_t i = 0; i < N; ++i) {
        EXPECT_EQ(headed[1 + 2 * i], 1) << i;
        EXPECT_EQ(ended[1 + 2 * i], 1) << i;
    }
}

// When a handler starts (SmallBody): a request with a small declared body
// has its handler started with the body's first bytes, not its HEADERS; one
// with END_STREAM on its HEADERS, or asking Expect: 100-continue, or with a
// body past 16 KB, at once with its HEADERS
TEST(H2Server_Tests, HandlerStartsWithTheBodyOfASmallRequest) {
    Running r(timed(), false);
    RawClient c(r.port);
    auto response = [&](uint32_t id, std::chrono::milliseconds within) {
        return c.next([id](auto& g) { return g.type == h2::FrameType::data && g.stream == id; }, within);
    };
    // END_STREAM on the HEADERS: at once
    c.request(1, "POST", "/early", true, "0");
    auto r1 = response(1, 3000ms);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->payload, "early\n");
    // a small body, none of it yet: the handler waits for it
    c.request(3, "POST", "/early", false, "10");
    EXPECT_FALSE(response(3, 300ms).has_value());
    c.data(3, 10, true);
    auto r3 = response(3, 3000ms);
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(r3->payload, "early\n");
    // Expect: 100-continue: at once with the HEADERS, before any body
    c.request(5, "POST", "/early", false, "10", true);
    auto r5 = response(5, 3000ms);
    ASSERT_TRUE(r5.has_value());
    EXPECT_EQ(r5->payload, "early\n");
    // a body past 16 KB: at once with the HEADERS
    c.request(7, "POST", "/early", false, "20000");
    auto r7 = response(7, 3000ms);
    ASSERT_TRUE(r7.has_value());
    // no content-length: at once
    c.request(9, "POST", "/early", false);
    auto r9 = response(9, 3000ms);
    ASSERT_TRUE(r9.has_value());
    // a deferred request reset by the client before its body: its place
    // comes back (the next request is served)
    c.request(11, "POST", "/early", false, "10");
    std::string rst;
    h2::FrameWriter(rst).rst_stream(11, h2::ErrorCode::cancel);
    c.send(rst);
    c.request(13, "GET", "/hello", true);
    auto r13 = response(13, 3000ms);
    ASSERT_TRUE(r13.has_value());
    EXPECT_EQ(r13->payload, "hello over HTTP/2.0\n");
}

// Deferred handlers do not keep places: max_concurrent_streams requests
// with small bodies, reset one by one before their bodies, then as many
// served; the server's handlers never wait on the ones that never started
TEST(H2Server_Tests, DeferredHandlersGiveTheirPlacesBack) {
    auto s = timed();
    s.max_concurrent_streams = 4;
    Running r(s, false);
    RawClient c(r.port);
    std::string out;
    for (uint32_t i = 0; i < 20; ++i) {
        const uint32_t id = 1 + 2 * i;
        c.request(id, "POST", "/early", false, "10");
        out.clear();
        h2::FrameWriter(out).rst_stream(id, h2::ErrorCode::cancel);
        c.send(out);
    }
    for (uint32_t i = 20; i < 28; ++i) {
        const uint32_t id = 1 + 2 * i;
        c.request(id, "POST", "/early", false, "10");
        c.data(id, 10, true);
        auto got = c.next([id](auto& g) { return g.type == h2::FrameType::data && g.stream == id; }, 3000ms);
        ASSERT_TRUE(got.has_value()) << i;
    }
}

// A response's body blocks go out in place (the connection's write takes
// them where they lie, no copy into its output) and back to the worker's
// pool only once that write is done: many streams at once on one
// connection, each body its own bytes, across blocks and frames, the pool
// taking and giving all the time; every body whole and its own, over h2c
// and TLS
TEST(H2Server_Tests, BodiesSentInPlaceStayTheirOwn) {
    auto body_of = [](size_t seed, size_t n) {
        std::string s(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            s[i] = char((seed * 131 + i * (seed % 7 + 1)) % 251);
        }
        return s;
    };
    auto server = [body_of] {   // one a run: a server closed is not served again
        net::http::server s;
        s.h2c = true;
        s.route("GET /pattern", [body_of](net::http::request r, net::http::response_writer w) {
            const size_t seed = size_t(std::atol(std::string(r.query("seed").view()).c_str()));
            const size_t n = 5000 + seed * 977 % 60000;
            std::string b = body_of(seed, n);
            const size_t third = n / 3;
            w.write(sgcl::string(b.substr(0, third)));
            w.write(sgcl::string(b.substr(third, third)));
            w.write(sgcl::string(b.substr(2 * third)));
        });
        return s;
    };
    for (bool secure : {false, true}) {
        SCOPED_TRACE(secure ? "tls" : "h2c");
        Running r(server(), secure);
        net::http::client c;
        c.timeout = 20s;
        if (secure) {
            c.tls.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
            c.tls.server_name = sgcl::string("localhost");
        } else {
            c.h2c = true;
        }
        const std::string base = (secure ? "https://127.0.0.1:" : "http://127.0.0.1:") + std::to_string(r.port) + "/pattern?seed=";
        for (size_t round = 0; round < 4; ++round) {
            std::vector<async::task<expected<net::http::response, io::error>>> tasks;
            for (size_t k = 0; k < 64; ++k) {
                tasks.push_back(c.async_get(sgcl::string(base + std::to_string(round * 64 + k))));
                tasks.back().spawn();
            }
            vector<expected<net::http::response, io::error>> results;
            for (auto& t : tasks) {
                results.push_back(t.wait());   // every one waited before any check returns
            }
            for (size_t k = 0; k < results.size(); ++k) {
                auto& res = results[k];
                ASSERT_TRUE(res) << std::string(res.error().message().view());
                EXPECT_EQ(res->proto(), "HTTP/2.0");
                const size_t seed = round * 64 + k;
                auto text = res->text();
                ASSERT_TRUE(text);
                const std::string want = body_of(seed, 5000 + seed * 977 % 60000);
                const std::string got(text->view());
                ASSERT_EQ(got.size(), want.size()) << seed;
                EXPECT_TRUE(got == want) << "stream of seed " << seed << ": another body's bytes";
            }
        }
    }
}

// DESIGN 408: a writer kept past the end of its stream's response: its
// writes go nowhere, a flush is io::errc::closed, and the connection's
// next stream is answered whole
TEST(H2Server_Tests, AWriterUsedAfterTheResponseEnded) {
    async::event go, done;
    static std::atomic<int> flushed{-1};
    net::http::server s;
    s.h2c = true;
    s.route("/first", [go, done](net::http::request, net::http::response_writer w) {
        w.write("first");
        async::go([](net::http::response_writer w, async::event go, async::event done) -> async::task<> {
            co_await go;
            w.set_header("X-Late", "1");
            w.write("late");
            w.error(503, "late");
            auto f = co_await w.async_flush();
            flushed = f ? 0 : int(f.error().code() == io::errc::closed);
            done.set();
        }(w, go, done));
    });
    s.route("/second", [](net::http::request, net::http::response_writer w) { w.write("second"); });
    Running r(s, false);
    net::http::client c;
    c.h2c = true;
    const std::string base = "http://127.0.0.1:" + std::to_string(r.port);
    auto first = c.get(sgcl::string(base + "/first"));
    ASSERT_TRUE(first);
    EXPECT_EQ(first->proto(), "HTTP/2.0");
    EXPECT_EQ(*first->text(), "first");
    go.set();
    done.wait();
    EXPECT_EQ(flushed.load(), 1);
    auto second = c.get(sgcl::string(base + "/second"));
    ASSERT_TRUE(second) << std::string(second.error().message().view());
    EXPECT_EQ(second->status(), 200);
    EXPECT_EQ(second->header("x-late"), "");
    EXPECT_EQ(*second->text(), "second");
}

// DESIGN 408: a file of the body that cannot be read (a directory), with
// nothing sent yet: 500 on the stream, as over HTTP/1.1
TEST(H2Server_Tests, AFileThatCannotBeReadIs500) {
    net::http::server s;
    s.h2c = true;
    static std::atomic<int> reported{0};
    s.on_error = [](const sgcl::string& line) { reported += line.view().find("wrote a file that failed") != std::string_view::npos; };
    const std::string dir = std::filesystem::temp_directory_path().string();
    s.route("/dir", [dir](net::http::request, net::http::response_writer w) {
        auto f = io::open(sgcl::string(dir));
        if (f) {
            w.write(*f);
        }
    });
    Running r(s, false);
    net::http::client c;
    c.h2c = true;
    auto res = c.get(sgcl::string("http://127.0.0.1:" + std::to_string(r.port) + "/dir"));
    ASSERT_TRUE(res) << std::string(res.error().message().view());
    EXPECT_EQ(res->proto(), "HTTP/2.0");
    EXPECT_EQ(res->status(), 500);
    EXPECT_EQ(*res->text(), "Internal Server Error\n");
    EXPECT_EQ(reported.load(), 1);
}

// DESIGN 408: a stream reset by the client in the middle of its body: the
// handler's read gives connection_reset, the connection serves the next
// stream. A DATA frame of exactly SETTINGS_MAX_FRAME_SIZE (2^14) is read;
// one byte more is the connection's FRAME_SIZE_ERROR
TEST(H2Server_Tests, AResetMidBodyAndFramesAtTheMaxSize) {
    static std::atomic<int> read_result{-1};
    static std::atomic<bool> reading{false};
    auto s = timed();
    s.route("POST /read", [](net::http::request r, net::http::response_writer w) -> async::task<> {
        reading = true;
        auto body = co_await r.async_bytes();
        read_result = body ? 0 : int(body.error().code() == std::errc::connection_reset);
        w.write("read\n");
    });
    Running r(s, false);
    RawClient c(r.port);
    c.request(1, "POST", "/read", false);
    c.data(1, 100, false);
    for (int i = 0; i < 300 && !reading.load(); ++i) {   // the handler reads: the reset comes in the middle
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_TRUE(reading.load());
    std::this_thread::sleep_for(20ms);
    std::string rst;
    h2::FrameWriter(rst).rst_stream(1, h2::ErrorCode::cancel);
    c.send(rst);
    for (int i = 0; i < 300 && read_result.load() < 0; ++i) {
        std::this_thread::sleep_for(10ms);
    }
    EXPECT_EQ(read_result.load(), 1);
    c.request(3, "POST", "/whole", false, "16384");
    c.data(3, h2::DefaultMaxFrameSize, true);
    auto whole = c.next([](auto& g) { return g.type == h2::FrameType::data && g.stream == 3; }, 3000ms);
    ASSERT_TRUE(whole.has_value());
    EXPECT_EQ(whole->payload, "got 16384\n");
    c.request(5, "POST", "/whole", false);
    c.data(5, h2::DefaultMaxFrameSize + 1, true);
    auto away = c.next([](auto& g) { return g.type == h2::FrameType::goaway; }, 3000ms);
    ASSERT_TRUE(away.has_value());
    EXPECT_EQ(away->code, uint32_t(h2::ErrorCode::frame_size_error));
}

// DESIGN 408: max_concurrent_streams of 0 is taken as 1 (a server that
// took no stream would answer nothing): streams in turn, each answered
TEST(H2Server_Tests, MaxConcurrentStreamsZeroIsOne) {
    auto s = handlers();
    s.max_concurrent_streams = 0;
    Running r(s, false);
    net::http::client c;
    c.h2c = true;
    for (int i = 0; i < 3; ++i) {
        auto res = c.get(sgcl::string("http://127.0.0.1:" + std::to_string(r.port) + "/hello"));
        ASSERT_TRUE(res) << std::string(res.error().message().view());
        EXPECT_EQ(*res->text(), "hello over HTTP/2.0\n");
    }
}

namespace {
    // A stream that gives its text and then fails
    class FailingBody final : public io::mixin::reader<FailingBody> {
    public:
        explicit FailingBody(std::string s) : _text(std::move(s)), _s(_text) {}

        expected<size_t, io::error> read(slice<byte> out) {
            if (_s.empty()) {
                return unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "failing"));
            }
            size_t k = std::min(out.size(), _s.size());
            std::memcpy(out.data(), _s.data(), k);
            _s.remove_prefix(k);
            return k;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

    private:
        std::string _text;   // its own: read after the expression that made it
        std::string_view _s;
    };
}

// DESIGN 408: a request body that fails half-way over HTTP/2: the send is
// the stream's error, the stream is reset, and the connection goes on with
// the next request
TEST(H2Server_Tests, ARequestBodyThatFailsHalfWay) {
    Running r(handlers(), false);
    net::http::client c;
    c.h2c = true;
    const std::string base = "http://127.0.0.1:" + std::to_string(r.port);
    net::http::request post("POST", sgcl::string(base + "/echo"));
    post.set_body(io::reader(make_tracked<FailingBody>(std::string(40000, 'x'))));
    auto failed = c.send(post);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code(), std::errc::io_error) << std::string(failed.error().message().view());
    net::http::request shorter("POST", sgcl::string(base + "/echo"));
    shorter.set_body(io::reader(make_tracked<io::buffer>("abc")), 10);   // ends before its length
    auto cut = c.send(shorter);
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), io::errc::unexpected_eof) << std::string(cut.error().message().view());
    auto next = c.post(sgcl::string(base + "/echo"), "text/plain", "after");
    ASSERT_TRUE(next) << std::string(next.error().message().view());
    EXPECT_EQ(next->proto(), "HTTP/2.0");
    EXPECT_EQ(*next->text(), "after");
}
