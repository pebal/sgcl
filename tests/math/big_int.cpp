//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "vectors.h"

#include <atomic>
#include <bit>
#include <cmath>
#include <random>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

using math::big_integer;

namespace {
    // The oracle's numbers are hexadecimal with a leading minus
    big_integer H(const char* s) {
        auto v = big_integer::parse(s, 16);
        EXPECT_TRUE(v.has_value()) << s;
        return v ? *v : big_integer();
    }

    std::string hx(const big_integer& v) {
        auto s = v.to_string(16);
        return std::string(s.data(), s.size());
    }

    std::string dec(const big_integer& v) {
        auto s = v.to_string();
        return std::string(s.data(), s.size());
    }

    // A random value of up to `limbs` limbs, with a random sign; now and
    // then limbs of all ones or of zeros
    big_integer random_value(std::mt19937_64& rng, int limbs) {
        int n = int(rng() % limbs) + 1;
        big_integer v;
        for (int i = 0; i < n; ++i) {
            uint64_t w = rng();
            switch (rng() % 8) {
                case 0: w = ~uint64_t(0); break;
                case 1: w = 0; break;
                default: break;
            }
            v = (v << 64) | big_integer(w);
        }
        return rng() & 1 ? -v : v;
    }

    template<class T>
    concept shiftable = requires(big_integer a, T b) { a << b; a >> b; };

