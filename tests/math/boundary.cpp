//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the module's types (DESIGN 408): zero and the values
// next to it, the edges of a limb, the arguments of the number theory at
// their limits, the errors of the program and of data, a value moved from,
// an argument that is the value itself.
#include "tests/types.h"
#include "sgcl/math/math.h"

#include <cfloat>
#include <climits>
#include <cmath>
#include <string>
#include <vector>

using math::big_integer;
using math::rational;

namespace {
    std::string dec(const big_integer& v) {
        auto s = v.to_string();
        return std::string(s.data(), s.size());
    }

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    big_integer two_to(int n) {
        return big_integer(1) << n;
    }

    // The values at the edges of the small form and of one and two limbs,
    // with their signs
    sgcl::vector<big_integer> edges() {
        sgcl::vector<big_integer> v;
        for (big_integer m : {big_integer(0), big_integer(1), big_integer(2), big_integer(INT64_MAX), two_to(63), two_to(63) + 1,
                              two_to(64) - 1, two_to(64), two_to(64) + 1, two_to(127) - 1, two_to(127), two_to(128) - 1, two_to(128),
                              two_to(128) + 1, two_to(192) - 1}) {
            v.push_back(m);
            if (m.sign()) {
                v.push_back(-m);
            }
        }
        return v;
    }

    bool fits(const big_integer& v, __int128& out) {
        if (v.bit_length() > 126) {
            return false;
        }
        auto lo = (v.abs() & big_integer(UINT64_MAX)).to_uint64();
        auto hi = (v.abs() >> 64).to_uint64();
        unsigned __int128 m = ((unsigned __int128)*hi << 64) | *lo;
        out = v.sign() < 0 ? -(__int128)m : (__int128)m;
        return true;
    }

    big_integer of(__int128 v) {
        return big_integer(v);
    }
}

//----------------------------------------------------------------------------
// big_integer
//----------------------------------------------------------------------------

TEST(BigIntBoundary_Tests, ZeroAndAValueMovedFrom) {
    big_integer large = two_to(200) + 12345;
    big_integer moved = large;
    big_integer to = std::move(moved);
    EXPECT_EQ(to, large);
    auto check = [](const big_integer& v) {
        EXPECT_EQ(v, 0);
        EXPECT_EQ(v.sign(), 0);
        EXPECT_EQ(v.bit_length(), 0u);
        EXPECT_EQ(v.trailing_zeros(), 0u);
        EXPECT_FALSE(v.bit(0));
        EXPECT_FALSE(v.bit(SIZE_MAX));
        EXPECT_EQ(dec(v), "0");
        EXPECT_EQ(text(v.to_string(2)), "0");
        EXPECT_EQ(text(v.to_string(36)), "0");
        EXPECT_TRUE(v.to_bytes().empty());
        EXPECT_TRUE(v.to_bytes(0).empty());
        EXPECT_EQ(v.to_bytes(3).size(), 3u);
        EXPECT_EQ(v.to_int64(), 0);
        EXPECT_EQ(v.to_uint64(), 0u);
        EXPECT_EQ(v.to_double(), 0.0);
        EXPECT_FALSE(std::signbit(v.to_double()));
        EXPECT_EQ(v.abs(), 0);
        EXPECT_EQ(-v, 0);
        EXPECT_EQ(~v, -1);
        EXPECT_EQ(v.pow(0), 1);
        EXPECT_EQ(v.pow(INT64_MAX), 0);
        EXPECT_EQ(v.sqrt(), 0);
        EXPECT_EQ(v.gcd(0), 0);
        EXPECT_EQ(v.gcd(-7), 7);
        EXPECT_EQ(v.lcm(7), 0);
        EXPECT_EQ(v.mod(7), 0);
        EXPECT_EQ(v.mod_pow(0, 7), 1);
        EXPECT_EQ(v.mod_pow(5, 7), 0);
        EXPECT_EQ(v.mod_inverse(7), nullopt);
        EXPECT_FALSE(v.is_probable_prime());
        EXPECT_EQ(v << 1000, 0);
        EXPECT_EQ(v >> 1000, 0);
        EXPECT_EQ(std::hash<big_integer>()(v), std::hash<big_integer>()(big_integer(0)));
        EXPECT_THROW((void)(big_integer(5) / v), domain_error);
        EXPECT_EQ(v / 5, 0);
        EXPECT_EQ(v % 5, 0);
    };
    check(big_integer());
    check(moved);
    moved = 7;   // a value moved from takes a new one
    EXPECT_EQ(moved, 7);
    moved += large;
    EXPECT_EQ(moved, large + 7);
}

// -0 in every form a number can be written in is zero, with no sign
TEST(BigIntBoundary_Tests, NegativeZero) {
    EXPECT_EQ(big_integer(-0.0), 0);
    EXPECT_EQ(big_integer(-0.0).sign(), 0);
    EXPECT_EQ(big_integer(-0.4), 0);
    EXPECT_EQ(big_integer(-0.99999), 0);
    EXPECT_EQ(big_integer::parse("-0"), 0);
    EXPECT_EQ(big_integer::parse("-0000"), 0);
    EXPECT_EQ(big_integer::parse("+0"), 0);
    EXPECT_EQ(big_integer::parse("-0", 2), 0);
    EXPECT_EQ(dec(*big_integer::parse("-0")), "0");
    big_integer z = big_integer(5) - 5;
    EXPECT_EQ(dec(-z), "0");
    EXPECT_EQ((-z).sign(), 0);
    EXPECT_EQ(big_integer(-5) % 5, 0);
    EXPECT_EQ((big_integer(-5) % 5).sign(), 0);
    EXPECT_EQ((-two_to(100)) % two_to(50), 0);
    EXPECT_EQ(((-two_to(100)) % two_to(50)).sign(), 0);
    EXPECT_EQ(dec((-two_to(100)) % two_to(50)), "0");
    EXPECT_EQ(big_integer(-1) >> 0, -1);
    EXPECT_EQ(dec(-two_to(100) * 0), "0");
    EXPECT_EQ(big_integer::from_bytes(slice<const byte>()), 0);
}

