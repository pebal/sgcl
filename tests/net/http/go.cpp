//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: Go's net/http as the other side, on the loopback (go_peer/main.go,
// built here with `go build`; the tests are skipped where there is no go):
// the module's client against Go's server (a body, a stream, trailers, a
// chain of redirects, HEAD, a cookie, Connection: close, the pool by the
// server's count of connections) and Go's client against the module's
// server (keep-alive, 1 MB with a length and chunked, 100-continue, HEAD,
// 404, a stream, a redirect, 800 requests from 32 goroutines).
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace sgcl;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // The peer built once; "" when there is no go to build it with
    const std::string& peer() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto src = std::filesystem::path(__FILE__).parent_path() / "go_peer" / "main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_http_go_peer";
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
}

TEST(HttpGo_Tests, OurClientAgainstGosServer) {
    if (peer().empty()) {
        GTEST_SKIP() << "no go to build the peer with";
    }
    FILE* p = popen(("'" + peer() + "' server").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof(line), p));
    int port = std::atoi(line + 5);
    ASSERT_GT(port, 0);
    std::string base = "http://127.0.0.1:" + std::to_string(port);
    auto at = [&](const std::string& path) { return sgcl::string(base + path); };
    net::http::client c;

    auto hello = c.get(at("/hello"));
    ASSERT_TRUE(hello) << text(hello.error().message());
    EXPECT_EQ(hello->status(), 200);
    EXPECT_EQ(hello->header("Content-Type"), "text/plain");
    EXPECT_EQ(*hello->text(), "hello from go\n");
    auto date = hello->headers().date("Date");                  // Go's Date read as an instant
    ASSERT_TRUE(date);
    EXPECT_LE(std::abs(date->unix() - time::now().unix()), 5);

    auto stream = c.get(at("/stream"));
    ASSERT_TRUE(stream);
    EXPECT_EQ(stream->header("Transfer-Encoding"), "chunked");
    EXPECT_EQ(*stream->text(), "part 0\npart 1\npart 2\n");

    auto trailer = c.get(at("/trailer"));
    ASSERT_TRUE(trailer);
    EXPECT_EQ(*trailer->text(), "body");
    EXPECT_EQ(trailer->trailers().get("X-Sum"), "4");

    std::string big;
    for (int i : range(1 << 16)) {
        big += "0123456789abcdef";
        (void)i;
    }
    auto echo = c.post(at("/echo"), "application/octet-stream", sgcl::string(big));
    ASSERT_TRUE(echo);
    EXPECT_EQ(echo->header("X-Length"), "1048576");
    EXPECT_EQ(echo->header("X-Chunked"), "false");
    EXPECT_EQ(text(*echo->text()), big);

    net::http::request streamed("PUT", at("/echo"));
    streamed.set_body(io::reader(make_tracked<io::buffer>(sgcl::string(big))));
    auto chunked = c.send(streamed);
    ASSERT_TRUE(chunked);
    EXPECT_EQ(chunked->header("X-Chunked"), "true");
    EXPECT_EQ(chunked->header("X-Method"), "PUT");
    EXPECT_EQ(text(*chunked->text()), big);

    auto redirected = c.get(at("/redirect/4"));
    ASSERT_TRUE(redirected);
    EXPECT_EQ(redirected->status(), 200);
    EXPECT_EQ(redirected->url().path(), "/hello");
    EXPECT_EQ(*redirected->text(), "hello from go\n");
    c.max_redirects = 2;
    auto too_many = c.get(at("/redirect/4"));
    ASSERT_FALSE(too_many);
    EXPECT_EQ(too_many.error().code(), net::errc::too_many_redirects);
    c.max_redirects = 10;

    auto head = c.head(at("/head"));
    ASSERT_TRUE(head);
    EXPECT_EQ(head->content_length(), 1234u);
    EXPECT_EQ(*head->text(), "");

    auto cookie = c.get(at("/cookie"));
    ASSERT_TRUE(cookie);
    auto parsed = net::http::cookie::parse(cookie->header("Set-Cookie"));
    ASSERT_TRUE(parsed) << text(cookie->header("Set-Cookie"));
    EXPECT_EQ(parsed->name, "id");
    EXPECT_EQ(parsed->value, "a b");
    EXPECT_EQ(parsed->path, "/");
    EXPECT_TRUE(parsed->http_only);
    EXPECT_EQ(parsed->same_site, "Lax");
    ASSERT_TRUE(parsed->max_age);
    EXPECT_EQ(parsed->max_age->nanoseconds(), 60'000'000'000);
    (void)cookie->text();

    // every request so far on one connection, but the one Go closed
    auto closing = c.get(at("/close"));
    ASSERT_TRUE(closing);
    EXPECT_EQ(*closing->text(), "bye");
    auto conns = c.get(at("/conns"));
    ASSERT_TRUE(conns);
    EXPECT_EQ(*conns->text(), "2");

    (void)c.get(at("/quit"));
    pclose(p);
}

TEST(HttpGo_Tests, GosClientAgainstOurServer) {
    if (peer().empty()) {
        GTEST_SKIP() << "no go to build the peer with";
    }
    net::http::server s;
    s.route("GET /hello", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("hello from sgcl\n");
    });
    s.route("/echo", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto b = co_await req.async_bytes();
        if (!b) {
            w.error(net::http::status::bad_request);
            co_return;
        }
        w.write(slice<const byte>(*b));
    });
    s.route("GET /stream", [](net::http::request, net::http::response_writer w) -> async::task<> {
        for (int i : range(3)) {
            w.write("part " + to_string(i) + "\n");
            (void)co_await w.async_flush();
        }
    });
    s.route("/go-away", [](net::http::request, net::http::response_writer w) { w.redirect("/hello"); });
    s.max_body_bytes = 4 << 20;
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(s.async_serve(l));
    auto out = run("'" + peer() + "' client http://127.0.0.1:" + std::to_string(l.local_endpoint().port()));
    s.close();
    (void)serving.wait();
    const std::string expected =
        "hello: 200 \"hello from sgcl\\n\" text/plain date=true\n"
        "hello: 200 \"hello from sgcl\\n\" text/plain date=true\n"
        "hello: 200 \"hello from sgcl\\n\" text/plain date=true\n"
        "reuse: fresh=1 reused=2\n"
        "echo: 200 chunked=false same=true\n"
        "echo: 200 chunked=true same=true\n"
        "continue: 200 \"continued\"\n"
        "head: 200 16 \"\"\n"
        "missing: 404 \"Not Found\\n\"\n"
        "stream: 200 [chunked] \"part 0\\npart 1\\npart 2\\n\"\n"
        "redirect: 200 \"hello from sgcl\\n\" /hello\n"
        "parallel: wrong=0\n";
    EXPECT_EQ(out, expected);
}
