//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/detail/os.h"
#include "../core/slice.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(__aarch64__) && defined(__ARM_NEON) && !defined(SGCL_MATH_PORTABLE)
#include <arm_neon.h>
#define SGCL_FFT_NEON 1
#endif

// The discrete Fourier transform of any length, by a plan: math::fft(n)
// computes once what every transform of length n needs and then transforms
// any number of sequences of that length, complex or real, in double or
// float, in place. A one-word handle to an immutable managed object: a copy
// shares the plan, and any number of threads use one at once.
//
// A length whose prime factors are 2, 3 and 5 goes by Stockham's form of
// the Cooley–Tukey algorithm, decimation in frequency, radix 4 while it
// can and 2, 3 and 5 after: a stage reads the whole sequence and writes it
// to a second buffer in the order the next stage reads, so there is no
// bit-reversal permutation, and the inner loop of every stage but the first
// runs over contiguous elements with one twiddle factor for all of them —
// NEON on arm64, four butterflies of float or two of double at a time. Any
// other length goes by Bluestein's algorithm: the transform as a
// convolution with a chirp, carried out by transforms of the next power of
// two at least 2n - 1, so a prime length is O(n log n) as well. A real
// sequence of an even length is transformed as a complex one of half its
// length and split.
//
// The forward transform is X_k = Σ x_j·e^(-2πi·jk/n), unscaled; the
// inverse has e^(+2πi·jk/n) and the factor 1/n, so inverse(forward(x)) is x
// — numpy's convention (FFTW and vDSP scale neither way).
namespace sgcl::math {
    namespace detail {
        constexpr double FftPi = 3.14159265358979323846;

        // The product of two complex numbers by the formula: std::complex's
        // operator* goes through the C library's __mulsc3, which mends the
        // infinities and NaN of Annex G at ten times the cost
        template<class T>
        SGCL_INLINE_HOT std::complex<T> cmul(const std::complex<T>& a, const std::complex<T>& b) noexcept {
            return {a.real() * b.real() - a.imag() * b.imag(), a.real() * b.imag() + a.imag() * b.real()};
        }

        // One stage of a Stockham transform: the sequence of the current
        // length L in `stride` interleaved copies, split by `radix` into
        // m = L/radix; its twiddle factors w_L^(p·j), j = 1 … radix - 1,
        // p < m, at `offset` of the plan's tables, j-major
        struct FftStage {
            uint32_t radix;
            size_t m;
            size_t stride;
            size_t offset;
        };

        template<class T>
        struct FftTables {
            std::vector<T> re;
            std::vector<T> im;
        };

        // The transform of one length: its stages and their twiddles, or
        // Bluestein's chirp over the transform of a length that has stages
        struct FftEngine {
            size_t n = 0;
            std::vector<FftStage> stages;
            FftTables<double> twiddles_d;
            FftTables<float> twiddles_f;
            std::unique_ptr<FftEngine> inner;   // Bluestein's, of length m >= 2n - 1
            std::vector<std::complex<double>> chirp_d;
            std::vector<std::complex<double>> spectrum_d;
            std::vector<std::complex<float>> chirp_f;
            std::vector<std::complex<float>> spectrum_f;
        };

        struct FftPlan {
            FftEngine full;                     // the complex transforms of n
            std::unique_ptr<FftEngine> half;    // n/2, for the real ones of an even n
            FftTables<double> split_d;          // e^(-2πi·k/n), k < n/2
            FftTables<float> split_f;
        };

        // n as radices 4, 2, 3 and 5 (in that order of stages), or false
        // when another prime divides it
        inline bool fft_factor(size_t n, std::vector<uint32_t>& radices) {
            radices.clear();
            while (n % 4 == 0) {
                radices.push_back(4);
                n /= 4;
            }
            if (n % 2 == 0) {
                radices.push_back(2);
                n /= 2;
            }
            for (uint32_t r : {3u, 5u}) {
                while (n % r == 0) {
                    radices.push_back(r);
                    n /= r;
                }
            }
            return n == 1;
        }

        inline void fft_build(FftEngine& e, size_t n);

        // The stages of a length of radices 2, 3, 5 and their twiddles, each
        // from its own angle in double (no recurrence, whose error grows)
        inline void fft_build_stages(FftEngine& e, const std::vector<uint32_t>& radices) {
            size_t length = e.n;
            size_t stride = 1;
            size_t offset = 0;
            for (uint32_t r : radices) {
                size_t m = length / r;
                e.stages.push_back({r, m, stride, offset});
                offset += (r - 1) * m;
                length = m;
                stride *= r;
            }
            e.twiddles_d.re.resize(offset);
            e.twiddles_d.im.resize(offset);
            length = e.n;
            for (const FftStage& s : e.stages) {
                for (size_t j = 1; j < s.radix; ++j) {
                    for (size_t p = 0; p < s.m; ++p) {
                        // w_L^(p·j), the exponent reduced modulo L in integers
                        uint64_t e_pj = uint64_t(p) * j % length;
                        double a = -2 * FftPi * double(e_pj) / double(length);
                        e.twiddles_d.re[s.offset + (j - 1) * s.m + p] = std::cos(a);
                        e.twiddles_d.im[s.offset + (j - 1) * s.m + p] = std::sin(a);
                    }
                }
                length = s.m;
            }
            e.twiddles_f.re.assign(e.twiddles_d.re.begin(), e.twiddles_d.re.end());
            e.twiddles_f.im.assign(e.twiddles_d.im.begin(), e.twiddles_d.im.end());
        }

        template<class T>
        const FftTables<T>& twiddles_of(const FftEngine& e) noexcept {
            if constexpr (std::is_same_v<T, double>) {
                return e.twiddles_d;
            } else {
                return e.twiddles_f;
            }
        }

        template<class T>
        const std::vector<std::complex<T>>& chirp_of(const FftEngine& e) noexcept {
            if constexpr (std::is_same_v<T, double>) {
                return e.chirp_d;
            } else {
                return e.chirp_f;
            }
        }