// Every pair of the edges in every operation of two values, against the
// processor's 128 bits where the result fits them, and against the
// identities of the division where it does not
TEST(BigIntBoundary_Tests, TheEdgesOfALimbInEveryOperation) {
    auto all = edges();
    for (const big_integer& a : all) {
        for (const big_integer& b : all) {
            __int128 x;
            __int128 y;
            bool small = fits(a, x) && fits(b, y);
            if (small) {
                __int128 r;
                if (!__builtin_add_overflow(x, y, &r)) {
                    ASSERT_EQ(a + b, of(r)) << dec(a) << " + " << dec(b);
                }
                if (!__builtin_sub_overflow(x, y, &r)) {
                    ASSERT_EQ(a - b, of(r)) << dec(a) << " - " << dec(b);
                }
                if (!__builtin_mul_overflow(x, y, &r)) {
                    ASSERT_EQ(a * b, of(r)) << dec(a) << " * " << dec(b);
                }
                ASSERT_EQ(a & b, of(x & y)) << dec(a) << " & " << dec(b);
                ASSERT_EQ(a | b, of(x | y)) << dec(a) << " | " << dec(b);
                ASSERT_EQ(a ^ b, of(x ^ y)) << dec(a) << " ^ " << dec(b);
                if (y != 0) {
                    ASSERT_EQ(a / b, of(x / y)) << dec(a) << " / " << dec(b);
                    ASSERT_EQ(a % b, of(x % y)) << dec(a) << " % " << dec(b);
                }
                ASSERT_EQ(a <=> b, x <=> y) << dec(a) << " <=> " << dec(b);
                ASSERT_EQ(a == b, x == y);
            }
            ASSERT_EQ(a + b - b, a);
            ASSERT_EQ((a - b) + b, a);
            if (b.sign()) {
                auto [q, r] = a.div_rem(b);
                ASSERT_EQ(q * b + r, a) << dec(a) << " / " << dec(b);
                ASSERT_LT(r.abs(), b.abs());
                ASSERT_TRUE(r.sign() == 0 || r.sign() == a.sign());
                ASSERT_EQ(q, a / b);
                ASSERT_EQ(r, a % b);
                ASSERT_EQ((a * b) / b, a);
                big_integer m = a.mod(b);
                ASSERT_GE(m.sign(), 0);
                ASSERT_LT(m, b.abs());
                ASSERT_EQ((a - m) % b, 0);
            }
            ASSERT_EQ(a.gcd(b), b.gcd(a));
            ASSERT_GE(a.gcd(b).sign(), 0);
            if (a.sign() && b.sign()) {
                ASSERT_EQ(a % a.gcd(b), 0);
                ASSERT_EQ(b % a.gcd(b), 0);
                ASSERT_EQ(a.lcm(b) * a.gcd(b), (a * b).abs());
            }
            ASSERT_EQ(~(a & b), ~a | ~b);
            ASSERT_EQ(a ^ b ^ b, a);
        }
        // The unary operations and the shifts across the edges of a limb
        ASSERT_EQ(-(-a), a);
        ASSERT_EQ(~a, -a - 1);
        ASSERT_EQ(a.abs(), a.sign() < 0 ? -a : a);
        big_integer inc = a;
        ++inc;
        ASSERT_EQ(inc, a + 1);
        --inc;
        ASSERT_EQ(inc, a);
        for (int s : {0, 1, 62, 63, 64, 65, 127, 128, 129}) {
            ASSERT_EQ(a << s, a * two_to(s)) << dec(a) << " << " << s;
            big_integer down = a >> s;
            ASSERT_LE(down * two_to(s), a) << dec(a) << " >> " << s;
            ASSERT_GT((down + 1) * two_to(s), a) << dec(a) << " >> " << s;
        }
        ASSERT_EQ(dec(*big_integer::parse(a.to_string())), dec(a));
        for (int base : {2, 3, 7, 10, 16, 35, 36}) {
            ASSERT_EQ(big_integer::parse(a.to_string(base), base), a) << dec(a) << " in base " << base;
        }
        ASSERT_EQ(big_integer::from_bytes(a.to_bytes()), a.abs());
    }
}

TEST(BigIntBoundary_Tests, DivisionByZero) {
    for (big_integer a : {big_integer(0), big_integer(1), big_integer(-1), big_integer(INT64_MIN), two_to(64), -two_to(200)}) {
        for (big_integer zero : {big_integer(0), big_integer(-0.0), two_to(64) - two_to(64)}) {
            EXPECT_THROW((void)(a / zero), domain_error);
            EXPECT_THROW((void)(a % zero), domain_error);
            EXPECT_THROW((void)a.div_rem(zero), domain_error);
            EXPECT_THROW((void)a.mod(zero), domain_error);
            EXPECT_THROW((void)a.mod_inverse(zero), domain_error);
            EXPECT_THROW((void)a.mod_pow(1, zero), domain_error);
            big_integer x = a;
            EXPECT_THROW(x /= zero, domain_error);
            EXPECT_EQ(x, a);   // the value as it was
            EXPECT_THROW(x %= zero, domain_error);
            EXPECT_EQ(x, a);
        }
    }
}

TEST(BigIntBoundary_Tests, PowAtItsLimits) {
    EXPECT_EQ(big_integer(1).pow(INT64_MAX), 1);
    EXPECT_EQ(big_integer(-1).pow(INT64_MAX), -1);
    EXPECT_EQ(big_integer(-1).pow(INT64_MAX - 1), 1);
    EXPECT_EQ(big_integer(-1).pow(0), 1);
    EXPECT_EQ(big_integer(2).pow(62), INT64_MAX / 2 + 1);
    EXPECT_EQ(big_integer(2).pow(63), two_to(63));
    EXPECT_EQ(big_integer(-2).pow(63), INT64_MIN);
    EXPECT_EQ(big_integer(-2).pow(63).to_int64(), INT64_MIN);   // in the small form
    EXPECT_EQ(big_integer(2).pow(64), two_to(64));
    EXPECT_EQ(big_integer(3).pow(40), big_integer(12157665459056928801ull));
    EXPECT_EQ(big_integer(3).pow(41), big_integer(12157665459056928801ull) * 3);
    EXPECT_EQ(two_to(64).pow(2), two_to(128));
    EXPECT_EQ((two_to(64) - 1).pow(2), two_to(128) - two_to(65) + 1);
    EXPECT_THROW((void)big_integer(2).pow(-1), domain_error);
    EXPECT_THROW((void)big_integer(0).pow(INT64_MIN), domain_error);
    EXPECT_THROW((void)big_integer(2).pow(INT64_MAX), length_error);
    EXPECT_THROW((void)big_integer(-3).pow(INT64_MAX), length_error);
    EXPECT_THROW((void)two_to(64).pow(int64_t(1) << 46), length_error);
    EXPECT_THROW((void)big_integer(2).pow(int64_t(1) << 52), length_error);
}

TEST(BigIntBoundary_Tests, SqrtAtItsLimits) {
    EXPECT_THROW((void)big_integer(-1).sqrt(), domain_error);
    EXPECT_THROW((void)(-two_to(200)).sqrt(), domain_error);
    sgcl::vector<big_integer> squares;
    for (int bits : {1, 31, 32, 52, 53, 54, 62, 63, 64, 65, 100, 125, 126, 127, 128, 129, 200, 251, 252, 253}) {
        squares.push_back(two_to(bits));
        squares.push_back(two_to(bits) - 1);
        squares.push_back(two_to(bits) + 1);
    }
    for (big_integer root : {two_to(63) - 1, two_to(63), two_to(64) - 1, two_to(64), two_to(126) - 1, two_to(126)}) {
        squares.push_back(root * root);
        squares.push_back(root * root - 1);
        squares.push_back(root * root + 2 * root);   // (root + 1)^2 - 1
    }
    squares.push_back(0);
    squares.push_back(1);
    squares.push_back(INT64_MAX);
    for (const big_integer& n : squares) {
        big_integer s = n.sqrt();
        ASSERT_LE(s * s, n) << dec(n);
        ASSERT_GT((s + 1) * (s + 1), n) << dec(n);
    }
}

TEST(BigIntBoundary_Tests, GcdAndLcmAtTheEdges) {
    big_integer min = INT64_MIN;
    EXPECT_EQ(min.gcd(0), two_to(63));
    EXPECT_EQ(big_integer(0).gcd(min), two_to(63));
    EXPECT_EQ(min.gcd(min), two_to(63));
    EXPECT_EQ(min.gcd(two_to(64)), two_to(63));
    EXPECT_EQ(min.gcd(3), 1);
    EXPECT_EQ(big_integer(-12).gcd(-18), 6);
    EXPECT_EQ(two_to(128).gcd(two_to(64) + 1), 1);
    EXPECT_EQ((two_to(128) - 1).gcd(two_to(64) - 1), two_to(64) - 1);   // 2^128 - 1 = (2^64 - 1)(2^64 + 1)
    EXPECT_EQ((two_to(64) + 1).gcd(-(two_to(128) - 1)), two_to(64) + 1);
    EXPECT_EQ(big_integer(1).gcd(-two_to(500)), 1);
    big_integer x = two_to(300) * 3;
    EXPECT_EQ(x.gcd(x), x);
    EXPECT_EQ(x.gcd(-x), x);
    EXPECT_EQ(min.lcm(3), two_to(63) * 3);
    EXPECT_EQ(min.lcm(min), two_to(63));
    EXPECT_EQ(big_integer(-4).lcm(6), 12);
    EXPECT_EQ(big_integer(0).lcm(0), 0);
    EXPECT_EQ(x.lcm(0), 0);
    EXPECT_EQ(x.lcm(-x), x);
}

