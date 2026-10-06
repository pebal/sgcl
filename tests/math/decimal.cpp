//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// math::decimal against Python's decimal and fractions (decimal_vectors.h,
// from tools/math_vectors.py decimal): parsing, the exact operations, the
// division to a scale and to digits in every rounding mode, rescaling,
// rounding to digits, square roots, the text both ways, the doubles both
// ways, the fractions; the representation (unscaled part and scale) is
// checked, not the value alone. Then what Python cannot say: NaN and the
// infinities as PostgreSQL has them, the order and the hash, the errors,
// PostgreSQL's range, the edges of the small form, the laws over random
// values against rational, and that a small value allocates nothing.
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "decimal_vectors.h"

#include <cmath>
#include <cstring>
#include <random>
#include <sstream>
#include <string>

using math::big_integer;
using math::decimal;
using math::rational;

namespace {
    constexpr rounding Modes[] = {rounding::half_even, rounding::half_up, rounding::half_down, rounding::up,
                                  rounding::down,      rounding::ceiling, rounding::floor};

    big_integer B(const char* s) {
        auto v = big_integer::parse(s);
        EXPECT_TRUE(v.has_value()) << s;
        return v ? *v : big_integer();
    }

    decimal D(const char* s) {
        auto v = decimal::parse(s);
        EXPECT_TRUE(v.has_value()) << s;
        return v ? *v : decimal();
    }

    std::string str(const string& s) {
        return std::string(s.data(), s.size());
    }

    std::string show(const decimal& d) {
        return str(d.unscaled().to_string()) + " @" + std::to_string(d.scale());
    }

    // The decimal (unscaled, scale) and the same representation
    ::testing::AssertionResult is(const decimal& d, const char* unscaled, int scale) {
        decimal want(B(unscaled), scale);
        if (d.identical(want)) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << show(d) << " is not " << unscaled << " @" << scale;
    }
}

TEST(Decimal_Tests, ParseAgainstPython) {
    for (auto& t : decimal_vectors::parsed) {
        auto d = decimal::parse(t.text);
        ASSERT_TRUE(d.has_value()) << t.text;
        EXPECT_TRUE(is(*d, t.unscaled, t.scale)) << t.text;
        EXPECT_TRUE(decimal(string(t.text)).identical(*d));
    }
}

TEST(Decimal_Tests, ArithmeticAgainstPython) {
    sgcl::vector<decimal> values;
    for (auto t : decimal_vectors::values) {
        values.push_back(D(t));
    }
    size_t checked = 0;
    for (auto& t : decimal_vectors::pairs) {
        const decimal& a = values[t.a];
        const decimal& b = values[t.b];
        std::string where = str(a.to_string()) + " and " + str(b.to_string());
        ASSERT_TRUE(is(a + b, t.sum, t.sum_scale)) << where;
        ASSERT_TRUE(is(a - b, t.difference, t.difference_scale)) << where;
        ASSERT_TRUE(is(a * b, t.product, t.product_scale)) << where;
        int order = a < b ? -1 : a > b ? 1 : 0;
        ASSERT_EQ(order, t.order) << where;
        ASSERT_EQ(a == b, t.order == 0) << where;
        if (!*t.quotient) {
            ASSERT_THROW(a % b, std::domain_error);
            ASSERT_THROW(a.div(b, 2), std::domain_error);
            ASSERT_THROW(a.div_precision(b, 5), std::domain_error);
            continue;
        }
        ASSERT_TRUE(is(a % b, t.remainder, t.remainder_scale)) << where;
        auto [q, r] = a.div_rem(b);
        ASSERT_TRUE(is(q, t.quotient, 0)) << where;
        ASSERT_TRUE(r.identical(a % b)) << where;
        ASSERT_TRUE(is(a.div(b, 10), t.div10, t.div10_scale)) << where;
        ASSERT_TRUE(is(a.div_precision(b, 28), t.div28, t.div28_scale)) << where;
        for (int m = 0; m < 7; ++m) {
            ASSERT_TRUE(is(a.div(b, 3, Modes[m]), t.div3[m], 3)) << where << " mode " << m;
            ASSERT_TRUE(is(a.div_precision(b, 7, Modes[m]), t.div7[m], t.div7_scale[m])) << where << " mode " << m;
        }
        ++checked;
    }
    EXPECT_GT(checked, 900u);
}