        template<class T>
        const std::vector<std::complex<T>>& spectrum_of(const FftEngine& e) noexcept {
            if constexpr (std::is_same_v<T, double>) {
                return e.spectrum_d;
            } else {
                return e.spectrum_f;
            }
        }

        // a·w and a·conj(w) on the numbers apart
        template<class T, bool Inverse>
        SGCL_INLINE_HOT void twiddle(T& re, T& im, T wr, T wi) noexcept {
            if constexpr (Inverse) {
                wi = -wi;
            }
            T r = re * wr - im * wi;
            im = re * wi + im * wr;
            re = r;
        }

        // The radix-r butterflies of one stage, plain arithmetic: for each
        // p and each of the stride copies, the r inputs a_k = x[q + s(p +
        // k·m)], their DFT of length r, the outputs times w_L^(p·j) to
        // y[q + s(r·p + j)]
        template<class T, bool Inverse>
        void fft_stage_plain(const T* x, T* y, const FftStage& st, const T* wr, const T* wi) noexcept {
            const size_t m = st.m;
            const size_t s = st.stride;
            const uint32_t r = st.radix;
            constexpr T Sign = Inverse ? T(1) : T(-1);   // the sign of i in ω_r = e^(∓2πi/r)
            for (size_t p = 0; p < m; ++p) {
                for (size_t q = 0; q < s; ++q) {
                    const T* in = x + 2 * (q + s * p);
                    T* out = y + 2 * (q + s * r * p);
                    const size_t step = 2 * s * m;     // from a_k to a_(k+1)
                    const size_t ostep = 2 * s;        // from y_j to y_(j+1)
                    if (r == 4) {
                        T a0r = in[0], a0i = in[1];
                        T a1r = in[step], a1i = in[step + 1];
                        T a2r = in[2 * step], a2i = in[2 * step + 1];
                        T a3r = in[3 * step], a3i = in[3 * step + 1];
                        T t0r = a0r + a2r, t0i = a0i + a2i;
                        T t1r = a0r - a2r, t1i = a0i - a2i;
                        T t2r = a1r + a3r, t2i = a1i + a3i;
                        // (a1 - a3)·(∓i)
                        T t3r = -Sign * (a1i - a3i), t3i = Sign * (a1r - a3r);
                        T y0r = t0r + t2r, y0i = t0i + t2i;
                        T y1r = t1r + t3r, y1i = t1i + t3i;
                        T y2r = t0r - t2r, y2i = t0i - t2i;
                        T y3r = t1r - t3r, y3i = t1i - t3i;
                        twiddle<T, Inverse>(y1r, y1i, wr[p], wi[p]);
                        twiddle<T, Inverse>(y2r, y2i, wr[m + p], wi[m + p]);
                        twiddle<T, Inverse>(y3r, y3i, wr[2 * m + p], wi[2 * m + p]);
                        out[0] = y0r, out[1] = y0i;
                        out[ostep] = y1r, out[ostep + 1] = y1i;
                        out[2 * ostep] = y2r, out[2 * ostep + 1] = y2i;
                        out[3 * ostep] = y3r, out[3 * ostep + 1] = y3i;
                    } else if (r == 2) {
                        T a0r = in[0], a0i = in[1];
                        T a1r = in[step], a1i = in[step + 1];
                        T y1r = a0r - a1r, y1i = a0i - a1i;
                        twiddle<T, Inverse>(y1r, y1i, wr[p], wi[p]);
                        out[0] = a0r + a1r, out[1] = a0i + a1i;
                        out[ostep] = y1r, out[ostep + 1] = y1i;
                    } else if (r == 3) {
                        constexpr T H = T(0.86602540378443864676);   // √3/2
                        T a0r = in[0], a0i = in[1];
                        T a1r = in[step], a1i = in[step + 1];
                        T a2r = in[2 * step], a2i = in[2 * step + 1];
                        T tr = a1r + a2r, ti = a1i + a2i;
                        T dr = a1r - a2r, di = a1i - a2i;
                        T br = a0r - tr / 2, bi = a0i - ti / 2;
                        // ± i·√3/2·d
                        T er = -Sign * H * di, ei = Sign * H * dr;
                        T y1r = br + er, y1i = bi + ei;
                        T y2r = br - er, y2i = bi - ei;
                        twiddle<T, Inverse>(y1r, y1i, wr[p], wi[p]);
                        twiddle<T, Inverse>(y2r, y2i, wr[m + p], wi[m + p]);
                        out[0] = a0r + tr, out[1] = a0i + ti;
                        out[ostep] = y1r, out[ostep + 1] = y1i;
                        out[2 * ostep] = y2r, out[2 * ostep + 1] = y2i;
                    } else {
                        constexpr T C1 = T(0.30901699437494742410);    // cos(2π/5)
                        constexpr T C2 = T(-0.80901699437494742410);   // cos(4π/5)
                        constexpr T S1 = T(0.95105651629515357212);    // sin(2π/5)
                        constexpr T S2 = T(0.58778525229247312917);    // sin(4π/5)
                        T a0r = in[0], a0i = in[1];
                        T a1r = in[step], a1i = in[step + 1];
                        T a2r = in[2 * step], a2i = in[2 * step + 1];
                        T a3r = in[3 * step], a3i = in[3 * step + 1];
                        T a4r = in[4 * step], a4i = in[4 * step + 1];
                        T t1r = a1r + a4r, t1i = a1i + a4i;
                        T t2r = a2r + a3r, t2i = a2i + a3i;
                        T d1r = a1r - a4r, d1i = a1i - a4i;
                        T d2r = a2r - a3r, d2i = a2i - a3i;
                        T b1r = a0r + C1 * t1r + C2 * t2r, b1i = a0i + C1 * t1i + C2 * t2i;
                        T b2r = a0r + C2 * t1r + C1 * t2r, b2i = a0i + C2 * t1i + C1 * t2i;
                        // ∓i·(s·d): the imaginary unit times the sums of sines
                        T e1r = S1 * d1r + S2 * d2r, e1i = S1 * d1i + S2 * d2i;
                        T e2r = S2 * d1r - S1 * d2r, e2i = S2 * d1i - S1 * d2i;
                        T f1r = -Sign * e1i, f1i = Sign * e1r;
                        T f2r = -Sign * e2i, f2i = Sign * e2r;
                        T y1r = b1r + f1r, y1i = b1i + f1i;
                        T y4r = b1r - f1r, y4i = b1i - f1i;
                        T y2r = b2r + f2r, y2i = b2i + f2i;
                        T y3r = b2r - f2r, y3i = b2i - f2i;
                        twiddle<T, Inverse>(y1r, y1i, wr[p], wi[p]);
                        twiddle<T, Inverse>(y2r, y2i, wr[m + p], wi[m + p]);
                        twiddle<T, Inverse>(y3r, y3i, wr[2 * m + p], wi[2 * m + p]);
                        twiddle<T, Inverse>(y4r, y4i, wr[3 * m + p], wi[3 * m + p]);
                        out[0] = a0r + t1r + t2r, out[1] = a0i + t1i + t2i;
                        out[ostep] = y1r, out[ostep + 1] = y1i;
                        out[2 * ostep] = y2r, out[2 * ostep + 1] = y2i;
                        out[3 * ostep] = y3r, out[3 * ostep + 1] = y3i;
                        out[4 * ostep] = y4r, out[4 * ostep + 1] = y4i;
                    }
                }
            }
        }

#if defined(SGCL_FFT_NEON)
        // NEON over the stride's copies, which lie one after another and
        // share the twiddle factors of their p: four complex floats (or two
        // doubles) at a time, vld2/vst2 taking them apart into their real
        // and imaginary parts. The radices 4 and 2, the stages of a power of
        // two; 3 and 5 go by the plain loop.
        template<class T>
        struct Neon;

