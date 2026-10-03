//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

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
        w.add_cookie(c);
        w.set_header("X-Got", req.cookie("id") + "|" + req.cookie("none") + "|" + req.query("q"));
    });
    s.not_found([](net::http::request, net::http::response_writer w) { w.set_status(418); });
    Running r(s);
    auto a = talk(r, "GET /c?q=v HTTP/1.1\r\nHost: x\r\nCookie: a=1; id=\"42\"; b=2\r\nConnection: close\r\n\r\n");
    EXPECT_NE(a.find("Set-Cookie: session=\"abc 1\"; Path=/; Max-Age=3600; HttpOnly; SameSite=Lax\r\n"), std::string::npos) << a;
    EXPECT_NE(a.find("X-Got: 42||v\r\n"), std::string::npos) << a;
    auto b = talk(r, "GET /nowhere HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(b.substr(0, 12), "HTTP/1.1 418") << b;
}

// Fields a handler cannot send (found by the fuzzer of the fields,
// tests/net/http/fuzz/http_fields_fuzz.cpp, on the client's side): a value
// with CR or LF from a user would split the response, a NUL or a name that
// is not a token would break it. None of that head goes out: the server
// answers 500 with its own fields, on_error names the field, the
// connection ends; a flush before it gives the error back
// (std::errc::invalid_argument) and sends nothing
TEST(HttpServer_Tests, FieldsThatWouldSplitTheResponse) {
    net::http::server s;
    std::mutex m;
    std::vector<std::string> errors;
    s.on_error = [&](const sgcl::string& e) {
        std::lock_guard g(m);
        errors.push_back(text(e));
    };
    std::atomic<bool> flush_refused{false};
    s.route("/value", [](net::http::request, net::http::response_writer w) {
        w.set_header("X-Bad", "a\r\nInjected: 1");
        w.write("body");
    });
    s.route("/name", [](net::http::request, net::http::response_writer w) {
        w.add_header("Bad Name", "x");
        w.write("body");
    });
    s.route("/nul", [](net::http::request, net::http::response_writer w) {
        w.set_header("X-Nul", sgcl::string(std::string("a\0b", 3)));
        w.write("body");
    });
    s.route("/redirect", [](net::http::request, net::http::response_writer w) {
        w.redirect("/x\r\nSet-Cookie: evil=1");
    });
    s.route("/flush", [&](net::http::request, net::http::response_writer w) -> async::task<> {
        w.set_header("X-Bad", "a\nInjected: 1");
        w.write("body");
        auto f = co_await w.async_flush();
        flush_refused = !f && f.error().code() == std::errc::invalid_argument;
        auto again = co_await w.async_flush();   // the first error, kept
        flush_refused = flush_refused && !again && again.error().code() == std::errc::invalid_argument;
    });
    Running r(s);
    for (auto path : {"/value", "/name", "/nul", "/redirect", "/flush"}) {
        auto a = talk(r, std::string("GET ") + path + " HTTP/1.1\r\nHost: x\r\n\r\n");
        EXPECT_EQ(a.substr(0, 35), "HTTP/1.1 500 Internal Server Error\r") << path << ": " << a;
        EXPECT_EQ(a.find("Injected"), std::string::npos) << a;
        EXPECT_EQ(a.find("evil"), std::string::npos) << a;
        EXPECT_EQ(a.find("Bad Name"), std::string::npos) << a;
        EXPECT_EQ(a.find("body"), std::string::npos) << a;
        EXPECT_NE(a.find("Connection: close\r\n"), std::string::npos) << a;
        EXPECT_EQ(count(a, "HTTP/1.1 "), 1u) << a;
    }
    EXPECT_TRUE(flush_refused);
    std::lock_guard g(m);
    ASSERT_EQ(errors.size(), 5u);
    EXPECT_EQ(errors[0], "a handler of GET /value wrote invalid header value: X-Bad");
    EXPECT_EQ(errors[1], "a handler of GET /name wrote invalid header name: Bad Name");
    EXPECT_EQ(errors[2], "a handler of GET /nul wrote invalid header value: X-Nul");
    EXPECT_EQ(errors[3], "a handler of GET /redirect wrote invalid header value: Location");
    EXPECT_EQ(errors[4], "a handler of GET /flush wrote invalid header value: X-Bad");
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

// A refusal to a HEAD is the head alone (RFC 9110 §9.3.2): no body a client
// would take for the next answer; the same refusal to a GET has its body.
// Found by the fuzzer of the server's path (tests/net/http/fuzz).
TEST(HttpServer_Tests, ARefusalToAHeadHasNoBody) {
    net::http::server s;
    s.route("/", [](net::http::request, net::http::response_writer w) { w.write("x"); });
    Running r(s);
    auto head = talk(r, "HEAD /@ HTTP/1.1\r\nHost: :1\r\n\r\n");
    EXPECT_EQ(head.substr(0, 24), "HTTP/1.1 400 Bad Request") << head;
    EXPECT_EQ(head.substr(head.size() - 4), "\r\n\r\n") << head;             // nothing after the head
    EXPECT_NE(head.find("Content-Length: 16"), std::string::npos) << head;   // the body a GET would have had
    auto get = talk(r, "GET /@ HTTP/1.1\r\nHost: :1\r\n\r\n");
    EXPECT_EQ(get.substr(get.size() - 16), "400 Bad Request\n") << get;
    auto expect = talk(r, "HEAD / HTTP/1.1\r\nHost: x\r\nExpect: something\r\n\r\n");
    EXPECT_EQ(expect.substr(0, 12), "HTTP/1.1 417") << expect;
    EXPECT_EQ(expect.substr(expect.size() - 4), "\r\n\r\n") << expect;
    auto framing = talk(r, "HEAD / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\n");
    EXPECT_EQ(framing.substr(0, 12), "HTTP/1.1 400") << framing;
    EXPECT_EQ(framing.substr(framing.size() - 4), "\r\n\r\n") << framing;
    // each refusal says Connection: close and closes after its head (talk
    // reads to the close); the 417 comes before any body is read
    for (auto& answer : {head, expect, framing}) {
        EXPECT_NE(answer.find("Connection: close\r\n"), std::string::npos) << answer;
    }
    auto early = talk(r, "HEAD / HTTP/1.1\r\nHost: x\r\nExpect: nothing\r\nContent-Length: 5\r\n\r\n");
    EXPECT_EQ(early.substr(0, 12), "HTTP/1.1 417") << early;
    EXPECT_EQ(early.substr(early.size() - 4), "\r\n\r\n") << early;
}

// The url of a request the head routed without it (url.h:
// origin_form_path): made on the first url() or query(), the same as the
// full parse at the head gives, by whichever of two tasks asks first; the
// method, the target and the path values as before. A host the fast path
// refuses goes through the full parse at the head: one it cannot take is
// answered 400 as ever
TEST(HttpServer_Tests, TheUrlMadeWhenTheHandlerAsks) {
    net::http::server s;
    s.route("GET /items/{id}", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto a = async::spawn([](net::http::request r) -> async::task<std::string> { co_return std::string(r.url().to_string().view()); }(req));
        auto b = async::spawn([](net::http::request r) -> async::task<std::string> { co_return std::string(r.url().to_string().view()); }(req));
        std::string ha = co_await a, hb = co_await b;
        w.write(sgcl::string(req.method() + " " + req.path_value("id") + " " + req.query("sort") + " " + (ha == hb ? ha : std::string("different: " + ha + " / " + hb))));
    });
    Running r(s);
    auto plain = talk(r, "GET /items/42?sort=asc HTTP/1.1\r\nHost: example.com:8080\r\nConnection: close\r\n\r\n");
    EXPECT_NE(plain.find("\r\n\r\nGET 42 asc http://example.com:8080/items/42?sort=asc"), std::string::npos) << plain;
    auto refused = talk(r, "GET /items/7?sort=d HTTP/1.1\r\nHost: EXAMPLE.com\r\nConnection: close\r\n\r\n");   // upper case: taken by the fast path, the url lower-cased by the parse
    EXPECT_NE(refused.find("\r\n\r\nGET 7 d http://example.com/items/7?sort=d"), std::string::npos) << refused;
    auto encoded = talk(r, "GET /items/%34%32?sort=x HTTP/1.1\r\nHost: h\r\nConnection: close\r\n\r\n");   // '%': the full parse at the head
    EXPECT_NE(encoded.find("\r\n\r\nGET "), std::string::npos) << encoded;
    auto bad = talk(r, "GET / HTTP/1.1\r\nHost: a.0x\r\n\r\n");   // a host the parse refuses: 400, as before
    EXPECT_EQ(bad.substr(0, 12), "HTTP/1.1 400") << bad;
}

