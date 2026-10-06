//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt's distances against the textbook tables: the pairs of
// tests/txt/distance_vectors.h (tools/distance_vectors.py) measured by the
// tables of the definitions; then the published examples, the cutoff, the
// units, the metric properties and the boundaries.
#include "tests/types.h"
#include "tests/txt/distance_vectors.h"

#include <cmath>
#include <random>
#include <string>

namespace {
    string s(const char* t) {
        return string(t);
    }

    std::string random_text(std::mt19937& g, size_t n, const char* alphabet) {
        std::string t;
        size_t k = std::char_traits<char>::length(alphabet);
        for (size_t i = 0; i < n; ++i) {
            t.push_back(alphabet[g() % k]);
        }
        return t;
    }
}

TEST(Distance_Tests, TableVectors) {
    size_t n = 0;
    for (const auto& v : DistanceVectors) {
        string a = s(v.a), b = s(v.b);
        txt::distance_options o{.bytes = v.bytes};
        EXPECT_EQ(txt::levenshtein(a, b, o), v.levenshtein) << v.a << " | " << v.b;
        EXPECT_EQ(txt::osa_distance(a, b, o), v.osa) << v.a << " | " << v.b;
        EXPECT_EQ(txt::damerau_levenshtein(a, b, o), v.damerau) << v.a << " | " << v.b;
        EXPECT_EQ(txt::lcs_length(a, b, o), v.lcs) << v.a << " | " << v.b;
        EXPECT_NEAR(txt::jaro(a, b, {.bytes = v.bytes}), v.jaro, 1e-12);
        EXPECT_NEAR(txt::jaro_winkler(a, b, {.bytes = v.bytes}), v.jaro_winkler, 1e-12);
        ++n;
    }
    EXPECT_EQ(n, 1500u);
}

TEST(Distance_Tests, PublishedExamples) {
    EXPECT_EQ(txt::levenshtein(s("kitten"), s("sitting")), 3u);
    EXPECT_EQ(txt::levenshtein(s("flaw"), s("lawn")), 2u);
    EXPECT_EQ(txt::osa_distance(s("CA"), s("ABC")), 3u);
    EXPECT_EQ(txt::damerau_levenshtein(s("CA"), s("ABC")), 2u);
    EXPECT_EQ(txt::osa_distance(s("ab"), s("ba")), 1u);
    EXPECT_EQ(txt::levenshtein(s("ab"), s("ba")), 2u);
    EXPECT_EQ(txt::lcs_length(s("ABCBDAB"), s("BDCABA")), 4u);
    EXPECT_NEAR(txt::jaro(s("MARTHA"), s("MARHTA")), 0.944444, 1e-6);
    EXPECT_NEAR(txt::jaro_winkler(s("MARTHA"), s("MARHTA")), 0.961111, 1e-6);
    EXPECT_NEAR(txt::jaro_winkler(s("DWAYNE"), s("DUANE")), 0.84, 1e-6);
    EXPECT_NEAR(txt::jaro_winkler(s("DIXON"), s("DICKSONX")), 0.813333, 1e-6);
}

TEST(Distance_Tests, Units) {
    // code points by default, bytes when asked
    EXPECT_EQ(txt::levenshtein(s("zażółć"), s("zazolc")), 4u);
    EXPECT_EQ(txt::levenshtein(s("zażółć"), s("zazolc"), {.bytes = true}), 8u);
    EXPECT_EQ(txt::lcs_length(s("日本語"), s("日本")), 2u);
    EXPECT_EQ(txt::lcs_length(s("日本語"), s("日本"), {.bytes = true}), 6u);
    // an ill-formed byte is a unit of its own, unlike U+FFFD and unlike another one
    EXPECT_EQ(txt::levenshtein(s("a\xFF"), s("a\xEF\xBF\xBD")), 1u);
    EXPECT_EQ(txt::levenshtein(s("\xFF"), s("\xFE")), 1u);
    EXPECT_EQ(txt::levenshtein(s("\xFF"), s("\xFF")), 0u);
    EXPECT_DOUBLE_EQ(txt::jaro(s("ą"), s("ą")), 1.0);
    EXPECT_DOUBLE_EQ(txt::jaro(s("ą"), s("a")), 0.0);
}

TEST(Distance_Tests, Cutoff) {
    EXPECT_EQ(txt::levenshtein(s("kitten"), s("sitting"), {.max = 3}), 3u);
    EXPECT_EQ(txt::levenshtein(s("kitten"), s("sitting"), {.max = 2}), 3u);
    EXPECT_EQ(txt::levenshtein(s("kitten"), s("sitting"), {.max = 1}), 2u);
    EXPECT_EQ(txt::levenshtein(s("a"), s("abcdef"), {.max = 2}), 3u);   // the lengths alone
    EXPECT_EQ(txt::levenshtein(s("abc"), s("abc"), {.max = 0}), 0u);
    EXPECT_EQ(txt::levenshtein(s("abc"), s("abd"), {.max = 0}), 1u);
    EXPECT_EQ(txt::damerau_levenshtein(s("CA"), s("ABC"), {.max = 1}), 2u);
    EXPECT_EQ(txt::osa_distance(s("CA"), s("ABC"), {.max = 1}), 2u);
    EXPECT_EQ(txt::lcs_length(s("ABCBDAB"), s("BDCABA"), {.max = 2}), 3u);
    // SIZE_MAX: no cutoff
    EXPECT_EQ(txt::levenshtein(s(""), s("abc"), {.max = SIZE_MAX}), 3u);
}

