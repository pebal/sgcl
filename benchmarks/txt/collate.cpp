//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of putting text in order, of the settings a collator can be asked
// for, and of the shape a search by collation is asked in.
//   collate <compare|key|search> [op] [count|kb]
//
// compare and key take the same six ops and the same two short words,
// which differ in their fourth letter, and print nanoseconds per call:
//   root      the root order, nothing asked for — the path every other
//             op is measured against, since a setting that was not asked
//             for is meant to cost nothing
//   polish    the same in Polish, where a tailoring is searched
//   numeric   the numeric order over two words that end in a number
//   shifted   the punctuation shifted aside, over a word with a hyphen
//   case      the case on a level of its own and the capitals first
//   backwards the accents read from the end of the word
//
// search takes the three shapes of the same question over a text of `kb`
// kilobytes, and prints microseconds for one pass over the whole text
// and how many occurrences the pass found:
//   oneshot   a loop over collator::find, the text weighed afresh on
//             every call — the shape to measure against
//   prepared  collated_matches, the text weighed once
//   build     the weighing alone, to say how much of `prepared` it is
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <cstdio>
#include <cstring>
#include <random>
#include <string>

namespace {
    using namespace sgcl;

    volatile int sink = 0;

    struct words {
        const char* a;
        const char* b;
    };

    words words_of(const char* op) {
        if (!std::strcmp(op, "numeric")) {
            return {"plik9", "plik10"};
        }
        if (!std::strcmp(op, "shifted")) {
            return {"re-sume", "resume"};
        }
        if (!std::strcmp(op, "case")) {
            return {"Zamek", "zamek"};
        }
        if (!std::strcmp(op, "backwards")) {
            return {"coté", "côte"};
        }
        return {"zamek", "zamku"};
    }

    txt::collator collator_of(const char* op) {
        if (!std::strcmp(op, "polish")) {
            return txt::collator(txt::locale("pl"));
        }
        if (!std::strcmp(op, "numeric")) {
            return txt::collator(txt::options{.numeric = true});
        }
        if (!std::strcmp(op, "shifted")) {
            return txt::collator(txt::options{.punctuation = txt::punctuation::shifted});
        }
        if (!std::strcmp(op, "case")) {
            return txt::collator(txt::options{.case_order = txt::case_order::upper_first,
                                              .case_level = true});
        }
        if (!std::strcmp(op, "backwards")) {
            return txt::collator(txt::options{.backwards = true});
        }
        return txt::collator();
    }

    // The first case in a process reads about twice high, so every one
    // runs a tenth of its iterations before the clock starts
    template<class F>
    double timed(long n, F&& body) {
        for (long i = 0; i < n / 10 + 1; ++i) {
            body();
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            body();
        }
        return bench::seconds_since(t0) * 1e9 / double(n);
    }

    double run_compare(const char* op, long count) {
        auto w = words_of(op);
        auto c = collator_of(op);
        string a(w.a), b(w.b);
        return timed(count, [&] { sink += c.compare(a, b); });
    }

    double run_key(const char* op, long count) {
        auto w = words_of(op);
        auto c = collator_of(op);
        string a(w.a);
        byte room[512];
        return timed(count, [&] { sink += int(c.key_to(room, a)); });
    }

    // Polish and French among the Latin, so that the weighing has marks
    // and a tailoring to deal with and the ASCII path is not the whole
    // story, with the word looked for scattered through it
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

    size_t pass(const char* op, const txt::collator& c, const string& text, const string& pattern) {
        if (!std::strcmp(op, "build")) {
            return txt::collated_text(c, text).size();
        }
        if (!std::strcmp(op, "prepared")) {
            return txt::collated_matches(c, text, pattern).count();
        }
        size_t n = 0;
        for (auto hit = c.find(text, pattern); hit; hit = c.find(text, pattern, hit->at + hit->size)) {
            ++n;
        }
        return n;
    }

    double run_search(const char* op, size_t kb, size_t& found) {
        auto raw = text_of(kb * 1024);
        string text(raw.data(), raw.size());
        string pattern("KOTA");
        // at primary strength, which is what a search box asks for: the
        // case and the accents are not differences
        txt::collator c{txt::options{.strength = txt::strength::primary}};
        found = pass(op, c, text, pattern);
        size_t passes = 0;
        auto t0 = bench::Clock::now();
        double spent = 0;
        while (spent < 2.0) {
            found = pass(op, c, text, pattern);
            ++passes;
            spent = bench::seconds_since(t0);
        }
        return spent / double(passes) * 1e6;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "compare";
    if (!bench::has_variant(variant, {"compare", "key", "search"})) {
        std::fprintf(stderr, "usage: collate <compare|key|search> [op] [count|kb]\n");
        return 2;
    }
    if (!std::strcmp(variant, "search")) {
        const char* op = argc > 2 ? argv[2] : "prepared";
        if (!bench::has_variant(op, {"oneshot", "prepared", "build"})) {
            std::fprintf(stderr, "collate search: no op called %s\n", op);
            return 2;
        }
        size_t kb = argc > 3 ? (size_t)std::atoi(argv[3]) : 64;
        size_t found = 0;
        double us = run_search(op, kb, found);
        std::printf("%s op=%s kb=%zu us/op=%.2f found=%zu\n", variant, op, kb, us, found);
        return 0;
    }
    const char* op = argc > 2 ? argv[2] : "root";
    if (!bench::has_variant(op, {"root", "polish", "numeric", "shifted", "case", "backwards"})) {
        std::fprintf(stderr, "collate %s: no op called %s\n", variant, op);
        return 2;
    }
    long count = argc > 3 ? std::atol(argv[3]) : 3000000;
    double ns = !std::strcmp(variant, "compare") ? run_compare(op, count) : run_key(op, count);
    std::printf("%s op=%s count=%ld ns/op=%.2f\n", variant, op, count, ns);
    return 0;
}
