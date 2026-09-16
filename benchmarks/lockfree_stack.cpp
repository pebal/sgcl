//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A Treiber stack shared by every thread (the shape of examples/
// lock_free_stack.cpp). With a collector the stack is the textbook one, a
// compare-exchange on the head and no ABA (a node is never reused while a
// thread still holds it); with shared_ptr the head is the atomic
// shared_ptr of the standard library (std::atomic_load /
// atomic_compare_exchange on a shared_ptr, which the library implements
// with a lock); with unique_ptr the nodes have one owner, so the stack is
// guarded by a mutex, the classic answer without a collector.
// Every compare-exchange variant backs off exponentially after a lost
// exchange (sgcl/detail/backoff.h, config::BackoffMax pauses at most; Go
// and Java the same), the answer of Herlihy and Shavit to many threads at
// one word.
//   lockfree_stack <sgcl|gc|shared|unique> [threads=4] [mode=mixed] [n=1000000]   (gc: gc::tracked_ptr)
// mixed: every thread pushes a node and pops one, n times over.
// pairs: half the threads push n nodes each, the other half pop n each
// (spinning on an empty stack), like the example.
// Prints nanoseconds per operation (a push or a pop) and the process CPU
// time.
#include "common.h"
#include "sgcl/sgcl.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    // Ptr: sgcl::tracked_ptr, or gc::tracked_ptr for the gc variant
    template<template<class> class Ptr>
    struct Sgcl {
        struct Node {
            Ptr<Node> next;
            long value;
        };
        sgcl::atomic<Ptr<Node>> head;
        void push(long v) {
            Ptr<Node> n = sgcl::make_tracked<Node>();
            n->value = v;
            n->next = head.load(std::memory_order_relaxed);
            sgcl::detail::Backoff backoff;
            while (!head.compare_exchange_weak(n->next, n, std::memory_order_release)) {
                backoff();
            }
        }
        long pop() {
            auto h = head.load(std::memory_order_acquire);
            sgcl::detail::Backoff backoff;
            while (h && !head.compare_exchange_weak(h, h->next, std::memory_order_acquire)) {
                backoff();
            }
            return h ? h->value : -1;
        }
    };

    struct Shared {
        struct Node {
            std::shared_ptr<Node> next;
            long value;
        };
        std::shared_ptr<Node> head;
        void push(long v) {
            auto n = std::make_shared<Node>();
            n->value = v;
            n->next = std::atomic_load(&head);
            sgcl::detail::Backoff backoff;
            while (!std::atomic_compare_exchange_weak(&head, &n->next, n)) {
                backoff();
            }
        }
        long pop() {
            auto h = std::atomic_load(&head);
            sgcl::detail::Backoff backoff;
            while (h && !std::atomic_compare_exchange_weak(&head, &h, h->next)) {
                backoff();
            }
            return h ? h->value : -1;
        }
    };

    struct Unique {
        struct Node {
            std::unique_ptr<Node> next;
            long value;
        };
        std::unique_ptr<Node> head;
        std::mutex mutex;
        void push(long v) {
            auto n = std::make_unique<Node>();
            n->value = v;
            std::lock_guard lock(mutex);
            n->next = std::move(head);
            head = std::move(n);
        }
        long pop() {
            std::unique_ptr<Node> h;
            {
                std::lock_guard lock(mutex);
                h = std::move(head);
                if (h) {
                    head = std::move(h->next);
                }
            }
            return h ? h->value : -1;
        }
    };

    template<class S>
    void run(int threads, const std::string& mode, long n) {
        S stack;
        std::vector<std::thread> ws;
        bool pairs = mode == "pairs";
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                long sum = 0;
                if (!pairs) {
                    for (long i = 0; i < n; ++i) {
                        stack.push(i);
                        sum += stack.pop();
                    }
                } else if (t % 2 == 0) {
                    for (long i = 0; i < n; ++i) {
                        stack.push(i);
                    }
                } else {
                    for (long i = 0; i < n;) {
                        auto v = stack.pop();
                        if (v >= 0) {
                            sum += v;
                            ++i;
                        } else {
                            std::this_thread::yield();
                        }
                    }
                }
                if (sum == -1) {
                    std::printf("?");
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        double wall = bench::seconds_since(t0);
        double ops = pairs ? (double)n * threads : 2.0 * n * threads;
        std::printf("threads=%d mode=%s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", threads, mode.c_str(), wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "gc", "shared", "unique"})) {
        std::fprintf(stderr, "usage: lockfree_stack <sgcl|gc|shared|unique> [threads] [mixed|pairs] [n]\n");
        return 2;
    }
    int threads = argc > 2 ? std::atoi(argv[2]) : 4;
    std::string mode = argc > 3 ? argv[3] : "mixed";
    long n = argc > 4 ? std::atol(argv[4]) : 1'000'000;
    if ((mode != "mixed" && mode != "pairs") || (mode == "pairs" && threads % 2)) {
        std::fprintf(stderr, "usage: lockfree_stack <sgcl|shared|unique> [threads] [mixed|pairs (an even number of threads)] [n]\n");
        return 2;
    }
    std::string v = variant;
    if (v == "sgcl") {
        run<Sgcl<sgcl::tracked_ptr>>(threads, mode, n);
    } else if (v == "gc") {
        run<Sgcl<gc::tracked_ptr>>(threads, mode, n);
    } else if (v == "shared") {
        run<Shared>(threads, mode, n);
    } else {
        run<Unique>(threads, mode, n);
    }
    return 0;
}
