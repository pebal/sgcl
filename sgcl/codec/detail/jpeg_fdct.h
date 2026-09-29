//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "jpeg_huffman.h"
#include "jpeg_idct.h"
#include "simd.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sgcl::codec::detail {
    // The forward DCT of an 8×8 block (T.81, A.3.3), in integers: the
    // factorization of Loeffler, Ligtenberg and Moschytz run forwards, the
    // constants of jpeg_idct.h (13-bit fixed point), 2 bits of extra
    // precision between the passes (rows, then columns), each result
    // rounded to the nearest. The output is 8 times the DCT of T.81 (the
    // quantizer divides by 8 more): libjpeg's "islow", so that the
    // coefficients are cjpeg's with `-dct int` bit for bit.
    //
    // in: 8 rows of 8 samples, `stride` apart; out: the 64 coefficients,
    // natural (row-major) order, each within ±8192 (the DC is the sum of
    // the 64 samples less 128 each).
    //
    // The plain road; fdct_islow() below takes the vector one where there
    // is one.
    inline void fdct_islow_plain(const uint8_t* in, size_t stride, int32_t* out) noexcept {
        using namespace idct;
        int64_t ws[64];
        // pass 1: the rows, centered on 0, scaled up by 2^2
        for (int y = 0; y < 8; ++y) {
            const uint8_t* s = in + size_t(y) * stride;
            int64_t* o = ws + 8 * y;
            int64_t d[8];
            for (int i = 0; i < 8; ++i) {
                d[i] = int64_t(s[i]) - 128;
            }
            const int64_t t0 = d[0] + d[7], t7 = d[0] - d[7];
            const int64_t t1 = d[1] + d[6], t6 = d[1] - d[6];
            const int64_t t2 = d[2] + d[5], t5 = d[2] - d[5];
            const int64_t t3 = d[3] + d[4], t4 = d[3] - d[4];
            // even
            const int64_t t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2;
            o[0] = (t10 + t11) * (1 << Pass1Bits);
            o[4] = (t10 - t11) * (1 << Pass1Bits);
            const int64_t z1 = (t12 + t13) * C0_541196100;
            o[2] = descale(z1 + t13 * C0_765366865, ConstBits - Pass1Bits);
            o[6] = descale(z1 - t12 * C1_847759065, ConstBits - Pass1Bits);
            // odd
            int64_t a = t4 + t7, b = t5 + t6, c = t4 + t6, e = t5 + t7;
            const int64_t z5 = (c + e) * C1_175875602;
            const int64_t u4 = t4 * C0_298631336, u5 = t5 * C2_053119869, u6 = t6 * C3_072711026, u7 = t7 * C1_501321110;
            a *= -C0_899976223;
            b *= -C2_562915447;
            c = c * -C1_961570560 + z5;
            e = e * -C0_390180644 + z5;
            o[7] = descale(u4 + a + c, ConstBits - Pass1Bits);
            o[5] = descale(u5 + b + e, ConstBits - Pass1Bits);
            o[3] = descale(u6 + b + c, ConstBits - Pass1Bits);
            o[1] = descale(u7 + a + e, ConstBits - Pass1Bits);
        }
        // pass 2: the columns, the 2^2 taken out
        for (int x = 0; x < 8; ++x) {
            auto w = [&](int k) { return ws[8 * k + x]; };
            const int64_t t0 = w(0) + w(7), t7 = w(0) - w(7);
            const int64_t t1 = w(1) + w(6), t6 = w(1) - w(6);
            const int64_t t2 = w(2) + w(5), t5 = w(2) - w(5);
            const int64_t t3 = w(3) + w(4), t4 = w(3) - w(4);
            const int64_t t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2;
            out[8 * 0 + x] = int32_t(descale(t10 + t11, Pass1Bits));
            out[8 * 4 + x] = int32_t(descale(t10 - t11, Pass1Bits));
            const int64_t z1 = (t12 + t13) * C0_541196100;
            out[8 * 2 + x] = int32_t(descale(z1 + t13 * C0_765366865, ConstBits + Pass1Bits));
            out[8 * 6 + x] = int32_t(descale(z1 - t12 * C1_847759065, ConstBits + Pass1Bits));
            int64_t a = t4 + t7, b = t5 + t6, c = t4 + t6, e = t5 + t7;
            const int64_t z5 = (c + e) * C1_175875602;
            const int64_t u4 = t4 * C0_298631336, u5 = t5 * C2_053119869, u6 = t6 * C3_072711026, u7 = t7 * C1_501321110;
            a *= -C0_899976223;
            b *= -C2_562915447;
            c = c * -C1_961570560 + z5;
            e = e * -C0_390180644 + z5;
            out[8 * 7 + x] = int32_t(descale(u4 + a + c, ConstBits + Pass1Bits));
            out[8 * 5 + x] = int32_t(descale(u5 + b + e, ConstBits + Pass1Bits));
            out[8 * 3 + x] = int32_t(descale(u6 + b + c, ConstBits + Pass1Bits));
            out[8 * 1 + x] = int32_t(descale(u7 + a + e, ConstBits + Pass1Bits));
        }
    }