    void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(BigInt_Tests, SixteenBytes) {
    static_assert(sizeof(big_integer) == 16);
    static_assert(std::is_nothrow_constructible_v<big_integer, int>);
    static_assert(std::is_nothrow_constructible_v<big_integer, int64_t>);
    static_assert(std::is_nothrow_constructible_v<big_integer, uint32_t>);
    static_assert(!std::is_nothrow_constructible_v<big_integer, uint64_t>);   // above INT64_MAX it allocates
    static_assert(!std::is_constructible_v<big_integer, bool>);
    static_assert(!std::is_convertible_v<double, big_integer>);
    SUCCEED();
}

// The small representation and its edges: every value int64_t holds is
// inline, and the first one past either end goes to limbs and back
TEST(BigInt_Tests, EdgesOfTheSmallRepresentation) {
    big_integer max = INT64_MAX;
    big_integer min = INT64_MIN;
    EXPECT_EQ(max.to_int64(), INT64_MAX);
    EXPECT_EQ(min.to_int64(), INT64_MIN);
    big_integer past = max + 1;
    EXPECT_EQ(past.to_int64(), nullopt);
    EXPECT_EQ(past.to_uint64(), uint64_t(1) << 63);
    EXPECT_EQ(hx(past), "8000000000000000");
    EXPECT_EQ((past - 1).to_int64(), INT64_MAX);
    big_integer below = min - 1;
    EXPECT_EQ(hx(below), "-8000000000000001");
    EXPECT_EQ((below + 1).to_int64(), INT64_MIN);
    EXPECT_EQ(hx(-min), "8000000000000000");
    EXPECT_EQ((-(-min)).to_int64(), INT64_MIN);
    EXPECT_EQ((-past).to_int64(), INT64_MIN);
    EXPECT_EQ((min / -1), past);
    EXPECT_EQ((min % -1), 0);
    auto [q, r] = min.div_rem(-1);
    EXPECT_EQ(q, past);
    EXPECT_EQ(r, 0);
    EXPECT_EQ((min * -1), past);
    EXPECT_EQ(min.abs(), past);
    big_integer u = UINT64_MAX;
    EXPECT_EQ(u.to_uint64(), UINT64_MAX);
    EXPECT_EQ(u.to_int64(), nullopt);
    EXPECT_EQ((-u).to_uint64(), nullopt);
    EXPECT_EQ((u + 1).to_uint64(), nullopt);
    big_integer m = max;
    ++m;
    EXPECT_EQ(m, past);
    --m;
    EXPECT_EQ(m.to_int64(), INT64_MAX);
    big_integer n = min;
    EXPECT_EQ((n--).to_int64(), INT64_MIN);
    EXPECT_EQ(n, below);
    EXPECT_EQ(big_integer(uint8_t(255)), 255);
    EXPECT_EQ(big_integer(int8_t(-128)), -128);
    EXPECT_EQ(big_integer(uint16_t(65535)), 65535);
    EXPECT_EQ(big_integer(short(-5)), -5);
    EXPECT_EQ(big_integer(0u), 0);
    EXPECT_EQ(big_integer(-3L), -3);
    EXPECT_EQ(big_integer(7ULL), 7);
}

TEST(BigInt_Tests, WideIntegers) {
    __int128 v = __int128(1) << 100;
    EXPECT_EQ(hx(big_integer(v)), "10000000000000000000000000");
    EXPECT_EQ(hx(big_integer(-v)), "-10000000000000000000000000");
    __int128 min = __int128(1) << 127;   // -2^127
    EXPECT_EQ(hx(big_integer(min)), "-80000000000000000000000000000000");
    unsigned __int128 all = ~(unsigned __int128)0;
    EXPECT_EQ(hx(big_integer(all)), "ffffffffffffffffffffffffffffffff");
    EXPECT_EQ(big_integer(__int128(-5)), -5);
    EXPECT_EQ(big_integer(all), all);
    EXPECT_TRUE(big_integer(all) > (unsigned __int128)(all - 1));
}

// Every operation over every pair of the oracle's operands: the edges of
// the representation in all sign combinations, and random operands of 1
// to 40 limbs
TEST(BigInt_Tests, BinaryOperationsAgainstPython) {
    size_t checked = 0;
    for (auto& t : math_vectors::binary) {
        big_integer a = H(t.a);
        big_integer b = H(t.b);
        ASSERT_EQ(hx(a + b), t.sum) << t.a << " + " << t.b;
        ASSERT_EQ(hx(a - b), t.difference) << t.a << " - " << t.b;
        ASSERT_EQ(hx(a * b), t.product) << t.a << " * " << t.b;
        if (*t.quotient) {
            ASSERT_EQ(hx(a / b), t.quotient) << t.a << " / " << t.b;
            ASSERT_EQ(hx(a % b), t.remainder) << t.a << " % " << t.b;
            ASSERT_EQ(hx(a.mod(b)), t.mod) << t.a << " mod " << t.b;
            auto [q, r] = a.div_rem(b);
            ASSERT_EQ(hx(q), t.quotient);
            ASSERT_EQ(hx(r), t.remainder);
        } else {
            EXPECT_THROW(a / b, std::domain_error);
            EXPECT_THROW(a % b, std::domain_error);
            EXPECT_THROW(a.mod(b), std::domain_error);
            EXPECT_THROW(a.div_rem(b), std::domain_error);
        }
        ASSERT_EQ(hx(a & b), t.and_) << t.a << " & " << t.b;
        ASSERT_EQ(hx(a | b), t.or_) << t.a << " | " << t.b;
        ASSERT_EQ(hx(a ^ b), t.xor_) << t.a << " ^ " << t.b;
        ++checked;
    }
    EXPECT_GT(checked, 1500u);
}

// Divisions built so that Knuth's estimate of a quotient limb is one too
// large after the test of D3, so that the subtraction goes negative and
// the divisor is added back (D6); the oracle's model of algorithm D
// confirms each of them takes that step
TEST(BigInt_Tests, DivisionAddBack) {
    for (auto& t : math_vectors::add_back) {
        big_integer a = H(t.a);
        big_integer b = H(t.b);
        ASSERT_EQ(hx(a / b), t.quotient) << t.a << " / " << t.b;
        ASSERT_EQ(hx(a % b), t.remainder) << t.a << " % " << t.b;
        auto [q, r] = a.div_rem(b);
        ASSERT_EQ(q * b + r, a);
    }
}

TEST(BigInt_Tests, UnaryAgainstPython) {
    for (auto& t : math_vectors::unary) {
        big_integer a = H(t.a);
        ASSERT_EQ(hx(-a), t.negated) << t.a;
        ASSERT_EQ(hx(~a), t.complement) << t.a;
        ASSERT_EQ(a.bit_length(), t.bit_length) << t.a;
        ASSERT_EQ(a.trailing_zeros(), t.trailing_zeros) << t.a;
        double d = a.to_double();
        ASSERT_EQ(std::bit_cast<uint64_t>(d), std::bit_cast<uint64_t>(t.to_double)) << t.a << ": " << d << " != " << t.to_double;
        ASSERT_EQ(a.sign(), (a > 0) - (a < 0));
        ASSERT_EQ(a.abs(), a.sign() < 0 ? -a : a);
        ASSERT_EQ(~~a, a);
    }
}

TEST(BigInt_Tests, BitsOfTwosComplement) {
    for (auto& t : math_vectors::bits) {
        ASSERT_EQ(H(t.a).bit(t.index), t.bit) << t.a << " bit " << t.index;
    }
}

TEST(BigInt_Tests, ShiftsAgainstPython) {
    for (auto& t : math_vectors::shifts) {
        big_integer a = H(t.a);
        ASSERT_EQ(hx(a << t.bits), t.left) << t.a << " << " << t.bits;
        ASSERT_EQ(hx(a >> t.bits), t.right) << t.a << " >> " << t.bits;
        ASSERT_EQ((a << t.bits) >> t.bits, a);
    }
    EXPECT_EQ(big_integer(-1) >> 100, -1);
    EXPECT_EQ(big_integer(-1) >> 0, -1);
    EXPECT_EQ(big_integer(-5) >> 1, -3);
    EXPECT_EQ((-(big_integer(1) << 200)) >> 200, -1);
    EXPECT_EQ((-(big_integer(1) << 200) - 1) >> 200, -2);
    EXPECT_THROW(big_integer(1) << -1, std::domain_error);
    EXPECT_THROW(big_integer(1) >> -1, std::domain_error);
    EXPECT_THROW(big_integer(1) << INT64_MAX, std::length_error);
    EXPECT_EQ(big_integer(0) << INT64_MAX, 0);
    EXPECT_EQ(big_integer(12345) >> INT64_MAX, 0);
    EXPECT_EQ((big_integer(1) << 1000) >> INT64_MAX, 0);
    big_integer x = 3;
    x <<= 130;
    x >>= 129;
    EXPECT_EQ(x, 6);
}

TEST(BigInt_Tests, TextInEveryBase) {
    for (auto& t : math_vectors::texts) {
        big_integer a = H(t.a);
        auto s = a.to_string(t.base);
        ASSERT_EQ(std::string(s.data(), s.size()), t.text) << t.a << " in base " << t.base;
        auto back = big_integer::parse(t.text, t.base);
        ASSERT_TRUE(back.has_value()) << t.text;
        ASSERT_EQ(*back, a) << t.text << " in base " << t.base;
        std::string upper = t.text;
        for (char& c : upper) {
            c = char(std::toupper((unsigned char)c));
        }
        ASSERT_EQ(*big_integer::parse(string(upper), t.base), a);
    }
}

TEST(BigInt_Tests, LongDecimal) {
    big_integer p = H(math_vectors::power_of_three_hex);
    EXPECT_EQ(dec(p), math_vectors::power_of_three_decimal);
    EXPECT_EQ(*big_integer::parse(math_vectors::power_of_three_decimal), p);
    // A hundred thousand digits both ways: a one and zeros, nines, and
    // the round trip of a number with all digits in it
    big_integer ten = 10;
    big_integer e = 1;
    big_integer base = 10;
    for (int64_t k = 100000; k; k >>= 1) {
        if (k & 1) {
            e *= base;
        }
        base *= base;
    }
    std::string s = dec(e);
    ASSERT_EQ(s.size(), 100001u);
    EXPECT_EQ(s.find_first_not_of('0', 1), std::string::npos);
    EXPECT_EQ(s[0], '1');
    std::string nines = dec(e - 1);
    EXPECT_EQ(nines, std::string(100000, '9'));
    EXPECT_EQ(*big_integer::parse(string(nines)), e - 1);
    big_integer mixed = (e - 1) / 7;
    EXPECT_EQ(*big_integer::parse(mixed.to_string()), mixed);
    EXPECT_EQ(*big_integer::parse(mixed.to_string(36), 36), mixed);
    EXPECT_EQ(*big_integer::parse(mixed.to_string(2), 2), mixed);
}

TEST(BigInt_Tests, ParseErrors) {
    auto error = [](const char* text, int base = 10) {
        auto r = big_integer::parse(text, base);
        EXPECT_FALSE(r.has_value()) << text;
        return r ? size_t(-1) : r.error().offset();
    };
    EXPECT_EQ(error(""), 0u);
    EXPECT_EQ(error("+"), 1u);
    EXPECT_EQ(error("-"), 1u);
    EXPECT_EQ(error("12x4"), 2u);
    EXPECT_EQ(error("ff"), 0u);
    EXPECT_EQ(error("fg", 16), 1u);
    EXPECT_EQ(error("0x10", 16), 1u);
    EXPECT_EQ(error(" 1"), 0u);
    EXPECT_EQ(error("1 "), 1u);
    EXPECT_EQ(error("1_000"), 1u);
    EXPECT_EQ(error("--1"), 1u);
    EXPECT_EQ(error("12\xc5\xbc"), 2u);   // a letter past ASCII: the byte where it starts
    EXPECT_EQ(error("2", 2), 0u);
    EXPECT_EQ(error("z", 35), 0u);
    EXPECT_EQ(error("12345678901234567890123456789012345678901234567890a"), 50u);
    auto r = big_integer::parse("12x4");
    auto m = r.error().message();
    EXPECT_EQ(std::string(m.data(), m.size()), "not a digit in base 10 at byte 2");
    m = big_integer::parse("").error().message();
    EXPECT_EQ(std::string(m.data(), m.size()), "empty text at byte 0");
    m = big_integer::parse("-").error().message();
    EXPECT_EQ(std::string(m.data(), m.size()), "no digits after the sign at byte 1");
    EXPECT_THROW(big_integer::parse("1", 1), std::invalid_argument);
    EXPECT_THROW(big_integer::parse("1", 37), std::invalid_argument);
    EXPECT_THROW(big_integer(5).to_string(1), std::invalid_argument);
    EXPECT_THROW(big_integer(5).to_string(37), std::invalid_argument);
    EXPECT_EQ(*big_integer::parse("-0"), 0);
    EXPECT_EQ(big_integer::parse("-0")->sign(), 0);
    EXPECT_EQ(*big_integer::parse("+5"), 5);
    EXPECT_EQ(*big_integer::parse("z", 36), 35);
    EXPECT_EQ(*big_integer::parse("Z", 36), 35);
    EXPECT_EQ(*big_integer::parse("0000000000000000000000000000000000000000000000000000001"), 1);
    EXPECT_EQ(*big_integer::parse("-0000000000000000000000000000000000000000009223372036854775808"), INT64_MIN);
    EXPECT_EQ(*big_integer::parse("9223372036854775808"), big_integer(INT64_MAX) + 1);
    EXPECT_EQ(*big_integer::parse("18446744073709551615"), UINT64_MAX);
    EXPECT_EQ(*big_integer::parse("18446744073709551616"), big_integer(UINT64_MAX) + 1);
    EXPECT_EQ(*big_integer::parse("-18446744073709551616"), -(big_integer(UINT64_MAX) + 1));
}

TEST(BigInt_Tests, FromDouble) {
    for (auto& t : math_vectors::from_double) {
        ASSERT_EQ(hx(big_integer(t.value)), t.whole) << t.value;
        if (std::fabs(t.value) < 0x1p1023) {
            ASSERT_EQ(big_integer(t.value).to_double(), std::trunc(t.value) + 0.0) << t.value;
        }
    }
    // A float, promoted exactly: the cases of its own, not the double's
    EXPECT_EQ(hx(big_integer(0x1p70f)), "400000000000000000");
    EXPECT_EQ(big_integer(-3.75f), -3);
    EXPECT_EQ(big_integer(0.999f), 0);
    EXPECT_EQ(hx(big_integer(std::numeric_limits<float>::max())), "ffffff00000000000000000000000000");
    EXPECT_EQ(big_integer(16777217.0f), 16777216);   // the float itself is 2^24: the value is what the float holds
    EXPECT_THROW((void)big_integer(std::numeric_limits<float>::infinity()), std::domain_error);
    static_assert(!std::is_constructible_v<big_integer, long double>);
    EXPECT_EQ(big_integer(-0x1p63).to_int64(), INT64_MIN);
    EXPECT_EQ(big_integer(0x1p63).to_int64(), nullopt);
    EXPECT_THROW((void)big_integer(std::nan("")), std::domain_error);
    EXPECT_THROW((void)big_integer(double(INFINITY)), std::domain_error);
    EXPECT_THROW((void)big_integer(-double(INFINITY)), std::domain_error);
}

TEST(BigInt_Tests, Bytes) {
    byte raw[] = {byte{0}, byte{0}, byte{1}, byte{2}, byte{3}, byte{4},
                       byte{5}, byte{6}, byte{7}, byte{8}, byte{9}, byte{0xff}};
    big_integer v = big_integer::from_bytes(slice<const byte>(raw, raw + sizeof raw));
    EXPECT_EQ(hx(v), "10203040506070809ff");
    auto bytes = v.to_bytes();
    ASSERT_EQ(bytes.size(), 10u);
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_EQ(bytes[i], raw[i + 2]);
    }
    auto padded = v.to_bytes(12);
    ASSERT_EQ(padded.size(), 12u);
    for (size_t i = 0; i < 12; ++i) {
        EXPECT_EQ(padded[i], raw[i]);
    }
    EXPECT_THROW(v.to_bytes(9), std::length_error);
    EXPECT_EQ((-v).to_bytes().size(), 10u);          // the magnitude
    EXPECT_EQ(big_integer(0).to_bytes().size(), 0u);
    EXPECT_EQ(big_integer(0).to_bytes(3).size(), 3u);
    EXPECT_EQ(big_integer::from_bytes(slice<const byte>()), 0);
    EXPECT_EQ(big_integer::from_bytes(slice<const byte>(raw, raw + 2)), 0);
    EXPECT_EQ(big_integer(255).to_bytes().size(), 1u);
    EXPECT_EQ(big_integer(256).to_bytes().size(), 2u);
    byte eight[] = {byte{0x80}, byte{0}, byte{0}, byte{0}, byte{0}, byte{0}, byte{0}, byte{1}};
    EXPECT_EQ(big_integer::from_bytes(slice<const byte>(eight, eight + 8)), big_integer((uint64_t(1) << 63) + 1));
    EXPECT_EQ(big_integer::from_bytes(slice<const byte>(eight + 1, eight + 8)), 1);
    std::mt19937_64 rng(3);
    for (int i = 0; i < 300; ++i) {
        big_integer x = random_value(rng, 9).abs();
        auto b = x.to_bytes();
        ASSERT_EQ(big_integer::from_bytes(slice<const byte>(b.data(), b.data() + b.size())), x);
        ASSERT_EQ(b.size(), (x.bit_length() + 7) / 8);
    }
}

