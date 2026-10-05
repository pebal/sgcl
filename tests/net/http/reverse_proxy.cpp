//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: reverse_proxy against backends started with test_server (and raw
// ones written in bytes): the URL joined, every header rule (hop-by-hop both
// ways, X-Forwarded-*, Forwarded, Via, the Host policy, TE), absolute-form,
// HEAD, streaming and Server-Sent Events, large bodies both ways, trailers,
// 103 Early Hints, WebSocket and a raw Upgrade, HTTP/2 in front and behind,
// TLS behind, a client gone (the backend's request cancelled), a backend
// down or slow (502, 504), the hooks, the boundaries.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <future>
#include <string>
#include <thread>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // Each request's head as the backend saw it, one field a line
    void echo_head(net::http::request r, net::http::response_writer w) {
        std::string out = text(r.method()) + " " + text(r.url().request_target()) + "\n";
        for (auto f : r.headers()) {
            out += text(f.first) + ": " + text(f.second) + "\n";
        }
        w.set_header("Content-Type", "text/plain");
        w.write(sgcl::string(out));
    }

    // A front server of a proxy to the backend's URL
    net::http::server front(const net::http::reverse_proxy& p) {
        net::http::server s;
        s.route("/", p);
        s.max_body_bytes = 0;
        return s;
    }

    // Bytes sent over a new connection, read back to the close or `until`
    std::string talk(const net::endpoint& at, const std::string& bytes, const std::string& until = std::string(), duration wait = 5s) {
        auto c = *net::tcp::connect(at);
        c.set_deadline(sgcl::clock::now() + wait);
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

    // A backend that answers every connection with the same bytes after
    // reading its head, the heads kept; run on a thread of its own
    struct RawBackend {
        std::string answer;
        std::thread t;
        std::atomic<bool> stop = false;
        std::promise<uint16_t> port_promise;
        uint16_t port = 0;
        std::mutex lock;
        std::vector<std::string> heads;

        explicit RawBackend(std::string a)
        : answer(std::move(a)) {
            t = std::thread([this] {
                auto l = *net::tcp::listen("127.0.0.1:0");
                port_promise.set_value(l.local_endpoint().port());
                while (!stop) {
                    auto c = l.accept();
                    if (!c || stop) {
                        break;
                    }
                    c->set_deadline(sgcl::clock::now() + 5s);
                    std::string head;
                    char buf[4096];
                    while (head.find("\r\n\r\n") == std::string::npos) {
                        auto n = c->read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
                        if (!n || *n == 0) {
                            break;
                        }
                        head.append(buf, *n);
                    }
                    {
                        std::lock_guard<std::mutex> g(lock);
                        heads.push_back(head);
                    }
                    (void)c->write(sgcl::string(answer));
                    (void)c->close();
                }
                (void)l.close();
            });
            port = port_promise.get_future().get();
        }

        ~RawBackend() {
            stop = true;
            (void)net::tcp::connect(net::endpoint(*net::ip_address::parse("127.0.0.1"), port));
            t.join();
        }

        sgcl::string url() const {
            return sgcl::string("http://127.0.0.1:" + std::to_string(port));
        }

        std::string head(size_t i) {
            std::lock_guard<std::mutex> g(lock);
            return i < heads.size() ? heads[i] : std::string();
        }
    };
}

TEST(HttpReverseProxy_Tests, PassesARequestAndItsResponse) {
    net::http::test_server backend(echo_head);
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())));
    net::http::request r("POST", proxy.url() + "/a/b?x=1&y=%20");
    r.set_header("X-Custom", "v");
    r.set_body("payload");
    auto res = proxy.client().send(r);
    ASSERT_TRUE(res) << text(res.error().message());
    EXPECT_EQ(res->status(), 200);
    EXPECT_EQ(res->header("Content-Type"), "text/plain");
    auto body = text(*res->text());
    EXPECT_TRUE(body.starts_with("POST /a/b?x=1&y=%20\n")) << body;
    EXPECT_NE(body.find("X-Custom: v\n"), std::string::npos) << body;
    EXPECT_NE(body.find("Host: " + text(backend.url()).substr(7) + "\n"), std::string::npos) << body;   // the backend's by default
    EXPECT_NE(body.find("X-Forwarded-For: 127.0.0.1\n"), std::string::npos) << body;
    EXPECT_NE(body.find("X-Forwarded-Host: " + text(proxy.url()).substr(7) + "\n"), std::string::npos) << body;
    EXPECT_NE(body.find("X-Forwarded-Proto: http\n"), std::string::npos) << body;
    EXPECT_EQ(body.find("Forwarded: "), std::string::npos) << body;   // an option, off
    EXPECT_EQ(body.find("Via: "), std::string::npos) << body;
}