#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    // The vector road: eight lines of a pass at once, the factorization
    // multiplied out as for the inverse (jpeg_idct.h), with the same
    // constants in the transposed places:
    //
    //   even  o0 = t10 + t11                  o4 = t10 - t11
    //         o2 = 10703 t13 + 4433 t12       o6 = 4433 t13 - 10704 t12
    //   odd   o7 = -11363 t4 +  9633 t5 -  6436 t6 +  2260 t7
    //         o5 =   9633 t4 +  2261 t5 - 11362 t6 +  6437 t7
    //         o3 =  -6436 t4 - 11362 t5 -  2259 t6 +  9633 t7
    //         o1 =   2260 t4 +  6437 t5 +  9633 t6 + 11363 t7
    //
    // (-11363 = 2446 - 7373 - 16069 + 9633, and so on); nothing is rounded
    // before the descale, so the integers are the plain road's. From 8-bit
    // samples every number fits: the first pass's inputs within ±128 and
    // its results within ±4096 (16 bits); the second's sums and
    // differences within ±16384 (16 bits), t10 ± t11 taken on 32, its
    // products under 2.5 × 10^8 (32 bits). No block leaves the road. The
    // constants, the transpose and the helpers are jpeg_idct.h's.
    namespace fdct {
        using namespace idct;
    }
#endif

