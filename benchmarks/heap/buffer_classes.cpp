//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The size classes of the managed buffers (core/detail/maker.h): what a
// buffer costs to make and how many pages a mix of buffers fills. One case
// per process, one line of named fields:
//   bench_buffer_classes reserve <n>    vector<int>: reserve(n), n push_backs; ns per vector
//   bench_buffer_classes grow <n>       vector<int>: n push_backs from empty; ns per vector
//   bench_buffer_classes alloc <n>      a buffer of n ints made and dropped; ns per buffer
//   bench_buffer_classes pages          10 000 buffers of 1-64 KB alive after a full cycle: pages
// A timed case runs 0.3 s untimed first (the first case in a process
// reads high), then ~2 s.
#include "sgcl/core/core.h"
#include "benchmarks/common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

using namespace sgcl;

namespace {
    volatile size_t sink = 0;

    template<class F>
    double ns_per_call(F&& f) {
        auto run = [&](double seconds) {
            size_t calls = 0;
            auto t0 = bench::Clock::now();
            double elapsed;
            do {
                for (int i = 0; i < 64; ++i) {
                    f();
                }
                calls += 64;
                elapsed = bench::seconds_since(t0);
            } while (elapsed < seconds);
            return elapsed * 1e9 / double(calls);
        };
        run(0.3);
        return run(2.0);
    }

    void reserve(size_t n) {
        double ns = ns_per_call([n] {
            vector<int> v;
            v.reserve(n);
            for (size_t i = 0; i < n; ++i) {
                v.push_back(int(i));
            }
            sink += v.size();
        });
        std::printf("case=reserve n=%zu ns=%.1f ns_per_element=%.3f\n", n, ns, ns / double(n));
    }

    void grow(size_t n) {
        double ns = ns_per_call([n] {
            vector<int> v;
            for (size_t i = 0; i < n; ++i) {
                v.push_back(int(i));
            }
            sink += v.size();
        });
        std::printf("case=grow n=%zu ns=%.1f ns_per_element=%.3f\n", n, ns, ns / double(n));
    }

    // The allocation alone: a buffer of n ints (the count read from memory,
    // so that the class is chosen at run time), dropped at once
    void alloc(size_t n) {
        static volatile size_t count;
        count = n;
        double ns = ns_per_call([] {
            auto p = sgcl::detail::Maker<int[]>::make_tracked_data(count);
            sink += size_t(p.get() != nullptr);
        });
        std::printf("case=alloc n=%zu ns=%.2f\n", n, ns);
    }

    // 10 000 buffers of sizes drawn uniformly from 1 KB to 64 KB (a fixed
    // seed), alive through a full cycle: the pages they hold, against the
    // pages their bytes would fill packed end to end
    void pages() {
        using sgcl::detail::MemoryCounters;
        const size_t count = 10000;
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
        auto before = MemoryCounters::live_pages();
        size_t bytes = 0;
        {
            vector<dynamic_array<std::byte>> kept;
            kept.reserve(count);
            std::mt19937_64 random(42);
            std::uniform_int_distribution<size_t> size(1024, 65536);
            for (size_t i = 0; i < count; ++i) {
                auto n = size(random);
                bytes += n;
                kept.emplace_back(n);
            }
            for (int i = 0; i < 3; ++i) {
                collector::force_collect(true);
            }
            auto used = MemoryCounters::live_pages() - before;
            auto packed = (bytes + config::page_size - 1) / config::page_size;
            std::printf("case=pages count=%zu bytes=%zu pages=%zu packed=%zu overhead=%.1f%%\n",
                count, bytes, used, packed, 100.0 * (double(used) / double(packed) - 1));
            sink += kept.size();
        }
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* c = argc > 1 ? argv[1] : "pages";
    size_t n = argc > 2 ? size_t(std::atol(argv[2])) : 1000;
    if (!std::strcmp(c, "reserve")) {
        reserve(n);
    } else if (!std::strcmp(c, "grow")) {
        grow(n);
    } else if (!std::strcmp(c, "alloc")) {
        alloc(n);
    } else {
        pages();
    }
    return sink == 42 ? 1 : 0;
}
