//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/string.h"
#include "../core/utf8.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <string_view>
#include <vector>

// How far apart two texts are: the edits that turn one into the other —
// Levenshtein's insertions, deletions and substitutions, with transpositions
// of neighbours (the optimal string alignment, and Damerau's unrestricted
// distance) — the longest common subsequence, and the similarity of Jaro
// and Winkler for names.
//
//     txt::levenshtein("kitten", "sitting")                 // 3
//     txt::levenshtein(typed, word, {.max = 2}) <= 2         // a spelling suggestion
//     txt::jaro_winkler("MARTHA", "MARHTA")                 // 0.961
//
// A unit is a code point (a byte that is not UTF-8 one of its own), or a
// byte when asked. Levenshtein and the subsequence are bit-parallel (Myers
// 1999, Hyyrö 2003, Allison and Dix 1986): 64 units of the shorter text a
// machine word, a step a unit of the longer.
namespace sgcl::txt {
    struct distance_options {
        size_t max = SIZE_MAX;
        bool bytes = false;
    };

    struct jaro_options {
        double prefix_scale = 0.1;
        size_t max_prefix = 4;
        double threshold = 0.7;
        bool bytes = false;
    };

    namespace detail::distance {
        // The units of a text: its bytes, when they are the units (asked
        // for, or a text of ASCII), else its code points decoded
        struct Units {
            const unsigned char* bytes = nullptr;
            std::vector<char32_t> points;
            size_t size = 0;
            bool wide = false;
        };

        inline void units_of(std::string_view s, bool wide, Units& u) {
            if (!wide) {
                u.bytes = reinterpret_cast<const unsigned char*>(s.data());
                u.size = s.size();
                return;
            }
            u.wide = true;
            u.points.reserve(s.size());
            for (size_t i = 0; i < s.size();) {
                auto [c, n] = utf8::decode(s, i);
                // an ill-formed byte: a unit of its own, unlike any code point
                u.points.push_back(n == 1 && static_cast<unsigned char>(s[i]) >= 0x80 ? char32_t(0x110000 + uint8_t(s[i])) : c);
                i += n;
            }
            u.size = u.points.size();
        }

        // The rows of bits of the pattern: for a unit, the bit of each
        // place of the pattern where it stands, one word per 64 places
        template<class U>
        class Masks;

        template<>
        class Masks<unsigned char> {
        public:
            Masks(const unsigned char* p, size_t m)
            : _words((m + 63) / 64) {
                if (_words == 1) {
                    std::fill(std::begin(_one), std::end(_one), uint64_t(0));
                    for (size_t i = 0; i < m; ++i) {
                        _one[p[i]] |= uint64_t(1) << i;
                    }
                    return;
                }
                _rows.assign(256 * _words, 0);
                for (size_t i = 0; i < m; ++i) {
                    _rows[size_t(p[i]) * _words + i / 64] |= uint64_t(1) << (i % 64);
                }
            }

            const uint64_t* row(unsigned char c) const noexcept {
                return _words == 1 ? &_one[c] : &_rows[size_t(c) * _words];
            }

        private:
            size_t _words;
            uint64_t _one[256];               // a pattern of one word: no allocation
            std::vector<uint64_t> _rows;
        };

        template<>
        class Masks<char32_t> {
        public:
            Masks(const char32_t* p, size_t m)
            : _words((m + 63) / 64) {
                size_t cap = 16;
                while (cap < 2 * m + 2) {
                    cap <<= 1;
                }
                _mask = cap - 1;
                _keys.assign(cap, Empty);
                _rows.assign((cap + 1) * _words, 0);   // the last row: of a unit the pattern has not
                for (size_t i = 0; i < m; ++i) {
                    size_t s = slot(p[i]);
                    _keys[s] = p[i];
                    _rows[s * _words + i / 64] |= uint64_t(1) << (i % 64);
                }
            }

            const uint64_t* row(char32_t c) const noexcept {
                size_t s = slot(c);
                return _keys[s] == c ? &_rows[s * _words] : &_rows[(_mask + 1) * _words];
            }

        private:
            static constexpr char32_t Empty = 0xFFFFFFFF;
            size_t _words;
            size_t _mask = 0;
            std::vector<char32_t> _keys;
            std::vector<uint64_t> _rows;

