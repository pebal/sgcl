//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The fast algorithms of big_integer: Karatsuba, Toom-3 and their squares,
// Burnikel–Ziegler, the conversions by divide and conquer. Two oracles:
// Python's int for long operands (fast_vectors.h, the operands made from
// seeds on both sides), and the schoolbook algorithms themselves, which
// every fast road must agree with when the thresholds are lowered to a
// few limbs so that each is taken on small numbers.
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "fast_vectors.h"

#include <random>
#include <string>

using math::big_integer;

namespace {
    constexpr uint64_t M64 = ~uint64_t(0);

    uint64_t splitmix(uint64_t& state) {
        state += 0x9e3779b97f4a7c15ull;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    // As tools/math_vectors.py makes them: n limbs from the seed, the top
    // one never zero; shape 0 random, 1 all ones, 2 random with an eighth
    // of the limbs all ones and an eighth zero
    big_integer operand(uint64_t seed, size_t n, int shape, bool negative = false) {
        std::vector<uint64_t> limbs(n);
        uint64_t s = seed;
        for (auto& x : limbs) {
            x = splitmix(s);
            if (shape == 1) {
                x = M64;
            } else if (shape == 2) {
                uint64_t y = splitmix(s);
                if (y % 8 == 0) {
                    x = M64;
                } else if (y % 8 == 1) {
                    x = 0;
                }
            }
        }
        if (!limbs.back()) {
            limbs.back() = 1;
        }
        std::vector<byte> bytes(n * 8);
        for (size_t i = 0; i < n; ++i) {
            for (size_t b = 0; b < 8; ++b) {
                bytes[n * 8 - 1 - (i * 8 + b)] = byte(limbs[i] >> (8 * b));
            }
        }
        big_integer v = big_integer::from_bytes(slice<const byte>(bytes.data(), bytes.data() + bytes.size()));
        return negative ? -v : v;
    }

    uint64_t fnv(const char* p, size_t n) {
        uint64_t h = 0xcbf29ce484222325ull;
        for (size_t i = 0; i < n; ++i) {
            h ^= uint8_t(p[i]);
            h *= 0x100000001b3ull;
        }
        return h;
    }

    uint64_t fnv(const string& s) {
        return fnv(s.data(), s.size());
    }

    std::string text(const big_integer& v, int base = 10) {
        auto s = v.to_string(base);
        return std::string(s.data(), s.size());
    }

    // The thresholds as a test sets them, put back when it ends
    struct Thresholds {
        math::detail::Thresholds saved = math::detail::thresholds;

        ~Thresholds() {
            math::detail::thresholds = saved;
        }

        // Every fast road closed: the schoolbook multiplication and
        // Knuth's division, the quadratic conversions
        static void schoolbook() {
            constexpr size_t Never = size_t(1) << 30;
            math::detail::thresholds = {Never, Never, Never, Never, Never, Never, Never};
        }
    };

    // A random operand of 1 to `limbs` limbs with a random sign, now and
    // then limbs of all ones or of zeros
    big_integer random_value(std::mt19937_64& rng, size_t limbs) {
        size_t n = size_t(rng() % limbs) + 1;
        return operand(rng(), n, int(rng() % 3), rng() & 1);
    }
}

TEST(Fast_Tests, ProductsAgainstPython) {
    for (auto& t : fast_vectors::products) {
        big_integer a = operand(t.sa, t.an, t.shape_a, t.na);
        big_integer b = operand(t.sb, t.bn, t.shape_b, t.nb);
        ASSERT_EQ(fnv((a * b).to_string(16)), t.product) << t.an << " x " << t.bn << " limbs, shapes " << t.shape_a << t.shape_b;
        ASSERT_EQ(fnv((b * a).to_string(16)), t.product) << t.bn << " x " << t.an << " limbs";
        ASSERT_EQ(fnv((a * a).to_string(16)), t.square) << t.an << " limbs squared";
    }
}

TEST(Fast_Tests, DivisionsAgainstPython) {
    for (auto& t : fast_vectors::divisions) {
        big_integer a = operand(t.sa, t.an, t.shape_a, t.na);
        big_integer b = operand(t.sb, t.bn, t.shape_b, t.nb);
        auto [q, r] = a.div_rem(b);
        ASSERT_EQ(fnv(q.to_string(16)), t.quotient) << t.an << " / " << t.bn << " limbs, shapes " << t.shape_a << t.shape_b;
        ASSERT_EQ(fnv(r.to_string(16)), t.remainder) << t.an << " % " << t.bn << " limbs";
        ASSERT_EQ(fnv((a / b).to_string(16)), t.quotient);
        ASSERT_EQ(fnv((a % b).to_string(16)), t.remainder);
    }
}

// Up to a number of 52 000 limbs, a million decimal digits: the
// conversion is no longer quadratic, and the number comes back whole
TEST(Fast_Tests, WrittenAgainstPython) {
    for (auto& t : fast_vectors::written) {
        big_integer x = operand(t.seed, t.n, t.shape, t.shape == 2);
        auto s = x.to_string(t.base);
        ASSERT_EQ(s.size(), t.length) << t.n << " limbs in base " << t.base;
        ASSERT_EQ(fnv(s), t.text) << t.n << " limbs in base " << t.base;
        std::string whole(s.data(), s.size());
        ASSERT_EQ(whole.substr(0, 20), t.head);
        ASSERT_EQ(whole.substr(whole.size() - std::min<size_t>(20, whole.size())), t.tail);
        ASSERT_EQ(*big_integer::parse(s, t.base), x) << t.n << " limbs in base " << t.base;
    }
}

TEST(Fast_Tests, ReadAgainstPython) {
    const char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    for (auto& t : fast_vectors::read) {
        std::string s(t.count, '0');
        uint64_t state = t.seed;
        for (size_t i = 0; i < t.count; ++i) {
            uint64_t d = splitmix(state) % uint64_t(t.base);
            s[i] = digits[i == 0 && d == 0 ? 1 : d];
        }
        auto v = big_integer::parse(string(s), t.base);
        ASSERT_TRUE(v.has_value());
        ASSERT_EQ(fnv(v->to_string(16)), t.value) << t.count << " digits of base " << t.base;
        // And the digits back, in capitals too
        ASSERT_EQ(text(*v, t.base), s);
    }
}

// Every road against the schoolbook ones, on operands of up to a few
// hundred limbs with the thresholds lowered: each configuration closes
// some roads and opens others, so that Karatsuba over the schoolbook,
// Toom-3 over Karatsuba, Toom-3 over Toom-3, the unbalanced split and
// the squares are each taken by some product, the recursive division
// at every depth, the conversions by divide and conquer down to one
// limb a part
TEST(Fast_Tests, EveryRoadAgreesWithTheSchoolbook) {
    Thresholds guard;
    std::mt19937_64 rng(20260925);
    struct Road {
        size_t karatsuba, toom3, square_karatsuba, square_toom3, burnikel_ziegler, to_string, parse;
    };
    const Road roads[] = {
        {2, 3, 2, 3, 4, 2, 2},
        {2, 1000, 2, 1000, 5, 3, 3},
        {4, 9, 5, 11, 6, 4, 2},
        {8, 24, 8, 20, 9, 7, 5},
        {3, 6, 3, 6, 12, 2, 7},
    };
    for (int i = 0; i < 400; ++i) {
        big_integer a = random_value(rng, i < 300 ? 60 : 300);
        big_integer b = random_value(rng, i < 300 ? 60 : 300);
        int base = i % 3 == 0 ? 10 : int(rng() % 35) + 2;
        Thresholds::schoolbook();
        big_integer product = a * b;
        big_integer square = a * a;
        auto [q, r] = (a * b + a).div_rem(b);
        auto [q2, r2] = a.div_rem(b);
        auto written = a.to_string(base);
        auto long_written = product.to_string(base);
        for (auto& road : roads) {
            math::detail::thresholds = {road.karatsuba, road.toom3, road.square_karatsuba, road.square_toom3,
                                        road.burnikel_ziegler, road.to_string, road.parse};
            ASSERT_EQ(a * b, product) << i << ": karatsuba " << road.karatsuba << " toom " << road.toom3;
            ASSERT_EQ(b * a, product) << i;
            ASSERT_EQ(a * a, square) << i << ": square karatsuba " << road.square_karatsuba;
            auto [fq, fr] = (a * b + a).div_rem(b);
            ASSERT_EQ(fq, q) << i << ": burnikel-ziegler " << road.burnikel_ziegler;
            ASSERT_EQ(fr, r) << i;
            auto [fq2, fr2] = a.div_rem(b);
            ASSERT_EQ(fq2, q2) << i;
            ASSERT_EQ(fr2, r2) << i;
            ASSERT_EQ(a.to_string(base), written) << i << " in base " << base;
            ASSERT_EQ(product.to_string(base), long_written) << i << " in base " << base;
            ASSERT_EQ(*big_integer::parse(long_written, base), product) << i << " in base " << base;
        }
    }
}

// The lengths on each threshold as it is set and one either side, at
// the thresholds the library ships with, against the schoolbook
TEST(Fast_Tests, AtEveryThreshold) {
    Thresholds guard;
    auto t = math::detail::thresholds;
    std::vector<size_t> lengths;
    for (size_t x : {t.karatsuba, t.toom3, t.square_karatsuba, t.square_toom3, t.burnikel_ziegler, t.to_string, t.parse}) {
        for (size_t d : {x - 1, x, x + 1, 2 * x - 1, 2 * x, 2 * x + 1, 3 * x}) {
            lengths.push_back(d);
        }
    }
    uint64_t seed = 77;
    for (size_t an : lengths) {
        for (size_t bn : lengths) {
            if (bn > an || (an > 3 * bn + 3 && an > 64)) {
                continue;
            }
            seed += 2;
            big_integer a = operand(seed, an, int(seed % 3), seed & 2);
            big_integer b = operand(seed + 1, bn, int((seed + 1) % 3), seed & 4);
            math::detail::thresholds = t;
            big_integer p = a * b;
            big_integer sq = a * a;
            auto [q, r] = (p + a).div_rem(b);
            auto s = p.to_string();
            auto back = big_integer::parse(s);
            Thresholds::schoolbook();
            ASSERT_EQ(p, a * b) << an << " x " << bn;
            ASSERT_EQ(sq, a * a) << an << " squared";
            auto [sq_, sr_] = (p + a).div_rem(b);
            ASSERT_EQ(q, sq_) << an + bn << " / " << bn;
            ASSERT_EQ(r, sr_) << an + bn << " % " << bn;
            ASSERT_EQ(s, p.to_string()) << an + bn << " limbs in decimal";
            ASSERT_EQ(*back, p);
        }
    }
}

// Quotients built to need the corrections of the recursive division:
// divisors whose low half is large (all ones), so that the quotient of
// the top halves overshoots, and dividends just below a multiple
TEST(Fast_Tests, RecursiveDivisionCorrections) {
    Thresholds guard;
    math::detail::thresholds.burnikel_ziegler = 4;
    for (size_t n : {8, 9, 16, 17, 33, 64, 100}) {
        big_integer ones = (big_integer(1) << (64 * n)) - 1;
        for (big_integer b : {ones, (big_integer(1) << (64 * n - 1)) + ((big_integer(1) << (32 * n)) - 1),
                              (big_integer(1) << (64 * n - 1)) + 1, ones >> 1}) {
            for (big_integer q : {ones, ones - 1, big_integer(1) << (64 * n - 3), big_integer(3)}) {
                for (big_integer r : {big_integer(0), b - 1, b / 2}) {
                    big_integer a = q * b + r;
                    auto [fq, fr] = a.div_rem(b);
                    ASSERT_EQ(fq, q) << n;
                    ASSERT_EQ(fr, r) << n;
                }
            }
        }
    }
}
