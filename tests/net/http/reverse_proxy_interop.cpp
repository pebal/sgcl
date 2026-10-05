//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: reverse_proxy against real peers on the loopback: the proxy in
// front of Go's net/http (go_reverse_proxy/main.go, built here with `go
// build`; skipped where there is no go), Go's client through the proxy, and
// curl through it (HTTP/1.1, h2c by prior knowledge, a stream of events).
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // The helper built once; "" when there is no go to build it with
    const std::string& helper() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto src = source_root() / "tests/net/http/go_reverse_proxy/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_http_go_reverse_proxy";
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

    // Go's backend, running until the object goes
    struct GoBackend {
        FILE* p = nullptr;
        std::string base;

        GoBackend() {
            p = popen(("'" + helper() + "' backend").c_str(), "r");
            char line[64] = {};
            if (p && fgets(line, sizeof line, p)) {
                base = "http://127.0.0.1:" + std::to_string(std::atoi(line + 5));
            }
        }

        ~GoBackend() {
            net::http::client c;
            c.proxy = net::http::proxy();
            (void)c.get(sgcl::string(base + "/quit"));
            if (p) {
                pclose(p);
            }
        }
    };

    net::http::server front(const net::http::reverse_proxy& p) {
        net::http::server s;
        s.route("/", p);
        s.max_body_bytes = 0;
        return s;
    }

    std::string talk(const net::endpoint& at, const std::string& bytes) {
        auto c = *net::tcp::connect(at);
        c.set_deadline(sgcl::clock::now() + 5s);
        (void)c.write(sgcl::string(bytes));
        std::string out;
        char buf[4096];
        for (;;) {
            auto n = c.read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
            if (!n || *n == 0) {
                break;
            }
            out.append(buf, *n);
        }
        return out;
    }
}

TEST(HttpReverseProxyInterop_Tests, InFrontOfGo) {
    if (helper().empty()) {
        GTEST_SKIP() << "no go to build the helper with";
    }
    GoBackend go;
    ASSERT_FALSE(go.base.empty());
    net::http::reverse_proxy::options o;
    o.via = "sgcl";
    net::http::test_server proxy(front(net::http::reverse_proxy(sgcl::string(go.base), o)));
    auto c = proxy.client();
    net::http::request r("POST", proxy.url() + "/echo?x=1");
    r.set_header("X-Custom", "v");
    r.set_body("12345");
    auto echo = c.send(r);
    ASSERT_TRUE(echo) << text(echo.error().message());
    auto body = text(*echo->text());
    EXPECT_TRUE(body.starts_with("POST /echo?x=1 HTTP/1.1\n")) << body;
    EXPECT_NE(body.find("Host: " + go.base.substr(7) + "\n"), std::string::npos) << body;
    EXPECT_NE(body.find("X-Custom: v\n"), std::string::npos) << body;
    EXPECT_NE(body.find("X-Forwarded-For: 127.0.0.1\n"), std::string::npos) << body;
    EXPECT_NE(body.find("Via: 1.1 sgcl\n"), std::string::npos) << body;
    EXPECT_NE(body.find("body 5 "), std::string::npos) << body;

    auto stream = c.get(proxy.url() + "/stream");
    ASSERT_TRUE(stream);
    EXPECT_EQ(text(*stream->text()), "part 0\npart 1\npart 2\n");

    auto trailer = c.get(proxy.url() + "/trailer");
    ASSERT_TRUE(trailer);
    EXPECT_EQ(text(*trailer->text()), "body");
    EXPECT_EQ(trailer->trailers().get("X-Sum"), "4");

    auto big = c.get(proxy.url() + "/big");
    ASSERT_TRUE(big);
    EXPECT_EQ(big->content_length(), optional<uint64_t>(8 << 20));
    EXPECT_EQ(big->bytes()->size(), size_t(8 << 20));

    auto sse = c.get(proxy.url() + "/sse");
    ASSERT_TRUE(sse);
    net::http::event_reader events(sse->body());
    auto e0 = events.next();
    ASSERT_TRUE(e0 && *e0);
    EXPECT_EQ((*e0)->data, "event 0");

    auto hints = talk(proxy.endpoint(), "GET /hints HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_TRUE(hints.starts_with("HTTP/1.1 103 Early Hints\r\nLink: </style.css>; rel=preload; as=style\r\n\r\nHTTP/1.1 200 OK\r\n")) << hints;
    EXPECT_TRUE(hints.ends_with("after hints")) << hints;

    std::string up(3 << 20, 'g');
    net::http::request put("PUT", proxy.url() + "/echo");
    put.set_body(io::reader(make_tracked<io::buffer>(sgcl::string(up))));
    auto streamed = c.send(put);
    ASSERT_TRUE(streamed);
    EXPECT_NE(text(*streamed->text()).find("body 3145728 "), std::string::npos);
}

TEST(HttpReverseProxyInterop_Tests, GoClientThroughTheProxy) {
    if (helper().empty()) {
        GTEST_SKIP() << "no go to build the helper with";
    }
    GoBackend go;
    ASSERT_FALSE(go.base.empty());
    net::http::test_server proxy(front(net::http::reverse_proxy(sgcl::string(go.base))));
    auto out = run("'" + helper() + "' client '" + text(proxy.url()) + "'");
    for (auto name : {"get", "post chunked=false", "post chunked=true", "stream", "trailer", "big", "hints"}) {
        EXPECT_NE(out.find(std::string("ok ") + name + "\n"), std::string::npos) << name << "\n" << out;
    }
    EXPECT_EQ(out.find("fail"), std::string::npos) << out;
}

TEST(HttpReverseProxyInterop_Tests, Curl) {
    if (std::system("command -v curl > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no curl";
    }
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) -> async::task<> {
        if (r.url().path() == "/events") {
            net::http::event_stream events(w);
            (void)co_await events.async_send(net::http::event{.data = "first"});
            (void)co_await events.async_send(net::http::event{.data = "second"});
            co_return;
        }
        auto body = co_await r.async_text();
        w.set_header("X-Proto-Seen", r.proto());
        w.write(string::concat(r.method(), " ", r.url().request_target(), " ", *body, " xff=", r.header("X-Forwarded-For")));
    });
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())), {.http2 = true});
    auto url = text(proxy.url());
    auto get = run("curl -s -i '" + url + "/a?b=c'");
    EXPECT_TRUE(get.starts_with("HTTP/1.1 200 OK\r\n")) << get;
    EXPECT_NE(get.find("GET /a?b=c  xff=127.0.0.1"), std::string::npos) << get;
    auto post = run("curl -s --data-binary 'payload' '" + url + "/p'");
    EXPECT_EQ(post, "POST /p payload xff=127.0.0.1");
    auto h2 = run("curl -s -i --http2-prior-knowledge '" + url + "/h2'");
    if (h2.find("HTTP/2 200") != std::string::npos) {   // a curl built with HTTP/2
        EXPECT_NE(h2.find("x-proto-seen: HTTP/1.1"), std::string::npos) << h2;
        EXPECT_NE(h2.find("GET /h2  xff=127.0.0.1"), std::string::npos) << h2;
    }
    auto events = run("curl -s -N '" + url + "/events'");
    EXPECT_EQ(events, "data: first\n\ndata: second\n\n");
    auto head = run("curl -s -I '" + url + "/h'");
    EXPECT_NE(head.find("Content-Length: "), std::string::npos) << head;
}
