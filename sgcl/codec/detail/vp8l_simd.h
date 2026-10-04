//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "../../core/detail/os.h"
#include "simd.h"

// The kernels of WebP lossless (VP8L) over rows of ARGB words (0xAARRGGBB):
// the inverse transforms that go over every pixel, and the words turned into
// the bytes of an image. Each has a plain loop, and a loop of 128-bit
// vectors on the codec's vector roads (simd.h: NEON, SSE2, no gate); the two
// give the same bytes, bit for bit.

namespace sgcl::codec::detail::vp8l {
    // Each channel of a and b added, modulo 256
    SGCL_INLINE_HOT uint32_t add_pixels(uint32_t a, uint32_t b) noexcept {
        const uint32_t ag = (a & 0xff00ff00u) + (b & 0xff00ff00u);
        const uint32_t rb = (a & 0x00ff00ffu) + (b & 0x00ff00ffu);
        return (ag & 0xff00ff00u) | (rb & 0x00ff00ffu);
    }

    // RFC 9649's Average2 on each channel: (a + b) / 2, rounded down
    SGCL_INLINE_HOT uint32_t average2(uint32_t a, uint32_t b) noexcept {
        return (((a ^ b) & 0xfefefefeu) >> 1) + (a & b);
    }

    // ColorTransformDelta: the 3.5 fixed-point product of two signed bytes
    SGCL_INLINE_HOT int color_delta(int8_t t, int8_t c) noexcept {
        return (int(t) * int(c)) >> 5;
    }

    // The words as RGBA bytes
    inline void argb_to_rgba(const uint32_t* src, uint8_t* dst, size_t n) noexcept {
        size_t i = 0;
#if defined(SGCL_CODEC_NEON)
        for (; i + 16 <= n; i += 16) {
            uint8x16x4_t v = vld4q_u8(reinterpret_cast<const uint8_t*>(src + i));   // b g r a planes
            uint8x16x4_t o = {{v.val[2], v.val[1], v.val[0], v.val[3]}};
            vst4q_u8(dst + i * 4, o);
        }
#elif defined(SGCL_CODEC_SSE2)
        const __m128i ag = _mm_set1_epi32(int(0xff00ff00u));
        const __m128i low = _mm_set1_epi32(0xff);
        for (; i + 4 <= n; i += 4) {
            const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
            const __m128i r = _mm_and_si128(_mm_srli_epi32(v, 16), low);
            const __m128i b = _mm_slli_epi32(_mm_and_si128(v, low), 16);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 4), _mm_or_si128(_mm_and_si128(v, ag), _mm_or_si128(r, b)));
        }
#endif
        for (; i < n; ++i) {
            const uint32_t v = src[i];
            dst[i * 4] = uint8_t(v >> 16);
            dst[i * 4 + 1] = uint8_t(v >> 8);
            dst[i * 4 + 2] = uint8_t(v);
            dst[i * 4 + 3] = uint8_t(v >> 24);
        }
    }

    // The words as RGB bytes, alpha dropped
    inline void argb_to_rgb(const uint32_t* src, uint8_t* dst, size_t n) noexcept {
        size_t i = 0;
#if defined(SGCL_CODEC_NEON)
        for (; i + 16 <= n; i += 16) {
            uint8x16x4_t v = vld4q_u8(reinterpret_cast<const uint8_t*>(src + i));
            uint8x16x3_t o = {{v.val[2], v.val[1], v.val[0]}};
            vst3q_u8(dst + i * 3, o);
        }
#endif
        for (; i < n; ++i) {
            const uint32_t v = src[i];
            dst[i * 3] = uint8_t(v >> 16);
            dst[i * 3 + 1] = uint8_t(v >> 8);
            dst[i * 3 + 2] = uint8_t(v);
        }
    }

    // The inverse of subtract green: green added to red and to blue
    inline void add_green(uint32_t* p, size_t n) noexcept {
        size_t i = 0;
#if defined(SGCL_CODEC_NEON)
        const uint32x4_t low = vdupq_n_u32(0xff);
        for (; i + 4 <= n; i += 4) {
            const uint32x4_t v = vld1q_u32(p + i);
            const uint32x4_t g = vandq_u32(vshrq_n_u32(v, 8), low);
            const uint32x4_t add = vorrq_u32(g, vshlq_n_u32(g, 16));
            vst1q_u32(p + i, vreinterpretq_u32_u8(vaddq_u8(vreinterpretq_u8_u32(v), vreinterpretq_u8_u32(add))));
        }
#elif defined(SGCL_CODEC_SSE2)
        const __m128i low = _mm_set1_epi32(0xff);
        for (; i + 4 <= n; i += 4) {
            const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
            const __m128i g = _mm_and_si128(_mm_srli_epi32(v, 8), low);
            const __m128i add = _mm_or_si128(g, _mm_slli_epi32(g, 16));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(p + i), _mm_add_epi8(v, add));
        }
