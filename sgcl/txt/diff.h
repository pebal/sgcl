//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/detail/hash_bytes.h"
#include "../core/expected.h"
#include "../core/string.h"
#include "../core/vector.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Differences between texts: the edit script that turns one into the other —
// by lines, by words or by code points — found by Myers' algorithm (a shortest
// script, in linear space) or by patience diff (lines unique to both sides
// anchoring first); the unified format of diff -u and git diff; a unified
// patch applied as patch(1) applies it, with its offsets and fuzz; and the
// three-way merge of diff3 and git merge-file.
//
//     auto patch = txt::unified_diff(before, after, {.old_name = "a/f.txt", .new_name = "b/f.txt"});
//     auto again = txt::apply_patch(before, patch);              // after
//     auto m = txt::merge3(base, ours, theirs);                  // m.conflicts, m.text
//
// An edit is a pair of byte ranges of the two texts, whatever the unit: whole
// lines with their line feeds, words, code points. /usr/bin/patch and git
// merge-file are the oracles of the tests.
namespace sgcl::txt {
    enum class diff_algorithm : uint8_t {
        myers,
        patience,
    };

    enum class diff_kind : uint8_t {
        equal,
        insert,
        remove,
    };

    struct diff_edit {
        diff_kind kind = diff_kind::equal;
        size_t old_begin = 0, old_end = 0;   // bytes of the old text
        size_t new_begin = 0, new_end = 0;   // bytes of the new text
    };

    struct diff_options {
        diff_algorithm algorithm = diff_algorithm::myers;
        bool ignore_whitespace = false;
    };

    struct unified_options {
        size_t context = 3;
        string old_name = string("a");
        string new_name = string("b");
        diff_algorithm algorithm = diff_algorithm::myers;
        bool ignore_whitespace = false;
    };

    struct patch_options {
        size_t fuzz = 2;
        bool reverse = false;
    };

    class patch_error {
    public:
        SGCL_INLINE_HOT patch_error(size_t hunk, size_t line, const char* reason) noexcept
        : _hunk(hunk)
        , _line(line)
        , _reason(reason) {
        }

        // The hunk that failed, from 1; 0 when the patch itself is not one
        SGCL_INLINE_HOT size_t hunk() const noexcept {
            return _hunk;
        }

        // Its line in the patch, from 1
        SGCL_INLINE_HOT size_t line() const noexcept {
            return _line;
        }

        // Why, in a few words
        SGCL_INLINE_HOT string message() const noexcept {
            return string(_reason);
        }

    private:
        size_t _hunk;
        size_t _line;
        const char* _reason;
    };

    struct merge_options {
        string ours_label = string("ours");
        string base_label = string("base");
        string theirs_label = string("theirs");
        bool diff3 = false;
    };

    struct merge_result {
        string text;
        size_t conflicts = 0;
    };

    namespace detail::diff {
        struct Unit {
            size_t begin, end;   // bytes
        };

        // the lines of a text, each with its line feed
        inline std::vector<Unit> lines_of(std::string_view s) {
            std::vector<Unit> out;
            out.reserve(size_t(std::count(s.begin(), s.end(), '\n')) + 1);
            size_t b = 0;
            while (b < s.size()) {
                size_t e = s.find('\n', b);
                e = e == std::string_view::npos ? s.size() : e + 1;
                out.push_back(Unit{b, e});
                b = e;
            }
            return out;
        }

        inline size_t utf8_length(unsigned char c) noexcept {
            return c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        }

        inline bool word_char(std::string_view s, size_t i) noexcept {
            unsigned char c = (unsigned char)s[i];
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c >= 0x80;
        }

        inline bool space_char(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
        }

        // words: runs of letters and digits (and of what is not ASCII), runs
        // of white space, each other character alone
        inline std::vector<Unit> words_of(std::string_view s) {
            std::vector<Unit> out;
            size_t i = 0;
            while (i < s.size()) {
                size_t b = i;
                if (space_char(s[i])) {
                    while (i < s.size() && space_char(s[i])) {
                        ++i;
                    }
                } else if (word_char(s, i)) {
                    while (i < s.size() && word_char(s, i)) {
                        i += utf8_length((unsigned char)s[i]);
                    }
                } else {
                    ++i;
                }
                if (i > s.size()) {
                    i = s.size();
                }
                out.push_back(Unit{b, i});
            }
            return out;
        }

        inline std::vector<Unit> chars_of(std::string_view s) {
            std::vector<Unit> out;
            size_t i = 0;
            while (i < s.size()) {
                size_t n = utf8_length((unsigned char)s[i]);
                size_t e = std::min(s.size(), i + n);
                out.push_back(Unit{i, e});
                i = e;
            }
            return out;
        }

        // the units as numbers, equal units one number
        struct Interned {
            std::vector<int> a, b;
        };

        // equal but for white space
        inline bool same_but_space(std::string_view x, std::string_view y) noexcept {
            size_t i = 0, j = 0;
            for (;;) {
                while (i < x.size() && space_char(x[i])) {
                    ++i;
                }
                while (j < y.size() && space_char(y[j])) {
                    ++j;
                }
                if (i == x.size() || j == y.size()) {
                    return i == x.size() && j == y.size();
                }
                if (x[i++] != y[j++]) {
                    return false;
                }
            }
        }