        template<>
        struct Neon<float> {
            using V = float32x4_t;
            using V2 = float32x4x2_t;
            static constexpr size_t Lanes = 4;
            static V2 load(const float* p) noexcept { return vld2q_f32(p); }
            static void store(float* p, V a, V b) noexcept {
                V2 v = {{a, b}};
                vst2q_f32(p, v);
            }
            static V add(V a, V b) noexcept { return vaddq_f32(a, b); }
            static V sub(V a, V b) noexcept { return vsubq_f32(a, b); }
            static V neg(V a) noexcept { return vnegq_f32(a); }
            static V mul(V a, float b) noexcept { return vmulq_n_f32(a, b); }
            static V fma(V acc, V a, float b) noexcept { return vfmaq_n_f32(acc, a, b); }
            static V fms(V acc, V a, float b) noexcept { return vfmsq_n_f32(acc, a, b); }
            static V mulv(V a, V b) noexcept { return vmulq_f32(a, b); }
            static V fmav(V acc, V a, V b) noexcept { return vfmaq_f32(acc, a, b); }
            static V fmsv(V acc, V a, V b) noexcept { return vfmsq_f32(acc, a, b); }
        };

        template<>
        struct Neon<double> {
            using V = float64x2_t;
            using V2 = float64x2x2_t;
            static constexpr size_t Lanes = 2;
            static V2 load(const double* p) noexcept { return vld2q_f64(p); }
            static void store(double* p, V a, V b) noexcept {
                V2 v = {{a, b}};
                vst2q_f64(p, v);
            }
            static V add(V a, V b) noexcept { return vaddq_f64(a, b); }
            static V sub(V a, V b) noexcept { return vsubq_f64(a, b); }
            static V neg(V a) noexcept { return vnegq_f64(a); }
            static V mul(V a, double b) noexcept { return vmulq_n_f64(a, b); }
            static V fma(V acc, V a, double b) noexcept { return vfmaq_n_f64(acc, a, b); }
            static V fms(V acc, V a, double b) noexcept { return vfmsq_n_f64(acc, a, b); }
            static V mulv(V a, V b) noexcept { return vmulq_f64(a, b); }
            static V fmav(V acc, V a, V b) noexcept { return vfmaq_f64(acc, a, b); }
            static V fmsv(V acc, V a, V b) noexcept { return vfmsq_f64(acc, a, b); }
        };