TEST(Decimal_Tests, SinglesAgainstPython) {
    constexpr int Scales[] = {-1, 0, 1, 2};
    constexpr int RootDigits[] = {1, 5, 28, 50};
    for (auto& t : decimal_vectors::singles) {
        decimal a = D(t.text);
        EXPECT_EQ(str(a.to_string()), t.plain) << t.text;
        EXPECT_EQ(str(a.to_scientific()), t.scientific) << t.text;
        EXPECT_EQ(str(txt::format("{:e}", a)), t.scientific) << t.text;
        EXPECT_EQ(a.to_double(), t.to_double) << t.text;
        EXPECT_EQ(a.precision(), size_t(t.digits)) << t.text;
        for (int s = 0; s < 4; ++s) {
            for (int m = 0; m < 7; ++m) {
                EXPECT_TRUE(is(a.rescale(Scales[s], Modes[m]), t.rescaled[s][m], Scales[s]))
                    << t.text << " to " << Scales[s] << " mode " << m;
            }
        }
        for (int m = 0; m < 7; ++m) {
            EXPECT_TRUE(is(a.round_precision(1, Modes[m]), t.rounded1[m], t.rounded1_scale[m])) << t.text << " mode " << m;
            EXPECT_TRUE(is(a.round_precision(3, Modes[m]), t.rounded3[m], t.rounded3_scale[m])) << t.text << " mode " << m;
        }
        if (a.sign() >= 0) {
            for (int i = 0; i < 4; ++i) {
                EXPECT_TRUE(is(a.sqrt(RootDigits[i]), t.root[i], t.root_scale[i])) << t.text << " to " << RootDigits[i];
            }
        } else {
            EXPECT_THROW(a.sqrt(10), std::domain_error);
        }
        EXPECT_TRUE(is(a.trim_scale(), t.trimmed, t.trimmed_scale)) << t.text;
    }
}

TEST(Decimal_Tests, DoublesAgainstPython) {
    for (auto& t : decimal_vectors::from_double) {
        EXPECT_TRUE(is(decimal(t.value), t.exact, t.exact_scale)) << t.value;
        EXPECT_TRUE(is(decimal::shortest(t.value), t.shortest, t.shortest_scale)) << t.value;
        EXPECT_EQ(decimal(t.value).to_double(), t.value);
        EXPECT_EQ(decimal::shortest(t.value).to_double(), t.value);
    }
    for (auto& t : decimal_vectors::to_double) {
        EXPECT_EQ(D(t.text).to_double(), t.value) << t.text;
        EXPECT_EQ((-D(t.text)).to_double(), -t.value) << t.text;
    }
    EXPECT_TRUE(is(decimal(-0.0), "0", 0));
    EXPECT_TRUE(is(decimal::shortest(-0.0), "0", 0));
    EXPECT_TRUE(decimal(std::nan("")).is_nan());
    EXPECT_TRUE(decimal::shortest(-HUGE_VAL) == -decimal::infinity());
    EXPECT_TRUE(std::isnan(decimal::nan().to_double()));
    EXPECT_EQ(decimal::infinity().to_double(), HUGE_VAL);
    EXPECT_EQ((-decimal::infinity()).to_double(), -HUGE_VAL);
    EXPECT_TRUE(std::signbit(D("-1e-400").to_double()));
    // every double read back from its exact decimal and its shortest
    std::mt19937_64 rng(5);
    for (int i = 0; i < 20000; ++i) {
        uint64_t bits = rng();
        double x;
        std::memcpy(&x, &bits, sizeof x);
        if (!std::isfinite(x)) {
            continue;
        }
        ASSERT_EQ(decimal(x).to_double(), x) << bits;
        ASSERT_EQ(decimal::shortest(x).to_double(), x) << bits;
        ASSERT_EQ(decimal(x), decimal(rational(x), decimal(x).scale())) << bits;
    }
}

TEST(Decimal_Tests, FractionsAgainstPython) {
    for (auto& t : decimal_vectors::from_rational) {
        rational r(B(t.n), B(t.d));
        for (int m = 0; m < 7; ++m) {
            EXPECT_TRUE(is(decimal(r, 4, Modes[m]), t.places4[m], 4)) << t.n << "/" << t.d << " mode " << m;
            EXPECT_TRUE(is(decimal(r, -2, Modes[m]), t.places_minus2[m], -2)) << t.n << "/" << t.d << " mode " << m;
        }
    }
    EXPECT_EQ(D("1.50").to_rational(), rational(3, 2));
    EXPECT_EQ(D("-2.5e3").to_rational(), rational(-2500));
    EXPECT_EQ(D("0.000").to_rational(), rational(0));
    EXPECT_THROW(decimal::nan().to_rational(), std::domain_error);
    EXPECT_THROW(decimal::infinity().to_rational(), std::domain_error);
}

