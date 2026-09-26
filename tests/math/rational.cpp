//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// math::rational against Python's fractions.Fraction and float
// (rational_vectors.h, from tools/math_vectors.py rational): arithmetic and
// order over pairs of named and random fractions, floor and ceil, the
// double nearest a fraction at the hard places of the rounding (ties in
// the normal and the subnormal range, the edges), the fraction a double
// is, parsing; to_decimal against the definition of rounding half away
// from zero.
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "rational_vectors.h"

#include <bit>
#include <cmath>
#include <random>
#include <sstream>
#include <string>

using math::big_integer;
using math::rational;

namespace {
    big_integer D(const char* s) {
        auto v = big_integer::parse(s);
        EXPECT_TRUE(v.has_value()) << s;
        return v ? *v : big_integer();
    }

    rational R(const char* n, const char* d) {
        return rational(D(n), D(d));
    }

    std::string str(const string& s) {
        return std::string(s.data(), s.size());
    }

    // The parts as they must be: the denominator above zero and lowest terms
    void expect_parts(const rational& r, const char* n, const char* d) {
        EXPECT_EQ(str(r.numerator().to_string()), n);
        EXPECT_EQ(str(r.denominator().to_string()), d);
    }
}

TEST(Rational_Tests, ArithmeticAgainstPython) {
    size_t checked = 0;
    for (auto& t : rational_vectors::pairs) {
        rational a = R(t.an, t.ad);
        rational b = R(t.bn, t.bd);
        std::string where = str(a.to_string()) + " and " + str(b.to_string());
        ASSERT_EQ(a + b, R(t.sn, t.sd)) << where;
        ASSERT_EQ(a - b, R(t.dn, t.dd)) << where;
        ASSERT_EQ(a * b, R(t.pn, t.pd)) << where;
        if (*t.qn) {
            ASSERT_EQ(a / b, R(t.qn, t.qd)) << where;
        } else {
            ASSERT_THROW(a / b, std::domain_error);
        }
        auto order = a <=> b;
        ASSERT_EQ((order > 0) - (order < 0), t.order) << where;
        ASSERT_EQ(a == b, t.order == 0);
        // The parts of every result are in lowest terms with the sign up
        rational s = a + b;
        ASSERT_GT(s.denominator(), 0);
        ASSERT_EQ(s.numerator().gcd(s.denominator()), s.numerator() == 0 ? s.denominator() : 1);
        ++checked;
    }
    EXPECT_GT(checked, 600u);
}

TEST(Rational_Tests, SinglesAgainstPython) {
    for (auto& t : rational_vectors::singles) {
        rational a = R(t.n, t.d);
        std::string where = str(a.to_string());
        ASSERT_EQ(a.floor(), D(t.floor)) << where;
        ASSERT_EQ(a.ceil(), D(t.ceil)) << where;
        ASSERT_EQ(std::bit_cast<uint64_t>(a.to_double()), std::bit_cast<uint64_t>(t.to_double)) << where;
        const size_t places[] = {0, 1, 2, 3, 10};
        for (size_t i = 0; i < 5; ++i) {
            ASSERT_EQ(str(a.to_decimal(places[i])), t.places[i]) << where << " to " << places[i] << " places";
        }
    }
}

TEST(Rational_Tests, ToDoubleAgainstPython) {
    for (auto& t : rational_vectors::to_double) {
        rational a = R(t.n, t.d);
        double v = a.to_double();
        ASSERT_EQ(std::bit_cast<uint64_t>(v), std::bit_cast<uint64_t>(t.value))
            << t.n << "/" << t.d << ": " << v << " != " << t.value;
    }
    // The quotient of the parts' doubles rounds three times; this rounds once
    rational third(1, 3);
    EXPECT_EQ(third.to_double(), 1.0 / 3);
    EXPECT_EQ(rational(-1, big_integer(1) << 1080).to_double(), -0.0);
    EXPECT_TRUE(std::signbit(rational(-1, big_integer(1) << 1080).to_double()));
}

TEST(Rational_Tests, FromDoubleAgainstPython) {
    for (auto& t : rational_vectors::from_double) {
        rational a(t.value);
        expect_parts(a, t.n, t.d);
        ASSERT_EQ(std::bit_cast<uint64_t>(a.to_double()), std::bit_cast<uint64_t>(t.value + 0.0)) << t.value;
    }
    EXPECT_NE(rational(0.1), rational(1, 10));
    EXPECT_THROW((void)rational(std::nan("")), std::domain_error);
    EXPECT_THROW((void)rational(HUGE_VAL), std::domain_error);
    EXPECT_THROW((void)rational(-HUGE_VAL), std::domain_error);
    static_assert(!std::is_constructible_v<rational, long double>);
    static_assert(!std::is_constructible_v<rational, bool>);
    static_assert(!std::is_convertible_v<double, rational>);
    static_assert(std::is_convertible_v<int, rational>);
    static_assert(std::is_convertible_v<big_integer, rational>);
}

