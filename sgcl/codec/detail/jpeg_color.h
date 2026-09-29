//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "simd.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <cstdint>

namespace sgcl::codec::detail {
    // Upsampling of a component stored at a fraction of the image's
    // resolution, one output row at a time. T.81 leaves it to the
    // application; these are the filters libjpeg-turbo applies by default
    // ("fancy"), so that the pixels are djpeg's bit for bit:
    //   2× across (4:2:2): each sample becomes two, 3/4 of itself and 1/4
    //     of its neighbor on that side, rounded alternately down and up;
    //   2× down (4:4:0): each row becomes two, 3/4 of itself and 1/4 of
    //     the row above (the upper) or below (the lower);
    //   2× both (4:2:0): the two combined, the sums kept at full precision
    //     (a triangle filter: 9/16, 3/16, 3/16, 1/16);
    //   any other factor, or a component 2 samples wide or less: each
    //     sample repeated.
    // The row above the first and below the last are the edge rows again;
    // the first and the last column take their edge value.
    namespace upsample {
        // 2× across: in (n samples) to out (2n). The plain road; h2()
        // below takes the vector one for the inner samples where there is one
        inline void h2_plain(const uint8_t* in, uint8_t* out, size_t n) noexcept {
            out[0] = in[0];
            out[1] = uint8_t((in[0] * 3 + in[1] + 2) >> 2);
            for (size_t i = 1; i + 1 < n; ++i) {
                const int v = in[i] * 3;
                out[2 * i] = uint8_t((v + in[i - 1] + 1) >> 2);
                out[2 * i + 1] = uint8_t((v + in[i + 1] + 2) >> 2);
            }
            const size_t l = n - 1;
            out[2 * l] = uint8_t((in[l] * 3 + in[l - 1] + 1) >> 2);
            out[2 * l + 1] = in[l];
        }

        // 2× down: the row and its neighbor (above for the upper output
        // row, below for the lower) to one output row
        inline void v2_plain(const uint8_t* row, const uint8_t* near, bool lower, uint8_t* out, size_t n) noexcept {
            const int bias = lower ? 2 : 1;
            for (size_t i = 0; i < n; ++i) {
                out[i] = uint8_t((row[i] * 3 + near[i] + bias) >> 2);
            }
        }

        // 2× both: one output row of the pair
        inline void h2v2_plain(const uint8_t* row, const uint8_t* near, uint8_t* out, size_t n) noexcept {
            auto col = [&](size_t i) { return int(row[i]) * 3 + near[i]; };
            int here = col(0), next = col(1), last;
            out[0] = uint8_t((here * 4 + 8) >> 4);
            out[1] = uint8_t((here * 3 + next + 7) >> 4);
            last = here;
            here = next;
            for (size_t i = 1; i + 1 < n; ++i) {
                next = col(i + 1);
                out[2 * i] = uint8_t((here * 3 + last + 8) >> 4);
                out[2 * i + 1] = uint8_t((here * 3 + next + 7) >> 4);
                last = here;
                here = next;
            }
            const size_t l = n - 1;
            out[2 * l] = uint8_t((here * 3 + last + 8) >> 4);
            out[2 * l + 1] = uint8_t((here * 4 + 7) >> 4);
        }

