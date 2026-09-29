//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "simd.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sgcl::codec::detail {
    // The inverse DCT of an 8×8 block (T.81, A.3.3), in integers: the
    // factorization of Loeffler, Ligtenberg and Moschytz (ICASSP 1989;
    // 12 multiplications, 32 additions a row), its constants in 13-bit
    // fixed point, two passes (columns, then rows) with 2 bits of extra
    // precision between them, each result rounded to the nearest. The
    // choice of those numbers is what libjpeg calls "islow", so that the
    // pixels are libjpeg-turbo's with `-dct int` bit for bit; T.81 itself
    // allows any IDCT within the accuracy of IEEE 1180.
    //
    // coef: the block in natural (row-major) order, quantized; quant: the
    // table in the same order; out: 8 rows of 8 samples, `stride` apart.
    namespace idct {
        inline constexpr int ConstBits = 13;
        inline constexpr int Pass1Bits = 2;

        // round(c × 2^13)
        inline constexpr int64_t C0_298631336 = 2446;
        inline constexpr int64_t C0_390180644 = 3196;
        inline constexpr int64_t C0_541196100 = 4433;
        inline constexpr int64_t C0_765366865 = 6270;
        inline constexpr int64_t C0_899976223 = 7373;
        inline constexpr int64_t C1_175875602 = 9633;
        inline constexpr int64_t C1_501321110 = 12299;
        inline constexpr int64_t C1_847759065 = 15137;
        inline constexpr int64_t C1_961570560 = 16069;
        inline constexpr int64_t C2_053119869 = 16819;
        inline constexpr int64_t C2_562915447 = 20995;
        inline constexpr int64_t C3_072711026 = 25172;

        // x / 2^n to the nearest, halves up
        constexpr int64_t descale(int64_t x, int n) noexcept {
            return (x + (int64_t(1) << (n - 1))) >> n;
        }

        // A sample from the result of the second pass (centered on 0):
        // +128, clamped to 0..255 within [-512, 511] and wrapping outside
        // it, as libjpeg's table of 1024 entries does (only corrupt data
        // gets that far)
        inline uint8_t sample(int64_t x) noexcept {
            const unsigned v = unsigned(x) & 1023u;
            if (v < 128) {
                return uint8_t(v + 128);
            }
            if (v < 512) {
                return 255;
            }
            if (v < 896) {
                return 0;
            }
            return uint8_t(v - 896);
        }

        // One 8-point transform, in → the eight outputs, the terms given as
        // i0..i7 (index k is frequency k); the even part and the odd part
        // of the factorization
        struct Parts {
            int64_t t10, t11, t12, t13;   // the even part's four sums
            int64_t t0, t1, t2, t3;       // the odd part's
        };

        inline Parts transform(int64_t i0, int64_t i1, int64_t i2, int64_t i3, int64_t i4, int64_t i5, int64_t i6, int64_t i7) noexcept {
            Parts p;
            // even: frequencies 0, 2, 4, 6
            const int64_t z1 = (i2 + i6) * C0_541196100;
            const int64_t e2 = z1 - i6 * C1_847759065;
            const int64_t e3 = z1 + i2 * C0_765366865;
            const int64_t e0 = (i0 + i4) << ConstBits;
            const int64_t e1 = (i0 - i4) << ConstBits;
            p.t10 = e0 + e3;
            p.t13 = e0 - e3;
            p.t11 = e1 + e2;
            p.t12 = e1 - e2;
            // odd: frequencies 7, 5, 3, 1
            int64_t o0 = i7, o1 = i5, o2 = i3, o3 = i1;
            int64_t a = o0 + o3, b = o1 + o2, c = o0 + o2, d = o1 + o3;
            const int64_t z5 = (c + d) * C1_175875602;
            o0 *= C0_298631336;
            o1 *= C2_053119869;
            o2 *= C3_072711026;
            o3 *= C1_501321110;
            a *= -C0_899976223;
            b *= -C2_562915447;
            c = c * -C1_961570560 + z5;
            d = d * -C0_390180644 + z5;
            p.t0 = o0 + a + c;
            p.t1 = o1 + b + d;
            p.t2 = o2 + b + c;
            p.t3 = o3 + a + d;
            return p;
        }
    }

    // The plain road, on 64-bit integers: exact on any coefficients
    inline void idct_islow_plain(const int16_t* coef, const uint16_t* quant, uint8_t* out, size_t stride) noexcept {
        using namespace idct;
        int32_t ws[64];
        // pass 1: the columns, dequantized, into the workspace scaled by 2^2
        for (int x = 0; x < 8; ++x) {
            auto q = [&](int k) { return int64_t(coef[8 * k + x]) * quant[8 * k + x]; };
            if (!coef[8 + x] && !coef[16 + x] && !coef[24 + x] && !coef[32 + x] && !coef[40 + x] && !coef[48 + x] && !coef[56 + x]) {
                // only the DC term: the same value down the column
                const int32_t dc = int32_t(q(0) * (1 << Pass1Bits));
                for (int y = 0; y < 8; ++y) {
                    ws[8 * y + x] = dc;
                }
                continue;
            }
            const Parts p = transform(q(0), q(1), q(2), q(3), q(4), q(5), q(6), q(7));
            const int n = ConstBits - Pass1Bits;
            ws[8 * 0 + x] = int32_t(descale(p.t10 + p.t3, n));
            ws[8 * 7 + x] = int32_t(descale(p.t10 - p.t3, n));
            ws[8 * 1 + x] = int32_t(descale(p.t11 + p.t2, n));
            ws[8 * 6 + x] = int32_t(descale(p.t11 - p.t2, n));
            ws[8 * 2 + x] = int32_t(descale(p.t12 + p.t1, n));
            ws[8 * 5 + x] = int32_t(descale(p.t12 - p.t1, n));
            ws[8 * 3 + x] = int32_t(descale(p.t13 + p.t0, n));
            ws[8 * 4 + x] = int32_t(descale(p.t13 - p.t0, n));
        }
        // pass 2: the rows, to samples (the 2^2 and the 8 of the transform's
        // scale divided out)
        for (int y = 0; y < 8; ++y) {
            const int32_t* w = ws + 8 * y;
            uint8_t* o = out + size_t(y) * stride;
            const int n = ConstBits + Pass1Bits + 3;
            const Parts p = transform(w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
            o[0] = sample(descale(p.t10 + p.t3, n));
            o[7] = sample(descale(p.t10 - p.t3, n));
            o[1] = sample(descale(p.t11 + p.t2, n));
            o[6] = sample(descale(p.t11 - p.t2, n));
            o[2] = sample(descale(p.t12 + p.t1, n));
            o[5] = sample(descale(p.t12 - p.t1, n));
            o[3] = sample(descale(p.t13 + p.t0, n));
            o[4] = sample(descale(p.t13 - p.t0, n));
        }
    }

    // A block whose only nonzero coefficient is its DC: every sample the
    // same, the plain road's arithmetic for it written out. Pass 1 puts the
    // dequantized DC times 2^2 down column 0 in its 32-bit workspace (the
    // same conversion, wrapping where the plain road wraps) and zeros
    // elsewhere; pass 2's rows then have their first term alone, so that
    // transform() gives each of the eight outputs as w << ConstBits. Held
    // equal to idct_islow_plain for every DC and every quantizer of 16 bits
    // (all 2^32 pairs, tools/jpeg_dc_only_proof.cpp) and in the tests.
    inline uint8_t idct_dc_only(int16_t dc, uint16_t quant) noexcept {
        using namespace idct;
        const int32_t w = int32_t(int64_t(dc) * quant * (1 << Pass1Bits));
        return sample(descale(int64_t(w) << ConstBits, ConstBits + Pass1Bits + 3));
    }

    // The 8x8 samples of such a block: one value, eight bytes a row
    inline void idct_dc_only_fill(int16_t dc, uint16_t quant, uint8_t* out, size_t stride) noexcept {
        const uint64_t row = 0x0101010101010101u * idct_dc_only(dc, quant);
        for (int y = 0; y < 8; ++y) {
            std::memcpy(out + size_t(y) * stride, &row, 8);
        }
    }

#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    // The same transform on vectors of eight 16-bit inputs, 32-bit sums:
    // the factorization multiplied out, each output a sum of products of
    // the inputs by constants (the grouping differs, the integers are the
    // same, and nothing is rounded before the descale):
    //
    //   even  e0 = 8192 (i0 + i4)          e1 = 8192 (i0 - i4)
    //         e2 = 4433 i2 - 10704 i6      e3 = 10703 i2 + 4433 i6
    //   odd   t0 = -11363 i7 + 2260 i1 - 6436 i3 + 9633 i5
    //         t1 =   9633 i7 + 6437 i1 - 11362 i3 + 2261 i5
    //         t2 =  -6436 i7 + 9633 i1 - 2259 i3 - 11362 i5
    //         t3 =   2260 i7 + 11363 i1 + 9633 i3 + 6437 i5
    //
    // (2259 = 25172 - 20995 - 16069 + 9633, and so on: each constant the sum
    // of those its input meets in transform() above). The constants' sums of
    // magnitudes stay under 61214, so with inputs in 16 bits no sum passes
    // 2^31: the vector road takes a block whose dequantized coefficients and
    // whose first pass's results fit 16 bits (every block of a valid file),
    // and leaves any other to the plain one, which is exact on 64 bits.
    namespace idct {
        inline constexpr int16_t K8192 = 8192, K4433 = 4433, Km10704 = -10704, K10703 = 10703;
        inline constexpr int16_t Km11363 = -11363, K2260 = 2260, Km6436 = -6436, K9633 = 9633;
        inline constexpr int16_t K6437 = 6437, Km11362 = -11362, K2261 = 2261, Km2259 = -2259, K11363 = 11363;
    }
#endif

#if defined(SGCL_CODEC_NEON)
    namespace idct {
        // One pass over eight vectors (input k: frequency k of eight lines),
        // its eight outputs rounded by `Shift`, 32 bits, four lanes a half
        template<int Shift>
        inline void pass_neon(const int16x8_t (&v)[8], int32x4_t (&lo)[8], int32x4_t (&hi)[8]) noexcept {
            for (int h = 0; h < 2; ++h) {
                auto half = [&](int k) { return h ? vget_high_s16(v[k]) : vget_low_s16(v[k]); };
                const int16x4_t i0 = half(0), i1 = half(1), i2 = half(2), i3 = half(3);
                const int16x4_t i4 = half(4), i5 = half(5), i6 = half(6), i7 = half(7);
                const int32x4_t a0 = vmull_n_s16(i0, K8192), a4 = vmull_n_s16(i4, K8192);
                const int32x4_t e0 = vaddq_s32(a0, a4), e1 = vsubq_s32(a0, a4);
                const int32x4_t e2 = vmlal_n_s16(vmull_n_s16(i2, K4433), i6, Km10704);
                const int32x4_t e3 = vmlal_n_s16(vmull_n_s16(i2, K10703), i6, K4433);
                const int32x4_t t10 = vaddq_s32(e0, e3), t13 = vsubq_s32(e0, e3);
                const int32x4_t t11 = vaddq_s32(e1, e2), t12 = vsubq_s32(e1, e2);
                const int32x4_t t0 = vmlal_n_s16(vmlal_n_s16(vmlal_n_s16(vmull_n_s16(i7, Km11363), i1, K2260), i3, Km6436), i5, K9633);
                const int32x4_t t1 = vmlal_n_s16(vmlal_n_s16(vmlal_n_s16(vmull_n_s16(i7, K9633), i1, K6437), i3, Km11362), i5, K2261);
                const int32x4_t t2 = vmlal_n_s16(vmlal_n_s16(vmlal_n_s16(vmull_n_s16(i7, Km6436), i1, K9633), i3, Km2259), i5, Km11362);
                const int32x4_t t3 = vmlal_n_s16(vmlal_n_s16(vmlal_n_s16(vmull_n_s16(i7, K2260), i1, K11363), i3, K9633), i5, K6437);
                int32x4_t (&o)[8] = h ? hi : lo;
                o[0] = vrshrq_n_s32(vaddq_s32(t10, t3), Shift);
                o[7] = vrshrq_n_s32(vsubq_s32(t10, t3), Shift);
                o[1] = vrshrq_n_s32(vaddq_s32(t11, t2), Shift);
                o[6] = vrshrq_n_s32(vsubq_s32(t11, t2), Shift);
                o[2] = vrshrq_n_s32(vaddq_s32(t12, t1), Shift);
                o[5] = vrshrq_n_s32(vsubq_s32(t12, t1), Shift);
                o[3] = vrshrq_n_s32(vaddq_s32(t13, t0), Shift);
                o[4] = vrshrq_n_s32(vsubq_s32(t13, t0), Shift);
            }
        }

        // An 8×8 matrix of 16-bit values transposed in place
        inline void transpose_neon(int16x8_t (&m)[8]) noexcept {
            const int16x8x2_t a01 = vtrnq_s16(m[0], m[1]), a23 = vtrnq_s16(m[2], m[3]);
            const int16x8x2_t a45 = vtrnq_s16(m[4], m[5]), a67 = vtrnq_s16(m[6], m[7]);
            const int32x4x2_t b0 = vtrnq_s32(vreinterpretq_s32_s16(a01.val[0]), vreinterpretq_s32_s16(a23.val[0]));
            const int32x4x2_t b1 = vtrnq_s32(vreinterpretq_s32_s16(a01.val[1]), vreinterpretq_s32_s16(a23.val[1]));
            const int32x4x2_t b2 = vtrnq_s32(vreinterpretq_s32_s16(a45.val[0]), vreinterpretq_s32_s16(a67.val[0]));
            const int32x4x2_t b3 = vtrnq_s32(vreinterpretq_s32_s16(a45.val[1]), vreinterpretq_s32_s16(a67.val[1]));
            auto low64 = [](int32x4_t x, int32x4_t y) {
                return vreinterpretq_s16_s64(vtrn1q_s64(vreinterpretq_s64_s32(x), vreinterpretq_s64_s32(y)));
            };
            auto high64 = [](int32x4_t x, int32x4_t y) {
                return vreinterpretq_s16_s64(vtrn2q_s64(vreinterpretq_s64_s32(x), vreinterpretq_s64_s32(y)));
            };
            m[0] = low64(b0.val[0], b2.val[0]);
            m[1] = low64(b1.val[0], b3.val[0]);
            m[2] = low64(b0.val[1], b2.val[1]);
            m[3] = low64(b1.val[1], b3.val[1]);
            m[4] = high64(b0.val[0], b2.val[0]);
            m[5] = high64(b1.val[0], b3.val[0]);
            m[6] = high64(b0.val[1], b2.val[1]);
            m[7] = high64(b1.val[1], b3.val[1]);
        }

        // Eight 32-bit halves into 16 bits; false when one does not fit
        inline bool narrow_neon(const int32x4_t (&lo)[8], const int32x4_t (&hi)[8], int16x8_t (&out)[8]) noexcept {
            int32x4_t low = vdupq_n_s32(0), high = vdupq_n_s32(0);
            for (int k = 0; k < 8; ++k) {
                low = vminq_s32(low, vminq_s32(lo[k], hi[k]));
                high = vmaxq_s32(high, vmaxq_s32(lo[k], hi[k]));
                out[k] = vcombine_s16(vmovn_s32(lo[k]), vmovn_s32(hi[k]));
            }
            return vminvq_s32(low) >= -32768 && vmaxvq_s32(high) <= 32767;
        }
    }

    // The block by the vector road; false (nothing written) when its
    // numbers leave 16 bits, for the plain road to take it
    inline bool idct_islow_vector(const int16_t* coef, const uint16_t* quant, uint8_t* out, size_t stride) noexcept {
        using namespace idct;
        int32x4_t lo[8], hi[8];
        for (int r = 0; r < 8; ++r) {
            const int16x8_t c = vld1q_s16(coef + 8 * r);
            const uint16x8_t q = vld1q_u16(quant + 8 * r);
            lo[r] = vmulq_s32(vmovl_s16(vget_low_s16(c)), vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(q))));
            hi[r] = vmulq_s32(vmovl_s16(vget_high_s16(c)), vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(q))));
        }
        int16x8_t m[8];
        if (!narrow_neon(lo, hi, m)) {
            return false;
        }
        // pass 1: the columns (vector r is row r, a lane a column)
        pass_neon<ConstBits - Pass1Bits>(m, lo, hi);
        if (!narrow_neon(lo, hi, m)) {
            return false;
        }
        // pass 2: the rows, their frequencies across the lanes
        transpose_neon(m);
        pass_neon<ConstBits + Pass1Bits + 3>(m, lo, hi);
        int16x8_t s[8];
        for (int k = 0; k < 8; ++k) {
            // sample(): (x + 128) & 1023 as it is below 256, 255 below 640,
            // 0 above
            const int16x8_t x = vcombine_s16(vmovn_s32(lo[k]), vmovn_s32(hi[k]));
            const int16x8_t u = vandq_s16(vaddq_s16(x, vdupq_n_s16(128)), vdupq_n_s16(1023));
            s[k] = vbicq_s16(vminq_s16(u, vdupq_n_s16(255)), vreinterpretq_s16_u16(vcgtq_s16(u, vdupq_n_s16(639))));
        }
        transpose_neon(s);
        for (int y = 0; y < 8; ++y) {
            vst1_u8(out + size_t(y) * stride, vqmovun_s16(s[y]));
        }
        return true;
    }
