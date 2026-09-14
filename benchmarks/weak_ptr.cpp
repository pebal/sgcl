//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of a weak pointer: SGCL's weak_ptr (a tracked_ptr to a cell the
// collector clears) against std::weak_ptr (a second reference count).
//   weak_ptr <sgcl|gc|shared> [threads=1] [op=lock|copy|make]   (gc: gc::tracked_ptr and gc::weak_ptr)
//   lock: a weak pointer to a live object locked, the strong pointer dropped
//   copy: a weak pointer copied into a local
//   make: a weak pointer made from a strong pointer (SGCL: a cell allocated)
// Prints nanoseconds per operation.
#include "common.h"
#include "sgcl/sgcl.h"
#include <memory>
namespace {
    struct Node {   // a line of its own: the threads' control blocks must not share one
        long v;
        char pad[120];
    };
    const long iters = 20'000'000;
    template<class Strong, class Weak, class Make>
    double run(int threads, const char* op, Make make) {
        std::vector<std::thread> ws;
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&] {
                Strong strong = make();
                Weak weak = strong;
                long sum = 0;
                if (!std::strcmp(op, "lock")) {
                    for (long i = 0; i < iters; ++i) {
                        if (auto p = weak.lock()) {
                            sum += p->v;
                        }
                    }
                } else if (!std::strcmp(op, "copy")) {
                    for (long i = 0; i < iters; ++i) {
                        Weak copy = weak;
                        sum += copy.expired();
                    }
                } else {
                    for (long i = 0; i < iters; ++i) {
                        Weak made = strong;
                        sum += made.expired();
                    }
                }
                asm volatile("" : : "r"(sum) : "memory");
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        return bench::seconds_since(t0) * 1e9 / iters;
    }
}
int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "gc", "shared"})) {
        std::fprintf(stderr, "usage: weak_ptr <sgcl|gc|shared> [threads] [lock|copy|make]\n");
        return 2;
    }
    int threads = argc > 2 ? std::atoi(argv[2]) : 1;
    const char* op = argc > 3 ? argv[3] : "lock";
    double ns = !std::strcmp(variant, "sgcl")
        ? run<sgcl::tracked_ptr<Node>, sgcl::weak_ptr<Node>>(threads, op, [] { return sgcl::make_tracked<Node>(); })
        : !std::strcmp(variant, "gc")
        ? run<gc::tracked_ptr<Node>, gc::weak_ptr<Node>>(threads, op, [] { return gc::make_tracked<Node>(); })
        : run<std::shared_ptr<Node>, std::weak_ptr<Node>>(threads, op, [] { return std::make_shared<Node>(); });
    std::printf("%s threads=%d op=%s ns/op=%.2f\n", variant, threads, op, ns);
    return 0;
}