TEST(Decimal_Tests, LongResultsAgainstPython) {
    decimal seventh = decimal(1).div_precision(7, 1000);
    EXPECT_EQ(str(seventh.unscaled().to_string()), decimal_vectors::seventh_1000);
    EXPECT_EQ(seventh.scale(), 1000);
    decimal root2 = decimal(2).sqrt(1000);
    EXPECT_EQ(str(root2.unscaled().to_string()), decimal_vectors::root2_1000);
    EXPECT_EQ(root2.scale(), 999);
    decimal root10 = decimal(10).sqrt(2000);
    EXPECT_EQ(str(root10.unscaled().to_string()), decimal_vectors::root10_2000);
    // the root squared is within half a unit of the last digit, squared
    decimal back = root2 * root2;
    EXPECT_LT((back - 2).abs(), D("1e-998"));
}

TEST(Decimal_Tests, ParseErrors) {
    struct Case {
        const char* text;
        size_t offset;
        const char* message;
    };
    const Case cases[] = {
        {"", 0, "empty text at byte 0"},
        {"-", 1, "no digits after the sign at byte 1"},
        {".", 1, "no digits after the sign at byte 1"},
        {"1.2.3", 3, "not a digit in base 10 at byte 3"},
        {"1e", 2, "no digits after the sign at byte 2"},
        {"1e+", 3, "no digits after the sign at byte 3"},
        {"1e5x", 3, "not a digit in base 10 at byte 3"},
        {" 1", 0, "not a digit in base 10 at byte 0"},
        {"1 ", 1, "not a digit in base 10 at byte 1"},
        {"1,5", 1, "not a digit in base 10 at byte 1"},
        {"1e1000001", 2, "an exponent past a million at byte 2"},
        {"1e-99999999999999999999999", 2, "an exponent past a million at byte 2"},
        {"12345e999997", 6, "an exponent past a million at byte 6"},
        {"0.00001e-999996", 8, "an exponent past a million at byte 8"},
        {"-NaN", 1, "not a digit in base 10 at byte 1"},
        {"nanx", 0, "not a digit in base 10 at byte 0"},
        {"infinit", 0, "not a digit in base 10 at byte 0"},
        {"+e5", 1, "not a digit in base 10 at byte 1"},
        {"0x10", 1, "not a digit in base 10 at byte 1"},
        {"1/2", 1, "not a digit in base 10 at byte 1"},
    };
    for (auto& c : cases) {
        auto d = decimal::parse(c.text);
        ASSERT_FALSE(d.has_value()) << c.text;
        EXPECT_EQ(d.error().offset(), c.offset) << c.text;
        EXPECT_EQ(str(d.error().message()), c.message) << c.text;
    }
    // the limit is on the place of the first digit: what to_scientific
    // writes reads back at both ends of it
    for (const char* text : {"1e1000000", "12345e999996", "1.2345e1000000", "-0.00001e-999995", "0e-1000000",
                             "9.99e-1000000"}) {
        auto d = decimal::parse(text);
        ASSERT_TRUE(d.has_value()) << text;
        auto back = decimal::parse(d->to_scientific());
        ASSERT_TRUE(back.has_value()) << text;
        EXPECT_TRUE(back->identical(*d)) << text;
    }
    EXPECT_FALSE(decimal::parse("0e-1000001").has_value());
    EXPECT_FALSE(decimal::parse(("0." + std::string(1000000, '0') + "1").c_str()).has_value());
    EXPECT_TRUE(decimal::parse(("0." + std::string(999999, '0') + "1").c_str()).has_value());
    EXPECT_THROW(decimal(string("1.2.3")), bad_expected_access<math::parse_error>);
    // a text with an embedded NUL is not read past it
    EXPECT_FALSE(decimal::parse(string(std::string("12\0" "3", 4))).has_value());
}

