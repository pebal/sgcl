//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of finding a text inside a text, and of the shape the question is
// asked in. Neither the search blind to case nor the one blind to the way
// a text was written can work on the bytes, so both map the text to code
// points first — and that mapping, not the searching, is where the time
// goes. Asked one call at a time it is paid again for every occurrence,
// which makes a loop over them quadratic; asked as a range it is paid
// once.
//   search <oneshot|prepared|build> [op=fold|normalized|bytes|marks] [kb=64]
//   oneshot:  a loop over find_fold / find_normalized, the text mapped
//             afresh on every call — the shape to measure against
//   prepared: fold_matches / normalized_matches, the text mapped once
//   build:    the mapping alone, to say how much of `prepared` it is
//   op=bytes: the searcher over the bytes, for scale; `build` there is
//             the skip table
//   op=marks: normalized, over the same words with a letter among them
//             whose two marks are written out of canonical order
// Prints microseconds per pass over the whole text, and how many
// occurrences the pass found.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <random>
#include <string>

namespace {
    using namespace sgcl;

    // Polish, German and Greek among the Latin, so that folding and
    // decomposing have work to do and the ASCII paths are not the whole
    // story, with the words looked for scattered through it. With
    // `marks`, an x with a dot above written before a dot below goes
    // among them, which is what the canonical ordering has to move.
    std::string text_of(size_t bytes, bool marks = false) {
        static const char* words[] = {
            "Ala", "ma", "kota", "zolw", "żółw", "straße", "café",
            "café", "ΣΟΦΟΣ", "Kot", "wiadro", "ŁÓDŹ",
        };
        std::mt19937 rng(1);
        std::string out;
        while (out.size() < bytes) {
            unsigned pick = rng() % (marks ? 13 : 12);
            out += pick < 12 ? words[pick] : "ẋ̣";
            out += ' ';
        }
        return out;
    }

    // Both ops that go through the normalized search
    bool normalizes(const char* op) {
        return !std::strcmp(op, "normalized") || !std::strcmp(op, "marks");
    }

    const char* pattern_of(const char* op) {
        if (!std::strcmp(op, "fold")) {
            return "KOTA";
        }
        if (normalizes(op)) {
            return "café";
        }
        return "kota";
    }

    // One pass: how many occurrences it found, so that nothing is
    // optimized away and the count can be printed
    size_t pass(const char* variant, const char* op, const string& text, const string& pattern) {
        if (!std::strcmp(variant, "build")) {
            if (!std::strcmp(op, "fold")) {
                return txt::folded_text(text).size();
            }
            if (normalizes(op)) {
                return txt::normalized_text(text).size();
            }
            return txt::searcher(pattern).pattern().size();
        }
        if (!std::strcmp(variant, "prepared")) {
            if (!std::strcmp(op, "fold")) {
                return txt::fold_matches(text, pattern).count();
            }
            if (normalizes(op)) {
                return txt::normalized_matches(text, pattern).count();
            }
            return txt::searcher(pattern).count(text);
        }
        size_t n = 0;
        if (!std::strcmp(op, "fold")) {
            for (auto at = txt::find_fold(text, pattern); at;
                 at = txt::find_fold(text, pattern, at->pos + 1)) {
                ++n;
            }
        } else if (normalizes(op)) {
            for (auto at = txt::find_normalized(text, pattern); at;
                 at = txt::find_normalized(text, pattern, at->pos + 1)) {
                ++n;
            }
        } else {
            txt::searcher s(pattern);
            for (size_t at = s.find(text); at != npos; at = s.find(text, at + 1)) {
                ++n;
            }
        }
        return n;
    }

    double run(const char* variant, const char* op, size_t kb, size_t& found) {
        auto raw = text_of(kb * 1024, !std::strcmp(op, "marks"));
        string text(raw.data(), raw.size());
        const char* p = pattern_of(op);
        string pattern(p, std::strlen(p));
        // one pass before the clock: the first case in a process reads
        // about twice high
        found = pass(variant, op, text, pattern);
        size_t passes = 0;
        auto t0 = bench::Clock::now();
        double spent = 0;
        while (spent < 2.0) {
            found = pass(variant, op, text, pattern);
            ++passes;
            spent = bench::seconds_since(t0);
        }
        return spent / double(passes) * 1e6;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "prepared";
    if (!bench::has_variant(variant, {"oneshot", "prepared", "build"})) {
        std::fprintf(stderr, "usage: search <oneshot|prepared|build> [fold|normalized|bytes|marks] [kb]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "fold";
    if (!bench::has_variant(op, {"fold", "normalized", "bytes", "marks"})) {
        std::fprintf(stderr, "usage: search <oneshot|prepared|build> [fold|normalized|bytes|marks] [kb]\n");
        return 2;
    }
    size_t kb = argc > 3 ? (size_t)std::atoi(argv[3]) : 64;
    size_t found = 0;
    double us = run(variant, op, kb, found);
    std::printf("%s op=%s kb=%zu us/op=%.2f found=%zu\n", variant, op, kb, us, found);
    return 0;
}
