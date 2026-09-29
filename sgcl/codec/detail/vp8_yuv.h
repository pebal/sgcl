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

// Lossy WebP's planes of Y, U and V (4:2:0) as RGB: the chroma brought to
// the luma's resolution by the "fancy" upsampling and the colors of
// ITU-R BT.601 in its limited range, in integers. Each kernel has a plain
// loop and one of 128-bit vectors (NEON, SSE2), the same bytes bit for bit.
//
// The upsampling. A chroma sample lies at the center of its 2x2 luma
// pixels, so a luma pixel has its nearest chroma sample at distance
// (1/4, 1/4), and three more at (3/4, 1/4), (1/4, 3/4), (3/4, 3/4): the
// bilinear weights are (3/4·3/4, 1/4·3/4, 3/4·1/4, 1/4·1/4) = (9, 3, 3, 1)
// / 16, rounded to the nearest: (9a + 3b + 3c + d + 8) >> 4, where a is the
// nearest, b the horizontal neighbor, c the vertical, d the diagonal. At the
// picture's edges the missing neighbor is the edge sample itself. It is
// computed as the vertical blend v = 3·near + far (exact, 16 bits), then
// (3·v + v') >> 4 with v' the horizontal neighbor's blend and 8 added: the
// same number, no rounding between.
//
// The colors. BT.601 with Kr = 0.299, Kb = 0.114, Kg = 1 − Kr − Kb = 0.587;
// limited range: Y 16..235 (219 steps), Cb, Cr 16..240 (224 steps) about 128:
//   R = 255/219 (Y − 16) + 255/112 (1 − Kr) (Cr − 128)
//   G = 255/219 (Y − 16) − 255/112 (1 − Kb) Kb/Kg (Cb − 128) − 255/112 (1 − Kr) Kr/Kg (Cr − 128)
//   B = 255/219 (Y − 16) + 255/112 (1 − Kb) (Cb − 128)
// whose factors are kY = 1.1643836, kRV = 1.5960268, kGU = 0.3917623,
// kGV = 0.8129676, kBU = 2.0172321. In fixed point of 14 bits, rounded:
// 19077, 26149, 6419, 13320, 33050. A term T(x, c) = (x·c) >> 8 is x·k in
// units of 1/64; the constant of each channel is −64·(16 kY + 128 kRV) and
// so on, rounded, plus 32 for rounding the final >> 6 to the nearest:
//   R: −14267 + 32 = −14235,  G: +8677 + 32 = +8709,  B: −17717 + 32 = −17685
// and the channel is (sum >> 6), clamped to 0..255.
//
// Two of the constants are calibrated: R −14234 and G +8708, one unit of
// 1/64 from the derivation. RFC 6386 decodes to YUV and gives no conversion
// to RGB; the reference decoder (libwebp, what browsers show) is the only
// specification of it a user can check, and with the floor of each 14-bit
// term the rounding of the constants is a choice, not a consequence of
// BT.601. These two are that decoder's choice, found by its output (with the
// derived values 0.7% of the bytes of the 88 lossy files of
// libwebp-test-data are one off; with these none is), not its code.
namespace sgcl::codec::detail::vp8 {
    inline constexpr int KY = 19077, KRV = 26149, KGU = 6419, KGV = 13320, KBU = 33050;
    inline constexpr int OR = -14234, OG = 8708, OB = -17685;

    inline int term(int x, int c) noexcept {
        return (x * c) >> 8;
    }

    inline uint8_t channel(int v) noexcept {
        return uint8_t(v < 0 ? 0 : (v >> 6) > 255 ? 255 : v >> 6);
    }

    // One row of chroma at the luma's width w: `near` the chroma row of
    // the luma row, `far` the one on the other side of it (the same row at
    // the picture's top and bottom); blend: scratch of (w + 1) / 2 + 2
    inline void upsample_row(const uint8_t* near, const uint8_t* far, uint8_t* out, uint32_t w, uint16_t* blend) noexcept {
        const uint32_t cw = (w + 1) / 2;
        uint16_t* v = blend + 1;
        uint32_t i = 0;
#if defined(SGCL_CODEC_NEON)
        for (; i + 8 <= cw; i += 8) {
            const uint16x8_t n = vmovl_u8(vld1_u8(near + i));
            const uint16x8_t f = vmovl_u8(vld1_u8(far + i));
            vst1q_u16(v + i, vaddq_u16(vmulq_n_u16(n, 3), f));
        }
#elif defined(SGCL_CODEC_SSE2)
        const __m128i zero = _mm_setzero_si128();
        for (; i + 8 <= cw; i += 8) {
            const __m128i n = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(near + i)), zero);
            const __m128i f = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(far + i)), zero);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(v + i), _mm_add_epi16(_mm_add_epi16(_mm_add_epi16(n, n), n), f));
        }
