//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// math::big_float against Go's math/big.Float (float_vectors.h, from
// tools/math_float_oracle.go): parsing at precisions 1 to 300 in each of
// Go's six modes, the four operations over pairs of every precision and
// mode with signed zeros and infinities, square roots, Text in every
// format, Float64, Int, SetRat — every value compared exactly, in Go's 'p'
// format. Then the laws against rational's exact arithmetic over random
// values (each operation is the exact result rounded once), the shortest
// text read back, the modes Go has not (half_down, unnecessary), NaN-making
// operations, the exponent's range, and the conversions to decimal.
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "float_vectors.h"

#include <cmath>
#include <cstring>
#include <random>
#include <sstream>
#include <string>

using math::big_float;
using math::big_integer;
using math::decimal;
using math::rational;

namespace {
    big_float F(const char* text, unsigned precision, int mode) {
        auto v = big_float::parse(text, precision, rounding(mode));
        EXPECT_TRUE(v.has_value()) << text;
        return v ? *v : big_float();
    }

    // Go's Text of a value, the 'p' format for an exact comparison
    std::string T(const big_float& v, char format, int digits) {
        auto s = math::detail::BigFloatAccess::text(v, format, digits);
        return std::string(s.data(), s.size());
    }

    std::string P(const big_float& v) {
        return T(v, 'p', 0);
    }

    std::string str(const string& s) {
        return std::string(s.data(), s.size());
    }
}

TEST(BigFloat_Tests, ParseAgainstGo) {
    for (auto& t : float_vectors::parses) {
        big_float v = F(t.text, t.precision, t.mode);
        ASSERT_EQ(P(v), t.value) << t.text << " at " << t.precision << " mode " << t.mode;
        ASSERT_EQ(v.precision(), t.precision);
        ASSERT_EQ(v.mode(), rounding(t.mode));
    }
}

TEST(BigFloat_Tests, ArithmeticAgainstGo) {
    size_t thrown = 0;
    for (auto& t : float_vectors::ops) {
        big_float a = F(t.a, t.pa, t.mode);
        big_float b = F(t.b, t.pb, 0);
        std::string where = std::string(t.a) + " " + t.b + " mode " + std::to_string(t.mode);
        auto check = [&](const char* want, auto op, const char* name) {
            if (!*want) {
                EXPECT_THROW(op(), std::domain_error) << name << " " << where;
                ++thrown;
                return;
            }
            big_float r = op();
            EXPECT_EQ(P(r), want) << name << " " << where;
            EXPECT_EQ(r.precision(), std::max(t.pa, t.pb)) << name << " " << where;
        };
        check(t.sum, [&] { return a + b; }, "sum");
        check(t.difference, [&] { return a - b; }, "difference");
        check(t.product, [&] { return a * b; }, "product");
        check(t.quotient, [&] { return a / b; }, "quotient");
    }
    EXPECT_GT(thrown, 0u);
}

TEST(BigFloat_Tests, SqrtAgainstGo) {
    for (auto& t : float_vectors::roots) {
        big_float x = F(t.value, t.precision, t.mode);
        EXPECT_EQ(P(x.sqrt()), t.root) << t.value << " at " << t.precision << " mode " << t.mode;
    }
}

