//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::client with a cookie_jar, against the module's server on the
// loopback: the Set-Cookie of every response, redirects among them, kept,
// the Cookie of every request taken from the jar after the request's own
// pairs (those a response of the exchange set again left out, as Go's client
// does), across hosts by the jar's rules, over HTTP/1.1 and HTTP/2 (h2c),
// shared by clients and by concurrent requests, and the WebSocket handshake.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <string>

using namespace sgcl;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // A server of the routes the tests ask: /echo answers the request's
    // Cookie field, /set?c=... sets the cookie c, /go?c=...&to=... sets it
    // and redirects
    struct Site {
        net::http::server srv;
        optional<net::listener> listener;
        optional<async::task<expected<void, io::error>>> serving;
        int port = 0;

        explicit Site(bool h2c = false) {
            srv.h2c = h2c;
            srv.route("GET /echo", [](net::http::request req, net::http::response_writer w) {
                w.write(req.header("Cookie"));
            });
            srv.route("GET /set", [](net::http::request req, net::http::response_writer w) {
                for (auto& c : req.url().query_params().get_all("c")) {
                    w.headers().add("Set-Cookie", c);
                }
                w.write("set");
            });
            srv.route("GET /go", [](net::http::request req, net::http::response_writer w) {
                auto q = req.url().query_params();
                for (auto& c : q.get_all("c")) {
                    w.headers().add("Set-Cookie", c);
                }
                w.redirect(q.get("to"));
            });
            srv.route("GET /ws", [](net::http::request req, net::http::response_writer w) -> async::task<> {
                net::http::websocket::options o;
                o.headers.add("Set-Cookie", "ws=1");
                auto c = co_await net::http::websocket::async_accept(req, w, o);
                if (c) {
                    (void)co_await c->async_send(req.header("Cookie"));
                    (void)co_await c->async_receive();
                }
            });
            listener = *net::tcp::listen("127.0.0.1:0");
            port = listener->local_endpoint().port();
            serving = async::spawn(srv.async_serve(*listener));
        }

        ~Site() {
            srv.close();
            (void)serving->wait();
        }

        sgcl::string at(const std::string& host, const std::string& path) const {
            return sgcl::string("http://" + host + ":" + std::to_string(port) + path);
        }

        sgcl::string at(const std::string& path) const {
            return at("127.0.0.1", path);
        }
    };

    net::http::client make_client() {
        net::http::client c;
        c.proxy = net::http::proxy();
        c.jar = net::http::cookie_jar();
        return c;
    }

    std::string body(const expected<net::http::response, io::error>& r) {
        if (!r) {
            return "error: " + text(r.error().message());
        }
        auto t = r->text();
        return t ? text(*t) : "error: " + text(t.error().message());
    }

    // a query value escaped: everything but the unreserved characters
    std::string q(const std::string& text_in) {
        const char* s = text_in.c_str();
        std::string out;
        for (const char* p = s; *p; ++p) {
            unsigned char c = uint8_t(*p);
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
                out += char(c);
            } else {
                char hex[4];
                std::snprintf(hex, sizeof hex, "%%%02X", c);
                out += hex;
            }
        }
        return out;
    }
}

TEST(HttpCookieJarClient_Tests, AResponsesCookieGoesWithTheNextRequest) {
    Site site;
    net::http::client c = make_client();
    EXPECT_EQ(body(c.get(site.at("/echo"))), "");
    EXPECT_EQ(body(c.get(site.at("/set?c=" + q("session=abc; Path=/")))), "set");
    EXPECT_EQ(body(c.get(site.at("/echo"))), "session=abc");
    EXPECT_EQ(c.jar->size(), 1u);
    // deleted by the server
    EXPECT_EQ(body(c.get(site.at("/set?c=" + q("session=; Max-Age=0")))), "set");
    EXPECT_EQ(body(c.get(site.at("/echo"))), "");
    EXPECT_EQ(c.jar->size(), 0u);
}

TEST(HttpCookieJarClient_Tests, NoJarNoCookies) {
    Site site;
    net::http::client c;
    c.proxy = net::http::proxy();
    EXPECT_FALSE(c.jar);
    EXPECT_EQ(body(c.get(site.at("/set?c=" + q("a=1")))), "set");
    EXPECT_EQ(body(c.get(site.at("/echo"))), "");
    // the request's own Cookie goes as it is
    auto r = c.send(net::http::request("GET", site.at("/echo")).set_header("Cookie", "mine=1"));
    EXPECT_EQ(body(r), "mine=1");
}

TEST(HttpCookieJarClient_Tests, EveryRedirectsCookieIsKeptAndSentOn) {
    Site site;
    net::http::client c = make_client();
    // /go sets a and redirects to /go, which sets b and redirects to /echo
    std::string second = "/go?c=" + q("b=2") + "&to=" + q("/echo");
    EXPECT_EQ(body(c.get(site.at("/go?c=" + q("a=1") + "&to=" + q(second)))), "a=1; b=2");
    EXPECT_EQ(c.jar->size(), 2u);
    // a redirect's cookie with a path: sent where it applies only
    EXPECT_EQ(body(c.get(site.at("/go?c=" + q("p=1; Path=/elsewhere") + "&to=" + q("/echo")))), "a=1; b=2");
}