            size_t slot(char32_t c) const noexcept {
                size_t s = (size_t(c) * 0x9E3779B97F4A7C15ull) >> 32 & _mask;
                while (_keys[s] != Empty && _keys[s] != c) {
                    s = (s + 1) & _mask;
                }
                return s;
            }
        };

        // Levenshtein by Myers' blocks: the vertical deltas of each column
        // of the table as two words a block (+1 and -1), the horizontal delta
        // carried from block to block; p the shorter
        template<class U>
        size_t levenshtein_bits(const U* p, size_t m, const U* t, size_t n) {
            Masks<U> pm(p, m);
            size_t words = (m + 63) / 64;
            uint64_t high = uint64_t(1) << ((m - 1) % 64);
            size_t score = m;
            if (words == 1) {
                uint64_t vp = ~uint64_t(0), vn = 0;
                for (size_t j = 0; j < n; ++j) {
                    uint64_t eq = pm.row(t[j])[0];
                    uint64_t xv = eq | vn;
                    uint64_t xh = (((eq & vp) + vp) ^ vp) | eq;
                    uint64_t hp = vn | ~(xh | vp);
                    uint64_t hn = vp & xh;
                    score += (hp & high) ? 1 : 0;
                    score -= (hn & high) ? 1 : 0;
                    hp = (hp << 1) | 1;
                    hn <<= 1;
                    vp = hn | ~(xv | hp);
                    vn = hp & xv;
                }
                return score;
            }
            std::vector<uint64_t> vps(words, ~uint64_t(0)), vns(words, 0);
            for (size_t j = 0; j < n; ++j) {
                const uint64_t* eqs = pm.row(t[j]);
                int h = 1;   // the top row grows by one a column
                for (size_t b = 0; b < words; ++b) {
                    uint64_t vp = vps[b], vn = vns[b], eq = eqs[b];
                    uint64_t xv = eq | vn;
                    if (h < 0) {
                        eq |= 1;
                    }
                    uint64_t xh = (((eq & vp) + vp) ^ vp) | eq;
                    uint64_t hp = vn | ~(xh | vp);
                    uint64_t hn = vp & xh;
                    uint64_t top = b + 1 == words ? high : uint64_t(1) << 63;
                    int out = (hp & top) ? 1 : (hn & top) ? -1 : 0;
                    hp <<= 1;
                    hn <<= 1;
                    if (h < 0) {
                        hn |= 1;
                    } else if (h > 0) {
                        hp |= 1;
                    }
                    vps[b] = hn | ~(xv | hp);
                    vns[b] = hp & xv;
                    h = out;
                }
                score = size_t(long(score) + h);
            }
            return score;
        }

        // The longest common subsequence: a zero bit for each place of p in
        // it, S + (S & Eq) | S - (S & Eq) a step (Allison and Dix, Hyyrö)
        template<class U>
        size_t lcs_bits(const U* p, size_t m, const U* t, size_t n) {
            Masks<U> pm(p, m);
            size_t words = (m + 63) / 64;
            std::vector<uint64_t> s(words, ~uint64_t(0));
            for (size_t j = 0; j < n; ++j) {
                const uint64_t* eqs = pm.row(t[j]);
                uint64_t carry = 0;
                for (size_t b = 0; b < words; ++b) {
                    uint64_t x = s[b], u = x & eqs[b];
                    uint64_t sum = x + u;
                    uint64_t c1 = sum < x;
                    sum += carry;
                    uint64_t c2 = sum < carry;
                    carry = c1 | c2;
                    s[b] = sum | (x - u);
                }
            }
            size_t zeros = 0;
            for (size_t b = 0; b < words; ++b) {
                uint64_t live = b + 1 == words && m % 64 ? (uint64_t(1) << (m % 64)) - 1 : ~uint64_t(0);
                zeros += size_t(std::popcount(~s[b] & live));
            }
            return zeros;
        }