TEST(HttpReverseProxy_Tests, JoinsPathsAndQueries) {
    net::http::test_server backend(echo_head);
    struct Case {
        std::string backend_suffix, request, seen;
    };
    for (auto& c : std::vector<Case>{
             {"", "/", "GET /"},
             {"/base", "/x", "GET /base/x"},
             {"/base/", "/x", "GET /base/x"},
             {"/base", "/", "GET /base/"},
             {"/base?k=1", "/x?y=2", "GET /base/x?k=1&y=2"},
             {"?k=1", "/x", "GET /x?k=1"},
             {"", "/a%2Fb", "GET /a%2Fb"},
         }) {
        net::http::test_server proxy(front(net::http::reverse_proxy(backend.url() + sgcl::string(c.backend_suffix))));
        auto res = proxy.client().get(proxy.url() + sgcl::string(c.request));
        ASSERT_TRUE(res);
        auto body = text(*res->text());
        EXPECT_TRUE(body.starts_with(c.seen + "\n")) << c.backend_suffix << " + " << c.request << ": " << body;
    }
}

TEST(HttpReverseProxy_Tests, HopByHopFieldsOfTheRequest) {
    net::http::test_server backend(echo_head);
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())));
    auto raw = talk(proxy.endpoint(),
                    "GET /h HTTP/1.1\r\nHost: front.test\r\nConnection: keep-alive, X-Hop\r\nX-Hop: 1\r\nKeep-Alive: timeout=5\r\n"
                    "Proxy-Authorization: Basic eDp5\r\nProxy-Connection: keep-alive\r\nTE: trailers, deflate\r\nTrailer: X-T\r\n"
                    "Upgrade: foo\r\nX-End: 2\r\nX-Forwarded-For: 10.0.0.1, 10.0.0.2\r\nX-Forwarded-Host: spoofed\r\n"
                    "X-Forwarded-Proto: https\r\nExpect: 100-continue\r\nConnection: close\r\n\r\n");
    EXPECT_NE(raw.find("200 OK"), std::string::npos) << raw;
    raw = raw.substr(raw.find("\r\n\r\n"));   // the body: what the backend saw
    for (auto gone : {"X-Hop", "Keep-Alive", "Proxy-Authorization", "Proxy-Connection", "Trailer:", "Upgrade", "Expect", "spoofed", "deflate",
                      "Connection: "}) {
        EXPECT_EQ(raw.find(std::string("\n") + gone), std::string::npos) << gone << "\n" << raw;
    }
    EXPECT_NE(raw.find("\nTE: trailers\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("\nX-End: 2\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("\nX-Forwarded-For: 10.0.0.1, 10.0.0.2, 127.0.0.1\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("\nX-Forwarded-Host: front.test\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("\nX-Forwarded-Proto: http\n"), std::string::npos) << raw;
}

TEST(HttpReverseProxy_Tests, HopByHopFieldsOfTheResponse) {
    RawBackend backend("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: X-Gone, keep-alive\r\nX-Gone: 1\r\nKeep-Alive: timeout=9\r\n"
                       "Proxy-Authenticate: Basic\r\nUpgrade: h2c\r\nX-Kept: 3\r\nServer: raw\r\nDate: Sun, 06 Nov 1994 08:49:37 GMT\r\n\r\nok");
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())));
    auto raw = talk(proxy.endpoint(), "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_TRUE(raw.starts_with("HTTP/1.1 200 OK\r\n")) << raw;
    for (auto gone : {"X-Gone", "Keep-Alive", "Proxy-Authenticate", "Upgrade", "keep-alive"}) {
        EXPECT_EQ(raw.find(gone), std::string::npos) << gone << "\n" << raw;
    }
    EXPECT_NE(raw.find("X-Kept: 3\r\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("Server: raw\r\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n"), std::string::npos) << raw;   // the backend's, not a second
    EXPECT_NE(raw.find("Content-Length: 2\r\n"), std::string::npos) << raw;
    EXPECT_TRUE(raw.ends_with("\r\n\r\nok")) << raw;
}

TEST(HttpReverseProxy_Tests, ForwardedViaAndHostOptions) {
    net::http::test_server backend(echo_head);
    net::http::reverse_proxy::options o;
    o.forwarded = true;
    o.via = "sgcl-proxy";
    o.preserve_host = true;
    o.x_forwarded = false;
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url(), o)));
    auto raw = talk(proxy.endpoint(),
                    "GET / HTTP/1.1\r\nHost: front.test:81\r\nForwarded: for=192.0.2.43, for=\"[2001:db8:cafe::17]\"\r\n"
                    "Via: 1.0 fred\r\nX-Forwarded-For: 1.2.3.4\r\nConnection: close\r\n\r\n");
    EXPECT_NE(raw.find("\nHost: front.test:81\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("\nForwarded: for=192.0.2.43, for=\"[2001:db8:cafe::17]\", for=127.0.0.1;host=\"front.test:81\";proto=http\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("\nVia: 1.0 fred, 1.1 sgcl-proxy\n"), std::string::npos) << raw;
    EXPECT_NE(raw.find("\nX-Forwarded-For: 1.2.3.4\n"), std::string::npos) << raw;   // left as it came
    EXPECT_NE(raw.find("\r\nVia: 1.1 sgcl-proxy\r\n"), std::string::npos) << raw;     // on the response too
    // a Forwarded that is not RFC 7239's is replaced, not appended to
    auto bad = talk(proxy.endpoint(), "GET / HTTP/1.1\r\nHost: h\r\nForwarded: for=\"unterminated\r\nConnection: close\r\n\r\n");
    EXPECT_NE(bad.find("\nForwarded: for=127.0.0.1;host=h;proto=http\n"), std::string::npos) << bad;
}

TEST(HttpReverseProxy_Tests, AbsoluteFormAndHead) {
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) {
        w.set_header("X-Target", r.url().request_target());
        w.write("0123456789");
    });
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())));
    auto abs = talk(proxy.endpoint(), "GET http://elsewhere.test/p?q=1 HTTP/1.1\r\nHost: elsewhere.test\r\nConnection: close\r\n\r\n");
    EXPECT_NE(abs.find("X-Target: /p?q=1\r\n"), std::string::npos) << abs;
    auto head = talk(proxy.endpoint(), "HEAD /h HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    EXPECT_NE(head.find("Content-Length: 10\r\n"), std::string::npos) << head;   // the GET's length
    EXPECT_TRUE(head.ends_with("\r\n\r\n")) << head;
}