TEST(BigIntBoundary_Tests, ModInverseAtItsLimits) {
    EXPECT_EQ(big_integer(5).mod_inverse(1), 0);
    EXPECT_EQ(big_integer(5).mod_inverse(-1), 0);
    EXPECT_EQ(big_integer(0).mod_inverse(1), 0);
    EXPECT_EQ(big_integer(1).mod_inverse(2), 1);
    EXPECT_EQ(big_integer(-1).mod_inverse(7), 6);
    EXPECT_EQ(big_integer(3).mod_inverse(-7), 5);   // modulo |m|
    EXPECT_EQ(big_integer(7).mod_inverse(7), nullopt);
    EXPECT_EQ(big_integer(14).mod_inverse(-7), nullopt);
    EXPECT_EQ(big_integer(2).mod_inverse(4), nullopt);
    EXPECT_EQ(big_integer(INT64_MIN).mod_inverse(3), big_integer(INT64_MIN).mod(3));   // its own inverse: ±1
    for (big_integer m : {two_to(63), two_to(64) - 1, two_to(64), two_to(64) + 1, two_to(127) - 1, two_to(128) + 1, two_to(521) - 1}) {
        for (big_integer a : {big_integer(-1), big_integer(1), big_integer(3), m - 1, m + 1, -m + 1, two_to(200) + 1, m * 5 + 3}) {
            auto inv = a.mod_inverse(m);
            if (a.gcd(m) != 1) {
                ASSERT_EQ(inv, nullopt) << dec(a) << " mod " << dec(m);
                continue;
            }
            ASSERT_TRUE(inv) << dec(a) << " mod " << dec(m);
            ASSERT_GE(inv->sign(), 0);
            ASSERT_LT(*inv, m);
            ASSERT_EQ((a * *inv).mod(m), m == 1 ? 0 : 1) << dec(a) << " mod " << dec(m);
        }
    }
    EXPECT_EQ(two_to(64).mod_inverse(two_to(64)), nullopt);
    EXPECT_EQ(two_to(128).mod_inverse(two_to(64)), nullopt);   // a multiple of m
}

TEST(BigIntBoundary_Tests, ModPowAtItsLimits) {
    EXPECT_THROW((void)big_integer(2).mod_pow(1, 0), domain_error);
    EXPECT_THROW((void)big_integer(2).mod_pow(1, -5), domain_error);
    EXPECT_THROW((void)big_integer(2).mod_pow(-1, 5), domain_error);
    EXPECT_THROW((void)big_integer(2).mod_pow(-two_to(100), 5), domain_error);
    EXPECT_EQ(big_integer(5).mod_pow(0, 1), 0);   // in [0, 1)
    EXPECT_EQ(big_integer(0).mod_pow(0, 1), 0);
    EXPECT_EQ(big_integer(0).mod_pow(0, 7), 1);
    EXPECT_EQ(big_integer(0).mod_pow(two_to(100), 7), 0);
    EXPECT_EQ(big_integer(-1).mod_pow(two_to(100) + 1, 7), 6);
    EXPECT_EQ(big_integer(-1).mod_pow(two_to(100), 7), 1);
    EXPECT_EQ(big_integer(7).mod_pow(5, 7), 0);
    EXPECT_EQ(big_integer(8).mod_pow(two_to(100), 7), 1);
    EXPECT_EQ(big_integer(3).mod_pow(1, 2), 1);
    // The odd and the even roads, at the edges of a limb, against the power
    // and a remainder
    for (big_integer m : {big_integer(2), big_integer(3), two_to(63), two_to(63) + 1, two_to(64) - 1, two_to(64), two_to(64) + 1,
                          two_to(128) - 1, two_to(128), two_to(128) + 1}) {
        for (big_integer b : {big_integer(-1), big_integer(2), m - 1, m + 1, -m - 1, two_to(65) + 3}) {
            for (int64_t e : {1, 2, 3, 17, 64}) {
                ASSERT_EQ(b.mod_pow(e, m), b.pow(e).mod(m)) << dec(b) << "^" << e << " mod " << dec(m);
            }
        }
    }
    // Fermat, with an exponent of many limbs: a^(p-1) = 1 modulo a prime
    for (big_integer p : {two_to(61) - 1, two_to(89) - 1, two_to(127) - 1, two_to(521) - 1}) {
        EXPECT_EQ(big_integer(3).mod_pow(p - 1, p), 1) << dec(p);
        EXPECT_EQ((p - 1).mod_pow(p - 1, p), 1);
        EXPECT_EQ(p.mod_pow(p - 1, p), 0);
    }
}

TEST(BigIntBoundary_Tests, PrimesAtTheEdges) {
    EXPECT_THROW((void)big_integer(7).is_probable_prime(-1), domain_error);
    EXPECT_THROW((void)big_integer(7).is_probable_prime(INT_MIN), domain_error);
    EXPECT_TRUE(big_integer(7).is_probable_prime(0));
    EXPECT_FALSE(big_integer(-7).is_probable_prime());
    EXPECT_FALSE(big_integer(INT64_MIN).is_probable_prime());
    EXPECT_FALSE(big_integer(1).is_probable_prime());
    EXPECT_TRUE(big_integer(2).is_probable_prime());
    EXPECT_TRUE(big_integer(61).is_probable_prime());
    EXPECT_FALSE(big_integer(63).is_probable_prime());
    EXPECT_FALSE(big_integer(64).is_probable_prime());
    EXPECT_TRUE(big_integer(67).is_probable_prime());
    EXPECT_TRUE(big_integer(211).is_probable_prime());
    EXPECT_TRUE(big_integer(223).is_probable_prime());
    EXPECT_FALSE(big_integer(223 * 223).is_probable_prime());
    EXPECT_TRUE((two_to(61) - 1).is_probable_prime());
    EXPECT_TRUE(big_integer(18446744073709551557ull).is_probable_prime());   // the largest prime below 2^64
    EXPECT_FALSE((two_to(64) - 1).is_probable_prime());
    EXPECT_FALSE(two_to(64).is_probable_prime());
    EXPECT_TRUE((two_to(89) - 1).is_probable_prime());
    EXPECT_FALSE(((two_to(61) - 1) * (two_to(61) - 1)).is_probable_prime());   // a square: no Lucas test
    EXPECT_FALSE(((two_to(89) - 1) * (two_to(61) - 1)).is_probable_prime(0));
    EXPECT_TRUE((two_to(127) - 1).is_probable_prime(INT_MAX / (1 << 20)));
}

