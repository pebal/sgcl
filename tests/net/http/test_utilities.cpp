//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: the test utilities (test.h) — test_server plain, over TLS on its own
// certificate, HTTP/2 by ALPN and h2c, its routes, its record of requests,
// close in its forms, a moved-from one; response_recorder with a plain and
// a task handler, flushes, trailers, informational responses, routing
// through a server; test_request's parts and body. And what the writer
// gained for them: trailers and informational responses on the wire of
// HTTP/1.1 (raw bytes) and of HTTP/2.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <string>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    void hello(net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("hello\n");
    }

    // Bytes sent over a new connection, everything read back to the
    // server's close or `until` seen
    std::string talk(const net::endpoint& at, const std::string& bytes, const std::string& until = std::string()) {
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
            if (!until.empty() && out.find(until) != std::string::npos) {
                break;
            }
        }
        (void)c.close();
        return out;
    }
}

TEST(HttpTestServer_Tests, PlainHandler) {
    net::http::test_server ts(hello);
    EXPECT_TRUE(text(ts.url()).starts_with("http://127.0.0.1:"));
    EXPECT_GT(ts.endpoint().port(), 0);
    EXPECT_FALSE(ts.certificate());
    auto r = ts.client().get(ts.url() + "/x");
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->status(), 200);
    EXPECT_EQ(r->proto(), "HTTP/1.1");
    EXPECT_EQ(*r->text(), "hello\n");
    EXPECT_TRUE(ts.requests().empty());   // not kept unless asked
}

TEST(HttpTestServer_Tests, TaskHandlerEchoes) {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) -> async::task<> {
        auto body = co_await r.async_text();
        w.write(body ? *body : sgcl::string("?"));
    });
    auto r = ts.client().post(ts.url(), "text/plain", "ping");
    ASSERT_TRUE(r);
    EXPECT_EQ(*r->text(), "ping");
}

TEST(HttpTestServer_Tests, RoutesOfAServer) {
    net::http::server s;
    s.route("GET /users/{id}", [](net::http::request r, net::http::response_writer w) {
        w.write("user " + r.path_value("id"));
    });
    s.read_header_timeout = 3s;
    net::http::test_server ts(s);
    EXPECT_EQ(*ts.client().get(ts.url() + "/users/7")->text(), "user 7");
    EXPECT_EQ(ts.client().get(ts.url() + "/none")->status(), 404);
    EXPECT_EQ(ts.client().post(ts.url() + "/users/7", "text/plain", "")->status(), 405);
    // a route added while it serves
    ts.server().route("/late", hello);
    EXPECT_EQ(*ts.client().get(ts.url() + "/late")->text(), "hello\n");
}

TEST(HttpTestServer_Tests, TlsOnItsOwnCertificate) {
    net::http::test_server ts(hello, {.tls = true});
    EXPECT_TRUE(text(ts.url()).starts_with("https://127.0.0.1:"));
    ASSERT_TRUE(ts.certificate());
    EXPECT_EQ(ts.certificate()->subject().to_string(), "CN=sgcl test CA");
    auto r = ts.client().get(ts.url());
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->proto(), "HTTP/1.1");
    EXPECT_EQ(*r->text(), "hello\n");
    // a client of the system's roots does not trust it
    net::http::client plain;
    plain.proxy = net::http::proxy();
    EXPECT_FALSE(plain.get(ts.url()));
    // localhost is among the leaf's names too
    auto by_name = ts.client().get(sgcl::string("https://localhost:") + sgcl::string(std::to_string(ts.endpoint().port())) + "/");
    ASSERT_TRUE(by_name) << text(by_name.error().message());
}

TEST(HttpTestServer_Tests, Http2OverTls) {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) {
        w.write(r.proto());
    }, {.tls = true, .http2 = true});
    auto r = ts.client().get(ts.url());
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->proto(), "HTTP/2.0");
    EXPECT_EQ(*r->text(), "HTTP/2.0");
}