        template<class T, bool Inverse>
        void fft_stage_simd(const T* x, T* y, const FftStage& st, const T* wr, const T* wi) noexcept {
            using N = Neon<T>;
            using V = typename N::V;
            const size_t m = st.m;
            const size_t s = st.stride;
            const size_t step = 2 * s * m;
            const size_t ostep = 2 * s;
            // (re, im)·(c, d) for a scalar twiddle c + i·d (conjugated for
            // the inverse)
            auto rotate = [](V& re, V& im, T c, T d) {
                if constexpr (Inverse) {
                    d = -d;
                }
                V r = N::fms(N::mul(re, c), im, d);
                im = N::fma(N::mul(im, c), re, d);
                re = r;
            };
            if (st.radix == 4) {
                for (size_t p = 0; p < m; ++p) {
                    T w1r = wr[p], w1i = wi[p];
                    T w2r = wr[m + p], w2i = wi[m + p];
                    T w3r = wr[2 * m + p], w3i = wi[2 * m + p];
                    const T* in = x + 2 * s * p;
                    T* out = y + 2 * s * 4 * p;
                    for (size_t q = 0; q < s; q += N::Lanes) {
                        auto a0 = N::load(in + 2 * q);
                        auto a1 = N::load(in + step + 2 * q);
                        auto a2 = N::load(in + 2 * step + 2 * q);
                        auto a3 = N::load(in + 3 * step + 2 * q);
                        V t0r = N::add(a0.val[0], a2.val[0]), t0i = N::add(a0.val[1], a2.val[1]);
                        V t1r = N::sub(a0.val[0], a2.val[0]), t1i = N::sub(a0.val[1], a2.val[1]);
                        V t2r = N::add(a1.val[0], a3.val[0]), t2i = N::add(a1.val[1], a3.val[1]);
                        V dr = N::sub(a1.val[0], a3.val[0]), di = N::sub(a1.val[1], a3.val[1]);
                        // (a1 - a3)·(-i) forward, ·(+i) inverse
                        V t3r = Inverse ? N::neg(di) : di;
                        V t3i = Inverse ? dr : N::neg(dr);
                        V y1r = N::add(t1r, t3r), y1i = N::add(t1i, t3i);
                        V y2r = N::sub(t0r, t2r), y2i = N::sub(t0i, t2i);
                        V y3r = N::sub(t1r, t3r), y3i = N::sub(t1i, t3i);
                        rotate(y1r, y1i, w1r, w1i);
                        rotate(y2r, y2i, w2r, w2i);
                        rotate(y3r, y3i, w3r, w3i);
                        N::store(out + 2 * q, N::add(t0r, t2r), N::add(t0i, t2i));
                        N::store(out + ostep + 2 * q, y1r, y1i);
                        N::store(out + 2 * ostep + 2 * q, y2r, y2i);
                        N::store(out + 3 * ostep + 2 * q, y3r, y3i);
                    }
                }
            } else if (st.radix == 2) {
                for (size_t p = 0; p < m; ++p) {
                    T w1r = wr[p], w1i = wi[p];
                    const T* in = x + 2 * s * p;
                    T* out = y + 2 * s * 2 * p;
                    for (size_t q = 0; q < s; q += N::Lanes) {
                        auto a0 = N::load(in + 2 * q);
                        auto a1 = N::load(in + step + 2 * q);
                        V y1r = N::sub(a0.val[0], a1.val[0]), y1i = N::sub(a0.val[1], a1.val[1]);
                        rotate(y1r, y1i, w1r, w1i);
                        N::store(out + 2 * q, N::add(a0.val[0], a1.val[0]), N::add(a0.val[1], a1.val[1]));
                        N::store(out + ostep + 2 * q, y1r, y1i);
                    }
                }
            } else if (st.radix == 3) {
                constexpr T H = T(0.86602540378443864676);
                constexpr T Sign = Inverse ? T(1) : T(-1);
                for (size_t p = 0; p < m; ++p) {
                    T w1r = wr[p], w1i = wi[p];
                    T w2r = wr[m + p], w2i = wi[m + p];
                    const T* in = x + 2 * s * p;
                    T* out = y + 2 * s * 3 * p;
                    for (size_t q = 0; q < s; q += N::Lanes) {
                        auto a0 = N::load(in + 2 * q);
                        auto a1 = N::load(in + step + 2 * q);
                        auto a2 = N::load(in + 2 * step + 2 * q);
                        V tr = N::add(a1.val[0], a2.val[0]), ti = N::add(a1.val[1], a2.val[1]);
                        V dr = N::sub(a1.val[0], a2.val[0]), di = N::sub(a1.val[1], a2.val[1]);
                        V br = N::fms(a0.val[0], tr, T(0.5)), bi = N::fms(a0.val[1], ti, T(0.5));
                        V er = N::mul(di, -Sign * H), ei = N::mul(dr, Sign * H);
                        V y1r = N::add(br, er), y1i = N::add(bi, ei);
                        V y2r = N::sub(br, er), y2i = N::sub(bi, ei);
                        rotate(y1r, y1i, w1r, w1i);
                        rotate(y2r, y2i, w2r, w2i);
                        N::store(out + 2 * q, N::add(a0.val[0], tr), N::add(a0.val[1], ti));
                        N::store(out + ostep + 2 * q, y1r, y1i);
                        N::store(out + 2 * ostep + 2 * q, y2r, y2i);
                    }
                }
            } else {
                constexpr T C1 = T(0.30901699437494742410);
                constexpr T C2 = T(-0.80901699437494742410);
                constexpr T S1 = T(0.95105651629515357212);
                constexpr T S2 = T(0.58778525229247312917);
                constexpr T Sign = Inverse ? T(1) : T(-1);
                for (size_t p = 0; p < m; ++p) {
                    T w1r = wr[p], w1i = wi[p];
                    T w2r = wr[m + p], w2i = wi[m + p];
                    T w3r = wr[2 * m + p], w3i = wi[2 * m + p];
                    T w4r = wr[3 * m + p], w4i = wi[3 * m + p];
                    const T* in = x + 2 * s * p;
                    T* out = y + 2 * s * 5 * p;
                    for (size_t q = 0; q < s; q += N::Lanes) {
                        auto a0 = N::load(in + 2 * q);
                        auto a1 = N::load(in + step + 2 * q);
                        auto a2 = N::load(in + 2 * step + 2 * q);
                        auto a3 = N::load(in + 3 * step + 2 * q);
                        auto a4 = N::load(in + 4 * step + 2 * q);
                        V t1r = N::add(a1.val[0], a4.val[0]), t1i = N::add(a1.val[1], a4.val[1]);
                        V t2r = N::add(a2.val[0], a3.val[0]), t2i = N::add(a2.val[1], a3.val[1]);
                        V d1r = N::sub(a1.val[0], a4.val[0]), d1i = N::sub(a1.val[1], a4.val[1]);
                        V d2r = N::sub(a2.val[0], a3.val[0]), d2i = N::sub(a2.val[1], a3.val[1]);
                        V b1r = N::fma(N::fma(a0.val[0], t1r, C1), t2r, C2);
                        V b1i = N::fma(N::fma(a0.val[1], t1i, C1), t2i, C2);
                        V b2r = N::fma(N::fma(a0.val[0], t1r, C2), t2r, C1);
                        V b2i = N::fma(N::fma(a0.val[1], t1i, C2), t2i, C1);
                        V e1r = N::fma(N::mul(d1r, S1), d2r, S2), e1i = N::fma(N::mul(d1i, S1), d2i, S2);
                        V e2r = N::fms(N::mul(d1r, S2), d2r, S1), e2i = N::fms(N::mul(d1i, S2), d2i, S1);
                        V f1r = N::mul(e1i, -Sign), f1i = N::mul(e1r, Sign);
                        V f2r = N::mul(e2i, -Sign), f2i = N::mul(e2r, Sign);
                        V y1r = N::add(b1r, f1r), y1i = N::add(b1i, f1i);
                        V y4r = N::sub(b1r, f1r), y4i = N::sub(b1i, f1i);
                        V y2r = N::add(b2r, f2r), y2i = N::add(b2i, f2i);
                        V y3r = N::sub(b2r, f2r), y3i = N::sub(b2i, f2i);
                        rotate(y1r, y1i, w1r, w1i);
                        rotate(y2r, y2i, w2r, w2i);
                        rotate(y3r, y3i, w3r, w3i);
                        rotate(y4r, y4i, w4r, w4i);
                        N::store(out + 2 * q, N::add(N::add(a0.val[0], t1r), t2r), N::add(N::add(a0.val[1], t1i), t2i));
                        N::store(out + ostep + 2 * q, y1r, y1i);
                        N::store(out + 2 * ostep + 2 * q, y2r, y2i);
                        N::store(out + 3 * ostep + 2 * q, y3r, y3i);
                        N::store(out + 4 * ostep + 2 * q, y4r, y4i);
                    }
                }
            }
        }