        // The vector road of h2v2: eight input samples a step, 16-bit sums
        // (the largest 3·(3·255 + 255) + 1020 + 8 = 4088), the same formulas
        // as the plain one; it returns the first sample it left, the plain
        // road writing that one and those after it. The edge samples stay
        // with the plain road (their neighbour is themselves).
#if defined(SGCL_CODEC_NEON)
        // 2× both, inner samples: col(i) = 3 row[i] + near[i]
        inline size_t h2v2_vector(const uint8_t* row, const uint8_t* near, uint8_t* out, size_t n) noexcept {
            auto col = [&](size_t at) { return vmlal_u8(vmovl_u8(vld1_u8(near + at)), vld1_u8(row + at), vdup_n_u8(3)); };
            size_t i = 1;
            for (; i + 9 <= n; i += 8) {
                const uint16x8_t here3 = vmulq_n_u16(col(i), 3);
                const uint8x8_t even = vshrn_n_u16(vaddq_u16(vaddq_u16(here3, col(i - 1)), vdupq_n_u16(8)), 4);
                const uint8x8_t odd = vshrn_n_u16(vaddq_u16(vaddq_u16(here3, col(i + 1)), vdupq_n_u16(7)), 4);
                const uint8x8x2_t pairs = {{even, odd}};
                vst2_u8(out + 2 * i, pairs);
            }
            return i;
        }
#elif defined(SGCL_CODEC_SSE2)
        inline __m128i load8(const uint8_t* p) noexcept {
            return _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(p)), _mm_setzero_si128());
        }

        // even and odd outputs of eight samples interleaved and stored
        inline void store_pairs(uint8_t* out, __m128i even, __m128i odd) noexcept {
            const __m128i e = _mm_packus_epi16(even, even), o = _mm_packus_epi16(odd, odd);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out), _mm_unpacklo_epi8(e, o));
        }

        inline size_t h2v2_vector(const uint8_t* row, const uint8_t* near, uint8_t* out, size_t n) noexcept {
            auto col = [&](size_t at) {
                const __m128i r = load8(row + at);
                return _mm_add_epi16(_mm_add_epi16(_mm_add_epi16(r, r), r), load8(near + at));
            };
            size_t i = 1;
            for (; i + 9 <= n; i += 8) {
                const __m128i h = col(i);
                const __m128i here3 = _mm_add_epi16(_mm_add_epi16(h, h), h);
                const __m128i even = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(here3, col(i - 1)), _mm_set1_epi16(8)), 4);
                const __m128i odd = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(here3, col(i + 1)), _mm_set1_epi16(7)), 4);
                store_pairs(out + 2 * i, even, odd);
            }
            return i;
        }
#endif

        // 2× across and 2× down: the plain roads, which the compiler turns
        // into vector code itself (hand-written vectors measured slower)
        inline void h2(const uint8_t* in, uint8_t* out, size_t n) noexcept {
            h2_plain(in, out, n);
        }

        inline void v2(const uint8_t* row, const uint8_t* near, bool lower, uint8_t* out, size_t n) noexcept {
            v2_plain(row, near, lower, out, n);
        }

        inline void h2v2(const uint8_t* row, const uint8_t* near, uint8_t* out, size_t n) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
            if (n > 2) {
                auto col = [&](size_t i) { return int(row[i]) * 3 + near[i]; };
                out[0] = uint8_t((col(0) * 4 + 8) >> 4);
                out[1] = uint8_t((col(0) * 3 + col(1) + 7) >> 4);
                size_t i = h2v2_vector(row, near, out, n);
                for (; i + 1 < n; ++i) {
                    const int here = col(i);
                    out[2 * i] = uint8_t((here * 3 + col(i - 1) + 8) >> 4);
                    out[2 * i + 1] = uint8_t((here * 3 + col(i + 1) + 7) >> 4);
                }
                const size_t l = n - 1;
                out[2 * l] = uint8_t((col(l) * 3 + col(l - 1) + 8) >> 4);
                out[2 * l + 1] = uint8_t((col(l) * 4 + 7) >> 4);
                return;
            }