        // an open-addressing table of the distinct units, each its first
        // occurrence (no copy of the texts)
        inline Interned intern(std::string_view a, const std::vector<Unit>& ua, std::string_view b,
                               const std::vector<Unit>& ub, bool ignore_ws) {
            struct Slot {
                uint64_t hash;
                const char* text;
                size_t size;
                int id;
            };
            size_t cap = 16;
            while (cap < (ua.size() + ub.size()) * 2) {
                cap <<= 1;
            }
            std::vector<Slot> table(cap, Slot{0, nullptr, 0, -1});
            std::string key;   // a unit without its white space
            int next = 0;
            auto id_of = [&](std::string_view u) {
                uint64_t h;
                if (ignore_ws) {
                    key.clear();
                    for (char c : u) {
                        if (!space_char(c)) {
                            key.push_back(c);
                        }
                    }
                    h = sgcl::detail::hash_bytes(key.data(), key.size());
                } else {
                    h = sgcl::detail::hash_bytes(u.data(), u.size());
                }
                for (size_t i = size_t(h) & (cap - 1);; i = (i + 1) & (cap - 1)) {
                    Slot& sl = table[i];
                    if (sl.id < 0) {
                        sl = Slot{h, u.data(), u.size(), next};
                        return next++;
                    }
                    if (sl.hash == h) {
                        std::string_view t(sl.text, sl.size);
                        if (ignore_ws ? same_but_space(t, u) : t == u) {
                            return sl.id;
                        }
                    }
                }
            };
            Interned r;
            r.a.reserve(ua.size());
            r.b.reserve(ub.size());
            for (const Unit& u : ua) {
                r.a.push_back(id_of(a.substr(u.begin, u.end - u.begin)));
            }
            for (const Unit& u : ub) {
                r.b.push_back(id_of(b.substr(u.begin, u.end - u.begin)));
            }
            return r;
        }

        // Myers' algorithm in linear space (Myers 1986, 4b): the units of a and
        // b changed, marked
        class Myers {
        public:
            Myers(const std::vector<int>& a, const std::vector<int>& b, std::vector<char>& ca, std::vector<char>& cb)
            : _a(a)
            , _b(b)
            , _ca(ca)
            , _cb(cb) {
            }

            void run(size_t a0, size_t a1, size_t b0, size_t b1) {
                // an explicit stack of ranges: the recursion of the halves
                std::vector<std::array<size_t, 4>> todo{{a0, a1, b0, b1}};
                while (!todo.empty()) {
                    auto [x0, x1, y0, y1] = todo.back();
                    todo.pop_back();
                    while (x0 < x1 && y0 < y1 && _a[x0] == _b[y0]) {
                        ++x0;
                        ++y0;
                    }
                    while (x0 < x1 && y0 < y1 && _a[x1 - 1] == _b[y1 - 1]) {
                        --x1;
                        --y1;
                    }
                    if (x0 == x1) {
                        for (size_t j = y0; j < y1; ++j) {
                            _cb[j] = 1;
                        }
                        continue;
                    }
                    if (y0 == y1) {
                        for (size_t i = x0; i < x1; ++i) {
                            _ca[i] = 1;
                        }
                        continue;
                    }
                    size_t sx, sy, ex, ey;
                    middle_snake(x0, x1, y0, y1, sx, sy, ex, ey);
                    todo.push_back({ex, x1, ey, y1});
                    todo.push_back({x0, sx, y0, sy});
                }
            }

        private:
            const std::vector<int>& _a;
            const std::vector<int>& _b;
            std::vector<char>& _ca;
            std::vector<char>& _cb;
            std::vector<long> _vf, _vb;

            // the middle snake of a[a0,a1) against b[b0,b1): from (sx,sy) to
            // (ex,ey), in absolute positions
            void middle_snake(size_t a0, size_t a1, size_t b0, size_t b1, size_t& sx, size_t& sy, size_t& ex,
                              size_t& ey) {
                long n = long(a1 - a0), m = long(b1 - b0);
                long delta = n - m;
                bool odd = delta & 1;
                long max = (n + m + 1) / 2 + 1;
                long off = max + 1;
                _vf.assign(size_t(2 * off + 2), 0);
                _vb.assign(size_t(2 * off + 2), 0);
                for (long d = 0; d <= max; ++d) {
                    for (long k = -d; k <= d; k += 2) {
                        long x;
                        if (k == -d || (k != d && _vf[size_t(off + k - 1)] < _vf[size_t(off + k + 1)])) {
                            x = _vf[size_t(off + k + 1)];
                        } else {
                            x = _vf[size_t(off + k - 1)] + 1;
                        }
                        long y = x - k;
                        long x_start = x, y_start = y;
                        while (x < n && y < m && _a[a0 + size_t(x)] == _b[b0 + size_t(y)]) {
                            ++x;
                            ++y;
                        }
                        _vf[size_t(off + k)] = x;
                        long kr = delta - k;
                        if (odd && kr >= -(d - 1) && kr <= d - 1 && x + _vb[size_t(off + kr)] >= n) {
                            sx = a0 + size_t(x_start);
                            sy = b0 + size_t(y_start);
                            ex = a0 + size_t(x);
                            ey = b0 + size_t(y);
                            return;
                        }
                    }
                    for (long kr = -d; kr <= d; kr += 2) {
                        long x;
                        if (kr == -d || (kr != d && _vb[size_t(off + kr - 1)] < _vb[size_t(off + kr + 1)])) {
                            x = _vb[size_t(off + kr + 1)];
                        } else {
                            x = _vb[size_t(off + kr - 1)] + 1;
                        }
                        long y = x - kr;
                        long x_start = x, y_start = y;
                        while (x < n && y < m && _a[a1 - 1 - size_t(x)] == _b[b1 - 1 - size_t(y)]) {
                            ++x;
                            ++y;
                        }
                        _vb[size_t(off + kr)] = x;
                        long k = delta - kr;
                        if (!odd && k >= -d && k <= d && x + _vf[size_t(off + k)] >= n) {
                            sx = a0 + size_t(n - x);
                            sy = b0 + size_t(m - y);
                            ex = a0 + size_t(n - x_start);
                            ey = b0 + size_t(m - y_start);
                            return;
                        }
                    }
                }
                // not reached: a snake always meets within (n+m+1)/2 steps
                sx = ex = a0;
                sy = ey = b0;
            }
        };