TEST(HttpTestServer_Tests, H2c) {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) {
        w.write(r.proto());
    }, {.http2 = true});
    EXPECT_TRUE(text(ts.url()).starts_with("http://"));
    auto r = ts.client().get(ts.url());
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->proto(), "HTTP/2.0");
    // HTTP/1.1 beside it on the same port
    net::http::client one;
    one.proxy = net::http::proxy();
    auto old = one.get(ts.url());
    ASSERT_TRUE(old);
    EXPECT_EQ(*old->text(), "HTTP/1.1");
}

TEST(HttpTestServer_Tests, KeepsRequests) {
    net::http::test_server ts(hello, {.keep_requests = true});
    auto c = ts.client();
    net::http::request a("PUT", ts.url() + "/a?q=1");
    a.set_header("X-Test", "one");
    a.set_body("body");
    ASSERT_TRUE(c.send(a));
    ASSERT_TRUE(c.get(ts.url() + "/b"));
    auto got = ts.requests();
    ASSERT_EQ(got.size(), 2u);
    EXPECT_EQ(got[0].method(), "PUT");
    EXPECT_EQ(got[0].url().path(), "/a");
    EXPECT_EQ(got[0].query("q"), "1");
    EXPECT_EQ(got[0].header("X-Test"), "one");
    EXPECT_EQ(got[0].content_length(), optional<uint64_t>(4));
    EXPECT_EQ(got[1].url().path(), "/b");
}

TEST(HttpTestServer_Tests, CloseAndAfter) {
    net::http::test_server ts(hello);
    auto c = ts.client();
    ASSERT_TRUE(c.get(ts.url()));
    auto url = ts.url();
    ts.close();
    EXPECT_FALSE(c.get(url));
    ts.close();   // a second does nothing
    EXPECT_EQ(ts.url(), url);
}

TEST(HttpTestServer_Tests, CloseWaitsForTheHandler) {
    std::atomic<bool> finished = false;
    async::event started;
    net::http::test_server ts([&](net::http::request r, net::http::response_writer w) -> async::task<> {
        started.set();
        co_await r.stop().stopped();   // the close stops it
        co_await async::sleep(50ms);
        finished = true;
        w.write("late");
    });
    std::thread t([&, url = std::string(text(ts.url()))] {
        net::http::client c;
        c.proxy = net::http::proxy();
        (void)c.get(sgcl::string(url));
    });
    started.wait();
    ts.close();
    EXPECT_TRUE(finished.load());
    t.join();
}

TEST(HttpTestServer_Tests, MovedFrom) {
    net::http::test_server a(hello);
    auto url = a.url();
    net::http::test_server b(std::move(a));
    EXPECT_EQ(a.url(), "");
    EXPECT_EQ(a.endpoint().port(), 0);
    EXPECT_THROW((void)a.client(), invalid_argument);
    EXPECT_THROW((void)a.requests(), invalid_argument);
    a.close();   // nothing
    EXPECT_EQ(b.url(), url);
    EXPECT_TRUE(b.client().get(url));
    // a move-assignment closes the server it replaces
    net::http::test_server c(hello);
    auto old = c.url();
    auto client = c.client();
    c = std::move(b);
    EXPECT_FALSE(client.get(old));
    EXPECT_TRUE(c.client().get(url));
}

TEST(HttpTestServer_Tests, DestructorCloses) {
    net::endpoint at;
    {
        net::http::test_server ts(hello);
        at = ts.endpoint();
        ASSERT_TRUE(net::tcp::connect(at));
    }
    for (int i : range(50)) {   // the listener closed at once; the port refuses
        (void)i;
        if (!net::tcp::connect(at)) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }
    EXPECT_FALSE(net::tcp::connect(at));
}