        // The optimal string alignment (Hyyrö 2003): the Levenshtein step
        // with the transpositions of the column before, the bit of a
        // transposition carried from word to word like the deltas
        template<class U>
        size_t osa_bits(const U* p, size_t m, const U* t, size_t n) {
            Masks<U> pm(p, m);
            size_t words = (m + 63) / 64;
            uint64_t high = uint64_t(1) << ((m - 1) % 64);
            size_t score = m;
            if (words == 1) {
                uint64_t vp = ~uint64_t(0), vn = 0, d0 = 0, before = 0;
                for (size_t j = 0; j < n; ++j) {
                    uint64_t eq = pm.row(t[j])[0];
                    uint64_t tr = (((~d0) & eq) << 1) & before;
                    d0 = (((eq & vp) + vp) ^ vp) | eq | vn | tr;
                    uint64_t hp = vn | ~(d0 | vp);
                    uint64_t hn = d0 & vp;
                    score += (hp & high) ? 1 : 0;
                    score -= (hn & high) ? 1 : 0;
                    hp = (hp << 1) | 1;
                    hn <<= 1;
                    vp = hn | ~(d0 | hp);
                    vn = hp & d0;
                    before = eq;
                }
                return score;
            }
            std::vector<uint64_t> vps(words, ~uint64_t(0)), vns(words, 0), d0s(words, 0);
            const uint64_t* before = nullptr;   // the row of the column before
            for (size_t j = 0; j < n; ++j) {
                const uint64_t* eqs = pm.row(t[j]);
                uint64_t hp_in = 1, hn_in = 0, tr_in = 0;
                for (size_t b = 0; b < words; ++b) {
                    uint64_t eq = eqs[b], vp = vps[b], vn = vns[b];
                    uint64_t x = (~d0s[b]) & eq;
                    uint64_t tr = before ? ((x << 1) | tr_in) & before[b] : 0;
                    tr_in = x >> 63;
                    uint64_t xe = eq | hn_in;
                    uint64_t d0 = (((xe & vp) + vp) ^ vp) | xe | vn | tr;
                    uint64_t hp = vn | ~(d0 | vp);
                    uint64_t hn = d0 & vp;
                    uint64_t top = b + 1 == words ? high : uint64_t(1) << 63;
                    uint64_t hp_out = (hp & top) ? 1 : 0, hn_out = (hn & top) ? 1 : 0;
                    hp = (hp << 1) | hp_in;
                    hn = (hn << 1) | hn_in;
                    vps[b] = hn | ~(d0 | hp);
                    vns[b] = hp & d0;
                    d0s[b] = d0;
                    hp_in = hp_out;
                    hn_in = hn_out;
                }
                score = score + hp_in - hn_in;
                before = eqs;
            }
            return score;
        }

        // The last row each unit stood on: a table of the bytes, a small
        // open-addressing map of the code points
        template<class U>
        class LastRow;

        template<>
        class LastRow<unsigned char> {
        public:
            size_t get(unsigned char c) const noexcept {
                return _rows[c];
            }

            void set(unsigned char c, size_t i) noexcept {
                _rows[c] = i;
            }

        private:
            size_t _rows[256] = {};
        };

        template<>
        class LastRow<char32_t> {
        public:
            LastRow()
            : _keys(16, Empty)
            , _rows(16, 0) {
            }

            size_t get(char32_t c) const noexcept {
                size_t s = slot(c);
                return _keys[s] == c ? _rows[s] : 0;
            }

            void set(char32_t c, size_t i) {
                size_t s = slot(c);
                if (_keys[s] != c) {
                    if (2 * (_used + 1) > _keys.size()) {
                        grow();
                        s = slot(c);
                    }
                    _keys[s] = c;
                    ++_used;
                }
                _rows[s] = i;
            }

        private:
            static constexpr char32_t Empty = 0xFFFFFFFF;
            std::vector<char32_t> _keys;
            std::vector<size_t> _rows;
            size_t _used = 0;

            size_t slot(char32_t c) const noexcept {
                size_t mask = _keys.size() - 1;
                size_t s = (size_t(c) * 0x9E3779B97F4A7C15ull) >> 32 & mask;
                while (_keys[s] != Empty && _keys[s] != c) {
                    s = (s + 1) & mask;
                }
                return s;
            }