TEST(BigFloat_Tests, TextAgainstGo) {
    size_t go_shortest_wrong = 0;
    for (auto& t : float_vectors::texts) {
        big_float x = F(t.value, t.precision, 0);
        // Go's shortest of a power of two may read back as the value below
        // (its interval has half a unit there, not a quarter): ours then
        // differs, reads back, and is no longer
        auto go_reads_back = big_float::parse(t.g, t.precision);
        if (go_reads_back && *go_reads_back == x) {
            EXPECT_EQ(T(x, 'g', -1), t.g) << t.value;
            EXPECT_EQ(T(x, 'e', -1), t.e) << t.value;
            EXPECT_EQ(str(x.to_scientific()), t.e) << t.value;
            EXPECT_EQ(T(x, 'f', -1), t.f) << t.value;
            // to_string: Go's %g but plain up to an exponent of 20
            std::string want = t.g;
            if (want.find("e+") != std::string::npos && std::atoi(want.c_str() + want.find("e+") + 2) < 21) {
                want = t.f;
            }
            EXPECT_EQ(str(x.to_string()), want) << t.value;
        } else {
            ++go_shortest_wrong;
            EXPECT_LE(x.to_scientific().size(), std::strlen(t.e) + 1) << t.value;
        }
        EXPECT_EQ(T(x, 'g', 10), t.g10) << t.value;
        EXPECT_EQ(T(x, 'e', 10), t.e10) << t.value;
        EXPECT_EQ(T(x, 'f', 10), t.f10) << t.value;
        EXPECT_EQ(T(x, 'x', -1), t.x) << t.value;
        EXPECT_EQ(T(x, 'x', 5), t.x5) << t.value;
        // to_hex: Go's x with the exponent as C's %a writes it; it reads back
        auto hex = big_float::parse(x.to_hex(), t.precision);
        ASSERT_TRUE(hex.has_value()) << t.value;
        EXPECT_EQ(P(*hex), P(x)) << t.value;
        double d = x.to_double();
        EXPECT_EQ(d, t.to_double) << t.value;
        EXPECT_EQ(std::signbit(d), std::signbit(t.to_double)) << t.value;
        if (x.is_infinite()) {
            EXPECT_THROW(x.to_big_integer(), std::domain_error);
        } else {
            EXPECT_EQ(str(x.to_big_integer().to_string()), t.integer) << t.value;
        }
        // the shortest reads back to the same value
        auto back = big_float::parse(x.to_string(), t.precision);
        ASSERT_TRUE(back.has_value()) << t.value;
        EXPECT_EQ(P(*back), P(x)) << t.value;
    }
    EXPECT_LT(go_shortest_wrong, 10u);
}

TEST(BigFloat_Tests, RationalsAgainstGo) {
    for (auto& t : float_vectors::rats) {
        rational r(string(t.fraction));
        EXPECT_EQ(P(big_float(r, t.precision, rounding(t.mode))), t.value) << t.fraction << " at " << t.precision;
    }
}

// Every result is the exact one rounded once: against rational's exact
// arithmetic rounded by the constructor from a fraction
TEST(BigFloat_Tests, LawsAgainstRational) {
    std::mt19937_64 rng(31);
    auto value = [&](unsigned precision) {
        big_integer m = big_integer(rng()) * big_integer(rng() >> (rng() % 64));
        if (rng() % 2) {
            m = -m;
        }
        int shift = int(rng() % 400) - 200;
        rational r = shift >= 0 ? rational(m << shift) : rational(m, big_integer(1) << -shift);
        return big_float(r, precision, rounding::half_even);
    };
    constexpr rounding Modes[] = {rounding::half_even, rounding::half_up, rounding::half_down, rounding::up,
                                  rounding::down,      rounding::ceiling, rounding::floor};
    for (int i = 0; i < 3000; ++i) {
        unsigned pa = unsigned(rng() % 150) + 2;
        unsigned pb = unsigned(rng() % 150) + 2;
        rounding mode = Modes[rng() % 7];
        big_float a = big_float(value(pa), pa, mode);
        big_float b = value(pb);
        unsigned p = std::max(pa, pb);
        rational ra = a.to_rational();
        rational rb = b.to_rational();
        auto exact = [&](const rational& r) {
            return r == 0 ? big_float(big_integer(0), p, mode) : big_float(r, p, mode);
        };
        // a sum of zero is a zero of the mode's sign: the values compared there
        auto same = [&](const big_float& got, const rational& r) {
            return r == 0 ? got.sign() == 0 : P(got) == P(exact(r));
        };
        ASSERT_TRUE(same(a + b, ra + rb)) << i;
        ASSERT_TRUE(same(a - b, ra - rb)) << i;
        ASSERT_TRUE(same(a * b, ra * rb)) << i;
        if (b.sign() != 0) {
            ASSERT_TRUE(same(a / b, ra / rb)) << i;
        }
        if (a.sign() > 0) {
            // the root rounded down squares to at most a, rounded up to at least
            big_float down = big_float(a, pa, rounding::down).sqrt();
            big_float up = big_float(a, pa, rounding::up).sqrt();
            ASSERT_LE(down.to_rational() * down.to_rational(), ra) << i;
            ASSERT_GE(up.to_rational() * up.to_rational(), ra) << i;
        }
        ASSERT_EQ((a <=> b) == 0, ra == rb);
        ASSERT_EQ(a < b, ra < rb);
        // the shortest text reads back
        auto back = big_float::parse(a.to_string(), pa);
        ASSERT_TRUE(back.has_value());
        ASSERT_EQ(*back, a) << str(a.to_string());
    }
}