TEST(Decimal_Tests, NanAndInfinityAsPostgreSql) {
    decimal nan = decimal::nan();
    decimal inf = decimal::infinity();
    decimal one(1);
    EXPECT_TRUE(D("NaN").is_nan());
    EXPECT_TRUE(D("nan").is_nan());
    EXPECT_TRUE(D("Infinity") == inf);
    EXPECT_TRUE(D("-inf") == -inf);
    EXPECT_TRUE(D("+INFINITY") == inf);
    EXPECT_EQ(str(nan.to_string()), "NaN");
    EXPECT_EQ(str(inf.to_string()), "Infinity");
    EXPECT_EQ(str((-inf).to_string()), "-Infinity");
    EXPECT_EQ(str(inf.to_scientific()), "Infinity");
    // arithmetic
    EXPECT_TRUE((nan + one).is_nan());
    EXPECT_TRUE((one - nan).is_nan());
    EXPECT_TRUE((nan * 0).is_nan());
    EXPECT_TRUE((inf + one) == inf);
    EXPECT_TRUE((one - inf) == -inf);
    EXPECT_TRUE((inf + inf) == inf);
    EXPECT_TRUE((inf - inf).is_nan());
    EXPECT_TRUE((-inf - inf) == -inf);
    EXPECT_TRUE((inf * D("-2.5")) == -inf);
    EXPECT_TRUE((inf * -inf) == -inf);
    EXPECT_TRUE((inf * 0).is_nan());
    EXPECT_TRUE((D("0.00") * -inf).is_nan());
    EXPECT_TRUE(inf.div(2, 3) == inf);
    EXPECT_TRUE(inf.div(-2, 3) == -inf);
    EXPECT_TRUE(inf.div(inf, 3).is_nan());
    EXPECT_TRUE(is(one.div(inf, 3), "0", 3));
    EXPECT_TRUE(nan.div(0, 3).is_nan());
    EXPECT_THROW(inf.div(0, 3), std::domain_error);
    EXPECT_TRUE(inf.div_precision(-3, 5) == -inf);
    EXPECT_TRUE(one.div_precision(inf, 5) == 0);
    EXPECT_TRUE((inf % 2).is_nan());
    EXPECT_TRUE((D("2.5") % inf).identical(D("2.5")));
    EXPECT_TRUE((nan % 0).is_nan());
    EXPECT_THROW(inf % 0, std::domain_error);
    EXPECT_TRUE(inf.sqrt(5) == inf);
    EXPECT_TRUE(nan.sqrt(5).is_nan());
    EXPECT_THROW((-inf).sqrt(5), std::domain_error);
    EXPECT_TRUE(inf.rescale(2) == inf);
    EXPECT_TRUE(inf.round_precision(2) == inf);
    EXPECT_TRUE(nan.trim_scale().is_nan());
    EXPECT_TRUE((-inf).abs() == inf);
    EXPECT_TRUE((-nan).is_nan());
    EXPECT_EQ(inf.sign(), 1);
    EXPECT_EQ((-inf).sign(), -1);
    EXPECT_EQ(nan.sign(), 0);
    EXPECT_FALSE(inf.is_finite());
    EXPECT_TRUE(inf.is_infinite());
    EXPECT_FALSE(nan.is_infinite());
    EXPECT_EQ(nan.unscaled(), 0);
    EXPECT_EQ(inf.scale(), 0);
    EXPECT_FALSE(inf.to_int64().has_value());
    EXPECT_THROW(inf.to_big_integer(), std::domain_error);
    EXPECT_THROW(nan.to_big_integer(), std::domain_error);
    // the total order: -inf < finite < +inf < NaN, NaN equal to itself
    sgcl::vector<decimal> ordered = {-inf, D("-1e1000"), D("-1.5"), D("0"), D("1e-1000"), D("2"), D("1e999999"), inf, nan};
    for (size_t i = 0; i < ordered.size(); ++i) {
        for (size_t j = 0; j < ordered.size(); ++j) {
            EXPECT_EQ(ordered[i] < ordered[j], i < j) << i << " " << j;
            EXPECT_EQ(ordered[i] == ordered[j], i == j) << i << " " << j;
        }
    }
    EXPECT_EQ(std::hash<decimal>()(nan), std::hash<decimal>()(D("NaN")));
    EXPECT_NE(std::hash<decimal>()(inf), std::hash<decimal>()(-inf));
    EXPECT_TRUE(nan.identical(decimal::nan()));
    EXPECT_FALSE(inf.identical(-inf));
}

TEST(Decimal_Tests, EqualityOfValuesAndTheirHash) {
    std::hash<decimal> h;
    EXPECT_TRUE(D("1.0") == D("1.00"));
    EXPECT_FALSE(D("1.0").identical(D("1.00")));
    EXPECT_TRUE(D("1.0").identical(D("1.0")));
    EXPECT_EQ(h(D("1.0")), h(D("1.00")));
    EXPECT_EQ(h(D("1.5e3")), h(D("1500.000")));
    EXPECT_EQ(h(D("0")), h(D("0.0000")));
    EXPECT_EQ(h(D("0")), h(D("0e5")));
    EXPECT_EQ(h(D("-12.5")), h(D("-1.25e1")));
    EXPECT_NE(h(D("12.5")), h(D("-12.5")));
    EXPECT_NE(h(D("1")), h(D("10")));
    EXPECT_TRUE(decimal(1, 2000000000) != D("0"));
    EXPECT_TRUE(decimal(1, 2000000000) < decimal(1, 1999999999));
    EXPECT_TRUE(decimal(-1, -2000000000) < decimal(-1, 2000000000));
    // hash and equality agree over values written at many scales
    std::mt19937_64 rng(7);
    for (int i = 0; i < 3000; ++i) {
        big_integer u = big_integer(int64_t(rng() >> (rng() % 64))) * (rng() % 2 ? 1 : -1);
        if (rng() % 3 == 0) {
            u = u * big_integer(rng()) * big_integer(rng());
        }
        int32_t s = int32_t(rng() % 60) - 20;
        decimal a(u, s);
        int32_t up = int32_t(rng() % 40);
        decimal b = a.rescale(s + up);
        ASSERT_TRUE(a == b);
        ASSERT_EQ(a <=> b, std::weak_ordering::equivalent);
        ASSERT_EQ(h(a), h(b));
        ASSERT_TRUE(a.trim_scale() == a);
        ASSERT_TRUE(b.trim_scale().identical(a.trim_scale()));
        decimal c = b + decimal(1, s + up + 1);   // the smallest step at a finer scale
        ASSERT_TRUE(c != a);
        ASSERT_TRUE(c > a);
    }
}

