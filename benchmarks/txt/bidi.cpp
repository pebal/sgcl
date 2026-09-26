//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of laying out text that runs both ways at once. Almost all of it
// is the paragraph analysis of UAX #9, which every one of these questions
// but the last has to do; the mirroring of rule L4 on top of it is small,
// and a renderer that already holds the levels — bidi_runs and levels()
// both work them out — should not pay for them twice.
//   bidi <levels|runs|order|mirror|mirror-levels|mirrored-of> [mixed|plain|arabic] [lines=1]
//   levels:        the level of every code point
//   runs:          the pieces in the order they are drawn
//   order:         the byte position of every code point, in that order
//   mirror:        rule L4, the paragraph worked out for it
//   mirror-levels: rule L4 alone, the levels handed in
//   mirrored-of:   is_mirrored over every code point, nothing else
// Prints nanoseconds per pass over the whole text.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <string>

namespace {
    using namespace sgcl;

    std::string text_of(const char* kind, size_t lines) {
        // a mixed line with brackets inside the Arabic, a line of Latin
        // with nothing to mirror, and one that is Arabic throughout
        const char* one =
            !std::strcmp(kind, "plain")  ? "Ala ma kota, a kot ma Ale, i nic sie tu nie odbija."
          : !std::strcmp(kind, "arabic") ? "شلوم (العربية) [123] مرحبا"
                                         : "Nazwa (شلوم [123]) OK i jeszcze (troche) tekstu";
        std::string out;
        for (size_t i = 0; i < lines; ++i) {
            out += one;
            out += ' ';
        }
        return out;
    }

    size_t pass(const char* variant, const string& text, const vector<uint8_t>& given) {
        if (!std::strcmp(variant, "levels")) {
            return txt::levels(text).size();
        }
        if (!std::strcmp(variant, "runs")) {
            return txt::bidi_runs(text).count();
        }
        if (!std::strcmp(variant, "order")) {
            return txt::visual_order(text).size();
        }
        if (!std::strcmp(variant, "mirror")) {
            return txt::mirrored(text).size();
        }
        if (!std::strcmp(variant, "mirror-levels")) {
            return txt::mirrored(text, given).size();
        }
        size_t n = 0;
        for (auto c : text.runes()) {
            n += txt::is_mirrored(c) ? 1 : 0;
        }
        return n;
    }

    double run(const char* variant, const char* kind, size_t lines, size_t& answer) {
        auto raw = text_of(kind, lines);
        string text(raw.data(), raw.size());
        auto given = txt::levels(text);
        // one pass before the clock: the first case in a process reads
        // about twice high
        answer = pass(variant, text, given);
        size_t passes = 0;
        auto t0 = bench::Clock::now();
        double spent = 0;
        while (spent < 2.0) {
            answer = pass(variant, text, given);
            ++passes;
            spent = bench::seconds_since(t0);
        }
        return spent / double(passes) * 1e9;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "mirror";
    if (!bench::has_variant(variant, {"levels", "runs", "order", "mirror", "mirror-levels",
                                      "mirrored-of"})) {
        std::fprintf(stderr, "usage: bidi <levels|runs|order|mirror|mirror-levels|mirrored-of>"
                             " [mixed|plain|arabic] [lines]\n");
        return 2;
    }
    const char* kind = argc > 2 ? argv[2] : "mixed";
    if (!bench::has_variant(kind, {"mixed", "plain", "arabic"})) {
        std::fprintf(stderr, "usage: bidi <...> [mixed|plain|arabic] [lines]\n");
        return 2;
    }
    size_t lines = argc > 3 ? (size_t)std::atoi(argv[3]) : 1;
    size_t answer = 0;
    double ns = run(variant, kind, lines, answer);
    std::printf("%s text=%s lines=%zu ns/op=%.1f answer=%zu\n", variant, kind, lines, ns, answer);
    return 0;
}
