//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: the client against a server of scripted bytes, dialed in memory
// (net::connection::in_memory through client.dial): the pool (reuse, a
// Connection: close, a body not read, close() of a response), the retry of
// a request on a pooled connection the server had closed, every framing of
// a response (length, chunked with trailers, to the close, HEAD, 1xx),
// redirects (each status, the headers of credentials across hosts, the
// limit), the errors (a URL that is not one, https, a malformed response,
// a head too large, a body cut, a timeout), and a body sent as a stream.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // The body of an answer as text, or its error in angle brackets: each
    // expected looked at before its value, so that a failure compares
    // unequal rather than throwing out of the assertion
    std::string body_of(const expected<net::http::response, io::error>& r) {
        if (!r) {
            return "<" + text(r.error().message()) + ">";
        }
        auto t = r->text();
        return t ? text(*t) : "<" + text(t.error().message()) + ">";
    }

    // The server's side of the script: what it received, and how it answers
    struct Script {
        std::vector<std::string> received;   // each request, head and body
        int dials = 0;
        size_t piece = 0;                    // the answer written this many bytes at a time; 0: whole
        std::atomic<int> ended{0};           // connections the client closed
        // the response to the n-th request (counted over all connections),
        // and whether the connection ends after it
        std::function<std::pair<std::string, bool>(int, const std::string&)> answer;
    };

    // One request off the connection: its head and its body (by
    // Content-Length or chunked); "" at the end of the stream
    async::task<std::string> read_request(net::connection c, std::string& pending) {
        auto block = make_tracked<array<byte, 4096>>();
        auto more = [&]() -> async::task<bool> {
            slice<byte> room(tracked_ptr<const void>(), block->data(), block->size());
            auto n = co_await c.async_read(room);
            if (!n || *n == 0) {
                co_return false;
            }
            pending.append(reinterpret_cast<const char*>(block->data()), *n);
            co_return true;
        };
        size_t end;
        while ((end = pending.find("\r\n\r\n")) == std::string::npos) {
            if (!co_await more()) {
                co_return std::string();
            }
        }
        std::string head = pending.substr(0, end + 4);
        pending.erase(0, end + 4);
        std::string body;
        auto cl = head.find("Content-Length: ");
        if (cl != std::string::npos) {
            size_t n = std::stoul(head.substr(cl + 16));
            while (pending.size() < n && co_await more()) {
            }
            body = pending.substr(0, n);
            pending.erase(0, n);
        } else if (head.find("Transfer-Encoding: chunked") != std::string::npos) {
            while (pending.find("0\r\n\r\n") == std::string::npos && co_await more()) {
            }
            auto z = pending.find("0\r\n\r\n");
            body = pending.substr(0, z + 5);
            pending.erase(0, z + 5);
        }
        co_return head + body;
    }

    // A script lives for the rest of the run: the tasks that serve it can
    // outlive the test (a connection the client left open is closed later,
    // by the collector or a pool's timer, and its task then wakes)
    Script& new_script() {
        return *new Script;
    }

    async::task<> serve_script(net::connection c, Script* s) {
        std::string pending;
        for (;;) {
            auto r = co_await read_request(c, pending);
            if (r.empty()) {
                break;
            }
            int n = int(s->received.size());
            s->received.push_back(r);
            auto [bytes, close] = s->answer(n, r);
            if (s->piece) {
                for (size_t at = 0; at < bytes.size(); at += s->piece) {
                    (void)co_await c.async_write(sgcl::string(bytes.substr(at, s->piece)));
                }
            } else {
                (void)co_await c.async_write(sgcl::string(bytes));
            }
            if (close) {
                break;
            }
        }
        ++s->ended;
        (void)co_await c.async_close();
    }

    async::task<expected<net::connection, io::error>> dial_script(Script* s) {
        auto [a, b] = net::connection::in_memory();
        ++s->dials;
        async::go(serve_script(b, s));
        co_return a;
    }

    net::http::client client_of(Script& s) {
        net::http::client c;
        Script* p = &s;
        c.dial = [p](const net::url&, async::stop_token) { return dial_script(p); };
        return c;
    }

    std::pair<std::string, bool> ok(const std::string& body, const std::string& extra = "", bool close = false) {
        return {"HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + extra + "\r\n" + body, close};
    }
}

