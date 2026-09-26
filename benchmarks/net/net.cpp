//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The net module, stage 1a: sockets on the loopback and addresses. One case
// per run; prints one line, ns per operation, for compare.sh (CASES=net;
// benchmarks/go/net/main.go has the Go side, the same cases).
//
//   net pingpong sgcl [n]   64 B there and back over one TCP connection, both ends tasks: per round trip
//   net stream sgcl [mb]    mb megabytes (1024 by default) one way over one connection, 32 KB writes: per byte, and GB/s
//   net connect sgcl [n]    tcp::async_connect and async_accept on the loopback, then both closed: per connection
//   net parse sgcl [n]      ip_address::parse of a mix of IPv4 and IPv6 text: per address
//   net format sgcl [n]     ip_address::to_string of the same mix: per address
//   net url sgcl [n]        url::parse of a mix of absolute URLs: per URL (Go's url.Parse
//                           reads RFC 3986, not the WHATWG standard: a different amount of work)
//   net http_parse sgcl [n] a request head of twelve fields checked by the server's parser: per head
//                           (Go: http.ReadRequest over a bufio.Reader of the same bytes)
//   net http_hello sgcl [n] GET of a 13-byte body, the module's client and server on the loopback,
//                           one kept connection, one request after another: per request
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"
#include "sgcl/net/net.h"
#include "sgcl/net/url.h"
#include "sgcl/net/http/http.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;

    const char* const Addresses[] = {"127.0.0.1", "10.1.2.3", "192.168.100.200", "8.8.8.8", "::1", "2001:db8::1", "fe80::1:2:3:4", "2001:db8:85a3::8a2e:370:7334", "::ffff:192.0.2.128", "2606:4700:4700::1111"};

    const char* const Urls[] = {"http://example.com/", "https://user:pass@example.com:8443/a/b/c?x=1&y=2#top", "http://10.0.0.1/index.html", "https://[2001:db8::1]:443/", "http://example.com/a/../b/./c/d?q=hello%20world", "ftp://ftp.example.org/pub/file.tar.gz", "https://api.example.com/v1/users/12345/orders?limit=50&offset=100", "ws://chat.example.com/socket"};

    void report(const char* what, double wall, double ops, const char* extra = "") {
        std::printf("net %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%s\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds(), extra);
    }

    task<> echo(net::connection c) {
        (void)co_await c.async_copy_to(c);
        (void)c.close();
    }

    task<> serve(net::listener l, long connections) {
        for (long i = 0; i < connections; ++i) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            go(echo(*c));
        }
    }

    task<long> ping(net::connection c, long n) {
        tracked_ptr<array<byte, 64>> block = make_tracked<array<byte, 64>>();
        slice<byte> buf(block, block->data(), block->size());
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto w = co_await c.async_write(buf);
            if (!w) {
                break;
            }
            auto r = co_await c.async_read_full(buf);
            if (!r) {
                break;
            }
            ++ok;
        }
        (void)c.close();
        co_return ok;
    }

    task<size_t> sink(net::listener l) {
        auto c = co_await l.async_accept();
        if (!c) {
            co_return 0;
        }
        tracked_ptr<array<byte, 32768>> block = make_tracked<array<byte, 32768>>();
        slice<byte> buf(block, block->data(), block->size());
        size_t total = 0;
        for (;;) {
            auto r = co_await c->async_read(buf);
            if (!r || *r == 0) {
                break;
            }
            total += *r;
        }
        (void)c->close();
        co_return total;
    }

    task<size_t> source(net::connection c, size_t bytes) {
        tracked_ptr<array<byte, 32768>> block = make_tracked<array<byte, 32768>>();
        slice<byte> buf(block, block->data(), block->size());
        size_t sent = 0;
        while (sent < bytes) {
            auto w = co_await c.async_write(buf);
            if (!w) {
                break;
            }
            sent += *w;
        }
        (void)c.close();
        co_return sent;
    }

    task<long> accept_all(net::listener l, long n) {
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto c = co_await l.async_accept();
            if (!c) {
                break;
            }
            (void)c->close();
            ++ok;
        }
        co_return ok;
    }

    task<long> connect_all(net::endpoint to, long n) {
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto c = co_await tcp::async_connect(to);
            if (!c) {
                break;
            }
            (void)c->close();
            ++ok;
        }
        co_return ok;
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: net <pingpong|stream|connect|parse|format|url|http_parse|http_hello> sgcl [n]\n");
        return 2;
    }
    std::string what = argv[1];
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    bool ok = true;
    if (what == "pingpong") {
        n = n ? n : 50000;
        auto l = tcp::listen("127.0.0.1:0");
        auto server = spawn(serve(*l, 1));
        auto c = tcp::connect(l->local_endpoint());
        auto t0 = bench::Clock::now();
        long done = spawn(ping(*c, n)).wait();
        report("pingpong", bench::seconds_since(t0), double(n));
        server.wait();
        l->close();
        ok = done == n;
    } else if (what == "stream") {
        size_t mb = n ? size_t(n) : 1024;
        size_t bytes = mb << 20;
        auto l = tcp::listen("127.0.0.1:0");
        auto receiver = spawn(sink(*l));
        auto c = tcp::connect(l->local_endpoint());
        auto t0 = bench::Clock::now();
        size_t sent = spawn(source(*c, bytes)).wait();
        size_t received = receiver.wait();
        double wall = bench::seconds_since(t0);
        char extra[64];
        std::snprintf(extra, sizeof(extra), " GB/s=%.2f", double(received) / wall / 1e9);
        report("stream", wall, double(received), extra);
        l->close();
        ok = sent >= bytes && received == sent;
    } else if (what == "connect") {
        n = n ? n : 2000;   // RUNS of both variants inside the 16384 ephemeral ports a TIME_WAIT of 30 s leaves
        auto l = tcp::listen("127.0.0.1:0");
        auto acceptor = spawn(accept_all(*l, n));
        auto t0 = bench::Clock::now();
        long made = spawn(connect_all(l->local_endpoint(), n)).wait();
        if (made < n) {
            l->close();   // a connect failed (the ports ran out): the accepts end rather than wait for connections that will not come
        }
        long taken = acceptor.wait();
        report("connect", bench::seconds_since(t0), double(n));
        l->close();
        ok = made == n && taken == n;
    } else if (what == "parse" || what == "format") {
        n = n ? n : 20000000;
        const size_t k = std::size(Addresses);
        sgcl::string texts[k];
        ip_address parsed[k];
        for (size_t i = 0; i < k; ++i) {
            texts[i] = Addresses[i];
            parsed[i] = *ip_address::parse(texts[i]);
        }
        size_t check = 0;
        auto t0 = bench::Clock::now();
        if (what == "parse") {
            for (long i = 0; i < n; ++i) {
                auto a = ip_address::parse(texts[size_t(i) % k]);
                check += a->is_v4();
            }
        } else {
            for (long i = 0; i < n; ++i) {
                check += parsed[size_t(i) % k].to_string().size();
            }
        }
        report(what.c_str(), bench::seconds_since(t0), double(n));
        ok = check > 0;
    } else if (what == "url") {
        n = n ? n : 2000000;
        const size_t k = std::size(Urls);
        sgcl::string texts[k];
        for (size_t i = 0; i < k; ++i) {
            texts[i] = Urls[i];
        }
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            auto u = url::parse(texts[size_t(i) % k]);
            check += u->path().size();
        }
        report("url", bench::seconds_since(t0), double(n));
        ok = check > 0;
    } else if (what == "http_parse") {
        n = n ? n : 2000000;
        const std::string text = "GET /api/v1/users/12345?fields=name,email HTTP/1.1\r\nHost: api.example.com\r\nUser-Agent: bench/1.0\r\n"
                                 "Accept: application/json\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: en-US,en;q=0.9\r\n"
                                 "Connection: keep-alive\r\nCookie: session=abc123; theme=dark\r\nReferer: https://example.com/page\r\n"
                                 "Cache-Control: no-cache\r\nX-Request-Id: 7f3c9a2e-1b4d-4e6f-8a9b-0c1d2e3f4a5b\r\n"
                                 "Authorization: Bearer eyJhbGciOiJIUzI1NiJ9.e30.x\r\nX-Forwarded-For: 203.0.113.7\r\n\r\n";
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sgcl::string head(text);
            net::http::detail::RequestLine line;
            net::http::headers h;
            net::http::detail::BodyFraming framing;
            check += net::http::detail::check_request_head(head, line, h, framing) == 0 ? h.size() : 0;
        }
        report("http_parse", bench::seconds_since(t0), double(n));
        ok = check == size_t(n) * 12;
    } else if (what == "http_hello") {
        n = n ? n : 50000;
        net::http::server server;
        server.route("GET /hello", [](net::http::request, net::http::response_writer w) { w.write("hello, world\n"); });
        auto l = tcp::listen("127.0.0.1:0");
        auto serving = spawn(server.async_serve(*l));
        net::http::client client;
        auto url = sgcl::string("http://127.0.0.1:" + std::to_string(l->local_endpoint().port()) + "/hello");
        size_t check = 0;
        auto t0 = bench::Clock::now();
        auto run = [&]() -> task<> {
            for (long i = 0; i < n; ++i) {
                auto res = co_await client.async_get(url);
                if (res) {
                    check += (co_await res->async_text())->size();
                }
            }
        };
        spawn(run()).wait();
        report("http_hello", bench::seconds_since(t0), double(n));
        server.close();
        (void)serving.wait();
        ok = check == size_t(n) * 13;
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    sgcl::async::scheduler::stop();
    return ok ? 0 : 1;
}
