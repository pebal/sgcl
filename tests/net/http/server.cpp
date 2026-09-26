//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: the server, spoken to in raw bytes over the loopback: the limits
// (431, 413, before the handler and while it reads), the timeouts on the
// manual clock (a head sent a byte at a time, an idle connection), the
// graceful shutdown with a request in flight and an idle connection, close
// and a request's stop, a handler that throws, the body a handler leaves
// (drained, or the connection closed), HEAD, HTTP/1.0 with and without
// keep-alive, hijack, flush and its framings, cookies, not_found.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    struct Running {
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        explicit Running(net::http::server s)
        : server(s) {
            listener = *net::tcp::listen("127.0.0.1:0");
            serving = async::spawn(server.async_serve(listener));
        }

        ~Running() {
            server.close();
            (void)serving.wait();
        }

        net::connection connect() const {
            return *net::tcp::connect(listener.local_endpoint());
        }
    };

    // Bytes sent, everything read back to the server's close
    std::string talk(const Running& r, const std::string& bytes) {
        auto c = r.connect();
        (void)c.write(sgcl::string(bytes));
        auto all = c.read_all_text();
        (void)c.close();
        return all ? text(*all) : "<" + text(all.error().message()) + ">";
    }

    size_t count(const std::string& s, const std::string& what) {
        size_t n = 0;
        for (size_t at = s.find(what); at != std::string::npos; at = s.find(what, at + 1)) {
            ++n;
        }
        return n;
    }

    void settle(async::manual_clock& clock) {
        clock.advance(0ms);
        std::this_thread::sleep_for(20ms);
        clock.advance(0ms);
    }
}

TEST(HttpServer_Tests, Limits) {
    net::http::server s;
    s.max_header_bytes = 256;
    s.max_body_bytes = 10;
    s.route("POST /quiet", [](net::http::request req, net::http::response_writer) -> async::task<> {
        (void)co_await req.async_text();   // a body too large: the handler writes nothing, the server answers
    });
    s.route("POST /loud", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto b = co_await req.async_text();
        w.write(b ? "read" : "not read: " + b.error().message());
    });
    Running r(s);
    auto big = talk(r, "GET / HTTP/1.1\r\nHost: x\r\nX: " + std::string(300, 'a') + "\r\n\r\n");
    EXPECT_EQ(big.substr(0, 45), "HTTP/1.1 431 Request Header Fields Too Large\r") << big;
    auto declared = talk(r, "POST /quiet HTTP/1.1\r\nHost: x\r\nContent-Length: 11\r\n\r\n12345678901");
    EXPECT_EQ(declared.substr(0, 31), "HTTP/1.1 413 Content Too Large\r") << declared;
    auto chunked = talk(r, "POST /quiet HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n14\r\n12345678901234567890\r\n0\r\n\r\n");
    EXPECT_EQ(chunked.substr(0, 31), "HTTP/1.1 413 Content Too Large\r") << chunked;
    EXPECT_NE(chunked.find("Connection: close"), std::string::npos);
    auto loud = talk(r, "POST /loud HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n14\r\n12345678901234567890\r\n0\r\n\r\n");
    EXPECT_EQ(loud.substr(0, 17), "HTTP/1.1 200 OK\r\n") << loud;   // the handler's answer stands
    EXPECT_NE(loud.find("not read: read body: body too large"), std::string::npos) << loud;
    auto fits = talk(r, "POST /loud HTTP/1.1\r\nHost: x\r\nContent-Length: 10\r\nConnection: close\r\n\r\n1234567890");
    EXPECT_NE(fits.find("\r\n\r\nread"), std::string::npos) << fits;
    EXPECT_EQ(talk(r, "GET / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: gzip\r\n\r\n").substr(0, 28), "HTTP/1.1 501 Not Implemented");
    EXPECT_EQ(talk(r, "GET / HTTP/2.0\r\nHost: x\r\n\r\n").substr(0, 12), "HTTP/1.1 505");
    EXPECT_EQ(talk(r, "GET / HTTP/1.1\r\nHost: x\r\nExpect: something\r\n\r\n").substr(0, 12), "HTTP/1.1 417");
}