TEST(HttpTestServer_Tests, CloseClientConnections) {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) -> async::task<> {
        for (int i : range(500)) {   // until the connection is gone
            (void)i;
            w.write("part");
            if (!co_await w.async_flush()) {
                break;
            }
            co_await async::sleep(10ms);
        }
    });
    net::http::client c = ts.client();
    auto r = c.get(ts.url());
    ASSERT_TRUE(r);
    ts.close_client_connections();
    auto body = r->text();
    EXPECT_FALSE(body);   // the stream cut short
    ts.close();
}

TEST(HttpTestServer_Tests, AsyncCloseInATask) {
    auto done = [](void (*h)(net::http::request, net::http::response_writer)) -> async::task<bool> {
        net::http::test_server ts(h);
        auto r = co_await ts.client().async_get(ts.url());
        bool ok = r && r->status() == 200;
        co_await ts.async_close();
        co_await ts.async_close();   // the second: nothing
        co_return ok;
    }(hello).wait();
    EXPECT_TRUE(done);
}

// --- response_recorder ------------------------------------------------------

TEST(HttpRecorder_Tests, PlainHandler) {
    net::http::response_recorder rec;
    EXPECT_EQ(rec.status(), 200);
    EXPECT_EQ(rec.body(), "");
    hello(net::http::test_request("GET", "/"), rec.writer());
    EXPECT_EQ(rec.status(), 200);
    EXPECT_EQ(rec.header("Content-Type"), "text/plain");
    EXPECT_EQ(rec.body(), "hello\n");
    EXPECT_EQ(rec.flushes(), 0);
    EXPECT_TRUE(rec.informational().empty());
    EXPECT_TRUE(rec.trailers().empty());
}

TEST(HttpRecorder_Tests, TaskHandlerFlushes) {
    net::http::response_recorder rec;
    auto h = [](net::http::request r, net::http::response_writer w) -> async::task<> {
        w.set_status(201);
        w.set_header("X-A", "1");
        w.write("one,");
        (void)co_await w.async_flush();
        w.set_header("X-B", "2");   // after the head went: not in it
        w.write(*co_await r.async_text());
        (void)co_await w.async_flush();
        w.write(",three");
        w.trailers().set("X-Sum", "9");
    };
    h(net::http::test_request("POST", "/", "two"), rec.writer()).wait();
    EXPECT_EQ(rec.status(), 201);
    EXPECT_EQ(rec.header("X-A"), "1");
    EXPECT_EQ(rec.header("X-B"), "");
    EXPECT_EQ(rec.flushes(), 2);
    EXPECT_EQ(rec.body(), "one,two,three");
    EXPECT_EQ(rec.trailers().get("X-Sum"), "9");
    EXPECT_TRUE(rec.writer().header_sent());
}

TEST(HttpRecorder_Tests, ErrorAndInformational) {
    net::http::response_recorder rec;
    auto w = rec.writer();
    net::http::headers hints;
    hints.add("Link", "</a.css>; rel=preload");
    ASSERT_TRUE(w.send_informational(103, hints));
    EXPECT_THROW((void)w.send_informational(101), invalid_argument);
    EXPECT_THROW((void)w.send_informational(200), invalid_argument);
    w.write("dropped");
    w.error(404);
    EXPECT_EQ(rec.status(), 404);
    EXPECT_EQ(rec.body(), "Not Found\n");
    EXPECT_EQ(rec.header("Content-Type"), "text/plain; charset=utf-8");
    auto info = rec.informational();
    ASSERT_EQ(info.size(), 1u);
    EXPECT_EQ(info[0].first, 103);
    EXPECT_EQ(info[0].second.get("Link"), "</a.css>; rel=preload");
    EXPECT_FALSE(w.hijack());   // nothing to take over
    ASSERT_TRUE(w.flush());
    EXPECT_FALSE(w.send_informational(103));   // after the head
}