#endif
        for (; i < cw; ++i) {
            v[i] = uint16_t(3 * near[i] + far[i]);
        }
        v[-1] = v[0];
        v[cw] = v[cw - 1];
        // even pixels lean on the chroma to the left, odd ones to the right
        uint32_t k = 0;
#if defined(SGCL_CODEC_NEON)
        const uint16x8_t eight = vdupq_n_u16(8);
        for (; k + 8 <= cw && 2 * k + 16 <= w; k += 8) {
            const uint16x8_t c = vld1q_u16(v + k);
            const uint16x8_t l = vld1q_u16(v + k - 1);
            const uint16x8_t r = vld1q_u16(v + k + 1);
            const uint16x8_t c3 = vaddq_u16(vmulq_n_u16(c, 3), eight);
            uint8x8x2_t o;
            o.val[0] = vshrn_n_u16(vaddq_u16(c3, l), 4);
            o.val[1] = vshrn_n_u16(vaddq_u16(c3, r), 4);
            vst2_u8(out + 2 * k, o);
        }
#elif defined(SGCL_CODEC_SSE2)
        const __m128i eight = _mm_set1_epi16(8);
        for (; k + 8 <= cw && 2 * k + 16 <= w; k += 8) {
            const __m128i c = _mm_loadu_si128(reinterpret_cast<const __m128i*>(v + k));
            const __m128i l = _mm_loadu_si128(reinterpret_cast<const __m128i*>(v + k - 1));
            const __m128i r = _mm_loadu_si128(reinterpret_cast<const __m128i*>(v + k + 1));
            const __m128i c3 = _mm_add_epi16(_mm_add_epi16(_mm_add_epi16(c, c), c), eight);
            const __m128i even = _mm_srli_epi16(_mm_add_epi16(c3, l), 4);
            const __m128i odd = _mm_srli_epi16(_mm_add_epi16(c3, r), 4);
            // even | odd << 8 in each 16-bit lane: the pixels in order
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2 * k), _mm_or_si128(even, _mm_slli_epi16(odd, 8)));
        }
#endif
        for (; k < cw; ++k) {
            const int c3 = 3 * v[k] + 8;
            out[2 * k] = uint8_t((c3 + v[ptrdiff_t(k) - 1]) >> 4);
            if (2 * k + 1 < w) {
                out[2 * k + 1] = uint8_t((c3 + v[k + 1]) >> 4);
            }
        }
    }

    // n pixels of Y, U, V (full resolution) and alpha (none: opaque) as
    // ARGB words
    inline void yuv_to_argb(const uint8_t* y, const uint8_t* u, const uint8_t* v, const uint8_t* a, uint32_t* out, size_t n) noexcept {
        size_t i = 0;
#if defined(SGCL_CODEC_NEON)
        auto mulhi = [](uint16x8_t x, uint16_t c) {
            return vcombine_u16(vshrn_n_u32(vmull_n_u16(vget_low_u16(x), c), 8), vshrn_n_u32(vmull_n_u16(vget_high_u16(x), c), 8));
        };
        for (; i + 8 <= n; i += 8) {
            const uint16x8_t Y = vmovl_u8(vld1_u8(y + i)), U = vmovl_u8(vld1_u8(u + i)), V = vmovl_u8(vld1_u8(v + i));
            const uint16x8_t ty = mulhi(Y, KY);
            uint8x8x4_t o;
            o.val[2] = vqshrn_n_u16(vqsubq_u16(vaddq_u16(ty, mulhi(V, KRV)), vdupq_n_u16(uint16_t(-OR))), 6);
            o.val[1] = vqshrn_n_u16(vqsubq_u16(vaddq_u16(ty, vdupq_n_u16(OG)), vaddq_u16(mulhi(U, KGU), mulhi(V, KGV))), 6);
            o.val[0] = vqshrn_n_u16(vqsubq_u16(vaddq_u16(ty, mulhi(U, KBU)), vdupq_n_u16(uint16_t(-OB))), 6);
            o.val[3] = a ? vld1_u8(a + i) : vdup_n_u8(255);
            vst4_u8(reinterpret_cast<uint8_t*>(out + i), o);
        }
#elif defined(SGCL_CODEC_SSE2)
        const __m128i zero = _mm_setzero_si128();
        auto mulhi = [](__m128i x, int c) { return _mm_mulhi_epu16(_mm_slli_epi16(x, 8), _mm_set1_epi16(int16_t(c))); };
        for (; i + 8 <= n; i += 8) {
            const __m128i Y = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(y + i)), zero);
            const __m128i U = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(u + i)), zero);
            const __m128i V = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(v + i)), zero);
            const __m128i ty = mulhi(Y, KY);
            const __m128i R = _mm_srli_epi16(_mm_subs_epu16(_mm_add_epi16(ty, mulhi(V, KRV)), _mm_set1_epi16(int16_t(-OR))), 6);
            const __m128i G = _mm_srli_epi16(_mm_subs_epu16(_mm_add_epi16(ty, _mm_set1_epi16(OG)), _mm_add_epi16(mulhi(U, KGU), mulhi(V, KGV))), 6);
            const __m128i B = _mm_srli_epi16(_mm_subs_epu16(_mm_add_epi16(ty, mulhi(U, KBU)), _mm_set1_epi16(int16_t(-OB))), 6);
            const __m128i A = a ? _mm_loadl_epi64(reinterpret_cast<const __m128i*>(a + i)) : _mm_set1_epi8(char(0xff));
            const __m128i b8 = _mm_packus_epi16(B, zero), g8 = _mm_packus_epi16(G, zero), r8 = _mm_packus_epi16(R, zero);
            const __m128i bg = _mm_unpacklo_epi8(b8, g8), ra = _mm_unpacklo_epi8(r8, A);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), _mm_unpacklo_epi16(bg, ra));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i + 4), _mm_unpackhi_epi16(bg, ra));
        }
