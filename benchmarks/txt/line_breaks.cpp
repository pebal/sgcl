//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of the line breaks of a text (UAX #14), per byte: txt::line_breaks
// walked to its end over a text of `kb` kilobytes of Latin words, Polish
// and French among them (the words of bench_collate's search), so that the
// ASCII path is not the whole story.
//   line_breaks [kb]
// prints nanoseconds per byte and the number of breaks. (The numbers of
// 2026-09-23 in DESIGN 149 and 150 had no program in the repository; this
// one came later and is not their measure.)
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

namespace {
    using namespace sgcl;

    std::string text_of(size_t bytes) {
        static const char* words[] = {
            "Ala", "ma", "kota", "zolw", "żółw", "résumé", "café",
            "kota", "Kot", "wiadro", "ŁÓDŹ", "resume",
        };
        std::mt19937 rng(1);
        std::string out;
        while (out.size() < bytes) {
            out += words[rng() % 12];
            out += ' ';
        }
        return out;
    }

    volatile size_t sink = 0;
}

int main(int argc, char** argv) {
    size_t kb = argc > 1 ? (size_t)std::atoi(argv[1]) : 64;
    auto raw = text_of(kb * 1024);
    string text(raw.data(), raw.size());
    // the first case in a process reads high: a warm-up before the clock
    for (int i = 0; i < 20; ++i) {
        sink += txt::line_breaks(text).count();
    }
    size_t breaks = txt::line_breaks(text).count();
    size_t passes = 0;
    auto t0 = bench::Clock::now();
    double spent = 0;
    while (spent < 2.0) {
        sink += txt::line_breaks(text).count();
        ++passes;
        spent = bench::seconds_since(t0);
    }
    std::printf("line_breaks kb=%zu ns/byte=%.2f breaks=%zu\n", kb, spent / double(passes) / double(raw.size()) * 1e9, breaks);
    return 0;
}