TEST(BigIntBoundary_Tests, FactorialAndBinomialAtTheirLimits) {
    EXPECT_THROW((void)big_integer::factorial(-1), domain_error);
    EXPECT_THROW((void)big_integer::factorial(INT64_MIN), domain_error);
    EXPECT_EQ(big_integer::factorial(0), 1);
    EXPECT_EQ(big_integer::factorial(1), 1);
    EXPECT_EQ(big_integer::factorial(2), 2);
    EXPECT_EQ(big_integer::factorial(20), 2432902008176640000);
    EXPECT_EQ(big_integer::factorial(21), big_integer(2432902008176640000) * 21);
    // A result past the longest number is refused before anything is
    // computed, as pow refuses one
    EXPECT_THROW((void)big_integer::factorial(INT64_MAX), length_error);
    EXPECT_THROW((void)big_integer::factorial(int64_t(1) << 50), length_error);
    EXPECT_THROW((void)big_integer::binomial(-1, 0), domain_error);
    EXPECT_THROW((void)big_integer::binomial(0, -1), domain_error);
    EXPECT_THROW((void)big_integer::binomial(INT64_MIN, INT64_MIN), domain_error);
    EXPECT_EQ(big_integer::binomial(0, 0), 1);
    EXPECT_EQ(big_integer::binomial(0, 1), 0);
    EXPECT_EQ(big_integer::binomial(5, 6), 0);
    EXPECT_EQ(big_integer::binomial(INT64_MAX - 1, INT64_MAX), 0);
    EXPECT_EQ(big_integer::binomial(INT64_MAX, 0), 1);
    EXPECT_EQ(big_integer::binomial(INT64_MAX, INT64_MAX), 1);
    EXPECT_EQ(big_integer::binomial(INT64_MAX, 1), INT64_MAX);
    EXPECT_EQ(big_integer::binomial(INT64_MAX, INT64_MAX - 1), INT64_MAX);
    EXPECT_EQ(big_integer::binomial(INT64_MAX, 2), big_integer(INT64_MAX) * (INT64_MAX - 1) / 2);
    EXPECT_EQ(big_integer::binomial(INT64_MAX, INT64_MAX - 2), big_integer(INT64_MAX) * (INT64_MAX - 1) / 2);
    EXPECT_THROW((void)big_integer::binomial(INT64_MAX, INT64_MAX / 2), length_error);
    EXPECT_THROW((void)big_integer::binomial(int64_t(1) << 60, int64_t(1) << 50), length_error);
}

TEST(BigIntBoundary_Tests, ShiftsAndBitsAtTheirLimits) {
    EXPECT_EQ(big_integer(-1) >> SIZE_MAX, -1);
    EXPECT_EQ(big_integer(1) >> SIZE_MAX, 0);
    EXPECT_EQ(-two_to(200) >> SIZE_MAX, -1);
    EXPECT_EQ(two_to(200) >> 200, 1);
    EXPECT_EQ(two_to(200) >> 201, 0);
    EXPECT_EQ(-two_to(200) >> 200, -1);
    EXPECT_EQ(-two_to(200) >> 201, -1);
    EXPECT_EQ((-two_to(200) - 1) >> 200, -2);
    EXPECT_EQ(big_integer(INT64_MIN) >> 63, -1);
    EXPECT_EQ(big_integer(INT64_MIN) >> 62, -2);
    EXPECT_EQ(big_integer(INT64_MIN) << 1, -two_to(64));
    EXPECT_EQ(big_integer(INT64_MAX) << 1, two_to(64) - 2);
    EXPECT_EQ(big_integer(0) << SIZE_MAX, 0);
    EXPECT_EQ(big_integer(5) << 0u, 5);
    EXPECT_THROW((void)(big_integer(1) << SIZE_MAX), length_error);
    EXPECT_THROW((void)(big_integer(1) << (uint64_t(64) << 46)), length_error);
    EXPECT_THROW((void)(big_integer(1) << -1), domain_error);
    EXPECT_THROW((void)(big_integer(1) >> INT64_MIN), domain_error);
    EXPECT_THROW((void)(big_integer(0) << -1), domain_error);
    big_integer x = 3;
    EXPECT_THROW(x <<= -1, domain_error);
    EXPECT_EQ(x, 3);
    EXPECT_THROW(x <<= SIZE_MAX, length_error);
    EXPECT_EQ(x, 3);
    // Bits of the two's complement past the length
    EXPECT_TRUE(big_integer(-1).bit(SIZE_MAX));
    EXPECT_FALSE(big_integer(INT64_MAX).bit(63));
    EXPECT_TRUE(big_integer(INT64_MIN).bit(63));
    EXPECT_TRUE(big_integer(INT64_MIN).bit(SIZE_MAX));
    EXPECT_FALSE(big_integer(INT64_MIN).bit(62));
    EXPECT_TRUE(two_to(63).bit(63));
    EXPECT_FALSE(two_to(63).bit(64));
    EXPECT_FALSE(two_to(63).bit(SIZE_MAX));
    EXPECT_TRUE((-two_to(64)).bit(64));
    EXPECT_FALSE((-two_to(64)).bit(63));
    EXPECT_TRUE((-two_to(64)).bit(65));
    EXPECT_TRUE((-two_to(64)).bit(SIZE_MAX));
    EXPECT_EQ(two_to(64).trailing_zeros(), 64u);
    EXPECT_EQ((-two_to(64)).trailing_zeros(), 64u);
    EXPECT_EQ(big_integer(INT64_MIN).trailing_zeros(), 63u);
    EXPECT_EQ(big_integer(INT64_MIN).bit_length(), 64u);
    EXPECT_EQ((two_to(64) - 1).bit_length(), 64u);
    EXPECT_EQ(two_to(64).bit_length(), 65u);
}