TEST(Decimal_Tests, RoundingUnnecessaryAsserts) {
    EXPECT_TRUE(is(D("1.500").rescale(1, rounding::unnecessary), "15", 1));
    EXPECT_THROW(D("1.55").rescale(1, rounding::unnecessary), std::domain_error);
    EXPECT_TRUE(is(D("1").div(4, 2, rounding::unnecessary), "25", 2));
    EXPECT_THROW(D("1").div(3, 20, rounding::unnecessary), std::domain_error);
    EXPECT_TRUE(is(D("1").div_precision(8, 5, rounding::unnecessary), "125", 3));
    EXPECT_THROW(D("1").div_precision(3, 5, rounding::unnecessary), std::domain_error);
    EXPECT_TRUE(is(D("123400").round_precision(4, rounding::unnecessary), "1234", -2));
    EXPECT_THROW(D("123401").round_precision(4, rounding::unnecessary), std::domain_error);
    EXPECT_TRUE(is(D("6.25").sqrt(3, rounding::unnecessary), "25", 1));
    EXPECT_THROW(D("2").sqrt(3, rounding::unnecessary), std::domain_error);
    EXPECT_EQ(D("7.00").to_big_integer(rounding::unnecessary), 7);
    EXPECT_THROW(D("7.01").to_big_integer(rounding::unnecessary), std::domain_error);
    EXPECT_THROW(decimal(rational(1, 3), 5, rounding::unnecessary), std::domain_error);
}

// The modes Python does not take for a square root: the root rounded each
// way is the one on that side of the exact root
TEST(Decimal_Tests, SquareRootInEveryMode) {
    std::mt19937_64 rng(11);
    for (int i = 0; i < 400; ++i) {
        decimal x(big_integer(rng() >> (rng() % 60)) * big_integer(int64_t(rng() % 1000 + 1)), int32_t(rng() % 30) - 10);
        int digits = int(rng() % 30) + 1;
        decimal down = x.sqrt(digits, rounding::down);
        decimal up = x.sqrt(digits, rounding::up);
        decimal even = x.sqrt(digits, rounding::half_even);
        rational exact = x.to_rational();
        ASSERT_LE(down.to_rational() * down.to_rational(), exact) << show(x);
        ASSERT_GE(up.to_rational() * up.to_rational(), exact) << show(x);
        ASSERT_TRUE(x.sqrt(digits, rounding::floor) == down);
        ASSERT_TRUE(x.sqrt(digits, rounding::ceiling) == up);
        if (down == up) {
            ASSERT_TRUE(down * down == x);
        } else {
            // one unit of the last digit apart, the nearest of the two the half modes'
            decimal unit(1, std::max(down.scale(), up.scale()));
            ASSERT_TRUE(up - down == unit || up - down == unit * 10) << show(x) << " " << show(down) << " " << show(up);
            rational mid = (down.to_rational() + up.to_rational()) / 2;
            decimal nearest = mid * mid < exact ? up : down;
            ASSERT_TRUE(even == nearest);
            ASSERT_TRUE(x.sqrt(digits, rounding::half_up) == nearest);
            ASSERT_TRUE(x.sqrt(digits, rounding::half_down) == nearest);
        }
    }
}