TEST(BigInt_Tests, ComparisonsWithBuiltins) {
    big_integer big = big_integer(1) << 70;
    EXPECT_TRUE(big > 10);
    EXPECT_TRUE(10 < big);
    EXPECT_TRUE(-big < INT64_MIN);
    EXPECT_TRUE(big > UINT64_MAX);
    EXPECT_TRUE(big != 0);
    EXPECT_TRUE(0 != big);
    EXPECT_TRUE(big_integer(0) == 0);
    EXPECT_TRUE(big_integer(UINT64_MAX) == UINT64_MAX);
    EXPECT_TRUE(big_integer(UINT64_MAX) > INT64_MAX);
    EXPECT_TRUE(big_integer(UINT64_MAX) != -1);
    EXPECT_TRUE(big_integer(-1) < 0u);
    EXPECT_TRUE(-big_integer(UINT64_MAX) < INT64_MIN);
    EXPECT_TRUE(-big_integer(UINT64_MAX) == -big_integer(UINT64_MAX));
    EXPECT_TRUE(big_integer(INT64_MIN) == INT64_MIN);
    EXPECT_TRUE(big_integer(INT64_MIN) < -9223372036854775807LL);
    EXPECT_EQ(big_integer(5) <=> 5u, std::strong_ordering::equal);
    EXPECT_EQ(big_integer(5) <=> 6, std::strong_ordering::less);
    EXPECT_EQ(big <=> big_integer(1) << 70, std::strong_ordering::equal);
    // Ordering across lengths and signs, against the order of the
    // oracle's values sorted by Python... by sign and magnitude
    std::mt19937_64 rng(4);
    vector<big_integer> values;
    for (int i = 0; i < 400; ++i) {
        values.push_back(random_value(rng, 5));
    }
    for (size_t i = 0; i + 1 < values.size(); ++i) {
        const big_integer& a = values[i];
        const big_integer& b = values[i + 1];
        big_integer d = a - b;
        ASSERT_EQ(a < b, d.sign() < 0);
        ASSERT_EQ(a == b, d.sign() == 0);
        ASSERT_EQ(a > b, d.sign() > 0);
        ASSERT_EQ((a <=> b) == 0, a == b);
    }
}