#endif
        for (; i < n; ++i) {
            const int ty = term(y[i], KY);
            const int r = ty + term(v[i], KRV) + OR;
            const int g = ty - term(u[i], KGU) - term(v[i], KGV) + OG;
            const int b = ty + term(u[i], KBU) + OB;
            out[i] = uint32_t(a ? a[i] : 255) << 24 | uint32_t(channel(r)) << 16 | uint32_t(channel(g)) << 8 | channel(b);
        }
    }

    // The same colors written as the image's bytes, without the ARGB word
    // between: R, G, B (rgb8) or R, G, B, A (rgba8) of each pixel. The
    // channels are yuv_to_argb's (the same terms, the same clamps)
    inline void yuv_to_rgb_plain(const uint8_t* y, const uint8_t* u, const uint8_t* v, uint8_t* out, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            const int ty = term(y[i], KY);
            out[3 * i] = channel(ty + term(v[i], KRV) + OR);
            out[3 * i + 1] = channel(ty - term(u[i], KGU) - term(v[i], KGV) + OG);
            out[3 * i + 2] = channel(ty + term(u[i], KBU) + OB);
        }
    }

    inline void yuv_to_rgba_plain(const uint8_t* y, const uint8_t* u, const uint8_t* v, const uint8_t* a, uint8_t* out, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            const int ty = term(y[i], KY);
            out[4 * i] = channel(ty + term(v[i], KRV) + OR);
            out[4 * i + 1] = channel(ty - term(u[i], KGU) - term(v[i], KGV) + OG);
            out[4 * i + 2] = channel(ty + term(u[i], KBU) + OB);
            out[4 * i + 3] = a ? a[i] : 255;
        }
    }

#if defined(SGCL_CODEC_NEON)
    namespace yuv {
        // eight pixels' R, G, B as yuv_to_argb computes them
        inline uint8x8x3_t rgb8(const uint8_t* y, const uint8_t* u, const uint8_t* v) noexcept {
            auto mulhi = [](uint16x8_t x, uint16_t c) {
                return vcombine_u16(vshrn_n_u32(vmull_n_u16(vget_low_u16(x), c), 8), vshrn_n_u32(vmull_n_u16(vget_high_u16(x), c), 8));
            };
            const uint16x8_t Y = vmovl_u8(vld1_u8(y)), U = vmovl_u8(vld1_u8(u)), V = vmovl_u8(vld1_u8(v));
            const uint16x8_t ty = mulhi(Y, KY);
            uint8x8x3_t o;
            o.val[0] = vqshrn_n_u16(vqsubq_u16(vaddq_u16(ty, mulhi(V, KRV)), vdupq_n_u16(uint16_t(-OR))), 6);
            o.val[1] = vqshrn_n_u16(vqsubq_u16(vaddq_u16(ty, vdupq_n_u16(OG)), vaddq_u16(mulhi(U, KGU), mulhi(V, KGV))), 6);
            o.val[2] = vqshrn_n_u16(vqsubq_u16(vaddq_u16(ty, mulhi(U, KBU)), vdupq_n_u16(uint16_t(-OB))), 6);
            return o;
        }
    }

    inline void yuv_to_rgb(const uint8_t* y, const uint8_t* u, const uint8_t* v, uint8_t* out, size_t n) noexcept {
        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            vst3_u8(out + 3 * i, yuv::rgb8(y + i, u + i, v + i));
        }
        yuv_to_rgb_plain(y + i, u + i, v + i, out + 3 * i, n - i);
    }

    inline void yuv_to_rgba(const uint8_t* y, const uint8_t* u, const uint8_t* v, const uint8_t* a, uint8_t* out, size_t n) noexcept {
        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            const uint8x8x3_t c = yuv::rgb8(y + i, u + i, v + i);
            uint8x8x4_t o;
            o.val[0] = c.val[0];
            o.val[1] = c.val[1];
            o.val[2] = c.val[2];
            o.val[3] = a ? vld1_u8(a + i) : vdup_n_u8(255);
            vst4_u8(out + 4 * i, o);
        }
        yuv_to_rgba_plain(y + i, u + i, v + i, a ? a + i : nullptr, out + 4 * i, n - i);
    }