TEST(Decimal_Tests, ErrorsOfTheProgram) {
    decimal x("12.5");
    EXPECT_THROW(x.div(0, 2), std::domain_error);
    EXPECT_THROW(x.div(D("0.000"), 2), std::domain_error);
    EXPECT_THROW(x.div_precision(0, 2), std::domain_error);
    EXPECT_THROW(x % 0, std::domain_error);
    EXPECT_THROW(x.div_rem(0), std::domain_error);
    EXPECT_THROW(x.div_precision(3, 0), std::domain_error);
    EXPECT_THROW(x.round_precision(0), std::domain_error);
    EXPECT_THROW(x.round_precision(-5), std::domain_error);
    EXPECT_THROW(x.sqrt(0), std::domain_error);
    EXPECT_THROW(D("-0.01").sqrt(5), std::domain_error);
    EXPECT_TRUE(is(D("-0.00").sqrt(5), "0", 1));
    // scales past int32_t
    decimal tiny(1, INT32_MAX);
    EXPECT_THROW(tiny * tiny, std::length_error);
    EXPECT_THROW(decimal(1, INT32_MIN) * decimal(1, -1), std::length_error);
    EXPECT_THROW(tiny.div_precision(decimal(1, INT32_MIN), 3), std::length_error);
    EXPECT_TRUE(is(tiny * decimal(1, INT32_MIN), "1", -1));
}

// PostgreSQL's NUMERIC: up to 131072 digits before the point and 16383 after
TEST(Decimal_Tests, PostgreSqlRange) {
    std::mt19937_64 rng(13);
    std::string whole(131072, '0');
    std::string fraction(16383, '0');
    for (auto& c : whole) {
        c = char('0' + rng() % 10);
    }
    for (auto& c : fraction) {
        c = char('0' + rng() % 10);
    }
    whole[0] = '9';
    fraction.back() = '7';
    std::string text = "-" + whole + "." + fraction;
    decimal big = D(text.c_str());
    EXPECT_EQ(big.scale(), 16383);
    EXPECT_EQ(big.precision(), 131072u + 16383u);
    EXPECT_EQ(str(big.to_string()), text);
    decimal smallest = decimal(1, 16383);
    EXPECT_TRUE(((big + smallest) - smallest).identical(big));
    EXPECT_TRUE(big + smallest > big);
    EXPECT_TRUE(big.rescale(0, rounding::down) == D(("-" + whole).c_str()));
    EXPECT_TRUE(big.rescale(0, rounding::floor) == D(("-" + whole).c_str()) - 1);
    decimal square = big * big;
    EXPECT_EQ(square.scale(), 2 * 16383);
    EXPECT_TRUE(square.sqrt(int32_t(big.precision())) == big.abs());
    EXPECT_TRUE(square.div(big, 16383) == big);
    EXPECT_EQ(big.to_double(), -HUGE_VAL);
    std::string sci = str(big.to_scientific());
    EXPECT_EQ(sci.substr(0, 4), "-9." + whole.substr(1, 1));
    EXPECT_EQ(sci.substr(sci.size() - 8), "e+131071");
}