TEST(HttpRecorder_Tests, ServeThroughRoutes) {
    net::http::server s;
    s.route("GET /items/{id}", [](net::http::request r, net::http::response_writer w) {
        w.write("item " + r.path_value("id") + " " + r.query("v"));
    });
    net::http::response_recorder rec;
    rec.serve(s, net::http::test_request("GET", "/items/42?v=x"));
    EXPECT_EQ(rec.body(), "item 42 x");
    net::http::response_recorder missing;
    missing.serve(s, net::http::test_request("GET", "/nothing"));
    EXPECT_EQ(missing.status(), 404);
    net::http::response_recorder wrong;
    wrong.serve(s, net::http::test_request("DELETE", "/items/1"));
    EXPECT_EQ(wrong.status(), 405);
    EXPECT_EQ(wrong.header("Allow"), "GET, HEAD");
    // a handler's throw comes out
    s.route("/throws", [](net::http::request, net::http::response_writer) {
        throw std::runtime_error("bad");
    });
    net::http::response_recorder thrown;
    EXPECT_THROW(thrown.serve(s, net::http::test_request("GET", "/throws")), std::runtime_error);
}

TEST(HttpRecorder_Tests, CopiesShare) {
    net::http::response_recorder a;
    auto b = a;
    a.writer().write("x");
    EXPECT_EQ(b.body(), "x");
}

TEST(HttpRecorder_Tests, HeadAndBodiless) {
    net::http::response_recorder rec;
    auto w = rec.writer();
    w.set_status(204);
    w.write("ignored");
    EXPECT_EQ(rec.body(), "");
}

// --- test_request -------------------------------------------------------------

TEST(HttpTestRequest_Tests, Parts) {
    auto r = net::http::test_request("GET", "/path?x=1");
    EXPECT_EQ(r.method(), "GET");
    EXPECT_EQ(r.url().to_string(), "http://example.com/path?x=1");
    EXPECT_EQ(r.header("Host"), "example.com");
    EXPECT_EQ(r.query("x"), "1");
    EXPECT_EQ(r.proto(), "HTTP/1.1");
    EXPECT_EQ(r.remote_endpoint().to_string(), "192.0.2.1:1234");
    EXPECT_FALSE(r.content_length());
    EXPECT_FALSE(r.stop().stop_requested());
    EXPECT_FALSE(r.tls());
    EXPECT_EQ(*r.text(), "");
    auto abs = net::http::test_request("POST", "https://api.test:8443/v1", "payload");
    EXPECT_EQ(abs.header("Host"), "api.test:8443");
    EXPECT_EQ(abs.content_length(), optional<uint64_t>(7));
    EXPECT_EQ(*abs.text(), "payload");
    EXPECT_EQ(*abs.text(), "");   // read once
    auto rel = net::http::test_request("GET", "no-slash");
    EXPECT_EQ(rel.url().path(), "/no-slash");
    EXPECT_THROW((void)net::http::test_request("GET", ""), invalid_argument);
    EXPECT_THROW((void)net::http::test_request("GET", "http://[bad/"), invalid_argument);
}

TEST(HttpTestRequest_Tests, LargeBodyAsStream) {
    std::string big(1 << 20, 'z');
    auto r = net::http::test_request("PUT", "/", sgcl::string(big));
    auto all = r.body().read_all();
    ASSERT_TRUE(all);
    EXPECT_EQ(all->size(), big.size());
}

// --- the writer's trailers and informational responses on the wire ------------