// A thousand requests from eight clients, every connection closed: then
// nothing of them is left. While the server runs: no connection's state
// and no more frame buffers than before the requests (the frames that
// served them gone). Once it is closed and the workers stopped: no request
// and no writer either. Those two are checked only then, as the other
// tests of the workers do (scheduler::stop): an idle worker clears the
// dead part of its stack only so far (SGCL_WORKER_STACK_CLEAR, 4 KB), and a
// word of a handler's deeper call chain may name the last request it
// served until the worker runs that deep again (one request or writer in
// about one run in ten of the suite; none with 64 KB cleared). Failed
// while the scheduler's rings kept the words of the slots taken: frames
// by the thousand stayed alive, and the requests they named by the
// hundred, the rings kept across the stop as well
TEST(HttpServer_Tests, NothingOfAThousandClosedRequestsStaysAlive) {
    net::http::server s;
    s.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.write("hello\n");
    });
    std::optional<Running> running(std::in_place, s);
    Running& r = *running;
    auto frames = [] {
        size_t n = 0;
        for (auto& st : collector::get_type_statistics()) {
            if (*st.type == typeid(sgcl::detail::FrameWord[])) {
                n += st.live_objects;
            }
        }
        return n;
    };
    const std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    (void)talk(r, request);   // the server's own frames made
    std::this_thread::sleep_for(50ms);
    collector::clear_stack();
    const size_t before = frames();
    std::atomic<int> answered = {0};
    std::vector<std::thread> clients;
    for (int c = 0; c < 8; ++c) {
        clients.emplace_back([&] {
            for (int i = 0; i < 125; ++i) {
                answered += talk(r, request).find("\r\n\r\nhello\n") != std::string::npos;
            }
        });
    }
    for (auto& c : clients) {
        c.join();
    }
    EXPECT_EQ(answered.load(), 1000);
    size_t conns = 0, now = 0;
    for (int i = 0; i < 50; ++i) {   // the server's ends close as the clients' did
        std::this_thread::sleep_for(20ms);
        collector::clear_stack();
        conns = live_objects_named("ServerConn");
        now = frames();
        if (!conns && now <= before) {
            break;
        }
    }
    EXPECT_EQ(conns, 0u);
    EXPECT_LE(now, before);
    running.reset();   // the server closed
    size_t requests = 0, writers = 0;
    for (int i = 0; i < 3; ++i) {
        sgcl::async::scheduler::stop();   // the workers' dead stacks gone with them
        collector::clear_stack();
        requests = live_objects_named("RequestImpl");
        writers = live_objects_named("WriterImpl");
        if (!requests && !writers) {
            break;
        }
    }
    EXPECT_EQ(requests, 0u);
    EXPECT_EQ(writers, 0u);
}