TEST(Decimal_Tests, Boundaries) {
    // default and moved-from
    decimal zero;
    EXPECT_TRUE(is(zero, "0", 0));
    EXPECT_EQ(str(zero.to_string()), "0");
    EXPECT_EQ(str(zero.to_scientific()), "0e+0");
    EXPECT_EQ(zero.precision(), 1u);
    EXPECT_EQ(zero.sign(), 0);
    decimal big = D("123456789012345678901234567890.5");
    decimal taken = std::move(big);
    EXPECT_TRUE(taken == D("123456789012345678901234567890.5"));
    EXPECT_TRUE(big.is_finite());   // moved from: a valid value (zero at its scale)
    EXPECT_EQ(big.sign(), 0);
    big = taken;
    EXPECT_TRUE(big.identical(taken));
    // aliasing
    decimal a = D("1.25");
    a += a;
    EXPECT_TRUE(is(a, "250", 2));
    a *= a;
    EXPECT_TRUE(is(a, "62500", 4));
    a -= a;
    EXPECT_TRUE(is(a, "0", 4));
    decimal b = D("7.5");
    b %= b;
    EXPECT_TRUE(is(b, "0", 1));
    EXPECT_TRUE(is(D("7.5").div(D("7.5"), 2), "100", 2));
    // the edges of the small form: sums and products across int64_t
    const char* edges[] = {"9223372036854775807", "-9223372036854775808", "922337203685477580.7", "-0.9223372036854775808",
                           "999999999999999999", "1", "-1", "0.000000000000000001"};
    for (auto x : edges) {
        for (auto y : edges) {
            decimal p = D(x);
            decimal q = D(y);
            rational rp = p.to_rational();
            rational rq = q.to_rational();
            ASSERT_EQ((p + q).to_rational(), rp + rq) << x << " " << y;
            ASSERT_EQ((p - q).to_rational(), rp - rq) << x << " " << y;
            ASSERT_EQ((p * q).to_rational(), rp * rq) << x << " " << y;
            ASSERT_EQ(p <=> q, rp <=> rq) << x << " " << y;
            ASSERT_EQ(p.div(q, 5).to_rational(), decimal(rp / rq, 5).to_rational()) << x << " " << y;
        }
    }
    // to_int64 at its edges
    EXPECT_EQ(D("9223372036854775807").to_int64(), INT64_MAX);
    EXPECT_EQ(D("-9223372036854775808").to_int64(), INT64_MIN);
    EXPECT_FALSE(D("9223372036854775808").to_int64().has_value());
    EXPECT_EQ(D("922337203685477580.700").to_int64(), nullopt);
    EXPECT_EQ(D("922337203685477580e1").to_int64(), 9223372036854775800);
    EXPECT_FALSE(D("922337203685477581e1").to_int64().has_value());
    EXPECT_EQ(D("2.000").to_int64(), 2);
    EXPECT_FALSE(D("2.5").to_int64().has_value());
    EXPECT_EQ(D("0e-50").to_int64(), 0);
    EXPECT_EQ(D("0e50").to_int64(), 0);
    EXPECT_FALSE(D("1e19").to_int64().has_value());
    EXPECT_FALSE(D("1e-50").to_int64().has_value());
    EXPECT_EQ(D("12345678901234567890123e-5").to_int64(), nullopt);
    EXPECT_EQ(D("1234567890123456789000000e-6").to_int64(), 1234567890123456789);
    // to_big_integer in each mode
    EXPECT_EQ(D("-2.5").to_big_integer(), -2);
    EXPECT_EQ(D("-2.5").to_big_integer(rounding::floor), -3);
    EXPECT_EQ(D("-2.5").to_big_integer(rounding::half_even), -2);
    EXPECT_EQ(D("-2.5").to_big_integer(rounding::half_up), -3);
    EXPECT_EQ(D("1.5e30").to_big_integer(), B("1500000000000000000000000000000"));
    // a rescale far below the digits, and far above for zero
    EXPECT_TRUE(is(D("123.45").rescale(-1000000000), "0", -1000000000));
    EXPECT_TRUE(is(D("123.45").rescale(-1000000000, rounding::up), "1", -1000000000));
    EXPECT_TRUE(is(D("-123.45").rescale(-1000000000, rounding::floor), "-1", -1000000000));
    EXPECT_TRUE(is(D("1").div(D("7e1000000"), 2), "0", 2));
    EXPECT_TRUE(is(D("1").div(D("7e1000000"), 2, rounding::ceiling), "1", 2));
    // sums at scales far apart, both ways
    decimal far = D("1e-1000") + D("1e1000");
    EXPECT_EQ(far.precision(), 2001u);
    EXPECT_TRUE(far - D("1e1000") == D("1e-1000"));
    // precision of values at the edges of the digit count
    EXPECT_EQ(D("9999999999999999999").precision(), 19u);
    EXPECT_EQ(D("10000000000000000000").precision(), 20u);
    EXPECT_EQ(D("99999999999999999999").precision(), 20u);
    EXPECT_EQ(D("18446744073709551615").precision(), 20u);
    EXPECT_EQ(D("18446744073709551616").precision(), 20u);
    for (int k = 1; k < 400; k += 7) {
        std::string nines(size_t(k), '9');
        EXPECT_EQ(D(nines.c_str()).precision(), size_t(k));
        EXPECT_EQ(D(("1" + std::string(size_t(k), '0')).c_str()).precision(), size_t(k + 1));
    }
}

