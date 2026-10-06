//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// math::fft against the definition: a naive DFT in long double for every
// length from 0 to 130 and for chosen ones up to 4096 (powers of two, primes,
// products of small primes), complex and real, forward and inverse, double
// and float, each held within a few units of the rounding of its precision
// times log n. Then what the definition cannot reach in reasonable time: the
// round trip, Parseval, linearity and the shift theorem at lengths up to
// 2^20; analytic transforms (an impulse, a sinusoid); convolution against
// the direct sum; the NEON stages against the plain ones; the errors of
// lengths; a plan shared by threads.
#include "tests/types.h"
#include "sgcl/math/math.h"

#include <cmath>
#include <complex>
#include <random>
#include <thread>
#include <vector>

using math::fft;
using cd = std::complex<double>;
using cf = std::complex<float>;

namespace {
    // The DFT by its definition, the angles reduced modulo n in integers
    std::vector<std::complex<long double>> dft(const std::vector<std::complex<long double>>& x, bool inverse) {
        size_t n = x.size();
        std::vector<std::complex<long double>> out(n);
        const long double pi = 3.141592653589793238462643383279502884L;
        for (size_t k = 0; k < n; ++k) {
            std::complex<long double> sum = 0;
            for (size_t j = 0; j < n; ++j) {
                long double a = (inverse ? 2 : -2) * pi * (long double)((uint64_t(j) * k) % n) / (long double)n;
                sum += x[j] * std::complex<long double>(std::cos(a), std::sin(a));
            }
            out[k] = inverse ? sum / (long double)n : sum;
        }
        return out;
    }

    template<class T>
    std::vector<std::complex<T>> random_complex(std::mt19937_64& rng, size_t n) {
        std::uniform_real_distribution<double> d(-1, 1);
        std::vector<std::complex<T>> v(n);
        for (auto& c : v) {
            c = {T(d(rng)), T(d(rng))};
        }
        return v;
    }

    // The largest error against the oracle over the largest magnitude, in
    // units of the precision's epsilon
    template<class T>
    double error_units(const std::vector<std::complex<T>>& got, const std::vector<std::complex<long double>>& want) {
        long double worst = 0;
        long double scale = 1e-30L;
        for (size_t i = 0; i < want.size(); ++i) {
            scale = std::max(scale, std::abs(want[i]));
        }
        for (size_t i = 0; i < want.size(); ++i) {
            std::complex<long double> g(got[i].real(), got[i].imag());
            worst = std::max(worst, std::abs(g - want[i]));
        }
        return double(worst / scale / std::numeric_limits<T>::epsilon());
    }

    template<class T>
    std::vector<std::complex<long double>> wide(const std::vector<std::complex<T>>& v) {
        std::vector<std::complex<long double>> w(v.size());
        for (size_t i = 0; i < v.size(); ++i) {
            w[i] = {v[i].real(), v[i].imag()};
        }
        return w;
    }

    // The bound of a transform's error, log2(n) stages of a few roundings
    double bound(size_t n) {
        return 8 + 4 * std::log2(double(std::max<size_t>(n, 2)));
    }

    template<class T>
    void check_complex(size_t n, std::mt19937_64& rng) {
        auto x = random_complex<T>(rng, n);
        auto want = dft(wide(x), false);
        fft plan(n);
        auto y = x;
        plan.forward(y);
        ASSERT_LE(error_units(y, want), bound(n) * (n > 1 && (n & (n - 1)) ? 4 : 1)) << "forward " << n;
        auto back = y;
        plan.inverse(back);
        ASSERT_LE(error_units(back, wide(x)), bound(n) * (n > 1 && (n & (n - 1)) ? 8 : 2)) << "round trip " << n;
        auto inv_want = dft(wide(x), true);
        auto z = x;
        plan.inverse(z);
        ASSERT_LE(error_units(z, inv_want), bound(n) * (n > 1 && (n & (n - 1)) ? 4 : 1)) << "inverse " << n;
    }

    template<class T>
    void check_real(size_t n, std::mt19937_64& rng) {
        std::uniform_real_distribution<double> d(-1, 1);
        std::vector<T> x(n);
        for (auto& v : x) {
            v = T(d(rng));
        }
        std::vector<std::complex<long double>> wx(n);
        for (size_t i = 0; i < n; ++i) {
            wx[i] = x[i];
        }
        auto want = dft(wx, false);
        want.resize(n ? n / 2 + 1 : 0);
        fft plan(n);
        std::vector<std::complex<T>> out(n ? n / 2 + 1 : 0);
        plan.forward_real(x, out);
        ASSERT_LE(error_units(out, want), bound(n) * (n > 1 && (n & (n - 1)) ? 4 : 1)) << "forward_real " << n;
        std::vector<T> back(n);
        plan.inverse_real(out, back);
        std::vector<std::complex<T>> backc(n);
        std::vector<std::complex<T>> xc(n);
        for (size_t i = 0; i < n; ++i) {
            backc[i] = back[i];
            xc[i] = x[i];
        }
        ASSERT_LE(error_units(backc, wide(xc)), bound(n) * (n > 1 && (n & (n - 1)) ? 8 : 2)) << "inverse_real " << n;
    }
}

