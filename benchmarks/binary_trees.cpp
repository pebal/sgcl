//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The binary-trees benchmark (Computer Language Benchmarks Game): allocation
// and destruction of many short-lived trees next to one long-lived tree.
//   binary_trees <sgcl|shared|unique|raw> [max_depth=21] [threads=1]
// The work for the depths 4, 6, ..., max is split over the threads. Prints
// wall time and process CPU time: for SGCL the destruction moves to the
// collector's thread and shows up in the CPU time, not the wall time.
#include "common.h"
#include "sgcl/sgcl.h"

#include <atomic>
#include <memory>

namespace {
    template<class Ptr>
    struct Node {
        Ptr left;
        Ptr right;
    };

    struct Sgcl {
        using N = Node<sgcl::tracked_ptr<void>>;
        struct Tree { sgcl::tracked_ptr<Tree> left, right; };
        using P = sgcl::tracked_ptr<Tree>;
        static P make(int depth) {
            auto n = sgcl::make_tracked<Tree>();
            if (depth > 0) {
                n->left = make(depth - 1);
                n->right = make(depth - 1);
            }
            return n;
        }
        static long check(const P& n) {
            return n->left ? 1 + check(n->left) + check(n->right) : 1;
        }
        static void free(P&) {}
    };

    struct Shared {
        struct Tree { std::shared_ptr<Tree> left, right; };
        using P = std::shared_ptr<Tree>;
        static P make(int depth) {
            auto n = std::make_shared<Tree>();
            if (depth > 0) {
                n->left = make(depth - 1);
                n->right = make(depth - 1);
            }
            return n;
        }
        static long check(const P& n) {
            return n->left ? 1 + check(n->left) + check(n->right) : 1;
        }
        static void free(P&) {}
    };

    struct Unique {
        struct Tree { std::unique_ptr<Tree> left, right; };
        using P = std::unique_ptr<Tree>;
        static P make(int depth) {
            auto n = std::make_unique<Tree>();
            if (depth > 0) {
                n->left = make(depth - 1);
                n->right = make(depth - 1);
            }
            return n;
        }
        static long check(const P& n) {
            return n->left ? 1 + check(n->left) + check(n->right) : 1;
        }
        static void free(P&) {}
    };

    struct Raw {
        struct Tree { Tree* left = nullptr; Tree* right = nullptr; };
        using P = Tree*;
        static P make(int depth) {
            auto n = new Tree;
            if (depth > 0) {
                n->left = make(depth - 1);
                n->right = make(depth - 1);
            }
            return n;
        }
        static long check(const P& n) {
            return n->left ? 1 + check(n->left) + check(n->right) : 1;
        }
        static void free(P& n) {
            if (n->left) {
                free(n->left);
                free(n->right);
            }
            delete n;
            n = nullptr;
        }
    };

    template<class V>
    void run(int max_depth, int threads) {
        const int min_depth = 4;
        max_depth = std::max(min_depth + 2, max_depth);
        auto t0 = bench::Clock::now();
        {
            auto stretch = V::make(max_depth + 1);
            std::printf("stretch tree of depth %d\t check: %ld\n", max_depth + 1, V::check(stretch));
            V::free(stretch);
        }
        auto long_lived = V::make(max_depth);
        std::vector<std::string> lines((max_depth - min_depth) / 2 + 1);
        std::atomic<int> next = {0};
        auto worker = [&] {
            for (;;) {
                int i = next.fetch_add(1);
                if (i >= (int)lines.size()) {
                    break;
                }
                int depth = min_depth + 2 * i;
                long iterations = 1L << (max_depth - depth + min_depth);
                long check = 0;
                for (long j = 0; j < iterations; ++j) {
                    auto tree = V::make(depth);
                    check += V::check(tree);
                    V::free(tree);
                }
                lines[i] = std::to_string(iterations) + "\t trees of depth " + std::to_string(depth) + "\t check: " + std::to_string(check);
            }
        };
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back(worker);
        }
        for (auto& w : ws) {
            w.join();
        }
        for (auto& l : lines) {
            std::printf("%s\n", l.c_str());
        }
        std::printf("long lived tree of depth %d\t check: %ld\n", max_depth, V::check(long_lived));
        V::free(long_lived);
        std::printf("wall=%.2fs cpu=%.2fs\n", bench::seconds_since(t0), bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "shared", "unique", "raw"})) {
        std::fprintf(stderr, "usage: binary_trees <sgcl|shared|unique|raw> [max_depth] [threads]\n");
        return 2;
    }
    int max_depth = argc > 2 ? std::atoi(argv[2]) : 21;
    int threads = argc > 3 ? std::atoi(argv[3]) : 1;
    if (!std::strcmp(variant, "sgcl")) run<Sgcl>(max_depth, threads);
    else if (!std::strcmp(variant, "shared")) run<Shared>(max_depth, threads);
    else if (!std::strcmp(variant, "unique")) run<Unique>(max_depth, threads);
    else run<Raw>(max_depth, threads);
}