TEST(Decimal_Tests, Format) {
    EXPECT_EQ(str(txt::format("{}", D("-1.50"))), "-1.50");
    EXPECT_EQ(str(txt::format("{}", D("1.5e3"))), "1500");
    EXPECT_EQ(str(txt::format("{:.2f}", D("2.675"))), "2.68");
    EXPECT_EQ(str(txt::format("{:.2f}", D("2.665"))), "2.66");
    EXPECT_EQ(str(txt::format("{:.0f}", D("-0.5"))), "-0");
    EXPECT_EQ(str(txt::format("{:.3}", D("1"))), "1.000");
    EXPECT_EQ(str(txt::format("{:f}", D("1.2300"))), "1.2300");
    EXPECT_EQ(str(txt::format("{:e}", D("-0.00123"))), "-1.23e-3");
    EXPECT_EQ(str(txt::format("{:.2e}", D("123456"))), "1.23e+5");
    EXPECT_EQ(str(txt::format("{:.1e}", D("9.99"))), "1.0e+1");
    EXPECT_EQ(str(txt::format("{:.3e}", D("1.5"))), "1.500e+0");
    EXPECT_EQ(str(txt::format("{:.3e}", D("0"))), "0.000e+3");
    EXPECT_EQ(str(txt::format("{:.3e}", D("0.00"))), "0.000e+1");
    EXPECT_EQ(str(txt::format("{:.0e}", D("0"))), "0e+0");
    EXPECT_EQ(str(txt::format("{:+}", D("1.5"))), "+1.5");
    EXPECT_EQ(str(txt::format("{: }", D("1.5"))), " 1.5");
    EXPECT_EQ(str(txt::format("{:>8}", D("-1.5"))), "    -1.5");
    EXPECT_EQ(str(txt::format("{:<8}|", D("1.5"))), "1.5     |");
    EXPECT_EQ(str(txt::format("{:08.2f}", D("-3.14159"))), "-0003.14");
    EXPECT_EQ(str(txt::format("{:*^9}", D("1.5"))), "***1.5***");
    EXPECT_EQ(str(txt::format("{:+.2f}", decimal::infinity())), "+Infinity");
    EXPECT_EQ(str(txt::format("{:e}", -decimal::infinity())), "-Infinity");
    EXPECT_EQ(str(txt::format("{}", decimal::nan())), "NaN");
    std::ostringstream os;
    os << D("-0.0012") << " " << D("1e2");
    EXPECT_EQ(os.str(), "-0.0012 100");
}

// The laws over random values: each operation against rational's exact
// arithmetic, rounded where the operation rounds by decimal's own
// constructor from a fraction (checked against Python above)
TEST(Decimal_Tests, LawsAgainstRational) {
    std::mt19937_64 rng(17);
    auto value = [&] {
        big_integer u(int64_t(rng() >> (rng() % 64)));
        if (rng() % 4 == 0) {
            u = u * big_integer(rng()) * big_integer(rng() >> (rng() % 64));
        }
        if (rng() % 2) {
            u = -u;
        }
        return decimal(u, int32_t(rng() % 50) - 15);
    };
    for (int i = 0; i < 4000; ++i) {
        decimal a = value();
        decimal b = value();
        rational ra = a.to_rational();
        rational rb = b.to_rational();
        ASSERT_EQ((a + b).to_rational(), ra + rb);
        ASSERT_EQ((a - b).to_rational(), ra - rb);
        ASSERT_EQ((a * b).to_rational(), ra * rb);
        ASSERT_EQ((a + b).scale(), std::max(a.scale(), b.scale()));
        ASSERT_EQ((a * b).scale(), a.scale() + b.scale());
        ASSERT_EQ(a <=> b, ra <=> rb);
        ASSERT_TRUE((a + b).identical(b + a));
        if (b.sign() == 0) {
            continue;
        }
        auto [q, r] = a.div_rem(b);
        ASSERT_EQ(q * b + r, a);
        ASSERT_LT(r.abs(), b.abs());
        ASSERT_TRUE(r.sign() == 0 || r.sign() == a.sign());
        rounding mode = Modes[rng() % 7];
        int32_t s = int32_t(rng() % 40) - 10;
        ASSERT_TRUE(a.div(b, s, mode).identical(decimal(ra / rb, s, mode)));
        ASSERT_TRUE(a.rescale(s, mode).identical(decimal(ra, s, mode)));
        int32_t digits = int32_t(rng() % 40) + 1;
        decimal p = a.div_precision(b, digits, mode);
        ASSERT_LE(p.precision(), size_t(digits));
        // within one unit of the last digit of the exact quotient
        rational unit = decimal(1, p.scale()).to_rational();
        ASSERT_LT((p.to_rational() - ra / rb).abs(), unit);
        ASSERT_TRUE(p.identical(p.round_precision(digits)));
    }
}

// The values of up to eighteen digits stay in the big_integer's word: their
// arithmetic, comparison, rounding and text allocate nothing managed but
// the strings asked for
TEST(Decimal_Tests, SmallValuesAllocateNothing) {
    decimal a = D("12345.678");
    decimal b = D("-0.25");
    decimal c = D("99.5");
    volatile int64_t keep = 0;
    size_t bytes = managed_bytes_of(20000, [&] {
        decimal s = a + b - c;
        decimal p = a * b;
        decimal q = a.div(b, 4, rounding::half_up);
        decimal r = a.rescale(1);
        decimal t = a.div_precision(c, 12);
        decimal m = a % c;
        keep = keep + (s > p) + (q < r) + (t == m) + (a == decimal(12345678, 3)) + int64_t(std::hash<decimal>()(a) & 1)
             + int64_t(a.precision()) + *r.unscaled().to_int64() + int64_t(a.to_double());
    });
    EXPECT_EQ(bytes, 0u);
}