TEST(HttpReverseProxy_Tests, StreamsTheResponse) {
    async::event go_on;
    net::http::test_server backend([&](net::http::request, net::http::response_writer w) -> async::task<> {
        w.write("first;");
        (void)co_await w.async_flush();
        co_await go_on;   // the rest only once the client has the first part
        w.write("second");
    });
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())));
    auto res = proxy.client().get(proxy.url());
    ASSERT_TRUE(res);
    char buf[64];
    auto n = res->body().read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
    ASSERT_TRUE(n);
    EXPECT_EQ(std::string(buf, *n), "first;");
    go_on.set();
    EXPECT_EQ(text(*res->text()), "second");
}

TEST(HttpReverseProxy_Tests, ServerSentEventsFlowThrough) {
    async::event go_on;
    net::http::test_server backend([&](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::event_stream events(w);
        (void)co_await events.async_send(net::http::event{.data = "one"});
        co_await go_on;
        (void)co_await events.async_send(net::http::event{.data = "two"});
    });
    net::http::reverse_proxy::options o;
    o.flush_interval = 1s;   // a stream of events goes at once whatever this says
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url(), o)));
    auto res = proxy.client().get(proxy.url());
    ASSERT_TRUE(res);
    EXPECT_EQ(res->header("Content-Type"), "text/event-stream");
    net::http::event_reader events(res->body());
    auto first = events.next();
    ASSERT_TRUE(first && *first);
    EXPECT_EQ((*first)->data, "one");
    go_on.set();
    auto second = events.next();
    ASSERT_TRUE(second && *second);
    EXPECT_EQ((*second)->data, "two");
}

