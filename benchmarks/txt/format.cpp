//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of txt::format against std::format, and of the roads txt::format
// has that the standard's has not.
//   format <sgcl|std> [op=simple] [count]
//
// The ones both can be asked, all of them writing into a buffer the
// caller lends so that nothing is allocated and the writing is what is
// measured (std::format_to_n against txt::format_to):
//   simple    "{} left" with a number
//   padded    "{:>12}"
//   centred   "{:*^40}"
//   mixed     "{:>8.3f} {:#x}"
//   literal   forty-five characters and no field at all
//   whole     "{}" of a whole double, which is the road that does not
//             go through to_chars
//   text      "{}" of a short piece of text
//   five      five fields in one pattern, which is one more than a
//             pattern keeps steps for
//   alloc     "{} left" into a string that is handed back, which is the
//             call a program actually writes
//   runtime   a pattern read where the program runs: txt::runtime here,
//             std::vformat there
//
// And the ones only this one can be asked, C++23 being where the
// standard grew them and this being built as C++20:
//   list      "{}" of a list of five numbers
//   list100   the same of a hundred
//   listwidth "{:>40}" of the list of five — the road that counts the
//             whole before it writes it, a width over a range not being
//             knowable until the elements are behind you
//   elements  "{::>5}", a specification handed to the elements
//   words     "{}" of a list of text, whose elements are written in the
//             debug form and so are escaped and quoted
//   pair      "{}" of a pair
//
// Every op runs over a stream of different values and not one repeated,
// because half of what writing a number costs is the length of it not
// being predictable, and a repeated value hides that. Prints nanoseconds
// per call.
//
// One thing to know before reading the std column. The standard's side
// is format_to_n and not format_to, because that is the same contract
// txt::format_to has — a buffer with a bound, and the whole size
// reported whether or not it fitted — and the unbounded one cannot be
// used safely for what is being compared. It is not free: measured on
// its own, format_to_n of the literal costs 87.8 ns where format_to of
// it costs 48.5, and over a number the two are the same (28.2 against
// 29.5). So the literal row flatters this side by about forty
// nanoseconds and every other row does not.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <format>
#include <random>
#include <string>
#include <vector>

namespace {
    // Enough different values that the branch predictor learns nothing
    // and enough of them to stay in the first level of cache
    const long Values = 1024;

    long count_for(const char* op, long given) {
        if (given) {
            return given;
        }
        if (!std::strcmp(op, "list100")) {
            return 200'000;
        }
        if (!std::strcmp(op, "alloc") || !std::strcmp(op, "list")
            || !std::strcmp(op, "listwidth") || !std::strcmp(op, "elements")
            || !std::strcmp(op, "words")) {
            return 2'000'000;
        }
        return 20'000'000;
    }

    struct Stream {
        std::vector<int> numbers;
        std::vector<double> reals;
        const char* words[4] = {"alpha", "beta", "gamma", "delta"};

        Stream() {
            std::mt19937_64 rng(1);
            for (long i = 0; i < Values; ++i) {
                numbers.push_back(int(rng() % 1000000));
                reals.push_back(double(rng() % 100000) + 0.5);
            }
        }
    };

    // The first case in a process reads about twice high, so the road is
    // walked before it is timed
    template<class F>
    void warm(F&& f) {
        for (long i = 0; i < 100000; ++i) {
            f(i);
        }
    }

    template<class F>
    double timed(long count, F&& f) {
        warm(f);
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            f(i);
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }

    volatile size_t sink = 0;

    double run_sgcl(const char* op, long count) {
        using namespace sgcl;
        Stream in;
        char room[1024];
        auto buffer = slice<char>(room, room + sizeof room);
        auto at = [&](long i) { return size_t(i % Values); };

        if (!std::strcmp(op, "simple")) {
            return timed(count, [&](long i) {
                sink += txt::format_to(buffer, "{} left", in.numbers[at(i)]);
            });
        }
        if (!std::strcmp(op, "padded")) {
            return timed(count, [&](long i) {
                sink += txt::format_to(buffer, "{:>12}", in.numbers[at(i)]);
            });
        }
        if (!std::strcmp(op, "centred")) {
            return timed(count, [&](long i) {
                sink += txt::format_to(buffer, "{:*^40}", in.numbers[at(i)]);
            });
        }
        if (!std::strcmp(op, "mixed")) {
            return timed(count, [&](long i) {
                sink += txt::format_to(buffer, "{:>8.3f} {:#x}",
                                       in.reals[at(i)], in.numbers[at(i)]);
            });
        }
        if (!std::strcmp(op, "literal")) {
            return timed(count, [&](long) {
                sink += txt::format_to(buffer, "a literal of forty-five characters, no field");
            });
        }
        if (!std::strcmp(op, "whole")) {
            return timed(count, [&](long i) {
                sink += txt::format_to(buffer, "{}", double(in.numbers[at(i)]));
            });
        }
        if (!std::strcmp(op, "text")) {
            return timed(count, [&](long i) {
                sink += txt::format_to(buffer, "{}", in.words[i & 3]);
            });
        }
        if (!std::strcmp(op, "five")) {
            return timed(count, [&](long i) {
                size_t k = at(i);
                sink += txt::format_to(buffer, "{} {} {} {} {}", in.numbers[k], in.numbers[k] + 1,
                                       in.numbers[k] + 2, in.numbers[k] + 3, in.numbers[k] + 4);
            });
        }
        if (!std::strcmp(op, "alloc")) {
            return timed(count, [&](long i) {
                sink += txt::format("{} left", in.numbers[at(i)]).size();
            });
        }
        if (!std::strcmp(op, "runtime")) {
            string pattern("{} left");
            auto made = txt::runtime(pattern);
            return timed(count, [&](long i) {
                sink += *txt::format_to(buffer, made, in.numbers[at(i)]);
            });
        }

        vector<int> five;
        for (int k = 0; k < 5; ++k) {
            five.push_back(k * 37 + 1);
        }
        if (!std::strcmp(op, "list") || !std::strcmp(op, "listwidth")
            || !std::strcmp(op, "elements")) {
            bool width = !std::strcmp(op, "listwidth");
            bool each = !std::strcmp(op, "elements");
            return timed(count, [&](long i) {
                five[0] = in.numbers[at(i)];
                if (width) {
                    sink += txt::format_to(buffer, "{:>40}", five);
                } else if (each) {
                    sink += txt::format_to(buffer, "{::>5}", five);
                } else {
                    sink += txt::format_to(buffer, "{}", five);
                }
            });
        }
        if (!std::strcmp(op, "list100")) {
            vector<int> hundred;
            for (int k = 0; k < 100; ++k) {
                hundred.push_back(k * 37 + 1);
            }
            return timed(count, [&](long i) {
                hundred[0] = in.numbers[at(i)];
                sink += txt::format_to(buffer, "{}", hundred);
            });
        }
        if (!std::strcmp(op, "words")) {
            // A vector of ours and not one of the standard's: a string
            // holds a tracked_ptr, which may not live in unmanaged memory
            vector<string> some;
            for (auto w : in.words) {
                some.push_back(string(w));
            }
            return timed(count, [&](long) {
                sink += txt::format_to(buffer, "{}", some);
            });
        }
        if (!std::strcmp(op, "pair")) {
            return timed(count, [&](long i) {
                size_t k = at(i);
                sink += txt::format_to(buffer, "{}", pair<int, int>(in.numbers[k], k));
            });
        }
        return 0;
    }