TEST(Rational_Tests, ParseAgainstPython) {
    for (auto& t : rational_vectors::parsed) {
        auto r = rational::parse(t.text);
        ASSERT_TRUE(r.has_value()) << t.text;
        expect_parts(*r, t.n, t.d);
    }
    // An exponent of a million is allowed, and gives 10^±1000000
    auto big = rational::parse("1e1000000");
    ASSERT_TRUE(big.has_value());
    EXPECT_EQ(big->numerator(), big_integer(10).pow(1000000));
    auto small = rational::parse("-1e-1000000");
    ASSERT_TRUE(small.has_value());
    EXPECT_EQ(small->numerator(), -1);
    EXPECT_EQ(small->denominator(), big_integer(10).pow(1000000));
}

TEST(Rational_Tests, ParseErrors) {
    auto error = [](const char* text) {
        auto r = rational::parse(text);
        EXPECT_FALSE(r.has_value()) << text;
        return r ? std::pair<size_t, std::string>(size_t(-1), "") : std::pair<size_t, std::string>(r.error().offset(), str(r.error().message()));
    };
    EXPECT_EQ(error("").first, 0u);
    EXPECT_EQ(error("-").first, 1u);
    EXPECT_EQ(error("+").first, 1u);
    EXPECT_EQ(error(".").first, 1u);
    EXPECT_EQ(error("x").first, 0u);
    EXPECT_EQ(error(" 1").first, 0u);
    EXPECT_EQ(error("1 ").first, 1u);
    EXPECT_EQ(error("1_000").first, 1u);
    EXPECT_EQ(error("/2").first, 0u);
    EXPECT_EQ(error("1/").first, 2u);
    EXPECT_EQ(error("1/-2").first, 2u);
    EXPECT_EQ(error("1/2/3").first, 3u);
    EXPECT_EQ(error("1/2e3").first, 3u);
    EXPECT_EQ(error("1.5/2").first, 3u);
    EXPECT_EQ(error("1e").first, 2u);
    EXPECT_EQ(error("1e+").first, 3u);
    EXPECT_EQ(error("1e5x").first, 3u);
    EXPECT_EQ(error("1.2.3").first, 3u);
    EXPECT_EQ(error("inf").first, 0u);
    EXPECT_EQ(error("nan").first, 0u);
    EXPECT_EQ(error("0x10").first, 1u);
    auto zero = error("3/0");
    EXPECT_EQ(zero.first, 2u);
    EXPECT_EQ(zero.second, "a denominator of zero at byte 2");
    EXPECT_EQ(error("-3/000").first, 3u);
    auto far = error("1e1000001");
    EXPECT_EQ(far.first, 2u);
    EXPECT_EQ(far.second, "an exponent past a million at byte 2");
    EXPECT_EQ(error("1e-99999999999999999999999999").first, 2u);   // held, not overflowed
    EXPECT_EQ(error("12x4").second, "not a digit in base 10 at byte 2");
}

TEST(Rational_Tests, Construction) {
    expect_parts(rational(), "0", "1");
    expect_parts(rational(6, -4), "-3", "2");
    expect_parts(rational(-6, -4), "3", "2");
    expect_parts(rational(0, -5), "0", "1");
    expect_parts(rational(7), "7", "1");
    expect_parts(rational(big_integer(1) << 100, big_integer(1) << 98), "4", "1");
    EXPECT_THROW(rational(1, 0), std::domain_error);
    rational r = 5;
    EXPECT_EQ(r, 5);
    EXPECT_EQ(r, big_integer(5));
    EXPECT_TRUE(r > 4);
    EXPECT_TRUE(big_integer(6) > r);
    EXPECT_TRUE(rational(1, 3) < rational(1, 2));
    EXPECT_EQ(rational(UINT64_MAX), big_integer(UINT64_MAX));
}