#endif
            h2v2_plain(row, near, out, n);
        }

        // Each sample repeated h times
        inline void box(const uint8_t* in, uint8_t* out, size_t n, unsigned h) noexcept {
            for (size_t i = 0; i < n; ++i) {
                for (unsigned k = 0; k < h; ++k) {
                    out[i * h + k] = in[i];
                }
            }
        }
    }

    // YCbCr to RGB (JFIF 1.02, T.871): R = Y + 1.402 (Cr - 128), G = Y -
    // 0.34414 (Cb - 128) - 0.71414 (Cr - 128), B = Y + 1.772 (Cb - 128),
    // each clamped to 0..255; in 16-bit fixed point, the red and blue terms
    // rounded on their own and green's two summed before its one rounding
    // (floor after adding a half), as libjpeg computes them, so that the
    // pixels are its
    struct YccTables {
        std::array<int, 256> cr_r, cb_b, cr_g, cb_g;

        static constexpr int64_t fix(double x) noexcept {
            return int64_t(x * 65536 + 0.5);
        }

        YccTables() noexcept {
            constexpr int64_t half = int64_t(1) << 15;
            for (int i = 0; i < 256; ++i) {
                const int64_t x = i - 128;
                cr_r[i] = int((fix(1.40200) * x + half) >> 16);
                cb_b[i] = int((fix(1.77200) * x + half) >> 16);
                cr_g[i] = int(-fix(0.71414) * x);
                cb_g[i] = int(-fix(0.34414) * x + half);
            }
        }
    };

    inline const YccTables& ycc_tables() noexcept {
        static const YccTables t;
        return t;
    }

    inline uint8_t clamp255(int v) noexcept {
        return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v);
    }

    // The plain road, through the tables
    inline void ycc_to_rgb_plain(const uint8_t* y, const uint8_t* cb, const uint8_t* cr, uint8_t* out, size_t n) noexcept {
        const auto& t = ycc_tables();
        for (size_t i = 0; i < n; ++i) {
            const int Y = y[i];
            out[3 * i] = clamp255(Y + t.cr_r[cr[i]]);
            out[3 * i + 1] = clamp255(Y + ((t.cb_g[cb[i]] + t.cr_g[cr[i]]) >> 16));
            out[3 * i + 2] = clamp255(Y + t.cb_b[cb[i]]);
        }
    }

#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    // The vector road: the tables' terms computed, with constants that fit
    // 16 bits. With x = Cr - 128 or Cb - 128 (-128..127) and the whole
    // multiple of 65536 taken out of each constant, the floor of a sum
    // with it is unchanged:
    //   red   (91881 x + 32768) >> 16            = x + ((26345 x + 32768) >> 16)
    //   blue  (116130 x + 32768) >> 16           = 2x + ((-14942 x + 32768) >> 16)
    //   green (-22554 xb - 46802 xr + 32768) >> 16 = -xr + ((-22554 xb + 18734 xr + 32768) >> 16)
    // (91881 = fix(1.402), 116130 = fix(1.772), 22554 = fix(0.34414),
    // 46802 = fix(0.71414)); the sums with Y clamped to 0..255 by the
    // narrowing
    namespace ycc {
        inline constexpr int16_t Red = 26345, Blue = -14942, GreenB = -22554, GreenR = 18734;
    }
#endif

#if defined(SGCL_CODEC_NEON)
    inline size_t ycc_to_rgb_vector(const uint8_t* y, const uint8_t* cb, const uint8_t* cr, uint8_t* out, size_t n) noexcept {
        const int32x4_t half = vdupq_n_s32(32768);
        const int16x8_t centre = vdupq_n_s16(128);
        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            const int16x8_t Y = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(y + i)));
            const int16x8_t xb = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vld1_u8(cb + i))), centre);
            const int16x8_t xr = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vld1_u8(cr + i))), centre);
            auto term = [&](int32x4_t lo, int32x4_t hi) {
                return vcombine_s16(vshrn_n_s32(vaddq_s32(lo, half), 16), vshrn_n_s32(vaddq_s32(hi, half), 16));
            };
            const int16x8_t red = vaddq_s16(xr, term(vmull_n_s16(vget_low_s16(xr), ycc::Red), vmull_n_s16(vget_high_s16(xr), ycc::Red)));
            const int16x8_t blue = vaddq_s16(vaddq_s16(xb, xb), term(vmull_n_s16(vget_low_s16(xb), ycc::Blue), vmull_n_s16(vget_high_s16(xb), ycc::Blue)));
            const int16x8_t green = vsubq_s16(term(vmlal_n_s16(vmull_n_s16(vget_low_s16(xb), ycc::GreenB), vget_low_s16(xr), ycc::GreenR),
                                                   vmlal_n_s16(vmull_n_s16(vget_high_s16(xb), ycc::GreenB), vget_high_s16(xr), ycc::GreenR)),
                                              xr);
            uint8x8x3_t rgb;
            rgb.val[0] = vqmovun_s16(vaddq_s16(Y, red));
            rgb.val[1] = vqmovun_s16(vaddq_s16(Y, green));
            rgb.val[2] = vqmovun_s16(vaddq_s16(Y, blue));
            vst3_u8(out + 3 * i, rgb);
        }
        return i;
    }
