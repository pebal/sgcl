//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The number theory of big_integer: pow, sqrt, gcd and lcm, mod_pow,
// mod_inverse, is_probable_prime, factorial, binomial, and random's draw
// of a big_integer. Oracles: Python's int and math (number_vectors.h, from
// tools/math_vectors.py number) and Go's ProbablyPrime (prime_vectors.h,
// from tools/math_primes_oracle.go); the lists of pseudoprimes are from
// the literature, each one checked in Python to be composite and to pass
// what it is said to pass.
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "number_vectors.h"
#include "prime_vectors.h"

#include <functional>
#include <random>
#include <string>
#include <vector>

using math::big_integer;

namespace {
    big_integer H(const char* s) {
        auto v = big_integer::parse(s, 16);
        EXPECT_TRUE(v.has_value()) << s;
        return v ? *v : big_integer();
    }

    std::string hx(const big_integer& v) {
        auto s = v.to_string(16);
        return std::string(s.data(), s.size());
    }

    uint64_t fnv(const string& s) {
        uint64_t h = 0xcbf29ce484222325ull;
        for (char c : s) {
            h ^= uint8_t(c);
            h *= 0x100000001b3ull;
        }
        return h;
    }

    uint64_t splitmix(uint64_t& state) {
        state += 0x9e3779b97f4a7c15ull;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    // As tools/math_vectors.py makes an operand (tests/math/fast.cpp)
    big_integer operand(uint64_t seed, size_t n, int shape) {
        big_integer v;
        uint64_t s = seed;
        std::vector<uint64_t> limbs(n);
        for (auto& x : limbs) {
            x = splitmix(s);
            if (shape == 1) {
                x = ~uint64_t(0);
            } else if (shape == 2) {
                uint64_t y = splitmix(s);
                if (y % 8 == 0) {
                    x = ~uint64_t(0);
                } else if (y % 8 == 1) {
                    x = 0;
                }
            }
        }
        if (!limbs.back()) {
            limbs.back() = 1;
        }
        for (size_t i = n; i-- > 0;) {
            v = (v << 64) | big_integer(limbs[i]);
        }
        return v;
    }

    big_integer random_value(std::mt19937_64& rng, size_t limbs) {
        size_t n = size_t(rng() % limbs) + 1;
        big_integer v = operand(rng(), n, int(rng() % 3));
        return rng() & 1 ? -v : v;
    }
}

TEST(Number_Tests, PowAgainstPython) {
    for (auto& t : number_vectors::pow) {
        big_integer a = H(t.a);
        auto r = a.pow(t.e).to_string(16);
        ASSERT_EQ(r.size(), t.length) << t.a << " ^ " << t.e;
        ASSERT_EQ(fnv(r), t.result) << t.a << " ^ " << t.e;
    }
    EXPECT_EQ(big_integer(0).pow(0), 1);
    EXPECT_EQ(big_integer(-2).pow(63), INT64_MIN);
    EXPECT_EQ(big_integer(-2).pow(64), big_integer(1) << 64);
    EXPECT_EQ(big_integer(2).pow(62), int64_t(1) << 62);
    EXPECT_THROW(big_integer(2).pow(-1), std::domain_error);
    EXPECT_THROW(big_integer(3).pow(INT64_MAX), std::length_error);
    EXPECT_THROW(big_integer(2).pow(INT64_MAX), std::length_error);
    EXPECT_EQ(big_integer(1).pow(INT64_MAX), 1);
    EXPECT_EQ(big_integer(-1).pow(INT64_MAX), -1);
    EXPECT_EQ(big_integer(0).pow(INT64_MAX), 0);
}

TEST(Number_Tests, SqrtAgainstPython) {
    for (auto& t : number_vectors::sqrt) {
        big_integer a = H(t.a);
        ASSERT_EQ(hx(a.sqrt()), t.root) << t.a;
    }
    for (auto& t : number_vectors::long_sqrt) {
        big_integer a = operand(t.seed, t.n, 0);
        big_integer s = a.sqrt();
        ASSERT_EQ(fnv(s.to_string(16)), t.root) << t.n << " limbs";
        ASSERT_LE(s * s, a);
        ASSERT_GT((s + 1) * (s + 1), a);
    }
    EXPECT_THROW(big_integer(-1).sqrt(), std::domain_error);
    EXPECT_THROW((-(big_integer(1) << 200)).sqrt(), std::domain_error);
}

TEST(Number_Tests, GcdAgainstPython) {
    for (auto& t : number_vectors::gcd) {
        big_integer a = H(t.a);
        big_integer b = H(t.b);
        ASSERT_EQ(fnv(a.gcd(b).to_string(16)), t.gcd) << t.a << " gcd " << t.b;
        ASSERT_EQ(fnv(b.gcd(a).to_string(16)), t.gcd) << t.b << " gcd " << t.a;
        ASSERT_EQ(fnv(a.lcm(b).to_string(16)), t.lcm) << t.a << " lcm " << t.b;
    }
    for (auto& t : number_vectors::long_gcd) {
        big_integer g = operand(t.seed, t.ng, 0) << t.shift;
        big_integer a = g * operand(t.seed + 1, t.na, 2);
        big_integer b = -(g * operand(t.seed + 2, t.nb, 2));
        ASSERT_EQ(fnv(a.gcd(b).to_string(16)), t.gcd) << t.na << ", " << t.nb << " limbs";
        ASSERT_EQ(fnv(a.lcm(b).to_string(16)), t.lcm) << t.na << ", " << t.nb << " limbs";
    }
}

TEST(Number_Tests, ModPowAgainstPython) {
    for (auto& t : number_vectors::mod_pow) {
        big_integer a = H(t.a);
        big_integer e = H(t.e);
        big_integer m = H(t.m);
        ASSERT_EQ(hx(a.mod_pow(e, m)), t.result) << t.a << " ^ " << t.e << " mod " << t.m;
    }
    EXPECT_THROW(big_integer(2).mod_pow(-1, 7), std::domain_error);
    EXPECT_THROW(big_integer(2).mod_pow(3, 0), std::domain_error);
    EXPECT_THROW(big_integer(2).mod_pow(3, -7), std::domain_error);
}

TEST(Number_Tests, ModInverseAgainstPython) {
    for (auto& t : number_vectors::mod_inverse) {
        big_integer a = H(t.a);
        big_integer m = H(t.m);
        auto x = a.mod_inverse(m);
        if (!*t.inverse) {
            ASSERT_FALSE(x.has_value()) << t.a << " mod " << t.m;
        } else {
            ASSERT_TRUE(x.has_value()) << t.a << " mod " << t.m;
            ASSERT_EQ(hx(*x), t.inverse) << t.a << " mod " << t.m;
            ASSERT_EQ((a * *x).mod(m), m.abs() == 1 ? 0 : 1);
        }
    }
    EXPECT_THROW((void)big_integer(3).mod_inverse(0), std::domain_error);
}

TEST(Number_Tests, FactorialAndBinomialAgainstPython) {
    for (auto& t : number_vectors::factorial) {
        auto r = big_integer::factorial(t.n).to_string(16);
        ASSERT_EQ(r.size(), t.length) << t.n << "!";
        ASSERT_EQ(fnv(r), t.result) << t.n << "!";
    }
    for (auto& t : number_vectors::binomial) {
        ASSERT_EQ(fnv(big_integer::binomial(t.n, t.k).to_string(16)), t.result) << t.n << " choose " << t.k;
    }
    EXPECT_THROW(big_integer::factorial(-1), std::domain_error);
    EXPECT_THROW(big_integer::binomial(-1, 0), std::domain_error);
    EXPECT_THROW(big_integer::binomial(5, -1), std::domain_error);
}

// The laws the answers obey, over random operands of different lengths
TEST(Number_Tests, AlgebraicProperties) {
    std::mt19937_64 rng(20260925);
    for (int i = 0; i < 600; ++i) {
        big_integer a = random_value(rng, i < 500 ? 12 : 80);
        big_integer b = random_value(rng, i < 500 ? 12 : 80);
        big_integer g = a.gcd(b);
        ASSERT_GE(g, 0);
        if (g != 0) {
            ASSERT_EQ(a % g, 0);
            ASSERT_EQ(b % g, 0);
            ASSERT_EQ((a / g).gcd(b / g), 1);
            ASSERT_EQ(g * a.lcm(b), (a * b).abs());
        }
        big_integer m = b.abs() + 2;
        auto x = a.mod_inverse(m);
        ASSERT_EQ(x.has_value(), a.gcd(m) == 1);
        if (x) {
            ASSERT_EQ((a * *x).mod(m), 1);
            ASSERT_LT(*x, m);
            ASSERT_GE(*x, 0);
        }
        big_integer n = a.abs();
        big_integer s = n.sqrt();
        ASSERT_LE(s * s, n);
        ASSERT_GT((s + 1) * (s + 1), n);
        ASSERT_EQ((n * n).sqrt(), n);
        int64_t e = int64_t(rng() % 40);
        ASSERT_EQ(a.mod_pow(e, m), a.pow(e).mod(m)) << i;
        ASSERT_EQ(a.mod_pow(e, m * 2), a.pow(e).mod(m * 2)) << i;   // an even modulus
    }
}

// Every number below 2^16 against a sieve, and the sign and the small cases
TEST(Number_Tests, PrimesBelowTwoToTheSixteenth) {
    std::vector<bool> composite(65536, false);
    composite[0] = composite[1] = true;
    for (size_t p = 2; p * p < composite.size(); ++p) {
        if (!composite[p]) {
            for (size_t q = p * p; q < composite.size(); q += p) {
                composite[q] = true;
            }
        }
    }
    for (int64_t n = 0; n < 65536; ++n) {
        ASSERT_EQ(big_integer(n).is_probable_prime(), !composite[size_t(n)]) << n;
    }
    EXPECT_FALSE(big_integer(-7).is_probable_prime());
    EXPECT_FALSE(big_integer(-2).is_probable_prime());
    EXPECT_THROW((void)big_integer(7).is_probable_prime(-1), std::domain_error);
    EXPECT_TRUE(big_integer(7).is_probable_prime(0));
}

TEST(Number_Tests, PrimesAgainstGo) {
    size_t primes = 0;
    for (auto& t : prime_vectors::cases) {
        big_integer v = H(t.value);
        ASSERT_EQ(v.is_probable_prime(), t.prime) << t.value;
        ASSERT_EQ(v.is_probable_prime(0), t.prime) << t.value;
        primes += t.prime;
    }
    EXPECT_GT(primes, 250u);
}

// The composites that fool the parts of the test, each alone
TEST(Number_Tests, Pseudoprimes) {
    // Carmichael numbers: every coprime base passes Fermat's test
    for (int64_t n : {561LL, 1105LL, 1729LL, 2465LL, 2821LL, 6601LL, 8911LL, 41041LL, 825265LL, 321197185LL, 5394826801LL,
                      232250619601LL, 9746347772161LL}) {
        EXPECT_FALSE(big_integer(n).is_probable_prime()) << n;
    }
    // Strong pseudoprimes to base 2 (OEIS A001262), and composites strong
    // to every prime base up to 31, 37 and 41 (Jaeschke; Zhang)
    const int64_t spsp2[] = {2047, 3277, 4033, 4681, 8321, 15841, 29341, 42799, 49141, 52633, 65281, 74665, 80581, 85489, 88357, 90751};
    for (int64_t n : spsp2) {
        EXPECT_FALSE(big_integer(n).is_probable_prime()) << n;
    }
    for (const char* n : {"3825123056546413051", "318665857834031151167461", "3317044064679887385961981"}) {
        EXPECT_FALSE(big_integer::parse(n)->is_probable_prime()) << n;
    }
    // Strong Lucas pseudoprimes with Selfridge's parameters (OEIS A217255)
    const int64_t slpsp[] = {5459, 5777, 10877, 16109, 18971, 22499, 24569, 25199, 40309, 58519, 75077, 97439};
    for (int64_t n : slpsp) {
        EXPECT_FALSE(big_integer(n).is_probable_prime()) << n;
    }
    // The two halves alone: each list passes its own test and fails the
    // other's, which is what makes Baillie–PSW
    auto halves = [](int64_t n, bool& strong2, bool& lucas) {
        math::detail::Limb p = math::detail::Limb(n);
        math::detail::Montgomery mg(&p, 1);
        math::detail::Limb d = p - 1;
        size_t s = size_t(std::countr_zero(d));
        d >>= s;
        math::detail::Limb two = 2;
        math::detail::Limb base;
        mg.to(&base, &two, 1);
        strong2 = math::detail::strong_probable_prime(mg, &base, &d, 1, s);
        lucas = math::detail::strong_lucas_probable_prime(mg, &p, 1);
    };
    for (int64_t n : spsp2) {
        bool strong2;
        bool lucas;
        halves(n, strong2, lucas);
        EXPECT_TRUE(strong2) << n;
        EXPECT_FALSE(lucas) << n;
    }
    for (int64_t n : slpsp) {
        bool strong2;
        bool lucas;
        halves(n, strong2, lucas);
        EXPECT_FALSE(strong2) << n;
        EXPECT_TRUE(lucas) << n;
    }
    // Primes pass both halves
    for (int64_t n : {65537LL, 1000000007LL, 2147483647LL}) {
        bool strong2;
        bool lucas;
        halves(n, strong2, lucas);
        EXPECT_TRUE(strong2 && lucas) << n;
    }
}

TEST(Number_Tests, MersenneNumbers) {
    for (int p : {61, 89, 107, 127, 521, 607, 1279}) {
        EXPECT_TRUE(((big_integer(1) << p) - 1).is_probable_prime()) << "M" << p;
    }
    for (int p : {67, 257, 1277}) {
        EXPECT_FALSE(((big_integer(1) << p) - 1).is_probable_prime()) << "M" << p;
    }
    // A product of two primes, one about twice the other: a strong liar
    // for a quarter of the bases
    big_integer p = (big_integer(1) << 127) - 1;
    EXPECT_FALSE((p * ((big_integer(1) << 521) - 1)).is_probable_prime());
    // A square of a prime: the Lucas test needs it caught before, since
    // no D has (D/n) = -1 for a square. The squares of the Wieferich
    // primes 1093 and 3511 are strong pseudoprimes to base 2, so nothing
    // else stops them on the way there.
    EXPECT_FALSE((p * p).is_probable_prime());
    EXPECT_FALSE(big_integer(1093 * 1093).is_probable_prime());
    EXPECT_FALSE(big_integer(3511 * 3511).is_probable_prime());
    EXPECT_FALSE(big_integer(1000003LL * 1000003LL).is_probable_prime());
}

// random's draw below a big_integer: the same stream as next_int(int64_t)
// for a bound that fits, below the bound, the same for the same seed, and
// every value of a small range reached as often as the others
TEST(Number_Tests, RandomBelowABigBound) {
    math::random a(7);
    math::random b(7);
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(a.next_int(big_integer(1000)), b.next_int(1000));
    }
    EXPECT_THROW(a.next_int(big_integer(0)), std::domain_error);
    EXPECT_THROW(a.next_int(-(big_integer(1) << 100)), std::domain_error);
    for (big_integer bound : {big_integer(1) << 64, (big_integer(1) << 64) + 1, (big_integer(1) << 200) - 1,
                              (big_integer(3) << 190), big_integer(UINT64_MAX)}) {
        math::random r(11);
        math::random again(11);
        for (int i = 0; i < 200; ++i) {
            big_integer v = r.next_int(bound);
            ASSERT_GE(v, 0);
            ASSERT_LT(v, bound);
            ASSERT_EQ(again.next_int(bound), v);
        }
    }
    // (2^64 + 3) · k for a small k: the high part is uniform over [0, k)
    // only when the rejection is right
    math::random r(3);
    big_integer unit = (big_integer(1) << 64) + 3;
    int counts[5] = {};
    for (int i = 0; i < 50000; ++i) {
        ++counts[*(r.next_int(unit * 5) / unit).to_int64()];
    }
    double chi = 0;
    for (int c : counts) {
        chi += (c - 10000.0) * (c - 10000.0) / 10000.0;
    }
    EXPECT_LT(chi, 20.0);   // four degrees of freedom: p < 0.0005
}

