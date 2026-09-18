//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The cost of a cycle over a large live graph: a binary tree of 2^(depth+1)-1
// nodes (three pointers each) and, optionally, a list, built once; then
// forced cycles on the stable heap.
//   marking [depth=20] [cycles=8] [list nodes, thousands=0]
// Prints the live count and the best and average wall time of one forced
// collection (collector::force_collect(true), which waits for a cycle that
// removed nothing: two cycles' worth of latency, the marking of the graph
// being most of each). The marking pass alone is timed by a build with
// -DSGCL_MARK_STATS, which prints every pass to stderr and reads the
// helper count from SGCL_MARK_WORKERS (0: the collector thread alone, k: k
// helpers):
//   clang++ -std=c++20 -O2 -DNDEBUG -I. -DSGCL_MARK_STATS
//       -DSGCL_MARK_OBJECT_THRESHOLD=1024 -DSGCL_HELPERS_GROWTH_THRESHOLD=0
//       benchmarks/marking.cpp -o marking
//   SGCL_MARK_WORKERS=4 ./marking 20 4 2>&1 | grep '^\[mark\]'
#include "sgcl/sgcl.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace {
    struct Node {
        sgcl::tracked_ptr<Node> left, right, next;
        long value;
    };

    sgcl::tracked_ptr<Node> make(long depth) {
        auto n = sgcl::make_tracked<Node>();
        n->value = depth;
        if (depth > 0) {
            n->left = make(depth - 1);
            n->right = make(depth - 1);
        }
        return n;
    }
}

int main(int argc, char** argv) {
    long depth = argc > 1 ? std::atol(argv[1]) : 20;
    int cycles = argc > 2 ? std::atoi(argv[2]) : 8;
    long list = argc > 3 ? std::atol(argv[3]) * 1000 : 0;
    auto root = make(depth);
    sgcl::tracked_ptr<Node> head;
    for (long i = 0; i < list; ++i) {
        auto n = sgcl::make_tracked<Node>();
        n->value = i;
        n->next = head;
        head = std::move(n);
    }
    sgcl::collector::force_collect(true);
    sgcl::collector::force_collect(true);
    auto live = sgcl::collector::get_live_object_count();
    double best = 1e9, sum = 0;
    for (int i = 0; i < cycles; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        sgcl::collector::force_collect(true);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        best = std::min(best, ms);
        sum += ms;
    }
    std::printf("live=%zu best=%.2fms avg=%.2fms\n", live, best, sum / cycles);
    return (int)(root->value + (head ? head->value : 0)) & 1;
}