#elif defined(SGCL_CODEC_SSE2)
    namespace ycc {
        // x (16 bits) times k, plus 32768, shifted down 16: the 32-bit
        // products from the low and high halves of the 16-bit ones
        inline __m128i term(__m128i x, int16_t k) noexcept {
            const __m128i m = _mm_set1_epi16(k);
            const __m128i lo16 = _mm_mullo_epi16(x, m), hi16 = _mm_mulhi_epi16(x, m);
            const __m128i half = _mm_set1_epi32(32768);
            const __m128i lo = _mm_srai_epi32(_mm_add_epi32(_mm_unpacklo_epi16(lo16, hi16), half), 16);
            const __m128i hi = _mm_srai_epi32(_mm_add_epi32(_mm_unpackhi_epi16(lo16, hi16), half), 16);
            return _mm_packs_epi32(lo, hi);
        }

        // xb·kb + xr·kr, plus 32768, shifted down 16: PMADDWD on the pairs
        inline __m128i term2(__m128i xb, int16_t kb, __m128i xr, int16_t kr) noexcept {
            const __m128i k = _mm_set1_epi32(int(uint16_t(kb)) | int(uint32_t(uint16_t(kr)) << 16));
            const __m128i half = _mm_set1_epi32(32768);
            const __m128i lo = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(xb, xr), k), half), 16);
            const __m128i hi = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(xb, xr), k), half), 16);
            return _mm_packs_epi32(lo, hi);
        }
    }

    // Eight pixels a step; the three bytes of each written as a 32-bit word
    // at 3i, its fourth byte the next pixel's first (written over by it), so
    // the last pixel of the row is left to the plain road
    inline size_t ycc_to_rgb_vector(const uint8_t* y, const uint8_t* cb, const uint8_t* cr, uint8_t* out, size_t n) noexcept {
        const __m128i zero = _mm_setzero_si128();
        const __m128i centre = _mm_set1_epi16(128);
        size_t i = 0;
        for (; i + 9 <= n; i += 8) {
            const __m128i Y = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(y + i)), zero);
            const __m128i xb = _mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(cb + i)), zero), centre);
            const __m128i xr = _mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(cr + i)), zero), centre);
            const __m128i red = _mm_add_epi16(xr, ycc::term(xr, ycc::Red));
            const __m128i blue = _mm_add_epi16(_mm_add_epi16(xb, xb), ycc::term(xb, ycc::Blue));
            const __m128i green = _mm_sub_epi16(ycc::term2(xb, ycc::GreenB, xr, ycc::GreenR), xr);
            const __m128i r = _mm_packus_epi16(_mm_add_epi16(Y, red), zero);
            const __m128i g = _mm_packus_epi16(_mm_add_epi16(Y, green), zero);
            const __m128i b = _mm_packus_epi16(_mm_add_epi16(Y, blue), zero);
            // words R G B 0 for the eight pixels
            const __m128i rg = _mm_unpacklo_epi8(r, g), b0 = _mm_unpacklo_epi8(b, zero);
            const __m128i w0 = _mm_unpacklo_epi16(rg, b0), w1 = _mm_unpackhi_epi16(rg, b0);
            uint32_t words[8];
            _mm_storeu_si128(reinterpret_cast<__m128i*>(words), w0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(words + 4), w1);
            for (int k = 0; k < 8; ++k) {
                std::memcpy(out + 3 * (i + size_t(k)), &words[k], 4);
            }
        }
        return i;
    }
