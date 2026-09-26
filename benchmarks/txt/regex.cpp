//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of txt::regex against std::regex.
//   regex <sgcl|std> [op=literal] [count]
//
// The text is four thousand bytes of prose built from thirty words, with
// no digit in it, no capital letter, no '@' and no '-'. That matters: a
// pattern that is *not* in the text makes the search walk the whole of
// it, and only then does a number divided by the length mean anything.
// A pattern that is there stops where it is found, and what that measures
// is where in the text it happens to be, which is why there is one op for
// that and it is named `hit`.
//
// The ops both sides can be asked. Each one is a shape a pattern really
// comes in, not a shape chosen to flatter:
//   literal   a word of seven letters that is not there
//   boundary  the same between \b and \b
//   class     [0-9]+, and there are no digits
//   alt       kot|pies|ryba, none of the three there
//   date      (\d{4})-(\d{2})-(\d{2})
//   address   (\w+)@(\w+)\.(com|pl) — the one that begins with \w, which
//             nearly every byte is
//   dotstar   zyz.*kot
//   upper     [QWX]+, a class of three letters that are not there
//   hit       a word that is there, a third of the way in
//   line      the address over one line of about sixty bytes, which is
//             what a log line or a form field is
//   find      the address with its three groups, asked for the match
//   all       every occurrence of [a-z]+, which is every word
//   replace   every one of them wrapped in <>
//   build     compiling the address pattern, which is where a counted
//             repetition is spelled out into instructions
//   blowup    (a+)+b over twenty-eight characters and no b — the whole
//             reason this engine is written the way it is. The standard
//             library does not hang on it: libc++ counts its steps and
//             throws regex_error, and the time printed for the std
//             variant is the time it took to give up.
//
// And the two only this one can be asked:
//   unicode   \p{Script=Greek}+ over Latin text. The standard's classes
//             over a std::string are classes of *bytes*, so the same
//             range written for it is not the same question and a
//             number for it would not be a comparison.
//   split     the text cut on every word. std::regex_token_iterator can
//             be made to do it, but only by building a string for every
//             piece, where this hands back slices of the text; again not
//             the same question.
//
// Every op walks its road a few thousand times before the clock starts,
// the first case in a process reading about twice high, and every op that
// takes a text takes it from a ring of sixty-four different ones rather
// than one repeated. Prints nanoseconds per call.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"
#include "sgcl/txt/regex.h"

#include <random>
#include <regex>
#include <string>
#include <vector>

using namespace sgcl;

namespace {
    const size_t Texts = 64;
    const size_t Bytes = 4000;
    const size_t LineBytes = 60;

    string of(std::string_view s) {
        return string(s.data(), s.size());
    }

    // No digit, no capital, no '@', no '-': what the misses above look
    // for is really not there, so the search walks to the end
    std::vector<std::string> corpus(size_t length, unsigned seed) {
        std::mt19937 rng(seed);
        static const char* words[] = {
            "ala", "ma", "trawa", "a", "dom", "przy", "ale", "zielona", "rosnie", "tam",
            "obok", "domu", "numer", "email", "biuro", "firma", "pl", "com", "org", "sam",
            "dnia", "o", "godzinie", "potem", "juz", "teraz", "gdy", "bo", "lub", "dla",
        };
        std::vector<std::string> out;
        for (size_t k = 0; k < Texts; ++k) {
            std::string s;
            while (s.size() < length) {
                s += words[rng() % std::size(words)];
                s += (rng() % 8) ? " " : "\n";
            }
            out.push_back(s);
        }
        return out;
    }

    struct Shape {
        const char* op;
        const char* ours;
        const char* theirs;    // null where the standard is not asked the same question
    };

