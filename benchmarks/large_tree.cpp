//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A large tree held for the whole run while threads make and drop small
// trees: the live set is large and old, the garbage young and plentiful,
// so every full cycle pays for the large tree and a generational cycle
// does not (config.h: SGCL_GENERATIONAL). The unique_ptr and shared_ptr
// variants free the small trees as they drop them, recursively, on the
// thread that made them. Same shape in benchmarks/go and benchmarks/java.
//   large_tree <sgcl|gc|unique|shared> [big_depth=22] [small_depth=8] [iterations=100000] [threads=1]   (gc: gc::tracked_ptr)
// Prints wall and process CPU time, small trees per second and, for SGCL,
// the collector's cycles.
#include "common.h"
#include "sgcl/sgcl.h"

#include <atomic>
#include <memory>
#include <thread>

namespace {
    // Ptr: sgcl::tracked_ptr, or gc::tracked_ptr for the gc variant
    template<template<class> class Ptr>
    struct SgclTree {
        Ptr<SgclTree> left, right;
        static Ptr<SgclTree> make(int depth) {
            Ptr<SgclTree> n = sgcl::make_tracked<SgclTree>();
            if (depth > 0) {
                n->left = make(depth - 1);
                n->right = make(depth - 1);
            }
            return n;
        }
    };

    struct UniqueTree {
        std::unique_ptr<UniqueTree> left, right;
        static std::unique_ptr<UniqueTree> make(int depth) {
            auto n = std::make_unique<UniqueTree>();
            if (depth > 0) {
                n->left = make(depth - 1);
                n->right = make(depth - 1);
            }
            return n;
        }
    };

    struct SharedTree {
        std::shared_ptr<SharedTree> left, right;
        static std::shared_ptr<SharedTree> make(int depth) {
            auto n = std::make_shared<SharedTree>();
            if (depth > 0) {
                n->left = make(depth - 1);
                n->right = make(depth - 1);
            }
            return n;
        }
    };

    template<class P>
    long check(const P& n) {
        return n->left ? 1 + check(n->left) + check(n->right) : 1;
    }

    template<class Tree>
    void run(const char* variant, int big, int small, long iterations, int threads) {
        auto t0 = bench::Clock::now();
        auto large = Tree::make(big);
        auto built = bench::seconds_since(t0);
        auto stats0 = sgcl::collector::get_statistics();
        gc::atomic<long> sum = {0};
        auto t1 = bench::Clock::now();
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&] {
                long s = 0;
                for (long i = 0; i < iterations; ++i) {
                    auto tree = Tree::make(small);
                    s += check(tree);
                }
                sum += s;
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        auto loop = bench::seconds_since(t1);
        auto stats = sgcl::collector::get_statistics();
        std::printf("large tree of depth %d (%ld nodes) built in %.2fs, check %ld\n", big, (1L << (big + 1)) - 1, built, check(large));
        std::printf("%s threads=%d small=%d iterations=%ld sum=%ld wall=%.2fs cpu=%.2fs trees/s=%.0f",
                    variant, threads, small, iterations, sum.load(), loop, bench::cpu_seconds(), iterations * threads / loop);
        if (std::is_same_v<Tree, SgclTree<sgcl::tracked_ptr>> || std::is_same_v<Tree, SgclTree<gc::tracked_ptr>>) {
            std::printf(" cycles=%zu full=%zu live=%zu", stats.cycles - stats0.cycles, stats.full_cycles - stats0.full_cycles, stats.live_objects);
        }
        std::printf("\n");
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "gc", "unique", "shared"})) {
        std::fprintf(stderr, "usage: large_tree <sgcl|gc|unique|shared> [big_depth] [small_depth] [iterations] [threads]\n");
        return 2;
    }
    int big = argc > 2 ? std::atoi(argv[2]) : 22;
    int small = argc > 3 ? std::atoi(argv[3]) : 8;
    long iterations = argc > 4 ? std::atol(argv[4]) : 100000;
    int threads = argc > 5 ? std::atoi(argv[5]) : 1;
    if (!std::strcmp(variant, "sgcl")) {
        run<SgclTree<sgcl::tracked_ptr>>(variant, big, small, iterations, threads);
    } else if (!std::strcmp(variant, "gc")) {
        run<SgclTree<gc::tracked_ptr>>(variant, big, small, iterations, threads);
    } else if (!std::strcmp(variant, "unique")) {
        run<UniqueTree>(variant, big, small, iterations, threads);
    } else {
        run<SharedTree>(variant, big, small, iterations, threads);
    }
    return 0;
}
