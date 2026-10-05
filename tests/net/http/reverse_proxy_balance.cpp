//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: reverse_proxy over several backends: round robin, least
// connections, random; passive health (a backend failing is skipped for a
// cooldown, then tried again), active health checks, fail open when every
// backend is down; retries of idempotent requests on another backend (a
// small body read whole and sent again, a POST never retried, a streamed
// body never retried); the counts of backends().
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <map>
#include <string>
#include <thread>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // A backend that names itself, and answers /health with health
    struct Named {
        std::atomic<int> health = 200;
        std::atomic<int> hits = 0;
        net::http::test_server ts;

        explicit Named(const std::string& name)
        : ts([this, name](net::http::request r, net::http::response_writer w) -> async::task<> {
            if (r.url().path() == "/health") {
                w.set_status(health.load());
                co_return;
            }
            ++hits;
            if (r.url().path() == "/hold") {
                co_await async::sleep(300ms);
            }
            auto body = co_await r.async_text();
            w.write(sgcl::string(name + (body && !body->empty() ? ":" + text(*body) : std::string())));
        }) {
        }
    };

    net::http::server front(const net::http::reverse_proxy& p) {
        net::http::server s;
        s.route("/", p);
        return s;
    }

    // An address nobody listens on
    sgcl::string dead_url() {
        auto l = *net::tcp::listen("127.0.0.1:0");
        auto port = l.local_endpoint().port();
        (void)l.close();
        return sgcl::string("http://127.0.0.1:" + std::to_string(port));
    }

    std::string get(const net::http::test_server& proxy, const std::string& path = "/") {
        auto r = proxy.client().get(proxy.url() + sgcl::string(path));
        if (!r) {
            return "error";
        }
        return std::to_string(r->status()) + " " + text(*r->text());
    }
}

TEST(HttpReverseProxyBalance_Tests, RoundRobin) {
    Named a("a"), b("b"), c("c");
    net::http::reverse_proxy p({a.ts.url(), b.ts.url(), c.ts.url()});
    net::http::test_server proxy(front(p));
    std::map<std::string, int> seen;
    std::string order;
    for (int i : range(9)) {
        (void)i;
        auto r = get(proxy);
        seen[r]++;
        order += r.substr(4);
    }
    EXPECT_EQ(seen["200 a"], 3);
    EXPECT_EQ(seen["200 b"], 3);
    EXPECT_EQ(seen["200 c"], 3);
    EXPECT_TRUE(order == "abcabcabc" || order == "bcabcabca" || order == "cabcabcab") << order;
    auto counts = p.backends();
    ASSERT_EQ(counts.size(), 3u);
    for (auto& x : counts) {
        EXPECT_EQ(x.requests, 3u);
        EXPECT_EQ(x.active, 0);
        EXPECT_EQ(x.failures, 0u);
        EXPECT_TRUE(x.healthy);
    }
}

TEST(HttpReverseProxyBalance_Tests, LeastConnections) {
    Named a("a"), b("b");
    net::http::reverse_proxy::options o;
    o.policy = net::http::reverse_proxy::balancing::least_connections;
    net::http::reverse_proxy p({a.ts.url(), b.ts.url()}, o);
    net::http::test_server proxy(front(p));
    // three held requests in flight, then quick ones: they go where fewer are held
    std::vector<std::thread> held;
    for (int i : range(3)) {
        (void)i;
        held.emplace_back([url = text(proxy.url())] {
            net::http::client c;
            c.proxy = net::http::proxy();
            (void)c.get(sgcl::string(url + "/hold"));
        });
    }
    for (int i : range(100)) {
        (void)i;
        auto bs = p.backends();
        if (bs[0].active + bs[1].active == 3) {
            break;
        }
        std::this_thread::sleep_for(2ms);
    }
    auto bs = p.backends();
    ASSERT_EQ(bs[0].active + bs[1].active, 3);
    const int fewer = bs[0].active < bs[1].active ? 0 : 1;
    auto r = get(proxy);
    EXPECT_EQ(r, fewer == 0 ? "200 a" : "200 b");
    for (auto& t : held) {
        t.join();
    }
    EXPECT_EQ(p.backends()[0].active + p.backends()[1].active, 0);
}

TEST(HttpReverseProxyBalance_Tests, RandomReachesEveryBackend) {
    Named a("a"), b("b"), c("c");
    net::http::reverse_proxy::options o;
    o.policy = net::http::reverse_proxy::balancing::random;
    net::http::test_server proxy(front(net::http::reverse_proxy({a.ts.url(), b.ts.url(), c.ts.url()}, o)));
    for (int i : range(90)) {
        (void)i;
        EXPECT_TRUE(get(proxy).starts_with("200 "));
    }
    EXPECT_GT(a.hits.load(), 5);
    EXPECT_GT(b.hits.load(), 5);
    EXPECT_GT(c.hits.load(), 5);
    EXPECT_EQ(a.hits + b.hits + c.hits, 90);
}

TEST(HttpReverseProxyBalance_Tests, PassiveHealthAndRetry) {
    Named live("live");
    net::http::reverse_proxy::options o;
    o.max_fails = 1;
    o.fail_timeout = 300ms;
    net::http::reverse_proxy p({dead_url(), live.ts.url()}, o);
    net::http::test_server proxy(front(p));
    for (int i : range(6)) {   // the dead one tried once, the GET retried on the live one, the dead one skipped after
        (void)i;
        EXPECT_EQ(get(proxy), "200 live");
    }
    auto bs = p.backends();
    EXPECT_EQ(bs[0].failures, 1u);
    EXPECT_FALSE(bs[0].healthy);
    EXPECT_TRUE(bs[1].healthy);
    EXPECT_EQ(bs[1].requests, 6u);
    std::this_thread::sleep_for(350ms);   // the cooldown over: tried again (and failed again)
    EXPECT_TRUE(p.backends()[0].healthy);
    for (int i : range(2)) {
        (void)i;
        EXPECT_EQ(get(proxy), "200 live");
    }
    EXPECT_EQ(p.backends()[0].failures, 2u);
}