TEST(BigIntBoundary_Tests, TextAndBytesAtTheirLimits) {
    EXPECT_THROW((void)big_integer::parse("1", 1), invalid_argument);
    EXPECT_THROW((void)big_integer::parse("1", 37), invalid_argument);
    EXPECT_THROW((void)big_integer::parse("1", INT_MIN), invalid_argument);
    EXPECT_THROW((void)big_integer(5).to_string(1), invalid_argument);
    EXPECT_THROW((void)big_integer(5).to_string(37), invalid_argument);
    EXPECT_THROW((void)big_integer(5).to_string(0), invalid_argument);
    EXPECT_EQ(big_integer::parse("zZ", 36), 36 * 35 + 35);
    EXPECT_FALSE(big_integer::parse("z", 35));
    EXPECT_FALSE(big_integer::parse("2", 2));
    EXPECT_EQ(big_integer::parse("1", 2), 1);
    auto offset = [](const char* s, int base = 10) {
        auto v = big_integer::parse(s, base);
        EXPECT_FALSE(v) << s;
        return v ? SIZE_MAX : v.error().offset();
    };
    EXPECT_EQ(offset(""), 0u);
    EXPECT_EQ(offset("+"), 1u);
    EXPECT_EQ(offset("-"), 1u);
    EXPECT_EQ(offset("+-1"), 1u);
    EXPECT_EQ(offset("--1"), 1u);
    EXPECT_EQ(offset(" 1"), 0u);
    EXPECT_EQ(offset("1 "), 1u);
    EXPECT_EQ(offset("0x10"), 1u);
    EXPECT_EQ(offset("1_000"), 1u);
    EXPECT_EQ(offset("12\xff"), 2u);
    auto nul = big_integer::parse(sgcl::string(std::string("1\0" "2", 3)));
    ASSERT_FALSE(nul);
    EXPECT_EQ(nul.error().offset(), 1u);
    // The edges of the small form and of a limb, in decimal and in base 16
    EXPECT_EQ(big_integer::parse("9223372036854775807")->to_int64(), INT64_MAX);
    EXPECT_EQ(big_integer::parse("9223372036854775808")->to_int64(), nullopt);
    EXPECT_EQ(big_integer::parse("-9223372036854775808")->to_int64(), INT64_MIN);
    EXPECT_EQ(big_integer::parse("-9223372036854775809")->to_int64(), nullopt);
    EXPECT_EQ(big_integer::parse("18446744073709551615"), two_to(64) - 1);
    EXPECT_EQ(big_integer::parse("18446744073709551616"), two_to(64));
    EXPECT_EQ(big_integer::parse("ffffffffffffffff", 16), two_to(64) - 1);
    EXPECT_EQ(big_integer::parse("10000000000000000", 16), two_to(64));
    EXPECT_EQ(big_integer::parse("-8000000000000000", 16), INT64_MIN);
    EXPECT_EQ(big_integer::parse("9999999999999999999"), big_integer(9999999999999999999ull));
    EXPECT_EQ(big_integer::parse(sgcl::string(std::string(10000, '0') + "1")), 1);
    EXPECT_EQ(big_integer::parse(sgcl::string("-" + std::string(10000, '0'))), 0);
    EXPECT_EQ(big_integer::parse(sgcl::string("1" + std::string(64, '0')), 2), two_to(64));
    EXPECT_EQ(text(big_integer(INT64_MIN).to_string(2)), "-1" + std::string(63, '0'));
    EXPECT_EQ(text(big_integer(INT64_MIN).to_string(16)), "-8000000000000000");
    EXPECT_EQ(text((two_to(64) - 1).to_string(36)), "3w5e11264sgsf");
    EXPECT_EQ(text(two_to(64).to_string(16)), "10000000000000000");
    // Bytes
    EXPECT_EQ(big_integer::from_bytes(slice<const byte>()), 0);
    auto bytes = [](const std::vector<byte>& v) { return slice<const byte>(v.data(), v.size()); };
    std::vector<byte> zeros(100, byte{0});
    EXPECT_EQ(big_integer::from_bytes(bytes(zeros)), 0);
    zeros.push_back(byte{1});
    EXPECT_EQ(big_integer::from_bytes(bytes(zeros)), 1);
    std::vector<byte> ones(8, byte{0xff});
    EXPECT_EQ(big_integer::from_bytes(bytes(ones)), two_to(64) - 1);
    ones.insert(ones.begin(), byte{1});
    EXPECT_EQ(big_integer::from_bytes(bytes(ones)), two_to(65) - 1);
    EXPECT_EQ((two_to(64) - 1).to_bytes().size(), 8u);
    EXPECT_EQ(two_to(64).to_bytes().size(), 9u);
    EXPECT_EQ(big_integer(-255).to_bytes().size(), 1u);
    EXPECT_EQ(big_integer(256).to_bytes(2).size(), 2u);
    EXPECT_THROW((void)big_integer(256).to_bytes(1), length_error);
    EXPECT_THROW((void)big_integer(1).to_bytes(0), length_error);
    EXPECT_EQ(big_integer(INT64_MIN).to_bytes().size(), 8u);
}

TEST(BigIntBoundary_Tests, DoublesAtTheirLimits) {
    EXPECT_THROW((void)big_integer(NAN), domain_error);
    EXPECT_THROW((void)big_integer(INFINITY), domain_error);
    EXPECT_THROW((void)big_integer(-INFINITY), domain_error);
    EXPECT_EQ(big_integer(0x1p63), two_to(63));
    EXPECT_EQ(big_integer(-0x1p63), INT64_MIN);
    EXPECT_EQ(big_integer(-0x1p63).to_int64(), INT64_MIN);
    EXPECT_EQ(big_integer(0x1p64), two_to(64));
    EXPECT_EQ(big_integer(DBL_MAX), (two_to(53) - 1) << 971);
    EXPECT_EQ(big_integer(-DBL_MAX), -((two_to(53) - 1) << 971));
    EXPECT_EQ(big_integer(DBL_MIN), 0);
    EXPECT_EQ(big_integer(DBL_TRUE_MIN), 0);
    EXPECT_EQ(big_integer(9007199254740993.0), two_to(53));   // the double is 2^53
    // The nearest double, a tie to the even one
    EXPECT_EQ((two_to(53) + 1).to_double(), 0x1p53);
    EXPECT_EQ((two_to(53) + 3).to_double(), 0x1p53 + 4);
    EXPECT_EQ((two_to(64) - 1).to_double(), 0x1p64);
    EXPECT_EQ(big_integer(INT64_MIN).to_double(), -0x1p63);
    EXPECT_EQ(((two_to(53) - 1) << 971).to_double(), DBL_MAX);
    EXPECT_EQ((((two_to(53) - 1) << 971) + two_to(969)).to_double(), DBL_MAX);
    EXPECT_EQ((two_to(1024) - two_to(970) - 1).to_double(), DBL_MAX);
    EXPECT_EQ((two_to(1024) - two_to(970)).to_double(), INFINITY);   // the tie, to the even one: past the largest
    EXPECT_EQ(two_to(1024).to_double(), INFINITY);
    EXPECT_EQ((-two_to(1024)).to_double(), -INFINITY);
    EXPECT_EQ(two_to(100000).to_double(), INFINITY);
    EXPECT_EQ((two_to(200) + two_to(147)).to_double(), 0x1p200);                 // a tie, to the even one
    EXPECT_EQ((two_to(200) + two_to(147) + 1).to_double(), 0x1p200 + 0x1p148);   // past the tie by the lowest bit
}

TEST(BigIntBoundary_Tests, AnArgumentThatIsTheValueItself) {
    for (big_integer a : {big_integer(INT64_MIN), two_to(64) - 1, -two_to(64), two_to(300) + 7}) {
        big_integer x = a * 1;   // a value no other has seen: its own object, written in place
        x += x;
        EXPECT_EQ(x, a * 2);
        x = a * 1;
        x -= x;
        EXPECT_EQ(x, 0);
        x = a * 1;
        x *= x;
        EXPECT_EQ(x, a * a);
        x = a * 1;
        x /= x;
        EXPECT_EQ(x, 1);
        x = a * 1;
        x %= x;
        EXPECT_EQ(x, 0);
        x = a * 1;
        x |= x;
        EXPECT_EQ(x, a);
        x ^= x;
        EXPECT_EQ(x, 0);
        x = a;
        EXPECT_EQ(x.gcd(x), a.abs());
        EXPECT_EQ(x.lcm(x), a.abs());
        auto [q, r] = x.div_rem(x);
        EXPECT_EQ(q, 1);
        EXPECT_EQ(r, 0);
        EXPECT_EQ(x.mod(x), 0);
        EXPECT_EQ(x.mod_inverse(x), nullopt);
        EXPECT_EQ(x.abs().mod_pow(x.abs(), x.abs()), 0);
        const big_integer& alias = x;
        x = alias;
        EXPECT_EQ(x, a);
        big_integer& self = x;
        x = std::move(self);
        EXPECT_EQ(x, a);
    }
}

TEST(BigIntBoundary_Tests, ComparisonsWithTheBuiltinsAtTheirEdges) {
    EXPECT_EQ(two_to(64) - 1, UINT64_MAX);
    EXPECT_NE(two_to(64), UINT64_MAX);
    EXPECT_NE(two_to(64), 0u);
    EXPECT_GT(two_to(64), UINT64_MAX);
    EXPECT_LT(-two_to(64), INT64_MIN);
    EXPECT_EQ(two_to(63), uint64_t(1) << 63);
    EXPECT_NE(two_to(63), INT64_MIN);
    EXPECT_EQ(-two_to(63), INT64_MIN);
    EXPECT_GT(two_to(63), INT64_MAX);
    EXPECT_LT(-two_to(63) - 1, INT64_MIN);
    EXPECT_EQ(-(two_to(64) - 1), -__int128(UINT64_MAX));
    EXPECT_LT(-(two_to(64) - 1), 0);
    EXPECT_EQ(big_integer(-1) <=> UINT64_MAX, std::strong_ordering::less);
    EXPECT_EQ(two_to(127) - 1, std::numeric_limits<__int128>::max());
    EXPECT_EQ(-two_to(127), std::numeric_limits<__int128>::min());
    EXPECT_EQ(two_to(128) - 1, ~(unsigned __int128)0);
    EXPECT_GT(two_to(128), ~(unsigned __int128)0);
}