        // The first stage of radix 4, of stride 1: the vectors run over p
        // instead, whose twiddle factors differ (loaded from the rows of the
        // table) and whose outputs y[4p + j] interleave — the four results
        // of four p's put in order by zipping the parts into complex numbers
        // and storing them four-way interleaved as 64-bit lanes (float), or
        // one complex double a register (double)
        template<bool Inverse>
        void fft_first_stage_simd(const float* x, float* y, const FftStage& st, const float* wr, const float* wi) noexcept {
            using N = Neon<float>;
            using V = float32x4_t;
            const size_t m = st.m;
            const size_t step = 2 * m;
            auto rotate = [](V& re, V& im, V c, V d) {
                if constexpr (Inverse) {
                    d = vnegq_f32(d);
                }
                V r = N::fmsv(N::mulv(re, c), im, d);
                im = N::fmav(N::mulv(im, c), re, d);
                re = r;
            };
            for (size_t p = 0; p < m; p += 4) {
                auto a0 = N::load(x + 2 * p);
                auto a1 = N::load(x + step + 2 * p);
                auto a2 = N::load(x + 2 * step + 2 * p);
                auto a3 = N::load(x + 3 * step + 2 * p);
                V t0r = N::add(a0.val[0], a2.val[0]), t0i = N::add(a0.val[1], a2.val[1]);
                V t1r = N::sub(a0.val[0], a2.val[0]), t1i = N::sub(a0.val[1], a2.val[1]);
                V t2r = N::add(a1.val[0], a3.val[0]), t2i = N::add(a1.val[1], a3.val[1]);
                V dr = N::sub(a1.val[0], a3.val[0]), di = N::sub(a1.val[1], a3.val[1]);
                V t3r = Inverse ? N::neg(di) : di;
                V t3i = Inverse ? dr : N::neg(dr);
                V y0r = N::add(t0r, t2r), y0i = N::add(t0i, t2i);
                V y1r = N::add(t1r, t3r), y1i = N::add(t1i, t3i);
                V y2r = N::sub(t0r, t2r), y2i = N::sub(t0i, t2i);
                V y3r = N::sub(t1r, t3r), y3i = N::sub(t1i, t3i);
                rotate(y1r, y1i, vld1q_f32(wr + p), vld1q_f32(wi + p));
                rotate(y2r, y2i, vld1q_f32(wr + m + p), vld1q_f32(wi + m + p));
                rotate(y3r, y3i, vld1q_f32(wr + 2 * m + p), vld1q_f32(wi + 2 * m + p));
                // the complex numbers of p, p+1 (low) and p+2, p+3 (high) of each output
                float64x2x4_t low = {{vreinterpretq_f64_f32(vzip1q_f32(y0r, y0i)), vreinterpretq_f64_f32(vzip1q_f32(y1r, y1i)),
                                      vreinterpretq_f64_f32(vzip1q_f32(y2r, y2i)), vreinterpretq_f64_f32(vzip1q_f32(y3r, y3i))}};
                float64x2x4_t high = {{vreinterpretq_f64_f32(vzip2q_f32(y0r, y0i)), vreinterpretq_f64_f32(vzip2q_f32(y1r, y1i)),
                                       vreinterpretq_f64_f32(vzip2q_f32(y2r, y2i)), vreinterpretq_f64_f32(vzip2q_f32(y3r, y3i))}};
                vst4q_f64(reinterpret_cast<double*>(y + 8 * p), low);
                vst4q_f64(reinterpret_cast<double*>(y + 8 * p + 16), high);
            }
        }

        template<bool Inverse>
        void fft_first_stage_simd(const double* x, double* y, const FftStage& st, const double* wr, const double* wi) noexcept {
            using N = Neon<double>;
            using V = float64x2_t;
            const size_t m = st.m;
            const size_t step = 2 * m;
            auto rotate = [](V& re, V& im, V c, V d) {
                if constexpr (Inverse) {
                    d = vnegq_f64(d);
                }
                V r = N::fmsv(N::mulv(re, c), im, d);
                im = N::fmav(N::mulv(im, c), re, d);
                re = r;
            };
            for (size_t p = 0; p < m; p += 2) {
                auto a0 = N::load(x + 2 * p);
                auto a1 = N::load(x + step + 2 * p);
                auto a2 = N::load(x + 2 * step + 2 * p);
                auto a3 = N::load(x + 3 * step + 2 * p);
                V t0r = N::add(a0.val[0], a2.val[0]), t0i = N::add(a0.val[1], a2.val[1]);
                V t1r = N::sub(a0.val[0], a2.val[0]), t1i = N::sub(a0.val[1], a2.val[1]);
                V t2r = N::add(a1.val[0], a3.val[0]), t2i = N::add(a1.val[1], a3.val[1]);
                V dr = N::sub(a1.val[0], a3.val[0]), di = N::sub(a1.val[1], a3.val[1]);
                V t3r = Inverse ? N::neg(di) : di;
                V t3i = Inverse ? dr : N::neg(dr);
                V y0r = N::add(t0r, t2r), y0i = N::add(t0i, t2i);
                V y1r = N::add(t1r, t3r), y1i = N::add(t1i, t3i);
                V y2r = N::sub(t0r, t2r), y2i = N::sub(t0i, t2i);
                V y3r = N::sub(t1r, t3r), y3i = N::sub(t1i, t3i);
                rotate(y1r, y1i, vld1q_f64(wr + p), vld1q_f64(wi + p));
                rotate(y2r, y2i, vld1q_f64(wr + m + p), vld1q_f64(wi + m + p));
                rotate(y3r, y3i, vld1q_f64(wr + 2 * m + p), vld1q_f64(wi + 2 * m + p));
                double* o = y + 8 * p;
                vst1q_f64(o, vzip1q_f64(y0r, y0i));
                vst1q_f64(o + 2, vzip1q_f64(y1r, y1i));
                vst1q_f64(o + 4, vzip1q_f64(y2r, y2i));
                vst1q_f64(o + 6, vzip1q_f64(y3r, y3i));
                vst1q_f64(o + 8, vzip2q_f64(y0r, y0i));
                vst1q_f64(o + 10, vzip2q_f64(y1r, y1i));
                vst1q_f64(o + 12, vzip2q_f64(y2r, y2i));
                vst1q_f64(o + 14, vzip2q_f64(y3r, y3i));
            }
        }
#endif

