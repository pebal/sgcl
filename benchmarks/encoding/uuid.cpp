//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::uuid against the work Go's github.com/google/uuid does, written
// with Go's standard library (benchmarks/go/uuid: Go itself has no UUID).
//   uuid sgcl [op=v4] [count]
//
//   v4         uuid::v4() (Go: crypto/rand's 16 bytes, NewRandom)
//   v7         uuid::v7() (Go: the clock, a counter under a lock, crypto/rand)
//   v4_string  uuid::v4().to_string() (Go: NewRandom().String())
//   parse      uuid::parse of the canonical text (Go: Parse)
//
// Prints nanoseconds per operation.
#include "benchmarks/common.h"
#include "sgcl/encoding/uuid.h"

#include <cstdlib>
#include <cstring>

using namespace sgcl;
using encoding::uuid;

namespace {
    volatile size_t sink = 0;

    template<class F>
    double timed(long count, F&& f) {
        for (long i = 0; i < std::min(count, 1000L); ++i) {
            f();
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            f();
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    const char* op = argc > 2 ? argv[2] : "v4";
    if (std::strcmp(variant, "sgcl") != 0) {
        std::fprintf(stderr, "usage: uuid sgcl [v4|v7|v4_string|parse] [count]\n");
        return 2;
    }
    long count = argc > 3 ? std::atol(argv[3]) : 10000000L;
    string text = "f81d4fae-7dec-11d0-a765-00a0c91e6bf6";
    double ns;
    if (!std::strcmp(op, "v4")) {
        ns = timed(count, [&] { sink += uuid::v4().bytes()[0]; });
    } else if (!std::strcmp(op, "v7")) {
        ns = timed(count, [&] { sink += uuid::v7().bytes()[15]; });
    } else if (!std::strcmp(op, "v4_string")) {
        ns = timed(count, [&] { sink += uuid::v4().to_string().size(); });
    } else if (!std::strcmp(op, "parse")) {
        ns = timed(count, [&] { sink += uuid::parse(text)->bytes()[3]; });
    } else {
        std::fprintf(stderr, "uuid: no op called %s\n", op);
        return 2;
    }
    std::printf("%s op=%s count=%ld ns/op=%.1f\n", variant, op, count, ns);
}