#if defined(SGCL_CODEC_NEON)
    namespace fdct {
        // One pass over eight vectors (input k: sample k of eight lines),
        // 32-bit results, four lanes a half; the DC and Nyquist terms
        // scaled up by 2^2 in the first pass, rounded down by it in the
        // second
        template<bool Second>
        inline void forward_neon(const int16x8_t (&v)[8], int32x4_t (&lo)[8], int32x4_t (&hi)[8]) noexcept {
            constexpr int Shift = Second ? ConstBits + Pass1Bits : ConstBits - Pass1Bits;
            const int16x8_t t0 = vaddq_s16(v[0], v[7]), t7 = vsubq_s16(v[0], v[7]);
            const int16x8_t t1 = vaddq_s16(v[1], v[6]), t6 = vsubq_s16(v[1], v[6]);
            const int16x8_t t2 = vaddq_s16(v[2], v[5]), t5 = vsubq_s16(v[2], v[5]);
            const int16x8_t t3 = vaddq_s16(v[3], v[4]), t4 = vsubq_s16(v[3], v[4]);
            const int16x8_t t10 = vaddq_s16(t0, t3), t13 = vsubq_s16(t0, t3);
            const int16x8_t t11 = vaddq_s16(t1, t2), t12 = vsubq_s16(t1, t2);
            for (int h = 0; h < 2; ++h) {
                auto half = [&](int16x8_t x) { return h ? vget_high_s16(x) : vget_low_s16(x); };
                int32x4_t (&o)[8] = h ? hi : lo;
                const int32x4_t sum = vaddl_s16(half(t10), half(t11)), difference = vsubl_s16(half(t10), half(t11));
                if constexpr (Second) {
                    o[0] = vrshrq_n_s32(sum, Pass1Bits);
                    o[4] = vrshrq_n_s32(difference, Pass1Bits);
                } else {
                    o[0] = vshlq_n_s32(sum, Pass1Bits);
                    o[4] = vshlq_n_s32(difference, Pass1Bits);
                }
                o[2] = vrshrq_n_s32(vmlal_n_s16(vmull_n_s16(half(t13), K10703), half(t12), K4433), Shift);
                o[6] = vrshrq_n_s32(vmlal_n_s16(vmull_n_s16(half(t13), K4433), half(t12), Km10704), Shift);
                const int16x4_t a = half(t4), b = half(t5), c = half(t6), d = half(t7);
                o[7] = vrshrq_n_s32(vmlal_n_s16(vmlal_n_s16(vmlal_n_s16(vmull_n_s16(a, Km11363), b, K9633), c, Km6436), d, K2260), Shift);
                o[5] = vrshrq_n_s32(vmlal_n_s16(vmlal_n_s16(vmlal_n_s16(vmull_n_s16(a, K9633), b, K2261), c, Km11362), d, K6437), Shift);
                o[3] = vrshrq_n_s32(vmlal_n_s16(vmlal_n_s16(vmlal_n_s16(vmull_n_s16(a, Km6436), b, Km11362), c, Km2259), d, K9633), Shift);
                o[1] = vrshrq_n_s32(vmlal_n_s16(vmlal_n_s16(vmlal_n_s16(vmull_n_s16(a, K2260), b, K6437), c, K9633), d, K11363), Shift);
            }
        }
    }

    inline void fdct_islow_vector(const uint8_t* in, size_t stride, int32_t* out) noexcept {
        using namespace fdct;
        int16x8_t m[8];
        for (int y = 0; y < 8; ++y) {
            m[y] = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(in + size_t(y) * stride), vdup_n_u8(128)));
        }
        // pass 1: the rows (after the transpose vector k is sample k, a
        // lane a row)
        transpose_neon(m);
        int32x4_t lo[8], hi[8];
        forward_neon<false>(m, lo, hi);
        for (int k = 0; k < 8; ++k) {
            m[k] = vcombine_s16(vmovn_s32(lo[k]), vmovn_s32(hi[k]));
        }
        // pass 2: the columns (vector y row y's results, a lane a column)
        transpose_neon(m);
        forward_neon<true>(m, lo, hi);
        for (int k = 0; k < 8; ++k) {
            vst1q_s32(out + 8 * k, lo[k]);
            vst1q_s32(out + 8 * k + 4, hi[k]);
        }
    }
