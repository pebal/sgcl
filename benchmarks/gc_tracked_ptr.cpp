//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// gc::tracked_ptr against tracked_ptr where both may live, and gc::tracked_ptr alone where
// only it may: tight loops, one thread.
//   ptr <tracked|gc> <stack|heap|deref|cell|make>
//   stack: a local constructed from a source and destroyed   (the constructor: location check, barrier)
//   heap:  holder->next = objs[i]                              (a field store)
//   deref: sum += p->v over an array of pointers               (the load and the sign test)
//   cell:  v[i] = src for a std::vector<gc::tracked_ptr> (gc only)      (the cell path: allocated once, then a store into it)
//   make:  a pointer constructed from a source and destroyed in unmanaged memory (gc only)   (a cell taken and given back)
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

    // One function per mode, out of line: the modes measured together in
    // one function change each other's code (the compiler's inlining of
    // the pointer's constructor depends on the size of the caller; the
    // "stack" loop measured 2.0 or 3.6 ns with the same library, by the
    // branches beside it).
    template<template<class> class Ptr>
    using Objs = sgcl::vector<Ptr<Node<Ptr>>>;

    template<template<class> class Ptr>
    SGCL_NOINLINE void run_stack(Objs<Ptr>& objs) {
        for (long i = 0; i < iters; ++i) {
            Ptr<Node<Ptr>> p = objs[i & (targets - 1)];
            asm volatile("" : : "r"(&p) : "memory");
        }
    }

    template<template<class> class Ptr>
    SGCL_NOINLINE void run_heap(Objs<Ptr>& objs) {
        Ptr<Node<Ptr>> holder = sgcl::make_tracked<Node<Ptr>>();
        for (long i = 0; i < iters; ++i) {
            holder->next = objs[i & (targets - 1)];
        }
    }

    template<template<class> class Ptr>
    SGCL_NOINLINE void run_deref(Objs<Ptr>& objs) {
        long sum = 0;
        for (long i = 0; i < iters; ++i) {
            sum += objs[i & (targets - 1)]->v;
        }
        asm volatile("" : : "r"(sum));
    }

    template<template<class> class Ptr>
    SGCL_NOINLINE void run_cell(Objs<Ptr>& objs) {
        std::vector<gc::tracked_ptr<Node<Ptr>>> v(targets);
        for (long i = 0; i < iters; ++i) {
            v[i & (targets - 1)] = objs[(i + 1) & (targets - 1)];
        }
    }

    template<template<class> class Ptr>
    SGCL_NOINLINE void run_make(Objs<Ptr>& objs) {
        auto storage = std::make_unique<unsigned char[]>(sizeof(gc::tracked_ptr<Node<Ptr>>));   // unmanaged memory, reused
        auto at = reinterpret_cast<gc::tracked_ptr<Node<Ptr>>*>(storage.get());
        for (long i = 0; i < iters; ++i) {
            new (at) gc::tracked_ptr<Node<Ptr>>(objs[i & (targets - 1)]);
            asm volatile("" : : "r"(at) : "memory");
            at->~tracked_ptr();
        }
    }

    template<template<class> class Ptr>
    double run(const char* mode) {
        Objs<Ptr> objs;
        for (int i = 0; i < targets; ++i) {
            objs.push_back(sgcl::make_tracked<Node<Ptr>>());
        }
        auto t0 = bench::Clock::now();
        if (!std::strcmp(mode, "stack")) {
            run_stack<Ptr>(objs);
        } else if (!std::strcmp(mode, "heap")) {
            run_heap<Ptr>(objs);
        } else if (!std::strcmp(mode, "deref")) {
            run_deref<Ptr>(objs);
        } else if (!std::strcmp(mode, "cell")) {
            run_cell<Ptr>(objs);
        } else if (!std::strcmp(mode, "make")) {
            run_make<Ptr>(objs);
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
