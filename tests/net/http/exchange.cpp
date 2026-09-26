//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: the server and the client of the module talking to each other over
// the loopback, and each of them to raw bytes over a connection: routes,
// methods, bodies of every framing, keep-alive and the pool, pipelining,
// 100-continue, the limits, redirects, errors, shutdown and close.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <cstdlib>
#include <chrono>
#include <string>
#include <thread>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // A server on a port of the loopback, serving on the scheduler
    struct Running {
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        std::string base;

        explicit Running(net::http::server s)
        : server(s) {
            listener = *net::tcp::listen("127.0.0.1:0");
            base = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port());
            serving = async::spawn(server.async_serve(listener));
        }

        ~Running() {
            server.close();
            (void)serving.wait();
        }
    };

    // Raw bytes to the server and everything it answers until it closes
    std::string raw(const Running& r, const std::string& bytes, bool half_close = true) {
        auto c = *net::tcp::connect(r.listener.local_endpoint());
        (void)c.write(sgcl::string(bytes));
        if (half_close) {
            (void)c.close_write();
        }
        c.set_read_deadline(sgcl::clock::now() + 5s);
        auto all = c.read_all_text();
        (void)c.close();
        return all ? text(*all) : std::string("<error>");
    }

    net::http::server basic() {
        net::http::server s;
        s.route("GET /hello", [](net::http::request, net::http::response_writer w) {
            w.set_header("Content-Type", "text/plain");
            w.write("hello\n");
        });
        s.route("GET /users/{id}", [](net::http::request req, net::http::response_writer w) {
            w.write("user " + req.path_value("id"));
        });
        s.route("POST /echo", [](net::http::request req, net::http::response_writer w) -> async::task<> {
            auto body = co_await req.async_text();
            if (!body) {
                w.error(net::http::status::bad_request);
                co_return;
            }
            w.write(*body);
        });
        s.route("/files/", [](net::http::request req, net::http::response_writer w) {
            w.write("files " + req.url().path());
        });
        s.route("GET /stream", [](net::http::request, net::http::response_writer w) -> async::task<> {
            for (int i : range(3)) {
                w.write("part " + to_string(i) + "\n");
                (void)co_await w.async_flush();
            }
        });
        s.route("GET /throw", [](net::http::request, net::http::response_writer) {
            throw std::runtime_error("boom");
        });
        return s;
    }
}

TEST(Http_Tests, GetThroughTheClient) {
    Running r(basic());
    net::http::client c;
    auto res = c.get(sgcl::string(r.base + "/hello"));
    ASSERT_TRUE(res) << res.error().message();
    EXPECT_EQ(res->status(), 200);
    EXPECT_TRUE(res->ok());
    EXPECT_EQ(res->header("content-type"), "text/plain");
    EXPECT_EQ(res->content_length(), 6u);
    auto date = res->headers().date("Date");                    // every response carries one
    ASSERT_TRUE(date);
    EXPECT_LE(std::abs(date->unix() - time::now().unix()), 5);
    auto body = res->text();
    ASSERT_TRUE(body);
    EXPECT_EQ(*body, "hello\n");

    auto user = c.get(sgcl::string(r.base + "/users/42"));
    ASSERT_TRUE(user);
    EXPECT_EQ(*user->text(), "user 42");

    auto missing = c.get(sgcl::string(r.base + "/nothing"));
    ASSERT_TRUE(missing);
    EXPECT_EQ(missing->status(), 404);
    EXPECT_FALSE(missing->ok());
    EXPECT_EQ(*missing->text(), "Not Found\n");

    auto post = c.post(sgcl::string(r.base + "/echo"), "text/plain", "ping");
    ASSERT_TRUE(post);
    EXPECT_EQ(*post->text(), "ping");

    auto wrong = c.post(sgcl::string(r.base + "/hello"), "text/plain", "x");
    ASSERT_TRUE(wrong);
    EXPECT_EQ(wrong->status(), 405);
    EXPECT_EQ(wrong->header("Allow"), "GET, HEAD");
    (void)wrong->text();

    auto stream = c.get(sgcl::string(r.base + "/stream"));
    ASSERT_TRUE(stream);
    EXPECT_EQ(stream->header("Transfer-Encoding"), "chunked");
    EXPECT_EQ(*stream->text(), "part 0\npart 1\npart 2\n");

    auto boom = c.get(sgcl::string(r.base + "/throw"));
    ASSERT_TRUE(boom);
    EXPECT_EQ(boom->status(), 500);
    (void)boom->text();

    auto head = c.head(sgcl::string(r.base + "/hello"));
    ASSERT_TRUE(head);
    EXPECT_EQ(head->status(), 200);
    EXPECT_EQ(head->content_length(), 6u);
    EXPECT_EQ(*head->text(), "");
}

TEST(Http_Tests, RawRequests) {
    Running r(basic());
    auto a = raw(r, "GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(a.find("HTTP/1.1 200 OK\r\n"), std::string::npos) << a;
    EXPECT_NE(a.find("Content-Length: 6\r\n"), std::string::npos) << a;
    EXPECT_NE(a.find("\r\n\r\nhello\n"), std::string::npos) << a;

    // pipelined, the second closing
    auto b = raw(r, "GET /hello HTTP/1.1\r\nHost: x\r\n\r\nGET /users/7 HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(b.find("hello\n"), std::string::npos) << b;
    EXPECT_NE(b.find("user 7"), std::string::npos) << b;

    // no Host in HTTP/1.1
    auto c = raw(r, "GET /hello HTTP/1.1\r\n\r\n");
    EXPECT_EQ(c.substr(0, 25), "HTTP/1.1 400 Bad Request\r") << c;

    // chunked request body
    auto d = raw(r, "POST /echo HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n3\r\nabc\r\n2;ext=1\r\nde\r\n0\r\n\r\n");
    EXPECT_NE(d.find("\r\n\r\nabcde"), std::string::npos) << d;

    // CL and TE together
    auto e = raw(r, "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n");
    EXPECT_EQ(e.substr(0, 25), "HTTP/1.1 400 Bad Request\r") << e;

    // the subtree redirect
    auto f = raw(r, "GET /files HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(f.substr(0, 33), "HTTP/1.1 307 Temporary Redirect\r\n") << f;
    EXPECT_NE(f.find("Location: /files/\r\n"), std::string::npos) << f;

    // 100-continue
    auto g = raw(r, "POST /echo HTTP/1.1\r\nHost: x\r\nExpect: 100-continue\r\nContent-Length: 2\r\nConnection: close\r\n\r\nhi");
    EXPECT_EQ(g.substr(0, 25), "HTTP/1.1 100 Continue\r\n\r\n") << g;
    EXPECT_NE(g.find("HTTP/1.1 200 OK"), std::string::npos) << g;

    // HTTP/1.0 closes after the response
    auto h = raw(r, "GET /hello HTTP/1.0\r\n\r\n", false);
    EXPECT_NE(h.find("Connection: close"), std::string::npos) << h;
}