TEST(HttpClient_Tests, ThePool) {
    Script& s = new_script();
    s.answer = [](int n, const std::string&) { return ok("body " + std::to_string(n)); };
    auto c = client_of(s);
    for (int i : range(3)) {
        auto r = c.get("http://example.com/x");
        ASSERT_TRUE(r) << r.error().message();
        EXPECT_EQ(body_of(r), "body " + std::to_string(i));
    }
    EXPECT_EQ(s.dials, 1);                                      // one connection, kept
    EXPECT_NE(s.received[0].find("GET /x HTTP/1.1\r\nHost: example.com\r\n"), std::string::npos) << s.received[0];

    // a response not read to its end keeps its connection out of the pool
    auto open = c.get("http://example.com/y");
    ASSERT_TRUE(open);
    auto next = c.get("http://example.com/z");
    ASSERT_TRUE(next);
    EXPECT_EQ(s.dials, 2);
    (void)next->text();
    // close() with the rest in the buffer: the connection goes back
    open->close();
    (void)c.get("http://example.com/w")->text();
    EXPECT_EQ(s.dials, 2);

    // another origin, another connection
    (void)c.get("http://example.com:81/")->text();
    EXPECT_EQ(s.dials, 3);

    // the pool emptied: the next request dials again
    static_assert(noexcept(c.close_idle_connections()));
    c.close_idle_connections();
    (void)c.get("http://example.com/v")->text();
    EXPECT_EQ(s.dials, 4);
}

TEST(HttpClient_Tests, ConnectionCloseAndToTheClose) {
    Script& s = new_script();
    s.answer = [](int n, const std::string&) -> std::pair<std::string, bool> {
        if (n == 0) {
            return ok("a", "Connection: close\r\n", true);
        }
        return {"HTTP/1.1 200 OK\r\n\r\nuntil the close", true};
    };
    auto c = client_of(s);
    EXPECT_EQ(body_of(c.get("http://h/")), "a");
    auto r = c.get("http://h/");
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->content_length());
    EXPECT_EQ(body_of(r), "until the close");
    EXPECT_EQ(s.dials, 2);
}

TEST(HttpClient_Tests, RetryOnAConnectionThePoolKeptTooLong) {
    Script& s = new_script();
    // the server answers each request and then closes, without saying so
    s.answer = [](int n, const std::string&) { return ok(std::to_string(n), "", true); };
    auto c = client_of(s);
    EXPECT_EQ(body_of(c.get("http://h/")), "0");
    auto r = c.get("http://h/");                          // on the dead connection first
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(body_of(r), "1");
    EXPECT_EQ(s.dials, 2);
    // a stream body is not sent twice: on the dead pooled connection the
    // request fails, and is not tried again
    auto post = net::http::request("POST", "http://h/");
    post.set_body(io::reader(make_tracked<io::buffer>("abc")));
    auto p = c.send(post);
    EXPECT_FALSE(p);
    EXPECT_EQ(s.dials, 2);
}