TEST(HttpServer_Tests, TimeoutsOnTheManualClock) {
    async::manual_clock clock;
    clock.install();
    {
        net::http::server s;
        s.read_header_timeout = 10s;
        s.idle_timeout = 60s;
        s.route("/", [](net::http::request, net::http::response_writer w) { w.write("ok"); });
        Running r(s);
        // a head that never ends: closed after read_header_timeout
        auto slow = r.connect();
        (void)slow.write(sgcl::string("GET / HTTP/1.1\r\nHost: x\r\n"));
        auto slow_end = async::spawn(slow.async_read_all_text());
        settle(clock);
        clock.advance(9s);
        settle(clock);
        EXPECT_FALSE(slow_end.done());
        (void)slow.write(sgcl::string("X-Byte: 1\r\n"));   // a byte more does not move the deadline
        settle(clock);
        clock.advance(2s);
        auto got = slow_end.wait();
        ASSERT_TRUE(got);
        EXPECT_EQ(text(*got), "");                                  // closed, nothing sent

        // a kept connection idle: closed after idle_timeout
        auto idle = r.connect();
        (void)idle.write(sgcl::string("GET / HTTP/1.1\r\nHost: x\r\n\r\n"));
        auto idle_end = async::spawn(idle.async_read_all_text());
        settle(clock);
        clock.advance(59s);
        settle(clock);
        EXPECT_FALSE(idle_end.done());
        clock.advance(2s);
        auto answered = idle_end.wait();
        ASSERT_TRUE(answered);
        EXPECT_NE(text(*answered).find("\r\n\r\nok"), std::string::npos);
    }
    clock.uninstall();
}

TEST(HttpServer_Tests, GracefulShutdown) {
    net::http::server s;
    tracked_ptr<async::event> go_on = make_tracked<async::event>();
    std::atomic<bool> entered{false};
    s.route("/slow", [go_on, &entered](net::http::request, net::http::response_writer w) -> async::task<> {
        entered = true;
        co_await *go_on;
        w.write("finished");
    });
    s.route("/fast", [](net::http::request, net::http::response_writer w) { w.write("fast"); });
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(s.async_serve(l));
    // an idle kept connection, and one with a request in flight
    auto idle = *net::tcp::connect(l.local_endpoint());
    (void)idle.write(sgcl::string("GET /fast HTTP/1.1\r\nHost: x\r\n\r\n"));
    auto idle_all = async::spawn(idle.async_read_all_text());
    auto busy = *net::tcp::connect(l.local_endpoint());
    (void)busy.write(sgcl::string("GET /slow HTTP/1.1\r\nHost: x\r\n\r\n"));
    auto busy_all = async::spawn(busy.async_read_all_text());
    while (!entered) {
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(20ms);   // the fast one answered and idle
    auto down = async::spawn(s.async_shutdown());
    auto idle_got = idle_all.wait();      // closed by the shutdown
    ASSERT_TRUE(idle_got);
    EXPECT_EQ(count(text(*idle_got), "HTTP/1.1 200"), 1u);
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(down.done());         // the slow one holds it
    go_on->set();
    down.wait();
    auto busy_got = busy_all.wait();
    ASSERT_TRUE(busy_got);
    EXPECT_NE(text(*busy_got).find("Connection: close\r\n"), std::string::npos) << text(*busy_got);
    EXPECT_NE(text(*busy_got).find("finished"), std::string::npos);
    auto result = serving.wait();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), net::errc::server_closed);
    auto again = s.serve(*net::tcp::listen("127.0.0.1:0"));
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code(), net::errc::server_closed);
}

