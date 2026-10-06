//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::webdav: the module's server of a temporary directory and its client,
// over one connection. One case per run; prints one line, ns per operation,
// for compare.sh (CASES=webdav). Go's standard library has no WebDAV
// (x/net/webdav is not taken): the Go side (benchmarks/go/webdav) is
// net/http's server and client, GET by http.FileServer, PUT and PROPFIND by
// hand (encoding/xml), the same tree.
//
//   webdav server DIR          the server of DIR under /dav on 127.0.0.1: prints "port N", serves until killed
//   webdav <case> sgcl ADDR [n] the module's client against the server at ADDR:
//     webdav_get               GET of a file of 64 KB: per request
//     webdav_put               PUT of 64 KB: per request
//     webdav_propfind          PROPFIND, Depth 1, of a collection of 100 files, read into resources: per request
#include "benchmarks/common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/http.h"
#include "sgcl/net/webdav.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;
    namespace dav = sgcl::net::webdav;

    void report(const char* what, double wall, double ops) {
        std::printf("webdav %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: webdav server DIR | webdav <webdav_get|webdav_put|webdav_propfind> sgcl ADDR [n]\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what == "server") {
        dav::server d(string(argv[2]), {.prefix = "/dav"});
        net::http::server srv;
        srv.route("/dav/", [d](net::http::request r, net::http::response_writer w) { return d.async_serve(r, w); });
        auto l = net::tcp::listen("127.0.0.1:0").value();
        std::printf("port %u\n", unsigned(l.local_endpoint().port()));
        std::fflush(stdout);
        (void)srv.serve(l);
        return 0;
    }
    if (argc < 4) {
        return 2;
    }
    dav::client c(string::concat("http://", string(argv[3]), "/dav/"));
    long n = argc > 4 ? std::atol(argv[4]) : 0;
    bool ok = true;
    string body(std::string(65536, 'w'));
    auto run = [&]() -> async::task<> {
        if (what == "webdav_get") {
            n = n ? n : 20000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto r = co_await c.async_read(string("/data.bin"));
                ok &= r && r->size() == 65536;
            }
            report("webdav_get", bench::seconds_since(t0), double(n));
        } else if (what == "webdav_put") {
            n = n ? n : 10000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await c.async_write(string("/put.bin"), body));
            }
            report("webdav_put", bench::seconds_since(t0), double(n));
        } else if (what == "webdav_propfind") {
            n = n ? n : 5000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto r = co_await c.async_list(string("/many"));
                ok &= r && r->size() == 100;
            }
            report("webdav_propfind", bench::seconds_since(t0), double(n));
        } else {
            ok = false;
        }
    };
    async::spawn(run()).wait();
    return ok ? 0 : 1;
}