TEST(HttpClient_Tests, Framings) {
    Script& s = new_script();
    s.answer = [](int n, const std::string& req) -> std::pair<std::string, bool> {
        switch (n) {
            case 0: return {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\nX-Sum: 11\r\n\r\n", false};
            case 1: return {"HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 103 Early Hints\r\nLink: </a>\r\n\r\nHTTP/1.1 201 Created\r\nContent-Length: 2\r\n\r\nok", false};
            case 2: return {"HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n", false};   // HEAD: no body
            case 3: return {"HTTP/1.1 204 No Content\r\n\r\n", false};
            case 4: return {"HTTP/1.1 304 Not Modified\r\nContent-Length: 50\r\n\r\n", false};
            default: return ok(req.substr(0, req.find(' ')));
        }
    };
    auto c = client_of(s);
    auto a = c.get("http://h/");
    ASSERT_TRUE(a);
    EXPECT_EQ(body_of(a), "hello world");
    EXPECT_EQ(a->trailers().get("x-sum"), "11");
    auto b = c.get("http://h/");
    ASSERT_TRUE(b);
    EXPECT_EQ(b->status(), 201);
    EXPECT_EQ(body_of(b), "ok");
    auto h = c.head("http://h/");
    ASSERT_TRUE(h);
    EXPECT_EQ(h->content_length(), 1000u);
    EXPECT_EQ(body_of(h), "");
    EXPECT_EQ(body_of(c.get("http://h/")), "");
    auto m = c.get("http://h/");
    EXPECT_EQ(m->status(), 304);
    EXPECT_EQ(body_of(m), "");
    EXPECT_EQ(body_of(c.get("http://h/")), "GET");
    EXPECT_EQ(s.dials, 1);                                       // every one of them on one connection
}

TEST(HttpClient_Tests, Redirects) {
    Script& s = new_script();
    s.answer = [](int n, const std::string&) -> std::pair<std::string, bool> {
        switch (n) {
            case 0: return {"HTTP/1.1 302 Found\r\nLocation: /b?x=1\r\nContent-Length: 3\r\n\r\nabc", false};
            case 1: return {"HTTP/1.1 301 Moved Permanently\r\nLocation: http://sub.h/c\r\nContent-Length: 0\r\n\r\n", false};
            case 2: return {"HTTP/1.1 308 Permanent Redirect\r\nLocation: http://other/d\r\nContent-Length: 0\r\n\r\n", false};
            default: return ok("end");
        }
    };
    auto c = client_of(s);
    net::http::request r("GET", "http://h/a");
    r.set_header("Authorization", "Bearer x").set_header("Cookie", "a=b");
    auto res = c.send(r);
    ASSERT_TRUE(res) << res.error().message();
    EXPECT_EQ(body_of(res), "end");
    EXPECT_EQ(res->url().to_string(), "http://other/d");
    ASSERT_EQ(s.received.size(), 4u);
    EXPECT_EQ(s.received[1].substr(0, 20), "GET /b?x=1 HTTP/1.1\r");
    EXPECT_NE(s.received[1].find("Authorization: Bearer x"), std::string::npos);   // the same host
    EXPECT_NE(s.received[2].find("Host: sub.h\r\n"), std::string::npos);
    EXPECT_NE(s.received[2].find("Authorization: Bearer x"), std::string::npos);   // a host under it
    EXPECT_EQ(s.received[3].find("Authorization"), std::string::npos);             // another host
    EXPECT_EQ(s.received[3].find("Cookie"), std::string::npos);
}

TEST(HttpClient_Tests, RedirectsOfABody) {
    Script& s = new_script();
    s.answer = [](int n, const std::string& req) -> std::pair<std::string, bool> {
        if (n == 0) {
            return {"HTTP/1.1 303 See Other\r\nLocation: /seen\r\nContent-Length: 0\r\n\r\n", false};
        }
        if (n == 2) {
            return {"HTTP/1.1 307 Temporary Redirect\r\nLocation: /again\r\nContent-Length: 0\r\n\r\n", false};
        }
        return ok(req);
    };
    auto c = client_of(s);
    auto a = c.post("http://h/form", "text/plain", "data");
    ASSERT_TRUE(a);
    auto seen = text(*a->text());
    EXPECT_EQ(seen.substr(0, 9), "GET /seen");                   // 303: GET, no body
    EXPECT_EQ(seen.find("Content-Type"), std::string::npos);
    EXPECT_EQ(seen.find("data"), std::string::npos);
    auto b = c.post("http://h/form", "text/plain", "data");
    ASSERT_TRUE(b);
    auto again = text(*b->text());
    EXPECT_EQ(again.substr(0, 11), "POST /again");               // 307: the method and the body
    EXPECT_NE(again.find("Content-Length: 4\r\n\r\ndata"), std::string::npos) << again;
}

TEST(HttpClient_Tests, TooManyRedirects) {
    Script& s = new_script();
    s.answer = [](int, const std::string&) -> std::pair<std::string, bool> {
        return {"HTTP/1.1 302 Found\r\nLocation: /loop\r\nContent-Length: 0\r\n\r\n", false};
    };
    auto c = client_of(s);
    c.max_redirects = 3;
    auto r = c.get("http://h/");
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), net::errc::too_many_redirects);
    EXPECT_EQ(s.received.size(), 4u);
}

// The URL parsed by the request's constructor is bounded (url.h: 512 MiB),
// so a request and the client's one-line forms throw nothing
static_assert(std::is_nothrow_constructible_v<net::http::request, const string&, const string&>);
static_assert(noexcept(std::declval<const net::http::client&>().async_get(std::declval<const string&>())));
static_assert(noexcept(std::declval<const net::http::client&>().async_head(std::declval<const string&>())));
static_assert(noexcept(std::declval<const net::http::client&>().async_post(std::declval<const string&>(), std::declval<const string&>(),
                                                                           std::declval<const string&>())));

