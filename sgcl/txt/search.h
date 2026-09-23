//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "case.h"

#include <algorithm>

// Finding a text inside a text. Three ways, for three questions. The
// bytes as they stand, which is what a parser wants and what `find` on a
// string already does — here with the pattern prepared once, for the
// loop that looks for the same thing in many texts. Blind to case, which
// is what a person searching wants. And blind to the way the text was
// written, which is what a search over names and file paths wants, since
// "é" typed in two code points must find "é" stored in one.
namespace sgcl::txt {
    // A pattern prepared once: Boyer–Moore–Horspool over the bytes, which
    // needs no more than a table of skips and finds a pattern of m bytes
    // in n bytes in about n/m steps on ordinary text. The bytes are safe
    // to search: UTF-8 synchronises itself, so a match of valid UTF-8
    // inside valid UTF-8 always begins on a character.
    class searcher {
    public:
        explicit searcher(const string& pattern)
        : _pattern(pattern) {
            auto p = pattern.view();
            // Capped, not truncated: a pattern of exactly 65536 bytes
            // narrowed to a skip of zero and the search below never
            // moved. A skip shorter than the true one only costs steps —
            // it can never pass over a match — so the cap is safe and
            // the table stays at half a kilobyte.
            uint16_t whole = uint16_t(std::min(p.size(), size_t(0xFFFF)));
            for (auto& s : _skip) {
                s = whole;
            }
            for (size_t i = 0; i + 1 < p.size(); ++i) {
                size_t back = p.size() - 1 - i;
                _skip[uint8_t(p[i])] = uint16_t(std::min(back, size_t(0xFFFF)));
            }
        }

        // The byte position of the first occurrence at or after `from`,
        // or npos. An empty pattern is found at once, as it is in a
        // std::string.
        size_t find(const string& text, size_t from = 0) const noexcept {
            auto t = text.view();
            auto p = _pattern.view();
            if (p.empty()) {
                return from <= t.size() ? from : npos;
            }
            if (p.size() > t.size()) {
                return npos;
            }
            size_t at = from;
            while (at + p.size() <= t.size()) {
                size_t last = at + p.size() - 1;
                if (t[last] == p.back() && t.compare(at, p.size(), p) == 0) {
                    return at;
                }
                at += _skip[uint8_t(t[last])];
            }
            return npos;
        }

        bool contains(const string& text) const noexcept {
            return find(text) != npos;
        }

        // The occurrences that do not overlap, counted left to right
        size_t count(const string& text) const noexcept {
            if (_pattern.empty()) {
                return 0;
            }
            size_t n = 0;
            for (size_t at = find(text); at != npos; at = find(text, at + _pattern.size())) {
                ++n;
            }
            return n;
        }

        const string& pattern() const noexcept {
            return _pattern;
        }

    private:
        string _pattern;
        uint16_t _skip[256] = {};
    };

    namespace detail {
        // A text as a sequence of code points, with the byte position in
        // the original that each of them came from, so that a match found
        // in the mapped text can be reported where the caller can use it
        struct mapped_text {
            vector<char32_t> points;
            vector<size_t> at;
        };

        template<class F>
        mapped_text mapped(std::string_view text, F&& each) {
            mapped_text out;
            out.points.reserve(text.size());
            out.at.reserve(text.size());
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                size_t before = out.points.size();
                each(out.points, text, i, i + n, c);
                for (size_t k = before; k < out.points.size(); ++k) {
                    out.at.push_back(i);
                }
                i += n;
            }
            out.at.push_back(text.size());
            return out;
        }

        inline mapped_text folded(std::string_view text) {
            return mapped(text, [](vector<char32_t>& out, std::string_view, size_t, size_t, char32_t c) {
                if (auto d = full_of(c, case_tables::FullFold)) {
                    append(out, d);
                } else {
                    out.push_back(unicode::to_lower(c));
                }
            });
        }

        // Decomposed and put in canonical order. The order is fixed over
        // the whole text rather than per code point, so the positions
        // follow the marks they were taken from
        inline mapped_text decomposed(std::string_view text) {
            auto out = mapped(text, [](vector<char32_t>& points, std::string_view, size_t, size_t, char32_t c) {
                decompose_into<false>(points, c);
            });
            for (size_t i = 1; i < out.points.size(); ++i) {
                uint8_t cc = ccc_fn(out.points[i]);
                if (cc == 0) {
                    continue;
                }
                char32_t c = out.points[i];
                size_t from = out.at[i];
                size_t j = i;
                while (j > 0 && ccc_fn(out.points[j - 1]) != 0 && ccc_fn(out.points[j - 1]) > cc) {
                    out.points[j] = out.points[j - 1];
                    out.at[j] = out.at[j - 1];
                    --j;
                }
                out.points[j] = c;
                out.at[j] = from;
            }
            return out;
        }

        // The pattern's code points inside the text's, and the byte
        // position in the original text where the match begins
        inline size_t find_points(const mapped_text& text, const vector<char32_t>& pattern, size_t from) {
            if (pattern.empty()) {
                return from;
            }
            if (pattern.size() > text.points.size()) {
                return npos;
            }
            for (size_t i = 0; i + pattern.size() <= text.points.size(); ++i) {
                if (text.at[i] < from) {
                    continue;
                }
                size_t k = 0;
                while (k < pattern.size() && text.points[i + k] == pattern[k]) {
                    ++k;
                }
                if (k == pattern.size()) {
                    return text.at[i];
                }
            }
            return npos;
        }
    }

    // The first occurrence of the pattern without regard to case, as a
    // byte position in the text (not in any folded copy of it), or npos.
    // Both sides are folded ([case]), so "STRASSE" finds "straße".
    inline size_t find_fold(const string& text, const string& pattern, size_t from = 0) {
        auto t = detail::folded(text.view());
        auto p = detail::folded(pattern.view());
        return detail::find_points(t, p.points, from);
    }

    inline bool contains_fold(const string& text, const string& pattern) {
        return find_fold(text, pattern) != npos;
    }

    // The first occurrence without regard to the way either side was
    // written: both are decomposed and put in canonical order first, so
    // an "é" of one code point finds an "é" of two. The position is in
    // the original text.
    inline size_t find_normalized(const string& text, const string& pattern, size_t from = 0) {
        auto t = detail::decomposed(text.view());
        auto p = detail::decomposed(pattern.view());
        return detail::find_points(t, p.points, from);
    }

    inline bool contains_normalized(const string& text, const string& pattern) {
        return find_normalized(text, pattern) != npos;
    }
}
