//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::regex over a pattern and a text from outside. The first byte picks
// where a search starts; the pattern and the text are split at a NUL.
// std::regex is no oracle — it backtracks, takes what this refuses and
// parts from it on the empty loop by design — so what is held is mostly
// what the engine says about itself, which must agree whichever road asks:
//   - compile answers a regex or an error with a message and a place in
//     the pattern, and never anything else; the regex's pattern is the
//     source and its group count is every match's;
//   - contains is find has a value is count above nought is all() not
//     empty, and full_match is \A(?:pattern)\z found;
//   - all() is find() asked again from where the last match left it (a
//     match of no width a code point on), the matches in order and never
//     overlapping, the first of them find's, the groups inside the match,
//     and the positions, over UTF-8, on the starts of code points;
//   - find from a place finds nothing before it;
//   - replace with "$0" gives the text back, with "" what the pieces of
//     split are put together, and split gives count + 1 pieces, a limit
//     no more than it says;
//   - the time is the engine's promise, linear in the text times the
//     program: libFuzzer's -timeout stops an input that is not, and the
//     text is held to a budget against the program's size so that a
//     linear one never comes near it.
// With SGCL_FUZZ_PCRE2 defined (and PCRE2 given to the compiler), PCRE2 is
// the oracle on the first match and its groups, over the patterns both
// engines read alike: ASCII letters and digits, any other code point, '.',
// classes without escapes, groups that capture and (?:), alternation,
// anchors, and the quantifiers on a single atom only — a quantified group
// is where the two part by design (the empty turn of a loop: regex.md).
// Built with libFuzzer:
//   tests/fuzz/run.sh tests/txt/fuzz/regex_fuzz.cpp 300
// and with the oracle:
//   SGCL_FUZZ_LIBS="-DSGCL_FUZZ_PCRE2 -I/opt/homebrew/opt/pcre2/include -L/opt/homebrew/opt/pcre2/lib -lpcre2-8" \
//       tests/fuzz/run.sh tests/txt/fuzz/regex_fuzz.cpp 300
#include "sgcl/txt/txt.h"

#ifdef SGCL_FUZZ_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Found {
        size_t begin;
        size_t end;
        std::vector<std::pair<size_t, size_t>> groups;   // npos for a group that took no part
    };

    constexpr size_t None = size_t(-1);

    Found found_of(const txt::match& m, const string& text) {
        Found f{m.begin_at(), m.end_at(), {}};
        for (size_t g = 1; g <= m.group_count(); ++g) {
            auto piece = m.group(g);
            if (!piece) {
                f.groups.push_back({None, None});
            } else {
                size_t at = size_t(piece->data() - text.data());
                f.groups.push_back({at, at + piece->size()});
            }
        }
        return f;
    }

    bool same(const Found& a, const Found& b) {
        return a.begin == b.begin && a.end == b.end && a.groups == b.groups;
    }

    bool starts_code_point(std::string_view text, size_t at) {
        return at >= text.size() || (uint8_t(text[at]) & 0xC0) != 0x80;
    }

    // What one match must be, wherever it came from
    void well_formed(const Found& f, const txt::regex& re, std::string_view text, bool utf8) {
        check(f.begin <= f.end && f.end <= text.size());
        check(f.groups.size() == re.group_count());
        for (auto [from, to] : f.groups) {
            if (from == None) {
                check(to == None);
                continue;
            }
            check(f.begin <= from && from <= to && to <= f.end);
            if (utf8) {
                check(starts_code_point(text, from) && starts_code_point(text, to));
            }
        }
        if (utf8) {
            check(starts_code_point(text, f.begin) && starts_code_point(text, f.end));
        }
    }

    size_t one_point(std::string_view text, size_t at) {
        return at < text.size() ? utf8::decode(text, at).second : 1;
    }