TEST(HttpReverseProxyBalance_Tests, MaxFailsInARow) {
    net::http::reverse_proxy::options o;
    o.max_fails = 3;
    o.retries = 0;
    net::http::reverse_proxy p(dead_url(), o);
    net::http::test_server proxy(front(p));
    for (int i : range(2)) {
        (void)i;
        EXPECT_EQ(get(proxy), "502 Bad Gateway\n");
    }
    EXPECT_TRUE(p.backends()[0].healthy);   // two in a row: still in the turn
    EXPECT_EQ(get(proxy), "502 Bad Gateway\n");
    EXPECT_FALSE(p.backends()[0].healthy);
    // every backend down: the proxy fails open, the one there still tried
    EXPECT_EQ(get(proxy), "502 Bad Gateway\n");
    EXPECT_EQ(p.backends()[0].failures, 4u);
}

TEST(HttpReverseProxyBalance_Tests, RetriesOnlyWhatMayGoAgain) {
    Named live("live");
    auto dead = dead_url();
    net::http::reverse_proxy::options o;
    o.max_fails = 0;   // never skipped: each request meets the dead one first in its turn
    net::http::reverse_proxy p({dead, live.ts.url()}, o);
    net::http::test_server proxy(front(p));
    auto c = proxy.client();
    int retried_put = 0, failed_post = 0, failed_stream = 0;
    for (int i : range(4)) {
        (void)i;
        net::http::request put("PUT", proxy.url());
        put.set_body("small");   // read whole and sent again
        auto r = c.send(put);
        ASSERT_TRUE(r);
        auto t = text(*r->text());
        if (t == "live:small") {
            ++retried_put;
        } else {
            ADD_FAILURE() << t;
        }
    }
    for (int i : range(4)) {   // in turn: two meet the dead backend
        (void)i;
        auto post = c.post(proxy.url(), "text/plain", "data");   // not idempotent: never sent again
        ASSERT_TRUE(post);
        failed_post += post->status() == 502 ? 1 : 0;
        (void)post->text();
    }
    for (int i : range(4)) {
        (void)i;
        net::http::request streamed("PUT", proxy.url());
        streamed.set_body(io::reader(make_tracked<io::buffer>(sgcl::string("stream"))));   // chunked: streamed on, not kept
        auto s = c.send(streamed);
        ASSERT_TRUE(s);
        failed_stream += s->status() == 502 ? 1 : 0;
        (void)s->text();
    }
    EXPECT_EQ(retried_put, 4);
    EXPECT_EQ(failed_post, 2);
    EXPECT_EQ(failed_stream, 2);
    // no retries asked: a GET that meets the dead one is a 502
    net::http::reverse_proxy::options none;
    none.retries = 0;
    none.max_fails = 0;
    net::http::test_server p2(front(net::http::reverse_proxy({dead, live.ts.url()}, none)));
    int bad = 0;
    for (int i : range(4)) {
        (void)i;
        bad += get(p2) == "502 Bad Gateway\n" ? 1 : 0;
    }
    EXPECT_EQ(bad, 2);
}

TEST(HttpReverseProxyBalance_Tests, ActiveHealthChecks) {
    Named a("a"), b("b");
    net::http::reverse_proxy::options o;
    o.health_path = "health";   // a slash added
    o.health_interval = 30ms;
    o.health_timeout = 500ms;
    net::http::reverse_proxy p({a.ts.url(), b.ts.url()}, o);
    net::http::test_server proxy(front(p));
    b.health = 503;
    for (int i : range(200)) {
        (void)i;
        if (!p.backends()[1].healthy) {
            break;
        }
        std::this_thread::sleep_for(5ms);
    }
    ASSERT_FALSE(p.backends()[1].healthy);
    for (int i : range(6)) {
        (void)i;
        EXPECT_EQ(get(proxy), "200 a");
    }
    b.health = 204;
    for (int i : range(200)) {
        (void)i;
        if (p.backends()[1].healthy) {
            break;
        }
        std::this_thread::sleep_for(5ms);
    }
    ASSERT_TRUE(p.backends()[1].healthy);
    std::set<std::string> seen;
    for (int i : range(4)) {
        (void)i;
        seen.insert(get(proxy));
    }
    EXPECT_EQ(seen.size(), 2u);
    p.close();   // the checks end
    a.health = 500;
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(p.backends()[0].healthy);
}

TEST(HttpReverseProxyBalance_Tests, ManyBackendsTriedInTurn) {
    // more than 64 backends, all but the last dead: a GET walks them with
    // its retries, each tried once
    Named live("live");
    vector<sgcl::string> urls;
    for (int i : range(70)) {
        (void)i;
        urls.push_back(dead_url());
    }
    urls.push_back(live.ts.url());
    net::http::reverse_proxy::options o;
    o.retries = 100;
    o.max_fails = 0;
    net::http::reverse_proxy p(urls, o);
    net::http::test_server proxy(front(p));
    EXPECT_EQ(get(proxy), "200 live");
    uint64_t failures = 0;
    for (auto& b : p.backends()) {
        failures += b.failures;
        EXPECT_LE(b.requests, 1u);
    }
    EXPECT_EQ(failures, 70u);
}