// An operand that is also the result: the old value is held by the
// caller's reference for the whole operation, and the new one is a new
// object
TEST(BigInt_Tests, Aliasing) {
    big_integer a = *big_integer::parse("123456789012345678901234567890123456789012345678901234567890");
    big_integer x = a;
    x += x;
    EXPECT_EQ(x, a * 2);
    x = a;
    x -= x;
    EXPECT_EQ(x, 0);
    x = a;
    x *= x;
    EXPECT_EQ(x, a * a);
    x = a;
    x /= x;
    EXPECT_EQ(x, 1);
    x = a;
    x %= x;
    EXPECT_EQ(x, 0);
    x = a;
    x = x.mod(x);
    EXPECT_EQ(x, 0);
    x = -a;
    x &= x;
    EXPECT_EQ(x, -a);
    x ^= x;
    EXPECT_EQ(x, 0);
    EXPECT_EQ(dec(a), "123456789012345678901234567890123456789012345678901234567890");
}

// +=, -= and *= by a small value write into the value's own object when
// nothing else has it (the class comment): a copy, a negation and a move
// each leave the other value as it was, and the object is the same one
// before and after while there is room
TEST(BigInt_Tests, InPlaceOnlyWhenNothingElseHasTheValue) {
    auto data = [](const big_integer& v) {
        math::detail::Limb room;
        size_t n;
        return math::detail::BigIntAccess::magnitude(v, room, n);
    };
    const big_integer a = *big_integer::parse("123456789012345678901234567890123456789012345678901234567890");
    big_integer s = a + 1;                 // a new value, nobody else's
    auto p = data(s);
    s += 5;
    s -= 3;
    s *= 7;
    EXPECT_EQ(data(s), p) << "written in place";
    EXPECT_EQ(s, (a + 3) * 7);

    big_integer copy = s;                  // shared from now on: neither is changed in place
    s += a;
    EXPECT_EQ(copy, (a + 3) * 7);
    EXPECT_EQ(s, (a + 3) * 7 + a);
    EXPECT_NE(data(s), data(copy));
    copy *= 2;
    EXPECT_EQ(s, (a + 3) * 7 + a);

    big_integer t = s * 1 + 0;             // a new one again
    big_integer negated = -t;              // shares t's limbs
    t += 1;
    t *= 3;
    EXPECT_EQ(negated, -((a + 3) * 7 + a));
    EXPECT_EQ(t, ((a + 3) * 7 + a + 1) * 3);

    big_integer u = a * 3;
    big_integer moved = std::move(u);      // handed on, zero left behind
    EXPECT_EQ(u, 0);
    moved += 1;
    EXPECT_EQ(moved, a * 3 + 1);
    u += 2;
    EXPECT_EQ(u, 2);
    EXPECT_EQ(moved, a * 3 + 1);

    big_integer w = a * 5;
    vector<big_integer> kept;
    kept.push_back(w);                     // a copy in the container
    w -= a * 4;
    EXPECT_EQ(kept[0], a * 5);
    EXPECT_EQ(w, a);
    EXPECT_EQ(dec(a), "123456789012345678901234567890123456789012345678901234567890");
}

