//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Cost of math::big_integer and math::random, against Go's math/big and
// math/rand/v2 (benchmarks/go/math, the same cases under the same names)
// and, for random, against the standard library's mt19937_64 with its
// distributions.
//   math <sgcl|std> <op> [n]
//
// big_integer (sgcl only; n is the length of the operands in limbs of 64
// bits, or in decimal digits for the conversions):
//   add       a + b, both n limbs
//   mul       a * b, both n limbs
//   sqr       a * a, n limbs
//   div       a / b, a of 2n limbs and b of n
//   tostr     to_string() of a number of n decimal digits
//   parse     parse() of n decimal digits
//   sum       sum += x a thousand times, x of n limbs: written in place,
//             sum being nobody else's (big_integer.h)
//   fact      f *= i for i up to n, one step reported
//   small     a * b + c over values that stay within int64_t
//   pow       3.pow(e) with e such that the result has n limbs
//   modpow    a.mod_pow(e, m), all three of n bits, m odd (a private
//             key's exponentiation, as a measure of the arithmetic)
//   gcd       a.gcd(b), both n limbs
//   modinv    a.mod_inverse(m), both n limbs, m odd
//   sqrt      a.sqrt() of a number of n limbs
//   prime     is_probable_prime() of a prime of n bits (20 rounds)
//   pi        n decimal digits of π: Chudnovsky's series by binary
//             splitting, a square root and the digits written
//   factorial big_integer::factorial(n)
//   binomial  big_integer::binomial(n, n / 2)
//   fib       the n-th Fibonacci number by doubling
//   harmonic  the rational sum 1/1 + 1/2 + … + 1/n, reported per term
//   cross     where an algorithm takes over from the one below it: the
//             operation at n limbs with the threshold named by the fourth
//             argument (karatsuba, toom3, square_karatsuba, square_toom3,
//             burnikel_ziegler, to_string, parse) at n + 1, so that the
//             top level takes the road below, and at n, so that it takes
//             the road above; prints both and their ratio (A/B for
//             setting the thresholds of sgcl/math/detail/mul.h)
//
// random (sgcl or std; n is ignored):
//   uint64    next_uint64 (std: mt19937_64())
//   intn      next_int(1000) (std: uniform_int_distribution)
//   double    next_double (std: generate_canonical)
//   normal    next_normal (std: normal_distribution)
//   exp       next_exponential (std: exponential_distribution)
//   shuffle   a shuffle of a million ints, reported per element
//             (std: std::shuffle)
//   make      math::random() made and one draw taken, the cost of a
//             default generator in a loop (std: random_device and
//             mt19937_64 seeded from it)
//
// Each op is repeated, doubling the count, until a run takes half a
// second; prints nanoseconds per operation.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"
#include "sgcl/math/math.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace sgcl;
using math::big_integer;

namespace {
    volatile uint64_t sink;

    inline void use(uint64_t v) {
        sink = sink ^ v;
    }

    big_integer random_big(std::mt19937_64& rng, long limbs) {
        big_integer v = 1;
        for (long i = 0; i < limbs; ++i) {
            v = (v << 64) | big_integer(rng());
        }
        return v >> 1;   // n limbs, the top one full
    }

    // Chudnovsky's series for π by binary splitting over [a, b): P, Q and T
    // of the half-open range, merged as P = P1·P2, Q = Q1·Q2, T = T1·Q2 +
    // P1·T2; π = 426880·√10005·Q / T
    struct Split {
        big_integer p, q, t;
    };

    Split chudnovsky(int64_t a, int64_t b, const big_integer& c3_24) {
        if (b - a == 1) {
            big_integer p = a == 0 ? big_integer(1) : big_integer(6 * a - 5) * (2 * a - 1) * (6 * a - 1);
            big_integer q = a == 0 ? big_integer(1) : big_integer(a) * a * a * c3_24;
            big_integer t = p * (13591409 + 545140134 * a);
            return {p, q, a % 2 ? -t : t};
        }
        int64_t m = (a + b) / 2;
        Split l = chudnovsky(a, m, c3_24);
        Split r = chudnovsky(m, b, c3_24);
        return {l.p * r.p, l.q * r.q, l.t * r.q + l.p * r.t};
    }

