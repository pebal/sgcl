//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of a string kept in managed objects: sgcl::string (immutable, one
// word to an object of exactly its size, no destructor) against
// std::string (a small buffer inside, a heap buffer past it).
//   string <sgcl|gc|std> [op=make|copy|hash1|hashn|sweep] [len=10]   (gc: gc::string in the same nodes)
//   make: 2 M strings made from tokens of a text buffer, each stored in a node
//   copy: 2 M strings copied from one node to another, in order
//   hash1: 2 M strings each hashed once, as a map key would be
//   hashn: 2 M strings each hashed eight times in a row (a string used as a key again and again)
//   sweep: 2 M nodes holding a string dropped: the full collection that frees them
// Prints nanoseconds per operation (sweep: milliseconds for the cycle).
// The same shape in benchmarks/go/string and benchmarks/java/Strings.java.
#include "common.h"
#include "sgcl/sgcl.h"
#include <random>
#include <string>
namespace {
    const long count = 2'000'000;

    template<class S>
    struct Node {
        S s;
    };

    std::vector<char> text(size_t len) {
        std::vector<char> buffer(count * len);
        std::mt19937_64 rng(1);
        for (auto& c : buffer) {
            c = (char)('a' + rng() % 26);
        }
        return buffer;
    }

    template<class S>
    S make(const char* tok, size_t len) {
        if constexpr(std::is_same_v<S, std::string>) {
            return std::string(tok, len);
        } else {
            return S(std::string_view(tok, len));   // sgcl::string and gc::string alike
        }
    }

    template<class S>
    double run(const char* op, size_t len) {
        auto buffer = text(len);
        sgcl::vector<sgcl::tracked_ptr<Node<S>>> nodes;
        for (long i = 0; i < count; ++i) {
            nodes.push_back(sgcl::make_tracked<Node<S>>());
        }
        if (!std::strcmp(op, "make")) {
            sgcl::collector::force_collect(true);
            auto t0 = bench::Clock::now();
            for (long i = 0; i < count; ++i) {
                nodes[i]->s = make<S>(&buffer[i * len], len);
            }
            return bench::seconds_since(t0) / count * 1e9;
        }
        for (long i = 0; i < count; ++i) {
            nodes[i]->s = make<S>(&buffer[i * len], len);
        }
        if (!std::strcmp(op, "copy")) {
            sgcl::vector<sgcl::tracked_ptr<Node<S>>> targets;
            for (long i = 0; i < count; ++i) {
                targets.push_back(sgcl::make_tracked<Node<S>>());
            }
            sgcl::collector::force_collect(true);
            auto t0 = bench::Clock::now();
            for (long i = 0; i < count; ++i) {
                targets[i]->s = nodes[i]->s;
            }
            return bench::seconds_since(t0) / count * 1e9;
        }
        if (!std::strcmp(op, "hash1") || !std::strcmp(op, "hashn")) {
            std::hash<S> h;
            size_t sum = 0;
            const int times = op[4] == 'n' ? 8 : 1;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < count; ++i) {
                for (int k = 0; k < times; ++k) {
                    sum += h(nodes[i]->s);
                }
            }
            auto ns = bench::seconds_since(t0) / count / times * 1e9;
            return sum == 1 ? 0 : ns;
        }
        if (!std::strcmp(op, "sweep")) {
            sgcl::collector::force_collect(true);
            nodes.clear();
            auto t0 = bench::Clock::now();
            sgcl::collector::force_collect(true);
            return bench::seconds_since(t0) * 1e3;
        }
        return 0;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "gc", "std"})) {
        std::fprintf(stderr, "usage: string <sgcl|gc|std> [make|copy|hash1|hashn|sweep] [len]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "make";
    size_t len = argc > 3 ? (size_t)std::atoi(argv[3]) : 10;
    double v = !std::strcmp(variant, "sgcl") ? run<sgcl::string>(op, len) : !std::strcmp(variant, "gc") ? run<gc::string>(op, len) : run<std::string>(op, len);
    if (!std::strcmp(op, "sweep")) {
        std::printf("%s op=%s len=%zu ms=%.1f\n", variant, op, len, v);
    } else {
        std::printf("%s op=%s len=%zu ns/op=%.2f\n", variant, op, len, v);
    }
    return 0;
}