TEST(HttpWriterTrailers_Tests, WholeBodyBecomesChunkedWithTrailers) {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) {
        w.trailers().set("X-Checksum", "abc");
        w.write("data");
    });
    auto raw = talk(ts.endpoint(), "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(raw.find("Transfer-Encoding: chunked\r\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("Trailer: X-Checksum\r\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("4\r\ndata\r\n0\r\nX-Checksum: abc\r\n\r\n"), std::string::npos) << raw;
    auto r = ts.client().get(ts.url());
    ASSERT_TRUE(r);
    EXPECT_EQ(*r->text(), "data");
    EXPECT_EQ(r->trailers().get("X-Checksum"), "abc");
}

TEST(HttpWriterTrailers_Tests, AfterFlushAndForbiddenNames) {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) -> async::task<> {
        w.write("a");
        (void)co_await w.async_flush();
        w.write("b");
        w.trailers().add("X-Late", "1");
        w.trailers().add("Content-Length", "99");   // framing: left out
        w.trailers().add("Bad Name", "x");          // not a token: left out
        w.trailers().add("X-Ctl", "a\r\nb");        // a control: left out
    });
    auto raw = talk(ts.endpoint(), "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(raw.find("0\r\nX-Late: 1\r\n\r\n"), std::string::npos) << raw;
    EXPECT_EQ(raw.find("99"), std::string::npos) << raw;
    auto r = ts.client().get(ts.url());
    ASSERT_TRUE(r);
    EXPECT_EQ(*r->text(), "ab");
    EXPECT_EQ(r->trailers().get("X-Late"), "1");
    EXPECT_EQ(r->trailers().size(), 1u);
}

TEST(HttpWriterTrailers_Tests, Http10GetsNone) {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) {
        w.trailers().set("X-T", "1");
        w.write("data");
    });
    auto raw = talk(ts.endpoint(), "GET / HTTP/1.0\r\n\r\n");
    EXPECT_NE(raw.find("Content-Length: 4\r\n"), std::string::npos) << raw;
    EXPECT_EQ(raw.find("X-T"), std::string::npos) << raw;
    auto head = talk(ts.endpoint(), "HEAD / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(head.find("X-T: 1"), std::string::npos) << head;
}

TEST(HttpWriterTrailers_Tests, OverHttp2) {
    for (bool tls : {false, true}) {
        net::http::test_server ts([](net::http::request, net::http::response_writer w) -> async::task<> {
            w.write("x");
            w.trailers().set("Grpc-Status", "0");
            co_return;
        }, {.tls = tls, .http2 = true});
        auto r = ts.client().get(ts.url());
        ASSERT_TRUE(r) << text(r.error().message());
        EXPECT_EQ(r->proto(), "HTTP/2.0");
        EXPECT_EQ(*r->text(), "x");
        EXPECT_EQ(r->trailers().get("grpc-status"), "0");
        // after a flush, and with no body at all
        net::http::test_server flushed([](net::http::request r, net::http::response_writer w) -> async::task<> {
            if (r.url().path() == "/flush") {
                w.write("y");
                (void)co_await w.async_flush();
            }
            w.trailers().set("X-T", "2");
        }, {.tls = tls, .http2 = true});
        auto f = flushed.client().get(flushed.url() + "/flush");
        ASSERT_TRUE(f);
        EXPECT_EQ(*f->text(), "y");
        EXPECT_EQ(f->trailers().get("x-t"), "2");
        auto e = flushed.client().get(flushed.url() + "/empty");
        ASSERT_TRUE(e);
        EXPECT_EQ(*e->text(), "");
        EXPECT_EQ(e->trailers().get("x-t"), "2");
    }
}

TEST(HttpWriterInformational_Tests, EarlyHintsOnHttp11) {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::headers hints;
        hints.add("Link", "</style.css>; rel=preload; as=style");
        auto sent = co_await w.async_send_informational(103, hints);
        w.write(sent ? "sent" : "failed");
    });
    auto raw = talk(ts.endpoint(), "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_TRUE(raw.starts_with("HTTP/1.1 103 Early Hints\r\nLink: </style.css>; rel=preload; as=style\r\n\r\nHTTP/1.1 200 OK\r\n")) << raw;
    EXPECT_NE(raw.find("sent"), std::string::npos);
    auto old = talk(ts.endpoint(), "GET / HTTP/1.0\r\n\r\n");
    EXPECT_TRUE(old.starts_with("HTTP/1.1 200 OK\r\n")) << old;   // none to HTTP/1.0
    auto r = ts.client().get(ts.url());   // the client passes over it
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status(), 200);
    EXPECT_EQ(*r->text(), "sent");
}