#endif

    inline void ycc_to_rgb(const uint8_t* y, const uint8_t* cb, const uint8_t* cr, uint8_t* out, size_t n) noexcept {
        size_t done = 0;
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        done = ycc_to_rgb_vector(y, cb, cr, out, n);
#endif
        ycc_to_rgb_plain(y + done, cb + done, cr + done, out + 3 * done, n - done);
    }

    // RGB to YCbCr (JFIF 1.02, T.871), the encoder's, in 16-bit fixed
    // point: Y the weighted sum rounded, Cb and Cr centered on 128 and
    // rounded a hair below the half (the sum never reaches 256), as
    // libjpeg's, so that the files are cjpeg's:
    //   Y  = (19595 R + 38470 G +  7471 B + 32768) >> 16
    //   Cb = (-11059 R - 21709 G + 32768 B + 128·65536 + 32767) >> 16
    //   Cr = (32768 R - 27439 G -  5329 B + 128·65536 + 32767) >> 16
    // (19595 = fix(0.299) and so on; each row of weights sums to 65536 or
    // to 0, so every sum lies within 0..2^24 and every result in 0..255).
    namespace rgb_ycc {
        constexpr int64_t fix(double x) noexcept {
            return int64_t(x * 65536 + 0.5);
        }

        inline constexpr int64_t YR = fix(0.29900), YG = fix(0.58700), YB = fix(0.11400);
        inline constexpr int64_t CbR = fix(0.16874), CbG = fix(0.33126), Half = fix(0.50000);
        inline constexpr int64_t CrG = fix(0.41869), CrB = fix(0.08131);
        inline constexpr int64_t Round = int64_t(1) << 15;
        inline constexpr int64_t Centre = (int64_t(128) << 16) + Round - 1;
        static_assert(YR == 19595 && YG == 38470 && YB == 7471 && CbR == 11059 && CbG == 21709 && Half == 32768 && CrG == 27439 && CrB == 5329);
    }

    // The plain road
    inline void rgb_to_ycc_plain(const uint8_t* rgb, uint8_t* y, uint8_t* cb, uint8_t* cr, size_t n) noexcept {
        using namespace rgb_ycc;
        for (size_t x = 0; x < n; ++x) {
            const int64_t R = rgb[3 * x], G = rgb[3 * x + 1], B = rgb[3 * x + 2];
            y[x] = uint8_t((YR * R + YG * G + YB * B + Round) >> 16);
            cb[x] = uint8_t((-CbR * R - CbG * G + Half * B + Centre) >> 16);
            cr[x] = uint8_t((Half * R - CrG * G - CrB * B + Centre) >> 16);
        }
    }

