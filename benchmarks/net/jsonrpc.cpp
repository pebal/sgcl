//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::jsonrpc: the module's peers over TCP, one JSON a line, and its
// methods on their own. One case per run; prints one line, ns per
// operation, for compare.sh (CASES=jsonrpc). The Go side
// (benchmarks/go/jsonrpc) is Go's net/rpc/jsonrpc, client and server (JSON-RPC
// 1.0 over a stream of JSON values: the same exchange of an object each way).
//
//   jsonrpc server               the module's server on 127.0.0.1: prints "port N", serves until killed
//   jsonrpc <case> sgcl ADDR [n] the module's peer against the server at ADDR:
//     jsonrpc_call               "add" of two integers, one call at a time: per call
//     jsonrpc_parallel           the same from 64 tasks at once over one connection: per call
//   jsonrpc jsonrpc_handle sgcl [n]  a request's text handled by the methods into the response's text (no network): per request
#include "benchmarks/common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/jsonrpc.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;
    namespace rpc = sgcl::net::jsonrpc;
    using encoding::json;

    void report(const char* what, double wall, double ops) {
        std::printf("jsonrpc %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    rpc::methods arith() {
        rpc::methods m;
        m.add("add", [](const json& p) -> expected<json, rpc::error> { return json(p["a"].as_int(0) + p["b"].as_int(0)); });
        return m;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: jsonrpc server | jsonrpc <jsonrpc_call|jsonrpc_parallel> sgcl ADDR [n] | jsonrpc jsonrpc_handle sgcl [n]\n");
        return 2;
    }
    const std::string what = argv[1];
    rpc::peer::options o;
    o.framing = rpc::framing::line;
    if (what == "server") {
        auto l = net::tcp::listen("127.0.0.1:0").value();
        std::printf("port %u\n", unsigned(l.local_endpoint().port()));
        std::fflush(stdout);
        o.methods = arith();
        async::spawn([](net::listener l, rpc::peer::options o) -> async::task<> {
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    co_return;
                }
                async::go([](rpc::peer p) -> async::task<> { (void)co_await p.async_wait(); }(rpc::peer::connect(*c, o)));
            }
        }(l, o)).wait();
        return 0;
    }
    if (what == "jsonrpc_handle") {
        long n = argc > 3 ? std::atol(argv[3]) : 300000;
        rpc::methods m = arith();
        string text(R"({"jsonrpc":"2.0","method":"add","params":{"a":20,"b":22},"id":1})");
        size_t total = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            total += m.handle(text)->size();
        }
        report("jsonrpc_handle", bench::seconds_since(t0), double(n));
        return total ? 0 : 1;
    }
    if (argc < 4) {
        return 2;
    }
    auto c = net::tcp::connect(string(argv[3])).value();
    auto p = rpc::peer::connect(c, o);
    long n = argc > 4 ? std::atol(argv[4]) : 0;
    bool ok = true;
    auto args = json::object({{string("a"), json(20)}, {string("b"), json(22)}});
    auto run = [&]() -> async::task<> {
        if (what == "jsonrpc_call") {
            n = n ? n : 50000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto r = co_await p.async_call(string("add"), args);
                ok &= r && r->as_int(0) == 42;
            }
            report("jsonrpc_call", bench::seconds_since(t0), double(n));
        } else if (what == "jsonrpc_parallel") {
            n = n ? n : 640000;
            auto t0 = bench::Clock::now();
            vector<async::task<bool>> all;
            for (int k = 0; k < 64; ++k) {
                all.push_back(async::spawn([](rpc::peer p, json args, long m) -> async::task<bool> {
                    bool ok = true;
                    for (long i = 0; i < m; ++i) {
                        auto r = co_await p.async_call(string("add"), args);
                        ok &= r && r->as_int(0) == 42;
                    }
                    co_return ok;
                }(p, args, n / 64)));
            }
            for (auto& t : all) {
                ok &= co_await t;
            }
            report("jsonrpc_parallel", bench::seconds_since(t0), double(n));
        } else {
            ok = false;
        }
        p.close();
    };
    async::spawn(run()).wait();
    return ok ? 0 : 1;
}