        // One stage by the road its shape allows
        template<class T, bool Inverse>
        SGCL_INLINE_HOT void fft_stage(const T* x, T* y, const FftStage& st, const T* wr, const T* wi) noexcept {
#if defined(SGCL_FFT_NEON)
            if (st.stride % Neon<T>::Lanes == 0) {
                fft_stage_simd<T, Inverse>(x, y, st, wr, wi);
                return;
            }
            if (st.stride == 1 && st.radix == 4 && st.m % Neon<T>::Lanes == 0) {
                fft_first_stage_simd<Inverse>(x, y, st, wr, wi);
                return;
            }
#endif
            fft_stage_plain<T, Inverse>(x, y, st, wr, wi);
        }

        // Working memory of a call: on the stack while small, outside the
        // managed heap past that
        template<class T>
        class FftScratch {
        public:
            explicit FftScratch(size_t n)
            : _p(n <= Inline ? reinterpret_cast<std::complex<T>*>(_inline)
                             : (_heap = std::make_unique_for_overwrite<std::complex<T>[]>(n)).get()) {
            }

            FftScratch(const FftScratch&) = delete;
            FftScratch& operator=(const FftScratch&) = delete;

            std::complex<T>* get() noexcept {
                return _p;
            }

        private:
            // raw bytes: an array of std::complex would be zeroed at every call
            static constexpr size_t Inline = 8192 / sizeof(std::complex<T>);
            alignas(16) unsigned char _inline[8192];
            std::unique_ptr<std::complex<T>[]> _heap;
            std::complex<T>* _p;
        };

        template<class T, bool Inverse>
        void fft_run(const FftEngine& e, std::complex<T>* data);

        // Bluestein: X_k = w_k · Σ (x_j·w_j)·conj(w_(k-j)), with w_k =
        // e^(-πi·k²/n), the sum a convolution of length m done by two
        // transforms and the plan's transform of the conjugate chirp; the
        // inverse with the conjugate chirp
        template<class T, bool Inverse>
        void fft_bluestein(const FftEngine& e, std::complex<T>* x) {
            size_t n = e.n;
            const FftEngine& inner = *e.inner;
            size_t m = inner.n;
            const auto& w = chirp_of<T>(e);
            const auto& b = spectrum_of<T>(e);
            FftScratch<T> scratch(m);
            std::complex<T>* a = scratch.get();
            for (size_t k = 0; k < n; ++k) {
                a[k] = cmul(x[k], Inverse ? conj(w[k]) : w[k]);
            }
            std::fill(a + n, a + m, std::complex<T>(0));
            fft_run<T, false>(inner, a);
            if constexpr (Inverse) {
                // the inverse's kernel is the forward one's conjugate: its
                // spectrum is the forward spectrum's, reversed and conjugated
                a[0] = cmul(a[0], conj(b[0]));
                for (size_t k = 1; k < m; ++k) {
                    a[k] = cmul(a[k], conj(b[m - k]));
                }
            } else {
                for (size_t k = 0; k < m; ++k) {
                    a[k] = cmul(a[k], b[k]);
                }
            }
            fft_run<T, true>(inner, a);   // the inner inverse is unscaled here: 1/m below
            T scale = T(1) / T(m);
            for (size_t k = 0; k < n; ++k) {
                x[k] = cmul(a[k], Inverse ? conj(w[k]) : w[k]) * scale;
            }
        }

        // The transform of e.n numbers in place, unscaled both ways
        template<class T, bool Inverse>
        void fft_run(const FftEngine& e, std::complex<T>* data) {
            if (e.n < 2) {
                return;
            }
            if (e.inner) {
                fft_bluestein<T, Inverse>(e, data);
                return;
            }
            const auto& t = twiddles_of<T>(e);
            FftScratch<T> scratch(e.n);
            T* x = reinterpret_cast<T*>(data);
            T* y = reinterpret_cast<T*>(scratch.get());
            for (const FftStage& st : e.stages) {
                fft_stage<T, Inverse>(x, y, st, t.re.data() + st.offset, t.im.data() + st.offset);
                std::swap(x, y);
            }
            if (x != reinterpret_cast<T*>(data)) {
                sgcl::detail::copy_bytes(data, x, e.n * sizeof(std::complex<T>));
            }
        }