//----------------------------------------------------------------------------
// rational
//----------------------------------------------------------------------------

TEST(RationalBoundary_Tests, Zero) {
    rational z;
    EXPECT_EQ(z.numerator(), 0);
    EXPECT_EQ(z.denominator(), 1);
    EXPECT_EQ(text(z.to_string()), "0");
    EXPECT_EQ(text(z.to_decimal(0)), "0");
    EXPECT_EQ(text(z.to_decimal(3)), "0.000");
    EXPECT_EQ(z.to_double(), 0.0);
    EXPECT_FALSE(std::signbit(z.to_double()));
    EXPECT_EQ(z.floor(), 0);
    EXPECT_EQ(z.ceil(), 0);
    EXPECT_EQ(z.abs(), z);
    EXPECT_EQ(-z, z);
    EXPECT_THROW((void)z.inverse(), domain_error);
    EXPECT_EQ(z.pow(0), 1);
    EXPECT_EQ(z.pow(INT64_MAX), 0);
    EXPECT_THROW((void)z.pow(-1), domain_error);
    EXPECT_THROW((void)z.pow(INT64_MIN), domain_error);
    EXPECT_EQ(std::hash<rational>()(z), std::hash<rational>()(rational(0, 5)));
    // Negative zero, every way it is written
    for (rational r : {rational(-0.0), rational(0, -5), rational(big_integer(-0.0), 7), *rational::parse("-0"), *rational::parse("-0/3"),
                       *rational::parse("-0.000"), *rational::parse("-0e5"), *rational::parse("-.0"), rational(-1, 3) + rational(1, 3)}) {
        EXPECT_EQ(r, z);
        EXPECT_EQ(r.numerator().sign(), 0);
        EXPECT_EQ(r.denominator(), 1);
        EXPECT_EQ(text(r.to_decimal(2)), "0.00");
        EXPECT_EQ(text(r.to_string()), "0");
        EXPECT_FALSE(std::signbit(r.to_double()));
    }
}

TEST(RationalBoundary_Tests, ADenominatorOfZero) {
    EXPECT_THROW(rational(1, 0), domain_error);
    EXPECT_THROW(rational(0, 0), domain_error);
    EXPECT_THROW(rational(-1, big_integer(-0.0)), domain_error);
    EXPECT_THROW(rational(two_to(200), two_to(64) - two_to(64)), domain_error);
    rational r(3, 4);
    EXPECT_THROW((void)(r / rational()), domain_error);
    EXPECT_THROW((void)(r / 0), domain_error);
    EXPECT_THROW(r /= 0, domain_error);
    EXPECT_EQ(r, rational(3, 4));   // the value as it was
    auto zero = [](const char* s) {
        auto v = rational::parse(s);
        EXPECT_FALSE(v) << s;
        return v ? SIZE_MAX : v.error().offset();
    };
    EXPECT_EQ(zero("1/0"), 2u);
    EXPECT_EQ(zero("0/0"), 2u);
    EXPECT_EQ(zero("-1/000"), 3u);
    EXPECT_EQ(zero("+5/0000000000000000000000000000000000"), 3u);
    EXPECT_EQ(text(rational::parse("1/0").error().message()), "a denominator of zero at byte 2");
}

TEST(RationalBoundary_Tests, ParseAtItsLimits) {
    auto offset = [](const char* s) {
        auto v = rational::parse(s);
        EXPECT_FALSE(v) << s;
        return v ? SIZE_MAX : v.error().offset();
    };
    EXPECT_EQ(offset(""), 0u);
    EXPECT_EQ(offset("+"), 1u);
    EXPECT_EQ(offset("-"), 1u);
    EXPECT_EQ(offset("."), 1u);
    EXPECT_EQ(offset("-."), 2u);
    EXPECT_EQ(offset("e5"), 0u);
    EXPECT_EQ(offset(".e5"), 1u);
    EXPECT_EQ(offset("1e"), 2u);
    EXPECT_EQ(offset("1e+"), 3u);
    EXPECT_EQ(offset("1e-"), 3u);
    EXPECT_EQ(offset("1.5e"), 4u);
    EXPECT_EQ(offset("1e5.0"), 3u);
    EXPECT_EQ(offset("/2"), 0u);
    EXPECT_EQ(offset("1/"), 2u);
    EXPECT_EQ(offset("1/-2"), 2u);
    EXPECT_EQ(offset("1/+2"), 2u);
    EXPECT_EQ(offset("1/2/3"), 3u);
    EXPECT_EQ(offset("1.5/2"), 3u);
    EXPECT_EQ(offset("1/2e3"), 3u);
    EXPECT_EQ(offset(" 1"), 0u);
    EXPECT_EQ(offset("1 "), 1u);
    EXPECT_EQ(offset("1e1000001"), 2u);
    EXPECT_EQ(offset("1e-1000001"), 2u);
    EXPECT_EQ(offset("1e+99999999999999999999999999"), 2u);
    EXPECT_EQ(text(rational::parse("1e1000001").error().message()), "an exponent past a million at byte 2");
    EXPECT_EQ(rational::parse("+.5"), rational(1, 2));
    EXPECT_EQ(rational::parse("-.5e1"), -5);
    EXPECT_EQ(rational::parse("5."), 5);
    EXPECT_EQ(rational::parse("5.e-1"), rational(1, 2));
    EXPECT_EQ(rational::parse("1e-0"), 1);
    EXPECT_EQ(rational::parse("1e+0"), 1);
    EXPECT_EQ(rational::parse("0e1000000"), 0);
    EXPECT_EQ(rational::parse("0.0001e4"), 1);
    // The exponent at its limit, both ways
    auto big = rational::parse("1e1000000");
    ASSERT_TRUE(big);
    EXPECT_EQ(big->denominator(), 1);
    EXPECT_EQ(big->numerator(), big_integer(10).pow(1000000));
    auto small = rational::parse("-1e-1000000");
    ASSERT_TRUE(small);
    EXPECT_EQ(small->numerator(), -1);
    EXPECT_EQ(small->denominator(), big_integer(10).pow(1000000));
    EXPECT_EQ(small->to_double(), -0.0);
    EXPECT_TRUE(std::signbit(small->to_double()));
    // The edges of a limb in the parts
    EXPECT_EQ(rational::parse("18446744073709551616/18446744073709551615"), rational(two_to(64), two_to(64) - 1));
    EXPECT_EQ(rational::parse("-9223372036854775808/1"), INT64_MIN);
    EXPECT_FALSE(rational::parse("9223372036854775808/-1"));
    EXPECT_EQ(rational::parse("18446744073709551616/36893488147419103232"), rational(1, 2));
}