TEST(HttpClient_Tests, Errors) {
    Script& s = new_script();
    s.answer = [](int n, const std::string&) -> std::pair<std::string, bool> {
        switch (n) {
            case 0: return {"HTTP/1.1 200 OK\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n", true};
            case 1: return {"HTTP/1.1 200 OK\r\nX: " + std::string(300, 'x') + "\r\n\r\n", true};
            case 2: return {"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nhalf.", true};
            case 3: return {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\nhello\r\n0\r\n\r\n", true};
            default: return {"HTTP/1.1 OK\r\n\r\n", true};
        }
    };
    auto c = client_of(s);
    c.max_response_header_bytes = 200;
    auto bad_url = c.get("not a url");
    ASSERT_FALSE(bad_url);
    EXPECT_EQ(bad_url.error().code(), net::errc::invalid_url);
    auto ftp = c.get("ftp://example.com/");   // https is spoken since TLS (https.cpp)
    ASSERT_FALSE(ftp);
    EXPECT_EQ(ftp.error().code(), net::errc::unsupported_scheme);
    EXPECT_EQ(text(ftp.error().message()).substr(0, 27), "GET ftp://example.com/: uns") << text(ftp.error().message());
    auto both = c.get("http://h/");
    ASSERT_FALSE(both);
    EXPECT_EQ(both.error().code(), net::errc::malformed_response);
    auto large = c.get("http://h/");
    ASSERT_FALSE(large);
    EXPECT_EQ(large.error().code(), net::errc::header_too_large);
    auto cut = c.get("http://h/");
    ASSERT_TRUE(cut);
    auto body = cut->text();
    ASSERT_FALSE(body);
    EXPECT_EQ(body.error().code(), io::errc::unexpected_eof);
    auto lf = c.get("http://h/");
    ASSERT_TRUE(lf);
    auto lf_body = lf->text();
    ASSERT_FALSE(lf_body);
    EXPECT_EQ(lf_body.error().code(), net::errc::malformed_response);
    auto garbage = c.get("http://h/");
    ASSERT_FALSE(garbage);
    EXPECT_EQ(garbage.error().code(), net::errc::malformed_response);
}

TEST(HttpClient_Tests, Timeout) {
    Script& s = new_script();
    s.answer = [](int, const std::string&) -> std::pair<std::string, bool> {
        return {"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nhalf.", false};   // never finishes
    };
    auto c = client_of(s);
    c.timeout = 200ms;
    auto r = c.get("http://h/");
    ASSERT_TRUE(r);
    auto body = r->text();
    ASSERT_FALSE(body);
    EXPECT_TRUE(body.error().is_timeout()) << text(body.error().message());
    c.timeout = duration::zero();
    c.response_header_timeout = 100ms;
    s.answer = [](int, const std::string&) -> std::pair<std::string, bool> { return {"", false}; };   // no answer at all
    auto silent = c.get("http://h/");
    ASSERT_FALSE(silent);
    EXPECT_TRUE(silent.error().is_timeout());
}

TEST(HttpClient_Tests, BodiesSent) {
    Script& s = new_script();
    s.answer = [](int, const std::string& req) { return ok(req); };
    auto c = client_of(s);
    net::http::request chunked("PUT", "http://h/up");
    chunked.set_body(io::reader(make_tracked<io::buffer>("streamed")));
    auto a = c.send(chunked);
    ASSERT_TRUE(a);
    auto seen = text(*a->text());
    EXPECT_NE(seen.find("Transfer-Encoding: chunked\r\n\r\n8\r\nstreamed\r\n0\r\n\r\n"), std::string::npos) << seen;
    net::http::request sized("PUT", "http://h/up");
    sized.set_body(io::reader(make_tracked<io::buffer>("streamed")), 8);
    auto b = c.send(sized);
    ASSERT_TRUE(b);
    seen = text(*b->text());
    EXPECT_NE(seen.find("Content-Length: 8\r\n\r\nstreamed"), std::string::npos) << seen;
    net::http::request bytes("POST", "http://h/");
    vector<byte> v(3, byte('z'));
    bytes.set_body(v).set_header("X-A", "1");
    seen = text(*c.send(bytes)->text());
    EXPECT_NE(seen.find("X-A: 1\r\n"), std::string::npos) << seen;
    EXPECT_NE(seen.find("Content-Length: 3\r\n\r\nzzz"), std::string::npos) << seen;
    net::http::request empty("POST", "http://h/");
    seen = text(*c.send(empty)->text());
    EXPECT_NE(seen.find("Content-Length: 0\r\n"), std::string::npos) << seen;
}

// Found by the fuzzer of the fields (tests/net/http/fuzz/http_fields_fuzz.cpp):
// the method went to the request line as given, so "GET / HTTP/1.1\r\nX: a"
// wrote a head of the caller's making (request splitting). A method that is
// not a token, a name that is not one, a value with CR, LF or NUL: the send
// is std::errc::invalid_argument, the part named, and no connection is
// dialed — not a byte of the request reaches a socket; no exception
TEST(HttpClient_Tests, RequestsThatWouldSplit) {
    Script& s = new_script();
    s.answer = [](int, const std::string&) { return ok("fine"); };
    auto c = client_of(s);
    auto refused = [&](const net::http::request& req, const std::string& what) {
        auto r = c.send(req);
        ASSERT_FALSE(r) << what;
        EXPECT_EQ(r.error().code(), std::errc::invalid_argument) << what;
        EXPECT_NE(text(r.error().message()).find(what), std::string::npos) << text(r.error().message());
    };
    refused(net::http::request("GET\r\nX: y", "http://h/"), "invalid method: GET\\x0D\\x0AX: y");
    refused(net::http::request("GET / HTTP/1.1\r\nX: a", "http://h/"), "invalid method");
    refused(net::http::request("GET", "http://h/").set_header("X-Foo", "1\r\nInjected: yes"), "invalid header value: X-Foo");
    refused(net::http::request("GET", "http://h/").set_header("X-Foo", "1\n folded"), "invalid header value: X-Foo");
    refused(net::http::request("GET", "http://h/").set_header("Bad Name", "x"), "invalid header name: Bad Name");
    refused(net::http::request("GET", "http://h/").add_header("X-Nul", sgcl::string(std::string("a\0b", 3))), "invalid header value: X-Nul");
    refused(net::http::request("POST", "http://h/").set_header("Host", "h\r\nX: 1"), "invalid header value: Host");
    EXPECT_EQ(s.dials, 0);
    EXPECT_TRUE(s.received.empty());
    // the async form the same, and the client goes on as before
    auto a = async::spawn(c.async_send(net::http::request("GET\n", "http://h/"))).wait();
    ASSERT_FALSE(a);
    EXPECT_EQ(a.error().code(), std::errc::invalid_argument);
    EXPECT_EQ(s.dials, 0);
    auto fine = c.get("http://h/x");
    ASSERT_TRUE(fine) << fine.error().message();
    EXPECT_EQ(body_of(fine), "fine");
    ASSERT_EQ(s.received.size(), 1u);
    EXPECT_EQ(s.received[0].find("GET /x HTTP/1.1\r\n"), 0u);
}

// A response that comes a byte, two, three or seven at a time reads the
// same: the head found across reads, the chunks and the trailers across
// them, the next response on the same connection after it
TEST(HttpClient_Tests, AResponseInPieces) {
    for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(7)}) {
        Script& s = new_script();
        s.piece = piece;
        s.answer = [](int n, const std::string&) -> std::pair<std::string, bool> {
            if (n % 2 == 0) {
                return {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nX-A: 1\r\n\r\n5;x=y\r\nhello\r\n1\r\n \r\n5\r\nworld\r\n0\r\nT: 2\r\n\r\n", false};
            }
            return ok("second");
        };
        auto c = client_of(s);
        auto a = c.get("http://h/");
        ASSERT_TRUE(a) << piece;
        EXPECT_EQ(a->header("X-A"), "1");
        EXPECT_EQ(body_of(a), "hello world") << piece;
        EXPECT_EQ(a->trailers().get("T"), "2");
        auto b = c.get("http://h/");
        ASSERT_TRUE(b);
        EXPECT_EQ(body_of(b), "second") << piece;
        EXPECT_EQ(s.dials, 1);
    }
}