// DESIGN 408: the head at max_header_bytes exactly (the request line long,
// or one field long) is served, one byte more is 431; a limit of zero is
// taken as 1, so every head is past it
TEST(HttpServer_Tests, TheHeadAtItsLimit) {
    net::http::server s;
    s.max_header_bytes = 256;
    s.route("/", [](net::http::request req, net::http::response_writer w) { w.write(sgcl::string(std::to_string(req.url().path().size()))); });
    Running r(s);
    const std::string tail = " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    auto line = [&](size_t head) { return "GET /" + std::string(head - 5 - tail.size(), 'a') + tail; };
    ASSERT_EQ(line(256).size(), 256u);
    auto at = talk(r, line(256));
    EXPECT_EQ(at.substr(0, 17), "HTTP/1.1 200 OK\r\n") << at;
    EXPECT_NE(at.find("\r\n\r\n" + std::to_string(256 - 4 - tail.size())), std::string::npos) << at;
    EXPECT_EQ(talk(r, line(257)).substr(0, 12), "HTTP/1.1 431");
    const std::string start = "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\nX: ";
    auto field = [&](size_t head) { return start + std::string(head - start.size() - 4, 'v') + "\r\n\r\n"; };
    ASSERT_EQ(field(256).size(), 256u);
    EXPECT_EQ(talk(r, field(256)).substr(0, 17), "HTTP/1.1 200 OK\r\n");
    EXPECT_EQ(talk(r, field(257)).substr(0, 12), "HTTP/1.1 431");
    // empty lines before a request line count toward the limit, not the head
    EXPECT_EQ(talk(r, std::string(100, '\n') + field(256)).substr(0, 17), "HTTP/1.1 200 OK\r\n");
    EXPECT_EQ(talk(r, std::string(257, '\n')).substr(0, 12), "HTTP/1.1 431");
    net::http::server none;
    none.max_header_bytes = 0;
    none.route("/", [](net::http::request, net::http::response_writer w) { w.write("x"); });
    Running z(none);
    EXPECT_EQ(talk(z, "GET / HTTP/1.1\r\nHost: x\r\n\r\n").substr(0, 12), "HTTP/1.1 431");
}

