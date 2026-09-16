//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of a weak pointer: SGCL's weak_ptr (a tracked_ptr to a cell the
// collector clears) against std::weak_ptr (a second reference count).
//   weak_ptr <sgcl|shared> [threads=1] [op=lock|copy|make|expired]
//   lock: a weak pointer to a live object locked, the strong pointer dropped
//   expired: a weak pointer to a dead object locked: the null answer
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

    // A weak pointer whose object is dropped: made on a thread of its
    // own, which exits, so that no stale word of any live stack keeps the
    // object (README, "Stack roots"); the weak pointer comes back through
    // a local of this frame, assigned from the other thread
    template<class Strong, class Weak, class Make>
    Weak make_dropped(Make& make) {
        if constexpr (std::is_same_v<Strong, std::shared_ptr<Node>>) {
            return Weak(make());   // the count drops to zero here
        } else {
            sgcl::weak_ptr<Node> out;
            std::thread([&] {
                Strong strong = make();
                out = sgcl::weak_ptr<Node>(strong);
            }).join();
            return Weak(out);
        }
    }

    template<class Strong, class Weak, class Make>
    double run(int threads, const char* op, Make make) {
        std::vector<std::thread> ws;
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&] {
                Strong strong;
                Weak weak;
                if (!std::strcmp(op, "expired")) {
                    // the object dropped and collected (or its count gone): the
                    // weak pointer answers null from then on
                    // a stale word of some frame may keep one object through
                    // the cycles (README, "Stack roots"): another is made then
                    for (int attempt = 0; attempt < 8; ++attempt) {
                        weak = make_dropped<Strong, Weak>(make);
                        if constexpr (std::is_same_v<Strong, std::shared_ptr<Node>>) {
                            break;
                        } else {
                            sgcl::collector::clear_stack();
                            for (int i = 0; i < 3; ++i) {
                                sgcl::collector::force_collect(true);
                            }
                            if (weak.expired()) {
                                break;
                            }
                        }
                    }
                    if (!weak.expired()) {
                        std::fprintf(stderr, "no object expired: the numbers below are of a live one\n");
                    }
                } else {
                    strong = make();
                    weak = strong;
                }
                long sum = 0;
                if (!std::strcmp(op, "lock") || !std::strcmp(op, "expired")) {
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
    if (!bench::has_variant(variant, {"sgcl", "shared"})) {
        std::fprintf(stderr, "usage: weak_ptr <sgcl|shared> [threads] [lock|copy|make|expired]\n");
        return 2;
    }
    int threads = argc > 2 ? std::atoi(argv[2]) : 1;
    const char* op = argc > 3 ? argv[3] : "lock";
    double ns = !std::strcmp(variant, "sgcl")
        ? run<sgcl::tracked_ptr<Node>, sgcl::weak_ptr<Node>>(threads, op, [] { return sgcl::make_tracked<Node>(); })
        : run<std::shared_ptr<Node>, std::weak_ptr<Node>>(threads, op, [] { return std::make_shared<Node>(); });
    std::printf("%s threads=%d op=%s ns/op=%.2f\n", variant, threads, op, ns);
    return 0;
}