TEST(BigFloat_Tests, DoublesExactlyAndBack) {
    std::mt19937_64 rng(37);
    for (int i = 0; i < 20000; ++i) {
        uint64_t bits = rng();
        double x;
        std::memcpy(&x, &bits, sizeof x);
        if (std::isnan(x)) {
            EXPECT_THROW(big_float{x}, std::domain_error);
            continue;
        }
        big_float f(x);
        ASSERT_EQ(f.precision(), 53u);
        ASSERT_EQ(f.to_double(), x) << bits;
        ASSERT_EQ(std::signbit(f.to_double()), std::signbit(x));
        if (std::isfinite(x)) {
            ASSERT_EQ(f.to_rational(), rational(x)) << bits;
        }
        if (std::isfinite(x) && (x == 0 || std::fabs(x) >= 0x1p-1022)) {
            // the shortest text is the double's own shortest (but for a
            // subnormal, whose shortest picks it among doubles of fewer bits)
            char buf[64];
            auto r = std::to_chars(buf, buf + sizeof buf, x);
            auto back = big_float::parse(string(std::string(buf, r.ptr)), 53);
            ASSERT_TRUE(back.has_value());
            ASSERT_EQ(*back, f);
        }
    }
}

TEST(BigFloat_Tests, NoNaN) {
    big_float inf = big_float::infinity();
    big_float zero;
    big_float one(1);
    EXPECT_THROW(inf - inf, std::domain_error);
    EXPECT_THROW(inf + -inf, std::domain_error);
    EXPECT_THROW(inf * zero, std::domain_error);
    EXPECT_THROW(zero * -inf, std::domain_error);
    EXPECT_THROW(zero / zero, std::domain_error);
    EXPECT_THROW(inf / inf, std::domain_error);
    EXPECT_THROW((-one).sqrt(), std::domain_error);
    EXPECT_THROW((-inf).sqrt(), std::domain_error);
    EXPECT_THROW(big_float(std::nan("")), std::domain_error);
    EXPECT_THROW(big_float(decimal::nan(), 10), std::domain_error);
    EXPECT_TRUE((one / zero).is_infinite());
    EXPECT_EQ((-one / zero).sign(), -1);
    EXPECT_EQ((one / inf).sign(), 0);
    EXPECT_TRUE((-one / inf).signbit());
    EXPECT_TRUE(big_float(-0.0).sqrt().signbit());
    EXPECT_EQ(inf.sqrt(), inf);
    EXPECT_TRUE(big_float(decimal::infinity(), 10).is_infinite());
    EXPECT_THROW(inf.to_rational(), std::domain_error);
    EXPECT_TRUE(inf.to_decimal().is_infinite());
    EXPECT_EQ(str(inf.to_string()), "+Inf");
    EXPECT_EQ(str((-inf).to_scientific()), "-Inf");
    EXPECT_EQ(str(inf.to_hex()), "+Inf");
}