TEST(HttpReverseProxy_Tests, FlushIntervalSendsWhatIsPending) {
    async::event go_on;
    net::http::test_server backend([&](net::http::request, net::http::response_writer w) -> async::task<> {
        w.set_header("Content-Length", "8");
        w.write("abcd");
        (void)co_await w.async_flush();
        co_await go_on;
        w.write("efgh");
    });
    net::http::reverse_proxy::options o;
    o.flush_interval = 20ms;
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url(), o)));
    auto res = proxy.client().get(proxy.url());
    ASSERT_TRUE(res);
    EXPECT_EQ(res->content_length(), optional<uint64_t>(8));
    char buf[16];
    auto n = res->body().read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
    ASSERT_TRUE(n);
    EXPECT_EQ(std::string(buf, *n), "abcd");
    go_on.set();
    EXPECT_EQ(text(*res->text()), "efgh");
}

TEST(HttpReverseProxy_Tests, LargeBodiesBothWays) {
    net::http::server b;
    b.max_body_bytes = 0;
    b.route("/", [](net::http::request r, net::http::response_writer w) -> async::task<> {
        auto all = co_await r.async_bytes();
        uint64_t sum = 0;
        for (auto x : *all) {
            sum = sum * 31 + uint8_t(x);
        }
        w.set_header("X-Length", sgcl::string(std::to_string(all->size())));
        w.set_header("X-Sum", sgcl::string(std::to_string(sum)));
        // and a large body back, written in pieces
        std::string piece(64 * 1024, 'q');
        for (int i : range(128)) {
            (void)i;
            w.write(sgcl::string(piece));
            (void)co_await w.async_flush();
        }
    });
    net::http::test_server backend(b);
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())));
    std::string up(9 * 1024 * 1024 + 7, 'u');
    for (size_t i = 0; i < up.size(); i += 4093) {
        up[i] = char('a' + i % 26);
    }
    uint64_t sum = 0;
    for (char x : up) {
        sum = sum * 31 + uint8_t(x);
    }
    for (bool streamed : {false, true}) {
        net::http::request r("PUT", proxy.url());
        if (streamed) {
            r.set_body(io::reader(make_tracked<io::buffer>(sgcl::string(up))));   // chunked to the proxy, chunked on
        } else {
            r.set_body(sgcl::string(up));
        }
        auto res = proxy.client().send(r);
        ASSERT_TRUE(res) << text(res.error().message());
        EXPECT_EQ(res->header("X-Length"), sgcl::string(std::to_string(up.size())));
        EXPECT_EQ(res->header("X-Sum"), sgcl::string(std::to_string(sum)));
        auto down = res->bytes();
        ASSERT_TRUE(down);
        EXPECT_EQ(down->size(), size_t(128 * 64 * 1024));
    }
}

TEST(HttpReverseProxy_Tests, TrailersPassThrough) {
    net::http::test_server backend([](net::http::request, net::http::response_writer w) -> async::task<> {
        w.set_header("Trailer", "X-Checksum");
        w.write("body");
        (void)co_await w.async_flush();
        w.write("-more");
        w.trailers().set("X-Checksum", "42");
    });
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())));
    auto res = proxy.client().get(proxy.url());
    ASSERT_TRUE(res);
    EXPECT_EQ(res->header("Trailer"), "X-Checksum");
    EXPECT_EQ(text(*res->text()), "body-more");
    EXPECT_EQ(res->trailers().get("X-Checksum"), "42");
    // a small whole body with trailers too
    net::http::test_server small_backend([](net::http::request, net::http::response_writer w) {
        w.write("s");
        w.trailers().set("X-T", "1");
    });
    net::http::test_server small_proxy(front(net::http::reverse_proxy(small_backend.url())));
    auto s = small_proxy.client().get(small_proxy.url());
    ASSERT_TRUE(s);
    EXPECT_EQ(text(*s->text()), "s");
    EXPECT_EQ(s->trailers().get("X-T"), "1");
}