        // patience diff: the units unique to both ranges, their longest
        // increasing run as anchors, the gaps between them again; Myers where
        // nothing is unique
        inline void patience(const std::vector<int>& a, const std::vector<int>& b, std::vector<char>& ca,
                             std::vector<char>& cb) {
            Myers myers(a, b, ca, cb);
            std::vector<std::array<size_t, 4>> todo{{0, a.size(), 0, b.size()}};
            while (!todo.empty()) {
                auto [a0, a1, b0, b1] = todo.back();
                todo.pop_back();
                while (a0 < a1 && b0 < b1 && a[a0] == b[b0]) {
                    ++a0;
                    ++b0;
                }
                while (a0 < a1 && b0 < b1 && a[a1 - 1] == b[b1 - 1]) {
                    --a1;
                    --b1;
                }
                if (a0 == a1 || b0 == b1) {
                    myers.run(a0, a1, b0, b1);
                    continue;
                }
                // unique in both
                std::unordered_map<int, std::array<long, 4>> seen;   // count a, count b, pos a, pos b
                for (size_t i = a0; i < a1; ++i) {
                    auto& s = seen[a[i]];
                    ++s[0];
                    s[2] = long(i);
                }
                for (size_t j = b0; j < b1; ++j) {
                    auto it = seen.find(b[j]);
                    if (it != seen.end()) {
                        ++it->second[1];
                        it->second[3] = long(j);
                    }
                }
                std::vector<std::pair<long, long>> pairs;   // (pos a, pos b), in a's order
                for (size_t i = a0; i < a1; ++i) {
                    auto& s = seen[a[i]];
                    if (s[0] == 1 && s[1] == 1) {
                        pairs.emplace_back(long(i), s[3]);
                    }
                }
                if (pairs.empty()) {
                    myers.run(a0, a1, b0, b1);
                    continue;
                }
                // the longest run increasing in b (patience sorting)
                std::vector<size_t> tails, prev(pairs.size(), SIZE_MAX);
                for (size_t k = 0; k < pairs.size(); ++k) {
                    size_t lo = 0, hi = tails.size();
                    while (lo < hi) {
                        size_t mid = (lo + hi) / 2;
                        if (pairs[tails[mid]].second < pairs[k].second) {
                            lo = mid + 1;
                        } else {
                            hi = mid;
                        }
                    }
                    if (lo > 0) {
                        prev[k] = tails[lo - 1];
                    }
                    if (lo == tails.size()) {
                        tails.push_back(k);
                    } else {
                        tails[lo] = k;
                    }
                }
                std::vector<std::pair<long, long>> anchors;
                for (size_t k = tails.back(); k != SIZE_MAX; k = prev[k]) {
                    anchors.push_back(pairs[k]);
                }
                std::reverse(anchors.begin(), anchors.end());
                // the gaps between the anchors, last first (so the stack takes them in order)
                size_t pa = a0, pb = b0;
                std::vector<std::array<size_t, 4>> gaps;
                for (auto [i, j] : anchors) {
                    gaps.push_back({pa, size_t(i), pb, size_t(j)});
                    pa = size_t(i) + 1;
                    pb = size_t(j) + 1;
                }
                gaps.push_back({pa, a1, pb, b1});
                for (size_t g = gaps.size(); g-- > 0;) {
                    todo.push_back(gaps[g]);
                }
            }
        }

        inline std::vector<diff_edit> edits(const std::vector<Unit>& ua, const std::vector<Unit>& ub,
                                            const std::vector<char>& ca, const std::vector<char>& cb, size_t a_size,
                                            size_t b_size) {
            std::vector<diff_edit> out;
            size_t i = 0, j = 0;
            auto pos_a = [&](size_t k) { return k < ua.size() ? ua[k].begin : a_size; };
            auto pos_b = [&](size_t k) { return k < ub.size() ? ub[k].begin : b_size; };
            while (i < ua.size() || j < ub.size()) {
                size_t si = i, sj = j;
                while (i < ua.size() && j < ub.size() && !ca[i] && !cb[j]) {
                    ++i;
                    ++j;
                }
                if (i > si) {
                    out.push_back(diff_edit{diff_kind::equal, pos_a(si), pos_a(i), pos_b(sj), pos_b(j)});
                }
                si = i;
                sj = j;
                while (i < ua.size() && ca[i]) {
                    ++i;
                }
                if (i > si) {
                    out.push_back(diff_edit{diff_kind::remove, pos_a(si), pos_a(i), pos_b(j), pos_b(j)});
                }
                while (j < ub.size() && cb[j]) {
                    ++j;
                }
                if (j > sj) {
                    out.push_back(diff_edit{diff_kind::insert, pos_a(i), pos_a(i), pos_b(sj), pos_b(j)});
                }
                if (i == si && j == sj && (i < ua.size() || j < ub.size())) {
                    // unchanged on one side only, which a script never leaves: guard the loop
                    break;
                }
            }
            return out;
        }