        inline void fft_build(FftEngine& e, size_t n) {
            e.n = n;
            std::vector<uint32_t> radices;
            if (n < 2 || fft_factor(n, radices)) {
                fft_build_stages(e, radices);
                return;
            }
            // Bluestein over the next power of two at least 2n - 1
            size_t m = std::bit_ceil(2 * n - 1);
            e.inner = std::make_unique<FftEngine>();
            fft_build(*e.inner, m);
            e.chirp_d.resize(n);
            for (size_t k = 0; k < n; ++k) {
                // k² modulo 2n, so the angle stays small and exact
                uint64_t q = uint64_t(k) * k % (2 * uint64_t(n));
                double a = -FftPi * double(q) / double(n);
                e.chirp_d[k] = {std::cos(a), std::sin(a)};
            }
            std::vector<std::complex<double>> b(m);
            b[0] = conj(e.chirp_d[0]);
            for (size_t k = 1; k < n; ++k) {
                b[k] = b[m - k] = conj(e.chirp_d[k]);
            }
            fft_run<double, false>(*e.inner, b.data());
            e.spectrum_d = std::move(b);
            e.chirp_f.assign(e.chirp_d.begin(), e.chirp_d.end());
            e.spectrum_f.assign(e.spectrum_d.begin(), e.spectrum_d.end());
        }

        template<class T>
        void fft_forward(const FftEngine& e, std::complex<T>* x) {
            fft_run<T, false>(e, x);
        }

        template<class T>
        void fft_inverse(const FftEngine& e, std::complex<T>* x) {
            fft_run<T, true>(e, x);
            size_t n = e.n;
            T scale = T(1) / T(n);
            T* p = reinterpret_cast<T*>(x);
            for (size_t i = 0; i < 2 * n; ++i) {
                p[i] *= scale;
            }
        }

        template<class T>
        const FftTables<T>& split_of(const FftPlan& plan) noexcept {
            if constexpr (std::is_same_v<T, double>) {
                return plan.split_d;
            } else {
                return plan.split_f;
            }
        }

        // A real sequence of an even length n as the complex one of its
        // pairs, z_j = x_(2j) + i·x_(2j+1), transformed at n/2 and split:
        // X_k = E_k + e^(-2πi·k/n)·O_k with E and O the transforms of the
        // even and the odd numbers, E_k = (Z_k + conj Z_(n/2-k))/2 and
        // O_k = (Z_k - conj Z_(n/2-k))/(2i)
        template<class T>
        void fft_forward_real(const FftPlan& plan, const T* in, std::complex<T>* out) {
            size_t n = plan.full.n;
            size_t half = n / 2;
            sgcl::detail::copy_bytes(out, in, n * sizeof(T));
            fft_run<T, false>(*plan.half, out);
            std::complex<T> z0 = out[0];
            out[0] = {z0.real() + z0.imag(), 0};
            out[half] = {z0.real() - z0.imag(), 0};
            const auto& w = split_of<T>(plan);
            for (size_t k = 1; k <= half / 2; ++k) {
                size_t j = half - k;
                std::complex<T> zk = out[k];
                std::complex<T> zj = conj(out[j]);
                std::complex<T> e = (zk + zj) * T(0.5);
                std::complex<T> d = zk - zj;
                std::complex<T> o(d.imag() * T(0.5), -d.real() * T(0.5));   // d/(2i)
                std::complex<T> wk(w.re[k], w.im[k]);
                std::complex<T> wj(w.re[j], w.im[j]);
                // X_j from the conjugate pair: E_j = conj(E_k), O_j = conj(O_k)
                out[k] = e + cmul(wk, o);
                out[j] = conj(e) + cmul(wj, conj(o));
            }
        }

        // The inverse of the above: E_k = (X_k + conj X_(n/2-k))/2,
        // O_k = (X_k - conj X_(n/2-k))·e^(+2πi·k/n)/2, Z_k = E_k + i·O_k,
        // and the inverse transform of Z at n/2 is the pairs of the numbers
        template<class T>
        void fft_inverse_real(const FftPlan& plan, const std::complex<T>* in, T* out) {
            size_t n = plan.full.n;
            size_t half = n / 2;
            auto* z = reinterpret_cast<std::complex<T>*>(out);
            const auto& w = split_of<T>(plan);
            T x0 = in[0].real();
            T xh = in[half].real();
            z[0] = {(x0 + xh) / 2, (x0 - xh) / 2};
            for (size_t k = 1; k < half; ++k) {
                std::complex<T> xk = in[k];
                std::complex<T> xj = conj(in[half - k]);
                std::complex<T> e = (xk + xj) * T(0.5);
                std::complex<T> o = cmul(xk - xj, std::complex<T>(w.re[k], -w.im[k])) * T(0.5);
                z[k] = e + std::complex<T>(-o.imag(), o.real());
            }
            fft_inverse(*plan.half, z);
        }
    }

    class fft {
    public:
        // A plan for transforms of length n; a length past 2^30 is
        // length_error. A plan of length 0 transforms the empty sequence
        explicit fft(size_t n);

        // The length of the transforms
        SGCL_INLINE_HOT size_t size() const noexcept {
            return _plan->full.n;
        }

        // The forward transform in place, unscaled; a slice of another
        // length is invalid_argument
        void forward(const slice<std::complex<double>>& data) const {
            _check(data.size(), "forward");
            detail::fft_forward(_plan->full, data.data());
        }

        void forward(const slice<std::complex<float>>& data) const {
            _check(data.size(), "forward");
            detail::fft_forward(_plan->full, data.data());
        }

        // The inverse transform in place, scaled by 1/n
        void inverse(const slice<std::complex<double>>& data) const {
            _check(data.size(), "inverse");
            if (data.size()) {
                detail::fft_inverse(_plan->full, data.data());
            }
        }

        void inverse(const slice<std::complex<float>>& data) const {
            _check(data.size(), "inverse");
            if (data.size()) {
                detail::fft_inverse(_plan->full, data.data());
            }
        }

        // The n numbers transformed into the n/2 + 1 coefficients of the
        // frequencies 0 … n/2 (the others are their conjugates, X_(n-k) =
        // conj X_k); out must hold n/2 + 1
        void forward_real(const slice<const double>& in, const slice<std::complex<double>>& out) const {
            _forward_real(in, out);
        }

        void forward_real(const slice<const float>& in, const slice<std::complex<float>>& out) const {
            _forward_real(in, out);
        }

        // The n numbers back from their n/2 + 1 coefficients, scaled by
        // 1/n; the imaginary parts of in[0], and of in[n/2] for an even n,
        // are taken as zero, as a real sequence has them
        void inverse_real(const slice<const std::complex<double>>& in, const slice<double>& out) const {
            _inverse_real(in, out);
        }