#if defined(SGCL_CODEC_NEON)
    namespace rgb_ycc {
        // Four pixels' Y, Cb and Cr on 32 bits unsigned (Cb's and Cr's
        // negative terms wrap and come back, the result being within range)
        struct Quarter {
            uint16x4_t y, cb, cr;
        };

        inline Quarter quarter(uint16x4_t r, uint16x4_t g, uint16x4_t b, uint32x4_t centre) noexcept {
            return {vrshrn_n_u32(vmlal_n_u16(vmlal_n_u16(vmull_n_u16(r, uint16_t(YR)), g, uint16_t(YG)), b, uint16_t(YB)), 16),
                    vshrn_n_u32(vmlsl_n_u16(vmlsl_n_u16(vmlal_n_u16(centre, b, uint16_t(Half)), r, uint16_t(CbR)), g, uint16_t(CbG)), 16),
                    vshrn_n_u32(vmlsl_n_u16(vmlsl_n_u16(vmlal_n_u16(centre, r, uint16_t(Half)), g, uint16_t(CrG)), b, uint16_t(CrB)), 16)};
        }
    }

    // Sixteen pixels a step (VLD3Q, four quarters, one 16-byte store a
    // plane), then eight, the sums as quarter() has them
    inline size_t rgb_to_ycc_vector(const uint8_t* rgb, uint8_t* y, uint8_t* cb, uint8_t* cr, size_t n) noexcept {
        using namespace rgb_ycc;
        const uint32x4_t centre = vdupq_n_u32(uint32_t(Centre));
        size_t x = 0;
        for (; x + 16 <= n; x += 16) {
            const uint8x16x3_t p = vld3q_u8(rgb + 3 * x);
            const uint16x8_t R0 = vmovl_u8(vget_low_u8(p.val[0])), R1 = vmovl_u8(vget_high_u8(p.val[0]));
            const uint16x8_t G0 = vmovl_u8(vget_low_u8(p.val[1])), G1 = vmovl_u8(vget_high_u8(p.val[1]));
            const uint16x8_t B0 = vmovl_u8(vget_low_u8(p.val[2])), B1 = vmovl_u8(vget_high_u8(p.val[2]));
            const Quarter q0 = quarter(vget_low_u16(R0), vget_low_u16(G0), vget_low_u16(B0), centre);
            const Quarter q1 = quarter(vget_high_u16(R0), vget_high_u16(G0), vget_high_u16(B0), centre);
            const Quarter q2 = quarter(vget_low_u16(R1), vget_low_u16(G1), vget_low_u16(B1), centre);
            const Quarter q3 = quarter(vget_high_u16(R1), vget_high_u16(G1), vget_high_u16(B1), centre);
            vst1q_u8(y + x, vcombine_u8(vmovn_u16(vcombine_u16(q0.y, q1.y)), vmovn_u16(vcombine_u16(q2.y, q3.y))));
            vst1q_u8(cb + x, vcombine_u8(vmovn_u16(vcombine_u16(q0.cb, q1.cb)), vmovn_u16(vcombine_u16(q2.cb, q3.cb))));
            vst1q_u8(cr + x, vcombine_u8(vmovn_u16(vcombine_u16(q0.cr, q1.cr)), vmovn_u16(vcombine_u16(q2.cr, q3.cr))));
        }
        for (; x + 8 <= n; x += 8) {
            const uint8x8x3_t p = vld3_u8(rgb + 3 * x);
            const uint16x8_t R = vmovl_u8(p.val[0]), G = vmovl_u8(p.val[1]), B = vmovl_u8(p.val[2]);
            uint16x4_t ys[2], cbs[2], crs[2];
            for (int h = 0; h < 2; ++h) {
                auto half = [&](uint16x8_t v) { return h ? vget_high_u16(v) : vget_low_u16(v); };
                const uint16x4_t r = half(R), g = half(G), b = half(B);
                ys[h] = vrshrn_n_u32(vmlal_n_u16(vmlal_n_u16(vmull_n_u16(r, uint16_t(YR)), g, uint16_t(YG)), b, uint16_t(YB)), 16);
                cbs[h] = vshrn_n_u32(vmlsl_n_u16(vmlsl_n_u16(vmlal_n_u16(centre, b, uint16_t(Half)), r, uint16_t(CbR)), g, uint16_t(CbG)), 16);
                crs[h] = vshrn_n_u32(vmlsl_n_u16(vmlsl_n_u16(vmlal_n_u16(centre, r, uint16_t(Half)), g, uint16_t(CrG)), b, uint16_t(CrB)), 16);
            }
            vst1_u8(y + x, vmovn_u16(vcombine_u16(ys[0], ys[1])));
            vst1_u8(cb + x, vmovn_u16(vcombine_u16(cbs[0], cbs[1])));
            vst1_u8(cr + x, vmovn_u16(vcombine_u16(crs[0], crs[1])));
        }
        return x;
    }