        // Each run of changed units slid as far down as it goes, as diff and
        // git leave them: a run whose first unit equals the unit after it
        // moves past it, which changes nothing of the script's length — and
        // only where the other side has no change at that point, so the
        // units both sides keep stay matched in order
        inline void slide(const std::vector<int>& a, std::vector<char>& ca, const std::vector<int>& b,
                          std::vector<char>& cb) {
            // the position in b of each unchanged unit of a's, walking both
            auto one = [](const std::vector<int>& x, std::vector<char>& cx, const std::vector<int>& y,
                          const std::vector<char>& cy) {
                size_t i = 0, j = 0;   // j: the next unit of y, kept level with i
                while (i < x.size()) {
                    if (!cx[i]) {
                        while (j < y.size() && cy[j]) {
                            ++j;
                        }
                        ++i;
                        ++j;
                        continue;
                    }
                    size_t start = i;
                    while (i < x.size() && cx[i]) {
                        ++i;
                    }
                    // the run [start, i) of x; y's changes waiting at j?
                    bool other = j < y.size() && cy[j];
                    while (!other && i < x.size() && !cx[i] && x[start] == x[i]) {
                        cx[start] = 0;
                        cx[i] = 1;
                        ++start;
                        ++i;
                        ++j;   // the unit that left the run is matched with the next of y's
                        other = j < y.size() && cy[j];
                        while (i < x.size() && cx[i]) {
                            ++i;   // joined the run after it
                        }
                    }
                }
            };
            one(a, ca, b, cb);
            one(b, cb, a, ca);
        }

        // The changed marks of two texts' units
        struct Script {
            std::vector<Unit> ua, ub;
            std::vector<char> ca, cb;
        };

        inline Script make_script(std::string_view a, std::string_view b, std::vector<Unit> ua, std::vector<Unit> ub,
                             const diff_options& o) {
            Script s;
            s.ua = std::move(ua);
            s.ub = std::move(ub);
            Interned in = intern(a, s.ua, b, s.ub, o.ignore_whitespace);
            s.ca.assign(in.a.size(), 0);
            s.cb.assign(in.b.size(), 0);
            if (o.algorithm == diff_algorithm::patience) {
                patience(in.a, in.b, s.ca, s.cb);
            } else {
                Myers(in.a, in.b, s.ca, s.cb).run(0, in.a.size(), 0, in.b.size());
            }
            slide(in.a, s.ca, in.b, s.cb);
            return s;
        }

        inline vector<diff_edit> to_vector(const std::vector<diff_edit>& e) {
            vector<diff_edit> out;
            out.reserve(e.size());
            for (const diff_edit& x : e) {
                out.push_back(x);
            }
            return out;
        }
    }

    // The edit script from a to b by lines (each with its line feed)
    inline vector<diff_edit> diff_lines(const string& a, const string& b, const diff_options& o = {}) {
        using namespace detail::diff;
        Script s = make_script(a.view(), b.view(), lines_of(a.view()), lines_of(b.view()), o);
        return to_vector(edits(s.ua, s.ub, s.ca, s.cb, a.size(), b.size()));
    }

    // by words: runs of letters and digits, runs of white space, each other character
    inline vector<diff_edit> diff_words(const string& a, const string& b, const diff_options& o = {}) {
        using namespace detail::diff;
        Script s = make_script(a.view(), b.view(), words_of(a.view()), words_of(b.view()), o);
        return to_vector(edits(s.ua, s.ub, s.ca, s.cb, a.size(), b.size()));
    }

    // by code points
    inline vector<diff_edit> diff_chars(const string& a, const string& b, const diff_options& o = {}) {
        using namespace detail::diff;
        Script s = make_script(a.view(), b.view(), chars_of(a.view()), chars_of(b.view()), o);
        return to_vector(edits(s.ua, s.ub, s.ca, s.cb, a.size(), b.size()));
    }

    namespace detail::diff {
        // ---- the unified format ----
        inline void range(std::string& out, size_t start, size_t count) {
            // a range of no lines names the line before it
            out += std::to_string(count == 0 ? start : start + 1);
            if (count != 1) {
                out += ',' + std::to_string(count);
            }
        }

