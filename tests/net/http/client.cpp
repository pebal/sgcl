//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
        EXPECT_EQ(*r->text(), "body " + std::to_string(i));
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
    EXPECT_EQ(*c.get("http://h/")->text(), "a");
    auto r = c.get("http://h/");
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->content_length());
    EXPECT_EQ(*r->text(), "until the close");
    EXPECT_EQ(s.dials, 2);
}

TEST(HttpClient_Tests, RetryOnAConnectionThePoolKeptTooLong) {
    Script& s = new_script();
    // the server answers each request and then closes, without saying so
    s.answer = [](int n, const std::string&) { return ok(std::to_string(n), "", true); };
    auto c = client_of(s);
    EXPECT_EQ(*c.get("http://h/")->text(), "0");
    auto r = c.get("http://h/");                          // on the dead connection first
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(*r->text(), "1");
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
    EXPECT_EQ(*a->text(), "hello world");
    EXPECT_EQ(a->trailers().get("x-sum"), "11");
    auto b = c.get("http://h/");
    ASSERT_TRUE(b);
    EXPECT_EQ(b->status(), 201);
    EXPECT_EQ(*b->text(), "ok");
    auto h = c.head("http://h/");
    ASSERT_TRUE(h);
    EXPECT_EQ(h->content_length(), 1000u);
    EXPECT_EQ(*h->text(), "");
    EXPECT_EQ(*c.get("http://h/")->text(), "");
    auto m = c.get("http://h/");
    EXPECT_EQ(m->status(), 304);
    EXPECT_EQ(*m->text(), "");
    EXPECT_EQ(*c.get("http://h/")->text(), "GET");
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
    EXPECT_EQ(*res->text(), "end");
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
    auto https = c.get("https://example.com/");
    ASSERT_FALSE(https);
    EXPECT_EQ(https.error().code(), net::errc::unsupported_scheme);
    EXPECT_EQ(text(https.error().message()).substr(0, 29), "GET https://example.com/: uns") << text(https.error().message());
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
    bytes.set_body(v).set_header("X-A", "1\r\nInjected: yes");
    seen = text(*c.send(bytes)->text());
    EXPECT_NE(seen.find("X-A: 1  Injected: yes\r\n"), std::string::npos) << seen;   // CR and LF made spaces
    EXPECT_NE(seen.find("Content-Length: 3\r\n\r\nzzz"), std::string::npos) << seen;
    net::http::request empty("POST", "http://h/");
    seen = text(*c.send(empty)->text());
    EXPECT_NE(seen.find("Content-Length: 0\r\n"), std::string::npos) << seen;
    EXPECT_THROW(net::http::request("GET", "http://h/").set_header("Bad Name", "x"), std::invalid_argument);
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
        EXPECT_EQ(*a->text(), "hello world") << piece;
        EXPECT_EQ(a->trailers().get("T"), "2");
        auto b = c.get("http://h/");
        ASSERT_TRUE(b);
        EXPECT_EQ(*b->text(), "second") << piece;
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
        EXPECT_EQ(*c.get("http://h/")->text(), "x");
        clock.advance(89s);
        EXPECT_EQ(s.ended.load(), 0);
        clock.advance(2s);
        for (int i = 0; i < 1000 && s.ended.load() == 0; ++i) {
            std::this_thread::sleep_for(1ms);
        }
        EXPECT_EQ(s.ended.load(), 1);                           // the pool closed it
        EXPECT_EQ(*c.get("http://h/")->text(), "x");
        EXPECT_EQ(s.dials, 2);
    }
    clock.uninstall();
}