TEST(Rational_Tests, Members) {
    rational third(1, 3);
    EXPECT_EQ(third + third + third, 1);
    EXPECT_EQ(third * 3, 1);
    EXPECT_EQ(2 * third, rational(2, 3));
    EXPECT_EQ(1 - third, rational(2, 3));
    EXPECT_EQ(third / 2, rational(1, 6));
    EXPECT_EQ(-third, rational(-1, 3));
    EXPECT_EQ((-third).abs(), third);
    EXPECT_EQ(rational(-2, 3).inverse(), rational(-3, 2));
    EXPECT_THROW((void)rational().inverse(), std::domain_error);
    EXPECT_THROW(third / 0, std::domain_error);
    EXPECT_EQ(rational(2, 3).pow(-2), rational(9, 4));
    EXPECT_EQ(rational(-2, 3).pow(3), rational(-8, 27));
    EXPECT_EQ(rational(-2, 3).pow(0), 1);
    EXPECT_EQ(rational().pow(0), 1);
    EXPECT_THROW((void)rational().pow(-1), std::domain_error);
    EXPECT_EQ(rational(1).pow(INT64_MIN), 1);
    EXPECT_EQ(rational(-1).pow(INT64_MIN), 1);
    EXPECT_EQ(rational(-1).pow(INT64_MIN + 1), -1);
    EXPECT_EQ(str(rational(3, 4).to_string()), "3/4");
    EXPECT_EQ(str(rational(-5).to_string()), "-5");
    EXPECT_EQ(str((third * 2).to_decimal(4)), "0.6667");
    rational x = third;
    x += third;
    x -= rational(1, 6);
    x *= 4;
    x /= 3;
    EXPECT_EQ(x, rational(2, 3));
    std::ostringstream os;
    os << rational(-7, 2) << ' ' << rational(4);
    EXPECT_EQ(os.str(), "-7/2 4");
}

TEST(Rational_Tests, Format) {
    rational r(-1, 3);
    EXPECT_EQ(txt::format("{}", rational(3, 4)), string("3/4"));
    EXPECT_EQ(txt::format("{}", r), string("-1/3"));
    EXPECT_EQ(txt::format("{:.5f}", rational(1, 3)), string("0.33333"));
    EXPECT_EQ(txt::format("{:.2}", rational(2, 3)), string("0.67"));
    EXPECT_EQ(txt::format("{:f}", rational(1, 8)), string("0.125000"));
    EXPECT_EQ(txt::format("{:.0f}", rational(5, 2)), string("3"));
    EXPECT_EQ(txt::format("{:+}", rational(1, 2)), string("+1/2"));
    EXPECT_EQ(txt::format("{: .1f}", rational(1, 2)), string(" 0.5"));
    EXPECT_EQ(txt::format("[{:>8}]", r), string("[    -1/3]"));
    EXPECT_EQ(txt::format("[{:<8}]", r), string("[-1/3    ]"));
    EXPECT_EQ(txt::format("{:08.3f}", r), string("-000.333"));
    EXPECT_FALSE(txt::format(txt::runtime("{:x}"), r).has_value());
    EXPECT_FALSE(txt::format(txt::runtime("{:d}"), r).has_value());
}

TEST(Rational_Tests, HashAndMaps) {
    std::hash<rational> h;
    EXPECT_EQ(h(rational(2, 4)), h(rational(1, 2)));
    EXPECT_NE(h(rational(1, 2)), h(rational(-1, 2)));
    EXPECT_NE(h(rational(1, 2)), h(rational(2, 1)));
    sgcl::map<rational, int> m;
    sgcl::sorted_map<rational, int> sorted;
    for (int i = 1; i <= 50; ++i) {
        m[rational(i, 7)] = i;
        sorted[rational(-i, 3)] = i;
    }
    EXPECT_EQ(m[rational(14, 49)], 2);
    EXPECT_EQ(sorted.begin()->second, 50);
}

// The laws over random fractions of different lengths
TEST(Rational_Tests, AlgebraicProperties) {
    std::mt19937_64 rng(20260926);
    auto value = [&] {
        auto part = [&] {
            big_integer v = int64_t(rng() >> 1);
            for (int i = int(rng() % 4); i > 0; --i) {
                v = (v << 64) | big_integer(rng());
            }
            return v;
        };
        big_integer n = part();
        big_integer d = part() + 1;
        return rational(rng() & 1 ? -n : n, d);
    };
    for (int i = 0; i < 400; ++i) {
        rational a = value();
        rational b = value();
        rational c = value();
        ASSERT_EQ((a + b) - b, a);
        ASSERT_EQ(a * (b + c), a * b + a * c);
        if (b != 0) {
            ASSERT_EQ((a / b) * b, a);
        }
        ASSERT_EQ(a - a, 0);
        ASSERT_LE(rational(a.floor()), a);
        ASSERT_GT(rational(a.floor()) + 1, a);
        ASSERT_GE(rational(a.ceil()), a);
        ASSERT_EQ(*rational::parse(a.to_string()), a);
        // The decimal is within half a unit of the last place
        auto s = a.to_decimal(20);
        auto back = rational::parse(s);
        ASSERT_TRUE(back.has_value()) << str(s);
        ASSERT_LE((*back - a).abs() * big_integer(10).pow(20) * 2, 1);
        // The double is the nearest: the exact value of it is no further
        // than the neighbours'
        double d = a.to_double();
        if (std::isfinite(d) && d != 0) {
            rational exact(d);
            rational up(std::nextafter(d, HUGE_VAL));
            rational down(std::nextafter(d, -HUGE_VAL));
            ASSERT_LE((exact - a).abs(), (up - a).abs());
            ASSERT_LE((exact - a).abs(), (down - a).abs());
        }
    }
}
