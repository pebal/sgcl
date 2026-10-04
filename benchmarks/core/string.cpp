//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of a string kept in managed objects: sgcl::string (immutable, one
// word to an object of exactly its size, no destructor) against
// std::string (a small buffer inside, a heap buffer past it).
//   string <sgcl|std> [op=make|copy|hash1|hashn|sweep] [len=10]
//   make: 2 M strings made from tokens of a text buffer, each stored in a node
//   copy: 2 M strings copied from one node to another, in order
// make and copy store into 2 M old nodes right after a full collection, and
// the first store into an old object after a collection takes the barrier's
// slow path (its state and its card): the loop runs four passes, the nodes
// emptied between them outside the timing, and ns/op is the mean of the
// last three, the steady state; first_pass is the first, for analysis only.
//   hash1: 2 M strings each hashed once, as a map key would be
//   hashn: 2 M strings each hashed eight times in a row (a string used as a key again and again)
//   sweep: 2 M nodes holding a string dropped: the full collection that frees them
// Prints nanoseconds per operation (sweep: milliseconds for the cycle).
// The same shape in benchmarks/go/string and benchmarks/java/Strings.java.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"
#include <random>
#include <string>
namespace {
    const long count = 2'000'000;
    const int passes = 4;
    double first_pass = -1;   // make and copy: the first pass, printed apart

    // passes of `body` over the nodes, `empty` run between them untimed;
    // returns the mean of every pass after the first (ns per element)
    template<class Empty, class Body>
    double steady(Empty empty, Body body) {
        double sum = 0;
        for (int p = 0; p < passes; ++p) {
            if (p > 0) {
                empty();
            }
            auto t0 = bench::Clock::now();
            body();
            double ns = bench::seconds_since(t0) / count * 1e9;
            if (p == 0) {
                first_pass = ns;
            } else {
                sum += ns;
            }
        }
        return sum / (passes - 1);
    }

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
            return S(std::string_view(tok, len));
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
            return steady([&] {
                for (long i = 0; i < count; ++i) {
                    nodes[i]->s = S();
                }
            }, [&] {
                for (long i = 0; i < count; ++i) {
                    nodes[i]->s = make<S>(&buffer[i * len], len);
                }
            });
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
            return steady([&] {
                for (long i = 0; i < count; ++i) {
                    targets[i]->s = S();
                }
            }, [&] {
                for (long i = 0; i < count; ++i) {
                    targets[i]->s = nodes[i]->s;
                }
            });
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
    if (!bench::has_variant(variant, {"sgcl", "std"})) {
        std::fprintf(stderr, "usage: string <sgcl|std> [make|copy|hash1|hashn|sweep] [len]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "make";
    size_t len = argc > 3 ? (size_t)std::atoi(argv[3]) : 10;
    double v = !std::strcmp(variant, "sgcl") ? run<sgcl::string>(op, len) : run<std::string>(op, len);
    if (!std::strcmp(op, "sweep")) {
        std::printf("%s op=%s len=%zu ms=%.1f\n", variant, op, len, v);
    } else if (first_pass >= 0) {
        std::printf("%s op=%s len=%zu ns/op=%.2f first_pass=%.2f\n", variant, op, len, v, first_pass);
    } else {
        std::printf("%s op=%s len=%zu ns/op=%.2f\n", variant, op, len, v);
    }
    return 0;
}