TEST(HttpWriterInformational_Tests, RefusedCases) {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) -> async::task<> {
        std::string out;
        net::http::headers bad;
        bad.add("X-Bad", "a\nb");
        auto b = co_await w.async_send_informational(103, bad);
        out += b ? "1" : "0";
        w.write("x");
        (void)co_await w.async_flush();
        auto late = co_await w.async_send_informational(103);
        out += late ? "1" : "0";
        w.write(sgcl::string(out));
        (void)r;
    });
    auto r = ts.client().get(ts.url());
    ASSERT_TRUE(r);
    EXPECT_EQ(*r->text(), "x00");
}

TEST(HttpWriterInformational_Tests, OverHttp2) {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::headers hints;
        hints.add("Link", "</a.js>; rel=preload");
        auto sent = co_await w.async_send_informational(103, hints);
        w.write(sent ? "sent" : "failed");
    }, {.http2 = true});
    auto r = ts.client().get(ts.url());
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->proto(), "HTTP/2.0");
    EXPECT_EQ(*r->text(), "sent");
}

// --- request::set_stop and set_url (the client's) ------------------------------

TEST(HttpClientStop_Tests, StopCancelsTheExchange) {
    for (bool h2 : {false, true}) {
        net::http::test_server ts([](net::http::request r, net::http::response_writer w) -> async::task<> {
            if (r.url().path() == "/body") {
                w.write("first");
                (void)co_await w.async_flush();
            }
            co_await async::sleep(3s);
            w.write("late");
        }, {.http2 = h2});
        // stopped before the send: at once
        async::stop_source early;
        early.request_stop();
        net::http::request a("GET", ts.url());
        a.set_stop(early.token());
        auto ra = ts.client().send(a);
        ASSERT_FALSE(ra);
        EXPECT_EQ(ra.error().code(), std::errc::operation_canceled);
        // stopped while it waits for the head
        async::stop_source later;
        later.stop_after(100ms);
        net::http::request b("GET", ts.url());
        b.set_stop(later.token());
        auto start = sgcl::clock::now();
        auto rb = ts.client().send(b);
        ASSERT_FALSE(rb) << h2;
        EXPECT_EQ(rb.error().code(), std::errc::operation_canceled) << h2 << " " << text(rb.error().message());
        EXPECT_LT(sgcl::clock::now() - start, 1500ms);
        // stopped while the body is read
        async::stop_source mid;
        net::http::request c("GET", ts.url() + "/body");
        c.set_stop(mid.token());
        auto rc = ts.client().send(c);
        ASSERT_TRUE(rc);
        char buf[8];
        ASSERT_TRUE(rc->body().read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf)));
        mid.stop_after(50ms);
        start = sgcl::clock::now();
        EXPECT_FALSE(rc->text());
        EXPECT_LT(sgcl::clock::now() - start, 1500ms);
        // a stop that never comes changes nothing
        async::stop_source never;
        net::http::test_server quick(hello, {.http2 = h2});
        net::http::request d("GET", quick.url());
        d.set_stop(never.token());
        auto rd = quick.client().send(d);
        ASSERT_TRUE(rd);
        EXPECT_EQ(*rd->text(), "hello\n");
        EXPECT_EQ(d.stop(), never.token());
    }
}

TEST(HttpClientStop_Tests, SetUrl) {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) {
        w.write(r.url().path() + " " + r.header("X-Kept"));
    });
    net::http::request r("GET", "not a url");
    r.set_header("X-Kept", "yes");
    EXPECT_FALSE(ts.client().send(r));
    r.set_url(ts.url() + "/there");
    EXPECT_EQ(r.url().path(), "/there");
    EXPECT_EQ(*ts.client().send(r)->text(), "/there yes");
    r.set_url("");
    auto bad = ts.client().send(r);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::invalid_url);
}