        inline std::string unified(std::string_view a, std::string_view b, const unified_options& o) {
            diff_options d{o.algorithm, o.ignore_whitespace};
            Script s = make_script(a, b, lines_of(a), lines_of(b), d);
            size_t na = s.ua.size(), nb = s.ub.size();
            // the changes as runs of lines: (a0, a1, b0, b1)
            struct Change {
                size_t a0, a1, b0, b1;
            };
            std::vector<Change> changes;
            size_t i = 0, j = 0;
            while (i < na || j < nb) {
                if (i < na && j < nb && !s.ca[i] && !s.cb[j]) {
                    ++i;
                    ++j;
                    continue;
                }
                Change c{i, i, j, j};
                while (i < na && s.ca[i]) {
                    ++i;
                }
                while (j < nb && s.cb[j]) {
                    ++j;
                }
                c.a1 = i;
                c.b1 = j;
                if (c.a0 == c.a1 && c.b0 == c.b1) {
                    break;
                }
                changes.push_back(c);
            }
            if (changes.empty()) {
                return std::string();
            }
            std::string out = "--- " + std::string(o.old_name.view()) + "\n+++ " + std::string(o.new_name.view()) + "\n";
            size_t ctx = o.context;
            auto line_a = [&](size_t k) { return a.substr(s.ua[k].begin, s.ua[k].end - s.ua[k].begin); };
            auto line_b = [&](size_t k) { return b.substr(s.ub[k].begin, s.ub[k].end - s.ub[k].begin); };
            auto put = [&](char mark, std::string_view l) {
                out.push_back(mark);
                out.append(l);
                if (l.empty() || l.back() != '\n') {
                    out.append("\n\\ No newline at end of file\n");
                }
            };
            size_t k = 0;
            while (k < changes.size()) {
                // a hunk: changes whose gaps are at most twice the context
                size_t last = k;
                while (last + 1 < changes.size() && changes[last + 1].a0 - changes[last].a1 <= 2 * ctx) {
                    ++last;
                }
                size_t ha0 = changes[k].a0 >= ctx ? changes[k].a0 - ctx : 0;
                size_t hb0 = changes[k].b0 - (changes[k].a0 - ha0);
                size_t ha1 = std::min(na, changes[last].a1 + ctx);
                size_t hb1 = changes[last].b1 + (ha1 - changes[last].a1);
                out += "@@ -";
                range(out, ha0, ha1 - ha0);
                out += " +";
                range(out, hb0, hb1 - hb0);
                out += " @@\n";
                size_t pa = ha0;
                for (size_t c = k; c <= last; ++c) {
                    while (pa < changes[c].a0) {
                        put(' ', line_a(pa));
                        ++pa;
                    }
                    for (size_t x = changes[c].a0; x < changes[c].a1; ++x) {
                        put('-', line_a(x));
                    }
                    for (size_t y = changes[c].b0; y < changes[c].b1; ++y) {
                        put('+', line_b(y));
                    }
                    pa = changes[c].a1;
                }
                while (pa < ha1) {
                    put(' ', line_a(pa));
                    ++pa;
                }
                k = last + 1;
            }
            return out;
        }

        // ---- patches ----
        struct Line {
            std::string_view text;   // without its line feed
            bool newline;
        };

        inline std::vector<Line> split_lines(std::string_view s) {
            std::vector<Line> out;
            out.reserve(size_t(std::count(s.begin(), s.end(), '\n')) + 1);
            size_t b = 0;
            while (b < s.size()) {
                size_t e = s.find('\n', b);
                if (e == std::string_view::npos) {
                    out.push_back(Line{s.substr(b), false});
                    break;
                }
                out.push_back(Line{s.substr(b, e - b), true});
                b = e + 1;
            }
            return out;
        }

        struct Hunk {
            size_t old_start = 0, old_count = 1, new_start = 0, new_count = 1;
            std::vector<char> kinds;      // ' ' '-' '+'
            std::vector<Line> lines;
            size_t at = 0;                // its line in the patch, from 1
        };

        inline bool parse_number(std::string_view s, size_t& i, size_t& v) noexcept {
            size_t b = i;
            v = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                if (v > (SIZE_MAX - 9) / 10) {
                    return false;
                }
                v = v * 10 + size_t(s[i] - '0');
                ++i;
            }
            return i > b;
        }

        // "@@ -l[,c] +l[,c] @@"
        inline bool parse_header(std::string_view h, Hunk& out) noexcept {
            size_t i = 0;
            if (h.substr(0, 4) != "@@ -") {
                return false;
            }
            i = 4;
            if (!parse_number(h, i, out.old_start)) {
                return false;
            }
            out.old_count = 1;
            if (i < h.size() && h[i] == ',') {
                ++i;
                if (!parse_number(h, i, out.old_count)) {
                    return false;
                }
            }
            if (h.substr(i, 2) != " +") {
                return false;
            }
            i += 2;
            if (!parse_number(h, i, out.new_start)) {
                return false;
            }
            out.new_count = 1;
            if (i < h.size() && h[i] == ',') {
                ++i;
                if (!parse_number(h, i, out.new_count)) {
                    return false;
                }
            }
            return h.substr(i, 3) == " @@";
        }

        inline optional<patch_error> parse_patch(std::string_view patch, std::vector<Hunk>& hunks) {
            std::vector<Line> lines = split_lines(patch);
            size_t k = 0;
            // the first file: a --- line and a +++ line (or straight to the hunks)
            while (k < lines.size() && lines[k].text.substr(0, 3) != "@@ ") {
                if (lines[k].text.substr(0, 4) == "--- " && k + 1 < lines.size() && lines[k + 1].text.substr(0, 4) == "+++ ") {
                    k += 2;
                    break;
                }
                ++k;
            }
            while (k < lines.size()) {
                std::string_view t = lines[k].text;
                if (t.substr(0, 4) == "--- " || t.substr(0, 5) == "diff ") {
                    break;   // the next file's
                }
                if (t.substr(0, 3) != "@@ ") {
                    ++k;
                    continue;
                }
                Hunk h;
                h.at = k + 1;
                if (!parse_header(t, h)) {
                    return patch_error(hunks.size() + 1, k + 1, "a hunk header that is not @@ -l,c +l,c @@");
                }
                ++k;
                size_t old_seen = 0, new_seen = 0;
                while (k < lines.size() && (old_seen < h.old_count || new_seen < h.new_count)) {
                    std::string_view l = lines[k].text;
                    char mark = l.empty() ? ' ' : l[0];
                    if (mark == '\\') {
                        // "\ No newline at end of file": the line before has none
                        if (!h.lines.empty()) {
                            h.lines.back().newline = false;
                        }
                        ++k;
                        continue;
                    }
                    if (mark != ' ' && mark != '-' && mark != '+') {
                        return patch_error(hunks.size() + 1, k + 1, "a hunk line that is not ' ', '-' or '+'");
                    }
                    h.kinds.push_back(mark);
                    h.lines.push_back(Line{l.empty() ? l : l.substr(1), true});
                    if (mark != '+') {
                        ++old_seen;
                    }
                    if (mark != '-') {
                        ++new_seen;
                    }
                    ++k;
                }
                if (old_seen != h.old_count || new_seen != h.new_count) {
                    return patch_error(hunks.size() + 1, h.at, "a hunk shorter than its header says");
                }
                // a marker right after the last line
                if (k < lines.size() && !lines[k].text.empty() && lines[k].text[0] == '\\') {
                    if (!h.lines.empty()) {
                        h.lines.back().newline = false;
                    }
                    ++k;
                }
                hunks.push_back(std::move(h));
            }
            if (hunks.empty()) {
                return patch_error(0, 0, "no hunk");
            }
            return nullopt;
        }

