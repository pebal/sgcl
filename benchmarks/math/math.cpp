//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
// decimal (sgcl only; n is the number of digits of the operands, which
// have two of them after the point; the same cases as benchmarks/math/
// decimal_python.py over Python's decimal, and Go's big.Rat for orientation):
//   dadd      a + b
//   daddmix   a + b, b at scale 4: one of the two brought to the other
//   dmul      a * b
//   dmoney    (a * b).rescale(2): a price times a rate, back to cents
//   ddiv      a.div_precision(b, n): the quotient to n digits (Python's
//             context of precision n)
//   dsqrt     2.sqrt(n)
//   dparse    parse() of the text of a
//   dformat   to_string() of a
//   dsum      sum += x over a thousand values of n digits, per term
//
// algebra (sgcl, or plain: the same arithmetic as the plain loops the
// compiler vectorizes as it likes, against the NEON / SSE2 paths):
//   mat4mul   a * b of two 4×4 matrices
//   mat4apply apply() of a 4×4 matrix to n vectors, per vector
//   affine    the composition of two affine transforms and a point through it
//
// curves (sgcl; no reference: Go has none, and these are measured as they are):
//   ease      easing::ease (CSS's cubic-bezier, solved for the progress) at
//             a progress that changes each time
//   bounce    easing::ease_in_out_bounce likewise
//   flatten   a cubic Bézier of about 200 units flattened within 0.25, per
//             curve (into a vector kept from one to the next)
//   curvelen  the length of the same cubic, to 1e-3
//
// fft (sgcl; the reference is Apple's vDSP, measured by a scratch program,
// Go having no FFT):
//   fft       a forward and an inverse complex transform of n doubles in
//             place, reported per transform
//   fftf      the same in float
//   rfftf     forward_real of n floats
//   dftf      a forward complex transform of n floats, n not a power of two
//
// big_float (sgcl; n is the precision in bits, Go's big.Float the same):
//   fadd      a + b
//   fmul      a * b
//   fdiv      a / b
//   fsqrt     a.sqrt()
//   ftext     to_string(), the shortest decimal that reads back (Go's
//             Text('g', -1))
//   fparse    parse() of that text
//
// statistics (sgcl; the reference is Python's statistics module by
// benchmarks/math/statistics_python.py for the functions of a sequence, none
// for the accumulators: Go has none; n values, a million by default):
//   ssummary  summary::add, per value
//   sdigest   t_digest::add, per value
//   squantile t_digest::quantile of a digest of n values
//   shist     histogram::add into 30 exponential buckets, per value
//   smean     mean() of n values, per value
//   smedian   median() of n values, per value
//   spct      quantile(values, 0.99) of n values, per value
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
#include <complex>
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

    // n digits, the first not zero, with the point before the last two
    std::string decimal_text(std::mt19937_64& rng, long n) {
        std::string t;
        for (long i = 0; i < n; ++i) {
            t += char('0' + (i ? rng() % 10 : 1 + rng() % 9));
        }
        if (n > 2) {
            t.insert(t.end() - 2, '.');
        }
        return t;
    }

    double float_op(const char* op, long n) {
        using math::big_float;
        std::mt19937_64 rng(1);
        auto bits = uint32_t(n ? n : 256);
        long limbs = long(bits + 63) / 64;
        auto value = [&] {
            big_integer m = random_big(rng, limbs) >> (uint64_t(limbs) * 64 - bits);   // bits bits
            return big_float(math::rational(m, big_integer(1) << (bits / 2)), bits);
        };
        big_float a = value();
        big_float b = value();
        auto run = [&](auto f) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(uint64_t(f().sign()));
                }
            });
        };
        if (!std::strcmp(op, "fadd")) {
            return run([&] { return a + b; });
        }
        if (!std::strcmp(op, "fmul")) {
            return run([&] { return a * b; });
        }
        if (!std::strcmp(op, "fdiv")) {
            return run([&] { return a / b; });
        }
        if (!std::strcmp(op, "fsqrt")) {
            return run([&] { return a.sqrt(); });
        }
        if (!std::strcmp(op, "ftext")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(a.to_string().size());
                }
            });
        }
        if (!std::strcmp(op, "fparse")) {
            string text = a.to_string();
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(uint64_t(big_float::parse(text, bits)->sign()));
                }
            });
        }
        return -1;
    }

    double decimal_op(const char* op, long n) {
        using math::decimal;
        std::mt19937_64 rng(1);
        std::string at = decimal_text(rng, n);
        decimal a(string(at.c_str()));
        decimal b(string(decimal_text(rng, n).c_str()));
        if (!std::strcmp(op, "dadd")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    decimal c = a + b;
                    use(uint64_t(c.sign()));
                }
            });
        }
        if (!std::strcmp(op, "daddmix")) {
            decimal b4(b.unscaled(), 4);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    decimal c = a + b4;
                    use(uint64_t(c.sign()));
                }
            });
        }
        if (!std::strcmp(op, "dmul")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    decimal c = a * b;
                    use(uint64_t(c.sign()));
                }
            });
        }
        if (!std::strcmp(op, "dmoney")) {
            decimal rate("0.0825");
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    decimal c = (a * rate).rescale(2);
                    use(uint64_t(c.sign()));
                }
            });
        }
        if (!std::strcmp(op, "ddiv")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    decimal c = a.div_precision(b, int32_t(n));
                    use(uint64_t(c.sign()));
                }
            });
        }
        if (!std::strcmp(op, "dsqrt")) {
            decimal two(2);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    decimal c = two.sqrt(int32_t(n));
                    use(uint64_t(c.sign()));
                }
            });
        }
        if (!std::strcmp(op, "dparse")) {
            string text(at.c_str());
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(uint64_t(decimal::parse(text)->scale()));
                }
            });
        }
        if (!std::strcmp(op, "dformat")) {
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    use(a.to_string().size());
                }
            });
        }
        if (!std::strcmp(op, "dsum")) {
            sgcl::vector<decimal> values;
            for (int i = 0; i < 1000; ++i) {
                values.push_back(decimal(string(decimal_text(rng, n).c_str())));
            }
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    decimal sum;
                    for (auto& v : values) {
                        sum += v;
                    }
                    use(uint64_t(sum.sign()));
                }
            }) / 1000.0;
        }
        return -1;
    }

    double algebra_op(bool plain, const char* op, long n) {
        using math::mat4;
        using math::vec4;
        std::mt19937 rng(1);
        std::uniform_real_distribution<float> d(-1, 1);
        auto random_mat = [&] {
            mat4 m;
            for (int i = 0; i < 16; ++i) {
                (&m.columns[0].x)[i] = d(rng);
            }
            return m;
        };
        if (!std::strcmp(op, "mat4mul")) {
            // a chain, so that one product waits on the one before, as in a
            // scene graph's walk
            mat4 a = random_mat();
            mat4 b = random_mat();
            return measure([&](long count) {
                mat4 r = a;
                for (long i = 0; i < count; ++i) {
                    r = plain ? math::detail::multiply_plain(r, b) : r * b;
                    r.columns[0].x *= 0.5f;   // kept bounded
                }
                use(uint64_t(r.columns[3].w != 0));
            });
        }
        if (!std::strcmp(op, "mat4apply")) {
            // a rotation, so that the vectors stay as long as they were
            // however many times it is applied
            mat4 m = mat4::rotation({1, 2, 3}, 0.5f);
            sgcl::vector<vec4> v(size_t(n ? n : 1000));
            for (auto& x : v) {
                x = vec4(d(rng), d(rng), d(rng), 1);
            }
            slice<vec4> all = v;
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    if (plain) {
                        math::detail::apply_plain(m, v.data(), v.size());
                    } else {
                        m.apply(all);
                    }
                }
                use(uint64_t(v[1].x != 0));
            }) / double(v.size());
        }
        if (!std::strcmp(op, "ease") || !std::strcmp(op, "bounce")) {
            const math::easing& e = op[0] == 'e' ? math::easing::ease : math::easing::ease_in_out_bounce;
            return measure([&](long count) {
                float sum = 0;
                float x = 0;
                for (long i = 0; i < count; ++i) {
                    sum += e(x);
                    x += 0.000123f;
                    x = x > 1 ? x - 1 : x;
                }
                use(uint64_t(sum));
            });
        }
        math::cubic_bezier curve{{0, 0}, {30, 180}, {170, 190}, {200, 20}};
        if (!std::strcmp(op, "flatten")) {
            sgcl::vector<math::point> line;
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    line.clear();
                    line.push_back(curve.p0);
                    curve.flatten(line, 0.25f);
                }
                use(line.size());
            });
        }
        if (!std::strcmp(op, "curvelen")) {
            return measure([&](long count) {
                float sum = 0;
                for (long i = 0; i < count; ++i) {
                    curve.p3.y = float(i & 15);
                    sum += curve.length();
                }
                use(uint64_t(sum));
            });
        }
        if (!std::strcmp(op, "fft") || !std::strcmp(op, "fftf") || !std::strcmp(op, "dftf")) {
            size_t len = size_t(n ? n : 1024);
            math::fft plan(len);
            auto run = [&](auto zero) {
                using C = std::complex<decltype(zero)>;
                std::vector<C> x(len);
                for (size_t i = 0; i < len; ++i) {
                    x[i] = C(decltype(zero)(std::sin(double(i))), decltype(zero)(std::cos(double(i) * 0.3)));
                }
                bool both = std::strcmp(op, "dftf") != 0;
                double ns = measure([&](long count) {
                    for (long i = 0; i < count; ++i) {
                        plan.forward(x);
                        if (both) {
                            plan.inverse(x);
                        }
                    }
                    use(uint64_t(x[1].real() != 0));
                });
                return both ? ns / 2 : ns;
            };
            return !std::strcmp(op, "fft") ? run(0.0) : run(0.0f);
        }
        if (!std::strcmp(op, "rfftf")) {
            size_t len = size_t(n ? n : 1024);
            math::fft plan(len);
            std::vector<float> x(len);
            for (size_t i = 0; i < len; ++i) {
                x[i] = float(std::sin(double(i)));
            }
            std::vector<std::complex<float>> out(len / 2 + 1);
            return measure([&](long count) {
                for (long i = 0; i < count; ++i) {
                    plan.forward_real(x, out);
                }
                use(uint64_t(out[1].real() != 0));
            });
        }
        if (op[0] == 's' && op[1] != 'm' && std::strcmp(op, "small") && std::strcmp(op, "sum") && std::strcmp(op, "sqr")
            && std::strcmp(op, "sqrt") && std::strcmp(op, "shuffle")) {
            size_t count = size_t(n ? n : 1000000);
            std::mt19937_64 g(5);
            std::lognormal_distribution<double> d(0, 1);
            std::vector<double> values(count);
            for (auto& v : values) {
                v = d(g);
            }
            if (!std::strcmp(op, "ssummary")) {
                return measure([&](long times) {
                    for (long i = 0; i < times; ++i) {
                        math::summary s(values);
                        use(uint64_t(s.mean() > 0));
                    }
                }) / double(count);
            }
            if (!std::strcmp(op, "sdigest")) {
                return measure([&](long times) {
                    for (long i = 0; i < times; ++i) {
                        math::t_digest t;
                        for (double v : values) {
                            t.add(v);
                        }
                        use(t.count());
                    }
                }) / double(count);
            }
            if (!std::strcmp(op, "squantile")) {
                math::t_digest t;
                for (double v : values) {
                    t.add(v);
                }
                double q = 0;
                return measure([&](long times) {
                    for (long i = 0; i < times; ++i) {
                        q = q > 0.99 ? 0.01 : q + 0.001;
                        use(uint64_t(t.quantile(q) > 0));
                    }
                });
            }
            if (!std::strcmp(op, "shist")) {
                math::histogram h = math::histogram::exponential(0.01, 1.5, 30);
                return measure([&](long times) {
                    for (long i = 0; i < times; ++i) {
                        for (double v : values) {
                            h.add(v);
                        }
                    }
                    use(h.count());
                }) / double(count);
            }
            if (!std::strcmp(op, "smedian") || !std::strcmp(op, "spct")) {
                bool median = op[1] == 'm';
                return measure([&](long times) {
                    for (long i = 0; i < times; ++i) {
                        use(uint64_t((median ? math::median(values) : math::quantile(values, 0.99)) > 0));
                    }
                }) / double(count);
            }
        }
        if (!std::strcmp(op, "smean") || !std::strcmp(op, "smedian")) {
            size_t count = size_t(n ? n : 1000000);
            std::mt19937_64 g(5);
            std::lognormal_distribution<double> d(0, 1);
            std::vector<double> values(count);
            for (auto& v : values) {
                v = d(g);
            }
            bool median = op[1] == 'm' && op[2] == 'e' && op[3] == 'd';
            return measure([&](long times) {
                for (long i = 0; i < times; ++i) {
                    use(uint64_t((median ? math::median(values) : math::mean(values)) > 0));
                }
            }) / double(count);
        }
        if (!std::strcmp(op, "affine")) {
            math::affine a = math::affine::rotation(0.3f) * math::affine::translation(1, 2);
            math::affine b = math::affine::scaling(1.0001f, 0.9999f);
            return measure([&](long count) {
                math::point p(1, 1);
                for (long i = 0; i < count; ++i) {
                    a = a * b;
                    p = a.apply(p);
                }
                use(uint64_t(p.x != 0));
            });
        }
        return -1;
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
    if (argc < 3 || !bench::has_variant(argv[1], {"sgcl", "std", "plain"})) {
        std::fprintf(stderr, "usage: bench_math <sgcl|std> <op> [n]\n"
                             "  big_integer (sgcl): add mul sqr div tostr parse sum fact small pow modpow gcd modinv sqrt prime pi\n"
                             "    factorial binomial fib harmonic, cross <n> <threshold>\n"
                             "  decimal (sgcl): dadd daddmix dmul dmoney ddiv dsqrt dparse dformat dsum\n"
                             "  algebra (sgcl, plain): mat4mul mat4apply affine\n"
                             "  curves (sgcl): ease bounce flatten curvelen\n"
                             "  fft (sgcl): fft fftf rfftf dftf\n"
                             "  big_float (sgcl): fadd fmul fdiv fsqrt ftext fparse\n"
                             "  statistics (sgcl): ssummary sdigest squantile shist smean smedian spct\n"
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
    if (double ns = algebra_op(!std::strcmp(variant, "plain"), op, n); ns >= 0) {
        std::printf("%s op=%s n=%ld ns/op=%.2f\n", variant, op, n, ns);
        return 0;
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
        ns = op[0] == 'd' && std::strcmp(op, "div") && std::strcmp(op, "double") ? decimal_op(op, n)
             : op[0] == 'f' && std::strcmp(op, "fact") && std::strcmp(op, "factorial") && std::strcmp(op, "fib")
                 ? float_op(op, n)
                 : big_op(op, n);
    }
    if (ns < 0) {
        std::fprintf(stderr, "math: no op called %s\n", op);
        return 1;
    }
    std::printf("%s op=%s n=%ld ns/op=%.2f\n", variant, op, n, ns);
}