#elif defined(SGCL_CODEC_SSE2)
    namespace rgb_ycc {
        // Four pixels from 16 bytes as 32-bit lanes R | G << 8 | B << 16
        // (the fourth byte the next pixel's)
        inline __m128i pixels4(const uint8_t* p) noexcept {
            const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
            const __m128i a = _mm_unpacklo_epi32(v, _mm_srli_si128(v, 3));
            const __m128i b = _mm_unpacklo_epi32(_mm_srli_si128(v, 6), _mm_srli_si128(v, 9));
            return _mm_unpacklo_epi64(a, b);
        }

        inline __m128i pair(int16_t a, int16_t b) noexcept {
            return _mm_set1_epi32(int(uint16_t(a)) | int(uint32_t(uint16_t(b)) << 16));
        }

        // x·a + y·b + z·c + w·d on the pairs (x, y) and (z, w), plus the
        // constant, shifted down 16, eight lanes of 16 bits
        inline __m128i sum(__m128i x, __m128i y, int16_t a, int16_t b, __m128i z, __m128i w, int16_t c, int16_t d, int32_t k) noexcept {
            const __m128i kk = _mm_set1_epi32(k);
            const __m128i lo = _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(x, y), pair(a, b)),
                                                           _mm_madd_epi16(_mm_unpacklo_epi16(z, w), pair(c, d))),
                                             kk);
            const __m128i hi = _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(x, y), pair(a, b)),
                                                           _mm_madd_epi16(_mm_unpackhi_epi16(z, w), pair(c, d))),
                                             kk);
            return _mm_packs_epi32(_mm_srli_epi32(lo, 16), _mm_srli_epi32(hi, 16));
        }
    }

    namespace rgb_ycc {
        struct Eight {
            __m128i y, cb, cr;
        };

        // Eight pixels from 3·8 bytes (the loads read 28), PMADDWD on
        // pairs; a weight past 16 bits signed split in two halves on the
        // same input (38470 G = 19235 G + 19235 G, 32768 B = 16384 B +
        // 16384 B); 16-bit lanes
        inline Eight eight(const uint8_t* p) noexcept {
            const __m128i mask = _mm_set1_epi32(0xFF);
            const __m128i d0 = pixels4(p), d1 = pixels4(p + 12);
            const __m128i R = _mm_packs_epi32(_mm_and_si128(d0, mask), _mm_and_si128(d1, mask));
            const __m128i G = _mm_packs_epi32(_mm_and_si128(_mm_srli_epi32(d0, 8), mask), _mm_and_si128(_mm_srli_epi32(d1, 8), mask));
            const __m128i B = _mm_packs_epi32(_mm_and_si128(_mm_srli_epi32(d0, 16), mask), _mm_and_si128(_mm_srli_epi32(d1, 16), mask));
            constexpr int16_t G2 = int16_t(YG / 2), H2 = int16_t(Half / 2);
            static_assert(G2 * 2 == YG && H2 * 2 == Half);
            return {sum(R, G, int16_t(YR), G2, B, G, int16_t(YB), G2, int32_t(Round)),
                    sum(R, B, int16_t(-CbR), H2, G, B, int16_t(-CbG), H2, int32_t(Centre)),
                    sum(G, R, int16_t(-CrG), H2, B, R, int16_t(-CrB), H2, int32_t(Centre))};
        }
    }

    // Sixteen pixels a step (two groups of eight, one 16-byte store a
    // plane), then eight; the loads read 28 bytes from a group's first
    // pixel: the steps stop where that passes the row
    inline size_t rgb_to_ycc_vector(const uint8_t* rgb, uint8_t* y, uint8_t* cb, uint8_t* cr, size_t n) noexcept {
        using namespace rgb_ycc;
        size_t x = 0;
        for (; 3 * x + 24 + 28 <= 3 * n; x += 16) {
            const Eight a = eight(rgb + 3 * x), b = eight(rgb + 3 * x + 24);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(y + x), _mm_packus_epi16(a.y, b.y));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(cb + x), _mm_packus_epi16(a.cb, b.cb));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(cr + x), _mm_packus_epi16(a.cr, b.cr));
        }
        for (; 3 * x + 28 <= 3 * n; x += 8) {
            const Eight a = eight(rgb + 3 * x);
            const __m128i zero = _mm_setzero_si128();
            _mm_storel_epi64(reinterpret_cast<__m128i*>(y + x), _mm_packus_epi16(a.y, zero));
            _mm_storel_epi64(reinterpret_cast<__m128i*>(cb + x), _mm_packus_epi16(a.cb, zero));
            _mm_storel_epi64(reinterpret_cast<__m128i*>(cr + x), _mm_packus_epi16(a.cr, zero));
        }
        return x;
    }