        inline bool same(const Line& x, const Line& y) noexcept {
            return x.text == y.text && x.newline == y.newline;
        }

        inline std::string apply(std::string_view text, const std::vector<Hunk>& hunks, const patch_options& o,
                                 optional<patch_error>& err) {
            std::vector<Line> lines = split_lines(text);
            std::string out;
            out.reserve(text.size() + text.size() / 8);
            auto put = [&out](const Line& l) {
                out.append(l.text);
                if (l.newline) {
                    out.push_back('\n');
                }
            };
            long offset = 0;
            size_t floor = 0;        // a hunk applies after the one before: the text's lines from here are not out yet
            size_t hunk_no = 0;
            for (const Hunk& h : hunks) {
                ++hunk_no;
                std::vector<Line> old_lines, new_lines;
                std::vector<char> old_kind, new_kind;
                for (size_t k = 0; k < h.lines.size(); ++k) {
                    char mark = h.kinds[k];
                    if (o.reverse) {
                        mark = mark == '-' ? '+' : mark == '+' ? '-' : ' ';
                    }
                    if (mark != '+') {
                        old_lines.push_back(h.lines[k]);
                        old_kind.push_back(mark);
                    }
                    if (mark != '-') {
                        new_lines.push_back(h.lines[k]);
                        new_kind.push_back(mark);
                    }
                }
                size_t start = o.reverse ? h.new_start : h.old_start;
                size_t count = o.reverse ? h.new_count : h.old_count;
                long expected = long(count == 0 ? start : start - (start > 0 ? 1 : 0)) + offset;
                bool done = false;
                // as patch(1): the context at the hunk's ends bounds the fuzz (lines dropped at both ends), and a
                // hunk with context at one end only was made at the other end of the text
                size_t prefix = 0, suffix = 0;
                while (prefix < h.kinds.size() && h.kinds[prefix] == ' ') {
                    ++prefix;
                }
                while (suffix < h.kinds.size() - prefix && h.kinds[h.kinds.size() - 1 - suffix] == ' ') {
                    ++suffix;
                }
                size_t blocks = 0;
                for (size_t k = 0; k < h.kinds.size(); ++k) {
                    blocks += h.kinds[k] != ' ' && (k == 0 || h.kinds[k - 1] == ' ');
                }
                // (patch(1) anchors a hunk without leading context only when it changes one run of lines)
                bool at_start = blocks == 1 && prefix == 0 && suffix > 0;
                bool at_end = blocks > 0 && suffix == 0 && prefix > 0;
                size_t old_suffix = 0;   // patch(1) bounds the fuzz by the old side's trailing context
                while (old_suffix < old_kind.size() && old_kind[old_kind.size() - 1 - old_suffix] == ' ') {
                    ++old_suffix;
                }
                size_t max_fuzz = std::min(o.fuzz, std::min(prefix, old_suffix));
                size_t size = old_lines.size();
                for (size_t fuzz = 0; fuzz <= max_fuzz && !done; ++fuzz) {
                    // the hunk stands at s, its first and last fuzz lines (context) not compared
                    if (size > lines.size() || (fuzz > 0 && size <= 2 * fuzz)) {
                        break;
                    }
                    long limit = long(lines.size() - size);
                    for (long step = 0; step <= limit + std::max<long>(0, expected) && !done; ++step) {
                        for (int side = 0; side < 2 && !done; ++side) {
                            long p = side == 0 ? expected + step : expected - step;
                            if ((side == 1 && step == 0) || p < long(floor) || p > limit || (size == 0 && p != expected)) {
                                continue;   // (a hunk of no old lines goes where its numbers say)
                            }
                            bool match = true;
                            for (size_t q = fuzz; q < size - fuzz && match; ++q) {
                                match = same(lines[size_t(p) + q], old_lines[q]);
                            }
                            // a new side whose last line has no line feed ends the text: the hunk must too
                            bool ends = size_t(p) + size == lines.size();
                            if (match && !new_lines.empty() && !new_lines.back().newline && !ends) {
                                match = false;
                            }
                            if (match && ((at_start && (p != long(floor) || !out.empty())) || (at_end && !ends))) {
                                match = false;
                            }
                            if (!match) {
                                continue;
                            }
                            // context lines are the text's (the fuzzed ones too), the rest the patch's
                            for (size_t k = floor; k < size_t(p); ++k) {
                                put(lines[k]);
                            }
                            size_t at = size_t(p);
                            for (size_t k = 0; k < h.lines.size(); ++k) {
                                char mark = h.kinds[k];
                                if (o.reverse) {
                                    mark = mark == '-' ? '+' : mark == '+' ? '-' : ' ';
                                }
                                if (mark == ' ') {
                                    Line l = lines[at++];
                                    if (at == size_t(p) + size && !new_lines.back().newline && new_kind.back() == ' ') {
                                        l.newline = false;
                                    }
                                    put(l);
                                } else if (mark == '-') {
                                    ++at;
                                } else {
                                    put(h.lines[k]);
                                }
                            }
                            offset = p - long(count == 0 ? start : start - (start > 0 ? 1 : 0));
                            floor = size_t(p) + size;
                            done = true;
                        }
                    }
                }
                if (!done) {
                    err = patch_error(hunk_no, h.at, "a hunk whose lines are not in the text");
                    return std::string();
                }
            }
            for (size_t k = floor; k < lines.size(); ++k) {
                put(lines[k]);
            }
            return out;
        }