TEST(BigFloat_Tests, Boundaries) {
    // the default: +0 at precision 0, which takes the other's
    big_float zero;
    EXPECT_EQ(zero.precision(), 0u);
    EXPECT_EQ(zero.sign(), 0);
    EXPECT_FALSE(zero.signbit());
    EXPECT_EQ(str(zero.to_string()), "0");
    big_float sum = zero + big_float(1, 100);
    EXPECT_EQ(sum.precision(), 100u);
    EXPECT_EQ((big_float() + big_float()).precision(), 0u);
    // integers at 64 bits, big_integers at their length, doubles at 53
    EXPECT_EQ(big_float(7).precision(), 64u);
    EXPECT_EQ(big_float(big_integer(1) << 200).precision(), 201u);
    EXPECT_EQ(big_float(0.5).precision(), 53u);
    EXPECT_EQ(big_float(__int128(1) << 100).precision(), 128u);
    // a move is the copy
    big_float a = big_float::parse("3.25", 70).value();
    big_float b = std::move(a);
    EXPECT_EQ(a, b);
    EXPECT_EQ(str(a.to_string()), "3.25");
    // signed zeros: equal, the sign kept and rounded as IEEE
    big_float nz(-0.0);
    EXPECT_EQ(nz, zero);
    EXPECT_TRUE(nz.signbit());
    EXPECT_TRUE((nz + nz).signbit());
    EXPECT_FALSE((nz + zero).signbit());
    big_float x(1.5);
    EXPECT_FALSE((x - x).signbit());
    EXPECT_TRUE((big_float(1.5, 53, rounding::floor) - x).signbit());
    // the exponent's range: past it an infinity, below it a zero
    big_float huge = big_float::parse("0x1p2147483646").value();
    EXPECT_FALSE(huge.is_infinite());
    EXPECT_TRUE((huge * 4).is_infinite());
    big_float tiny = big_float::parse("0x1p-2147483649").value();
    EXPECT_EQ(tiny.exponent(), -2147483648ll);
    EXPECT_EQ((tiny / 4).sign(), 0);
    // half_down and unnecessary, which Go has not
    EXPECT_EQ(P(big_float(big_integer(5), 2, rounding::half_down)), P(big_float(big_integer(4), 2)));
    EXPECT_EQ(P(big_float(big_integer(7), 2, rounding::half_down)), P(big_float(big_integer(6), 2)));
    EXPECT_EQ(P(big_float(big_integer(7), 2, rounding::half_up)), P(big_float(big_integer(8), 2)));
    EXPECT_EQ(P(big_float(big_integer(6), 2, rounding::unnecessary)), "0x.cp+3");
    EXPECT_THROW(big_float(big_integer(5), 2, rounding::unnecessary), std::domain_error);
    EXPECT_THROW(big_float(1, 64, rounding::unnecessary) / 3, std::domain_error);
    // a precision of 0 for a value that is not zero
    EXPECT_THROW(big_float(big_integer(1), 0), std::invalid_argument);
    EXPECT_EQ(big_float(big_integer(0), 0).precision(), 0u);
    // to_big_integer in each mode, is_integer, exponent
    big_float half(-2.5);
    EXPECT_EQ(half.to_big_integer(), -2);
    EXPECT_EQ(half.to_big_integer(rounding::floor), -3);
    EXPECT_EQ(half.to_big_integer(rounding::half_even), -2);
    EXPECT_EQ(half.to_big_integer(rounding::half_up), -3);
    EXPECT_TRUE(big_float(1e30).is_integer());
    EXPECT_FALSE(half.is_integer());
    EXPECT_EQ(big_float(0.75).exponent(), 0);
    EXPECT_EQ(big_float(1.0).exponent(), 1);
    // exact decimals both ways
    EXPECT_EQ(big_float(0.1).to_decimal(), decimal(0.1));
    EXPECT_TRUE(big_float(0.1).to_decimal().identical(decimal(0.1)));
    EXPECT_EQ(big_float(-1e300).to_decimal(), decimal(-1e300));
    EXPECT_EQ(P(big_float(decimal("0.1"), 53)), P(big_float(0.1)));
    EXPECT_EQ(P(big_float(decimal("1e30"), 64)), P(big_float::parse("1e30").value()));
    // the order over infinities and zeros, and the hash of equal values
    sgcl::vector<big_float> ordered = {-big_float::infinity(), big_float(-1e300), big_float(-1), big_float(), big_float(1e-300),
                                       big_float(1), big_float::infinity()};
    for (size_t i = 0; i < ordered.size(); ++i) {
        for (size_t j = 0; j < ordered.size(); ++j) {
            EXPECT_EQ(ordered[i] < ordered[j], i < j);
        }
    }
    std::hash<big_float> h;
    EXPECT_EQ(h(big_float(1.5)), h(big_float(1.5, 200, rounding::half_even)));
    EXPECT_EQ(h(big_float(0.0)), h(big_float(-0.0)));
    // errors of the data
    EXPECT_FALSE(big_float::parse("").has_value());
    EXPECT_FALSE(big_float::parse("1.2.3").has_value());
    EXPECT_FALSE(big_float::parse("0x").has_value());
    EXPECT_FALSE(big_float::parse("0x1p").has_value());
    EXPECT_FALSE(big_float::parse("0x1g").has_value());
    EXPECT_FALSE(big_float::parse("1e9999999").has_value());
    EXPECT_FALSE(big_float::parse("0x1p99999999999").has_value());
    EXPECT_THROW(big_float(string("x")), bad_expected_access<math::parse_error>);
    // a whole number with a precision is exact before the rounding (not a
    // double on the way: 2^53 + 1 would come out 2^53)
    EXPECT_EQ(big_float(9007199254740993LL, 200).to_big_integer(), big_integer(9007199254740993LL));
    EXPECT_EQ(big_float(uint64_t(-1), 64).to_big_integer(), big_integer(uint64_t(-1)));
    EXPECT_EQ(P(big_float(9007199254740993LL, 53)), P(big_float(9007199254740992.0)));
    // the texts
    EXPECT_EQ(str(big_float(1234567).to_string()), "1234567");
    EXPECT_EQ(str(big_float::parse("1e21").value().to_string()), "1e+21");
    EXPECT_EQ(str(big_float::parse("1e20").value().to_string()), "100000000000000000000");
    EXPECT_EQ(str(big_float(0.0001).to_string()), "0.0001");
    EXPECT_EQ(str(big_float(0.00001).to_string()), "1e-05");
    EXPECT_EQ(str(big_float(1234567).to_scientific()), "1.234567e+06");
    EXPECT_EQ(str(big_float(12).to_hex()), "0x1.8p+3");
    EXPECT_EQ(str(big_float(0.1).to_hex()), "0x1.999999999999ap-4");
    EXPECT_EQ(str(big_float(-0.0).to_hex()), "-0x0p+0");
    EXPECT_EQ(str(big_float::parse("0x1p-1074", 53).value().to_hex()), "0x1p-1074");
}