TEST(HttpReverseProxy_Tests, EarlyHintsForwarded) {
    for (bool h2 : {false, true}) {
        net::http::test_server backend([](net::http::request, net::http::response_writer w) -> async::task<> {
            net::http::headers hints;
            hints.add("Link", "</s.css>; rel=preload");
            (void)co_await w.async_send_informational(103, hints);
            w.write("final");
        }, {.http2 = h2});
        net::http::reverse_proxy::options o;
        o.client = backend.client();
        net::http::test_server proxy(front(net::http::reverse_proxy(backend.url(), o)));
        auto raw = talk(proxy.endpoint(), "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
        const char* expected = h2 ? "HTTP/1.1 103 Early Hints\r\nlink: </s.css>; rel=preload\r\n\r\nHTTP/1.1 200 OK\r\n"   // HTTP/2 writes names in lower case
                                  : "HTTP/1.1 103 Early Hints\r\nLink: </s.css>; rel=preload\r\n\r\nHTTP/1.1 200 OK\r\n";
        EXPECT_TRUE(raw.starts_with(expected)) << h2 << "\n" << raw;
        EXPECT_TRUE(raw.ends_with("final")) << raw;
    }
}

TEST(HttpReverseProxy_Tests, WebSocketThroughTheProxy) {
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) -> async::task<> {
        auto ws = co_await net::http::websocket::async_accept(r, w);
        if (!ws) {
            co_return;
        }
        for (;;) {
            auto m = co_await ws->async_receive();
            if (!m) {
                break;
            }
            (void)co_await ws->async_send(string::concat("echo: ", m->text()));
        }
    });
    net::http::reverse_proxy::options o;
    o.via = "p";
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url(), o)));
    auto url = string::concat("ws", proxy.url().view().substr(4), "/socket");
    auto ws = proxy.client().websocket(url);
    ASSERT_TRUE(ws) << text(ws.error().message());
    for (int i : range(3)) {
        ASSERT_TRUE(ws->send(sgcl::string("m" + std::to_string(i))));
        auto m = ws->receive();
        ASSERT_TRUE(m);
        EXPECT_EQ(m->text(), sgcl::string("echo: m" + std::to_string(i)));
    }
    std::string big(200000, 'w');
    ASSERT_TRUE(ws->send(sgcl::string(big)));
    auto m = ws->receive();
    ASSERT_TRUE(m);
    EXPECT_EQ(m->text().size(), big.size() + 6);
    (void)ws->close();
}

TEST(HttpReverseProxy_Tests, RawUpgradeIsAPipe) {
    // a backend that switches to a protocol of its own: echoes lines upper case
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) -> async::task<> {
        if (r.header("Upgrade") != "shout") {
            w.error(400);
            co_return;
        }
        auto taken = w.hijack();
        if (!taken) {
            co_return;
        }
        auto c = taken->first;
        (void)co_await c.async_write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: shout\r\nConnection: Upgrade\r\n\r\nready\n");
        for (;;) {
            char buf[256];
            auto n = co_await taken->second.async_read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
            if (!n || *n == 0) {
                break;
            }
            std::string s(buf, *n);
            for (auto& ch : s) {
                ch = char(std::toupper(uint8_t(ch)));
            }
            (void)co_await c.async_write(sgcl::string(s));
        }
        (void)co_await c.async_close();
    });
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url())));
    auto c = *net::tcp::connect(proxy.endpoint());
    c.set_deadline(sgcl::clock::now() + 5s);
    (void)c.write("GET /u HTTP/1.1\r\nHost: x\r\nUpgrade: shout\r\nConnection: Upgrade\r\n\r\nhello\n");
    std::string got;
    char buf[1024];
    while (got.find("HELLO\n") == std::string::npos) {
        auto n = c.read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
        if (!n || *n == 0) {
            break;
        }
        got.append(buf, *n);
    }
    EXPECT_TRUE(got.starts_with("HTTP/1.1 101 Switching Protocols\r\n")) << got;
    EXPECT_NE(got.find("Upgrade: shout\r\n"), std::string::npos) << got;
    EXPECT_NE(got.find("Connection: Upgrade\r\n"), std::string::npos) << got;
    EXPECT_NE(got.find("ready\nHELLO\n"), std::string::npos) << got;
    (void)c.write("again\n");
    std::string more;
    while (more.find("AGAIN\n") == std::string::npos) {
        auto n = c.read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
        if (!n || *n == 0) {
            break;
        }
        more.append(buf, *n);
    }
    EXPECT_EQ(more, "AGAIN\n");
    (void)c.close();
    // a backend that refuses the upgrade: its answer passed back
    auto refused = talk(proxy.endpoint(), "GET / HTTP/1.1\r\nHost: x\r\nUpgrade: other\r\nConnection: Upgrade, close\r\n\r\n");
    EXPECT_TRUE(refused.starts_with("HTTP/1.1 400 Bad Request\r\n")) << refused;
}