    string pi_digits(long digits) {
        big_integer c3_24 = big_integer(640320).pow(3) / 24;
        Split s = chudnovsky(0, digits / 14 + 2, c3_24);
        big_integer unity = big_integer(10).pow(digits);
        big_integer root = (10005 * unity * unity).sqrt();
        return ((426880 * root * s.q) / s.t).to_string();
    }

    // F(n) by doubling: F(2k) = F(k)·(2F(k+1) - F(k)), F(2k+1) = F(k)² +
    // F(k+1)²
    big_integer fibonacci(long n) {
        big_integer a = 0;
        big_integer b = 1;
        for (int i = 63 - std::countl_zero(uint64_t(n)); i >= 0; --i) {
            big_integer c = a * (2 * b - a);
            big_integer d = a * a + b * b;
            if ((n >> i) & 1) {
                a = d;
                b = c + d;
            } else {
                a = c;
                b = d;
            }
        }
        return a;
    }

    template<class F>
    double measure(F&& f) {
        long count = 1;
        for (;;) {
            auto t0 = bench::Clock::now();
            f(count);
            double s = bench::seconds_since(t0);
            if (s > 0.5 || count > (1L << 40)) {
                return s * 1e9 / double(count);
            }
            count *= 2;
        }
    }

    double big_op(const char* op, long n) {
        std::mt19937_64 rng(1);
        if (!std::strcmp(op, "add")) {
            big_integer a = random_big(rng, n);
            big_integer b = random_big(rng, n);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    big_integer c = a + b;
                    use(c.bit_length());
                }
            });
        }
        if (!std::strcmp(op, "mul")) {
            big_integer a = random_big(rng, n);
            big_integer b = random_big(rng, n);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    big_integer c = a * b;
                    use(c.bit_length());
                }
            });
        }
        if (!std::strcmp(op, "sqr")) {
            big_integer a = random_big(rng, n);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    big_integer c = a * a;
                    use(c.bit_length());
                }
            });
        }
        if (!std::strcmp(op, "div")) {
            big_integer a = random_big(rng, 2 * n);
            big_integer b = random_big(rng, n);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    big_integer c = a / b;
                    use(c.bit_length());
                }
            });
        }
        if (!std::strcmp(op, "tostr") || !std::strcmp(op, "parse")) {
            std::string digits(size_t(n), '0');
            for (auto& c : digits) {
                c = char('0' + rng() % 10);
            }
            digits[0] = '7';
            string text(digits);
            big_integer a = *big_integer::parse(text);
            if (!std::strcmp(op, "tostr")) {
                return measure([&](long count) {
                    for (long i = 0; i < count; ++i) {
                        use(a.to_string().size());
                    }
                });
            }
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(big_integer::parse(text)->bit_length());
                }
            });
        }
        if (!std::strcmp(op, "sum")) {
            big_integer x = random_big(rng, n);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    big_integer s;
                    for (int k = 0; k < 1000; ++k) {
                        s += x;
                    }
                    use(s.bit_length());
                }
            }) / 1000;
        }
        if (!std::strcmp(op, "fact")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    big_integer f = 1;
                    for (long k = 2; k <= n; ++k) {
                        f *= k;
                    }
                    use(f.bit_length());
                }
            }) / double(n);
        }
        if (!std::strcmp(op, "pow")) {
            auto e = int64_t(double(n) * 64 / 1.584962500721156);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(big_integer(3).pow(e).bit_length());
                }
            });
        }
        if (!std::strcmp(op, "modpow")) {
            long limbs = (n + 63) / 64;
            big_integer m = (random_big(rng, limbs) >> (limbs * 64 - n)) | 1;
            big_integer a = random_big(rng, limbs).mod(m);
            big_integer e = random_big(rng, limbs) >> (limbs * 64 - n);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(a.mod_pow(e, m).bit_length());
                }
            });
        }
        if (!std::strcmp(op, "gcd") || !std::strcmp(op, "modinv")) {
            big_integer a = random_big(rng, n);
            big_integer b = random_big(rng, n) | 1;
            bool inverse = !std::strcmp(op, "modinv");
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(inverse ? a.mod_inverse(b).value_or(0).bit_length() : a.gcd(b).bit_length());
                }
            });
        }
        if (!std::strcmp(op, "sqrt")) {
            big_integer a = random_big(rng, n);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(a.sqrt().bit_length());
                }
            });
        }
        if (!std::strcmp(op, "prime")) {
            long limbs = (n + 63) / 64;
            big_integer p = (random_big(rng, limbs) >> (limbs * 64 - n)) | 1;
            while (!p.is_probable_prime()) {
                p += 2;
            }
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(p.is_probable_prime());
                }
            });
        }
        if (!std::strcmp(op, "pi")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(pi_digits(n).size());
                }
            });
        }
        if (!std::strcmp(op, "factorial")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(big_integer::factorial(n).bit_length());
                }
            });
        }
        if (!std::strcmp(op, "binomial")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(big_integer::binomial(n, n / 2).bit_length());
                }
            });
        }
        if (!std::strcmp(op, "fib")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(fibonacci(n).bit_length());
                }
            });
        }
        if (!std::strcmp(op, "harmonic")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    math::rational sum;
                    for (long k = 1; k <= n; ++k) {
                        sum += math::rational(1, k);
                    }
                    use(sum.denominator().bit_length());
                }
            }) / double(n);
        }
        if (!std::strcmp(op, "small")) {
            std::vector<int64_t> values(1024);
            for (auto& v : values) {
                v = int64_t(rng() % 2000000) - 1000000;
            }
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    big_integer a = values[size_t(i) & 1023];
                    big_integer b = values[size_t(i + 1) & 1023];
                    big_integer c = a * b + values[size_t(i + 2) & 1023];
                    use(uint64_t(*c.to_int64()));
                }
            });
        }
        return -1;
    }

    // The threshold A/B of `cross`: below (the threshold one above n) and
    // above (the threshold at n)
    int crossover(const char* which, long n) {
        namespace d = math::detail;
        size_t d::Thresholds::*field = nullptr;
        const char* names[] = {"karatsuba", "toom3", "square_karatsuba", "square_toom3", "burnikel_ziegler", "to_string", "parse"};
        size_t d::Thresholds::*fields[] = {&d::Thresholds::karatsuba, &d::Thresholds::toom3, &d::Thresholds::square_karatsuba,
                                           &d::Thresholds::square_toom3, &d::Thresholds::burnikel_ziegler,
                                           &d::Thresholds::to_string, &d::Thresholds::parse};
        for (size_t i = 0; i < 7; ++i) {
            if (!std::strcmp(which, names[i])) {
                field = fields[i];
            }
        }
        if (!field || n < 2) {
            std::fprintf(stderr, "math: cross <n> <karatsuba|toom3|square_karatsuba|square_toom3|burnikel_ziegler|to_string|parse>\n");
            return 1;
        }
        std::mt19937_64 rng(1);
        big_integer a = random_big(rng, n);
        big_integer b = random_big(rng, n);
        big_integer a2 = random_big(rng, 2 * n);
        std::string digits(size_t(n) * 19, '7');
        for (auto& c : digits) {
            c = char('0' + rng() % 10);
        }
        string text(digits);
        auto run = [&] {
            if (field == &d::Thresholds::karatsuba || field == &d::Thresholds::toom3) {
                return measure([&](long count) { for (long i = 0; i < count; ++i) use((a * b).bit_length()); });
            }
            if (field == &d::Thresholds::square_karatsuba || field == &d::Thresholds::square_toom3) {
                return measure([&](long count) { for (long i = 0; i < count; ++i) use((a * a).bit_length()); });
            }
            if (field == &d::Thresholds::burnikel_ziegler) {
                return measure([&](long count) { for (long i = 0; i < count; ++i) use((a2 / b).bit_length()); });
            }
            if (field == &d::Thresholds::to_string) {
                return measure([&](long count) { for (long i = 0; i < count; ++i) use(a.to_string().size()); });
            }
            return measure([&](long count) { for (long i = 0; i < count; ++i) use(big_integer::parse(text)->bit_length()); });
        };
        auto saved = d::thresholds;
        double below = 1e300;
        double above = 1e300;
        for (int round = 0; round < 3; ++round) {
            d::thresholds.*field = size_t(n) + 1;
            below = std::min(below, run());
            d::thresholds.*field = size_t(n);
            above = std::min(above, run());
        }
        d::thresholds = saved;
        std::printf("sgcl op=cross:%s n=%ld below=%.2f above=%.2f ratio=%.3f\n", which, n, below, above, above / below);
        return 0;
    }

    double random_op(bool std_side, const char* op) {
        math::random r(1);
        std::mt19937_64 m(1);
        if (!std::strcmp(op, "uint64")) {
            return std_side ? measure([&](long count) { for (long i = 0; i < count; ++i) use(m()); })
                            : measure([&](long count) { for (long i = 0; i < count; ++i) use(r.next_uint64()); });
        }
        if (!std::strcmp(op, "intn")) {
            std::uniform_int_distribution<int64_t> d(0, 999);
            return std_side ? measure([&](long count) { for (long i = 0; i < count; ++i) use(uint64_t(d(m))); })
                            : measure([&](long count) { for (long i = 0; i < count; ++i) use(uint64_t(r.next_int(1000))); });
        }
        if (!std::strcmp(op, "double")) {
            return std_side ? measure([&](long count) { for (long i = 0; i < count; ++i) use(uint64_t(std::generate_canonical<double, 53>(m) * 1e6)); })
                            : measure([&](long count) { for (long i = 0; i < count; ++i) use(uint64_t(r.next_double() * 1e6)); });
        }
        if (!std::strcmp(op, "normal")) {
            std::normal_distribution<double> d;
            return std_side ? measure([&](long count) { for (long i = 0; i < count; ++i) use(uint64_t(int64_t(d(m) * 1e6))); })
                            : measure([&](long count) { for (long i = 0; i < count; ++i) use(uint64_t(int64_t(r.next_normal() * 1e6))); });
        }
        if (!std::strcmp(op, "exp")) {
            std::exponential_distribution<double> d;
            return std_side ? measure([&](long count) { for (long i = 0; i < count; ++i) use(uint64_t(int64_t(d(m) * 1e6))); })
                            : measure([&](long count) { for (long i = 0; i < count; ++i) use(uint64_t(r.next_exponential() * 1e6)); });
        }
        if (!std::strcmp(op, "shuffle")) {
            std::vector<int> v(1000000);
            for (size_t i = 0; i < v.size(); ++i) {
                v[i] = int(i);
            }
            return (std_side ? measure([&](long count) { for (long i = 0; i < count; ++i) std::shuffle(v.begin(), v.end(), m); })
                             : measure([&](long count) { for (long i = 0; i < count; ++i) r.shuffle(v); })) / double(v.size());
        }
        if (!std::strcmp(op, "make")) {
            return std_side ? measure([&](long count) {
                                  for (long i = 0; i < count; ++i) {
                                      std::random_device device;
                                      std::mt19937_64 g(device());
                                      use(g());
                                  }
                              })
                            : measure([&](long count) {
                                  for (long i = 0; i < count; ++i) {
                                      math::random g;
                                      use(g.next_uint64());
                                  }
                              });
        }
        return -1;
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || !bench::has_variant(argv[1], {"sgcl", "std"})) {
        std::fprintf(stderr, "usage: bench_math <sgcl|std> <op> [n]\n"
                             "  big_integer (sgcl): add mul sqr div tostr parse sum fact small pow modpow gcd modinv sqrt prime pi\n"
                             "    factorial binomial fib harmonic, cross <n> <threshold>\n"
                             "  random: uint64 intn double normal exp shuffle make\n");
        return 1;
    }
    const char* variant = argv[1];
    const char* op = argv[2];
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    bool std_side = !std::strcmp(variant, "std");
    if (!std_side && !std::strcmp(op, "cross")) {
        return crossover(argc > 4 ? argv[4] : "", n);
    }
    double ns = random_op(std_side, op);
    if (ns < 0) {
        if (std_side) {
            std::fprintf(stderr, "math: %s has no std variant\n", op);
            return 1;
        }
        if (!n) {
            n = !std::strcmp(op, "tostr") || !std::strcmp(op, "parse") || !std::strcmp(op, "fact") || !std::strcmp(op, "pi")
                        || !std::strcmp(op, "factorial") || !std::strcmp(op, "binomial") || !std::strcmp(op, "harmonic") ? 1000
                : !std::strcmp(op, "modpow") || !std::strcmp(op, "prime") ? 1024
                : !std::strcmp(op, "fib") ? 100000 : 10;
        }
        ns = big_op(op, n);
    }
    if (ns < 0) {
        std::fprintf(stderr, "math: no op called %s\n", op);
        return 1;
    }
    std::printf("%s op=%s n=%ld ns/op=%.2f\n", variant, op, n, ns);
}