            void grow() {
                std::vector<char32_t> keys(_keys.size() * 2, Empty);
                std::vector<size_t> rows(_keys.size() * 2, 0);
                std::swap(keys, _keys);
                std::swap(rows, _rows);
                for (size_t k = 0; k < keys.size(); ++k) {
                    if (keys[k] != Empty) {
                        size_t s = slot(keys[k]);
                        _keys[s] = keys[k];
                        _rows[s] = rows[k];
                    }
                }
            }
        };

        // Damerau's distance without the restriction (Lowrance and Wagner
        // 1975) in linear memory: a transposition is only ever worth taking
        // where one of its two sides is next to its end (Zhao and Sahni
        // 2019), so the table needs, besides the row before, the row before
        // that at the column of each match and the value of two rows up
        // where the row last matched
        template<class U>
        size_t damerau_rows(const U* a, size_t n, const U* b, size_t m) {
            const size_t big = n + m + 1;
            // the rows i - 2, i - 1 and i; column j at index j + 1, index 0 the column before the first
            std::vector<size_t> r2(m + 2, big), r1(m + 2), r(m + 2);
            r1[0] = big;
            for (size_t j = 0; j <= m; ++j) {
                r1[j + 1] = j;
            }
            // of each column j, at its last match (row k): the row k - 1 two columns left
            std::vector<size_t> fr(m + 2, big);
            LastRow<U> last;   // the last row (from 1) each unit of a stood on
            for (size_t i = 1; i <= n; ++i) {
                r[0] = big;
                r[1] = i;
                size_t l = 0;          // the last column of this row where b matched a[i - 1]
                size_t two_up = big;   // the row i - 2 a column left of that match
                for (size_t j = 1; j <= m; ++j) {
                    bool same = a[i - 1] == b[j - 1];
                    size_t v = std::min({r1[j] + (same ? 0 : 1), r[j] + 1, r1[j + 1] + 1});
                    if (same) {
                        l = j;
                        fr[j + 1] = r1[j - 1];
                        two_up = r2[j];
                    } else if (l > 0) {
                        size_t k = last.get(b[j - 1]);
                        if (k > 0) {
                            if (j - l == 1) {
                                v = std::min(v, fr[j + 1] + (i - k));
                            } else if (i - k == 1) {
                                v = std::min(v, two_up + (j - l));
                            }
                        }
                    }
                    r[j + 1] = v;
                }
                last.set(a[i - 1], i);
                std::swap(r2, r1);   // r2: the row i - 1
                std::swap(r1, r);    // r1: the row i; r: the row i - 2, written over next
            }
            return r1[m + 1];
        }

        enum class Measure { levenshtein, osa, damerau, lcs };

        template<class U>
        size_t measure(Measure what, const U* a, size_t n, const U* b, size_t m) {
            // the common ends are in every alignment
            size_t front = 0;
            while (front < n && front < m && a[front] == b[front]) {
                ++front;
            }
            size_t back = 0;
            while (back < n - front && back < m - front && a[n - 1 - back] == b[m - 1 - back]) {
                ++back;
            }
            a += front;
            b += front;
            n -= front + back;
            m -= front + back;
            if (what == Measure::lcs) {
                if (n == 0 || m == 0) {
                    return front + back;
                }
                return front + back + (n <= m ? lcs_bits(a, n, b, m) : lcs_bits(b, m, a, n));
            }
            if (n == 0 || m == 0) {
                return n + m;
            }
            if (n > m) {
                std::swap(a, b);
                std::swap(n, m);
            }
            switch (what) {
                case Measure::levenshtein:
                    return levenshtein_bits(a, n, b, m);
                case Measure::osa:
                    return osa_bits(a, n, b, m);
                default:
                    return damerau_rows(a, n, b, m);
            }
        }

        inline size_t run(Measure what, std::string_view a, std::string_view b, const distance_options& o) {
            bool wide = !o.bytes && !(utf8::all_ascii(a) && utf8::all_ascii(b));
            Units ua, ub;
            units_of(a, wide, ua);
            units_of(b, wide, ub);
            // the lengths alone: a distance is at least their difference
            if (what != Measure::lcs && (ua.size > ub.size ? ua.size - ub.size : ub.size - ua.size) > o.max) {
                return o.max + 1;
            }
            size_t d = wide ? measure(what, ua.points.data(), ua.size, ub.points.data(), ub.size)
                            : measure(what, ua.bytes, ua.size, ub.bytes, ub.size);
            return d > o.max ? o.max + 1 : d;
        }