// An idle connection is closed by its timer after idle_timeout, on the
// module's clock, and the next request dials a new one
TEST(HttpClient_Tests, IdleConnectionsExpire) {
    async::manual_clock clock;
    clock.install();
    {
        Script& s = new_script();
        s.answer = [](int, const std::string&) { return ok("x"); };
        auto c = client_of(s);
        c.idle_timeout = 90s;
        EXPECT_EQ(body_of(c.get("http://h/")), "x");
        clock.advance(89s);
        EXPECT_EQ(s.ended.load(), 0);
        clock.advance(2s);
        for (int i = 0; i < 1000 && s.ended.load() == 0; ++i) {
            std::this_thread::sleep_for(1ms);
        }
        EXPECT_EQ(s.ended.load(), 1);                           // the pool closed it
        EXPECT_EQ(body_of(c.get("http://h/")), "x");
        EXPECT_EQ(s.dials, 2);
    }
    clock.uninstall();
}

// follow_redirects off: every redirecting status is the response itself
// (Go's CheckRedirect returning ErrUseLastResponse), its Location and its
// body as they came, one request sent whatever max_redirects is, a POST
// not turned into a GET, the connection kept for the next request once the
// body is read; its Set-Cookie into the jar; a copy keeps the setting, and
// back on the same client follows again
TEST(HttpClient_Tests, RedirectsNotFollowed) {
    Script& s = new_script();
    s.answer = [](int, const std::string& req) -> std::pair<std::string, bool> {
        auto path = req.substr(req.find(' ') + 1, req.find(' ', req.find(' ') + 1) - req.find(' ') - 1);
        if (path == "/end") {
            return ok("end");
        }
        const std::string status = path.substr(1);
        const bool head = req.rfind("HEAD", 0) == 0;
        return {"HTTP/1.1 " + status + " Moved\r\nLocation: /end\r\nSet-Cookie: s" + status + "=1\r\nContent-Length: 3\r\n\r\n" + (head ? "" : "abc"), false};
    };
    auto c = client_of(s);
    c.follow_redirects = false;
    c.max_redirects = 0;   // not reached: nothing is followed
    c.jar = net::http::cookie_jar();
    for (int status : {301, 302, 303, 307, 308}) {
        s.received.clear();
        auto r = c.get(sgcl::string("http://h/" + std::to_string(status)));
        ASSERT_TRUE(r) << status << ": " << text(r.error().message());
        EXPECT_EQ(r->status(), status);
        EXPECT_EQ(r->header("Location"), "/end");
        EXPECT_EQ(r->url().path(), "/" + std::to_string(status));
        EXPECT_EQ(body_of(r), "abc");
        EXPECT_EQ(s.received.size(), 1u) << status;
    }
    EXPECT_EQ(c.jar->size(), 5u);                                 // every 3xx's Set-Cookie kept
    s.received.clear();
    auto posted = c.post("http://h/303", "text/plain", "data");
    ASSERT_TRUE(posted);
    EXPECT_EQ(posted->status(), 303);
    EXPECT_EQ(body_of(posted), "abc");
    ASSERT_EQ(s.received.size(), 1u);
    EXPECT_EQ(s.received[0].substr(0, 9), "POST /303");
    auto head = c.head("http://h/302");
    ASSERT_TRUE(head);
    EXPECT_EQ(head->status(), 302);
    EXPECT_EQ(head->header("Location"), "/end");
    auto copy = c;
    EXPECT_FALSE(copy.follow_redirects);
    auto again = copy.get("http://h/307");
    ASSERT_TRUE(again);
    EXPECT_EQ(again->status(), 307);
    EXPECT_EQ(body_of(again), "abc");
    c.follow_redirects = true;
    c.max_redirects = 10;
    auto followed = c.get("http://h/302");
    ASSERT_TRUE(followed);
    EXPECT_EQ(followed->status(), 200);
    EXPECT_EQ(body_of(followed), "end");
    EXPECT_EQ(s.dials, 1);                                       // one connection throughout
    EXPECT_TRUE(net::http::client().follow_redirects);            // the default
}