// Long runs of the three in place against the same arithmetic done anew
// each time: growth past the room of the object's class, a carry into a
// new limb at the room's edge, the sign crossing zero, a value falling
// back to int64_t and growing again, *= by 0, -1 and INT64_MIN; a copy
// taken now and then must keep the value it was taken at
TEST(BigInt_Tests, InPlaceAgainstNewValues) {
    auto data = [](const big_integer& v) {
        math::detail::Limb room;
        size_t n;
        return math::detail::BigIntAccess::magnitude(v, room, n);
    };
    // new values, nobody else's: a + 1 and a * 1 make one each (a + 0
    // would be a copy of a)
    auto fresh = [&](std::mt19937_64& rng) {
        return random_value(rng, 12) * 1 + 1;
    };
    std::mt19937_64 rng(11);
    size_t in_place = 0;
    for (int round = 0; round < 200; ++round) {
        big_integer s = fresh(rng);
        big_integer ref = s * 1;
        vector<big_integer> snaps;               // a big_integer lives on a stack or in a managed object
        std::vector<std::string> snap_texts;
        for (int step = 0; step < 300; ++step) {
            auto before = data(s);
            switch (rng() % 7) {
                case 0:
                case 1: {
                    big_integer x = random_value(rng, int(rng() % 3 == 0 ? 20 : 3));
                    s += x;
                    ref = ref + x;
                    break;
                }
                case 2:
                case 3: {
                    big_integer x = random_value(rng, int(rng() % 3 == 0 ? 20 : 3));
                    s -= x;
                    ref = ref - x;
                    break;
                }
                case 4: {
                    int64_t m;
                    switch (rng() % 8) {
                        case 0: m = 0; break;
                        case 1: m = -1; break;
                        case 2: m = INT64_MIN; break;
                        case 3: m = INT64_MAX; break;
                        default: m = int64_t(rng() % 2000) - 1000; break;
                    }
                    s *= m;
                    ref = ref * m;
                    break;
                }
                case 5: {
                    big_integer x = ref;           // exactly the value, so s cancels to zero or doubles
                    if (rng() & 1) {
                        s -= x;
                        ref = ref - x;
                    } else {
                        s += x;
                        ref = ref + x;
                    }
                    break;
                }
                default:
                    if (snaps.size() < 8) {
                        snaps.push_back(s);
                        snap_texts.push_back(dec(s));
                    }
                    break;
            }
            ASSERT_EQ(s, ref) << "round " << round << " step " << step;
            in_place += data(s) == before && s.abs().bit_length() > 64;
            if (s.abs().bit_length() > 64 * 400) {
                s = fresh(rng);
                ref = s * 1;
            }
        }
        for (size_t i = 0; i < snaps.size(); ++i) {
            ASSERT_EQ(dec(snaps[i]), snap_texts[i]) << "a copy changed with the value it was taken from";
        }
    }
    EXPECT_GT(in_place, size_t(5000)) << "the steps done in place";
}

// The edges of the in-place paths: a carry at the edge of the room and a
// longer operand move the value to a new object (with the room of a
// larger class); a value that falls within int64_t is small again, equal
// to the same small value; s += s and s -= s in place
TEST(BigInt_Tests, InPlaceEdges) {
    auto data = [](const big_integer& v) {
        math::detail::Limb room;
        size_t n;
        return math::detail::BigIntAccess::magnitude(v, room, n);
    };
    const big_integer two192 = big_integer(1) << 192;
    big_integer s = two192 - 1;                  // three limbs of ones, in an object of room three
    auto p = data(s);
    s += 1;                                      // the carry past the room
    EXPECT_NE(data(s), p);
    EXPECT_EQ(s, two192);
    p = data(s);
    s += 1;
    EXPECT_EQ(data(s), p) << "the new object has room";
    EXPECT_EQ(s, two192 + 1);

    big_integer m = two192 - 1;
    p = data(m);
    m *= 3;
    EXPECT_NE(data(m), p);
    EXPECT_EQ(m, (two192 - 1) * 3);

    const big_integer longer = (big_integer(1) << 300) + 7;
    big_integer t = (big_integer(1) << 100) + 1; // two limbs
    p = data(t);
    t += longer;
    EXPECT_NE(data(t), p);
    EXPECT_EQ(t, (big_integer(1) << 300) + (big_integer(1) << 100) + 8);
    big_integer u = (big_integer(1) << 100) + 1;
    u -= longer;                                 // |b| > |a|, the sign changes
    EXPECT_EQ(u, (big_integer(1) << 100) + 1 - longer);
    EXPECT_LT(u, 0);

    big_integer v = (big_integer(1) << 200) + 3; // four limbs, room for more
    p = data(v);
    v -= (big_integer(1) << 200) - (big_integer(1) << 70);
    EXPECT_EQ(data(v), p);
    EXPECT_EQ(v, (big_integer(1) << 70) + 3);
    v -= (big_integer(1) << 70) - 2;             // within int64_t: small again
    EXPECT_EQ(v, 5);
    EXPECT_EQ(std::hash<big_integer>{}(v), std::hash<big_integer>{}(big_integer(5)));
    big_integer w = (big_integer(1) << 200) + 3;
    w -= (big_integer(1) << 200) + 3;
    EXPECT_EQ(w, 0);
    EXPECT_EQ(w.sign(), 0);
    big_integer n = (big_integer(1) << 130) + 9;
    n -= (big_integer(1) << 130) + (big_integer(1) << 63) + 9;   // -2^63: INT64_MIN, small
    EXPECT_EQ(n, INT64_MIN);
    EXPECT_EQ(n.to_int64().value_or(0), INT64_MIN);

    big_integer d = (big_integer(1) << 130) + 12345;
    p = data(d);
    d += d;
    EXPECT_EQ(data(d), p);
    EXPECT_EQ(d, (big_integer(1) << 131) + 24690);
    d -= d;
    EXPECT_EQ(d, 0);
}

