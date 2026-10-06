//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The concurrent module's sketches: what an add, a question, an estimate
// and a merge cost. One case per run; prints one line, ns per operation,
// for compare.sh (CASES=sketch). benchmarks/go/sketches is the Go side:
// the same structures written out single-threaded over the same XXH3
// (github.com/zeebo/xxh3), Go's standard library having none.
//
//   sketches bloom_add sgcl [n]       bloom_filter(1e6, 0.01).add(i) of numbers, per add
//   sketches bloom_contains sgcl [n]  contains(i) of a filter of a million, half of them in it
//   sketches bloom_count sgcl         approximate_count() of that filter (9.6 Mbit), per call
//   sketches hll_add sgcl [n]         hyperloglog(14).add(i), per add
//   sketches hll_estimate sgcl        estimate() of a sketch of a million, per call
//   sketches hll_merge sgcl           merge() of two sketches of a million each (p 14), per call
//   sketches cms_add sgcl [n]         count_min_sketch(0.001, 0.01).add(i % 100000), per add
//   sketches cms_estimate sgcl [n]    estimate(i % 100000), per call
//   sketches bloom_addpar sgcl [n]    8 threads adding to one filter, per add (no Go counterpart)
#include "benchmarks/common.h"
#include "sgcl/concurrent.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {
    void report(const char* what, double wall, long ops, double sink) {
        std::printf("sketches %s ns/op=%.2f ops/s=%.0f wall=%.2fs cpu=%.2fs%s\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds(), sink < 0 ? " " : "");
    }
}

int main(int argc, char** argv) {
    using namespace sgcl;
    std::string what = argc > 1 ? argv[1] : "";
    std::string v = argc > 2 ? argv[2] : "";
    if (!bench::has_variant(v.c_str(), {"sgcl"})) {
        std::fprintf(stderr, "usage: sketches <bloom_add|bloom_contains|bloom_count|hll_add|hll_estimate|hll_merge|cms_add|cms_estimate|bloom_addpar> sgcl [n]\n");
        return 2;
    }
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    double sink = 0;
    if (what == "bloom_add") {
        n = n ? n : 20'000'000;
        concurrent::bloom_filter f(1'000'000, 0.01);
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sink += f.add(uint64_t(i));
        }
        report("bloom_add", bench::seconds_since(t0), n, sink);
    } else if (what == "bloom_contains") {
        n = n ? n : 20'000'000;
        concurrent::bloom_filter f(1'000'000, 0.01);
        for (uint64_t i = 0; i < 1'000'000; ++i) {
            f.add(i * 2);
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sink += f.contains(uint64_t(i % 2'000'000));
        }
        report("bloom_contains", bench::seconds_since(t0), n, sink);
    } else if (what == "bloom_count") {
        concurrent::bloom_filter f(1'000'000, 0.01);
        for (uint64_t i = 0; i < 1'000'000; ++i) {
            f.add(i);
        }
        n = n ? n : 2000;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sink += f.approximate_count();
        }
        report("bloom_count", bench::seconds_since(t0), n, sink);
    } else if (what == "hll_add") {
        n = n ? n : 50'000'000;
        concurrent::hyperloglog h;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            h.add(uint64_t(i));
        }
        report("hll_add", bench::seconds_since(t0), n, h.estimate());
    } else if (what == "hll_estimate" || what == "hll_merge") {
        concurrent::hyperloglog a, b;
        for (uint64_t i = 0; i < 1'000'000; ++i) {
            a.add(i);
            b.add(i + 500'000);
        }
        n = n ? n : 20'000;
        auto t0 = bench::Clock::now();
        if (what == "hll_estimate") {
            for (long i = 0; i < n; ++i) {
                sink += a.estimate();
            }
        } else {
            for (long i = 0; i < n; ++i) {
                a.merge(b);
            }
            sink = a.estimate();
        }
        report(what.c_str(), bench::seconds_since(t0), n, sink);
    } else if (what == "cms_add" || what == "cms_estimate") {
        n = n ? n : 20'000'000;
        concurrent::count_min_sketch c(0.001, 0.01);
        if (what == "cms_estimate") {
            for (uint64_t i = 0; i < 1'000'000; ++i) {
                c.add(i % 100'000);
            }
        }
        auto t0 = bench::Clock::now();
        if (what == "cms_add") {
            for (long i = 0; i < n; ++i) {
                c.add(uint64_t(i % 100'000));
            }
            sink = double(c.total());
        } else {
            for (long i = 0; i < n; ++i) {
                sink += double(c.estimate(uint64_t(i % 100'000)));
            }
        }
        report(what.c_str(), bench::seconds_since(t0), n, sink);
    } else if (what == "bloom_addpar") {
        n = n ? n : 2'000'000;
        concurrent::bloom_filter f(16'000'000, 0.01);
        std::atomic<long> fresh{0};
        auto t0 = bench::Clock::now();
        {
            std::vector<std::thread> ts;
            for (int t = 0; t < 8; ++t) {
                ts.emplace_back([&, t] {
                    concurrent::bloom_filter mine = f;   // on this thread's stack
                    long k = 0;
                    for (long i = 0; i < n; ++i) {
                        k += mine.add(uint64_t(t) << 40 | uint64_t(i));
                    }
                    fresh += k;
                });
            }
            for (auto& t : ts) {
                t.join();
            }
        }
        report("bloom_addpar", bench::seconds_since(t0), 8 * n, double(fresh));
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
}