    double run_std(const char* op, long count) {
        Stream in;
        char room[1024];
        auto at = [&](long i) { return size_t(i % Values); };
        // Each call names its pattern where it stands: a std::format
        // pattern is checked by the compiler and passing it through a
        // helper of one's own is what takes that away
#define PUT(...) sink += size_t(std::format_to_n(room, sizeof room, __VA_ARGS__).size)

        if (!std::strcmp(op, "simple")) {
            return timed(count, [&](long i) { PUT("{} left", in.numbers[at(i)]); });
        }
        if (!std::strcmp(op, "padded")) {
            return timed(count, [&](long i) { PUT("{:>12}", in.numbers[at(i)]); });
        }
        if (!std::strcmp(op, "centred")) {
            return timed(count, [&](long i) { PUT("{:*^40}", in.numbers[at(i)]); });
        }
        if (!std::strcmp(op, "mixed")) {
            return timed(count, [&](long i) {
                PUT("{:>8.3f} {:#x}", in.reals[at(i)], in.numbers[at(i)]);
            });
        }
        if (!std::strcmp(op, "literal")) {
            return timed(count, [&](long) {
                PUT("a literal of forty-five characters, no field");
            });
        }
        if (!std::strcmp(op, "whole")) {
            return timed(count, [&](long i) { PUT("{}", double(in.numbers[at(i)])); });
        }
        if (!std::strcmp(op, "text")) {
            return timed(count, [&](long i) { PUT("{}", in.words[i & 3]); });
        }
        if (!std::strcmp(op, "five")) {
            return timed(count, [&](long i) {
                size_t k = at(i);
                PUT("{} {} {} {} {}", in.numbers[k], in.numbers[k] + 1, in.numbers[k] + 2,
                    in.numbers[k] + 3, in.numbers[k] + 4);
            });
        }
        if (!std::strcmp(op, "alloc")) {
            return timed(count, [&](long i) {
                sink += std::format("{} left", in.numbers[at(i)]).size();
            });
        }
        if (!std::strcmp(op, "runtime")) {
            std::string pattern("{} left");
            return timed(count, [&](long i) {
                int v = in.numbers[at(i)];
                auto r = std::vformat_to(room, pattern, std::make_format_args(v));
                sink += size_t(r - room);
            });
        }
#undef PUT
        // list, list100, listwidth, elements, words, pair: the standard
        // grew these in C++23 and this is built as C++20
        return -1;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "std"})) {
        std::fprintf(stderr,
                     "usage: format <sgcl|std> [simple|padded|centred|mixed|literal|whole|text|"
                     "five|alloc|runtime|list|list100|listwidth|elements|words|pair] [count]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "simple";
    // An op nobody wrote would otherwise fall through every comparison
    // and print a nought, which in a table reads as a measurement
    if (!bench::has_variant(op, {"simple", "padded", "centred", "mixed", "literal", "whole",
                                 "text", "five", "alloc", "runtime", "list", "list100",
                                 "listwidth", "elements", "words", "pair"})) {
        std::fprintf(stderr, "format: no op called %s\n", op);
        return 2;
    }
    long count = count_for(op, argc > 3 ? std::atol(argv[3]) : 0);
    double ns = !std::strcmp(variant, "sgcl") ? run_sgcl(op, count) : run_std(op, count);
    if (ns < 0) {
        // Said rather than left out, so that a matrix has a cell for it
        std::printf("%s op=%s count=%ld ns/op=n/a\n", variant, op, count);
        return 0;
    }
    std::printf("%s op=%s count=%ld ns/op=%.2f\n", variant, op, count, ns);
    return 0;
}