TEST(HttpServer_Tests, CloseStopsTheRequests) {
    net::http::server s;
    std::atomic<bool> entered{false}, stopped{false};
    s.route("/wait", [&](net::http::request req, net::http::response_writer) -> async::task<> {
        entered = true;
        co_await req.stop().stopped();
        stopped = true;
    });
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(s.async_serve(l));
    auto c = *net::tcp::connect(l.local_endpoint());
    (void)c.write(sgcl::string("GET /wait HTTP/1.1\r\nHost: x\r\n\r\n"));
    while (!entered) {
        std::this_thread::sleep_for(1ms);
    }
    s.close();
    EXPECT_FALSE(serving.wait());
    for (int i = 0; i < 1000 && !stopped; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_TRUE(stopped);
    auto rest = c.read_all_text();
    EXPECT_TRUE(!rest || text(*rest).empty());
}

TEST(HttpServer_Tests, AHandlerThatThrows) {
    net::http::server s;
    std::mutex m;
    std::vector<std::string> errors;
    s.on_error = [&](const sgcl::string& e) {
        std::lock_guard g(m);
        errors.push_back(text(e));
    };
    s.route("/throw", [](net::http::request, net::http::response_writer w) {
        w.write("half");
        throw std::runtime_error("broken");
    });
    s.route("/after-flush", [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.write("sent");
        (void)co_await w.async_flush();
        throw 42;
    });
    Running r(s);
    auto a = talk(r, "GET /throw HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(a.substr(0, 35), "HTTP/1.1 500 Internal Server Error\r") << a;
    EXPECT_EQ(a.find("half"), std::string::npos);
    auto b = talk(r, "GET /after-flush HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(b.substr(0, 17), "HTTP/1.1 200 OK\r\n") << b;       // the head had gone
    EXPECT_NE(b.find("sent"), std::string::npos);
    std::lock_guard g(m);
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_EQ(errors[0], "a handler of GET /throw threw: broken");
    EXPECT_EQ(errors[1], "a handler of GET /after-flush threw");
}

TEST(HttpServer_Tests, TheBodyAHandlerLeaves) {
    net::http::server s;
    s.route("/", [](net::http::request req, net::http::response_writer w) { w.write("seen " + req.url().path()); });
    Running r(s);
    auto small = talk(r, "POST /a HTTP/1.1\r\nHost: x\r\nContent-Length: 1000\r\n\r\n" + std::string(1000, 'b') +
                                 "GET /b HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(small.find("seen /a"), std::string::npos);
    EXPECT_NE(small.find("seen /b"), std::string::npos) << small;   // drained, the connection kept
    auto large = talk(r, "POST /a HTTP/1.1\r\nHost: x\r\nContent-Length: 400000\r\n\r\n" + std::string(400000, 'b') +
                                 "GET /b HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_NE(large.find("seen /a"), std::string::npos);
    EXPECT_EQ(large.find("seen /b"), std::string::npos);            // past 256 KB: closed
}

TEST(HttpServer_Tests, HeadAndHttp10) {
    net::http::server s;
    s.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("twelve bytes"); });
    s.route("GET /flush", [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.write("a");
        (void)co_await w.async_flush();
        w.write("b");
    });
    s.route("GET /sized", [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.set_header("Content-Length", "2");
        w.write("a");
        (void)co_await w.async_flush();
        w.write("b");
    });
    Running r(s);
    auto head = talk(r, "HEAD / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(head.find("Content-Length: 12\r\n"), std::string::npos) << head;
    EXPECT_EQ(head.find("twelve"), std::string::npos);
    auto kept = talk(r, "GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.0\r\n\r\n");
    EXPECT_EQ(count(kept, "twelve bytes"), 2u) << kept;
    EXPECT_NE(kept.find("Connection: keep-alive\r\n"), std::string::npos);
    auto closed = talk(r, "GET / HTTP/1.0\r\n\r\nGET / HTTP/1.0\r\n\r\n");
    EXPECT_EQ(count(closed, "twelve bytes"), 1u);
    auto chunked = talk(r, "GET /flush HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(chunked.find("Transfer-Encoding: chunked\r\n"), std::string::npos) << chunked;
    EXPECT_NE(chunked.find("\r\n\r\n1\r\na\r\n1\r\nb\r\n0\r\n\r\n"), std::string::npos) << chunked;
    auto to_close = talk(r, "GET /flush HTTP/1.0\r\n\r\n");
    EXPECT_NE(to_close.find("Connection: close\r\n\r\nab"), std::string::npos) << to_close;
    auto sized = talk(r, "GET /sized HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(sized.find("Content-Length: 2\r\n"), std::string::npos) << sized;
    EXPECT_NE(sized.find("\r\n\r\nab"), std::string::npos) << sized;
    EXPECT_EQ(sized.find("Transfer-Encoding"), std::string::npos);
}

TEST(HttpServer_Tests, Hijack) {
    net::http::server s;
    s.route("/raw", [](net::http::request, net::http::response_writer w) -> async::task<> {
        auto taken = w.hijack();
        if (!taken) {
            co_return;
        }
        auto [c, rest] = *taken;
        tracked_ptr block = make_tracked<array<byte, 16>>();
        slice<byte> buf(block, block->data(), 5);
        (void)co_await rest.async_read_full(buf);                            // the bytes past the head, then the connection
        (void)co_await c.async_write(sgcl::string("raw:"));
        (void)co_await c.async_write(buf);
        (void)co_await c.async_close();
    });
    Running r(s);
    auto c = r.connect();
    (void)c.write(sgcl::string("GET /raw HTTP/1.1\r\nHost: x\r\n\r\nhel"));
    std::this_thread::sleep_for(10ms);
    (void)c.write(sgcl::string("lo"));
    auto all = c.read_all_text();
    ASSERT_TRUE(all);
    EXPECT_EQ(text(*all), "raw:hello");
}

TEST(HttpServer_Tests, CookiesHeadersAndNotFound) {
    net::http::server s;
    s.route("/c", [](net::http::request req, net::http::response_writer w) {
        net::http::cookie c("session", "abc 1");
        c.path = "/";
        c.http_only = true;
        c.max_age = 3600s;
        c.same_site = "lax";
        w.set_cookie(c);
        w.set_header("X-Got", req.cookie("id") + "|" + req.cookie("none") + "|" + req.query("q"));
        w.set_header("X-Bad", "a\r\nInjected: 1");
    });
    s.not_found([](net::http::request, net::http::response_writer w) { w.set_status(418); });
    Running r(s);
    auto a = talk(r, "GET /c?q=v HTTP/1.1\r\nHost: x\r\nCookie: a=1; id=\"42\"; b=2\r\nConnection: close\r\n\r\n");
    EXPECT_NE(a.find("Set-Cookie: session=\"abc 1\"; Path=/; Max-Age=3600; HttpOnly; SameSite=Lax\r\n"), std::string::npos) << a;
    EXPECT_NE(a.find("X-Got: 42||v\r\n"), std::string::npos) << a;
    EXPECT_NE(a.find("X-Bad: a  Injected: 1\r\n"), std::string::npos) << a;
    auto b = talk(r, "GET /nowhere HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(b.substr(0, 12), "HTTP/1.1 418") << b;
}

// Requests that come a byte at a time, and in pieces of 2, 3 and 7, read the
// same: two pipelined, the first with a chunked body
TEST(HttpServer_Tests, ARequestInPieces) {
    net::http::server s;
    s.route("/", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto b = co_await req.async_text();
        w.write(req.method() + " " + req.url().path() + " [" + (b ? *b : "?") + "]\n");
    });
    Running r(s);
    const std::string bytes = "POST /a HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n2;e=1\r\nde\r\n0\r\nT: v\r\n\r\n"
                              "GET /b HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(7)}) {
        auto c = r.connect();
        for (size_t at = 0; at < bytes.size(); at += piece) {
            (void)c.write(sgcl::string(bytes.substr(at, piece)));
        }
        auto all = c.read_all_text();
        ASSERT_TRUE(all);
        EXPECT_NE(text(*all).find("POST /a [abcde]"), std::string::npos) << piece << text(*all);
        EXPECT_NE(text(*all).find("GET /b []"), std::string::npos) << piece;
        (void)c.close();
    }
}