#endif
        for (; i < n; ++i) {
            const uint32_t v = p[i];
            const uint32_t g = (v >> 8) & 0xff;
            p[i] = add_pixels(v, g | g << 16);
        }
    }

    // The inverse color transform of n pixels under one element: the
    // element's green_to_red (its blue byte), green_to_blue (green) and
    // red_to_blue (red)
    inline void color_transform(uint32_t* p, size_t n, uint32_t element) noexcept {
        const int8_t g2r = int8_t(element & 0xff);
        const int8_t g2b = int8_t((element >> 8) & 0xff);
        const int8_t r2b = int8_t((element >> 16) & 0xff);
        size_t i = 0;
#if defined(SGCL_CODEC_NEON)
        const int8x8_t tg2r = vdup_n_s8(g2r), tg2b = vdup_n_s8(g2b), tr2b = vdup_n_s8(r2b);
        for (; i + 16 <= n; i += 16) {
            uint8x16x4_t v = vld4q_u8(reinterpret_cast<const uint8_t*>(p + i));   // b g r a
            const int8x16_t g = vreinterpretq_s8_u8(v.val[1]);
            auto delta = [](int8x16_t c, int8x8_t t) {
                const int16x8_t lo = vshrq_n_s16(vmull_s8(vget_low_s8(c), t), 5);
                const int16x8_t hi = vshrq_n_s16(vmull_s8(vget_high_s8(c), t), 5);
                return vcombine_u8(vmovn_u16(vreinterpretq_u16_s16(lo)), vmovn_u16(vreinterpretq_u16_s16(hi)));
            };
            const uint8x16_t red = vaddq_u8(v.val[2], delta(g, tg2r));
            uint8x16_t blue = vaddq_u8(v.val[0], delta(g, tg2b));
            blue = vaddq_u8(blue, delta(vreinterpretq_s8_u8(red), tr2b));
            v.val[2] = red;
            v.val[0] = blue;
            vst4q_u8(reinterpret_cast<uint8_t*>(p + i), v);
        }
#elif defined(SGCL_CODEC_SSE2)
        const __m128i low = _mm_set1_epi32(0xff);
        const __m128i ag = _mm_set1_epi32(int(0xff00ff00u));
        const __m128i tg2r = _mm_set1_epi32(g2r), tg2b = _mm_set1_epi32(g2b), tr2b = _mm_set1_epi32(r2b);
        // a signed byte of each lane's word at bit `shift`, sign-extended
        auto signed_byte = [](__m128i v, int shift) {
            return _mm_srai_epi32(_mm_sll_epi32(v, _mm_cvtsi32_si128(24 - shift)), 24);
        };
        // (t * c) >> 5 in the low 16 bits of each lane (the products fit)
        auto delta = [&](__m128i c, __m128i t) {
            return _mm_and_si128(_mm_srai_epi16(_mm_mullo_epi16(c, t), 5), low);
        };
        for (; i + 4 <= n; i += 4) {
            const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
            const __m128i g = signed_byte(v, 8);
            const __m128i red = _mm_and_si128(_mm_add_epi32(_mm_and_si128(_mm_srli_epi32(v, 16), low), delta(g, tg2r)), low);
            __m128i blue = _mm_add_epi32(_mm_and_si128(v, low), delta(g, tg2b));
            blue = _mm_and_si128(_mm_add_epi32(blue, delta(signed_byte(red, 0), tr2b)), low);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(p + i), _mm_or_si128(_mm_and_si128(v, ag), _mm_or_si128(_mm_slli_epi32(red, 16), blue)));
        }