#elif defined(SGCL_CODEC_SSE2)
    namespace idct {
        // The pair (a, b) repeated: PMADDWD's constants for inputs
        // interleaved as (x, y) pairs, x·a + y·b
        inline __m128i pair(int16_t a, int16_t b) noexcept {
            return _mm_set1_epi32(int(uint16_t(a)) | int(uint32_t(uint16_t(b)) << 16));
        }

        template<int Shift>
        inline __m128i round_sse2(__m128i x) noexcept {
            return _mm_srai_epi32(_mm_add_epi32(x, _mm_set1_epi32(1 << (Shift - 1))), Shift);
        }

        template<int Shift>
        inline void pass_sse2(const __m128i (&v)[8], __m128i (&lo)[8], __m128i (&hi)[8]) noexcept {
            for (int h = 0; h < 2; ++h) {
                auto zip = [&](int a, int b) { return h ? _mm_unpackhi_epi16(v[a], v[b]) : _mm_unpacklo_epi16(v[a], v[b]); };
                const __m128i p04 = zip(0, 4), p26 = zip(2, 6), p71 = zip(7, 1), p35 = zip(3, 5);
                const __m128i e0 = _mm_madd_epi16(p04, pair(K8192, K8192));
                const __m128i e1 = _mm_madd_epi16(p04, pair(K8192, int16_t(-K8192)));
                const __m128i e2 = _mm_madd_epi16(p26, pair(K4433, Km10704));
                const __m128i e3 = _mm_madd_epi16(p26, pair(K10703, K4433));
                const __m128i t10 = _mm_add_epi32(e0, e3), t13 = _mm_sub_epi32(e0, e3);
                const __m128i t11 = _mm_add_epi32(e1, e2), t12 = _mm_sub_epi32(e1, e2);
                const __m128i t0 = _mm_add_epi32(_mm_madd_epi16(p71, pair(Km11363, K2260)), _mm_madd_epi16(p35, pair(Km6436, K9633)));
                const __m128i t1 = _mm_add_epi32(_mm_madd_epi16(p71, pair(K9633, K6437)), _mm_madd_epi16(p35, pair(Km11362, K2261)));
                const __m128i t2 = _mm_add_epi32(_mm_madd_epi16(p71, pair(Km6436, K9633)), _mm_madd_epi16(p35, pair(Km2259, Km11362)));
                const __m128i t3 = _mm_add_epi32(_mm_madd_epi16(p71, pair(K2260, K11363)), _mm_madd_epi16(p35, pair(K9633, K6437)));
                __m128i (&o)[8] = h ? hi : lo;
                o[0] = round_sse2<Shift>(_mm_add_epi32(t10, t3));
                o[7] = round_sse2<Shift>(_mm_sub_epi32(t10, t3));
                o[1] = round_sse2<Shift>(_mm_add_epi32(t11, t2));
                o[6] = round_sse2<Shift>(_mm_sub_epi32(t11, t2));
                o[2] = round_sse2<Shift>(_mm_add_epi32(t12, t1));
                o[5] = round_sse2<Shift>(_mm_sub_epi32(t12, t1));
                o[3] = round_sse2<Shift>(_mm_add_epi32(t13, t0));
                o[4] = round_sse2<Shift>(_mm_sub_epi32(t13, t0));
            }
        }

        inline void transpose_sse2(__m128i (&m)[8]) noexcept {
            const __m128i a0 = _mm_unpacklo_epi16(m[0], m[1]), a1 = _mm_unpackhi_epi16(m[0], m[1]);
            const __m128i a2 = _mm_unpacklo_epi16(m[2], m[3]), a3 = _mm_unpackhi_epi16(m[2], m[3]);
            const __m128i a4 = _mm_unpacklo_epi16(m[4], m[5]), a5 = _mm_unpackhi_epi16(m[4], m[5]);
            const __m128i a6 = _mm_unpacklo_epi16(m[6], m[7]), a7 = _mm_unpackhi_epi16(m[6], m[7]);
            const __m128i b0 = _mm_unpacklo_epi32(a0, a2), b1 = _mm_unpackhi_epi32(a0, a2);
            const __m128i b2 = _mm_unpacklo_epi32(a1, a3), b3 = _mm_unpackhi_epi32(a1, a3);
            const __m128i b4 = _mm_unpacklo_epi32(a4, a6), b5 = _mm_unpackhi_epi32(a4, a6);
            const __m128i b6 = _mm_unpacklo_epi32(a5, a7), b7 = _mm_unpackhi_epi32(a5, a7);
            m[0] = _mm_unpacklo_epi64(b0, b4);
            m[1] = _mm_unpackhi_epi64(b0, b4);
            m[2] = _mm_unpacklo_epi64(b1, b5);
            m[3] = _mm_unpackhi_epi64(b1, b5);
            m[4] = _mm_unpacklo_epi64(b2, b6);
            m[5] = _mm_unpackhi_epi64(b2, b6);
            m[6] = _mm_unpacklo_epi64(b3, b7);
            m[7] = _mm_unpackhi_epi64(b3, b7);
        }

        // Eight 32-bit halves into 16 bits; false when one does not fit
        inline bool narrow_sse2(const __m128i (&lo)[8], const __m128i (&hi)[8], __m128i (&out)[8]) noexcept {
            const __m128i bias = _mm_set1_epi32(32768);
            __m128i outside = _mm_setzero_si128();
            for (int k = 0; k < 8; ++k) {
                outside = _mm_or_si128(outside, _mm_srli_epi32(_mm_add_epi32(lo[k], bias), 16));
                outside = _mm_or_si128(outside, _mm_srli_epi32(_mm_add_epi32(hi[k], bias), 16));
                out[k] = _mm_packs_epi32(lo[k], hi[k]);
            }
            return _mm_movemask_epi8(_mm_cmpeq_epi32(outside, _mm_setzero_si128())) == 0xFFFF;
        }
    }

    inline bool idct_islow_vector(const int16_t* coef, const uint16_t* quant, uint8_t* out, size_t stride) noexcept {
        using namespace idct;
        __m128i lo[8], hi[8];
        for (int r = 0; r < 8; ++r) {
            const __m128i c = _mm_loadu_si128(reinterpret_cast<const __m128i*>(coef + 8 * r));
            const __m128i q = _mm_loadu_si128(reinterpret_cast<const __m128i*>(quant + 8 * r));
            // the products of a signed and an unsigned 16-bit value, 32 bits:
            // the unsigned one's high bit taken as its own term
            const __m128i low16 = _mm_mullo_epi16(c, q);
            __m128i high16 = _mm_mulhi_epi16(c, q);
            high16 = _mm_add_epi16(high16, _mm_and_si128(c, _mm_srai_epi16(q, 15)));
            lo[r] = _mm_unpacklo_epi16(low16, high16);
            hi[r] = _mm_unpackhi_epi16(low16, high16);
        }
        __m128i m[8];
        if (!narrow_sse2(lo, hi, m)) {
            return false;
        }
        pass_sse2<ConstBits - Pass1Bits>(m, lo, hi);
        if (!narrow_sse2(lo, hi, m)) {
            return false;
        }
        transpose_sse2(m);
        pass_sse2<ConstBits + Pass1Bits + 3>(m, lo, hi);
        __m128i s[8];
        for (int k = 0; k < 8; ++k) {
            const __m128i x = _mm_packs_epi32(lo[k], hi[k]);
            const __m128i u = _mm_and_si128(_mm_add_epi16(x, _mm_set1_epi16(128)), _mm_set1_epi16(1023));
            s[k] = _mm_andnot_si128(_mm_cmpgt_epi16(u, _mm_set1_epi16(639)), _mm_min_epi16(u, _mm_set1_epi16(255)));
        }
        transpose_sse2(s);
        for (int y = 0; y < 8; y += 2) {
            const __m128i two = _mm_packus_epi16(s[y], s[y + 1]);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(out + size_t(y) * stride), two);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(out + size_t(y + 1) * stride), _mm_srli_si128(two, 8));
        }
        return true;
    }
#endif

    inline void idct_islow(const int16_t* coef, const uint16_t* quant, uint8_t* out, size_t stride) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        if (idct_islow_vector(coef, quant, out, stride)) {
            return;
        }
#endif
        idct_islow_plain(coef, quant, out, stride);
    }
}