// DESIGN 408: empty bodies on the server's side: a Content-Length of 0,
// chunked with no chunk (with and without a trailer), none at all, each
// read whole and as a stream, and the next request on the same connection
TEST(HttpServer_Tests, EmptyBodies) {
    net::http::server s;
    s.route("POST /", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto cl = req.content_length();
        auto t = co_await req.async_text();
        tracked_ptr block = make_tracked<array<byte, 8>>();
        auto n = co_await req.body().async_read(slice<byte>(block, block->data(), 8));
        w.write(sgcl::string((cl ? std::to_string(*cl) : std::string("none")) + "|" + (t ? std::string(t->view()) : "err") + "|"
                             + (n ? std::to_string(*n) : "err") + "|" + std::to_string(req.trailers().size())));
    });
    s.route("GET /", [](net::http::request, net::http::response_writer) {});   // nothing written: 200, Content-Length: 0
    Running r(s);
    auto all = talk(r, "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n"
                       "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n"
                       "POST / HTTP/1.1\r\nHost: x\r\n\r\n"
                       "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nT: 1\r\n\r\n"
                       "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(count(all, "HTTP/1.1 200 OK\r\n"), 5u) << all;
    EXPECT_NE(all.find("\r\n\r\n0||0|0HTTP/1.1"), std::string::npos) << all;
    EXPECT_NE(all.find("\r\n\r\nnone||0|0HTTP/1.1"), std::string::npos) << all;
    EXPECT_NE(all.find("\r\n\r\nnone||0|1HTTP/1.1"), std::string::npos) << all;   // the trailer of zero chunks
    EXPECT_NE(all.find("Content-Length: 0\r\n"), std::string::npos) << all;
    EXPECT_EQ(all.substr(all.size() - 4), "\r\n\r\n") << all;                   // the GET's: a head and no body
}