// DESIGN 408: the redirect limit at its ends: 0 follows none (the first
// redirect is the error, one request sent), exactly max_redirects followed
// is a response, one more the error; a limit below zero is as 0
TEST(HttpClient_Tests, TheRedirectLimitAtItsEnds) {
    Script& s = new_script();
    s.answer = [](int, const std::string& req) -> std::pair<std::string, bool> {
        // /0 answers, /n redirects to /n-1
        auto path = req.substr(5, req.find(' ', 5) - 5);
        int n = std::stoi(path);
        if (n == 0) {
            return ok("arrived");
        }
        return {"HTTP/1.1 302 Found\r\nLocation: /" + std::to_string(n - 1) + "\r\nContent-Length: 0\r\n\r\n", false};
    };
    auto c = client_of(s);
    for (int most : {0, 1, 3}) {
        c.max_redirects = most;
        s.received.clear();
        auto exact = c.get(sgcl::string("http://h/" + std::to_string(most)));
        ASSERT_TRUE(exact) << most << ": " << text(exact.error().message());
        EXPECT_EQ(body_of(exact), "arrived");
        EXPECT_EQ(exact->url().path(), "/0");
        EXPECT_EQ(s.received.size(), size_t(most) + 1);
        s.received.clear();
        auto past = c.get(sgcl::string("http://h/" + std::to_string(most + 1)));
        ASSERT_FALSE(past) << most;
        EXPECT_EQ(past.error().code(), net::errc::too_many_redirects);
        EXPECT_EQ(s.received.size(), size_t(most) + 1);
    }
    c.max_redirects = -1;
    s.received.clear();
    EXPECT_EQ(body_of(c.get("http://h/0")), "arrived");
    auto none = c.get("http://h/1");
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), net::errc::too_many_redirects);
    EXPECT_EQ(s.received.size(), 2u);
    EXPECT_EQ(s.dials, 1);                                       // the redirects' bodies read: one connection throughout
}