    const Shape Shapes[] = {
        {"literal",  "zyzykot",                    "zyzykot"},
        {"boundary", "\\bzyzykot\\b",              "\\bzyzykot\\b"},
        {"class",    "[0-9]+",                     "[0-9]+"},
        {"alt",      "kot|pies|ryba",              "kot|pies|ryba"},
        {"date",     "(\\d{4})-(\\d{2})-(\\d{2})", "(\\d{4})-(\\d{2})-(\\d{2})"},
        {"address",  "(\\w+)@(\\w+)\\.(com|pl)",   "(\\w+)@(\\w+)\\.(com|pl)"},
        {"dotstar",  "zyz.*kot",                   "zyz.*kot"},
        {"upper",    "[QWX]+",                     "[QWX]+"},
        {"hit",      "trawa",                      "trawa"},
        {"line",     "(\\w+)@(\\w+)\\.(com|pl)",   "(\\w+)@(\\w+)\\.(com|pl)"},
        {"find",     "(\\w+)@(\\w+)\\.(com|pl)",   "(\\w+)@(\\w+)\\.(com|pl)"},
        {"all",      "[a-z]+",                     "[a-z]+"},
        {"replace",  "[a-z]+",                     "[a-z]+"},
        {"build",    "(\\d{4})-(\\d{2})-(\\d{2})", "(\\d{4})-(\\d{2})-(\\d{2})"},
        {"blowup",   "(a+)+b",                     "(a+)+b"},
        {"unicode",  "\\p{Script=Greek}+",         nullptr},
        {"split",    "[a-z]+",                     nullptr},
    };

    const Shape* shape_of(const char* op) {
        for (const auto& s : Shapes) {
            if (!std::strcmp(s.op, op)) {
                return &s;
            }
        }
        return nullptr;
    }

    // The count is per variant as well as per op, which the benchmarks
    // beside this one do not need: over four kilobytes this side is a
    // thousand times quicker than the standard's, and one count that
    // suited both would either take a minute a cell or measure nothing.
    // ns/op is still ns/op, so the cells compare.
    long count_for(const char* variant, const char* op, long given) {
        if (given) {
            return given;
        }
        bool ours = !std::strcmp(variant, "sgcl");
        if (!std::strcmp(op, "build")) {
            return 100'000;
        }
        if (!std::strcmp(op, "blowup")) {
            return ours ? 100'000 : 1'000;
        }
        if (!std::strcmp(op, "line")) {
            return ours ? 2'000'000 : 20'000;
        }
        if (!std::strcmp(op, "all") || !std::strcmp(op, "replace") || !std::strcmp(op, "split")
            || !std::strcmp(op, "find")) {
            return ours ? 20'000 : 2'000;
        }
        return ours ? 200'000 : 2'000;
    }

    size_t sink = 0;

    template<class F>
    double timed(long count, F&& f) {
        // a pass before the clock: the first case in a process reads
        // about twice high
        long warm = std::max(1L, std::min(2000L, count / 10));
        for (long i = 0; i < warm; ++i) {
            f(i);
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            f(i);
        }
        double s = bench::seconds_since(t0);
        return s * 1e9 / double(count);
    }

    //----------------------------------------------------------------
    double run_sgcl(const Shape& shape, long count) {
        auto plain = corpus(Bytes, 7);
        auto lines = corpus(LineBytes, 11);
        sgcl::vector<string> texts;
        sgcl::vector<string> shorts;
        for (size_t k = 0; k < Texts; ++k) {
            texts.push_back(of(plain[k]));
            shorts.push_back(of(lines[k]));
        }
        auto ring = [&](long i) -> const string& { return texts[size_t(i) % Texts]; };

        if (!std::strcmp(shape.op, "build")) {
            return timed(count, [&](long) {
                auto re = txt::regex::compile(of(shape.ours));
                sink += re ? re->program_size() : 0;
            });
        }
        if (!std::strcmp(shape.op, "blowup")) {
            auto re = *txt::regex::compile(of(shape.ours));
            string text = of(std::string(28, 'a') + "!");
            return timed(count, [&](long) { sink += re.contains(text) ? 1 : 0; });
        }

        auto made = txt::regex::compile(of(shape.ours));
        auto re = *made;
        if (!std::strcmp(shape.op, "line")) {
            return timed(count, [&](long i) { sink += re.contains(shorts[size_t(i) % Texts]) ? 1 : 0; });
        }
        if (!std::strcmp(shape.op, "find")) {
            return timed(count, [&](long i) { sink += re.find(ring(i)) ? 1 : 0; });
        }
        if (!std::strcmp(shape.op, "all")) {
            return timed(count, [&](long i) { sink += re.count(ring(i)); });
        }
        if (!std::strcmp(shape.op, "replace")) {
            return timed(count, [&](long i) { sink += re.replace(ring(i), of("<$0>")).size(); });
        }
        if (!std::strcmp(shape.op, "split")) {
            return timed(count, [&](long i) { sink += re.split(ring(i)).size(); });
        }
        return timed(count, [&](long i) { sink += re.contains(ring(i)) ? 1 : 0; });
    }