#elif defined(SGCL_CODEC_SSE2)
    namespace fdct {
        template<bool Second>
        inline void forward_sse2(const __m128i (&v)[8], __m128i (&lo)[8], __m128i (&hi)[8]) noexcept {
            constexpr int Shift = Second ? ConstBits + Pass1Bits : ConstBits - Pass1Bits;
            const __m128i t0 = _mm_add_epi16(v[0], v[7]), t7 = _mm_sub_epi16(v[0], v[7]);
            const __m128i t1 = _mm_add_epi16(v[1], v[6]), t6 = _mm_sub_epi16(v[1], v[6]);
            const __m128i t2 = _mm_add_epi16(v[2], v[5]), t5 = _mm_sub_epi16(v[2], v[5]);
            const __m128i t3 = _mm_add_epi16(v[3], v[4]), t4 = _mm_sub_epi16(v[3], v[4]);
            const __m128i t10 = _mm_add_epi16(t0, t3), t13 = _mm_sub_epi16(t0, t3);
            const __m128i t11 = _mm_add_epi16(t1, t2), t12 = _mm_sub_epi16(t1, t2);
            for (int h = 0; h < 2; ++h) {
                auto zip = [&](__m128i a, __m128i b) { return h ? _mm_unpackhi_epi16(a, b) : _mm_unpacklo_epi16(a, b); };
                __m128i (&o)[8] = h ? hi : lo;
                const __m128i p1011 = zip(t10, t11), p1312 = zip(t13, t12), p45 = zip(t4, t5), p67 = zip(t6, t7);
                const __m128i sum = _mm_madd_epi16(p1011, pair(1, 1)), difference = _mm_madd_epi16(p1011, pair(1, -1));
                if constexpr (Second) {
                    o[0] = round_sse2<Pass1Bits>(sum);
                    o[4] = round_sse2<Pass1Bits>(difference);
                } else {
                    o[0] = _mm_slli_epi32(sum, Pass1Bits);
                    o[4] = _mm_slli_epi32(difference, Pass1Bits);
                }
                o[2] = round_sse2<Shift>(_mm_madd_epi16(p1312, pair(K10703, K4433)));
                o[6] = round_sse2<Shift>(_mm_madd_epi16(p1312, pair(K4433, Km10704)));
                auto odd = [&](int16_t k4, int16_t k5, int16_t k6, int16_t k7) {
                    return round_sse2<Shift>(_mm_add_epi32(_mm_madd_epi16(p45, pair(k4, k5)), _mm_madd_epi16(p67, pair(k6, k7))));
                };
                o[7] = odd(Km11363, K9633, Km6436, K2260);
                o[5] = odd(K9633, K2261, Km11362, K6437);
                o[3] = odd(Km6436, Km11362, Km2259, K9633);
                o[1] = odd(K2260, K6437, K9633, K11363);
            }
        }
    }

    inline void fdct_islow_vector(const uint8_t* in, size_t stride, int32_t* out) noexcept {
        using namespace fdct;
        const __m128i zero = _mm_setzero_si128(), centre = _mm_set1_epi16(128);
        __m128i m[8];
        for (int y = 0; y < 8; ++y) {
            const __m128i row = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(in + size_t(y) * stride));
            m[y] = _mm_sub_epi16(_mm_unpacklo_epi8(row, zero), centre);
        }
        transpose_sse2(m);
        __m128i lo[8], hi[8];
        forward_sse2<false>(m, lo, hi);
        for (int k = 0; k < 8; ++k) {
            m[k] = _mm_packs_epi32(lo[k], hi[k]);
        }
        transpose_sse2(m);
        forward_sse2<true>(m, lo, hi);
        for (int k = 0; k < 8; ++k) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 8 * k), lo[k]);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 8 * k + 4), hi[k]);
        }
    }
#endif

    inline void fdct_islow(const uint8_t* in, size_t stride, int32_t* out) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        fdct_islow_vector(in, stride, out);
#else
        fdct_islow_plain(in, stride, out);