// DESIGN 408: empty bodies on the client's side: a length of 0, chunked
// with no chunk (with a trailer and without), to the close with nothing;
// read whole, as bytes and as a stream; read twice; the connection back to
// the pool after each that has an end of its own
TEST(HttpClient_Tests, EmptyBodies) {
    Script& s = new_script();
    s.answer = [](int n, const std::string&) -> std::pair<std::string, bool> {
        switch (n % 4) {
            case 0: return {"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", false};
            case 1: return {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", false};
            case 2: return {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nT: 1\r\n\r\n", false};
            default: return {"HTTP/1.1 200 OK\r\n\r\n", true};
        }
    };
    auto c = client_of(s);
    for (int round : range(2)) {
        auto length = c.get("http://h/");
        ASSERT_TRUE(length);
        EXPECT_EQ(length->content_length(), 0u);
        EXPECT_TRUE(length->trailers().empty());
        auto bytes = length->bytes();
        ASSERT_TRUE(bytes);
        EXPECT_TRUE(bytes->empty());
        EXPECT_EQ(body_of(length), "");                          // read again: still nothing
        length->close();
        length->close();
        auto chunked = c.get("http://h/");
        ASSERT_TRUE(chunked);
        EXPECT_FALSE(chunked->content_length());
        tracked_ptr block = make_tracked<array<byte, 8>>();
        auto n = chunked->body().read(slice<byte>(block, block->data(), 8));
        ASSERT_TRUE(n);
        EXPECT_EQ(*n, 0u);
        auto trailed = c.get("http://h/");
        ASSERT_TRUE(trailed);
        EXPECT_TRUE(trailed->trailers().empty());               // not read yet
        EXPECT_EQ(body_of(trailed), "");
        EXPECT_EQ(trailed->trailers().get("t"), "1");
        EXPECT_EQ(s.dials, round + 1);                           // the three on one connection
        auto to_close = c.get("http://h/");
        ASSERT_TRUE(to_close);
        EXPECT_FALSE(to_close->content_length());
        EXPECT_EQ(body_of(to_close), "");
    }
    EXPECT_EQ(s.dials, 2);
}

// DESIGN 408: the response's head at max_response_header_bytes exactly is
// read, one byte more is header_too_large; zero is taken as 1
TEST(HttpClient_Tests, TheResponseHeadAtItsLimit) {
    Script& s = new_script();
    static size_t size = 0;
    s.answer = [](int, const std::string&) -> std::pair<std::string, bool> {
        const std::string start = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nX: ";
        return {start + std::string(size - start.size() - 4, 'x') + "\r\n\r\nok", false};
    };
    auto c = client_of(s);
    c.max_response_header_bytes = 200;
    size = 200;
    EXPECT_EQ(body_of(c.get("http://h/")), "ok");
    size = 201;
    auto past = c.get("http://h/");
    ASSERT_FALSE(past);
    EXPECT_EQ(past.error().code(), net::errc::header_too_large);
    c.max_response_header_bytes = 0;
    size = 60;
    auto zero = c.get("http://h/");
    ASSERT_FALSE(zero);
    EXPECT_EQ(zero.error().code(), net::errc::header_too_large);
}

// DESIGN 408: a client moved from is the same client: the pool shared and
// the settings kept, its dial among them (a move of a word-sized handle
// copies the word, as tracked_ptr's does); a client into itself the same
TEST(HttpClient_Tests, AMovedFromClientIsTheSameClient) {
    Script& s = new_script();
    s.answer = [](int n, const std::string&) { return ok(std::to_string(n)); };
    auto c = client_of(s);
    c.max_redirects = 4;
    net::http::client to(std::move(c));
    EXPECT_EQ(body_of(to.get("http://h/")), "0");
    EXPECT_EQ(body_of(c.get("http://h/")), "1");                // through its own dial, from the shared pool
    EXPECT_EQ(s.dials, 1);
    EXPECT_EQ(c.max_redirects, 4);
    ASSERT_EQ(c.tls.alpn.size(), 1u);
    EXPECT_EQ(c.tls.alpn[0], "http/1.1");
    net::http::client assigned;
    assigned = std::move(c);
    EXPECT_EQ(body_of(c.get("http://h/")), "2");
    EXPECT_EQ(body_of(assigned.get("http://h/")), "3");
    auto& self = assigned;
    assigned = self;
    EXPECT_EQ(body_of(assigned.get("http://h/")), "4");
    EXPECT_EQ(s.dials, 1);
    // the request's and the response's handles the same: a moved-from one
    // is the same message
    net::http::request req("GET", "http://h/");
    net::http::request moved(std::move(req));
    EXPECT_EQ(req.method(), "GET");
    auto res = c.send(req);
    ASSERT_TRUE(res);
    net::http::response kept(std::move(*res));
    EXPECT_EQ(res->status(), 200);
    EXPECT_EQ(body_of(res), "5");
    EXPECT_EQ(body_of(kept), "");                                // the same body, read already
}

// DESIGN 408: requests at their ends: an empty method or URL, a URL
// without a path, the accessors of a request to send, a request sent again
// after a change
TEST(HttpClient_Tests, RequestsAtTheirEnds) {
    Script& s = new_script();
    s.answer = [](int, const std::string& req) { return ok(req.substr(0, req.find("\r\n"))); };
    auto c = client_of(s);
    net::http::request empty_url("GET", "");
    EXPECT_THROW((void)empty_url.url(), std::invalid_argument);
    auto r = c.send(empty_url);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), net::errc::invalid_url);
    auto no_method = c.send(net::http::request("", "http://h/"));
    ASSERT_FALSE(no_method);
    EXPECT_EQ(no_method.error().code(), std::errc::invalid_argument);
    EXPECT_EQ(body_of(c.get("http://h")), "GET / HTTP/1.1");      // no path: "/"
    EXPECT_EQ(body_of(c.get("http://h?q")), "GET /?q HTTP/1.1");
    net::http::request req("GET", "http://h/a");
    EXPECT_EQ(req.proto(), "HTTP/1.1");
    EXPECT_EQ(req.header("x"), "");
    EXPECT_EQ(req.path_value("id"), "");
    EXPECT_EQ(req.query("q"), "");
    EXPECT_EQ(req.cookie(""), "");
    EXPECT_FALSE(req.content_length());
    EXPECT_TRUE(req.trailers().empty());
    EXPECT_EQ(*req.text(), "");                                   // a request to send has no body to read
    EXPECT_TRUE(req.bytes()->empty());
    EXPECT_FALSE(req.stop().stop_requested());
    req.set_body("").set_header("X", "1");
    EXPECT_EQ(body_of(c.send(req)), "GET /a HTTP/1.1");
    req.set_header("X", "2");                                     // the same request again
    EXPECT_EQ(body_of(c.send(req)), "GET /a HTTP/1.1");
    EXPECT_NE(s.received.back().find("X: 2\r\n"), std::string::npos);
    EXPECT_NE(s.received.back().find("Content-Length: 0\r\n"), std::string::npos);
}