TEST(BigFloat_Tests, Format) {
    big_float x = big_float::parse("3.14159265358979323846", 100).value();
    EXPECT_EQ(str(txt::format("{}", x)), str(x.to_string()));
    EXPECT_EQ(str(txt::format("{:e}", x)), str(x.to_scientific()));
    EXPECT_EQ(str(txt::format("{:a}", big_float(12))), "0x1.8p+3");
    EXPECT_EQ(str(txt::format("{:.1a}", big_float(0.1))), "0x1.ap-4");
    EXPECT_EQ(str(txt::format("{:.5f}", x)), "3.14159");
    EXPECT_EQ(str(txt::format("{:.3e}", -x)), "-3.142e+00");
    EXPECT_EQ(str(txt::format("{:+.2f}", x)), "+3.14");
    EXPECT_EQ(str(txt::format("{:>10.2f}", x)), "      3.14");
    EXPECT_EQ(str(txt::format("{}", -big_float::infinity())), "-Inf");
    EXPECT_EQ(str(txt::format("{}", big_float::infinity())), "+Inf");
    EXPECT_EQ(str(txt::format("{:+}", big_float::infinity())), "+Inf");
    std::ostringstream os;
    os << big_float(0.1);
    EXPECT_EQ(os.str(), "0.1");
}
