//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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

        void request(uint32_t id, const std::string& method, const std::string& path, bool end_stream, const std::string& length = "") {
            std::string b;
            enc.encode(b, ":method", method);
            enc.encode(b, ":scheme", "http");
            enc.encode(b, ":authority", "localhost");
            enc.encode(b, ":path", path);
            if (!length.empty()) {
                enc.encode(b, "content-length", length);
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
                          std::string(reinterpret_cast<const char*>(f->frame.payload.data()), f->frame.payload.size())};
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