TEST(HttpReverseProxy_Tests, Http2InFrontAndBehind) {
    for (int mode : range(4)) {
        const bool front_h2 = mode & 1;
        const bool back_tls = mode & 2;
        net::http::test_server backend([](net::http::request r, net::http::response_writer w) -> async::task<> {
            auto body = co_await r.async_text();
            w.set_header("X-Proto", r.proto());
            w.write(string::concat("got ", *body));
            w.trailers().set("X-T", "t");
        }, {.tls = back_tls, .http2 = true});
        net::http::reverse_proxy::options o;
        o.client = backend.client();
        net::http::test_server proxy(front(net::http::reverse_proxy(backend.url(), o)), {.http2 = front_h2});
        net::http::request r("POST", proxy.url() + "/x");
        r.set_body("ping");
        auto res = proxy.client().send(r);
        ASSERT_TRUE(res) << mode << " " << text(res.error().message());
        EXPECT_EQ(res->proto(), front_h2 ? "HTTP/2.0" : "HTTP/1.1");
        EXPECT_EQ(res->header("X-Proto"), "HTTP/2.0");
        EXPECT_EQ(text(*res->text()), "got ping");
        EXPECT_EQ(res->trailers().get("X-T"), "t");
    }
}

TEST(HttpReverseProxy_Tests, TlsBehindAndInFront) {
    net::http::test_server backend(echo_head, {.tls = true});
    net::http::reverse_proxy::options o;
    o.client = backend.client();   // trusts the backend's CA
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url(), o)), {.tls = true});
    auto res = proxy.client().get(proxy.url() + "/s");
    ASSERT_TRUE(res) << text(res.error().message());
    auto body = text(*res->text());
    EXPECT_TRUE(body.starts_with("GET /s\n")) << body;
    EXPECT_NE(body.find("X-Forwarded-Proto: https\n"), std::string::npos) << body;
    // the default client does not trust the test CA: 502
    net::http::test_server untrusted(front(net::http::reverse_proxy(backend.url())));
    auto bad = untrusted.client().get(untrusted.url());
    ASSERT_TRUE(bad);
    EXPECT_EQ(bad->status(), 502);
    EXPECT_EQ(text(*bad->text()), "Bad Gateway\n");
}

TEST(HttpReverseProxy_Tests, BackendDownAndSlow) {
    // a port nobody listens on
    uint16_t port = 0;
    {
        auto l = *net::tcp::listen("127.0.0.1:0");
        port = l.local_endpoint().port();
        (void)l.close();
    }
    net::http::test_server down(front(net::http::reverse_proxy(sgcl::string("http://127.0.0.1:" + std::to_string(port)))));
    auto r = down.client().get(down.url());
    ASSERT_TRUE(r);
    EXPECT_EQ(r->status(), 502);
    net::http::test_server slow_backend([](net::http::request r, net::http::response_writer w) -> async::task<> {
        co_await async::sleep(r.url().path() == "/slow" ? 2s : 0s);
        w.write("late");
    });
    net::http::reverse_proxy::options o;
    o.client.response_header_timeout = 200ms;
    net::http::test_server slow(front(net::http::reverse_proxy(slow_backend.url(), o)));
    auto t = slow.client().get(slow.url() + "/slow");
    ASSERT_TRUE(t);
    EXPECT_EQ(t->status(), 504);
    EXPECT_EQ(text(*t->text()), "Gateway Timeout\n");
    auto fast = slow.client().get(slow.url() + "/fast");
    ASSERT_TRUE(fast);
    EXPECT_EQ(fast->status(), 200);
    // a backend that dies half-way through a body: 502 while nothing
    // went to the client, the response cut short after its head went
    RawBackend dying("HTTP/1.1 200 OK\r\nContent-Length: 100000\r\n\r\nonly a little");
    net::http::test_server unsent(front(net::http::reverse_proxy(dying.url())));
    auto u = unsent.client().get(unsent.url());
    ASSERT_TRUE(u);
    EXPECT_EQ(u->status(), 502);
    net::http::reverse_proxy::options every;
    every.flush_interval = -1ms;   // after every read: the head goes with the first bytes
    net::http::test_server cut(front(net::http::reverse_proxy(dying.url(), every)));
    auto c = cut.client().get(cut.url());
    ASSERT_TRUE(c);
    EXPECT_EQ(c->status(), 200);
    EXPECT_FALSE(c->text());
    RawBackend dying_chunked("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n");
    net::http::test_server cut2(front(net::http::reverse_proxy(dying_chunked.url())));
    auto c2 = cut2.client().get(cut2.url());
    ASSERT_TRUE(c2);
    EXPECT_FALSE(c2->text());   // no last chunk: never taken for a whole body
}