TEST(Fft_Tests, EveryShortLengthAgainstTheDefinition) {
    std::mt19937_64 rng(1);
    for (size_t n = 0; n <= 130; ++n) {
        check_complex<double>(n, rng);
        check_complex<float>(n, rng);
        check_real<double>(n, rng);
        check_real<float>(n, rng);
    }
}

TEST(Fft_Tests, ChosenLengthsAgainstTheDefinition) {
    std::mt19937_64 rng(2);
    for (size_t n : {256u, 509u, 512u, 625u, 768u, 960u, 1000u, 1024u, 1031u, 2048u, 2187u, 3125u, 4096u}) {
        check_complex<double>(n, rng);
        check_complex<float>(n, rng);
        check_real<double>(n, rng);
        check_real<float>(n, rng);
    }
}

// The theorems a transform keeps, at lengths the definition cannot reach in
// a test: the round trip, Parseval, linearity, the shift theorem
TEST(Fft_Tests, PropertiesAtLargeLengths) {
    std::mt19937_64 rng(3);
    for (size_t n : {size_t(1) << 16, size_t(1) << 20, size_t(65537), size_t(3 * 5 * 7 * 11 * 13 * 17)}) {
        fft plan(n);
        auto x = random_complex<double>(rng, n);
        auto y = random_complex<double>(rng, n);
        auto fx = x;
        plan.forward(fx);
        // Parseval: Σ|x|² = Σ|X|²/n
        long double ex = 0;
        long double efx = 0;
        for (size_t i = 0; i < n; ++i) {
            ex += std::norm(std::complex<long double>(x[i].real(), x[i].imag()));
            efx += std::norm(std::complex<long double>(fx[i].real(), fx[i].imag()));
        }
        EXPECT_NEAR(double(efx / n / ex), 1.0, 1e-12) << n;
        // linearity: F(x + 2y) = F(x) + 2F(y)
        auto fy = y;
        plan.forward(fy);
        auto sum = x;
        for (size_t i = 0; i < n; ++i) {
            sum[i] += 2.0 * y[i];
        }
        plan.forward(sum);
        double worst = 0;
        for (size_t i = 0; i < n; ++i) {
            worst = std::max(worst, std::abs(sum[i] - (fx[i] + 2.0 * fy[i])));
        }
        EXPECT_LT(worst, 1e-9 * std::sqrt(double(n))) << n;
        // the shift theorem: x shifted by one is X times e^(-2πi·k/n)
        auto shifted = x;
        std::rotate(shifted.begin(), shifted.end() - 1, shifted.end());
        plan.forward(shifted);
        worst = 0;
        for (size_t k = 0; k < n; k += 7) {
            double a = -2 * 3.14159265358979323846 * double(k) / double(n);
            worst = std::max(worst, std::abs(shifted[k] - fx[k] * cd(std::cos(a), std::sin(a))));
        }
        EXPECT_LT(worst, 1e-9 * std::sqrt(double(n))) << n;
        // the round trip
        plan.inverse(fx);
        worst = 0;
        for (size_t i = 0; i < n; ++i) {
            worst = std::max(worst, std::abs(fx[i] - x[i]));
        }
        EXPECT_LT(worst, 1e-12) << n;
    }
}

TEST(Fft_Tests, AnalyticTransforms) {
    // an impulse at 0 is all ones; at 1 the roots of unity
    fft plan(8);
    std::vector<cd> x(8);
    x[0] = 1;
    plan.forward(x);
    for (auto& v : x) {
        EXPECT_EQ(v, cd(1, 0));
    }
    // a cosine of frequency 3 over 64 points: n/2 at bins 3 and 61
    fft p64(64);
    std::vector<cd> c(64);
    for (size_t i = 0; i < 64; ++i) {
        c[i] = std::cos(2 * 3.14159265358979323846 * 3 * double(i) / 64);
    }
    p64.forward(c);
    for (size_t k = 0; k < 64; ++k) {
        double want = k == 3 || k == 61 ? 32 : 0;
        EXPECT_NEAR(c[k].real(), want, 1e-12) << k;
        EXPECT_NEAR(c[k].imag(), 0, 1e-12) << k;
    }
    // the example of the page
    std::vector<cd> four = {1, 2, 3, 4};
    fft(4).forward(four);
    EXPECT_EQ(four[0], cd(10, 0));
    EXPECT_EQ(four[1], cd(-2, 2));
    EXPECT_EQ(four[2], cd(-2, 0));
    EXPECT_EQ(four[3], cd(-2, -2));
    // a constant real sequence: n at 0, nothing else
    std::vector<double> ones(12, 1.0);
    std::vector<cd> half(7);
    fft(12).forward_real(ones, half);
    EXPECT_NEAR(half[0].real(), 12, 1e-12);
    for (size_t k = 1; k < 7; ++k) {
        EXPECT_NEAR(std::abs(half[k]), 0, 1e-12) << k;
    }
}