#ifdef SGCL_FUZZ_PCRE2
    // Whether PCRE2 reads the pattern as this engine does (above)
    bool both_read_alike(std::string_view p) {
        if (!utf8::valid(p)) {
            return false;
        }
        bool atom = false;         // whether a quantifier may follow
        bool quantified = false;   // whether one just did, which only '?' (lazy) may follow
        bool lazy = false;
        for (size_t i = 0; i < p.size(); ++i) {
            char c = p[i];
            if (c == '*' || c == '+' || c == '?' || c == '{') {
                if (c == '?' && quantified && !lazy) {
                    lazy = true;
                    continue;
                }
                if (!atom || quantified) {
                    return false;
                }
                if (c == '{') {
                    size_t k = i + 1;
                    size_t digits = 0;
                    while (k < p.size() && p[k] >= '0' && p[k] <= '9') {
                        ++k, ++digits;
                    }
                    if (!digits || k >= p.size()) {
                        return false;
                    }
                    if (p[k] == ',') {
                        ++k;
                        while (k < p.size() && p[k] >= '0' && p[k] <= '9') {
                            ++k;
                        }
                    }
                    if (k >= p.size() || p[k] != '}') {
                        return false;
                    }
                    i = k;
                }
                quantified = true;
                lazy = false;
                continue;
            }
            quantified = false;
            lazy = false;
            if (c == '\\' || c == '}' || c == ']') {
                return false;
            }
            if (c == '(') {
                if (i + 1 < p.size() && p[i + 1] == '?') {
                    if (i + 2 >= p.size() || p[i + 2] != ':') {
                        return false;
                    }
                    i += 2;
                }
                atom = false;
            } else if (c == ')') {
                atom = false;   // a quantified group: where the two part by design
            } else if (c == '|' || c == '^' || c == '$') {
                atom = false;
            } else if (c == '[') {
                size_t k = i + 1;
                if (k < p.size() && p[k] == '^') {
                    ++k;
                }
                if (k >= p.size() || p[k] == ']') {
                    return false;
                }
                while (k < p.size() && p[k] != ']') {
                    if (p[k] == '[' || p[k] == '\\') {
                        return false;
                    }
                    ++k;
                }
                if (k >= p.size()) {
                    return false;
                }
                i = k;
                atom = true;
            } else {
                atom = true;   // a letter, a digit, '.', any other code point
            }
        }
        return true;
    }

    void against_pcre2(const txt::regex& re, std::string_view p, const string& text) {
        if (!both_read_alike(p) || !utf8::valid(text.view())) {
            return;
        }
        pcre2_compile_context* cc = pcre2_compile_context_create(nullptr);
        pcre2_set_newline(cc, PCRE2_NEWLINE_LF);
        int error = 0;
        PCRE2_SIZE at = 0;
        pcre2_code* code = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(p.data()), p.size(),
                                         PCRE2_UTF | PCRE2_DOLLAR_ENDONLY, &error, &at, cc);
        pcre2_compile_context_free(cc);
        if (!code) {
            return;   // what PCRE2 refuses (a count past its own bounds) is nothing to compare
        }
        uint32_t groups = 0;
        pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &groups);
        check(groups == re.group_count());
        pcre2_match_data* md = pcre2_match_data_create_from_pattern(code, nullptr);
        int rc = pcre2_match(code, reinterpret_cast<PCRE2_SPTR>(text.data()), text.size(), 0, 0, md, nullptr);
        auto ours = re.find(text);
        if (rc == PCRE2_ERROR_NOMATCH) {
            check(!ours);
        } else if (rc > 0) {
            check(ours.has_value());
            PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
            Found theirs{size_t(ov[0]), size_t(ov[1]), {}};
            for (uint32_t g = 1; g <= groups; ++g) {
                bool unset = g >= uint32_t(rc) || ov[2 * g] == PCRE2_UNSET;
                theirs.groups.push_back(unset ? std::pair{None, None} : std::pair{size_t(ov[2 * g]), size_t(ov[2 * g + 1])});
            }
            check(same(found_of(*ours, text), theirs));
        }
        pcre2_match_data_free(md);
        pcre2_code_free(code);
    }
#endif

    void matching(const txt::regex& re, std::string_view p, const string& text, size_t from) {
        std::string_view view = text.view();
        bool utf8 = utf8::valid(view);
        check(re.pattern().view() == p);

        auto first = re.find(text);
        bool contains = re.contains(text);
        size_t count = re.count(text);
        check(contains == first.has_value());
        check(contains == (count > 0));
        check(re.all(text).empty() == !contains);

        // all() is find() asked again from where the last match left it
        std::vector<Found> every;
        size_t next = 0;
        for (const auto& m : re.all(text)) {
            Found f = found_of(m, text);
            well_formed(f, re, view, utf8);
            check(f.begin >= next);
            auto again = re.find(text, next);
            check(again.has_value() && same(found_of(*again, text), f));
            next = f.end + (f.begin == f.end ? one_point(view, f.end) : 0);
            every.push_back(std::move(f));
            check(every.size() <= view.size() + 1);
        }
        check(next > view.size() || !re.find(text, next));
        check(every.size() == count);
        if (first) {
            check(same(found_of(*first, text), every.front()));
        }

        // a search from a place finds nothing before it
        auto later = re.find(text, from);
        if (from > view.size()) {
            check(!later);
        } else if (later) {
            check(later->begin_at() >= from);
            well_formed(found_of(*later, text), re, view, utf8);
        }

        // the whole text
        bool full = re.full_match(text);
        if (full) {
            check(contains);
        }
        std::string wrapped = "\\A(?:" + std::string(p) + ")\\z";
        auto whole = txt::regex::compile(string(wrapped));
        if (whole) {
            auto m = whole->find(text);
            check(m.has_value() == full);
            if (m) {
                check(m->begin_at() == 0 && m->end_at() == view.size());
            }
        }

        // replace and split, which walk the matches a road of their own
        check(re.replace(text, string("$0")).view() == view);
        check(re.replace_first(text, string("$0")).view() == view);
        auto pieces = re.split(text);
        check(pieces.size() == count + 1);
        std::string joined;
        size_t at = 0;
        for (const auto& piece : pieces) {
            size_t piece_at = size_t(piece.data() - text.data());
            check(piece_at >= at && piece_at + piece.size() <= view.size());
            at = piece_at + piece.size();
            joined.append(piece.data(), piece.size());
        }
        check(re.replace(text, string("")).view() == joined);
        size_t limit = 1 + from % 4;
        check(re.split(text, limit).size() <= limit);
    }

    void compiling(std::string_view p, const string& text, size_t from) {
        auto re = txt::regex::compile(string(p));
        if (!re) {
            check(!re.error().message().empty());
            check(re.error().offset() <= p.size());
            return;
        }
        // linear, and kept well inside the clock: the text held to a
        // budget of instructions times bytes
        size_t budget = 400000 / (re->program_size() + 1);
        string subject = text.size() <= budget ? text : string(text.view().substr(0, budget));
        matching(*re, p, subject, from);
#ifdef SGCL_FUZZ_PCRE2
        against_pcre2(*re, p, subject);
#endif
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 4096) {
        return 0;
    }
    size_t from = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    size_t nul = rest.find('\0');
    std::string_view pattern = rest.substr(0, nul);
    std::string_view text = nul == std::string_view::npos ? std::string_view() : rest.substr(nul + 1);
    if (pattern.size() > 512) {
        return 0;
    }
    compiling(pattern, string(text), from);
    return 0;
}