#elif defined(SGCL_CODEC_SSE2)
    namespace yuv {
        struct Rgb {
            __m128i r, g, b;   // eight bytes each, in the low halves
        };

        inline Rgb rgb8(const uint8_t* y, const uint8_t* u, const uint8_t* v) noexcept {
            const __m128i zero = _mm_setzero_si128();
            auto mulhi = [](__m128i x, int c) { return _mm_mulhi_epu16(_mm_slli_epi16(x, 8), _mm_set1_epi16(int16_t(c))); };
            const __m128i Y = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(y)), zero);
            const __m128i U = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(u)), zero);
            const __m128i V = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(v)), zero);
            const __m128i ty = mulhi(Y, KY);
            const __m128i R = _mm_srli_epi16(_mm_subs_epu16(_mm_add_epi16(ty, mulhi(V, KRV)), _mm_set1_epi16(int16_t(-OR))), 6);
            const __m128i G = _mm_srli_epi16(_mm_subs_epu16(_mm_add_epi16(ty, _mm_set1_epi16(OG)), _mm_add_epi16(mulhi(U, KGU), mulhi(V, KGV))), 6);
            const __m128i B = _mm_srli_epi16(_mm_subs_epu16(_mm_add_epi16(ty, mulhi(U, KBU)), _mm_set1_epi16(int16_t(-OB))), 6);
            return {_mm_packus_epi16(R, zero), _mm_packus_epi16(G, zero), _mm_packus_epi16(B, zero)};
        }
    }

    // Eight pixels a step; each pixel's three bytes written as a 32-bit
    // word R G B 0 at 3i (the fourth byte the next pixel's, written over
    // by it), so the last pixel of the row is left to the plain road
    inline void yuv_to_rgb(const uint8_t* y, const uint8_t* u, const uint8_t* v, uint8_t* out, size_t n) noexcept {
        size_t i = 0;
        const __m128i zero = _mm_setzero_si128();
        for (; i + 9 <= n; i += 8) {
            const yuv::Rgb c = yuv::rgb8(y + i, u + i, v + i);
            const __m128i rg = _mm_unpacklo_epi8(c.r, c.g), b0 = _mm_unpacklo_epi8(c.b, zero);
            uint32_t words[8];
            _mm_storeu_si128(reinterpret_cast<__m128i*>(words), _mm_unpacklo_epi16(rg, b0));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(words + 4), _mm_unpackhi_epi16(rg, b0));
            for (int k = 0; k < 8; ++k) {
                std::memcpy(out + 3 * (i + size_t(k)), &words[k], 4);
            }
        }
        yuv_to_rgb_plain(y + i, u + i, v + i, out + 3 * i, n - i);
    }

    inline void yuv_to_rgba(const uint8_t* y, const uint8_t* u, const uint8_t* v, const uint8_t* a, uint8_t* out, size_t n) noexcept {
        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            const yuv::Rgb c = yuv::rgb8(y + i, u + i, v + i);
            const __m128i A = a ? _mm_loadl_epi64(reinterpret_cast<const __m128i*>(a + i)) : _mm_set1_epi8(char(0xff));
            const __m128i rg = _mm_unpacklo_epi8(c.r, c.g), ba = _mm_unpacklo_epi8(c.b, A);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 4 * i), _mm_unpacklo_epi16(rg, ba));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 4 * i + 16), _mm_unpackhi_epi16(rg, ba));
        }
        yuv_to_rgba_plain(y + i, u + i, v + i, a ? a + i : nullptr, out + 4 * i, n - i);
    }
#else
    inline void yuv_to_rgb(const uint8_t* y, const uint8_t* u, const uint8_t* v, uint8_t* out, size_t n) noexcept {
        yuv_to_rgb_plain(y, u, v, out, n);
    }

    inline void yuv_to_rgba(const uint8_t* y, const uint8_t* u, const uint8_t* v, const uint8_t* a, uint8_t* out, size_t n) noexcept {
        yuv_to_rgba_plain(y, u, v, a, out, n);
    }
#endif
}