TEST(Fft_Tests, Convolution) {
    std::mt19937_64 rng(4);
    std::uniform_real_distribution<double> d(-1, 1);
    auto direct = [](const std::vector<double>& a, const std::vector<double>& b) {
        std::vector<double> c(a.size() + b.size() - 1);
        for (size_t i = 0; i < a.size(); ++i) {
            for (size_t j = 0; j < b.size(); ++j) {
                c[i + j] += a[i] * b[j];
            }
        }
        return c;
    };
    for (auto [p, q] : {std::pair<size_t, size_t>{1, 1}, {3, 2}, {48, 48}, {49, 49}, {100, 3}, {3, 100}, {64, 200},
                        {1000, 777}, {4096, 4096}, {5000, 60}}) {
        std::vector<double> a(p);
        std::vector<double> b(q);
        for (auto& v : a) {
            v = d(rng);
        }
        for (auto& v : b) {
            v = d(rng);
        }
        sgcl::vector<double> c = math::convolve(a, b);
        auto want = direct(a, b);
        ASSERT_EQ(c.size(), want.size());
        double worst = 0;
        for (size_t i = 0; i < want.size(); ++i) {
            worst = std::max(worst, std::fabs(c[i] - want[i]));
        }
        EXPECT_LT(worst, 1e-11 * std::sqrt(double(std::min(p, q)))) << p << " " << q;
        std::vector<float> af(a.begin(), a.end());
        std::vector<float> bf(b.begin(), b.end());
        sgcl::vector<float> cf32 = math::convolve(af, bf);
        ASSERT_EQ(cf32.size(), want.size());
        worst = 0;
        for (size_t i = 0; i < want.size(); ++i) {
            worst = std::max(worst, std::fabs(double(cf32[i]) - want[i]));
        }
        EXPECT_LT(worst, 2e-5 * std::sqrt(double(std::min(p, q)))) << p << " " << q;
    }
    std::vector<double> none;
    std::vector<double> some = {1, 2};
    EXPECT_TRUE(math::convolve(none, some).empty());
    EXPECT_TRUE(math::convolve(some, none).empty());
    std::vector<double> x = {1, 2, 3};
    std::vector<double> y = {1, 1};
    sgcl::vector<double> c = math::convolve(x, y);
    EXPECT_EQ(c, (sgcl::vector<double>{1, 3, 5, 3}));
}

// The NEON stages against the plain loop, stage by stage of real plans
// (radices 4 and 2 at every stride the vectors take): within an FMA's rounding
TEST(Fft_Tests, VectorStagesAgainstPlain) {
    std::mt19937_64 rng(5);
    for (size_t n : {size_t(8), size_t(16), size_t(32), size_t(64), size_t(512), size_t(1024), size_t(1) << 15}) {
        math::detail::FftEngine e;
        math::detail::fft_build(e, n);
        for (bool inverse : {false, true}) {
            auto x = random_complex<double>(rng, n);
            std::vector<cf> xf(x.begin(), x.end());
            for (const auto& st : e.stages) {
                std::vector<cd> plain(n);
                std::vector<cd> fast(n);
                std::vector<cf> plain_f(n);
                std::vector<cf> fast_f(n);
                const double* wr = e.twiddles_d.re.data() + st.offset;
                const double* wi = e.twiddles_d.im.data() + st.offset;
                const float* wrf = e.twiddles_f.re.data() + st.offset;
                const float* wif = e.twiddles_f.im.data() + st.offset;
                auto d = [](auto& v) { return reinterpret_cast<double*>(v.data()); };
                auto f = [](auto& v) { return reinterpret_cast<float*>(v.data()); };
                auto cdp = [](const auto& v) { return reinterpret_cast<const double*>(v.data()); };
                auto cfp = [](const auto& v) { return reinterpret_cast<const float*>(v.data()); };
                if (inverse) {
                    math::detail::fft_stage_plain<double, true>(cdp(x), d(plain), st, wr, wi);
                    math::detail::fft_stage<double, true>(cdp(x), d(fast), st, wr, wi);
                    math::detail::fft_stage_plain<float, true>(cfp(xf), f(plain_f), st, wrf, wif);
                    math::detail::fft_stage<float, true>(cfp(xf), f(fast_f), st, wrf, wif);
                } else {
                    math::detail::fft_stage_plain<double, false>(cdp(x), d(plain), st, wr, wi);
                    math::detail::fft_stage<double, false>(cdp(x), d(fast), st, wr, wi);
                    math::detail::fft_stage_plain<float, false>(cfp(xf), f(plain_f), st, wrf, wif);
                    math::detail::fft_stage<float, false>(cfp(xf), f(fast_f), st, wrf, wif);
                }
                double worst = 0;
                double worst_f = 0;
                double scale = 1;
                for (size_t i = 0; i < n; ++i) {
                    worst = std::max(worst, std::abs(plain[i] - fast[i]));
                    worst_f = std::max(worst_f, double(std::abs(plain_f[i] - fast_f[i])));
                    scale = std::max(scale, std::abs(plain[i]));
                }
                ASSERT_LT(worst, 1e-15 * scale) << n << " stride " << st.stride;
                ASSERT_LT(worst_f, 1e-6 * scale) << n << " stride " << st.stride;
                x = plain;
                xf = plain_f;
            }
        }
    }
}

