//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The distances over pairs of texts:
//   distance <words|lines|long> <levenshtein|osa|damerau|lcs|jaro|table|...>
//   words: 10,000 pairs of words of 4 to 14 letters, one a misspelling of the other
//   lines: 1,000 pairs of lines of 100 to 200 characters, a few edits apart
//   long:  10 pairs of texts of 5,000 characters, a hundred edits apart
//   table, table_osa, table_damerau, table_lcs: the references, the tables
//   everyone writes (two rows; three for the alignment; Lowrance and Wagner's
//   whole table for Damerau's distance)
// Prints nanoseconds per pair.
#include "benchmarks/common.h"
#include "sgcl/txt.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    std::string word(std::mt19937& g, size_t n) {
        std::string w;
        for (size_t i = 0; i < n; ++i) {
            w.push_back(char('a' + g() % 26));
        }
        return w;
    }

    std::string edited(std::mt19937& g, std::string w, int edits) {
        for (int k = 0; k < edits; ++k) {
            size_t at = g() % (w.size() + 1);
            switch (g() % 4) {
                case 0:
                    w.insert(w.begin() + long(at), char('a' + g() % 26));
                    break;
                case 1:
                    if (at < w.size()) {
                        w.erase(w.begin() + long(at));
                    }
                    break;
                case 2:
                    if (at + 1 < w.size()) {
                        std::swap(w[at], w[at + 1]);
                    }
                    break;
                default:
                    if (at < w.size()) {
                        w[at] = char('a' + g() % 26);
                    }
            }
        }
        return w;
    }

    size_t table_osa(std::string_view a, std::string_view b) {
        std::vector<size_t> r2(b.size() + 1), r1(b.size() + 1), r(b.size() + 1);
        for (size_t j = 0; j <= b.size(); ++j) {
            r1[j] = j;
        }
        for (size_t i = 1; i <= a.size(); ++i) {
            r[0] = i;
            for (size_t j = 1; j <= b.size(); ++j) {
                r[j] = std::min({r1[j] + 1, r[j - 1] + 1, r1[j - 1] + (a[i - 1] != b[j - 1])});
                if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                    r[j] = std::min(r[j], r2[j - 2] + 1);
                }
            }
            std::swap(r2, r1);
            std::swap(r1, r);
        }
        return r1[b.size()];
    }

    size_t table_damerau(std::string_view a, std::string_view b) {
        size_t n = a.size(), m = b.size(), inf = n + m;
        std::vector<size_t> d((n + 2) * (m + 2));
        auto at = [&](size_t i, size_t j) -> size_t& { return d[i * (m + 2) + j]; };
        size_t da[256] = {};
        at(0, 0) = inf;
        for (size_t i = 0; i <= n; ++i) {
            at(i + 1, 0) = inf;
            at(i + 1, 1) = i;
        }
        for (size_t j = 0; j <= m; ++j) {
            at(0, j + 1) = inf;
            at(1, j + 1) = j;
        }
        for (size_t i = 1; i <= n; ++i) {
            size_t db = 0;
            for (size_t j = 1; j <= m; ++j) {
                size_t k = da[uint8_t(b[j - 1])], l = db, cost = 1;
                if (a[i - 1] == b[j - 1]) {
                    cost = 0;
                    db = j;
                }
                at(i + 1, j + 1) = std::min({at(i, j) + cost, at(i + 1, j) + 1, at(i, j + 1) + 1,
                                             at(k, l) + (i - k - 1) + 1 + (j - l - 1)});
            }
            da[uint8_t(a[i - 1])] = i;
        }
        return at(n + 1, m + 1);
    }

    size_t table_lcs(std::string_view a, std::string_view b) {
        std::vector<size_t> prev(b.size() + 1, 0), cur(b.size() + 1, 0);
        for (size_t i = 0; i < a.size(); ++i) {
            for (size_t j = 0; j < b.size(); ++j) {
                cur[j + 1] = a[i] == b[j] ? prev[j] + 1 : std::max(prev[j + 1], cur[j]);
            }
            std::swap(prev, cur);
        }
        return prev[b.size()];
    }

    size_t table(std::string_view a, std::string_view b) {
        std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
        for (size_t j = 0; j <= b.size(); ++j) {
            prev[j] = j;
        }
        for (size_t i = 1; i <= a.size(); ++i) {
            cur[0] = i;
            for (size_t j = 1; j <= b.size(); ++j) {
                cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1])});
            }
            std::swap(prev, cur);
        }
        return prev[b.size()];
    }
}

int main(int argc, char** argv) {
    const char* set = argc > 1 ? argv[1] : "words";
    const char* what = argc > 2 ? argv[2] : "levenshtein";
    if (!bench::has_variant(set, {"words", "lines", "long"}) ||
        !bench::has_variant(what, {"levenshtein", "osa", "damerau", "lcs", "jaro", "table", "table_osa",
                                   "table_damerau", "table_lcs"})) {
        std::fprintf(stderr, "usage: distance <words|lines|long> <levenshtein|osa|damerau|lcs|jaro|table|table_osa|"
                             "table_damerau|table_lcs>\n");
        return 2;
    }
    std::mt19937 g(2026);
    vector<string> as, bs;
    size_t pairs = !std::strcmp(set, "words") ? 10000 : !std::strcmp(set, "lines") ? 1000 : 10;
    for (size_t k = 0; k < pairs; ++k) {
        std::string a = !std::strcmp(set, "words") ? word(g, 4 + g() % 11)
                        : !std::strcmp(set, "lines") ? word(g, 100 + g() % 101)
                                                     : word(g, 5000);
        std::string b = edited(g, a, !std::strcmp(set, "words") ? 1 + int(g() % 2) : !std::strcmp(set, "lines") ? 5 : 100);
        as.push_back(string(std::string_view(a)));
        bs.push_back(string(std::string_view(b)));
    }
    size_t rounds = !std::strcmp(set, "long") ? 20 : 50;
    double sink = 0;
    auto t0 = bench::Clock::now();
    for (size_t r = 0; r < rounds; ++r) {
        for (size_t k = 0; k < pairs; ++k) {
            const string& a = as[k];
            const string& b = bs[k];
            if (!std::strcmp(what, "levenshtein")) {
                sink += double(txt::levenshtein(a, b));
            } else if (!std::strcmp(what, "osa")) {
                sink += double(txt::osa_distance(a, b));
            } else if (!std::strcmp(what, "damerau")) {
                sink += double(txt::damerau_levenshtein(a, b));
            } else if (!std::strcmp(what, "lcs")) {
                sink += double(txt::lcs_length(a, b));
            } else if (!std::strcmp(what, "jaro")) {
                sink += txt::jaro_winkler(a, b);
            } else if (!std::strcmp(what, "table_osa")) {
                sink += double(table_osa(a.view(), b.view()));
            } else if (!std::strcmp(what, "table_damerau")) {
                sink += double(table_damerau(a.view(), b.view()));
            } else if (!std::strcmp(what, "table_lcs")) {
                sink += double(table_lcs(a.view(), b.view()));
            } else {
                sink += double(table(a.view(), b.view()));
            }
        }
    }
    double s = bench::seconds_since(t0);
    std::printf("%s %s ns=%.1f (%d)\n", set, what, s * 1e9 / double(rounds * pairs), int(sink) % 7);
    return 0;
}