TEST(HttpCookieJarClient_Tests, AcrossHostsTheJarDecides) {
    Site site;
    net::http::client c = make_client();
    EXPECT_EQ(body(c.get(site.at("localhost", "/set?c=" + q("there=1")))), "set");
    // 127.0.0.1 sets mine and redirects to localhost: localhost's own cookie goes, not 127.0.0.1's
    std::string to = text(site.at("localhost", "/echo"));
    EXPECT_EQ(body(c.get(site.at("/go?c=" + q("mine=1") + "&to=" + q(to)))), "there=1");
    EXPECT_EQ(body(c.get(site.at("/echo"))), "mine=1");
    // the request's own Cookie does not follow to another host (the client's rule), the jar's does
    auto r = c.send(net::http::request("GET", site.at("/go?to=" + q(to))).set_header("Cookie", "own=1"));
    EXPECT_EQ(body(r), "there=1");
}

TEST(HttpCookieJarClient_Tests, TheRequestsOwnCookieComesFirst) {
    Site site;
    net::http::client c = make_client();
    EXPECT_EQ(body(c.get(site.at("/set?c=" + q("jar=1")))), "set");
    auto r = c.send(net::http::request("GET", site.at("/echo")).set_header("Cookie", "own=1; other=2"));
    EXPECT_EQ(body(r), "own=1; other=2; jar=1");
    // two Cookie fields of the request in one
    net::http::request two("GET", site.at("/echo"));
    two.add_header("Cookie", "x=1").add_header("Cookie", "y=2");
    EXPECT_EQ(body(c.send(two)), "x=1; y=2; jar=1");
    // a cookie of the request's own that a response of the exchange set
    // again is left out after it: the jar's is the one sent
    std::string go = "/go?c=" + q("own=new") + "&to=" + q("/echo");
    auto again = c.send(net::http::request("GET", site.at(go)).set_header("Cookie", "own=old; keep=1"));
    EXPECT_EQ(body(again), "keep=1; jar=1; own=new");
}

TEST(HttpCookieJarClient_Tests, OverHttp2) {
    Site site(true);
    net::http::client c = make_client();
    c.h2c = true;
    EXPECT_EQ(body(c.get(site.at("/set?c=" + q("h2=1")))), "set");
    auto r = c.get(site.at("/go?c=" + q("hop=1") + "&to=" + q("/echo")));
    ASSERT_TRUE(r);
    EXPECT_EQ(text(r->proto()), "HTTP/2.0");
    EXPECT_EQ(body(r), "h2=1; hop=1");
}

TEST(HttpCookieJarClient_Tests, ClientsShareAJar) {
    Site site;
    net::http::client a = make_client();
    net::http::client b = make_client();
    b.jar = a.jar;
    EXPECT_EQ(body(a.get(site.at("/set?c=" + q("shared=1")))), "set");
    EXPECT_EQ(body(b.get(site.at("/echo"))), "shared=1");
    net::http::client copy = a;   // a copy of a client carries its jar, the same one
    EXPECT_EQ(body(copy.get(site.at("/echo"))), "shared=1");
    copy.jar = nullopt;
    EXPECT_EQ(body(copy.get(site.at("/echo"))), "");
    EXPECT_EQ(body(a.get(site.at("/echo"))), "shared=1");
}

// Many requests at once through one client and its jar, on the scheduler
TEST(HttpCookieJarClient_Tests, ConcurrentRequestsShareTheJar) {
    Site site;
    net::http::client c = make_client();
    auto one = [](net::http::client c, sgcl::string set, sgcl::string echo, int i) -> async::task<bool> {
        auto r = co_await c.async_get(set);
        if (!r || !co_await r->async_text()) {
            co_return false;
        }
        auto e = co_await c.async_get(echo);
        if (!e) {
            co_return false;
        }
        auto t = co_await e->async_text();
        co_return t && t->view().find("k" + std::to_string(i) + "=") != std::string_view::npos;
    };
    auto all = [&]() -> async::task<int> {
        vector<async::task<bool>> tasks;
        for (int i : range(32)) {
            tasks.push_back(async::spawn(one(c, site.at("/set?c=" + q("k" + std::to_string(i) + "=v")), site.at("/echo"), i)));
        }
        int good = 0;
        for (auto& t : tasks) {
            good += co_await t ? 1 : 0;
        }
        co_return good;
    };
    EXPECT_EQ(async::spawn(all()).wait(), 32);
    EXPECT_EQ(c.jar->size(), 32u);
}

TEST(HttpCookieJarClient_Tests, TheWebSocketHandshakeCarriesAndKeepsCookies) {
    Site site;
    net::http::client c = make_client();
    EXPECT_EQ(body(c.get(site.at("/set?c=" + q("auth=1")))), "set");
    auto ws = c.websocket(site.at("/ws"));
    ASSERT_TRUE(ws) << text(ws.error().message());
    auto m = ws->receive();
    ASSERT_TRUE(m);
    EXPECT_EQ(text(m->text()), "auth=1");
    (void)ws->send("bye");
    (void)ws->close();
    EXPECT_EQ(body(c.get(site.at("/echo"))), "auth=1; ws=1");
}
