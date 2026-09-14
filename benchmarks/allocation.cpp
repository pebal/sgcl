//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Allocation throughput per thread: each thread allocates objects of one
// size in a loop and keeps only the newest one, so every allocation also
// retires one object (freed at once by shared_ptr/unique_ptr, by the
// collector for SGCL).
//   allocation <sgcl|gc|shared|unique> [threads=1] [size=32] [iterations=20000000]   (gc: gc::tracked_ptr)
// Prints nanoseconds per allocation (wall time of the slowest thread / n)
// and the process CPU time in seconds.
#include "common.h"
#include "sgcl/sgcl.h"

#include <memory>

template<size_t N>
struct Obj {
    char pad[N];
};

// Ptr: sgcl::tracked_ptr, or gc::tracked_ptr for the gc variant
template<class T, template<class> class Ptr>
double run_sgcl(int threads, long n) {
    std::vector<std::thread> ws;
    auto t0 = bench::Clock::now();
    for (int t = 0; t < threads; ++t) {
        ws.emplace_back([&] {
            Ptr<T> keep;
            for (long i = 0; i < n; ++i) {
                keep = sgcl::make_tracked<T>();
            }
        });
    }
    for (auto& w : ws) {
        w.join();
    }
    return bench::seconds_since(t0) * 1e9 / n;
}

template<class T>
double run_shared(int threads, long n) {
    std::vector<std::thread> ws;
    auto t0 = bench::Clock::now();
    for (int t = 0; t < threads; ++t) {
        ws.emplace_back([&] {
            std::shared_ptr<T> keep;
            for (long i = 0; i < n; ++i) {
                keep = std::make_shared<T>();
            }
        });
    }
    for (auto& w : ws) {
        w.join();
    }
    return bench::seconds_since(t0) * 1e9 / n;
}

template<class T>
double run_unique(int threads, long n) {
    std::vector<std::thread> ws;
    auto t0 = bench::Clock::now();
    for (int t = 0; t < threads; ++t) {
        ws.emplace_back([&] {
            std::unique_ptr<T> keep;
            for (long i = 0; i < n; ++i) {
                keep = std::make_unique<T>();
            }
        });
    }
    for (auto& w : ws) {
        w.join();
    }
    return bench::seconds_since(t0) * 1e9 / n;
}

template<class T>
double run(const char* variant, int threads, long n) {
    if (!std::strcmp(variant, "sgcl")) return run_sgcl<T, sgcl::tracked_ptr>(threads, n);
    if (!std::strcmp(variant, "gc")) return run_sgcl<T, gc::tracked_ptr>(threads, n);
    if (!std::strcmp(variant, "shared")) return run_shared<T>(threads, n);
    return run_unique<T>(threads, n);
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "gc", "shared", "unique"})) {
        std::fprintf(stderr, "usage: allocation <sgcl|gc|shared|unique> [threads] [size: 8|32|256|4096] [iterations]\n");
        return 2;
    }
    int threads = argc > 2 ? std::atoi(argv[2]) : 1;
    int size = argc > 3 ? std::atoi(argv[3]) : 32;
    long n = argc > 4 ? std::atol(argv[4]) : 20'000'000;
    double ns = size == 8 ? run<Obj<8>>(variant, threads, n)
              : size == 256 ? run<Obj<256>>(variant, threads, n / 4)
              : size == 4096 ? run<Obj<4096>>(variant, threads, n / 16)
              : run<Obj<32>>(variant, threads, n);
    std::printf("%s threads=%d size=%d ns/alloc=%.2f cpu=%.2fs\n", variant, threads, size, ns, bench::cpu_seconds());
}