// π by Chudnovsky's series with binary splitting, whole numbers only,
// against Machin's formula — two formulas with nothing in common — to
// five thousand digits
TEST(Number_Tests, PiTwoWays) {
    const int64_t digits = 5000;
    big_integer unity = big_integer(10).pow(digits + 10);
    // Machin: π = 16·arccot(5) - 4·arccot(239)
    auto arccot = [&](int64_t x) {
        big_integer term = unity / x;
        big_integer sum = term;
        big_integer x2 = x * x;
        for (int64_t n = 3; term != 0; n += 2) {
            term /= x2;
            sum += (n / 2 % 2 ? -term : term) / n;
        }
        return sum;
    };
    big_integer machin = 4 * (4 * arccot(5) - arccot(239));
    // Chudnovsky: 1/π = 12 Σ (-1)^k (6k)! (13591409 + 545140134k) /
    // ((3k)! (k!)^3 640320^(3k + 3/2)), split over [a, b)
    struct Split {
        big_integer p, q, t;
    };
    const big_integer c3_24 = big_integer(640320).pow(3) / 24;
    std::function<Split(int64_t, int64_t)> split = [&](int64_t a, int64_t b) -> Split {
        if (b - a == 1) {
            big_integer p = a == 0 ? big_integer(1) : big_integer(6 * a - 5) * (2 * a - 1) * (6 * a - 1);
            big_integer q = a == 0 ? big_integer(1) : big_integer(a) * a * a * c3_24;
            big_integer t = p * (13591409 + 545140134 * a);
            return {p, q, a % 2 ? -t : t};
        }
        int64_t m = (a + b) / 2;
        Split l = split(a, m);
        Split r = split(m, b);
        return {l.p * r.p, l.q * r.q, l.t * r.q + l.p * r.t};
    };
    Split s = split(0, digits / 14 + 2);
    big_integer root = (10005 * unity * unity).sqrt();
    big_integer chudnovsky = (426880 * root * s.q) / s.t;
    auto a = machin.to_string();
    auto b = chudnovsky.to_string();
    std::string ma(a.data(), size_t(digits));
    std::string mb(b.data(), size_t(digits));
    EXPECT_EQ(ma, mb);
    EXPECT_EQ(ma.substr(0, 20), "31415926535897932384");
}