        template<class U>
        double jaro_of(const U* a, size_t n, const U* b, size_t m) {
            if (n == 0 && m == 0) {
                return 1.0;
            }
            if (n == 0 || m == 0) {
                return 0.0;
            }
            size_t window = std::max(n, m) / 2;
            window = window > 0 ? window - 1 : 0;
            std::vector<char> fa(n, 0), fb(m, 0);
            size_t matches = 0;
            for (size_t i = 0; i < n; ++i) {
                size_t lo = i > window ? i - window : 0, hi = std::min(m, i + window + 1);
                for (size_t j = lo; j < hi; ++j) {
                    if (!fb[j] && a[i] == b[j]) {
                        fa[i] = fb[j] = 1;
                        ++matches;
                        break;
                    }
                }
            }
            if (matches == 0) {
                return 0.0;
            }
            size_t half = 0;   // matched units out of order, each transposition counted twice
            for (size_t i = 0, k = 0; i < n; ++i) {
                if (fa[i]) {
                    while (!fb[k]) {
                        ++k;
                    }
                    half += a[i] != b[k];
                    ++k;
                }
            }
            double mm = double(matches);
            return (mm / double(n) + mm / double(m) + (mm - double(half) / 2) / mm) / 3;
        }

        inline double jaro_run(std::string_view a, std::string_view b, const jaro_options& o, bool winkler) {
            bool wide = !o.bytes && !(utf8::all_ascii(a) && utf8::all_ascii(b));
            Units ua, ub;
            units_of(a, wide, ua);
            units_of(b, wide, ub);
            double j = wide ? jaro_of(ua.points.data(), ua.size, ub.points.data(), ub.size)
                            : jaro_of(ua.bytes, ua.size, ub.bytes, ub.size);
            if (!winkler || !(j > o.threshold)) {
                return j;
            }
            size_t prefix = 0, most = std::min({o.max_prefix, ua.size, ub.size});
            if (wide) {
                while (prefix < most && ua.points[prefix] == ub.points[prefix]) {
                    ++prefix;
                }
            } else {
                while (prefix < most && ua.bytes[prefix] == ub.bytes[prefix]) {
                    ++prefix;
                }
            }
            return j + double(prefix) * o.prefix_scale * (1 - j);
        }
    }

    // The fewest insertions, deletions and substitutions of units that turn
    // a into b
    SGCL_INLINE_HOT size_t levenshtein(const string& a, const string& b, const distance_options& o = {}) {
        return detail::distance::run(detail::distance::Measure::levenshtein, a.view(), b.view(), o);
    }

    // ... and transpositions of neighbouring units, no unit edited twice
    // (the optimal string alignment)
    SGCL_INLINE_HOT size_t osa_distance(const string& a, const string& b, const distance_options& o = {}) {
        return detail::distance::run(detail::distance::Measure::osa, a.view(), b.view(), o);
    }

    // ... and transpositions without that restriction: Damerau's distance,
    // a metric
    SGCL_INLINE_HOT size_t damerau_levenshtein(const string& a, const string& b, const distance_options& o = {}) {
        return detail::distance::run(detail::distance::Measure::damerau, a.view(), b.view(), o);
    }

    // The length of the longest subsequence both texts have
    SGCL_INLINE_HOT size_t lcs_length(const string& a, const string& b, const distance_options& o = {}) {
        return detail::distance::run(detail::distance::Measure::lcs, a.view(), b.view(), o);
    }

    // Jaro's similarity: 1 for equal texts, 0 for none in common
    SGCL_INLINE_HOT double jaro(const string& a, const string& b, const jaro_options& o = {}) {
        return detail::distance::jaro_run(a.view(), b.view(), o, false);
    }

    // Jaro's similarity raised for a common prefix (Winkler)
    SGCL_INLINE_HOT double jaro_winkler(const string& a, const string& b, const jaro_options& o = {}) {
        return detail::distance::jaro_run(a.view(), b.view(), o, true);
    }
}
