//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client's side of the HTTP load test: the module's client POSTing a
// body of N bytes to a server (bench_http_hello's POST /resp?size=16: the
// body read, 16 bytes back), C tasks for D seconds, one request in flight
// each, their connections kept in the client's pool and made before the
// clock. https: the certificate's authority given, the name "localhost";
// HTTP/2 by ALPN when the server offers it. Go's side is load/main.go with
// -method POST -body N. One line:
//
//   bench_http_post|req/s|client cpu us/req|errors N
//
//   SGCL_WORKERS=N bench_http_post <url> <conns> <seconds> <body bytes> [ca.pem]
#include "sgcl/net/http/http.h"
#include "sgcl/io/file.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/resource.h>
#include <vector>

using namespace sgcl;

namespace {
    std::atomic<long> done{0};
    std::atomic<long> errors{0};

    // The process's user and system time, in seconds
    double cpu_seconds() {
        rusage u;
        getrusage(RUSAGE_SELF, &u);
        return double(u.ru_utime.tv_sec + u.ru_stime.tv_sec) + double(u.ru_utime.tv_usec + u.ru_stime.tv_usec) / 1e6;
    }

    // One of the C: requests one after another until the time is up, each
    // response read to its end (its connection back to the pool)
    async::task<> poster(net::http::client c, string url, string body, std::chrono::steady_clock::time_point until) {
        do {
            auto r = co_await c.async_post(url, "application/octet-stream", body);
            if (!r) {
                ++errors;
                continue;
            }
            auto t = co_await r->async_text();
            if (!t) {
                ++errors;
                continue;
            }
            ++done;
        } while (std::chrono::steady_clock::now() < until);
    }

    void run(const net::http::client& c, const string& url, const string& body, int conns, std::chrono::steady_clock::time_point until) {
        std::vector<async::task<>> tasks;
        for (int i = 0; i < conns; ++i) {
            tasks.push_back(async::spawn(poster(c, url, body, until)));
        }
        for (auto& t : tasks) {
            t.wait();
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "bench_http_post <url> <conns> <seconds> <body bytes> [ca.pem]\n");
        return 2;
    }
    string url(argv[1]);
    const int conns = std::atoi(argv[2]);
    const double seconds = std::atof(argv[3]);
    const size_t size = std::strtoull(argv[4], nullptr, 10);
    net::http::client c;
    c.max_idle_per_host = size_t(conns);   // every task's connection kept
    if (argc > 5) {
        auto pem = io::read_text(argv[5]);
        if (!pem) {
            std::fprintf(stderr, "bench_http_post: %s\n", std::string(pem.error().message().view()).c_str());
            return 1;
        }
        c.tls.roots = crypto::x509::certificate_pool::from_pem(*pem);
        c.tls.server_name = string("localhost");
    }
    string body(std::string(size, 'p'));
    run(c, url, body, conns, std::chrono::steady_clock::now());   // one request each: the connections made before the clock
    done = 0;
    errors = 0;
    const double cpu0 = cpu_seconds();
    const auto t0 = std::chrono::steady_clock::now();
    run(c, url, body, conns, t0 + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds)));
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double cpu = cpu_seconds() - cpu0;
    const long n = done.load();
    std::printf("bench_http_post|%.0f|%.1f|errors %ld\n", double(n) / wall, n ? cpu * 1e6 / double(n) : 0.0, errors.load());
    return 0;
}