#endif
        for (; i < n; ++i) {
            const uint32_t v = p[i];
            const int8_t green = int8_t((v >> 8) & 0xff);
            const uint32_t red = ((v >> 16) + uint32_t(color_delta(g2r, green))) & 0xff;
            uint32_t blue = (v + uint32_t(color_delta(g2b, green))) & 0xff;
            blue = (blue + uint32_t(color_delta(r2b, int8_t(red)))) & 0xff;
            p[i] = (v & 0xff00ff00u) | red << 16 | blue;
        }
    }

    // The predictors that do not read the pixel to the left, over n pixels
    // of a row (cur, its residuals turned into pixels in place) under the
    // row above (top: top[-1] is TL, top[1] TR, all inside the image):
    // 0 black, 2 T, 3 TR, 4 TL, 8 Average2(TL, T), 9 Average2(T, TR). A
    // row of these needs no pixel before it, so it goes by vectors.
    inline void predict_top(unsigned mode, uint32_t* cur, const uint32_t* top, size_t n) noexcept {
        size_t i = 0;
#if defined(SGCL_CODEC_NEON)
        for (; i + 4 <= n; i += 4) {
            const uint8x16_t t = vreinterpretq_u8_u32(vld1q_u32(top + i));
            uint8x16_t pred;
            switch (mode) {
                case 2: pred = t; break;
                case 3: pred = vreinterpretq_u8_u32(vld1q_u32(top + i + 1)); break;
                case 4: pred = vreinterpretq_u8_u32(vld1q_u32(top + i - 1)); break;
                case 8: pred = vhaddq_u8(vreinterpretq_u8_u32(vld1q_u32(top + i - 1)), t); break;
                case 9: pred = vhaddq_u8(t, vreinterpretq_u8_u32(vld1q_u32(top + i + 1))); break;
                default: pred = vreinterpretq_u8_u32(vdupq_n_u32(0xff000000u)); break;
            }
            vst1q_u32(cur + i, vreinterpretq_u32_u8(vaddq_u8(vreinterpretq_u8_u32(vld1q_u32(cur + i)), pred)));
        }
#elif defined(SGCL_CODEC_SSE2)
        const __m128i half = _mm_set1_epi8(0x7f);
        auto avg = [&](__m128i a, __m128i b) {
            return _mm_add_epi8(_mm_and_si128(a, b), _mm_and_si128(_mm_srli_epi16(_mm_xor_si128(a, b), 1), half));
        };
        auto load = [](const uint32_t* q) {
            return _mm_loadu_si128(reinterpret_cast<const __m128i*>(q));
        };
        for (; i + 4 <= n; i += 4) {
            const __m128i t = load(top + i);
            __m128i pred;
            switch (mode) {
                case 2: pred = t; break;
                case 3: pred = load(top + i + 1); break;
                case 4: pred = load(top + i - 1); break;
                case 8: pred = avg(load(top + i - 1), t); break;
                case 9: pred = avg(t, load(top + i + 1)); break;
                default: pred = _mm_set1_epi32(int(0xff000000u)); break;
            }
            _mm_storeu_si128(reinterpret_cast<__m128i*>(cur + i), _mm_add_epi8(load(cur + i), pred));
        }
#endif
        for (; i < n; ++i) {
            uint32_t pred;
            switch (mode) {
                case 2: pred = top[i]; break;
                case 3: pred = top[i + 1]; break;
                case 4: pred = top[ptrdiff_t(i) - 1]; break;
                case 8: pred = average2(top[ptrdiff_t(i) - 1], top[i]); break;
                case 9: pred = average2(top[i], top[i + 1]); break;
                default: pred = 0xff000000u; break;
            }
            cur[i] = add_pixels(cur[i], pred);
        }
    }

    // The predictors that read the pixel to the left (RFC 9649 §4.1): 1 L,
    // 5 Average2(Average2(L, TR), T), 6 Average2(L, TL), 7 Average2(L, T),
    // 10 Average2(Average2(L, TL), Average2(T, TR)), 11 Select, 12
    // ClampAddSubtractFull, 13 ClampAddSubtractHalf. Each channel on its own.
    //
    // Select picks L when Σ|T − TL| < Σ|L − TL| over the channels, else T
    // (p − L is T − TL and p − T is L − TL for p = L + T − TL): the first
    // sum is of the row above alone. ClampAddSubtractFull is clamp(L + T −
    // TL); ClampAddSubtractHalf is clamp(a + (a − TL) / 2) with a =
    // Average2(L, T), the division C's (toward zero).
    SGCL_INLINE_HOT int channel_of(uint32_t v, unsigned shift) noexcept {
        return int((v >> shift) & 0xff);
    }

    SGCL_INLINE_HOT uint32_t clamp_channel(int v) noexcept {
        return uint32_t(v < 0 ? 0 : v > 255 ? 255 : v);
    }

    inline uint32_t select_plain(uint32_t L, uint32_t T, uint32_t TL) noexcept {
        int pl = 0, pt = 0;
        for (unsigned s = 0; s < 32; s += 8) {
            const int p = channel_of(L, s) + channel_of(T, s) - channel_of(TL, s);
            pl += std::abs(p - channel_of(L, s));
            pt += std::abs(p - channel_of(T, s));
        }
        return pl < pt ? L : T;
    }

    inline uint32_t clamp_full_plain(uint32_t a, uint32_t b, uint32_t c) noexcept {
        uint32_t r = 0;
        for (unsigned s = 0; s < 32; s += 8) {
            r |= clamp_channel(channel_of(a, s) + channel_of(b, s) - channel_of(c, s)) << s;
        }
        return r;
    }

    inline uint32_t clamp_half_plain(uint32_t a, uint32_t b) noexcept {
        uint32_t r = 0;
        for (unsigned s = 0; s < 32; s += 8) {
            const int ca = channel_of(a, s);
            r |= clamp_channel(ca + (ca - channel_of(b, s)) / 2) << s;
        }
        return r;
    }

    // One pixel's prediction by mode (the plain road, the modes above)
    inline uint32_t predict_left_plain(unsigned mode, uint32_t L, uint32_t T, uint32_t TL, uint32_t TR) noexcept {
        switch (mode) {
            case 1: return L;
            case 5: return average2(average2(L, TR), T);
            case 6: return average2(L, TL);
            case 7: return average2(L, T);
            case 10: return average2(average2(L, TL), average2(T, TR));
            case 11: return select_plain(L, T, TL);
            case 12: return clamp_full_plain(L, T, TL);
            default: return clamp_half_plain(average2(L, T), TL);
        }
    }

    // n pixels of a row, cur[-1] the pixel to the left of the first and
    // top[-1 .. n] the row above (TR of the last is top[n], inside the
    // image: the caller leaves the row's last pixel, whose TR is the row's
    // first, to itself)
    inline void predict_left_run_plain(unsigned mode, uint32_t* cur, const uint32_t* top, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            cur[i] = add_pixels(cur[i], predict_left_plain(mode, cur[ptrdiff_t(i) - 1], top[i], top[ptrdiff_t(i) - 1], top[i + 1]));
        }
    }

    namespace predict_run {
#if defined(SGCL_CODEC_NEON)
        // mode 1: each pixel the sum of the residuals so far and L, per
        // channel modulo 256: a prefix sum over four pixels (shifted by
        // one pixel, then by two) plus the carry from the vector before
        inline void left(uint32_t* cur, size_t n) noexcept {
            size_t i = 0;
            uint8x16_t carry = vreinterpretq_u8_u32(vdupq_n_u32(cur[-1]));
            const uint8x16_t zero = vdupq_n_u8(0);
            for (; i + 4 <= n; i += 4) {
                uint8x16_t v = vreinterpretq_u8_u32(vld1q_u32(cur + i));
                v = vaddq_u8(v, vextq_u8(zero, v, 12));
                v = vaddq_u8(v, vextq_u8(zero, v, 8));
                v = vaddq_u8(v, carry);
                vst1q_u32(cur + i, vreinterpretq_u32_u8(v));
                carry = vreinterpretq_u8_u32(vdupq_laneq_u32(vreinterpretq_u32_u8(v), 3));
            }
            for (; i < n; ++i) {
                cur[i] = add_pixels(cur[i], cur[ptrdiff_t(i) - 1]);
            }
        }

        SGCL_INLINE_HOT uint8x8_t px(uint32_t v) noexcept {
            return vreinterpret_u8_u32(vdup_n_u32(v));
        }

        SGCL_INLINE_HOT uint32_t word(uint8x8_t v) noexcept {
            return vget_lane_u32(vreinterpret_u32_u8(v), 0);
        }

        // modes 12 and 13: one pixel at a time in the low four lanes
        template<unsigned Mode>
        inline void vector(uint32_t* cur, const uint32_t* top, size_t n) noexcept {
            uint32_t L = cur[-1];
            for (size_t i = 0; i < n; ++i) {
                const uint8x8_t T = px(top[i]), TL = px(top[ptrdiff_t(i) - 1]), l = px(L);
                uint32_t pred;
                if constexpr (Mode == 12) {
                    // T − TL wraps in 16 bits and reads back as the signed difference
                    const int16x8_t s = vreinterpretq_s16_u16(vsubl_u8(T, TL));
                    pred = word(vqmovun_s16(vaddq_s16(s, vreinterpretq_s16_u16(vmovl_u8(l)))));
                } else {
                    const int16x8_t a = vreinterpretq_s16_u16(vmovl_u8(vhadd_u8(l, T)));
                    const int16x8_t d = vsubq_s16(a, vreinterpretq_s16_u16(vmovl_u8(TL)));
                    // (a − TL) / 2 toward zero: add 1 to a negative d first
                    const int16x8_t half = vshrq_n_s16(vsubq_s16(d, vshrq_n_s16(d, 15)), 1);
                    pred = word(vqmovun_s16(vaddq_s16(a, half)));
                }
                L = add_pixels(cur[i], pred);
                cur[i] = L;
            }
        }
#elif defined(SGCL_CODEC_SSE2)
        inline void left(uint32_t* cur, size_t n) noexcept {
            size_t i = 0;
            __m128i carry = _mm_set1_epi32(int(cur[-1]));
            for (; i + 4 <= n; i += 4) {
                __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(cur + i));
                v = _mm_add_epi8(v, _mm_slli_si128(v, 4));
                v = _mm_add_epi8(v, _mm_slli_si128(v, 8));
                v = _mm_add_epi8(v, carry);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(cur + i), v);
                carry = _mm_shuffle_epi32(v, _MM_SHUFFLE(3, 3, 3, 3));
            }
            for (; i < n; ++i) {
                cur[i] = add_pixels(cur[i], cur[ptrdiff_t(i) - 1]);
            }
        }

        template<unsigned Mode>
        inline void vector(uint32_t* cur, const uint32_t* top, size_t n) noexcept {
            const __m128i zero = _mm_setzero_si128();
            uint32_t L = cur[-1];
            for (size_t i = 0; i < n; ++i) {
                const __m128i T = _mm_cvtsi32_si128(int(top[i])), TL = _mm_cvtsi32_si128(int(top[ptrdiff_t(i) - 1]));
                const __m128i l = _mm_cvtsi32_si128(int(L));
                uint32_t pred;
                if constexpr (Mode == 12) {
                    const __m128i s = _mm_sub_epi16(_mm_add_epi16(_mm_unpacklo_epi8(l, zero), _mm_unpacklo_epi8(T, zero)), _mm_unpacklo_epi8(TL, zero));
                    pred = uint32_t(_mm_cvtsi128_si32(_mm_packus_epi16(s, s)));
                } else {
                    // Average2 rounded down: PAVGB rounds up, the odd sums' bit taken back
                    const __m128i avg = _mm_sub_epi8(_mm_avg_epu8(l, T), _mm_and_si128(_mm_xor_si128(l, T), _mm_set1_epi8(1)));
                    const __m128i a = _mm_unpacklo_epi8(avg, zero);
                    const __m128i d = _mm_sub_epi16(a, _mm_unpacklo_epi8(TL, zero));
                    const __m128i half = _mm_srai_epi16(_mm_sub_epi16(d, _mm_srai_epi16(d, 15)), 1);
                    const __m128i v = _mm_add_epi16(a, half);
                    pred = uint32_t(_mm_cvtsi128_si32(_mm_packus_epi16(v, v)));
                }
                L = add_pixels(cur[i], pred);
                cur[i] = L;
            }
        }
#endif
    }

    // The dispatching entry: the vector runs where they win against the
    // plain one (ab_vp8l_modes on a photo's rows: mode 1 0.32, 12 0.67, 13
    // 0.66); Select (11, 1.16) and the averages (5, 6, 7, 10, 0.98-1.01)
    // stay with the plain run
    inline void predict_left_run(unsigned mode, uint32_t* cur, const uint32_t* top, size_t n) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        using namespace predict_run;
        switch (mode) {
            case 1: left(cur, n); return;
            case 12: vector<12>(cur, top, n); return;
            case 13: vector<13>(cur, top, n); return;
            default: break;
        }
#endif
        predict_left_run_plain(mode, cur, top, n);
    }
}
