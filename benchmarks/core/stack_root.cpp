//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of a tracked_ptr on the stack: construction and destruction of a
// local in a tight loop (no shared_ptr counterpart: its stack copy is the
// reference count measured by write_barrier).
//   stack_root [threads=1] [mode=copy|null]
//   mode copy: tracked_ptr<Node> p = src;  (constructor + barrier + destructor)
//   mode null: tracked_ptr<Node> p;        (constructor + destructor only)
// Prints nanoseconds per construction.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

struct Node { long v; };

int main(int argc, char** argv) {
    int threads = argc > 1 ? std::atoi(argv[1]) : 1;
    const char* mode = argc > 2 ? argv[2] : "copy";
    const long iters = 50'000'000;
    std::vector<std::thread> ws;
    auto t0 = bench::Clock::now();
    for (int t = 0; t < threads; ++t) ws.emplace_back([&] {
        sgcl::tracked_ptr<Node> src = sgcl::make_tracked<Node>();
        if (!std::strcmp(mode, "copy")) {
            for (long i = 0; i < iters; ++i) {
                sgcl::tracked_ptr<Node> p = src;
                asm volatile("" : : "r"(&p) : "memory");
            }
        } else {
            for (long i = 0; i < iters; ++i) {
                sgcl::tracked_ptr<Node> p;
                asm volatile("" : : "r"(&p) : "memory");
            }
        }
    });
    for (auto& w : ws) w.join();
    std::printf("sgcl threads=%d mode=%s ns/construction=%.2f\n", threads, mode, bench::seconds_since(t0) * 1e9 / iters);
}