#endif

    // rgb: n pixels of three bytes; y, cb, cr: n samples each
    inline void rgb_to_ycc(const uint8_t* rgb, uint8_t* y, uint8_t* cb, uint8_t* cr, size_t n) noexcept {
        size_t done = 0;
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        done = rgb_to_ycc_vector(rgb, y, cb, cr, n);
#endif
        rgb_to_ycc_plain(rgb + 3 * done, y + done, cb + done, cr + done, n - done);
    }

    // Subsampling of a component for the encoder, as libjpeg's: the
    // average of two samples across (4:2:2), rounded 0, 1, 0, 1… along the
    // row, or of four (4:2:0), rounded 1, 2, 1, 2…; out[x] from in[2x] and
    // in[2x + 1] (and the row below's), n outputs.
    namespace downsample {
        inline void h2v1_plain(const uint8_t* a, uint8_t* out, size_t n) noexcept {
            int bias = 0;
            for (size_t x = 0; x < n; ++x) {
                out[x] = uint8_t((a[2 * x] + a[2 * x + 1] + bias) >> 1);
                bias ^= 1;
            }
        }

        inline void h2v2_plain(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n) noexcept {
            int bias = 1;
            for (size_t x = 0; x < n; ++x) {
                out[x] = uint8_t((a[2 * x] + a[2 * x + 1] + b[2 * x] + b[2 * x + 1] + bias) >> 2);
                bias ^= 3;
            }
        }

        // Eight outputs a step, from an even one: the biases alternate
        // within the vector as along the row
#if defined(SGCL_CODEC_NEON)
        inline size_t h2v1_vector(const uint8_t* a, uint8_t* out, size_t n) noexcept {
            const uint16x8_t bias = vreinterpretq_u16_u32(vdupq_n_u32(0x00010000));
            size_t x = 0;
            for (; x + 8 <= n; x += 8) {
                vst1_u8(out + x, vshrn_n_u16(vaddq_u16(vpaddlq_u8(vld1q_u8(a + 2 * x)), bias), 1));
            }
            return x;
        }

        inline size_t h2v2_vector(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n) noexcept {
            const uint16x8_t bias = vreinterpretq_u16_u32(vdupq_n_u32(0x00020001));
            size_t x = 0;
            for (; x + 8 <= n; x += 8) {
                const uint16x8_t s = vpadalq_u8(vpaddlq_u8(vld1q_u8(a + 2 * x)), vld1q_u8(b + 2 * x));
                vst1_u8(out + x, vshrn_n_u16(vaddq_u16(s, bias), 2));
            }
            return x;
        }
#elif defined(SGCL_CODEC_SSE2)
        // the sums of the byte pairs of 16 bytes, eight lanes of 16 bits
        inline __m128i pairs(const uint8_t* p) noexcept {
            const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
            return _mm_add_epi16(_mm_and_si128(v, _mm_set1_epi16(0xFF)), _mm_srli_epi16(v, 8));
        }

        inline size_t h2v1_vector(const uint8_t* a, uint8_t* out, size_t n) noexcept {
            const __m128i bias = _mm_set1_epi32(0x00010000);
            size_t x = 0;
            for (; x + 8 <= n; x += 8) {
                const __m128i s = _mm_srli_epi16(_mm_add_epi16(pairs(a + 2 * x), bias), 1);
                _mm_storel_epi64(reinterpret_cast<__m128i*>(out + x), _mm_packus_epi16(s, s));
            }
            return x;
        }

        inline size_t h2v2_vector(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n) noexcept {
            const __m128i bias = _mm_set1_epi32(0x00020001);
            size_t x = 0;
            for (; x + 8 <= n; x += 8) {
                const __m128i s = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(pairs(a + 2 * x), pairs(b + 2 * x)), bias), 2);
                _mm_storel_epi64(reinterpret_cast<__m128i*>(out + x), _mm_packus_epi16(s, s));
            }
            return x;
        }
#endif

        inline void h2v1(const uint8_t* a, uint8_t* out, size_t n) noexcept {
            size_t done = 0;
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
            done = h2v1_vector(a, out, n);
#endif
            // an even count done: the bias starts again at 0
            h2v1_plain(a + 2 * done, out + done, n - done);
        }

        inline void h2v2(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n) noexcept {
            size_t done = 0;
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
            done = h2v2_vector(a, b, out, n);
#endif
            h2v2_plain(a + 2 * done, b + 2 * done, out + done, n - done);
        }
    }
}