#endif
    }

    // A block's coefficients quantized: to the nearest, halves away from
    // zero (the DCT's output is 8 times T.81's: the step is 8q). The plain
    // road divides.
    inline void quantize_plain(const int32_t* dct, const uint16_t* q, int16_t* out) noexcept {
        for (int i = 0; i < 64; ++i) {
            const int32_t step = int32_t(q[i]) * 8;
            const int32_t x = dct[i];
            out[i] = int16_t(x < 0 ? -((-x + step / 2) / step) : (x + step / 2) / step);
        }
    }

    // The vector road multiplies instead: with v = |x| + 4q, v / 8q is
    // (v · m) >> (16 + s), s = floor(log2 8q) and m = ceil(2^(16+s) / 8q)
    // (for a power of two m = 2^15 and s one less), taken as the high
    // halves of two 16-bit products, ((v · m) >> 16) · 2^(16-s) >> 16, which
    // both instruction sets have. It equals the division for every q of
    // 1..255 (a baseline table) and every v under 2^15, each tried
    // (tests/codec/simd_paths.cpp); the FDCT's ±8192 plus 4q stays under
    // that. A table with a step outside 1..255 keeps the division.
    struct QuantSteps {
        uint16_t q[64] = {}, half[64] = {}, m[64] = {}, scale[64] = {};
        bool reciprocal = false;

        QuantSteps() noexcept = default;

        explicit QuantSteps(const uint16_t* table) noexcept {
            std::memcpy(q, table, sizeof q);
            for (int i = 0; i < 64; ++i) {
                const uint32_t qi = table[i];
                if (qi < 1 || qi > 255) {
                    return;
                }
                const uint32_t step = 8 * qi;
                uint32_t s = uint32_t(std::bit_width(step)) - 1;
                uint32_t mi;
                if ((step & (step - 1)) == 0) {
                    mi = 1u << 15;
                    s -= 1;
                } else {
                    mi = uint32_t(((uint64_t(1) << (16 + s)) + step - 1) / step);
                }
                half[i] = uint16_t(4 * qi);
                m[i] = uint16_t(mi);
                scale[i] = uint16_t(1u << (16 - s));
            }
            reciprocal = true;
        }
    };

#if defined(SGCL_CODEC_NEON)
    inline void quantize_vector(const int32_t* dct, const QuantSteps& steps, int16_t* out) noexcept {
        for (int i = 0; i < 64; i += 8) {
            const int16x8_t x = vcombine_s16(vmovn_s32(vld1q_s32(dct + i)), vmovn_s32(vld1q_s32(dct + i + 4)));
            const int16x8_t sign = vshrq_n_s16(x, 15);
            const uint16x8_t v = vaddq_u16(vreinterpretq_u16_s16(vabsq_s16(x)), vld1q_u16(steps.half + i));
            const uint16x8_t m = vld1q_u16(steps.m + i), scale = vld1q_u16(steps.scale + i);
            const uint16x8_t p = vcombine_u16(vshrn_n_u32(vmull_u16(vget_low_u16(v), vget_low_u16(m)), 16),
                                              vshrn_n_u32(vmull_u16(vget_high_u16(v), vget_high_u16(m)), 16));
            const uint16x8_t d = vcombine_u16(vshrn_n_u32(vmull_u16(vget_low_u16(p), vget_low_u16(scale)), 16),
                                              vshrn_n_u32(vmull_u16(vget_high_u16(p), vget_high_u16(scale)), 16));
            vst1q_s16(out + i, vsubq_s16(veorq_s16(vreinterpretq_s16_u16(d), sign), sign));
        }
    }
#elif defined(SGCL_CODEC_SSE2)
    inline void quantize_vector(const int32_t* dct, const QuantSteps& steps, int16_t* out) noexcept {
        for (int i = 0; i < 64; i += 8) {
            const __m128i x = _mm_packs_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(dct + i)),
                                              _mm_loadu_si128(reinterpret_cast<const __m128i*>(dct + i + 4)));
            const __m128i sign = _mm_srai_epi16(x, 15);
            const __m128i a = _mm_sub_epi16(_mm_xor_si128(x, sign), sign);
            const __m128i v = _mm_add_epi16(a, _mm_loadu_si128(reinterpret_cast<const __m128i*>(steps.half + i)));
            const __m128i p = _mm_mulhi_epu16(v, _mm_loadu_si128(reinterpret_cast<const __m128i*>(steps.m + i)));
            const __m128i d = _mm_mulhi_epu16(p, _mm_loadu_si128(reinterpret_cast<const __m128i*>(steps.scale + i)));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), _mm_sub_epi16(_mm_xor_si128(d, sign), sign));
        }
    }