    //----------------------------------------------------------------
    double run_std(const Shape& shape, long count) {
        if (!shape.theirs) {
            return -1;
        }
        auto texts = corpus(Bytes, 7);
        auto lines = corpus(LineBytes, 11);
        auto ring = [&](long i) -> const std::string& { return texts[size_t(i) % Texts]; };

        if (!std::strcmp(shape.op, "build")) {
            return timed(count, [&](long) {
                std::regex re(shape.theirs, std::regex::ECMAScript);
                sink += re.mark_count();
            });
        }

        std::regex re(shape.theirs, std::regex::ECMAScript | std::regex::optimize);
        if (!std::strcmp(shape.op, "blowup")) {
            std::string text(28, 'a');
            text += "!";
            // libc++ counts its steps and throws rather than hanging;
            // the time is what it took to give up, and that is the
            // number worth having
            return timed(count, [&](long) {
                try {
                    sink += std::regex_search(text, re) ? 1 : 0;
                } catch (const std::regex_error&) {
                    ++sink;
                }
            });
        }
        if (!std::strcmp(shape.op, "line")) {
            return timed(count, [&](long i) {
                sink += std::regex_search(lines[size_t(i) % Texts], re) ? 1 : 0;
            });
        }
        if (!std::strcmp(shape.op, "find")) {
            return timed(count, [&](long i) {
                std::smatch m;
                sink += std::regex_search(ring(i), m, re) ? m.size() : 0;
            });
        }
        if (!std::strcmp(shape.op, "all")) {
            return timed(count, [&](long i) {
                const std::string& t = ring(i);
                auto k = std::sregex_iterator(t.begin(), t.end(), re);
                sink += size_t(std::distance(k, std::sregex_iterator()));
            });
        }
        if (!std::strcmp(shape.op, "replace")) {
            return timed(count, [&](long i) { sink += std::regex_replace(ring(i), re, "<$&>").size(); });
        }
        return timed(count, [&](long i) { sink += std::regex_search(ring(i), re) ? 1 : 0; });
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "std"})) {
        std::fprintf(stderr,
                     "usage: regex <sgcl|std> [literal|boundary|class|alt|date|address|dotstar|"
                     "upper|hit|line|find|all|replace|build|blowup|unicode|split] [count]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "literal";
    // An op nobody wrote would otherwise fall through every comparison
    // and print a nought, which in a table reads as a measurement
    const Shape* shape = shape_of(op);
    if (!shape) {
        std::fprintf(stderr, "regex: no op called %s\n", op);
        return 2;
    }
    long count = count_for(variant, op, argc > 3 ? std::atol(argv[3]) : 0);
    double ns = !std::strcmp(variant, "sgcl") ? run_sgcl(*shape, count) : run_std(*shape, count);
    if (ns < 0) {
        // Said rather than left out, so that a matrix has a cell for it
        std::printf("%s op=%s count=%ld ns/op=n/a\n", variant, op, count);
        return 0;
    }
    std::printf("%s op=%s count=%ld ns/op=%.2f\n", variant, op, count, ns);
    return 0;
}