TEST(Distance_Tests, Empty) {
    EXPECT_EQ(txt::levenshtein(s(""), s("")), 0u);
    EXPECT_EQ(txt::levenshtein(s(""), s("abc")), 3u);
    EXPECT_EQ(txt::osa_distance(s("abc"), s("")), 3u);
    EXPECT_EQ(txt::damerau_levenshtein(s(""), s("ąę")), 2u);
    EXPECT_EQ(txt::lcs_length(s(""), s("abc")), 0u);
    EXPECT_DOUBLE_EQ(txt::jaro(s(""), s("")), 1.0);
    EXPECT_DOUBLE_EQ(txt::jaro(s(""), s("a")), 0.0);
    EXPECT_DOUBLE_EQ(txt::jaro_winkler(s("a"), s("")), 0.0);
    txt::distance_options defaults;
    EXPECT_EQ(defaults.max, SIZE_MAX);
    EXPECT_FALSE(defaults.bytes);
}

TEST(Distance_Tests, JaroOptions) {
    // no prefix bonus at or under the threshold
    double j = txt::jaro(s("abcxyz"), s("abczyx"));
    EXPECT_DOUBLE_EQ(txt::jaro_winkler(s("abcxyz"), s("abczyx"), {.threshold = 1.0}), j);
    EXPECT_NEAR(txt::jaro_winkler(s("abcxyz"), s("abczyx"), {.threshold = 0.0}), j + 3 * 0.1 * (1 - j), 1e-12);
    EXPECT_NEAR(txt::jaro_winkler(s("abcxyz"), s("abczyx"), {.max_prefix = 2, .threshold = 0.0}),
                j + 2 * 0.1 * (1 - j), 1e-12);
    EXPECT_NEAR(txt::jaro_winkler(s("abcxyz"), s("abczyx"), {.prefix_scale = 0.25, .threshold = 0.0}),
                j + 3 * 0.25 * (1 - j), 1e-12);
    EXPECT_DOUBLE_EQ(txt::jaro_winkler(s("same"), s("same")), 1.0);
}

TEST(Distance_Tests, MetricProperties) {
    std::mt19937 g(5);
    for (int round = 0; round < 300; ++round) {
        string a(random_text(g, g() % 90, "abc").c_str()), b(random_text(g, g() % 90, "abc").c_str()),
            c(random_text(g, g() % 90, "abc").c_str());
        size_t ab = txt::levenshtein(a, b), ba = txt::levenshtein(b, a);
        EXPECT_EQ(ab, ba);
        EXPECT_LE(txt::levenshtein(a, c), ab + txt::levenshtein(b, c));
        size_t dab = txt::damerau_levenshtein(a, b);
        EXPECT_EQ(dab, txt::damerau_levenshtein(b, a));
        EXPECT_LE(txt::damerau_levenshtein(a, c), dab + txt::damerau_levenshtein(b, c));
        // each measure below the one with fewer operations
        size_t osa = txt::osa_distance(a, b);
        EXPECT_LE(dab, osa);
        EXPECT_LE(osa, ab);
        size_t lcs = txt::lcs_length(a, b);
        EXPECT_LE(lcs, std::min(a.size(), b.size()));
        EXPECT_GE(ab, std::max(a.size(), b.size()) - lcs);
        double jw = txt::jaro_winkler(a, b);
        EXPECT_GE(jw, 0.0);
        EXPECT_LE(jw, 1.0);
    }
}

TEST(Distance_Tests, Long) {
    // texts of thousands of units: many words of bits, the full Damerau table in linear memory
    std::mt19937 g(9);
    std::string a = random_text(g, 3000, "acgt"), b = a;
    for (int k = 0; k < 40; ++k) {
        b[g() % b.size()] = "acgt"[g() % 4];
    }
    std::swap(b[100], b[101]);
    size_t lev = txt::levenshtein(string(a.c_str()), string(b.c_str()));
    EXPECT_LE(lev, 42u);
    EXPECT_GE(txt::lcs_length(string(a.c_str()), string(b.c_str())), 3000u - 42);
    EXPECT_LE(txt::damerau_levenshtein(string(a.c_str()), string(b.c_str())), lev);
    EXPECT_LE(txt::osa_distance(string(a.c_str()), string(b.c_str())), lev);
    std::string c = random_text(g, 5000, "ab");
    EXPECT_EQ(txt::levenshtein(string(c.c_str()), s("")), 5000u);
    EXPECT_EQ(txt::levenshtein(string(c.c_str()), string(c.c_str())), 0u);
}