TEST(Fft_Tests, Boundaries) {
    // a plan of length 0 and of 1
    fft empty(0);
    EXPECT_EQ(empty.size(), 0u);
    std::vector<cd> none;
    empty.forward(none);
    empty.inverse(none);
    std::vector<double> nothing;
    std::vector<cd> no_coefficients;
    empty.forward_real(nothing, no_coefficients);
    empty.inverse_real(no_coefficients, nothing);
    fft one(1);
    std::vector<cd> single = {cd(3, -4)};
    one.forward(single);
    EXPECT_EQ(single[0], cd(3, -4));
    one.inverse(single);
    EXPECT_EQ(single[0], cd(3, -4));
    // lengths that do not match are errors of the program
    fft plan(8);
    std::vector<cd> seven(7);
    EXPECT_THROW(plan.forward(seven), std::invalid_argument);
    EXPECT_THROW(plan.inverse(seven), std::invalid_argument);
    std::vector<double> eight(8);
    std::vector<cd> four(4);
    EXPECT_THROW(plan.forward_real(eight, four), std::invalid_argument);
    std::vector<cd> five(5);
    std::vector<double> nine(9);
    EXPECT_THROW(plan.inverse_real(five, nine), std::invalid_argument);
    EXPECT_THROW(fft((size_t(1) << 30) + 1), std::length_error);
    // the imaginary parts of the real ends are taken as zero
    std::vector<cd> spectrum = {cd(8, 5), cd(0, 0), cd(0, 0), cd(0, 0), cd(0, 7)};
    plan.inverse_real(spectrum, eight);
    for (double v : eight) {
        EXPECT_NEAR(v, 1, 1e-15);
    }
    fft odd(9);
    std::vector<cd> spectrum9 = {cd(9, 5), cd(0, 0), cd(0, 0), cd(0, 0), cd(0, 0)};
    std::vector<double> out9(9);
    odd.inverse_real(spectrum9, out9);
    for (double v : out9) {
        EXPECT_NEAR(v, 1, 1e-14);
    }
    // a copy shares the plan; a sgcl::vector and a part of one are slices
    fft copy = plan;
    EXPECT_EQ(copy.size(), 8u);
    sgcl::vector<cd> v(16, cd(1, 0));
    copy.forward(slice<cd>(v).subslice(0, 8));
    EXPECT_EQ(v[0], cd(8, 0));
    EXPECT_EQ(v[8], cd(1, 0));
    // NaN goes through
    std::vector<cd> bad(8, cd(0, 0));
    bad[3] = cd(std::nan(""), 0);
    plan.forward(bad);
    EXPECT_TRUE(std::isnan(bad[0].real()));
}

// One plan used by several threads at once: the plan is immutable
TEST(Fft_Tests, PlanSharedByThreads) {
    fft plan(1000);
    std::vector<cd> x(1000);
    for (size_t i = 0; i < 1000; ++i) {
        x[i] = cd(std::sin(double(i)), std::cos(double(i) * 0.3));
    }
    auto want = x;
    plan.forward(want);
    std::atomic<int> wrong{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&plan, &x, &want, &wrong] {
            for (int r = 0; r < 50; ++r) {
                auto y = x;
                plan.forward(y);
                for (size_t i = 0; i < y.size(); ++i) {
                    if (y[i] != want[i]) {
                        wrong.fetch_add(1);
                        break;
                    }
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(wrong.load(), 0);
}