#endif

    // dct: the output of fdct_islow() (each within ±8192)
    inline void quantize(const int32_t* dct, const QuantSteps& steps, int16_t* out) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        if (steps.reciprocal) {
            quantize_vector(dct, steps, out);
            return;
        }
#endif
        quantize_plain(dct, steps.q, out);
    }

    // The block in zigzag order (F.1.2.2's order of the AC coefficients)
    // and a mask of its nonzero ones: bit i for coefficient i of that order.
    // The coder walks the mask, a run of zeros the distance between two set
    // bits, instead of looking at all 63.
    inline uint64_t nonzero_mask_plain(const int16_t* z) noexcept {
        uint64_t m = 0;
        for (int i = 0; i < 64; ++i) {
            m |= uint64_t(z[i] != 0) << i;
        }
        return m;
    }

#if defined(SGCL_CODEC_NEON)
    // Lanes of 0 or 0xFF weighted 1, 2, 4 … 128 in each group of eight and
    // summed pairwise three times: byte j the bits of lanes 8j … 8j + 7
    inline uint64_t nonzero_mask_vector(const int16_t* z) noexcept {
        static constexpr uint8_t Weights[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
        const uint8x16_t w = vld1q_u8(Weights);
        uint8x16_t q[4];
        for (int k = 0; k < 4; ++k) {
            const uint16x8_t lo = vtstq_s16(vld1q_s16(z + 16 * k), vld1q_s16(z + 16 * k));
            const uint16x8_t hi = vtstq_s16(vld1q_s16(z + 16 * k + 8), vld1q_s16(z + 16 * k + 8));
            q[k] = vandq_u8(vcombine_u8(vmovn_u16(lo), vmovn_u16(hi)), w);
        }
        const uint8x16_t s = vpaddq_u8(vpaddq_u8(q[0], q[1]), vpaddq_u8(q[2], q[3]));
        return vgetq_lane_u64(vreinterpretq_u64_u8(vpaddq_u8(s, s)), 0);
    }
#elif defined(SGCL_CODEC_SSE2)
    // Sixteen coefficients to bytes (saturated: zero stays zero), compared
    // with zero, their signs gathered
    inline uint64_t nonzero_mask_vector(const int16_t* z) noexcept {
        const __m128i zero = _mm_setzero_si128();
        uint64_t m = 0;
        for (int k = 0; k < 4; ++k) {
            const __m128i b = _mm_packs_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(z + 16 * k)),
                                              _mm_loadu_si128(reinterpret_cast<const __m128i*>(z + 16 * k + 8)));
            m |= uint64_t(uint16_t(~_mm_movemask_epi8(_mm_cmpeq_epi8(b, zero)))) << (16 * k);
        }
        return m;
    }
#endif

    inline uint64_t nonzero_mask(const int16_t* z) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        return nonzero_mask_vector(z);
#else
        return nonzero_mask_plain(z);
#endif
    }

    // A block made ready for the entropy coder (F.1.2): the coefficients in
    // zigzag order, each one's category (the length of its magnitude) and
    // its value bits (the value if positive, less one if negative, in the
    // category's low bits; F.1.2.1), and the mask of the nonzero ones. The
    // coder then only reads: a run is the distance between two set bits.
    struct CodedBlock {
        int16_t z[64];
        uint16_t bits[64];
        uint8_t size[64];
        uint64_t mask;
    };

    inline void coded_block_plain(const int16_t* b, CodedBlock& c) noexcept {
        for (int i = 0; i < 64; ++i) {
            const int v = b[ZigZag[i]];
            const int sign = v >> 31;
            const unsigned size = unsigned(std::bit_width(unsigned((v ^ sign) - sign)));
            c.z[i] = int16_t(v);
            c.size[i] = uint8_t(size);
            c.bits[i] = uint16_t(uint32_t(v + sign) & ((1u << size) - 1));
        }
        c.mask = nonzero_mask_plain(c.z);
    }