// DESIGN 408: the writer at its ends: the status's range, a redirect's,
// nothing written (whole and flushed), an empty file, a file at its end,
// a file that cannot be read, an error after a flush, a second hijack
TEST(HttpServer_Tests, TheWritersBoundaries) {
    static std::atomic<int> refused{0};
    auto dir = std::filesystem::temp_directory_path() / ("sgcl_http_writer_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    { std::ofstream(dir / "empty"); }
    { std::ofstream(dir / "three") << "abc"; }
    const std::string root = dir.string();
    net::http::server s;
    s.on_error = [](const sgcl::string&) {};
    s.route("/status", [](net::http::request, net::http::response_writer w) {
        for (int code : {199, 1000, -1, 0}) {
            try {
                w.set_status(code);
            } catch (const std::invalid_argument&) {
                ++refused;
            }
        }
        for (int code : {299, 400, 200}) {
            try {
                w.redirect("/r", code);
            } catch (const std::invalid_argument&) {
                ++refused;
            }
        }
        w.redirect("/r", 399);
        w.set_status(999);
        w.write("").write(slice<const byte>());
    });
    s.route("/flushed", [](net::http::request, net::http::response_writer w) -> async::task<> {
        (void)co_await w.async_flush();                         // nothing written: the head alone
        w.set_status(500);                                      // after the head: ignored
        w.set_header("X-After", "1");
        w.error(404, "added");                                  // after the head: the line added
        auto h = w.hijack();
        w.write(h ? "hijacked" : "|not hijacked");
    });
    s.route("/empty", [root](net::http::request, net::http::response_writer w) { w.write(*io::open(sgcl::string(root + "/empty"))); });
    s.route("/at-end", [root](net::http::request, net::http::response_writer w) {
        auto f = *io::open(sgcl::string(root + "/three"));
        (void)io::read_all(f);
        w.write(f);
    });
    s.route("/dir", [root](net::http::request, net::http::response_writer w) {
        auto f = io::open(sgcl::string(root));
        if (f) {
            w.write(*f);
        } else {
            w.write("not opened");
        }
    });
    s.route("/hijack", [](net::http::request, net::http::response_writer w) -> async::task<> {
        auto first = w.hijack();
        auto second = w.hijack();
        if (first && !second && second.error().code() == io::errc::closed) {
            auto [c, rest] = *first;
            (void)co_await c.async_write(sgcl::string("once"));
            (void)co_await c.async_close();
        }
    });
    Running r(s);
    auto status = talk(r, "GET /status HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(refused.load(), 7);
    EXPECT_EQ(status.substr(0, 13), "HTTP/1.1 999 ") << status;
    EXPECT_NE(status.find("Location: /r\r\n"), std::string::npos) << status;
    EXPECT_NE(status.find("Content-Length: 0\r\n"), std::string::npos) << status;
    auto flushed = talk(r, "GET /flushed HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(flushed.substr(0, 17), "HTTP/1.1 200 OK\r\n") << flushed;
    EXPECT_EQ(flushed.find("X-After"), std::string::npos) << flushed;
    EXPECT_NE(flushed.find("Transfer-Encoding: chunked\r\n"), std::string::npos) << flushed;
    EXPECT_NE(flushed.find("\r\n\r\n13\r\nadded\n|not hijacked\r\n0\r\n\r\n"), std::string::npos) << flushed;
    auto empty = talk(r, "GET /empty HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(empty.find("Content-Length: 0\r\n"), std::string::npos) << empty;
    EXPECT_EQ(empty.substr(empty.size() - 4), "\r\n\r\n") << empty;
    auto at_end = talk(r, "GET /at-end HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(at_end.find("Content-Length: 0\r\n"), std::string::npos) << at_end;
    auto unread = talk(r, "GET /dir HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(unread.substr(0, 12), "HTTP/1.1 500") << unread;   // the page: a read that failed, 500 when nothing was sent
    EXPECT_EQ(talk(r, "GET /hijack HTTP/1.1\r\nHost: x\r\n\r\n"), "once");
    std::filesystem::remove_all(dir);
}

// DESIGN 408: a writer kept past the end of its response (a task the
// handler started holds it): its writes go nowhere, a flush is
// io::errc::closed, and nothing of it reaches the connection, which has
// gone on to the next request
TEST(HttpServer_Tests, AWriterUsedAfterTheResponseEnded) {
    async::event go, done;
    static std::atomic<int> flushed{-1}, hijacked{-1}, status{0};
    static std::atomic<bool> sent{false};
    net::http::server s;
    s.route("/first", [go, done](net::http::request, net::http::response_writer w) {
        w.write("first");
        async::go([](net::http::response_writer w, async::event go, async::event done) -> async::task<> {
            co_await go;
            w.set_status(500);
            w.set_header("X-Late", "1");
            w.write("late");
            w.write(sgcl::string("late"));
            w.error(503, "late");
            auto f = co_await w.async_flush();
            flushed = f ? 0 : int(f.error().code() == io::errc::closed);
            hijacked = w.hijack() ? 0 : 1;
            sent = w.header_sent();
            status = w.status();
            done.set();
        }(w, go, done));
    });
    s.route("/second", [](net::http::request, net::http::response_writer w) { w.write("second"); });
    Running r(s);
    auto c = r.connect();
    (void)c.write(sgcl::string("GET /first HTTP/1.1\r\nHost: x\r\n\r\n"));
    std::string got;
    tracked_ptr block = make_tracked<array<byte, 4096>>();
    while (got.find("\r\n\r\nfirst") == std::string::npos) {
        auto n = c.read(slice<byte>(block, block->data(), block->size()));
        ASSERT_TRUE(n && *n);
        got.append(reinterpret_cast<const char*>(block->data()), *n);
    }
    go.set();
    done.wait();
    EXPECT_EQ(flushed.load(), 1);
    EXPECT_EQ(hijacked.load(), 1);
    EXPECT_TRUE(sent.load());
    EXPECT_EQ(status.load(), 200);                              // the status that was sent
    (void)c.write(sgcl::string("GET /second HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"));
    auto rest = c.read_all_text();
    ASSERT_TRUE(rest);
    auto second = text(*rest);
    EXPECT_EQ(second.substr(0, 17), "HTTP/1.1 200 OK\r\n") << second;
    EXPECT_EQ(second.find("late"), std::string::npos) << second;
    EXPECT_EQ(second.substr(second.size() - 10), "\r\n\r\nsecond") << second;
}

// DESIGN 408: a server moved from is the same server: the routes shared,
// the settings kept, on_error among them; into itself the same. Closed
// before it serves, and closed or shut down twice, it says server_closed
TEST(HttpServer_Tests, AMovedFromServerIsTheSameServer) {
    static std::atomic<int> told{0};
    net::http::server s;
    s.on_error = [](const sgcl::string&) { ++told; };
    s.max_body_bytes = 5;
    s.route("/throw", [](net::http::request, net::http::response_writer) { throw std::runtime_error("boom"); });
    net::http::server to(std::move(s));
    to.route("/late", [](net::http::request, net::http::response_writer w) { w.write("late"); });
    auto& self = s;
    s = self;
    {
        Running r(s);
        EXPECT_EQ(talk(r, "GET /throw HTTP/1.1\r\nHost: x\r\n\r\n").substr(0, 12), "HTTP/1.1 500");
        EXPECT_EQ(told.load(), 1);                                  // its own on_error, kept
        EXPECT_EQ(talk(r, "POST /late HTTP/1.1\r\nHost: x\r\nContent-Length: 6\r\n\r\n123456").substr(0, 12), "HTTP/1.1 413");
        auto late = talk(r, "GET /late HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
        EXPECT_EQ(late.substr(late.size() - 4), "late") << late;   // a route added through the other copy
    }
    // closed (by Running), then served again, closed and shut down again
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto again = s.serve(l);
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code(), net::errc::server_closed);
    s.close();
    s.shutdown();
    net::http::server fresh;
    fresh.shutdown();                                               // never served: at once
    auto after = fresh.serve(*net::tcp::listen("127.0.0.1:0"));
    ASSERT_FALSE(after);
    EXPECT_EQ(after.error().code(), net::errc::server_closed);
    net::http::server bad;
    auto nowhere = bad.serve("not an address");
    EXPECT_FALSE(nowhere);
}

// DESIGN 408: a received request's readers at their ends: cookies (an
// empty name, the first of two, one without '=', a bad value passed over,
// two Cookie fields), the query (an empty name, an empty value, none), a
// path value that is not the route's, the URL of the absolute form
TEST(HttpServer_Tests, TheRequestsReadersAtTheirEnds) {
    net::http::server s;
    s.route("/r/{id}", [](net::http::request req, net::http::response_writer w) {
        std::string out;
        for (const char* name : {"", "a", "b", "c", "d", "e"}) {
            out += "[" + std::string(req.cookie(name).view()) + "]";
        }
        out += "|" + std::string(req.query("").view()) + "|" + std::string(req.query("q").view()) + "|"
               + std::string(req.query("none").view()) + "|" + std::string(req.path_value("id").view()) + "|"
               + std::string(req.path_value("other").view()) + "|" + std::string(req.url().to_string().view());
        w.write(sgcl::string(out));
    });
    Running r(s);
    auto a = talk(r, "GET /r/7?=x&q= HTTP/1.1\r\nHost: h\r\n"
                     "Cookie: =z; a=1; a=2; b; c=\"x; c=ok\r\nCookie: d=\"q\"; e=v v,w\r\nConnection: close\r\n\r\n");
    EXPECT_NE(a.find("\r\n\r\n[][1][][ok][q][v v,w]|x|||7||http://h/r/7?=x&q="), std::string::npos) << a;
    auto absolute = talk(r, "GET http://other:8080/r/8 HTTP/1.1\r\nHost: h\r\nConnection: close\r\n\r\n");
    EXPECT_NE(absolute.find("|8||http://other:8080/r/8"), std::string::npos) << absolute;
}
