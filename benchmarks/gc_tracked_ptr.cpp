//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// gc::tracked_ptr against tracked_ptr where both may live, and gc::tracked_ptr alone where
// only it may: tight loops, one thread.
//   ptr <tracked|gc> <stack|heap|deref|cell>
//   stack: a local constructed from a source and destroyed   (the constructor: location check, barrier)
//   heap:  holder->next = objs[i]                              (a field store)
//   deref: sum += p->v over an array of pointers               (the load and the sign test)
//   cell:  v[i] = src for a std::vector<gc::tracked_ptr> (gc only)      (the cell path: allocated once, then a store into it)
// Prints nanoseconds per operation.
#include "common.h"
#include "sgcl/sgcl.h"

namespace {
    template<template<class> class Ptr>
    struct Node {
        long v = 1;
        Ptr<Node> next;
    };

    const long iters = 50'000'000;
    const int targets = 64;

    template<template<class> class Ptr>
    double run(const char* mode) {
        using N = Node<Ptr>;
        sgcl::vector<Ptr<N>> objs;
        for (int i = 0; i < targets; ++i) {
            objs.push_back(sgcl::make_tracked<N>());
        }
        auto t0 = bench::Clock::now();
        if (!std::strcmp(mode, "stack")) {
            for (long i = 0; i < iters; ++i) {
                Ptr<N> p = objs[i & (targets - 1)];
                asm volatile("" : : "r"(&p) : "memory");
            }
        } else if (!std::strcmp(mode, "heap")) {
            Ptr<N> holder = sgcl::make_tracked<N>();
            for (long i = 0; i < iters; ++i) {
                holder->next = objs[i & (targets - 1)];
            }
        } else if (!std::strcmp(mode, "deref")) {
            long sum = 0;
            for (long i = 0; i < iters; ++i) {
                sum += objs[i & (targets - 1)]->v;
            }
            asm volatile("" : : "r"(sum));
        } else if (!std::strcmp(mode, "cell")) {
            std::vector<gc::tracked_ptr<N>> v(targets);
            for (long i = 0; i < iters; ++i) {
                v[i & (targets - 1)] = objs[(i + 1) & (targets - 1)];
            }
        } else {
            std::fprintf(stderr, "unknown mode %s\n", mode);
            std::exit(1);
        }
        return bench::seconds_since(t0) * 1e9 / iters;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "tracked";
    const char* mode = argc > 2 ? argv[2] : "stack";
    double ns = !std::strcmp(variant, "gc") ? run<gc::tracked_ptr>(mode) : run<sgcl::tracked_ptr>(mode);
    std::printf("%s mode=%s ns/op=%.2f\n", variant, mode, ns);
}