#if defined(SGCL_CODEC_NEON)
    namespace coded {
        // The byte indices of TBL for each output vector j of the zigzag
        // order: from the block's first 64 bytes (lo) or its last 64 (hi),
        // 0xFF (a zero from TBL) where the byte is in the other half
        struct ZigZagBytes {
            uint8_t lo[128];
            uint8_t hi[128];
        };

        constexpr ZigZagBytes zigzag_bytes() noexcept {
            ZigZagBytes t{};
            for (unsigned k = 0; k < 64; ++k) {
                for (unsigned byte = 0; byte < 2; ++byte) {
                    const unsigned src = 2u * ZigZag[k] + byte, dst = 2 * k + byte;
                    t.lo[dst] = src < 64 ? uint8_t(src) : uint8_t(0xFF);
                    t.hi[dst] = src < 64 ? uint8_t(0xFF) : uint8_t(src - 64);
                }
            }
            return t;
        }

        inline constexpr ZigZagBytes Bytes = zigzag_bytes();
    }

    // Eight coefficients a vector: TBL over the block's two halves for the
    // order, then |v| (as unsigned: -32768 is 32768), 16 less its leading
    // zeros for the category, v + sign masked for the bits, as the plain road
    inline void coded_block_vector(const int16_t* b, CodedBlock& c) noexcept {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(b);
        const uint8x16x4_t lo = {{vld1q_u8(bytes), vld1q_u8(bytes + 16), vld1q_u8(bytes + 32), vld1q_u8(bytes + 48)}};
        const uint8x16x4_t hi = {{vld1q_u8(bytes + 64), vld1q_u8(bytes + 80), vld1q_u8(bytes + 96), vld1q_u8(bytes + 112)}};
        const uint16x8_t one = vdupq_n_u16(1), sixteen = vdupq_n_u16(16);
        for (int j = 0; j < 8; ++j) {
            const uint8x16_t v8 = vorrq_u8(vqtbl4q_u8(lo, vld1q_u8(coded::Bytes.lo + 16 * j)), vqtbl4q_u8(hi, vld1q_u8(coded::Bytes.hi + 16 * j)));
            const int16x8_t z = vreinterpretq_s16_u8(v8);
            const int16x8_t sign = vshrq_n_s16(z, 15);
            const uint16x8_t magnitude = vreinterpretq_u16_s16(vabsq_s16(z));
            const uint16x8_t size = vsubq_u16(sixteen, vclzq_u16(magnitude));
            const uint16x8_t low = vsubq_u16(vshlq_u16(one, vreinterpretq_s16_u16(size)), one);
            vst1q_s16(c.z + 8 * j, z);
            vst1q_u16(c.bits + 8 * j, vandq_u16(vreinterpretq_u16_s16(vaddq_s16(z, sign)), low));
            vst1_u8(c.size + 8 * j, vmovn_u16(size));
        }
        c.mask = nonzero_mask_vector(c.z);
    }
#elif defined(SGCL_CODEC_SSE2)
    // SSE2 has no byte shuffle or leading-zero count: the plain order and
    // categories, the mask by the vector road
    inline void coded_block_vector(const int16_t* b, CodedBlock& c) noexcept {
        for (int i = 0; i < 64; ++i) {
            const int v = b[ZigZag[i]];
            const int sign = v >> 31;
            const unsigned size = unsigned(std::bit_width(unsigned((v ^ sign) - sign)));
            c.z[i] = int16_t(v);
            c.size[i] = uint8_t(size);
            c.bits[i] = uint16_t(uint32_t(v + sign) & ((1u << size) - 1));
        }
        c.mask = nonzero_mask_vector(c.z);
    }
#endif

    inline void coded_block(const int16_t* b, CodedBlock& c) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        coded_block_vector(b, c);
#else
        coded_block_plain(b, c);
#endif
    }
}
