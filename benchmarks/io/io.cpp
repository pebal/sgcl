//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The io module: what a child process costs. One case per run; prints one
// line, ns per operation, for compare.sh (CASES=io).
//
//   io run sgcl [n]         command("true").run(): posix_spawn and a wait, from a thread
//   io output sgcl [n]      command("echo", "hello").output(): a pipe, a copying task, the text
//   io asyncrun sgcl [n]    run() from a task: the exit waited for on the reactor
//   io parallel sgcl [n]    n run() of "true" at once, 32 in flight
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

using namespace sgcl::async;

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    sgcl::async::task<long> async_runs(long n) {
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            sgcl::io::command c("true");
            ok += (bool)co_await c.async_run();
        }
        co_return ok;
    }

    sgcl::async::task<long> parallel_runs(long n, long width) {
        long ok = 0;
        for (long done = 0; done < n; done += width) {
            sgcl::vector<sgcl::async::task<sgcl::expected<void, sgcl::io::error>>> batch;
            sgcl::vector<sgcl::io::command> commands;
            for (long i = 0; i < width && done + i < n; ++i) {
                commands.push_back(sgcl::io::command("true"));
            }
            for (auto& c : commands) {
                batch.push_back(sgcl::async::spawn(c.async_run()));
            }
            for (auto& t : batch) {
                ok += (bool)co_await t;
            }
        }
        co_return ok;
    }

    void report(const char* what, double wall, long ops) {
        std::printf("io %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: io <run|output|asyncrun|parallel> sgcl [n]\n");
        return 2;
    }
    std::string what = argv[1];
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    long ok = 0;
    if (what == "run") {
        n = n ? n : 1000;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sgcl::io::command c("true");
            ok += (bool)c.run();
        }
        report("run", bench::seconds_since(t0), n);
    } else if (what == "output") {
        n = n ? n : 1000;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sgcl::io::command c("echo", "hello");
            auto out = c.output();
            ok += out && out->size() == 6;
        }
        report("output", bench::seconds_since(t0), n);
    } else if (what == "asyncrun") {
        n = n ? n : 1000;
        auto t0 = bench::Clock::now();
        ok = sgcl::async::spawn(async_runs(n)).wait();
        report("asyncrun", bench::seconds_since(t0), n);
    } else if (what == "parallel") {
        n = n ? n : 2000;
        auto t0 = bench::Clock::now();
        ok = sgcl::async::spawn(parallel_runs(n, 32)).wait();
        report("parallel", bench::seconds_since(t0), n);
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    return ok == n ? 0 : 1;
}