        void inverse_real(const slice<const std::complex<float>>& in, const slice<float>& out) const {
            _inverse_real(in, out);
        }

    private:
        void _check(size_t length, const char* what) const {
            if (length != _plan->full.n) {
                throw invalid_argument(std::string("sgcl::math::fft::") + what + ": a slice of another length than the plan's");
            }
        }

        template<class T>
        void _forward_real(const slice<const T>& in, const slice<std::complex<T>>& out) const {
            size_t n = _plan->full.n;
            if (in.size() != n || out.size() != (n ? n / 2 + 1 : 0)) {
                throw invalid_argument("sgcl::math::fft::forward_real: n numbers in, n/2 + 1 coefficients out");
            }
            if (!n) {
                return;
            }
            if (_plan->half) {
                detail::fft_forward_real(*_plan, in.data(), out.data());
                return;
            }
            detail::FftScratch<T> scratch(n);
            std::complex<T>* z = scratch.get();
            for (size_t i = 0; i < n; ++i) {
                z[i] = in[i];
            }
            detail::fft_forward(_plan->full, z);
            sgcl::detail::copy_bytes(out.data(), z, (n / 2 + 1) * sizeof(std::complex<T>));
        }

        template<class T>
        void _inverse_real(const slice<const std::complex<T>>& in, const slice<T>& out) const {
            size_t n = _plan->full.n;
            if (out.size() != n || in.size() != (n ? n / 2 + 1 : 0)) {
                throw invalid_argument("sgcl::math::fft::inverse_real: n/2 + 1 coefficients in, n numbers out");
            }
            if (!n) {
                return;
            }
            if (_plan->half) {
                detail::fft_inverse_real(*_plan, in.data(), out.data());
                return;
            }
            // the whole spectrum from its half, conjugate symmetric
            detail::FftScratch<T> scratch(n);
            std::complex<T>* z = scratch.get();
            z[0] = {in[0].real(), 0};
            for (size_t k = 1; k <= n / 2; ++k) {
                z[k] = in[k];
            }
            if (n % 2 == 0) {
                z[n / 2] = {in[n / 2].real(), 0};
            }
            for (size_t k = n / 2 + 1; k < n; ++k) {
                z[k] = conj(in[n - k]);
            }
            detail::fft_inverse(_plan->full, z);
            for (size_t i = 0; i < n; ++i) {
                out[i] = z[i].real();
            }
        }

        tracked_ptr<const detail::FftPlan> _plan;
    };

    inline fft::fft(size_t n) {
        if (n > (size_t(1) << 30)) {
            throw length_error("sgcl::math::fft: a length past 2^30");
        }
        tracked_ptr<detail::FftPlan> plan = make_tracked<detail::FftPlan>();
        detail::fft_build(plan->full, n);
        if (n >= 2 && n % 2 == 0) {
            plan->half = std::make_unique<detail::FftEngine>();
            detail::fft_build(*plan->half, n / 2);
            size_t half = n / 2;
            plan->split_d.re.resize(half);
            plan->split_d.im.resize(half);
            for (size_t k = 0; k < half; ++k) {
                double a = -2 * detail::FftPi * double(k) / double(n);
                plan->split_d.re[k] = std::cos(a);
                plan->split_d.im[k] = std::sin(a);
            }
            plan->split_f.re.assign(plan->split_d.re.begin(), plan->split_d.re.end());
            plan->split_f.im.assign(plan->split_d.im.begin(), plan->split_d.im.end());
        }
        _plan = std::move(plan);
    }

    namespace detail {
        template<class T>
        vector<T> convolve(const slice<const T>& a, const slice<const T>& b) {
            size_t p = a.size();
            size_t q = b.size();
            if (!p || !q) {
                return {};
            }
            size_t length = p + q - 1;
            vector<T> out(length, T(0));
            // directly while the shorter is short: no transform pays for itself
            if (std::min(p, q) <= 48) {
                const T* x = a.data();
                const T* y = b.data();
                if (p < q) {
                    std::swap(x, y);
                    std::swap(p, q);
                }
                T* o = out.data();
                for (size_t j = 0; j < q; ++j) {
                    T v = y[j];
                    for (size_t i = 0; i < p; ++i) {
                        o[i + j] += x[i] * v;
                    }
                }
                return out;
            }
            size_t m = std::bit_ceil(length);
            fft plan(m);
            FftScratch<T> fa(m / 2 + 1);
            FftScratch<T> fb(m / 2 + 1);
            std::unique_ptr<T[]> padded = std::make_unique<T[]>(m);
            sgcl::detail::copy_bytes(padded.get(), a.data(), p * sizeof(T));
            plan.forward_real(slice<const T>(padded.get(), m), slice<std::complex<T>>(fa.get(), m / 2 + 1));
            std::fill(padded.get(), padded.get() + m, T(0));
            sgcl::detail::copy_bytes(padded.get(), b.data(), q * sizeof(T));
            plan.forward_real(slice<const T>(padded.get(), m), slice<std::complex<T>>(fb.get(), m / 2 + 1));
            for (size_t k = 0; k <= m / 2; ++k) {
                fa.get()[k] = cmul(fa.get()[k], fb.get()[k]);
            }
            plan.inverse_real(slice<const std::complex<T>>(fa.get(), m / 2 + 1), slice<T>(padded.get(), m));
            sgcl::detail::copy_bytes(out.data(), padded.get(), length * sizeof(T));
            return out;
        }
    }

    // The linear convolution of two sequences: c_k = Σ a_i·b_(k-i), a.size()
    // + b.size() - 1 numbers, empty when either is empty. Directly while the
    // shorter has at most 48 numbers, by real transforms of the next power
    // of two otherwise
    inline vector<double> convolve(const slice<const double>& a, const slice<const double>& b) {
        return detail::convolve(a, b);
    }

    inline vector<float> convolve(const slice<const float>& a, const slice<const float>& b) {
        return detail::convolve(a, b);
    }
}