        // ---- three-way merge ----
        struct Region {
            size_t b0, b1;   // base lines replaced
            size_t s0, s1;   // by these of the side
        };

        inline std::vector<Region> regions(std::string_view base, const std::vector<Unit>& ubase, std::string_view side,
                                           const std::vector<Unit>& uside) {
            Script s = make_script(base, side, ubase, uside, diff_options{});
            std::vector<Region> out;
            size_t i = 0, j = 0;
            while (i < s.ua.size() || j < s.ub.size()) {
                if (i < s.ua.size() && j < s.ub.size() && !s.ca[i] && !s.cb[j]) {
                    ++i;
                    ++j;
                    continue;
                }
                Region r{i, i, j, j};
                while (i < s.ua.size() && s.ca[i]) {
                    ++i;
                }
                while (j < s.ub.size() && s.cb[j]) {
                    ++j;
                }
                r.b1 = i;
                r.s1 = j;
                if (r.b0 == r.b1 && r.s0 == r.s1) {
                    break;
                }
                out.push_back(r);
            }
            return out;
        }

        struct MergePiece {
            bool conflict;
            std::string text, ours, base, theirs;
        };

        inline bool has_alnum(std::string_view t) noexcept {
            for (char c : t) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                    return true;
                }
            }
            return false;
        }

        inline size_t line_count(std::string_view t) noexcept {
            size_t n = size_t(std::count(t.begin(), t.end(), '\n'));
            return n + (!t.empty() && t.back() != '\n' ? 1 : 0);
        }

        // git's refinement of conflicts (its default, zealous merge): the two
        // sides of a conflict diffed against each other, the lines they share
        // taken out of it; then conflicts with at most three lines between
        // them, or only lines without a letter or a digit, joined into one
        template<class P>
        std::vector<P> refine(const std::vector<P>& in) {
            std::vector<P> split;
            auto push_plain = [&](std::vector<P>& v, const std::string& t) {
                if (t.empty()) {
                    return;
                }
                if (!v.empty() && !v.back().conflict) {
                    v.back().text += t;
                } else {
                    v.push_back(P{false, t, {}, {}, {}});
                }
            };
            for (const P& p : in) {
                if (!p.conflict) {
                    push_plain(split, p.text);
                    continue;
                }
                std::vector<Unit> ua = lines_of(p.ours), ut = lines_of(p.theirs);
                Script sc = make_script(p.ours, p.theirs, ua, ut, diff_options{});
                size_t i = 0, j = 0;
                while (i < ua.size() || j < ut.size()) {
                    size_t si = i, sj = j;
                    while (i < ua.size() && j < ut.size() && !sc.ca[i] && !sc.cb[j]) {
                        ++i;
                        ++j;
                    }
                    if (i > si) {
                        push_plain(split, p.ours.substr(ua[si].begin, ua[i - 1].end - ua[si].begin));
                    }
                    si = i;
                    sj = j;
                    while (i < ua.size() && sc.ca[i]) {
                        ++i;
                    }
                    while (j < ut.size() && sc.cb[j]) {
                        ++j;
                    }
                    if (i == si && j == sj) {
                        break;
                    }
                    P c{true, {}, {}, {}, {}};
                    if (i > si) {
                        c.ours = p.ours.substr(ua[si].begin, ua[i - 1].end - ua[si].begin);
                    }
                    if (j > sj) {
                        c.theirs = p.theirs.substr(ut[sj].begin, ut[j - 1].end - ut[sj].begin);
                    }
                    split.push_back(c);
                }
            }
            std::vector<P> out;
            for (size_t k = 0; k < split.size(); ++k) {
                const P& p = split[k];
                if (p.conflict && out.size() >= 2 && out.back().conflict == false && out[out.size() - 2].conflict
                    && (line_count(out.back().text) <= 3 || !has_alnum(out.back().text))) {
                    // the plain lines between two conflicts go into both sides of one
                    std::string gap = out.back().text;
                    out.pop_back();
                    P& prev = out.back();
                    prev.ours += gap + p.ours;
                    prev.theirs += gap + p.theirs;
                    continue;
                }
                out.push_back(p);
            }
            return out;
        }

        inline std::string merge(std::string_view base, std::string_view ours, std::string_view theirs,
                                 const merge_options& o, size_t& conflicts) {
            std::vector<Unit> ub = lines_of(base), uo = lines_of(ours), ut = lines_of(theirs);
            std::vector<Region> ro = regions(base, ub, ours, uo), rt = regions(base, ub, theirs, ut);
            auto text = [](std::string_view s, const std::vector<Unit>& u, size_t a, size_t b) {
                if (a >= b) {
                    return std::string_view();
                }
                return s.substr(u[a].begin, u[b - 1].end - u[a].begin);
            };
            // a side's lines for base lines [b0, b1), its regions inside applied
            auto side_text = [&](std::string_view s, const std::vector<Unit>& u, const std::vector<Region>& r,
                                 size_t first, size_t last, size_t b0, size_t b1) {
                std::string out;
                size_t pb = b0;
                for (size_t k = first; k < last; ++k) {
                    out.append(text(base, ub, pb, r[k].b0));
                    out.append(text(s, u, r[k].s0, r[k].s1));
                    pb = r[k].b1;
                }
                out.append(text(base, ub, pb, b1));
                return out;
            };
            // the merge as pieces: text both sides agree on, and conflicts
            using Piece = MergePiece;
            std::vector<Piece> pieces;
            auto plain = [&](std::string_view t) {
                if (t.empty()) {
                    return;
                }
                if (!pieces.empty() && !pieces.back().conflict) {
                    pieces.back().text.append(t);
                } else {
                    pieces.push_back(Piece{false, std::string(t), {}, {}, {}});
                }
            };
            size_t pb = 0, io = 0, it = 0;
            while (io < ro.size() || it < rt.size()) {
                // the next block: regions of both sides that meet, chained
                bool from_ours = it >= rt.size() || (io < ro.size() && ro[io].b0 <= rt[it].b0);
                size_t b0 = from_ours ? ro[io].b0 : rt[it].b0;
                size_t b1 = from_ours ? ro[io].b1 : rt[it].b1;
                size_t eo = io, et = it;
                if (from_ours) {
                    ++eo;
                } else {
                    ++et;
                }
                bool grew = true;
                while (grew) {
                    grew = false;
                    // regions of the two sides meet when their base ranges intersect or touch (no
                    // unchanged line between them), as git's merge has it
                    while (eo < ro.size() && ro[eo].b0 <= b1) {
                        b1 = std::max(b1, ro[eo].b1);
                        ++eo;
                        grew = true;
                    }
                    while (et < rt.size() && rt[et].b0 <= b1) {
                        b1 = std::max(b1, rt[et].b1);
                        ++et;
                        grew = true;
                    }
                }
                plain(text(base, ub, pb, b0));
                bool ours_changed = eo > io, theirs_changed = et > it;
                std::string so = side_text(ours, uo, ro, io, eo, b0, b1);
                std::string st = side_text(theirs, ut, rt, it, et, b0, b1);
                if (!theirs_changed) {
                    plain(so);
                } else if (!ours_changed || so == st) {
                    plain(st);
                } else {
                    pieces.push_back(Piece{true, {}, so, std::string(text(base, ub, b0, b1)), st});
                }
                pb = b1;
                io = eo;
                it = et;
            }
            plain(text(base, ub, pb, ub.size()));
            if (!o.diff3) {
                pieces = refine(pieces);
            }
            std::string out;
            conflicts = 0;
            auto ensure_newline = [&](std::string& t) {
                if (!t.empty() && t.back() != '\n') {
                    t.push_back('\n');
                }
            };
            for (const Piece& p : pieces) {
                if (!p.conflict) {
                    out.append(p.text);
                    continue;
                }
                ++conflicts;
                ensure_newline(out);
                out += "<<<<<<< " + std::string(o.ours_label.view()) + "\n";
                out.append(p.ours);
                ensure_newline(out);
                if (o.diff3) {
                    out += "||||||| " + std::string(o.base_label.view()) + "\n";
                    out.append(p.base);
                    ensure_newline(out);
                }
                out += "=======\n";
                out.append(p.theirs);
                ensure_newline(out);
                out += ">>>>>>> " + std::string(o.theirs_label.view()) + "\n";
            }
            return out;
        }
    }

    // The unified format of diff -u and git diff; empty when a and b are equal
    inline string unified_diff(const string& a, const string& b, const unified_options& o = {}) {
        return string(std::string_view(detail::diff::unified(a.view(), b.view(), o)));
    }

    // A unified patch (its first file's hunks) applied as patch(1) applies
    // it: each hunk where its line numbers say, else at the nearest offset
    // where its lines stand, else with up to o.fuzz lines of context at both
    // ends not compared; a hunk with context at one end only stands at the
    // other end of the text
    inline expected<string, patch_error> apply_patch(const string& text, const string& patch,
                                                     const patch_options& o = {}) {
        std::vector<detail::diff::Hunk> hunks;
        if (auto e = detail::diff::parse_patch(patch.view(), hunks)) {
            return unexpected<patch_error>(*e);
        }
        optional<patch_error> err;
        std::string out = detail::diff::apply(text.view(), hunks, o, err);
        if (err) {
            return unexpected<patch_error>(*err);
        }
        return string(std::string_view(out));
    }

    // The three-way merge by lines of diff3 -m and git merge-file: a change
    // made on one side taken, the same change made on both taken once,
    // different changes of the same or touching lines a conflict
    inline merge_result merge3(const string& base, const string& ours, const string& theirs,
                               const merge_options& o = {}) {
        merge_result r;
        std::string text = detail::diff::merge(base.view(), ours.view(), theirs.view(), o, r.conflicts);
        r.text = string(std::string_view(text));
        return r;
    }
}
