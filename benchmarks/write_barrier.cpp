//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of copying a pointer: for SGCL the write barrier, for shared_ptr the
// reference count, for unique_ptr the raw pointer a program built on it
// hands around (unique_ptr itself has no copy). Tight loops, no
// allocation in the loop.
//   write_barrier <sgcl|unique|shared> [threads=1] [mode=stack|heap] [targets=1] [shared]
//   mode stack: local = objs[i]         (root store)
//   mode heap:  holder->next = objs[i]  (field store)
//   targets: number of distinct pointees cycled through (1 = one hot line)
//   shared: every thread copies pointers to the same objects (contention)
// Prints nanoseconds per copy.
#include "common.h"
#include "sgcl/sgcl.h"

#include <memory>

namespace {
    struct SgclNode {
        long v;
        sgcl::tracked_ptr<SgclNode> next;
    };

    struct SharedNode {
        long v;
        std::shared_ptr<SharedNode> next;
    };

    struct UniqueNode {
        long v;
        UniqueNode* next;   // a non-owning pointer: the owner is a unique_ptr elsewhere
    };

    const long iters = 50'000'000;

    double run_sgcl(int threads, const char* mode, int targets, bool shared) {
        sgcl::vector<sgcl::tracked_ptr<SgclNode>> shared_objs;
        if (shared) {
            for (int i = 0; i < targets; ++i) {
                shared_objs.push_back(sgcl::make_tracked<SgclNode>());
            }
        }
        std::vector<std::thread> ws;
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&] {
                sgcl::vector<sgcl::tracked_ptr<SgclNode>> own;
                if (!shared) {
                    for (int i = 0; i < targets; ++i) {
                        own.push_back(sgcl::make_tracked<SgclNode>());
                    }
                }
                auto& objs = shared ? shared_objs : own;
                sgcl::tracked_ptr<SgclNode> holder = sgcl::make_tracked<SgclNode>();
                if (!std::strcmp(mode, "stack")) {
                    sgcl::tracked_ptr<SgclNode> dst;
                    for (long i = 0; i < iters; ++i) {
                        dst = objs[i % targets];
                    }
                } else {
                    for (long i = 0; i < iters; ++i) {
                        holder->next = objs[i % targets];
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        return bench::seconds_since(t0) * 1e9 / iters;
    }

    double run_shared(int threads, const char* mode, int targets, bool shared) {
        std::vector<std::shared_ptr<SharedNode>> shared_objs;
        if (shared) {
            for (int i = 0; i < targets; ++i) {
                shared_objs.push_back(std::make_shared<SharedNode>());
            }
        }
        std::vector<std::thread> ws;
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&] {
                std::vector<std::shared_ptr<SharedNode>> own;
                if (!shared) {
                    for (int i = 0; i < targets; ++i) {
                        own.push_back(std::make_shared<SharedNode>());
                    }
                }
                auto& objs = shared ? shared_objs : own;
                auto holder = std::make_shared<SharedNode>();
                if (!std::strcmp(mode, "stack")) {
                    std::shared_ptr<SharedNode> dst;
                    for (long i = 0; i < iters; ++i) {
                        dst = objs[i % targets];
                    }
                } else {
                    for (long i = 0; i < iters; ++i) {
                        holder->next = objs[i % targets];
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        return bench::seconds_since(t0) * 1e9 / iters;
    }

    double run_unique(int threads, const char* mode, int targets, bool shared) {
        std::vector<std::unique_ptr<UniqueNode>> shared_objs;
        if (shared) {
            for (int i = 0; i < targets; ++i) {
                shared_objs.push_back(std::make_unique<UniqueNode>());
            }
        }
        std::vector<std::thread> ws;
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&] {
                std::vector<std::unique_ptr<UniqueNode>> own;
                if (!shared) {
                    for (int i = 0; i < targets; ++i) {
                        own.push_back(std::make_unique<UniqueNode>());
                    }
                }
                auto& objs = shared ? shared_objs : own;
                auto holder = std::make_unique<UniqueNode>();
                if (!std::strcmp(mode, "stack")) {
                    UniqueNode* dst = nullptr;
                    for (long i = 0; i < iters; ++i) {
                        dst = objs[i % targets].get();
                        asm volatile("" : : "r"(dst) : "memory");
                    }
                } else {
                    for (long i = 0; i < iters; ++i) {
                        holder->next = objs[i % targets].get();
                        asm volatile("" : : "r"(holder->next) : "memory");
                    }
                }
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
    if (!bench::has_variant(variant, {"sgcl", "unique", "shared"})) {
        std::fprintf(stderr, "usage: write_barrier <sgcl|unique|shared> [threads] [stack|heap] [targets] [shared]\n");
        return 2;
    }
    int threads = argc > 2 ? std::atoi(argv[2]) : 1;
    const char* mode = argc > 3 ? argv[3] : "stack";
    int targets = argc > 4 ? std::atoi(argv[4]) : 1;
    bool shared = argc > 5 && !std::strcmp(argv[5], "shared");
    double ns = !std::strcmp(variant, "sgcl") ? run_sgcl(threads, mode, targets, shared)
        : !std::strcmp(variant, "unique") ? run_unique(threads, mode, targets, shared)
        : run_shared(threads, mode, targets, shared);
    std::printf("%s threads=%d mode=%s targets=%d%s ns/copy=%.2f\n", variant, threads, mode, targets, shared ? " shared" : "", ns);
}