// A value no other has, copied by many threads at once: each copy marks
// the object shared (an atomic write) while the others read it; after
// they finish, the owner's += must leave every copy as it was
TEST(BigInt_Tests, CopiesFromThreads) {
    const big_integer a = *big_integer::parse("98765432109876543210987654321098765432109876543210");
    vector<big_integer> held;
    held.push_back(a * 3 + 1);                   // moved in: still nobody else's
    std::vector<std::thread> threads;
    std::vector<std::string> seen(8);
    for (size_t t = 0; t < seen.size(); ++t) {
        threads.emplace_back([&, t] {
            big_integer last;
            for (int i = 0; i < 2000; ++i) {
                big_integer c = held[0];
                last = c;
            }
            seen[t] = dec(last);
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    big_integer copy = held[0];
    held[0] += a;
    for (auto& text : seen) {
        EXPECT_EQ(text, dec(a * 3 + 1));
    }
    EXPECT_EQ(copy, a * 3 + 1);
    EXPECT_EQ(held[0], a * 4 + 1);
}

// Random operands of different lengths and a fixed seed: the laws of
// arithmetic, which hold for every value the oracle has not named
TEST(BigInt_Tests, AlgebraicProperties) {
    std::mt19937_64 rng(20260924);
    for (int i = 0; i < 3000; ++i) {
        big_integer a = random_value(rng, 12);
        big_integer b = random_value(rng, 12);
        big_integer c = random_value(rng, 6);
        ASSERT_EQ((a + b) - b, a);
        ASSERT_EQ(a + b, b + a);
        ASSERT_EQ(a * b, b * a);
        ASSERT_EQ(a * (b + c), a * b + a * c);
        ASSERT_EQ((a - b) + b, a);
        ASSERT_EQ(-(a - b), b - a);
        if (b != 0) {
            auto [q, r] = a.div_rem(b);
            ASSERT_EQ(q * b + r, a);
            ASSERT_LT(r.abs(), b.abs());
            ASSERT_TRUE(r == 0 || r.sign() == a.sign());
            big_integer m = a.mod(b);
            ASSERT_TRUE(m >= 0 && m < b.abs());
            ASSERT_EQ((a - m) % b, 0);
            ASSERT_EQ((a * b) / b, a);
            ASSERT_EQ((a * b) % b, 0);
        }
        ASSERT_EQ(~~a, a);
        ASSERT_EQ(~a, -a - 1);
        ASSERT_EQ(a ^ b ^ b, a);
        ASSERT_EQ((a & b) + (a | b), a + b);
        ASSERT_EQ((a | b) - (a & b), a ^ b);
        ASSERT_EQ(~(a & b), ~a | ~b);
        auto k = int64_t(rng() % 300);
        ASSERT_EQ((a << k) >> k, a);
        ASSERT_EQ(a << k, a * (big_integer(1) << k));
        big_integer p = big_integer(1) << k;
        big_integer floor_div = a / p - ((a % p) < 0 ? 1 : 0);
        ASSERT_EQ(a >> k, floor_div);
        int base = int(rng() % 35) + 2;
        ASSERT_EQ(*big_integer::parse(a.to_string(base), base), a);
        size_t bit = size_t(rng() % 900);
        ASSERT_EQ(a.bit(bit), ((a >> int64_t(bit)) & 1) == 1);
    }
}

TEST(BigInt_Tests, Literal) {
    using namespace math::literals;
    EXPECT_EQ(0_big, 0);
    EXPECT_EQ(42_big, 42);
    EXPECT_EQ(1'000'000_big, 1000000);
    EXPECT_EQ(0x7f_big, 127);
    EXPECT_EQ(0XfF_big, 255);
    EXPECT_EQ(0b1010_big, 10);
    EXPECT_EQ(0B11_big, 3);
    EXPECT_EQ(017_big, 15);   // octal, as the language reads it
    EXPECT_EQ(9223372036854775807_big, INT64_MAX);
    EXPECT_EQ(9223372036854775808_big, big_integer(INT64_MAX) + 1);
    EXPECT_EQ(18446744073709551615_big, UINT64_MAX);
    EXPECT_EQ(0xffff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_big, (big_integer(1) << 128) - 1);
    EXPECT_EQ(123456789012345678901234567890123456789012345678901234567890_big,
              *big_integer::parse("123456789012345678901234567890123456789012345678901234567890"));
    EXPECT_EQ(-0x1'0000'0000'0000'0000_big, -(big_integer(1) << 64));
}

TEST(BigInt_Tests, Format) {
    big_integer big = (big_integer(1) << 100) + 255;
    EXPECT_EQ(txt::format("{}", big), string("1267650600228229401496703205631"));
    EXPECT_EQ(txt::format("{:d}", -big), string("-1267650600228229401496703205631"));
    EXPECT_EQ(txt::format("{:x}", big), string("100000000000000000000000ff"));
    EXPECT_EQ(txt::format("{:X}", big), string("100000000000000000000000FF"));
    EXPECT_EQ(txt::format("{:#x}", -big), string("-0x100000000000000000000000ff"));
    EXPECT_EQ(txt::format("{:#X}", big_integer(255)), string("0XFF"));
    EXPECT_EQ(txt::format("{:#b}", big_integer(5)), string("0b101"));
    EXPECT_EQ(txt::format("{:B}", big_integer(5)), string("101"));
    EXPECT_EQ(txt::format("{:o}", big_integer(8)), string("10"));
    EXPECT_EQ(txt::format("{:#o}", big_integer(8)), string("010"));
    EXPECT_EQ(txt::format("{:#o}", big_integer(0)), string("0"));
    EXPECT_EQ(txt::format("{:+}", big_integer(5)), string("+5"));
    EXPECT_EQ(txt::format("{: }", big_integer(5)), string(" 5"));
    EXPECT_EQ(txt::format("{:+}", big_integer(-5)), string("-5"));
    EXPECT_EQ(txt::format("[{:>8}]", big_integer(-42)), string("[     -42]"));
    EXPECT_EQ(txt::format("[{:<8}]", big_integer(42)), string("[42      ]"));
    EXPECT_EQ(txt::format("[{:*^9}]", big_integer(42)), string("[***42****]"));
    EXPECT_EQ(txt::format("{:08}", big_integer(-42)), string("-0000042"));
    EXPECT_EQ(txt::format("{:#010x}", big_integer(255)), string("0x000000ff"));
    EXPECT_EQ(txt::format("{:>#40x}", big), string("            0x100000000000000000000000ff"));
    EXPECT_EQ(txt::format("{} and {}", big_integer(1), big_integer(-1)), string("1 and -1"));
    // A specification a whole number does not take is refused, by the
    // compiler for a literal pattern and as nothing for a runtime one
    EXPECT_FALSE(txt::format(txt::runtime("{:.3}"), big).has_value());
    EXPECT_FALSE(txt::format(txt::runtime("{:f}"), big).has_value());
    EXPECT_FALSE(txt::format(txt::runtime("{:c}"), big).has_value());
    EXPECT_EQ(txt::format(txt::runtime("{:x}"), big), string("100000000000000000000000ff"));
}

TEST(BigInt_Tests, HashAndMaps) {
    std::hash<big_integer> h;
    big_integer a = *big_integer::parse("340282366920938463463374607431768211457");
    big_integer b = (big_integer(1) << 128) + 1;
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
    EXPECT_EQ(h(big_integer(5)), h(big_integer(10) - 5));
    EXPECT_NE(h(big_integer(5)), h(big_integer(-5)));
    EXPECT_NE(h(b), h(-b));
    sgcl::map<big_integer, int> m;
    sgcl::sorted_map<big_integer, int> sorted;
    std::mt19937_64 rng(5);
    vector<big_integer> keys;
    for (int i = 0; i < 500; ++i) {
        big_integer k = random_value(rng, 4);
        keys.push_back(k);
        m[k] = i;
        sorted[k] = i;
    }
    for (int i = 0; i < 500; ++i) {
        big_integer copy = *big_integer::parse(keys[size_t(i)].to_string());
        ASSERT_TRUE(m.contains(copy));
        ASSERT_TRUE(sorted.contains(copy));
    }
    big_integer last;
    bool first = true;
    for (auto& [k, v] : sorted) {
        if (!first) {
            ASSERT_LT(last, k);
        }
        last = k;
        first = false;
    }
}

TEST(BigInt_Tests, Stream) {
    std::ostringstream os;
    os << ((big_integer(1) << 64) + 1) << ' ' << big_integer(-255) << ' ' << std::hex << big_integer(-255) << ' ' << std::oct << big_integer(8);
    EXPECT_EQ(os.str(), "18446744073709551617 -255 -ff 10");
}

// Every combination of the flags a number obeys, against int64_t: base,
// showbase, showpos, uppercase, the three adjustments and none, width and
// fill — and the width used up by the one value. Negative values only in
// decimal: in hexadecimal and octal an int64_t writes its two's complement,
// which a number of no fixed width has not got.
TEST(BigInt_Tests, StreamFlagsAsInt) {
    const int64_t values[] = {0, 5, -5, 255, -255, 8, INT64_MAX, INT64_MIN, -1};
    const std::ios_base::fmtflags bases[] = {std::ios_base::dec, std::ios_base::hex, std::ios_base::oct, {}};
    const std::ios_base::fmtflags adjusts[] = {{}, std::ios_base::left, std::ios_base::right, std::ios_base::internal};
    int checked = 0;
    for (int64_t v : values) {
        for (auto base : bases) {
            if (v < 0 && (base == std::ios_base::hex || base == std::ios_base::oct)) {
                continue;
            }
            for (int extras = 0; extras < 8; ++extras) {
                for (auto adjust : adjusts) {
                    for (int width : {0, 1, 12, 25}) {
                        for (char fill : {' ', '*'}) {
                            auto flags = base | adjust;
                            if (extras & 1) flags |= std::ios_base::showbase;
                            if (extras & 2) flags |= std::ios_base::showpos;
                            if (extras & 4) flags |= std::ios_base::uppercase;
                            auto write = [&](auto value) {
                                std::ostringstream os;
                                os.flags(flags);
                                os.fill(fill);
                                os.width(width);
                                os << value << '|' << value;   // the second without the width
                                return os.str();
                            };
                            ASSERT_EQ(write(big_integer(v)), write(v))
                                << v << " flags " << std::hex << flags << std::dec << " width " << width << " fill " << fill;
                            ++checked;
                        }
                    }
                }
            }
        }
    }
    EXPECT_GT(checked, 1500);
    // Past int64_t the same rules
    std::ostringstream os;
    os << std::showbase << std::uppercase << std::hex << std::internal << std::setfill('0') << std::setw(24) << (big_integer(1) << 64);
    EXPECT_EQ(os.str(), "0X0000010000000000000000");
    std::ostringstream neg;
    neg << std::internal << std::setw(24) << -(big_integer(1) << 64);
    EXPECT_EQ(neg.str(), "-   18446744073709551616");
}

// The count of a shift is any whole number up to 64 bits, taken as its own
// value: a size_t is never negative, however large
TEST(BigInt_Tests, ShiftCounts) {
    big_integer x = 12345;
    EXPECT_THROW(x << -1, std::domain_error);
    EXPECT_THROW(x >> -1, std::domain_error);
    EXPECT_THROW(x << int8_t(-3), std::domain_error);
    EXPECT_THROW(x << size_t(-1), std::length_error);
    EXPECT_EQ(x >> size_t(-1), 0);
    EXPECT_EQ(-x >> size_t(-1), -1);
    EXPECT_EQ((big_integer(1) << 1000) >> uint64_t(-1), 0);
    EXPECT_EQ(x << 3u, 98760);
    EXPECT_EQ(x << x.bit_length(), 12345 * 16384);
    EXPECT_EQ(x >> short(2), 3086);
    EXPECT_EQ(big_integer(-3) << 62, big_integer(-3) * (big_integer(1) << 62));    // the small road through 128 bits
    EXPECT_EQ(big_integer(INT64_MIN) << 63, -(big_integer(1) << 126));
    EXPECT_EQ(big_integer(INT64_MAX) << 63, big_integer(INT64_MAX) * (big_integer(1) << 63));
    EXPECT_EQ((big_integer(1) << 62).to_int64(), int64_t(1) << 62);
    EXPECT_EQ(big_integer(1) << 63, big_integer(uint64_t(1) << 63));
    EXPECT_EQ((big_integer(-1) << 63).to_int64(), INT64_MIN);
    big_integer y = 1;
    y <<= size_t(70);
    y >>= 69u;
    EXPECT_EQ(y, 2);
    static_assert(!shiftable<bool>);
    static_assert(!shiftable<big_integer>);
    static_assert(!shiftable<double>);
    static_assert(shiftable<size_t> && shiftable<int> && shiftable<unsigned char>);
}

TEST(BigInt_Tests, FactorialAndPi) {
    big_integer f = 1;
    for (auto i : range(1, 101)) {
        f *= i;
    }
    auto digits = dec(f);
    EXPECT_EQ(digits.size(), 158u);
    EXPECT_EQ(digits.substr(0, 20), "93326215443944152681");
    EXPECT_EQ(f.trailing_zeros(), 97u);
    // Machin's formula, whole numbers only
    auto arccot = [](int64_t x, const big_integer& unity) {
        big_integer term = unity / x;
        big_integer sum = term;
        for (int64_t n = 3; term != 0; n += 2) {
            term /= x * x;
            sum += (n / 2 % 2 ? -term : term) / n;
        }
        return sum;
    };
    big_integer unity = 1;
    for (int i = 0; i < 1010; ++i) {
        unity *= 10;
    }
    big_integer pi = 4 * (4 * arccot(5, unity) - arccot(239, unity));
    auto text = dec(pi);
    EXPECT_EQ(text.substr(0, 52), "3141592653589793238462643383279502884197169399375105");
    EXPECT_EQ(text.substr(991, 10), "2164201989");   // decimals 991 to 1000 of pi
}

// Values in managed objects while the collector runs in a loop: long
// products and conversions made and dropped through its cycles, every one
// kept checked. After the last cycle the pages of the numbers are given
// back, the buffers of page ranges among them: what is left is measured
// against what one round left (the pages every size class keeps in hand
// for the thread), so that nineteen rounds more leave nothing more — six
// buffers of 64 KB a round are 7 MB if they stayed, where a size class
// first touched in a later round keeps a page or two more (1.75 MB seen).
TEST(BigInt_Tests, UnderTheCollector) {
    std::mt19937_64 rng(6);
    auto rounds = [&](int count) {
        for (int round = 0; round < count; ++round) {
            vector<big_integer> kept;
            vector<string> texts;
            for (int i = 0; i < 300; ++i) {
                big_integer a = random_value(rng, 40);
                big_integer b = random_value(rng, 40);
                big_integer p = a * b;
                if (i % 50 == 0) {
                    p = (p << (64 * 8000)) + a;   // past a page of limbs: a buffer of its own
                }
                kept.push_back(p);
                texts.push_back(p.to_string(i % 50 == 0 ? 16 : 10));
            }
            for (size_t i = 0; i < kept.size(); ++i) {
                ASSERT_EQ(*big_integer::parse(texts[i], i % 50 == 0 ? 16 : 10), kept[i]);
                if (kept[i] != 0) {
                    ASSERT_EQ((kept[i] / kept[i]), 1);
                }
            }
        }
    };
    off_frame([&] { rounds(1); });
    settle();
    auto live0 = collector::get_statistics().live_bytes;
    std::atomic<bool> stop = false;
    std::thread collecting([&] {
        while (!stop) {
            collector::force_collect(true);
        }
    });
    off_frame([&] { rounds(19); });
    stop = true;
    collecting.join();
    settle();
    auto live = collector::get_statistics().live_bytes;
    EXPECT_LT(live, live0 + (size_t(4) << 20)) << (double(live) - double(live0)) / 1048576.0 << " MB more left after the numbers died";
}