TEST(HttpReverseProxy_Tests, ClientGoneCancelsTheBackend) {
    // HTTP/2 in front: the client's connection lost while the backend
    // thinks; the backend's request ended by the proxy at once
    std::atomic<bool> backend_saw_end = false;
    net::http::test_server backend([&](net::http::request, net::http::response_writer w) -> async::task<> {
        for (int i : range(200)) {
            (void)i;
            co_await async::sleep(20ms);
            w.write(".");
            if (!co_await w.async_flush()) {
                backend_saw_end = true;
                co_return;
            }
        }
    });
    net::http::reverse_proxy p(backend.url());
    for (bool h2 : {false, true}) {
        backend_saw_end = false;
        net::http::test_server proxy(front(p), {.http2 = h2});
        auto res = proxy.client().get(proxy.url());
        ASSERT_TRUE(res);
        char buf[8];
        ASSERT_TRUE(res->body().read(slice<byte>(reinterpret_cast<byte*>(buf), 1)));
        proxy.close_client_connections();
        for (int i : range(200)) {
            (void)i;
            if (backend_saw_end) {
                break;
            }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT_TRUE(backend_saw_end.load()) << h2;
    }
    // waiting for the head: over HTTP/2 the request's stop cancels it
    net::http::test_server thinking([](net::http::request, net::http::response_writer w) -> async::task<> {
        co_await async::sleep(3s);
        w.write("late");
    });
    net::http::reverse_proxy q(thinking.url());
    net::http::test_server proxy(front(q), {.http2 = true});
    std::thread t([url = text(proxy.url()), c = 0]() mutable {
        (void)c;
        net::http::client client;
        client.proxy = net::http::proxy();
        client.h2c = true;
        (void)client.post(sgcl::string(url), "text/plain", "x");   // a POST: not sent again when its connection is lost
    });
    for (int i : range(100)) {
        (void)i;
        if (q.backends()[0].active == 1) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_EQ(q.backends()[0].active, 1);
    auto start = sgcl::clock::now();
    proxy.close_client_connections();
    while (q.backends()[0].active != 0 && sgcl::clock::now() - start < 2s) {
        std::this_thread::sleep_for(5ms);
    }
    EXPECT_EQ(q.backends()[0].active, 0);
    EXPECT_LT(sgcl::clock::now() - start, 1s);
    t.join();
}

TEST(HttpReverseProxy_Tests, Hooks) {
    net::http::test_server backend(echo_head);
    net::http::reverse_proxy::options o;
    o.rewrite = [](net::http::request& out, const net::http::request& in) {
        out.set_header("X-Original-Path", in.url().path());
        out.set_url(string::concat(out.url().scheme(), "://", out.url().host(), "/rewritten"));
    };
    o.modify_response = [](net::http::response_writer& w, const net::http::response& res) {
        w.set_header("X-Backend-Status", sgcl::string(std::to_string(res.status())));
        w.headers().erase("Content-Type");
    };
    net::http::test_server proxy(front(net::http::reverse_proxy(backend.url(), o)));
    auto res = proxy.client().get(proxy.url() + "/orig");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->header("X-Backend-Status"), "200");
    EXPECT_EQ(res->header("Content-Type"), "");
    auto body = text(*res->text());
    EXPECT_TRUE(body.starts_with("GET /rewritten\n")) << body;
    EXPECT_NE(body.find("X-Original-Path: /orig\n"), std::string::npos) << body;
    // a body of the modifier's own replaces the backend's
    net::http::reverse_proxy::options replace;
    replace.modify_response = [](net::http::response_writer& w, const net::http::response&) {
        w.set_status(203);
        w.write("replaced");
    };
    net::http::test_server p2(front(net::http::reverse_proxy(backend.url(), replace)));
    auto r2 = p2.client().get(p2.url());
    ASSERT_TRUE(r2);
    EXPECT_EQ(r2->status(), 203);
    EXPECT_EQ(text(*r2->text()), "replaced");
    // the error handler
    net::http::reverse_proxy::options eh;
    eh.error_handler = [](net::http::response_writer& w, const net::http::request& in, const io::error& e) {
        w.set_status(503);
        w.write(string::concat("sorry ", in.url().path(), e.code() ? " (failed)" : ""));
    };
    net::http::test_server p3(front(net::http::reverse_proxy(sgcl::string("http://127.0.0.1:1"), eh)));
    auto r3 = p3.client().get(p3.url() + "/z");
    ASSERT_TRUE(r3);
    EXPECT_EQ(r3->status(), 503);
    EXPECT_EQ(text(*r3->text()), "sorry /z (failed)");
}

TEST(HttpReverseProxy_Tests, ARecorderDrivesIt) {
    net::http::test_server backend(echo_head);
    net::http::reverse_proxy p(backend.url());
    net::http::response_recorder rec;
    p(net::http::test_request("GET", "/via-recorder?a=b"), rec.writer()).wait();
    EXPECT_EQ(rec.status(), 200);
    auto body = text(rec.body());
    EXPECT_TRUE(body.starts_with("GET /via-recorder?a=b\n")) << body;
    EXPECT_NE(body.find("X-Forwarded-For: 192.0.2.1\n"), std::string::npos) << body;
    EXPECT_NE(body.find("X-Forwarded-Host: example.com\n"), std::string::npos) << body;
}

TEST(HttpReverseProxy_Tests, Boundaries) {
    EXPECT_THROW(net::http::reverse_proxy(sgcl::string("")), invalid_argument);
    EXPECT_THROW(net::http::reverse_proxy(sgcl::string("ftp://x/")), invalid_argument);
    EXPECT_THROW(net::http::reverse_proxy(sgcl::string("not a url")), invalid_argument);
    EXPECT_THROW(net::http::reverse_proxy(vector<sgcl::string>()), invalid_argument);
    net::http::reverse_proxy a(sgcl::string("http://127.0.0.1:1/base"));
    auto b = std::move(a);   // a move is the copy
    EXPECT_EQ(a.backends().size(), 1u);
    EXPECT_EQ(b.backends()[0].url, "http://127.0.0.1:1/base");
    EXPECT_TRUE(b.backends()[0].healthy);
    EXPECT_EQ(b.backends()[0].requests, 0u);
    a.close();
    a.close();
    // a request whose body is past the front's limit: the proxy's read fails, 413
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) -> async::task<> {
        auto t = co_await r.async_text();
        w.write(t ? "read" : "failed");
    });
    net::http::server s;
    s.route("/", net::http::reverse_proxy(backend.url()));
    s.max_body_bytes = 10;
    net::http::test_server proxy(s);
    auto res = proxy.client().post(proxy.url(), "text/plain", "0123456789abcdef");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status(), 413);   // the declared length past the limit: refused before the proxy
    // chunked past the limit: the proxy's stream of it breaks, 413, and the
    // backend is not blamed
    net::http::reverse_proxy counted(backend.url());
    net::http::server s2;
    s2.route("/", counted);
    s2.max_body_bytes = 10;
    net::http::test_server limited(s2);
    net::http::request big("PUT", limited.url());
    big.set_body(io::reader(make_tracked<io::buffer>(sgcl::string(std::string(4096, 'x')))));
    auto r2 = limited.client().send(big);
    ASSERT_TRUE(r2);
    EXPECT_EQ(r2->status(), 413);
    EXPECT_EQ(counted.backends()[0].failures, 0u);
    // a hook that throws: the server's 500, its on_error told
    net::http::reverse_proxy::options o;
    o.rewrite = [](net::http::request&, const net::http::request&) {
        throw std::runtime_error("hook failed");
    };
    std::atomic<int> reported = 0;
    net::http::server s3;
    s3.route("/", net::http::reverse_proxy(backend.url(), o));
    s3.on_error = [&](const sgcl::string&) { ++reported; };
    net::http::test_server throwing(s3);
    auto r3 = throwing.client().get(throwing.url());
    ASSERT_TRUE(r3);
    EXPECT_EQ(r3->status(), 500);
    EXPECT_EQ(reported.load(), 1);
}