namespace {
    // A stream that gives its text and then fails (or ends, when told to)
    class Failing final : public io::mixin::reader<Failing> {
    public:
        Failing(std::string_view s, bool fail) : _s(s), _fail(fail) {}

        expected<size_t, io::error> read(slice<byte> out) {
            if (_s.empty()) {
                if (_fail) {
                    return unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "failing"));
                }
                return size_t(0);
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
        std::string_view _s;
        bool _fail;
    };
}

// DESIGN 408: a stream body that fails half-way, or gives less or more
// than its length: the send is the stream's error (or unexpected_eof), the
// connection is not kept, and the next request is sent whole on a new one;
// a stream longer than its length sends the length
TEST(HttpClient_Tests, AStreamBodyThatFailsHalfWay) {
    Script& s = new_script();
    s.answer = [](int, const std::string& req) { return ok(req.substr(req.find("\r\n\r\n") + 4)); };
    auto c = client_of(s);
    EXPECT_EQ(body_of(c.get("http://h/")), "");
    EXPECT_EQ(s.dials, 1);
    for (optional<uint64_t> length : {optional<uint64_t>(), optional<uint64_t>(10)}) {
        net::http::request put("PUT", "http://h/");
        put.set_body(io::reader(make_tracked<Failing>("hello", true)), length);
        auto r = c.send(put);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), std::errc::io_error) << text(r.error().message());
    }
    net::http::request shorter("PUT", "http://h/");
    shorter.set_body(io::reader(make_tracked<Failing>("hello", false)), 10);
    auto cut = c.send(shorter);
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), io::errc::unexpected_eof) << text(cut.error().message());
    const int dials = s.dials;
    EXPECT_EQ(body_of(c.get("http://h/")), "");                 // a new connection: none of the broken ones kept
    EXPECT_EQ(s.dials, dials + 1);
    net::http::request longer("PUT", "http://h/");
    longer.set_body(io::reader(make_tracked<Failing>("hello world", false)), 5);
    EXPECT_EQ(body_of(c.send(longer)), "hello");
    net::http::request nothing("PUT", "http://h/");
    nothing.set_body(io::reader(make_tracked<Failing>("", false)));
    auto empty = c.send(nothing);
    ASSERT_TRUE(empty);
    EXPECT_EQ(body_of(empty), "0\r\n\r\n");                      // chunked with no chunk
    net::http::request zero("PUT", "http://h/");
    zero.set_body(io::reader(make_tracked<Failing>("", false)), 0);
    EXPECT_EQ(body_of(c.send(zero)), "");
    EXPECT_NE(s.received.back().find("Content-Length: 0\r\n"), std::string::npos) << s.received.back();
}