TEST(RationalBoundary_Tests, PowAtItsLimits) {
    EXPECT_EQ(rational(1).pow(INT64_MIN), 1);
    EXPECT_EQ(rational(-1).pow(INT64_MIN), 1);
    EXPECT_EQ(rational(-1).pow(INT64_MAX), -1);
    EXPECT_EQ(rational(-1).pow(INT64_MIN + 1), -1);
    EXPECT_THROW((void)rational(2).pow(INT64_MIN), length_error);
    EXPECT_THROW((void)rational(1, 2).pow(INT64_MIN), length_error);
    EXPECT_THROW((void)rational(1, 2).pow(INT64_MAX), length_error);
    EXPECT_THROW((void)rational(3, 2).pow(INT64_MAX), length_error);
    EXPECT_EQ(rational(-2, 3).pow(-1), rational(-3, 2));
    EXPECT_EQ(rational(-2, 3).pow(-2), rational(9, 4));
    EXPECT_EQ(rational(-2, 3).pow(1), rational(-2, 3));
    EXPECT_EQ(rational(INT64_MIN).pow(-1), rational(-1, two_to(63)));
    EXPECT_EQ(rational(1, INT64_MIN).pow(-1), INT64_MIN);
    EXPECT_EQ(rational(1, INT64_MIN).pow(-1).numerator().to_int64(), INT64_MIN);
}

TEST(RationalBoundary_Tests, TheEdgesOfALimbInThePartsAndTheSigns) {
    EXPECT_EQ(rational(INT64_MIN, -1), two_to(63));
    EXPECT_EQ(rational(1, INT64_MIN), rational(-1, two_to(63)));
    EXPECT_EQ(rational(1, INT64_MIN).denominator(), two_to(63));
    EXPECT_EQ(rational(INT64_MIN, INT64_MIN), 1);
    EXPECT_EQ(rational(-two_to(64), two_to(128)), rational(-1, two_to(64)));
    EXPECT_EQ(rational(INT64_MIN).abs(), two_to(63));
    EXPECT_EQ(-rational(INT64_MIN), two_to(63));
    EXPECT_EQ(rational(INT64_MIN).inverse(), rational(-1, two_to(63)));
    EXPECT_LT(rational(INT64_MIN), rational(INT64_MIN + 1));
    EXPECT_LT(rational(-1, two_to(64)), rational(-1, two_to(64) + 1));
    EXPECT_GT(rational(1, two_to(64) - 1), rational(1, two_to(64)));
    EXPECT_EQ(rational(1, two_to(64) - 1) <=> rational(1, two_to(64) - 1), std::strong_ordering::equal);
    // floor and ceil round the right way on both sides of zero
    EXPECT_EQ(rational(-1, 2).floor(), -1);
    EXPECT_EQ(rational(-1, 2).ceil(), 0);
    EXPECT_EQ(rational(1, 2).floor(), 0);
    EXPECT_EQ(rational(1, 2).ceil(), 1);
    EXPECT_EQ(rational(INT64_MIN).floor(), INT64_MIN);
    EXPECT_EQ(rational(INT64_MIN).ceil(), INT64_MIN);
    EXPECT_EQ(rational(-two_to(64) - 1, 2).floor(), -two_to(63) - 1);
    EXPECT_EQ(rational(-two_to(64) - 1, 2).ceil(), -two_to(63));
    EXPECT_EQ(rational(two_to(200) + 1, two_to(200)).floor(), 1);
    EXPECT_EQ(rational(two_to(200) + 1, two_to(200)).ceil(), 2);
    // The arithmetic at the edges, against the identities
    for (big_integer n : {big_integer(INT64_MIN), two_to(64) - 1, -two_to(64), big_integer(1), big_integer(-1)}) {
        for (big_integer d : {big_integer(1), big_integer(INT64_MAX), two_to(63), two_to(64) + 1}) {
            rational a(n, d);
            rational b(d, n);
            ASSERT_EQ(a * b, 1);
            ASSERT_EQ(a + b - b, a);
            ASSERT_EQ(a - a, 0);
            ASSERT_EQ(a / a, 1);
            ASSERT_EQ(a * a.inverse(), 1);
            ASSERT_EQ(*rational::parse(a.to_string()), a);
            ASSERT_GT(a.denominator().sign(), 0);
            ASSERT_EQ(a.numerator().gcd(a.denominator()), 1);
        }
    }
}

TEST(RationalBoundary_Tests, ToDecimalAtItsLimits) {
    EXPECT_EQ(text(rational(1, 2).to_decimal(0)), "1");
    EXPECT_EQ(text(rational(-1, 2).to_decimal(0)), "-1");
    EXPECT_EQ(text(rational(3, 2).to_decimal(0)), "2");
    EXPECT_EQ(text(rational(-5, 2).to_decimal(0)), "-3");
    EXPECT_EQ(text(rational(1, 3).to_decimal(0)), "0");
    EXPECT_EQ(text(rational(-1, 3).to_decimal(0)), "-0");
    EXPECT_EQ(text(rational(-1, 1000).to_decimal(2)), "-0.00");
    EXPECT_EQ(text(rational(-5, 1000).to_decimal(2)), "-0.01");
    EXPECT_EQ(text(rational(999, 1000).to_decimal(2)), "1.00");
    EXPECT_EQ(text(rational(-999, 1000).to_decimal(2)), "-1.00");
    EXPECT_EQ(text(rational(1, two_to(64)).to_decimal(20)), "0.00000000000000000005");
    EXPECT_EQ(text(rational(INT64_MIN).to_decimal(1)), "-9223372036854775808.0");
    EXPECT_THROW((void)rational(1, 3).to_decimal(sgcl::string::max_size()), length_error);
    EXPECT_THROW((void)rational(1, 3).to_decimal(SIZE_MAX), length_error);
    EXPECT_THROW((void)rational().to_decimal(SIZE_MAX), length_error);
}

TEST(RationalBoundary_Tests, ToDoubleAtItsLimits) {
    EXPECT_EQ(rational(1, two_to(1074)).to_double(), DBL_TRUE_MIN);
    EXPECT_EQ(rational(-1, two_to(1074)).to_double(), -DBL_TRUE_MIN);
    EXPECT_EQ(rational(1, two_to(1075)).to_double(), 0.0);                 // the tie, to the even zero
    EXPECT_TRUE(std::signbit(rational(-1, two_to(1075)).to_double()));
    EXPECT_EQ(rational(3, two_to(1076)).to_double(), DBL_TRUE_MIN);        // past the tie
    EXPECT_EQ(rational(1, two_to(1076)).to_double(), 0.0);
    EXPECT_EQ(rational(3, two_to(1075)).to_double(), 2 * DBL_TRUE_MIN);   // 1.5 of the smallest: the tie to the even 2
    EXPECT_EQ(rational(1, two_to(1022)).to_double(), DBL_MIN);
    EXPECT_EQ(rational(two_to(52) - 1, two_to(1074)).to_double(), DBL_MIN - DBL_TRUE_MIN);   // the largest subnormal
    EXPECT_EQ(rational((two_to(53) - 1) << 971).to_double(), DBL_MAX);
    EXPECT_EQ(rational(((two_to(53) - 1) << 1) + 1, 2).to_double(), double(two_to(53).to_double()));   // (2^54 - 1)/2: a tie to even
    EXPECT_EQ(rational(two_to(1024) - two_to(970), 1).to_double(), INFINITY);
    EXPECT_EQ(rational(two_to(2048) - 1, two_to(1024)).to_double(), INFINITY);   // just below 2^1024: rounds past the largest
    EXPECT_EQ(rational(-(two_to(2048) - 1), two_to(1024)).to_double(), -INFINITY);
    EXPECT_EQ(rational(1, 3).to_double(), 1.0 / 3);
    EXPECT_EQ(rational(-1, 3).to_double(), -1.0 / 3);
    // From the doubles at their limits and back
    for (double d : {DBL_MAX, -DBL_MAX, DBL_MIN, -DBL_MIN, DBL_TRUE_MIN, -DBL_TRUE_MIN, 1.0, -1.0, 0x1p-1022 * 1.5, 0.1}) {
        EXPECT_EQ(rational(d).to_double(), d) << d;
    }
    EXPECT_THROW((void)rational(NAN), domain_error);
    EXPECT_THROW((void)rational(INFINITY), domain_error);
    EXPECT_THROW((void)rational(-INFINITY), domain_error);
    EXPECT_EQ(rational(DBL_TRUE_MIN), rational(1, two_to(1074)));
    EXPECT_EQ(rational(DBL_MAX), rational((two_to(53) - 1) << 971));
}

