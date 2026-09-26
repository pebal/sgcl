//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of the questions asked about a name: whether it may be one
// (UAX #31), the form two of them are compared in (NFKC_Casefold), and
// what UTS #39 has to say about it before it is handed out.
//   identifier <op> [text=ascii] [count]
//
// The ops:
//   identifier  is_identifier — rule R1 with R1a
//   profile     the same with program_syntax, the underscore and the dollar
//   casefold    nfkc_casefold
//   casefolded  is_nfkc_casefolded — the two properties, no folding
//   fold        txt::fold_case, for scale: what the module already had,
//               and what nfkc_casefold costs over and above it
//   skeleton    the confusable prototypes of UTS #39 §4
//   confusable  two skeletons and a comparison
//   allowed     is_allowed_identifier — Identifier_Status over the text
//   level       restriction_level_of — the ladder of §5.2
//   script      is_single_script — §5.1 alone
//   marks       without_marks, which lives in normalize.h and is here
//               because it is the same walk with one filter
//
// The texts, each a stream of 1024 different names and never the same
// one twice in a row, because a repeated value hides the searching:
//   ascii   plain ASCII names, the road that answers without a table
//   latin   Polish, French, German, Czech — two of the six have a
//           capital in them, so the fold has work to do on a third of
//           the stream and the quick check settles the rest
//   upper   the same letters all in capitals: every name changes, which
//           is the fold road with no quick check to save it
//   compat  fullwidth letters, a ligature, circled digits, Greek,
//           Cyrillic, Japanese — the compatibility decompositions
//   folded  the latin stream already put through nfkc_casefold, to say
//           what a name that arrives in the form costs
//   mixed   half Latin and half Cyrillic, which is what the restriction
//           level exists to notice
// Prints nanoseconds per call.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <random>

namespace {
    using namespace sgcl;

    // Enough different names that the branch predictor learns nothing
    // and few enough to stay in the first level of cache
    const long Names = 1024;

    long count_for(const char* op) {
        if (!std::strcmp(op, "skeleton") || !std::strcmp(op, "confusable")
            || !std::strcmp(op, "casefold") || !std::strcmp(op, "fold")
            || !std::strcmp(op, "marks")) {
            return 2'000'000;
        }
        return 10'000'000;
    }

    const char* const* words_of(const char* text, size_t& n) {
        static const char* ascii[] = {"paypal", "user_name", "someVariable", "acct", "login", "handle"};
        static const char* latin[] = {"wartość", "café", "Grüße", "naïve", "Łódź", "šťastný"};
        static const char* upper[] = {"WARTOŚĆ", "CAFÉ", "GRÜSSE", "NAÏVE", "ŁÓDŹ", "ŠŤASTNÝ"};
        static const char* compat[] = {"ＦＵＬＬwidth", "ﬁle①②", "Straße", "ΑΘΗΝΑ", "переменная", "変数のカタカナ"};
        static const char* mixed[] = {"раypal", "gоogle", "аpple", "miсrosoft", "amazоn", "netflіx"};
        n = 6;
        if (!std::strcmp(text, "latin") || !std::strcmp(text, "folded")) {
            return latin;
        }
        if (!std::strcmp(text, "upper")) {
            return upper;
        }
        if (!std::strcmp(text, "compat")) {
            return compat;
        }
        if (!std::strcmp(text, "mixed")) {
            return mixed;
        }
        return ascii;
    }

    // A name is a word of the set with a number after it, so that no two
    // in the stream are the same object and none is answered from a
    // cache of the string the call before it was handed
    vector<string> stream_of(const char* text) {
        size_t n = 0;
        auto words = words_of(text, n);
        vector<string> out;
        char buf[64];
        for (long i = 0; i < Names; ++i) {
            int written = std::snprintf(buf, sizeof buf, "%s%ld", words[size_t(i) % n], i);
            string name(buf, size_t(written));
            out.push_back(!std::strcmp(text, "folded") ? txt::nfkc_casefold(name) : name);
        }
        return out;
    }

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

    double run(const char* op, const char* text, long count) {
        auto names = stream_of(text);
        string against = names[0];
        auto at = [&](long i) -> const string& { return names[size_t(i) % Names]; };

#define PUT(name, expr) \
        if (!std::strcmp(op, name)) { \
            return timed(count, [&](long i) { sink += size_t(expr); }); \
        }
        PUT("identifier", txt::is_identifier(at(i)))
        PUT("profile", txt::is_identifier(at(i), txt::program_syntax))
        PUT("casefold", txt::nfkc_casefold(at(i)).size())
        PUT("casefolded", txt::is_nfkc_casefolded(at(i)))
        PUT("fold", txt::fold_case(at(i)).size())
        PUT("skeleton", txt::skeleton(at(i)).size())
        PUT("confusable", txt::is_confusable(at(i), against))
        PUT("allowed", txt::is_allowed_identifier(at(i)))
        PUT("level", size_t(txt::restriction_level_of(at(i))))
        PUT("script", txt::is_single_script(at(i)))
        PUT("marks", txt::without_marks(at(i)).size())
#undef PUT
        return -1;
    }
}

int main(int argc, char** argv) {
    const char* ops = "identifier|profile|casefold|casefolded|fold|skeleton|confusable|"
                      "allowed|level|script|marks";
    const char* op = argc > 1 ? argv[1] : "identifier";
    if (!bench::has_variant(op, {"identifier", "profile", "casefold", "casefolded", "fold",
                                 "skeleton", "confusable", "allowed", "level", "script", "marks"})) {
        std::fprintf(stderr, "usage: identifier <%s> [ascii|latin|upper|compat|folded|mixed] [count]\n", ops);
        return 2;
    }
    const char* text = argc > 2 ? argv[2] : "ascii";
    if (!bench::has_variant(text, {"ascii", "latin", "upper", "compat", "folded", "mixed"})) {
        std::fprintf(stderr, "identifier: no text called %s\n", text);
        return 2;
    }
    long count = argc > 3 ? std::atol(argv[3]) : count_for(op);
    double ns = run(op, text, count);
    std::printf("%s text=%s count=%ld ns/op=%.2f\n", op, text, count, ns);
    return 0;
}
