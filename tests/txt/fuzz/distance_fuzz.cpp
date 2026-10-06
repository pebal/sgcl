//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The distances on any two texts. The first byte picks the units and a
// cutoff; the rest splits at its first NUL into the two texts, each copied
// into a buffer of exactly its size (never a managed copy, where ASan does not
// see a read past the end). What must hold:
//   - the bit-parallel Levenshtein and subsequence, the optimal string
//     alignment and Damerau's distance in linear memory equal the textbook
//     tables computed here, over the same units;
//   - every distance is symmetric, Damerau ≤ OSA ≤ Levenshtein, the cutoff
//     caps at max + 1, Jaro and Jaro-Winkler lie in [0, 1].
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/distance_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/txt/txt.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    namespace d = txt::detail::distance;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Exact {
        char* p;
        size_t n;

        explicit Exact(std::string_view s)
        : p(static_cast<char*>(std::malloc(s.size() ? s.size() : 1)))
        , n(s.size()) {
            std::copy(s.begin(), s.end(), p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(p);
        }

        std::string_view view() const {
            return std::string_view(p, n);
        }
    };

    std::vector<char32_t> units(std::string_view s, bool bytes) {
        d::Units u;
        bool wide = !bytes && !utf8::all_ascii(s);
        d::units_of(s, wide, u);
        std::vector<char32_t> out;
        for (size_t i = 0; i < u.size; ++i) {
            out.push_back(wide ? u.points[i] : char32_t(u.bytes[i]));
        }
        return out;
    }

    // the tables of the definitions
    size_t lev_table(const std::vector<char32_t>& a, const std::vector<char32_t>& b, bool osa) {
        std::vector<std::vector<size_t>> t(a.size() + 1, std::vector<size_t>(b.size() + 1));
        for (size_t i = 0; i <= a.size(); ++i) {
            t[i][0] = i;
        }
        for (size_t j = 0; j <= b.size(); ++j) {
            t[0][j] = j;
        }
        for (size_t i = 1; i <= a.size(); ++i) {
            for (size_t j = 1; j <= b.size(); ++j) {
                t[i][j] = std::min({t[i - 1][j] + 1, t[i][j - 1] + 1, t[i - 1][j - 1] + (a[i - 1] != b[j - 1])});
                if (osa && i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                    t[i][j] = std::min(t[i][j], t[i - 2][j - 2] + 1);
                }
            }
        }
        return t[a.size()][b.size()];
    }

    size_t damerau_table(const std::vector<char32_t>& a, const std::vector<char32_t>& b) {
        size_t inf = a.size() + b.size();
        std::vector<std::vector<size_t>> t(a.size() + 2, std::vector<size_t>(b.size() + 2, 0));
        t[0][0] = inf;
        for (size_t i = 0; i <= a.size(); ++i) {
            t[i + 1][0] = inf;
            t[i + 1][1] = i;
        }
        for (size_t j = 0; j <= b.size(); ++j) {
            t[0][j + 1] = inf;
            t[1][j + 1] = j;
        }
        std::map<char32_t, size_t> da;
        for (size_t i = 1; i <= a.size(); ++i) {
            size_t db = 0;
            for (size_t j = 1; j <= b.size(); ++j) {
                auto it = da.find(b[j - 1]);
                size_t k = it == da.end() ? 0 : it->second, l = db, cost = 1;
                if (a[i - 1] == b[j - 1]) {
                    cost = 0;
                    db = j;
                }
                t[i + 1][j + 1] = std::min({t[i][j] + cost, t[i + 1][j] + 1, t[i][j + 1] + 1,
                                            t[k][l] + (i - k - 1) + 1 + (j - l - 1)});
            }
            da[a[i - 1]] = i;
        }
        return t[a.size() + 1][b.size() + 1];
    }

    size_t lcs_table(const std::vector<char32_t>& a, const std::vector<char32_t>& b) {
        std::vector<size_t> prev(b.size() + 1, 0), cur(b.size() + 1, 0);
        for (size_t i = 0; i < a.size(); ++i) {
            for (size_t j = 0; j < b.size(); ++j) {
                cur[j + 1] = a[i] == b[j] ? prev[j] + 1 : std::max(prev[j + 1], cur[j]);
            }
            std::swap(prev, cur);
        }
        return prev[b.size()];
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 2048) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    size_t z = rest.find('\0');
    Exact a(rest.substr(0, z)), b(z == std::string_view::npos ? std::string_view() : rest.substr(z + 1));
    bool bytes = mode & 1;
    txt::distance_options o{.bytes = bytes};
    using M = d::Measure;
    size_t lev = d::run(M::levenshtein, a.view(), b.view(), o), osa = d::run(M::osa, a.view(), b.view(), o),
           dam = d::run(M::damerau, a.view(), b.view(), o), lcs = d::run(M::lcs, a.view(), b.view(), o);
    check(lev == d::run(M::levenshtein, b.view(), a.view(), o));
    check(osa == d::run(M::osa, b.view(), a.view(), o));
    check(dam == d::run(M::damerau, b.view(), a.view(), o));
    check(lcs == d::run(M::lcs, b.view(), a.view(), o));
    check(dam <= osa && osa <= lev);
    std::vector<char32_t> ua = units(a.view(), bytes), ub = units(b.view(), bytes);
    check(lev == lev_table(ua, ub, false));
    check(osa == lev_table(ua, ub, true));
    check(dam == damerau_table(ua, ub));
    check(lcs == lcs_table(ua, ub));
    // the cutoff
    size_t max = mode >> 1;
    txt::distance_options capped{.max = max, .bytes = bytes};
    check(d::run(M::levenshtein, a.view(), b.view(), capped) == std::min(lev, max + 1));
    check(d::run(M::damerau, a.view(), b.view(), capped) == std::min(dam, max + 1));
    txt::jaro_options j{.bytes = bytes};
    double jr = d::jaro_run(a.view(), b.view(), j, false), jw = d::jaro_run(a.view(), b.view(), j, true);
    check(jr >= 0 && jr <= 1 && jw >= jr && jw <= 1);
    return 0;
}