TEST(RationalBoundary_Tests, AnArgumentThatIsTheValueItself) {
    rational r(-7, 3);
    rational x = r;
    x += x;
    EXPECT_EQ(x, rational(-14, 3));
    x = r;
    x -= x;
    EXPECT_EQ(x, 0);
    x = r;
    x *= x;
    EXPECT_EQ(x, rational(49, 9));
    x = r;
    x /= x;
    EXPECT_EQ(x, 1);
    x = rational();
    EXPECT_THROW(x /= x, domain_error);
    EXPECT_EQ(x, 0);
    x = r;
    const rational& alias = x;
    x = alias;
    EXPECT_EQ(x, r);
    rational& self = x;
    x = std::move(self);
    EXPECT_EQ(x, r);
}

//----------------------------------------------------------------------------
// random
//----------------------------------------------------------------------------

TEST(RandomBoundary_Tests, TheBoundsAtTheirLimits) {
    math::random g(7);
    EXPECT_THROW((void)g.next_int(0), domain_error);
    EXPECT_THROW((void)g.next_int(-1), domain_error);
    EXPECT_THROW((void)g.next_int(INT64_MIN), domain_error);
    EXPECT_THROW((void)g.next_int(5, 5), domain_error);
    EXPECT_THROW((void)g.next_int(6, 5), domain_error);
    EXPECT_THROW((void)g.next_int(INT64_MAX, INT64_MIN), domain_error);
    EXPECT_THROW((void)g.next_int(big_integer(0)), domain_error);
    EXPECT_THROW((void)g.next_int(-two_to(100)), domain_error);
    for (int i = 0; i < 1000; ++i) {
        ASSERT_EQ(g.next_int(1), 0);
        ASSERT_EQ(g.next_int(INT64_MIN, INT64_MIN + 1), INT64_MIN);
        ASSERT_EQ(g.next_int(INT64_MAX - 1, INT64_MAX), INT64_MAX - 1);
        ASSERT_EQ(g.next_int(big_integer(1)), 0);
        int64_t any = g.next_int(INT64_MIN, INT64_MAX);
        ASSERT_LT(any, INT64_MAX);
        int64_t below = g.next_int(INT64_MAX);
        ASSERT_GE(below, 0);
        ASSERT_LT(below, INT64_MAX);
        int64_t power = g.next_int(int64_t(1) << 62);
        ASSERT_GE(power, 0);
        ASSERT_LT(power, int64_t(1) << 62);
        for (big_integer bound : {two_to(63), two_to(64) - 1, two_to(64), two_to(64) + 1, two_to(128)}) {
            big_integer v = g.next_int(bound);
            ASSERT_GE(v, 0);
            ASSERT_LT(v, bound);
        }
    }
}

TEST(RandomBoundary_Tests, TheDistributionsAtTheirLimits) {
    math::random g(11);
    EXPECT_THROW((void)g.next_normal(0, -1), domain_error);
    EXPECT_THROW((void)g.next_normal(0, NAN), domain_error);
    EXPECT_THROW((void)g.next_normal(0, -INFINITY), domain_error);
    EXPECT_THROW((void)g.next_exponential(0), domain_error);
    EXPECT_THROW((void)g.next_exponential(-0.0), domain_error);
    EXPECT_THROW((void)g.next_exponential(-1), domain_error);
    EXPECT_THROW((void)g.next_exponential(NAN), domain_error);
    EXPECT_THROW((void)g.next_exponential(-INFINITY), domain_error);
    for (int i = 0; i < 1000; ++i) {
        ASSERT_EQ(g.next_normal(5, 0), 5);
        ASSERT_EQ(g.next_normal(-3, -0.0), -3);
        ASSERT_EQ(g.next_exponential(INFINITY), 0);
        double e = g.next_exponential(DBL_TRUE_MIN);
        ASSERT_GE(e, 0);
        double d = g.next_double();
        ASSERT_GE(d, 0);
        ASSERT_LT(d, 1);
    }
}

TEST(RandomBoundary_Tests, EmptyAndSingleRanges) {
    math::random g(3);
    math::random same = g;
    auto out = [](std::vector<byte>& v) { return slice<byte>(v.data(), v.size()); };
    std::vector<byte> none;
    g.next_bytes(out(none));   // no draw for no bytes
    EXPECT_EQ(g.next_uint64(), same.next_uint64());
    std::vector<byte> one(1);
    g.next_bytes(out(one));    // one draw for the first eight
    (void)same.next_uint64();
    EXPECT_EQ(g.next_uint64(), same.next_uint64());
    std::vector<byte> nine(9);
    g.next_bytes(out(nine));   // two for nine
    (void)same.next_uint64();
    (void)same.next_uint64();
    EXPECT_EQ(g.next_uint64(), same.next_uint64());
    std::vector<int> empty;
    g.shuffle(empty);
    EXPECT_TRUE(empty.empty());
    EXPECT_THROW((void)g.pick(empty), out_of_range);
    std::vector<int> single = {42};
    g.shuffle(single);
    EXPECT_EQ(single, std::vector<int>{42});
    EXPECT_EQ(g.pick(single), 42);
    EXPECT_EQ(&g.pick(single), &single[0]);
    EXPECT_TRUE(g.permutation(0).empty());
    EXPECT_EQ(g.permutation(1).size(), 1u);
    EXPECT_EQ(g.permutation(1)[0], 0u);
    // A random moved from draws on, as its copy does
    math::random a(9);
    math::random b = std::move(a);
    math::random c(9);
    EXPECT_EQ(b.next_uint64(), c.next_uint64());
    (void)a.next_uint64();
    static_assert(math::random::min() == 0 && math::random::max() == UINT64_MAX);
}

//----------------------------------------------------------------------------
// parse_error
//----------------------------------------------------------------------------

TEST(ParseErrorBoundary_Tests, EveryReasonAndItsByte) {
    EXPECT_EQ(text(big_integer::parse("").error().message()), "empty text at byte 0");
    EXPECT_EQ(text(big_integer::parse("-").error().message()), "no digits after the sign at byte 1");
    EXPECT_EQ(text(big_integer::parse("1z", 35).error().message()), "not a digit in base 35 at byte 1");
    EXPECT_EQ(text(big_integer::parse("12", 2).error().message()), "not a digit in base 2 at byte 1");
    EXPECT_EQ(text(rational::parse("2/0").error().message()), "a denominator of zero at byte 2");
    EXPECT_EQ(text(rational::parse("2e-1000001").error().message()), "an exponent past a million at byte 2");
    EXPECT_EQ(big_integer::parse("x").error(), big_integer::parse("y").error());
    EXPECT_NE(big_integer::parse("x").error(), big_integer::parse("1x").error());
    EXPECT_NE(big_integer::parse("x", 10).error(), big_integer::parse("x", 16).error());   // the base is part of the sentence
    EXPECT_NE(big_integer::parse("").error(), big_integer::parse("x").error());
    auto e = big_integer::parse("").error();
    auto copy = e;
    EXPECT_EQ(copy, e);
}
