//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "simd.h"
#include "vp8_tables.h"
#include "../error.h"
#include "../../core/string.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// The VP8 decoder of lossy WebP (RFC 6386), key frames only (all WebP
// has): the frame header, the per-macroblock modes, the DCT tokens, the
// inverse transforms, the intra predictions and the loop filter, into the
// planes of Y, U and V (4:2:0) as RFC 6386 decodes them.
namespace sgcl::codec::detail::vp8 {
    // The boolean entropy decoder (RFC 6386 §7) over a range of bytes, in
    // RFC 6386's arithmetic. The value is a 64-bit window: _bits of it
    // below the 8 the decoder compares, refilled up to 7 bytes at once
    // when they run out. The first fill is at init, so an empty range
    // counts as read past its end. A fill past the last byte marks eof
    // (its bits read as zeros), which the caller refuses. A first byte of
    // 0xFF puts the value past the range from the start, which no encoder
    // writes (bad_start: refused, where libwebp decodes on).
    class BoolDecoder {
    public:
        void init(const uint8_t* p, size_t n) noexcept {
            _p = p;
            _end = p + n;
            _value = 0;
            _bits = -8;
            _range = 255;
            _eof = false;
            _bad_start = n != 0 && p[0] == 0xFF;
            _fill();
        }

        bool bit(unsigned prob) noexcept {
            if (_bits < 0) {
                _fill();
            }
            const uint32_t split = 1 + (((_range - 1) * prob) >> 8);
            const uint64_t big = uint64_t(split) << _bits;
            // the two outcomes as selects, not a branch (the bit is the
            // data's, a branch on it mispredicted half the time)
            const bool one = _value >= big;
            _range = one ? _range - split : split;
            _value = one ? _value - big : _value;
            // doubled until it is 128 or more
            const int shift = __builtin_clz(_range) - 24;
            _range <<= shift;
            _bits -= shift;
            return one;
        }

        // An n-bit literal, most significant bit first (L(n))
        uint32_t literal(unsigned n) noexcept {
            uint32_t v = 0;
            while (n--) {
                v = v << 1 | uint32_t(bit(128));
            }
            return v;
        }

        // A magnitude of n bits and its sign
        SGCL_INLINE_HOT int32_t signed_literal(unsigned n) noexcept {
            const int32_t v = int32_t(literal(n));
            return bit(128) ? -v : v;
        }

        // A tree of RFC 6386 §8.1: positive entries lead on, the others
        // are the negated values
        int tree(const int8_t* t, const uint8_t* probs, int start = 0) noexcept {
            int i = start;
            while ((i = t[i + int(bit(probs[i >> 1]))]) > 0) {
            }
            return -i;
        }

        SGCL_INLINE_HOT bool eof() const noexcept {
            return _eof;
        }

        SGCL_INLINE_HOT bool bad_start() const noexcept {
            return _bad_start;
        }

    private:
        void _fill() noexcept {
            if (_p < _end) {
                const size_t n = std::min<size_t>(7, size_t(_end - _p));
                for (size_t i = 0; i < n; ++i) {
                    _value = _value << 8 | _p[i];
                }
                _p += n;
                _bits += int(8 * n);
            } else if (!_eof) {
                _value <<= 8;
                _bits += 8;
                _eof = true;
            } else {
                _bits = 0;
            }
        }

        const uint8_t* _p = nullptr;
        const uint8_t* _end = nullptr;
        uint64_t _value = 0;
        int _bits = -8;
        uint32_t _range = 255;
        bool _eof = false;
        bool _bad_start = false;
    };

    // Why a frame was refused as invalid (Decoder::damage)
    enum class Damage : uint8_t { none, overrun, bad_start };

    enum : uint8_t { DC_PRED, V_PRED, H_PRED, TM_PRED, B_PRED };
    enum : uint8_t { B_DC_PRED, B_TM_PRED, B_VE_PRED, B_HE_PRED, B_LD_PRED, B_RD_PRED, B_VR_PRED, B_VL_PRED, B_HD_PRED, B_HU_PRED };

    // The trees of RFC 6386 (§8.1, §11.2, §11.4, §13.2), values negated
    // (a leaf 0 as -0: the trees below have no leaf 0 at a positive index,
    // so 0 read as an index ends the walk with value 0)
    inline constexpr int8_t KfYmodeTree[8] = {-B_PRED, 2, 4, 6, -DC_PRED, -V_PRED, -H_PRED, -TM_PRED};
    inline constexpr uint8_t KfYmodeProbs[4] = {145, 156, 163, 128};
    inline constexpr int8_t UvModeTree[6] = {-DC_PRED, 2, -V_PRED, 4, -H_PRED, -TM_PRED};
    inline constexpr uint8_t KfUvModeProbs[3] = {142, 114, 183};
    inline constexpr int8_t BmodeTree[18] = {-B_DC_PRED, 2, -B_TM_PRED, 4, -B_VE_PRED, 6, 8, 12, -B_HE_PRED, 10, -B_RD_PRED, -B_VR_PRED, -B_LD_PRED, 14, -B_VL_PRED, 16, -B_HD_PRED, -B_HU_PRED};
    inline constexpr int8_t SegmentTree[6] = {2, 4, -0, -1, -2, -3};

    enum : uint8_t { DCT_0, DCT_1, DCT_2, DCT_3, DCT_4, CAT1, CAT2, CAT3, CAT4, CAT5, CAT6, DCT_EOB };
    inline constexpr int8_t CoeffTree[22] = {-DCT_EOB, 2, -DCT_0, 4, -DCT_1, 6, 8, 12, -DCT_2, 10, -DCT_3, -DCT_4, 14, 16, -CAT1, -CAT2, 18, 20, -CAT3, -CAT4, -CAT5, -CAT6};

    inline constexpr uint8_t Bands[17] = {0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 0};
    inline constexpr uint8_t Zigzag[16] = {0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15};
    inline constexpr uint8_t Pcat1[] = {159, 0};
    inline constexpr uint8_t Pcat2[] = {165, 145, 0};
    inline constexpr uint8_t Pcat3[] = {173, 148, 140, 0};
    inline constexpr uint8_t Pcat4[] = {176, 155, 140, 135, 0};
    inline constexpr uint8_t Pcat5[] = {180, 157, 141, 134, 130, 0};
    inline constexpr uint8_t Pcat6[] = {254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0};
    inline constexpr const uint8_t* Pcats[6] = {Pcat1, Pcat2, Pcat3, Pcat4, Pcat5, Pcat6};
    inline constexpr int CatBase[6] = {5, 7, 11, 19, 35, 67};

    SGCL_INLINE_HOT uint8_t clamp255(int v) noexcept {
        return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v);
    }

    // 14.3: the inverse WHT. Its rows between the passes are kept whole,
    // as libwebp and Go keep them (RFC 6386's reference keeps them in 16
    // bits, which wraps on large coefficients where they do not); the
    // output, the blocks' DC coefficients, 16 bits as theirs
    inline void inverse_wht(const int16_t* in, int16_t* out) noexcept {
        int t[16];
        for (int i = 0; i < 4; ++i) {
            const int a1 = in[i] + in[12 + i];
            const int b1 = in[4 + i] + in[8 + i];
            const int c1 = in[4 + i] - in[8 + i];
            const int d1 = in[i] - in[12 + i];
            t[i] = a1 + b1;
            t[4 + i] = c1 + d1;
            t[8 + i] = a1 - b1;
            t[12 + i] = d1 - c1;
        }
        for (int i = 0; i < 4; ++i) {
            const int a1 = t[4 * i] + t[4 * i + 3];
            const int b1 = t[4 * i + 1] + t[4 * i + 2];
            const int c1 = t[4 * i + 1] - t[4 * i + 2];
            const int d1 = t[4 * i] - t[4 * i + 3];
            out[4 * i] = int16_t((a1 + b1 + 3) >> 3);
            out[4 * i + 1] = int16_t((c1 + d1 + 3) >> 3);
            out[4 * i + 2] = int16_t((a1 - b1 + 3) >> 3);
            out[4 * i + 3] = int16_t((d1 - c1 + 3) >> 3);
        }
    }

    // 14.4: the inverse DCT of a 4x4 block, its residue added to the
    // prediction in place (dst, stride), clamped (14.5). The rows between
    // the passes and the residue kept whole, as libwebp and Go keep them
    // (RFC 6386's reference: 16 bits, see inverse_wht); the second pass's
    // products in 64 bits (past 32 on large coefficients)
    inline void inverse_dct_add_plain(const int16_t* in, uint8_t* dst, ptrdiff_t stride) noexcept {
        constexpr int C1 = 20091;   // sqrt(2)·cos(π/8) − 1, 16-bit fixed point
        constexpr int S1 = 35468;   // sqrt(2)·sin(π/8)
        auto mul = [](int v, int k) {
            return int((int64_t(v) * k) >> 16);
        };
        int t[16];
        for (int i = 0; i < 4; ++i) {
            const int a1 = in[i] + in[8 + i];
            const int b1 = in[i] - in[8 + i];
            int temp1 = (in[4 + i] * S1) >> 16;
            int temp2 = in[12 + i] + ((in[12 + i] * C1) >> 16);
            const int c1 = temp1 - temp2;
            temp1 = in[4 + i] + ((in[4 + i] * C1) >> 16);
            temp2 = (in[12 + i] * S1) >> 16;
            const int d1 = temp1 + temp2;
            t[i] = a1 + d1;
            t[12 + i] = a1 - d1;
            t[4 + i] = b1 + c1;
            t[8 + i] = b1 - c1;
        }
        for (int i = 0; i < 4; ++i) {
            const int* r = t + 4 * i;
            const int a1 = r[0] + r[2];
            const int b1 = r[0] - r[2];
            int temp1 = mul(r[1], S1);
            int temp2 = r[3] + mul(r[3], C1);
            const int c1 = temp1 - temp2;
            temp1 = r[1] + mul(r[1], C1);
            temp2 = mul(r[3], S1);
            const int d1 = temp1 + temp2;
            uint8_t* d = dst + i * stride;
            d[0] = clamp255(d[0] + ((a1 + d1 + 4) >> 3));
            d[3] = clamp255(d[3] + ((a1 - d1 + 4) >> 3));
            d[1] = clamp255(d[1] + ((b1 + c1 + 4) >> 3));
            d[2] = clamp255(d[2] + ((b1 - c1 + 4) >> 3));
        }
    }

#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    // The vector road of inverse_dct_add: a row of the block a vector of
    // four 32-bit lanes, pass 1 lanewise (the columns), a transpose, pass 2
    // lanewise (the rows), a transpose back to add to the rows. S1 = 35468
    // does not fit 16 bits signed: x·S1 >> 16 is x + (x·(S1 − 65536) >> 16)
    // exactly (65536·x shifted down 16 is x). With every input within
    // ±15000, pass 1 stays within ±57750 and pass 2's products under 2^31,
    // so 32 bits hold the plain road's integers; a block with a larger
    // input goes the plain road (64-bit products), which is false (no
    // write) here.
    namespace idct4 {
        inline constexpr int C1 = 20091, S1m = 35468 - 65536;
    }

#if defined(SGCL_CODEC_NEON)
    namespace idct4 {
        SGCL_INLINE_HOT int32x4_t mul_s1(int32x4_t x) noexcept {   // x·S1 >> 16
            return vaddq_s32(x, vshrq_n_s32(vmulq_n_s32(x, S1m), 16));
        }

        SGCL_INLINE_HOT int32x4_t mul_c1(int32x4_t x) noexcept {   // x + x·C1 >> 16
            return vaddq_s32(x, vshrq_n_s32(vmulq_n_s32(x, C1), 16));
        }

        SGCL_INLINE_HOT void transpose(int32x4_t& a, int32x4_t& b, int32x4_t& c, int32x4_t& d) noexcept {
            const int32x4x2_t ab = vtrnq_s32(a, b), cd = vtrnq_s32(c, d);
            a = vcombine_s32(vget_low_s32(ab.val[0]), vget_low_s32(cd.val[0]));
            b = vcombine_s32(vget_low_s32(ab.val[1]), vget_low_s32(cd.val[1]));
            c = vcombine_s32(vget_high_s32(ab.val[0]), vget_high_s32(cd.val[0]));
            d = vcombine_s32(vget_high_s32(ab.val[1]), vget_high_s32(cd.val[1]));
        }
    }

    inline bool inverse_dct_add_vector(const int16_t* in, uint8_t* dst, ptrdiff_t stride) noexcept {
        using namespace idct4;
        const int16x8_t lo = vld1q_s16(in), hi = vld1q_s16(in + 8);
        if (vmaxvq_u16(vreinterpretq_u16_s16(vmaxq_s16(vabsq_s16(lo), vabsq_s16(hi)))) > 15000) {
            return false;   // |-32768| reads as 32768 here, above the bound too
        }
        int32x4_t r0 = vmovl_s16(vget_low_s16(lo)), r1 = vmovl_s16(vget_high_s16(lo));
        int32x4_t r2 = vmovl_s16(vget_low_s16(hi)), r3 = vmovl_s16(vget_high_s16(hi));
        // pass 1: the columns (lane i is column i), in the plain road's terms
        {
            const int32x4_t a1 = vaddq_s32(r0, r2), b1 = vsubq_s32(r0, r2);
            const int32x4_t c1 = vsubq_s32(mul_s1(r1), mul_c1(r3));
            const int32x4_t d1 = vaddq_s32(mul_c1(r1), mul_s1(r3));
            r0 = vaddq_s32(a1, d1);
            r3 = vsubq_s32(a1, d1);
            r1 = vaddq_s32(b1, c1);
            r2 = vsubq_s32(b1, c1);
        }
        transpose(r0, r1, r2, r3);   // lane i now row i of the pass's result
        const int32x4_t a1 = vaddq_s32(r0, r2), b1 = vsubq_s32(r0, r2);
        const int32x4_t c1 = vsubq_s32(mul_s1(r1), mul_c1(r3));
        const int32x4_t d1 = vaddq_s32(mul_c1(r1), mul_s1(r3));
        int32x4_t o0 = vshrq_n_s32(vaddq_s32(vaddq_s32(a1, d1), vdupq_n_s32(4)), 3);
        int32x4_t o3 = vshrq_n_s32(vaddq_s32(vsubq_s32(a1, d1), vdupq_n_s32(4)), 3);
        int32x4_t o1 = vshrq_n_s32(vaddq_s32(vaddq_s32(b1, c1), vdupq_n_s32(4)), 3);
        int32x4_t o2 = vshrq_n_s32(vaddq_s32(vsubq_s32(b1, c1), vdupq_n_s32(4)), 3);
        transpose(o0, o1, o2, o3);   // vector y now row y's residue
        const int32x4_t rows[4] = {o0, o1, o2, o3};
        for (int y = 0; y < 4; ++y) {
            uint8_t* d = dst + y * stride;
            uint32_t w;
            std::memcpy(&w, d, 4);
            const int32x4_t px = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(vmovl_u8(vcreate_u8(w)))));
            const int32x4_t sum = vaddq_s32(px, rows[y]);
            const uint8x8_t out = vqmovun_s16(vcombine_s16(vqmovn_s32(sum), vdup_n_s16(0)));
            w = vget_lane_u32(vreinterpret_u32_u8(out), 0);
            std::memcpy(d, &w, 4);
        }
        return true;
    }
#elif defined(SGCL_CODEC_SSE2)
    namespace idct4 {
        // x·k >> 16 for 32-bit lanes and a constant of 17 bits at most: the
        // two 16-bit halves of x multiplied apart (SSE2 has no 32-bit mullo)
        inline __m128i mulhi(__m128i x, int k) noexcept {
            // x = xh·65536 + xl (xl unsigned 16 bits): (x·k) >> 16 =
            // xh·k + (xl·k >> 16), exact as the floor of an integer sum
            const __m128i xl = _mm_and_si128(x, _mm_set1_epi32(0xFFFF));
            const __m128i xh = _mm_srai_epi32(x, 16);
            // xl·k and xh·k on 32 bits through the 16-bit multiplies: xl
            // and |k| fit 16 bits unsigned/signed as used below
            const __m128i kk = _mm_set1_epi32(k);
            auto mul32 = [](__m128i a, __m128i b) {   // low 32 bits of a·b, lanes 0..3
                const __m128i even = _mm_mul_epu32(a, b);
                const __m128i odd = _mm_mul_epu32(_mm_srli_epi64(a, 32), _mm_srli_epi64(b, 32));
                return _mm_unpacklo_epi32(_mm_shuffle_epi32(even, _MM_SHUFFLE(0, 0, 2, 0)), _mm_shuffle_epi32(odd, _MM_SHUFFLE(0, 0, 2, 0)));
            };
            return _mm_add_epi32(mul32(xh, kk), _mm_srai_epi32(mul32(xl, kk), 16));
        }

        SGCL_INLINE_HOT __m128i mul_s1(__m128i x) noexcept {
            return _mm_add_epi32(x, mulhi(x, S1m));
        }

        SGCL_INLINE_HOT __m128i mul_c1(__m128i x) noexcept {
            return _mm_add_epi32(x, mulhi(x, C1));
        }

        inline void transpose(__m128i& a, __m128i& b, __m128i& c, __m128i& d) noexcept {
            const __m128i t0 = _mm_unpacklo_epi32(a, b), t1 = _mm_unpacklo_epi32(c, d);
            const __m128i t2 = _mm_unpackhi_epi32(a, b), t3 = _mm_unpackhi_epi32(c, d);
            a = _mm_unpacklo_epi64(t0, t1);
            b = _mm_unpackhi_epi64(t0, t1);
            c = _mm_unpacklo_epi64(t2, t3);
            d = _mm_unpackhi_epi64(t2, t3);
        }
    }

    inline bool inverse_dct_add_vector(const int16_t* in, uint8_t* dst, ptrdiff_t stride) noexcept {
        using namespace idct4;
        const __m128i lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in));
        const __m128i hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 8));
        // |v| > 15000 for any: v outside [-15000, 15000]
        const __m128i bound = _mm_set1_epi16(15000), nbound = _mm_set1_epi16(-15000);
        const __m128i out = _mm_or_si128(_mm_or_si128(_mm_cmpgt_epi16(lo, bound), _mm_cmplt_epi16(lo, nbound)),
                                         _mm_or_si128(_mm_cmpgt_epi16(hi, bound), _mm_cmplt_epi16(hi, nbound)));
        if (_mm_movemask_epi8(out)) {
            return false;
        }
        auto widen = [](__m128i v, bool high) {
            const __m128i x = high ? _mm_unpackhi_epi16(v, v) : _mm_unpacklo_epi16(v, v);
            return _mm_srai_epi32(x, 16);
        };
        __m128i r0 = widen(lo, false), r1 = widen(lo, true), r2 = widen(hi, false), r3 = widen(hi, true);
        {
            const __m128i a1 = _mm_add_epi32(r0, r2), b1 = _mm_sub_epi32(r0, r2);
            const __m128i c1 = _mm_sub_epi32(mul_s1(r1), mul_c1(r3));
            const __m128i d1 = _mm_add_epi32(mul_c1(r1), mul_s1(r3));
            r0 = _mm_add_epi32(a1, d1);
            r3 = _mm_sub_epi32(a1, d1);
            r1 = _mm_add_epi32(b1, c1);
            r2 = _mm_sub_epi32(b1, c1);
        }
        transpose(r0, r1, r2, r3);
        const __m128i four = _mm_set1_epi32(4);
        const __m128i a1 = _mm_add_epi32(r0, r2), b1 = _mm_sub_epi32(r0, r2);
        const __m128i c1 = _mm_sub_epi32(mul_s1(r1), mul_c1(r3));
        const __m128i d1 = _mm_add_epi32(mul_c1(r1), mul_s1(r3));
        __m128i o0 = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(a1, d1), four), 3);
        __m128i o3 = _mm_srai_epi32(_mm_add_epi32(_mm_sub_epi32(a1, d1), four), 3);
        __m128i o1 = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(b1, c1), four), 3);
        __m128i o2 = _mm_srai_epi32(_mm_add_epi32(_mm_sub_epi32(b1, c1), four), 3);
        transpose(o0, o1, o2, o3);
        const __m128i rows[4] = {o0, o1, o2, o3};
        const __m128i zero = _mm_setzero_si128();
        for (int y = 0; y < 4; ++y) {
            uint8_t* d = dst + y * stride;
            uint32_t w;
            std::memcpy(&w, d, 4);
            const __m128i px = _mm_unpacklo_epi16(_mm_unpacklo_epi8(_mm_cvtsi32_si128(int(w)), zero), zero);
            const __m128i sum = _mm_add_epi32(px, rows[y]);
            const __m128i narrow = _mm_packus_epi16(_mm_packs_epi32(sum, zero), zero);
            w = uint32_t(_mm_cvtsi128_si32(narrow));
            std::memcpy(d, &w, 4);
        }
        return true;
    }
#endif
#endif

    SGCL_INLINE_HOT void inverse_dct_add(const int16_t* in, uint8_t* dst, ptrdiff_t stride) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        if (inverse_dct_add_vector(in, dst, stride)) {
            return;
        }
#endif
        inverse_dct_add_plain(in, dst, stride);
    }

    // A 4x4 block whose only coefficient is its DC: inverse_dct_add's
    // arithmetic with the other fifteen zero leaves (dc + 4) >> 3 at every
    // pixel (the multiplied terms are of zeros), added and clamped
    inline void dc_only_add(int16_t dc, uint8_t* dst, ptrdiff_t stride) noexcept {
        const int v = (int(dc) + 4) >> 3;
        for (int r = 0; r < 4; ++r) {
            uint8_t* d = dst + r * stride;
            for (int c = 0; c < 4; ++c) {
                d[c] = clamp255(d[c] + v);
            }
        }
    }

    // TM_PRED of a 16x16 or 8x8 block (§12.2): pixel (r, c) = clamp(L[r] +
    // A[c] − P), A[-1] being P. The plain road; tm_predict() takes the
    // vector one where there is one
    template<int N>
    inline void tm_predict_plain(uint8_t* dst, ptrdiff_t stride, const uint8_t* A, const uint8_t* L) noexcept {
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                dst[r * stride + c] = clamp255(L[r] + A[c] - A[-1]);
            }
        }
    }

    // A[c] − P once on 16 bits, L[r] added a row, narrowed with saturation
    // (the clamp): the plain road's integers
    template<int N>
    inline void tm_predict(uint8_t* dst, ptrdiff_t stride, const uint8_t* A, const uint8_t* L) noexcept {
#if defined(SGCL_CODEC_NEON)
        const uint8x8_t p = vdup_n_u8(A[-1]);
        const int16x8_t d0 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(A), p));
        if constexpr (N == 16) {
            const int16x8_t d1 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(A + 8), p));
            for (int r = 0; r < 16; ++r) {
                const int16x8_t l = vdupq_n_s16(L[r]);
                vst1q_u8(dst + r * stride, vcombine_u8(vqmovun_s16(vaddq_s16(d0, l)), vqmovun_s16(vaddq_s16(d1, l))));
            }
        } else {
            for (int r = 0; r < 8; ++r) {
                vst1_u8(dst + r * stride, vqmovun_s16(vaddq_s16(d0, vdupq_n_s16(L[r]))));
            }
        }
#elif defined(SGCL_CODEC_SSE2)
        const __m128i zero = _mm_setzero_si128(), p = _mm_set1_epi16(A[-1]);
        const __m128i d0 = _mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(A)), zero), p);
        if constexpr (N == 16) {
            const __m128i d1 = _mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(A + 8)), zero), p);
            for (int r = 0; r < 16; ++r) {
                const __m128i l = _mm_set1_epi16(L[r]);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + r * stride), _mm_packus_epi16(_mm_add_epi16(d0, l), _mm_add_epi16(d1, l)));
            }
        } else {
            for (int r = 0; r < 8; ++r) {
                const __m128i v = _mm_add_epi16(d0, _mm_set1_epi16(L[r]));
                _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + r * stride), _mm_packus_epi16(v, v));
            }
        }
#else
        tm_predict_plain<N>(dst, stride, A, L);
#endif
    }

    // What a 4x4 block of a macroblock holds, for its reconstruction
    enum BlockKind : uint8_t { BlockZero, BlockDcOnly, BlockFull };

    // The loop filter's arithmetic (RFC 6386 §15.2, §15.3)
    namespace filter {
        SGCL_INLINE_HOT int c8(int v) noexcept {
            return v < -128 ? -128 : v > 127 ? 127 : v;
        }

        SGCL_INLINE_HOT int u2s(uint8_t v) noexcept {
            return int(v) - 128;
        }

        SGCL_INLINE_HOT uint8_t s2u(int v) noexcept {
            return uint8_t(c8(v) + 128);
        }

        // p points at the pixel after the edge (q0), step across the edge
        inline int common_adjust(bool outer, uint8_t* q, ptrdiff_t step) noexcept {
            const int p1 = u2s(q[-2 * step]), p0 = u2s(q[-step]), q0 = u2s(q[0]), q1 = u2s(q[step]);
            int a = c8((outer ? c8(p1 - q1) : 0) + 3 * (q0 - p0));
            const int b = c8(a + 3) >> 3;
            a = c8(a + 4) >> 3;
            q[0] = s2u(q0 - a);
            q[-step] = s2u(p0 + b);
            return a;
        }

        SGCL_INLINE_HOT bool simple_yes(const uint8_t* q, ptrdiff_t step, int edge) noexcept {
            return std::abs(q[-step] - q[0]) * 2 + std::abs(q[-2 * step] - q[step]) / 2 <= edge;
        }

        SGCL_INLINE_HOT bool normal_yes(const uint8_t* q, ptrdiff_t step, int interior, int edge) noexcept {
            const int p3 = q[-4 * step], p2 = q[-3 * step], p1 = q[-2 * step], p0 = q[-step];
            const int q0 = q[0], q1 = q[step], q2 = q[2 * step], q3 = q[3 * step];
            return std::abs(p0 - q0) * 2 + std::abs(p1 - q1) / 2 <= edge && std::abs(p3 - p2) <= interior && std::abs(p2 - p1) <= interior &&
                   std::abs(p1 - p0) <= interior && std::abs(q3 - q2) <= interior && std::abs(q2 - q1) <= interior && std::abs(q1 - q0) <= interior;
        }

        SGCL_INLINE_HOT bool hev(const uint8_t* q, ptrdiff_t step, int threshold) noexcept {
            return std::abs(q[-2 * step] - q[-step]) > threshold || std::abs(q[step] - q[0]) > threshold;
        }

        // n segments along an edge: q the first after-edge pixel, step
        // across the edge, along the stride between segments
        inline void simple_edge(uint8_t* q, ptrdiff_t step, ptrdiff_t along, int n, int edge) noexcept {
            for (int i = 0; i < n; ++i, q += along) {
                if (simple_yes(q, step, edge)) {
                    common_adjust(true, q, step);
                }
            }
        }

        inline void subblock_edge(uint8_t* q, ptrdiff_t step, ptrdiff_t along, int n, int edge, int interior, int hev_threshold) noexcept {
            for (int i = 0; i < n; ++i, q += along) {
                if (!normal_yes(q, step, interior, edge)) {
                    continue;
                }
                const bool hv = hev(q, step, hev_threshold);
                const int p1 = u2s(q[-2 * step]), q1 = u2s(q[step]);
                const int a = (common_adjust(hv, q, step) + 1) >> 1;
                if (!hv) {
                    q[step] = s2u(q1 - a);
                    q[-2 * step] = s2u(p1 + a);
                }
            }
        }

        inline void macroblock_edge(uint8_t* q, ptrdiff_t step, ptrdiff_t along, int n, int edge, int interior, int hev_threshold) noexcept {
            for (int i = 0; i < n; ++i, q += along) {
                if (!normal_yes(q, step, interior, edge)) {
                    continue;
                }
                if (hev(q, step, hev_threshold)) {
                    common_adjust(true, q, step);
                    continue;
                }
                const int p2 = u2s(q[-3 * step]), p1 = u2s(q[-2 * step]), p0 = u2s(q[-step]);
                const int q0 = u2s(q[0]), q1 = u2s(q[step]), q2 = u2s(q[2 * step]);
                const int w = c8(c8(p1 - q1) + 3 * (q0 - p0));
                int a = c8((27 * w + 63) >> 7);
                q[0] = s2u(q0 - a);
                q[-step] = s2u(p0 + a);
                a = c8((18 * w + 63) >> 7);
                q[step] = s2u(q1 - a);
                q[-2 * step] = s2u(p1 + a);
                a = c8((9 * w + 63) >> 7);
                q[2 * step] = s2u(q2 - a);
                q[-3 * step] = s2u(p2 + a);
            }
        }
    }

    // The vector road of the loop filters: sixteen segments of an edge at
    // once, the eight pixels across it (p3 … q3) in eight vectors of 16
    // lanes. A horizontal edge's rows load as they are; a vertical edge's
    // are transposed in and out (16 rows of 8 bytes). The chroma planes go
    // together, U in lanes 0–7 and V in 8–15. The arithmetic on signed bytes
    // (x - 128) with saturation, which is RFC 6386's c8(): each sum that
    // c8() clamps is a chain of additions in one direction once it leaves the
    // range, so saturating at every step and clamping once give the same
    // (tests/codec/simd_paths.cpp holds it for every input); the macroblock
    // filter's 27, 18 and 9 taps on 16 bits. Every lane computes all the
    // variants and a mask picks what the scalar road's branches pick.
    namespace filter_vector {
#if defined(SGCL_CODEC_NEON)
        using U8 = uint8x16_t;
        using S8 = int8x16_t;

        SGCL_INLINE_HOT U8 dup(int v) noexcept {
            return vdupq_n_u8(uint8_t(v));
        }

        SGCL_INLINE_HOT S8 flip(U8 x) noexcept {
            return vreinterpretq_s8_u8(veorq_u8(x, vdupq_n_u8(0x80)));
        }

        SGCL_INLINE_HOT U8 unflip(S8 x) noexcept {
            return veorq_u8(vreinterpretq_u8_s8(x), vdupq_n_u8(0x80));
        }

        SGCL_INLINE_HOT U8 abd(U8 a, U8 b) noexcept {
            return vabdq_u8(a, b);
        }

        SGCL_INLINE_HOT U8 le(U8 a, U8 b) noexcept {
            return vcleq_u8(a, b);
        }

        SGCL_INLINE_HOT U8 gt(U8 a, U8 b) noexcept {
            return vcgtq_u8(a, b);
        }

        SGCL_INLINE_HOT U8 both(U8 a, U8 b) noexcept {
            return vandq_u8(a, b);
        }

        SGCL_INLINE_HOT U8 either(U8 a, U8 b) noexcept {
            return vorrq_u8(a, b);
        }

        SGCL_INLINE_HOT S8 sdup(int v) noexcept {
            return vdupq_n_s8(int8_t(v));
        }

        SGCL_INLINE_HOT U8 but_not(U8 a, U8 b) noexcept {   // a and not b
            return vbicq_u8(a, b);
        }

        SGCL_INLINE_HOT U8 select(U8 mask, U8 yes, U8 no) noexcept {
            return vbslq_u8(mask, yes, no);
        }

        SGCL_INLINE_HOT S8 sadd(S8 a, S8 b) noexcept {
            return vqaddq_s8(a, b);
        }

        SGCL_INLINE_HOT S8 ssub(S8 a, S8 b) noexcept {
            return vqsubq_s8(a, b);
        }

        SGCL_INLINE_HOT S8 shr3(S8 a) noexcept {
            return vshrq_n_s8(a, 3);
        }

        SGCL_INLINE_HOT S8 half_up(S8 a) noexcept {   // (a + 1) >> 1
            return vrshrq_n_s8(a, 1);
        }

        SGCL_INLINE_HOT U8 sadd_u(U8 a, U8 b) noexcept {
            return vqaddq_u8(a, b);
        }

        SGCL_INLINE_HOT U8 half_u(U8 a) noexcept {
            return vshrq_n_u8(a, 1);
        }

        SGCL_INLINE_HOT S8 sand(S8 a, U8 mask) noexcept {
            return vreinterpretq_s8_u8(vandq_u8(vreinterpretq_u8_s8(a), mask));
        }

        // c8((k·w + 63) >> 7)
        template<int K>
        SGCL_INLINE_HOT S8 tap(S8 w) noexcept {
            const int16x8_t lo = vshrq_n_s16(vmlaq_n_s16(vdupq_n_s16(63), vmovl_s8(vget_low_s8(w)), K), 7);
            const int16x8_t hi = vshrq_n_s16(vmlaq_n_s16(vdupq_n_s16(63), vmovl_s8(vget_high_s8(w)), K), 7);
            return vcombine_s8(vqmovn_s16(lo), vqmovn_s16(hi));
        }

        // 16 rows of 8 bytes (row r at p + r·stride; rows 8–15 from p2 when
        // not null, else p + (r)·stride) as eight vectors of the columns
        inline void transpose_in(const uint8_t* p, const uint8_t* p2, ptrdiff_t stride, U8 (&c)[8]) noexcept {
            uint8x16_t r[8];
            for (int i = 0; i < 8; ++i) {
                const uint8_t* second = p2 ? p2 + i * stride : p + (i + 8) * stride;
                r[i] = vcombine_u8(vld1_u8(p + i * stride), vld1_u8(second));
            }
            const uint8x16x2_t a01 = vtrnq_u8(r[0], r[1]), a23 = vtrnq_u8(r[2], r[3]);
            const uint8x16x2_t a45 = vtrnq_u8(r[4], r[5]), a67 = vtrnq_u8(r[6], r[7]);
            const uint16x8x2_t b02 = vtrnq_u16(vreinterpretq_u16_u8(a01.val[0]), vreinterpretq_u16_u8(a23.val[0]));
            const uint16x8x2_t b13 = vtrnq_u16(vreinterpretq_u16_u8(a01.val[1]), vreinterpretq_u16_u8(a23.val[1]));
            const uint16x8x2_t b46 = vtrnq_u16(vreinterpretq_u16_u8(a45.val[0]), vreinterpretq_u16_u8(a67.val[0]));
            const uint16x8x2_t b57 = vtrnq_u16(vreinterpretq_u16_u8(a45.val[1]), vreinterpretq_u16_u8(a67.val[1]));
            const uint32x4x2_t c04 = vtrnq_u32(vreinterpretq_u32_u16(b02.val[0]), vreinterpretq_u32_u16(b46.val[0]));
            const uint32x4x2_t c15 = vtrnq_u32(vreinterpretq_u32_u16(b13.val[0]), vreinterpretq_u32_u16(b57.val[0]));
            const uint32x4x2_t c26 = vtrnq_u32(vreinterpretq_u32_u16(b02.val[1]), vreinterpretq_u32_u16(b46.val[1]));
            const uint32x4x2_t c37 = vtrnq_u32(vreinterpretq_u32_u16(b13.val[1]), vreinterpretq_u32_u16(b57.val[1]));
            c[0] = vreinterpretq_u8_u32(c04.val[0]);
            c[1] = vreinterpretq_u8_u32(c15.val[0]);
            c[2] = vreinterpretq_u8_u32(c26.val[0]);
            c[3] = vreinterpretq_u8_u32(c37.val[0]);
            c[4] = vreinterpretq_u8_u32(c04.val[1]);
            c[5] = vreinterpretq_u8_u32(c15.val[1]);
            c[6] = vreinterpretq_u8_u32(c26.val[1]);
            c[7] = vreinterpretq_u8_u32(c37.val[1]);
        }

        // the inverse: the transpose is its own inverse on these halves
        inline void transpose_out(uint8_t* p, uint8_t* p2, ptrdiff_t stride, const U8 (&c)[8]) noexcept {
            U8 r[8];
            const uint8x16x2_t a01 = vtrnq_u8(c[0], c[1]), a23 = vtrnq_u8(c[2], c[3]);
            const uint8x16x2_t a45 = vtrnq_u8(c[4], c[5]), a67 = vtrnq_u8(c[6], c[7]);
            const uint16x8x2_t b02 = vtrnq_u16(vreinterpretq_u16_u8(a01.val[0]), vreinterpretq_u16_u8(a23.val[0]));
            const uint16x8x2_t b13 = vtrnq_u16(vreinterpretq_u16_u8(a01.val[1]), vreinterpretq_u16_u8(a23.val[1]));
            const uint16x8x2_t b46 = vtrnq_u16(vreinterpretq_u16_u8(a45.val[0]), vreinterpretq_u16_u8(a67.val[0]));
            const uint16x8x2_t b57 = vtrnq_u16(vreinterpretq_u16_u8(a45.val[1]), vreinterpretq_u16_u8(a67.val[1]));
            const uint32x4x2_t c04 = vtrnq_u32(vreinterpretq_u32_u16(b02.val[0]), vreinterpretq_u32_u16(b46.val[0]));
            const uint32x4x2_t c15 = vtrnq_u32(vreinterpretq_u32_u16(b13.val[0]), vreinterpretq_u32_u16(b57.val[0]));
            const uint32x4x2_t c26 = vtrnq_u32(vreinterpretq_u32_u16(b02.val[1]), vreinterpretq_u32_u16(b46.val[1]));
            const uint32x4x2_t c37 = vtrnq_u32(vreinterpretq_u32_u16(b13.val[1]), vreinterpretq_u32_u16(b57.val[1]));
            r[0] = vreinterpretq_u8_u32(c04.val[0]);
            r[1] = vreinterpretq_u8_u32(c15.val[0]);
            r[2] = vreinterpretq_u8_u32(c26.val[0]);
            r[3] = vreinterpretq_u8_u32(c37.val[0]);
            r[4] = vreinterpretq_u8_u32(c04.val[1]);
            r[5] = vreinterpretq_u8_u32(c15.val[1]);
            r[6] = vreinterpretq_u8_u32(c26.val[1]);
            r[7] = vreinterpretq_u8_u32(c37.val[1]);
            for (int i = 0; i < 8; ++i) {
                uint8_t* second = p2 ? p2 + i * stride : p + (i + 8) * stride;
                vst1_u8(p + i * stride, vget_low_u8(r[i]));
                vst1_u8(second, vget_high_u8(r[i]));
            }
        }

        SGCL_INLINE_HOT U8 load(const uint8_t* p, const uint8_t* p2) noexcept {
            return p2 ? vcombine_u8(vld1_u8(p), vld1_u8(p2)) : vld1q_u8(p);
        }

        SGCL_INLINE_HOT void store(uint8_t* p, uint8_t* p2, U8 v) noexcept {
            if (p2) {
                vst1_u8(p, vget_low_u8(v));
                vst1_u8(p2, vget_high_u8(v));
            } else {
                vst1q_u8(p, v);
            }
        }
#elif defined(SGCL_CODEC_SSE2)
        using U8 = __m128i;
        using S8 = __m128i;

        SGCL_INLINE_HOT U8 dup(int v) noexcept {
            return _mm_set1_epi8(char(v));
        }

        SGCL_INLINE_HOT S8 flip(U8 x) noexcept {
            return _mm_xor_si128(x, _mm_set1_epi8(char(0x80)));
        }

        SGCL_INLINE_HOT U8 unflip(S8 x) noexcept {
            return _mm_xor_si128(x, _mm_set1_epi8(char(0x80)));
        }

        SGCL_INLINE_HOT U8 abd(U8 a, U8 b) noexcept {
            return _mm_or_si128(_mm_subs_epu8(a, b), _mm_subs_epu8(b, a));
        }

        SGCL_INLINE_HOT U8 le(U8 a, U8 b) noexcept {   // a <= b, unsigned
            return _mm_cmpeq_epi8(_mm_subs_epu8(a, b), _mm_setzero_si128());
        }

        SGCL_INLINE_HOT U8 gt(U8 a, U8 b) noexcept {
            return _mm_xor_si128(le(a, b), _mm_set1_epi8(char(0xFF)));
        }

        SGCL_INLINE_HOT U8 both(U8 a, U8 b) noexcept {
            return _mm_and_si128(a, b);
        }

        SGCL_INLINE_HOT U8 either(U8 a, U8 b) noexcept {
            return _mm_or_si128(a, b);
        }

        SGCL_INLINE_HOT S8 sdup(int v) noexcept {
            return _mm_set1_epi8(char(v));
        }

        SGCL_INLINE_HOT U8 but_not(U8 a, U8 b) noexcept {
            return _mm_andnot_si128(b, a);
        }

        SGCL_INLINE_HOT U8 select(U8 mask, U8 yes, U8 no) noexcept {
            return _mm_or_si128(_mm_and_si128(mask, yes), _mm_andnot_si128(mask, no));
        }

        SGCL_INLINE_HOT S8 sadd(S8 a, S8 b) noexcept {
            return _mm_adds_epi8(a, b);
        }

        SGCL_INLINE_HOT S8 ssub(S8 a, S8 b) noexcept {
            return _mm_subs_epi8(a, b);
        }

        // arithmetic shifts of bytes through 16-bit lanes (the byte in the
        // high half, shifted down 8 more)
        template<int N>
        SGCL_INLINE_HOT S8 sar(S8 a) noexcept {
            const __m128i lo = _mm_srai_epi16(_mm_unpacklo_epi8(_mm_setzero_si128(), a), 8 + N);
            const __m128i hi = _mm_srai_epi16(_mm_unpackhi_epi8(_mm_setzero_si128(), a), 8 + N);
            return _mm_packs_epi16(lo, hi);
        }

        SGCL_INLINE_HOT S8 shr3(S8 a) noexcept {
            return sar<3>(a);
        }

        SGCL_INLINE_HOT S8 half_up(S8 a) noexcept {   // (a + 1) >> 1 on a within ±16
            return sar<1>(_mm_add_epi8(a, _mm_set1_epi8(1)));
        }

        SGCL_INLINE_HOT U8 sadd_u(U8 a, U8 b) noexcept {
            return _mm_adds_epu8(a, b);
        }

        SGCL_INLINE_HOT U8 half_u(U8 a) noexcept {
            return _mm_and_si128(_mm_srli_epi16(a, 1), _mm_set1_epi8(0x7F));
        }

        SGCL_INLINE_HOT S8 sand(S8 a, U8 mask) noexcept {
            return _mm_and_si128(a, mask);
        }

        template<int K>
        SGCL_INLINE_HOT S8 tap(S8 w) noexcept {
            const __m128i k = _mm_set1_epi16(K), r = _mm_set1_epi16(63);
            const __m128i lo = _mm_srai_epi16(_mm_add_epi16(_mm_mullo_epi16(_mm_srai_epi16(_mm_unpacklo_epi8(w, w), 8), k), r), 7);
            const __m128i hi = _mm_srai_epi16(_mm_add_epi16(_mm_mullo_epi16(_mm_srai_epi16(_mm_unpackhi_epi8(w, w), 8), k), r), 7);
            return _mm_packs_epi16(lo, hi);
        }

        // 8x8 bytes (rows in the low halves of r[0..7]) transposed: column c
        // in the low half of out[c]
        inline void transpose8(const __m128i (&r)[8], __m128i (&out)[8]) noexcept {
            const __m128i u0 = _mm_unpacklo_epi8(r[0], r[1]), u1 = _mm_unpacklo_epi8(r[2], r[3]);
            const __m128i u2 = _mm_unpacklo_epi8(r[4], r[5]), u3 = _mm_unpacklo_epi8(r[6], r[7]);
            const __m128i v0 = _mm_unpacklo_epi16(u0, u1), v1 = _mm_unpackhi_epi16(u0, u1);
            const __m128i v2 = _mm_unpacklo_epi16(u2, u3), v3 = _mm_unpackhi_epi16(u2, u3);
            const __m128i w0 = _mm_unpacklo_epi32(v0, v2), w1 = _mm_unpackhi_epi32(v0, v2);
            const __m128i w2 = _mm_unpacklo_epi32(v1, v3), w3 = _mm_unpackhi_epi32(v1, v3);
            out[0] = w0;
            out[1] = _mm_srli_si128(w0, 8);
            out[2] = w1;
            out[3] = _mm_srli_si128(w1, 8);
            out[4] = w2;
            out[5] = _mm_srli_si128(w2, 8);
            out[6] = w3;
            out[7] = _mm_srli_si128(w3, 8);
        }

        inline void transpose_in(const uint8_t* p, const uint8_t* p2, ptrdiff_t stride, U8 (&c)[8]) noexcept {
            __m128i a[8], b[8], ta[8], tb[8];
            for (int i = 0; i < 8; ++i) {
                const uint8_t* second = p2 ? p2 + i * stride : p + (i + 8) * stride;
                a[i] = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p + i * stride));
                b[i] = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(second));
            }
            transpose8(a, ta);
            transpose8(b, tb);
            for (int k = 0; k < 8; ++k) {
                c[k] = _mm_unpacklo_epi64(ta[k], tb[k]);
            }
        }

        inline void transpose_out(uint8_t* p, uint8_t* p2, ptrdiff_t stride, const U8 (&c)[8]) noexcept {
            __m128i a[8], b[8], ta[8], tb[8];
            for (int k = 0; k < 8; ++k) {
                a[k] = c[k];
                b[k] = _mm_srli_si128(c[k], 8);
            }
            transpose8(a, ta);
            transpose8(b, tb);
            for (int i = 0; i < 8; ++i) {
                uint8_t* second = p2 ? p2 + i * stride : p + (i + 8) * stride;
                _mm_storel_epi64(reinterpret_cast<__m128i*>(p + i * stride), ta[i]);
                _mm_storel_epi64(reinterpret_cast<__m128i*>(second), tb[i]);
            }
        }

        SGCL_INLINE_HOT U8 load(const uint8_t* p, const uint8_t* p2) noexcept {
            return p2 ? _mm_unpacklo_epi64(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(p)), _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p2)))
                      : _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
        }

        SGCL_INLINE_HOT void store(uint8_t* p, uint8_t* p2, U8 v) noexcept {
            if (p2) {
                _mm_storel_epi64(reinterpret_cast<__m128i*>(p), v);
                _mm_storel_epi64(reinterpret_cast<__m128i*>(p2), _mm_srli_si128(v, 8));
            } else {
                _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v);
            }
        }
#endif

#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        // The eight pixels across an edge: c[0] = p3 … c[3] = p0, c[4] = q0 … c[7] = q3
        enum Kind { Simple, Subblock, Macroblock };

        // c8(c8(p1 - q1)·outer + 3 (q0 - p0)) as saturating steps
        SGCL_INLINE_HOT S8 value(S8 p1, S8 p0, S8 q0, S8 q1, U8 outer) noexcept {
            const S8 d = ssub(q0, p0);
            S8 a = sand(ssub(p1, q1), outer);
            a = sadd(a, d);
            a = sadd(a, d);
            return sadd(a, d);
        }

        template<Kind K>
        inline void edge(U8 (&c)[8], int edge_limit, int interior, int hev_threshold) noexcept {
            const U8 p3 = c[0], p2 = c[1], p1 = c[2], p0 = c[3], q0 = c[4], q1 = c[5], q2 = c[6], q3 = c[7];
            // |p0 - q0|·2 + |p1 - q1|/2 <= edge (saturated: the limit is under 255)
            U8 mask = le(sadd_u(sadd_u(abd(p0, q0), abd(p0, q0)), half_u(abd(p1, q1))), dup(edge_limit));
            if constexpr (K != Simple) {
                const U8 i = dup(interior);
                mask = both(mask, both(both(le(abd(p3, p2), i), le(abd(p2, p1), i)), both(le(abd(p1, p0), i), le(abd(q3, q2), i))));
                mask = both(mask, both(le(abd(q2, q1), i), le(abd(q1, q0), i)));
            }
            const S8 sp1 = flip(p1), sp0 = flip(p0), sq0 = flip(q0), sq1 = flip(q1);
            if constexpr (K == Simple) {
                const S8 w = value(sp1, sp0, sq0, sq1, dup(0xFF));
                const S8 a = shr3(sadd(w, sdup(4)));
                const S8 b = shr3(sadd(w, sdup(3)));
                c[4] = select(mask, unflip(ssub(sq0, a)), q0);
                c[3] = select(mask, unflip(sadd(sp0, b)), p0);
            } else {
                const U8 t = dup(hev_threshold);
                const U8 hev = either(gt(abd(p1, p0), t), gt(abd(q1, q0), t));
                if constexpr (K == Subblock) {
                    const S8 w = value(sp1, sp0, sq0, sq1, hev);
                    const S8 a = shr3(sadd(w, sdup(4)));
                    const S8 b = shr3(sadd(w, sdup(3)));
                    c[4] = select(mask, unflip(ssub(sq0, a)), q0);
                    c[3] = select(mask, unflip(sadd(sp0, b)), p0);
                    const U8 outer = but_not(mask, hev);
                    const S8 a2 = half_up(a);
                    c[5] = select(outer, unflip(ssub(sq1, a2)), q1);
                    c[2] = select(outer, unflip(sadd(sp1, a2)), p1);
                } else {
                    const S8 w = value(sp1, sp0, sq0, sq1, dup(0xFF));
                    // hev lanes: the common adjustment of p0 and q0
                    const S8 a = shr3(sadd(w, sdup(4)));
                    const S8 b = shr3(sadd(w, sdup(3)));
                    const U8 h = both(mask, hev), n = but_not(mask, hev);
                    const S8 sp2 = flip(p2), sq2 = flip(q2);
                    const S8 a27 = tap<27>(w), a18 = tap<18>(w), a9 = tap<9>(w);
                    c[4] = select(h, unflip(ssub(sq0, a)), select(n, unflip(ssub(sq0, a27)), q0));
                    c[3] = select(h, unflip(sadd(sp0, b)), select(n, unflip(sadd(sp0, a27)), p0));
                    c[5] = select(n, unflip(ssub(sq1, a18)), q1);
                    c[2] = select(n, unflip(sadd(sp1, a18)), p1);
                    c[6] = select(n, unflip(ssub(sq2, a9)), q2);
                    c[1] = select(n, unflip(sadd(sp2, a9)), p2);
                }
            }
        }

        // An edge down a column (step 1 across it): 16 rows from p, or 8
        // from p and 8 from p2 (U and V); p the first pixel after the edge
        template<Kind K>
        SGCL_INLINE_HOT void vertical_edge(uint8_t* p, uint8_t* p2, ptrdiff_t stride, int limit, int interior, int hev_threshold) noexcept {
            U8 c[8];
            transpose_in(p - 4, p2 ? p2 - 4 : nullptr, stride, c);
            edge<K>(c, limit, interior, hev_threshold);
            transpose_out(p - 4, p2 ? p2 - 4 : nullptr, stride, c);
        }

        // An edge along a row (stride across it): 16 bytes from p, or 8
        // from p and 8 from p2
        template<Kind K>
        inline void horizontal_edge(uint8_t* p, uint8_t* p2, ptrdiff_t stride, int limit, int interior, int hev_threshold) noexcept {
            U8 c[8];
            for (int k = 0; k < 8; ++k) {
                c[k] = load(p + (k - 4) * stride, p2 ? p2 + (k - 4) * stride : nullptr);
            }
            edge<K>(c, limit, interior, hev_threshold);
            for (int k = 1; k < 7; ++k) {
                store(p + (k - 4) * stride, p2 ? p2 + (k - 4) * stride : nullptr, c[k]);
            }
        }
#endif
    }

    // A frame of key-frame VP8 decoded into planes of Y (stride of 16 a
    // macroblock) and U, V (8 a macroblock), the macroblocks whole (the
    // picture's width and height cropped by the reader of the planes)
    class Decoder {
    public:
        // The frame of `size` bytes at `data` (a VP8 chunk's payload): the
        // header checked (key frame, version 0..3, shown, the start code, a
        // size of neither side 0), then everything decoded. False with the
        // error; `at` the offset of the chunk, for its messages
        SGCL_INLINE_HOT bool decode(const uint8_t* data, size_t size, uint64_t at, optional<error>& err) noexcept {
            _at = at;
            _beyond_encoders = false;
            _damage = Damage::none;
            if (!_header(data, size, err)) {
                return false;
            }
            return _frame(err);
        }

        // Whether a coefficient of the last frame, as a block's inverse DCT
        // takes it, is past MadeLimit: past what an encoder makes (the
        // residue of 8-bit samples keeps them within about 2040), where
        // libwebp's paths (C, NEON's saturating 16 bits, SSE2's wrapping
        // ones) and Go (32-bit products) each give pixels of their own;
        // within it they and the module agree. For the tests
        SGCL_INLINE_HOT bool beyond_encoders() const noexcept {
            return _beyond_encoders;
        }

        static constexpr int MadeLimit = 2048;

        // Why the last decode failed, when a partition was invalid (read
        // past its end, or a start no encoder writes); none otherwise
        SGCL_INLINE_HOT Damage damage() const noexcept {
            return _damage;
        }

        // The header alone: the size of the picture
        static bool size_of(const uint8_t* data, size_t size, uint32_t& width, uint32_t& height) noexcept {
            if (size < 10) {
                return false;
            }
            const uint32_t tag = uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16;
            const bool key = !(tag & 1);
            const unsigned version = (tag >> 1) & 7;
            const bool show = (tag >> 4) & 1;
            const uint32_t first = tag >> 5;
            if (!key || version > 3 || !show || data[3] != 0x9d || data[4] != 0x01 || data[5] != 0x2a || first >= size) {
                return false;
            }
            width = (uint32_t(data[6]) | uint32_t(data[7]) << 8) & 0x3fff;
            height = (uint32_t(data[8]) | uint32_t(data[9]) << 8) & 0x3fff;
            return width && height;
        }

        SGCL_INLINE_HOT uint32_t width() const noexcept {
            return _width;
        }

        SGCL_INLINE_HOT uint32_t height() const noexcept {
            return _height;
        }

        SGCL_INLINE_HOT const uint8_t* y() const noexcept {
            return _y.data();
        }

        SGCL_INLINE_HOT const uint8_t* u() const noexcept {
            return _u.data();
        }

        SGCL_INLINE_HOT const uint8_t* v() const noexcept {
            return _v.data();
        }

        SGCL_INLINE_HOT size_t y_stride() const noexcept {
            return size_t(_mbw) * 16;
        }

        SGCL_INLINE_HOT size_t uv_stride() const noexcept {
            return size_t(_mbw) * 8;
        }

    private:
        struct Segment {
            int16_t y1[2];   // DC, AC
            int16_t y2[2];
            int16_t uv[2];
        };

        struct MbInfo {
            uint8_t y_mode = DC_PRED;
            uint8_t uv_mode = DC_PRED;
            uint8_t segment = 0;
            bool skip = false;
            uint8_t sub[16] = {};
        };

        struct FilterInfo {
            uint8_t level = 0;
            bool inner = false;
        };

        bool _fail(optional<error>& err, errc code, const char* what, Damage damage = Damage::none) noexcept {
            err = error(code, _at, string(what));
            _damage = damage;
            return false;
        }

        bool _header(const uint8_t* data, size_t size, optional<error>& err) noexcept {
            if (size < 10) {
                return _fail(err, errc::unexpected_end, "webp: a VP8 chunk shorter than its header");
            }
            if (!size_of(data, size, _width, _height)) {
                return _fail(err, errc::corrupt, "webp: not a VP8 key frame header (frame tag, start code, size)");
            }
            const uint32_t tag = uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16;
            const uint32_t first = tag >> 5;
            if (first > size - 10) {
                return _fail(err, errc::corrupt, "webp: a VP8 first partition past the chunk");
            }
            _mbw = (_width + 15) / 16;
            _mbh = (_height + 15) / 16;
            _part0.init(data + 10, first);
            if (_part0.bad_start()) {
                return _fail(err, errc::corrupt, "webp: a VP8 partition no encoder writes (its value past its range)", Damage::bad_start);
            }
            BoolDecoder& b = _part0;
            (void)b.literal(1);   // color space: 0 (the only one there is)
            (void)b.literal(1);   // clamping type: pixels are clamped in any case
            _segmentation = b.literal(1);
            _update_map = false;
            std::memset(_seg_quant, 0, sizeof _seg_quant);
            std::memset(_seg_filter, 0, sizeof _seg_filter);
            std::memset(_seg_probs, 255, sizeof _seg_probs);
            // segmentation on with no segment data (no encoder writes it):
            // the segments' values are absolute zeros, as libwebp and Go
            // take them; RFC 6386's reference decoder clears the mode on a
            // key frame to deltas, which leaves the frame's values (the
            // module's planes are libwebp's and Go's, webp.md)
            _seg_absolute = true;
            if (_segmentation) {
                _update_map = b.literal(1);
                const bool update_data = b.literal(1);
                if (update_data) {
                    _seg_absolute = b.literal(1);   // 1: absolute values (Annex A), 0: deltas
                    for (auto& q : _seg_quant) {
                        q = b.literal(1) ? int8_t(b.signed_literal(7)) : 0;
                    }
                    for (auto& f : _seg_filter) {
                        f = b.literal(1) ? int8_t(b.signed_literal(6)) : 0;
                    }
                }
                if (_update_map) {
                    for (auto& p : _seg_probs) {
                        p = b.literal(1) ? uint8_t(b.literal(8)) : 255;
                    }
                }
            }
            _simple = b.literal(1);
            _level = int(b.literal(6));
            _sharpness = int(b.literal(3));
            _lf_delta = b.literal(1);
            std::memset(_ref_delta, 0, sizeof _ref_delta);
            std::memset(_mode_delta, 0, sizeof _mode_delta);
            if (_lf_delta && b.literal(1)) {
                for (auto& d : _ref_delta) {
                    d = b.literal(1) ? b.signed_literal(6) : 0;
                }
                for (auto& d : _mode_delta) {
                    d = b.literal(1) ? b.signed_literal(6) : 0;
                }
            }
            // the token partitions: their sizes after the first partition
            const unsigned parts = 1u << b.literal(2);
            const uint8_t* sizes = data + 10 + first;
            const uint8_t* end = data + size;
            if (size_t(end - sizes) < 3 * (parts - 1)) {
                return _fail(err, errc::unexpected_end, "webp: the VP8 data ends in its partition sizes");
            }
            // each partition as long as it says (cut at the data's end), the
            // last one the rest, which has to begin before the end
            const uint8_t* part = sizes + 3 * (parts - 1);
            _parts = parts;
            for (unsigned p = 0; p + 1 < parts; ++p) {
                const size_t declared = size_t(sizes[3 * p]) | size_t(sizes[3 * p + 1]) << 8 | size_t(sizes[3 * p + 2]) << 16;
                const size_t n = std::min(declared, size_t(end - part));
                _tokens[p].init(part, n);
                part += n;
            }
            if (part >= end) {
                return _fail(err, errc::unexpected_end, "webp: the VP8 data ends before its last partition");
            }
            _tokens[parts - 1].init(part, size_t(end - part));
            // the quantizer indices
            const int base = int(b.literal(7));
            const int ydc = b.literal(1) ? b.signed_literal(4) : 0;
            const int y2dc = b.literal(1) ? b.signed_literal(4) : 0;
            const int y2ac = b.literal(1) ? b.signed_literal(4) : 0;
            const int uvdc = b.literal(1) ? b.signed_literal(4) : 0;
            const int uvac = b.literal(1) ? b.signed_literal(4) : 0;
            auto dc_q = [](int i) { return DcQuant[i < 0 ? 0 : i > 127 ? 127 : i]; };
            auto ac_q = [](int i) { return AcQuant[i < 0 ? 0 : i > 127 ? 127 : i]; };
            for (int s = 0; s < 4; ++s) {
                int q = base;
                if (_segmentation) {
                    q = _seg_absolute ? _seg_quant[s] : base + _seg_quant[s];
                }
                Segment& g = _segments[s];
                g.y1[0] = dc_q(q + ydc);
                g.y1[1] = ac_q(q);
                g.uv[0] = dc_q(q + uvdc);
                g.uv[1] = ac_q(q + uvac);
                g.y2[0] = int16_t(dc_q(q + y2dc) * 2);
                g.y2[1] = int16_t(ac_q(q + y2ac) * 155 / 100);
                if (g.y2[1] < 8) {
                    g.y2[1] = 8;
                }
                if (g.uv[0] > 132) {
                    g.uv[0] = 132;
                }
            }
            (void)b.literal(1);   // refresh_entropy_probs: one frame, no matter
            std::memcpy(_probs, DefaultCoeffProbs, sizeof _probs);
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 8; ++j) {
                    for (int k = 0; k < 3; ++k) {
                        for (int l = 0; l < 11; ++l) {
                            if (b.bit(CoeffUpdateProbs[i][j][k][l])) {
                                _probs[i][j][k][l] = uint8_t(b.literal(8));
                            }
                        }
                    }
                }
            }
            _skip_prob_used = b.literal(1);
            _skip_prob = _skip_prob_used ? uint8_t(b.literal(8)) : 0;
            if (b.eof()) {
                return _fail(err, errc::unexpected_end, "webp: the VP8 data ends in its frame header", Damage::overrun);
            }
            return true;
        }

        void _modes(MbInfo& m, uint8_t* above, uint8_t* left) noexcept {
            BoolDecoder& b = _part0;
            m.segment = _update_map ? uint8_t(b.tree(SegmentTree, _seg_probs)) : 0;
            m.skip = _skip_prob_used ? b.bit(_skip_prob) : false;
            m.y_mode = uint8_t(b.tree(KfYmodeTree, KfYmodeProbs));
            if (m.y_mode == B_PRED) {
                for (int i = 0; i < 16; ++i) {
                    const uint8_t a = above[i & 3], l = left[i >> 2];
                    const uint8_t mode = uint8_t(b.tree(BmodeTree, KfBmodeProbs[a][l]));
                    m.sub[i] = mode;
                    above[i & 3] = mode;
                    left[i >> 2] = mode;
                }
            } else {
                // the subblock mode a 16x16 mode stands for, for the contexts (§11.3)
                const uint8_t b_mode = m.y_mode == DC_PRED ? B_DC_PRED : m.y_mode == V_PRED ? B_VE_PRED : m.y_mode == H_PRED ? B_HE_PRED : B_TM_PRED;
                std::memset(above, b_mode, 4);
                std::memset(left, b_mode, 4);
            }
            m.uv_mode = uint8_t(b.tree(UvModeTree, KfUvModeProbs));
        }

        // The tokens of one block (§13): its coefficients dequantized into
        // out (zigzag undone); the position of its end of block (16 without
        // one). A block is non-zero for its neighbours' contexts when that
        // is past `first` (a token read, zero or not: libwebp and Go), and
        // for the loop filter when past 1 or its DC is not zero (libwebp)
        //
        // The token tree (CoeffTree, §13.2) walked as its branches, the bits
        // read in the order tree() reads them: p[0] end of block (not after a
        // zero), p[1] zero, p[2] one, p[3] 2..4 or larger, p[4] and p[5]
        // 2, 3 or 4, p[6] a category of 1–2 or 3–6, p[7] CAT1 or CAT2, p[8]
        // 3–4 or 5–6, p[9] and p[10] which; each category's extra bits by
        // its probabilities (Pcats) after it
        int _block(BoolDecoder& b, int plane, int ctx, int first, const int16_t* dq, int16_t* out) noexcept {
            const uint8_t(*probs)[3][11] = _probs[plane];
            bool prev_zero = false;
            int i = first;
            for (; i < 16; ++i) {
                const uint8_t* p = probs[Bands[i]][ctx];
                if (!prev_zero && !b.bit(p[0])) {
                    break;   // DCT_EOB
                }
                if (!b.bit(p[1])) {   // DCT_0
                    ctx = 0;
                    prev_zero = true;
                    continue;
                }
                int v;
                if (!b.bit(p[2])) {
                    v = 1;
                } else if (!b.bit(p[3])) {
                    v = !b.bit(p[4]) ? 2 : 3 + int(b.bit(p[5]));
                } else {
                    int token;
                    if (!b.bit(p[6])) {
                        token = !b.bit(p[7]) ? CAT1 : CAT2;
                    } else if (!b.bit(p[8])) {
                        token = !b.bit(p[9]) ? CAT3 : CAT4;
                    } else {
                        token = !b.bit(p[10]) ? CAT5 : CAT6;
                    }
                    const uint8_t* cat = Pcats[token - CAT1];
                    int extra = 0;
                    while (*cat) {
                        extra = extra * 2 + int(b.bit(*cat++));
                    }
                    v = CatBase[token - CAT1] + extra;
                }
                ctx = v > 1 ? 2 : 1;
                prev_zero = false;
                if (b.bit(128)) {
                    v = -v;
                }
                const int16_t c = int16_t(v * dq[i > 0]);
                if (plane != 1 && (c > MadeLimit || c < -MadeLimit)) {
                    _beyond_encoders = true;
                }
                out[Zigzag[i]] = c;
            }
            return i;
        }

        // The residue of a macroblock; the non-zero contexts above and to
        // the left carried (§13.3); whether a block is non-zero for the loop
        // filter (its test of the inner edges; _block)
        //
        // coeffs comes all zero and leaves with the blocks' coefficients;
        // kinds[i] what block i (16 Y, then 4 U and 4 V) holds. A block's
        // end past 1 is a token read past its first position; at or below
        // 1 only its coefficient 0 can be set. The Y2 block, once gone into
        // the Y blocks' DCs, is zero again.
        bool _residue(BoolDecoder& b, const MbInfo& m, uint8_t* above, uint8_t* left, int16_t (*coeffs)[16], uint8_t* kinds) noexcept {
            const Segment& q = _segments[m.segment];
            int first = 0;
            int plane = 3;
            bool any = false;
            if (m.y_mode != B_PRED) {
                const int ctx = above[8] + left[8];
                const bool nz = _block(b, 1, ctx, 0, q.y2, coeffs[24]) > 0;
                above[8] = left[8] = nz;
                int16_t dc[16];
                inverse_wht(coeffs[24], dc);
                std::memset(coeffs[24], 0, sizeof coeffs[24]);
                for (int i = 0; i < 16; ++i) {
                    coeffs[i][0] = dc[i];
                    if (dc[i] > MadeLimit || dc[i] < -MadeLimit) {
                        _beyond_encoders = true;
                    }
                }
                first = 1;
                plane = 0;
            }
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const int ctx = above[x] + left[y];
                    const int end = _block(b, plane, ctx, first, q.y1, coeffs[y * 4 + x]);
                    above[x] = left[y] = end > first;
                    any |= end > 1 || coeffs[y * 4 + x][0] != 0;
                    kinds[y * 4 + x] = end > 1 ? BlockFull : coeffs[y * 4 + x][0] ? BlockDcOnly : BlockZero;
                }
            }
            for (int c = 0; c < 2; ++c) {
                for (int y = 0; y < 2; ++y) {
                    for (int x = 0; x < 2; ++x) {
                        const int ctx = above[4 + c * 2 + x] + left[4 + c * 2 + y];
                        const int k = 16 + c * 4 + y * 2 + x;
                        const int end = _block(b, 2, ctx, 0, q.uv, coeffs[k]);
                        above[4 + c * 2 + x] = left[4 + c * 2 + y] = end > 0;
                        any |= end > 1 || coeffs[k][0] != 0;
                        kinds[k] = end > 1 ? BlockFull : coeffs[k][0] ? BlockDcOnly : BlockZero;
                    }
                }
            }
            return any;
        }

        // The prediction of a 16x16 or 8x8 block (§12.2, §12.3) into dst:
        // A above (A[-1] is P), L to the left; the edges of the frame as
        // RFC 6386 gives them. The side a constant, so that the rows'
        // copies and fills are plain moves
        template<int N>
        static void _predict_block(uint8_t mode, uint8_t* dst, ptrdiff_t stride, const uint8_t* A, const uint8_t* L, bool top, bool leftmost) noexcept {
            static_assert(N == 16 || N == 8);
            constexpr int n = N;
            switch (mode) {
                case DC_PRED: {
                    int v = 128;
                    const int shift = n == 16 ? 3 : 2;
                    if (!top && !leftmost) {
                        int sum = 0;
                        for (int i = 0; i < n; ++i) {
                            sum += A[i] + L[i];
                        }
                        v = (sum + n) >> (shift + 2);
                    } else if (!top) {
                        int sum = 0;
                        for (int i = 0; i < n; ++i) {
                            sum += A[i];
                        }
                        v = (sum + n / 2) >> (shift + 1);
                    } else if (!leftmost) {
                        int sum = 0;
                        for (int i = 0; i < n; ++i) {
                            sum += L[i];
                        }
                        v = (sum + n / 2) >> (shift + 1);
                    }
                    for (int r = 0; r < n; ++r) {
                        std::memset(dst + r * stride, v, size_t(n));
                    }
                    break;
                }
                case V_PRED:
                    for (int r = 0; r < n; ++r) {
                        std::memcpy(dst + r * stride, A, size_t(n));
                    }
                    break;
                case H_PRED:
                    for (int r = 0; r < n; ++r) {
                        std::memset(dst + r * stride, L[r], size_t(n));
                    }
                    break;
                default:
                    tm_predict<N>(dst, stride, A, L);
                    break;
            }
        }

        // §12.3: a 4x4 subblock's prediction; A[-1] is P, A[0..7] above
        // and above-right, L[0..3] to the left
        static void _predict_sub(uint8_t mode, uint8_t* B, ptrdiff_t stride, const uint8_t* A, const uint8_t* L) noexcept {
            auto avg3 = [](int x, int y, int z) { return uint8_t((x + y + y + z + 2) >> 2); };
            auto avg2 = [](int x, int y) { return uint8_t((x + y + 1) >> 1); };
            // E: L[3], L[2], L[1], L[0], P, A[0..3]
            const uint8_t E[9] = {L[3], L[2], L[1], L[0], A[-1], A[0], A[1], A[2], A[3]};
            auto at = [&](int r, int c) -> uint8_t& { return B[r * stride + c]; };
            switch (mode) {
                case B_DC_PRED: {
                    int v = 4;
                    for (int i = 0; i < 4; ++i) {
                        v += A[i] + L[i];
                    }
                    v >>= 3;
                    for (int r = 0; r < 4; ++r) {
                        std::memset(B + r * stride, v, 4);
                    }
                    break;
                }
                case B_TM_PRED:
                    for (int r = 0; r < 4; ++r) {
                        for (int c = 0; c < 4; ++c) {
                            at(r, c) = clamp255(L[r] + A[c] - A[-1]);
                        }
                    }
                    break;
                case B_VE_PRED:
                    for (int c = 0; c < 4; ++c) {
                        const uint8_t v = avg3(A[c - 1], A[c], A[c + 1]);
                        at(0, c) = at(1, c) = at(2, c) = at(3, c) = v;
                    }
                    break;
                case B_HE_PRED: {
                    const uint8_t rows[4] = {avg3(A[-1], L[0], L[1]), avg3(L[0], L[1], L[2]), avg3(L[1], L[2], L[3]), avg3(L[2], L[3], L[3])};
                    for (int r = 0; r < 4; ++r) {
                        std::memset(B + r * stride, rows[r], 4);
                    }
                    break;
                }
                case B_LD_PRED:
                    at(0, 0) = avg3(A[0], A[1], A[2]);
                    at(0, 1) = at(1, 0) = avg3(A[1], A[2], A[3]);
                    at(0, 2) = at(1, 1) = at(2, 0) = avg3(A[2], A[3], A[4]);
                    at(0, 3) = at(1, 2) = at(2, 1) = at(3, 0) = avg3(A[3], A[4], A[5]);
                    at(1, 3) = at(2, 2) = at(3, 1) = avg3(A[4], A[5], A[6]);
                    at(2, 3) = at(3, 2) = avg3(A[5], A[6], A[7]);
                    at(3, 3) = avg3(A[6], A[7], A[7]);
                    break;
                case B_RD_PRED:
                    at(3, 0) = avg3(E[0], E[1], E[2]);
                    at(3, 1) = at(2, 0) = avg3(E[1], E[2], E[3]);
                    at(3, 2) = at(2, 1) = at(1, 0) = avg3(E[2], E[3], E[4]);
                    at(3, 3) = at(2, 2) = at(1, 1) = at(0, 0) = avg3(E[3], E[4], E[5]);
                    at(2, 3) = at(1, 2) = at(0, 1) = avg3(E[4], E[5], E[6]);
                    at(1, 3) = at(0, 2) = avg3(E[5], E[6], E[7]);
                    at(0, 3) = avg3(E[6], E[7], E[8]);
                    break;
                case B_VR_PRED:
                    at(3, 0) = avg3(E[1], E[2], E[3]);
                    at(2, 0) = avg3(E[2], E[3], E[4]);
                    at(3, 1) = at(1, 0) = avg3(E[3], E[4], E[5]);
                    at(2, 1) = at(0, 0) = avg2(E[4], E[5]);
                    at(3, 2) = at(1, 1) = avg3(E[4], E[5], E[6]);
                    at(2, 2) = at(0, 1) = avg2(E[5], E[6]);
                    at(3, 3) = at(1, 2) = avg3(E[5], E[6], E[7]);
                    at(2, 3) = at(0, 2) = avg2(E[6], E[7]);
                    at(1, 3) = avg3(E[6], E[7], E[8]);
                    at(0, 3) = avg2(E[7], E[8]);
                    break;
                case B_VL_PRED:
                    at(0, 0) = avg2(A[0], A[1]);
                    at(1, 0) = avg3(A[0], A[1], A[2]);
                    at(2, 0) = at(0, 1) = avg2(A[1], A[2]);
                    at(1, 1) = at(3, 0) = avg3(A[1], A[2], A[3]);
                    at(2, 1) = at(0, 2) = avg2(A[2], A[3]);
                    at(3, 1) = at(1, 2) = avg3(A[2], A[3], A[4]);
                    at(2, 2) = at(0, 3) = avg2(A[3], A[4]);
                    at(3, 2) = at(1, 3) = avg3(A[3], A[4], A[5]);
                    at(2, 3) = avg3(A[4], A[5], A[6]);
                    at(3, 3) = avg3(A[5], A[6], A[7]);
                    break;
                case B_HD_PRED:
                    at(3, 0) = avg2(E[0], E[1]);
                    at(3, 1) = avg3(E[0], E[1], E[2]);
                    at(2, 0) = at(3, 2) = avg2(E[1], E[2]);
                    at(2, 1) = at(3, 3) = avg3(E[1], E[2], E[3]);
                    at(2, 2) = at(1, 0) = avg2(E[2], E[3]);
                    at(2, 3) = at(1, 1) = avg3(E[2], E[3], E[4]);
                    at(1, 2) = at(0, 0) = avg2(E[3], E[4]);
                    at(1, 3) = at(0, 1) = avg3(E[3], E[4], E[5]);
                    at(0, 2) = avg3(E[4], E[5], E[6]);
                    at(0, 3) = avg3(E[5], E[6], E[7]);
                    break;
                default:   // B_HU_PRED
                    at(0, 0) = avg2(L[0], L[1]);
                    at(0, 1) = avg3(L[0], L[1], L[2]);
                    at(0, 2) = at(1, 0) = avg2(L[1], L[2]);
                    at(0, 3) = at(1, 1) = avg3(L[1], L[2], L[3]);
                    at(1, 2) = at(2, 0) = avg2(L[2], L[3]);
                    at(1, 3) = at(2, 1) = avg3(L[2], L[3], L[3]);
                    at(2, 2) = at(2, 3) = at(3, 0) = at(3, 1) = at(3, 2) = at(3, 3) = L[3];
                    break;
            }
        }

        // One macroblock predicted and its residue added, in the planes
        // A block's residue added by what it holds (a zero block adds
        // nothing), its coefficients zero again after
        SGCL_INLINE_HOT static void _add_residue(int16_t* c, uint8_t kind, uint8_t* dst, ptrdiff_t stride) noexcept {
            if (kind == BlockFull) {
                inverse_dct_add(c, dst, stride);
                std::memset(c, 0, 16 * sizeof(int16_t));
            } else if (kind == BlockDcOnly) {
                dc_only_add(c[0], dst, stride);
                c[0] = 0;
            }
        }

        void _reconstruct(int mx, int my, const MbInfo& m, int16_t (*coeffs)[16], const uint8_t* kinds) noexcept {
            const bool top = my == 0, leftmost = mx == 0;
            // luma: the row above (P, 16, and 4 above-right) and the column to the left
            const size_t ys = y_stride();
            uint8_t* Y = _y.data() + size_t(my) * 16 * ys + size_t(mx) * 16;
            uint8_t above[21];
            uint8_t left[16];
            if (top) {
                std::memset(above, 127, sizeof above);
            } else {
                std::memcpy(above + 1, Y - ys, 16);
                above[0] = leftmost ? 129 : Y[-ptrdiff_t(ys) - 1];
                if (mx + 1 < int(_mbw)) {
                    std::memcpy(above + 17, Y - ys + 16, 4);
                } else {
                    std::memset(above + 17, above[16], 4);
                }
            }
            for (int r = 0; r < 16; ++r) {
                left[r] = leftmost ? 129 : Y[ptrdiff_t(r * ys) - 1];
            }
            if (m.y_mode == B_PRED) {
                for (int i = 0; i < 16; ++i) {
                    const int r = i >> 2, c = i & 3;
                    uint8_t* B = Y + r * 4 * ys + c * 4;
                    uint8_t A[9];   // P, A[0..7]
                    uint8_t L[4];
                    if (r == 0) {
                        std::memcpy(A, above + c * 4, 9);
                    } else {
                        A[0] = c == 0 ? left[r * 4 - 1] : B[-ptrdiff_t(ys) - 1];
                        std::memcpy(A + 1, B - ys, 4);
                        if (c < 3) {
                            std::memcpy(A + 5, B - ys + 4, 4);
                        } else {
                            std::memcpy(A + 5, above + 17, 4);   // subblocks 7, 11, 15: those of 3
                        }
                    }
                    for (int k = 0; k < 4; ++k) {
                        L[k] = c == 0 ? left[r * 4 + k] : B[ptrdiff_t(k * ys) - 1];
                    }
                    _predict_sub(m.sub[i], B, ptrdiff_t(ys), A + 1, L);
                    _add_residue(coeffs[i], kinds[i], B, ptrdiff_t(ys));
                }
            } else {
                _predict_block<16>(m.y_mode, Y, ptrdiff_t(ys), above + 1, left, top, leftmost);
                for (int i = 0; i < 16; ++i) {
                    _add_residue(coeffs[i], kinds[i], Y + (i >> 2) * 4 * ys + (i & 3) * 4, ptrdiff_t(ys));
                }
            }
            // chroma
            const size_t cs = uv_stride();
            for (int c = 0; c < 2; ++c) {
                uint8_t* C = (c == 0 ? _u.data() : _v.data()) + size_t(my) * 8 * cs + size_t(mx) * 8;
                uint8_t A[9];
                uint8_t L[8];
                if (top) {
                    std::memset(A, 127, sizeof A);
                } else {
                    std::memcpy(A + 1, C - cs, 8);
                    A[0] = leftmost ? 129 : C[-ptrdiff_t(cs) - 1];
                }
                for (int r = 0; r < 8; ++r) {
                    L[r] = leftmost ? 129 : C[ptrdiff_t(r * cs) - 1];
                }
                _predict_block<8>(m.uv_mode, C, ptrdiff_t(cs), A + 1, L, top, leftmost);
                for (int i = 0; i < 4; ++i) {
                    _add_residue(coeffs[16 + c * 4 + i], kinds[16 + c * 4 + i], C + (i >> 1) * 4 * cs + (i & 1) * 4, ptrdiff_t(cs));
                }
            }
        }

        // The filter level of a macroblock: the segment's level and the
        // deltas of a key frame's intra macroblock, clamped once, as libwebp
        // and Go clamp it. RFC 6386's reference decoder clamps the segment's
        // level first too, which gives another level when the segment's is
        // past 0..63 and the deltas bring it back (valid data; found with
        // files made so, tests/codec/webp.cpp)
        SGCL_INLINE_HOT int _filter_level(const MbInfo& m) const noexcept {
            int level = _level;
            if (_segmentation) {
                level = _seg_absolute ? _seg_filter[m.segment] : level + _seg_filter[m.segment];
            }
            if (_lf_delta) {
                level += _ref_delta[0];
                if (m.y_mode == B_PRED) {
                    level += _mode_delta[0];
                }
            }
            return level < 0 ? 0 : level > 63 ? 63 : level;
        }

#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        // One macroblock's edges by the vector road, in the scalar road's
        // order (each edge reads what the one before it wrote): the left
        // edge, the inner vertical ones, the top edge, the inner horizontal
        // ones; luma sixteen segments an edge, U and V together
        void _filter_vector(uint8_t* Y, uint8_t* U, uint8_t* V, ptrdiff_t yss, ptrdiff_t css, bool left, bool top, bool inner, int mb_edge,
                            int sub_edge, int interior, int hev_threshold) const noexcept {
            using namespace filter_vector;
            const int i = interior, h = hev_threshold;
            if (_simple) {
                if (left) {
                    vertical_edge<Simple>(Y, nullptr, yss, mb_edge, i, h);
                }
                if (inner) {
                    for (int x = 4; x < 16; x += 4) {
                        vertical_edge<Simple>(Y + x, nullptr, yss, sub_edge, i, h);
                    }
                }
                if (top) {
                    horizontal_edge<Simple>(Y, nullptr, yss, mb_edge, i, h);
                }
                if (inner) {
                    for (int y = 4; y < 16; y += 4) {
                        horizontal_edge<Simple>(Y + y * yss, nullptr, yss, sub_edge, i, h);
                    }
                }
                return;
            }
            if (left) {
                vertical_edge<Macroblock>(Y, nullptr, yss, mb_edge, i, h);
                vertical_edge<Macroblock>(U, V, css, mb_edge, i, h);
            }
            if (inner) {
                for (int x = 4; x < 16; x += 4) {
                    vertical_edge<Subblock>(Y + x, nullptr, yss, sub_edge, i, h);
                }
                vertical_edge<Subblock>(U + 4, V + 4, css, sub_edge, i, h);
            }
            if (top) {
                horizontal_edge<Macroblock>(Y, nullptr, yss, mb_edge, i, h);
                horizontal_edge<Macroblock>(U, V, css, mb_edge, i, h);
            }
            if (inner) {
                for (int y = 4; y < 16; y += 4) {
                    horizontal_edge<Subblock>(Y + y * yss, nullptr, yss, sub_edge, i, h);
                }
                horizontal_edge<Subblock>(U + 4 * css, V + 4 * css, css, sub_edge, i, h);
            }
        }
#endif

        // §15: every macroblock in raster order: its left edge, its inner
        // vertical edges, its top edge, its inner horizontal edges
        void _loop_filter() noexcept {
            if (_level == 0) {
                return;
            }
            const size_t ys = y_stride(), cs = uv_stride();
            for (uint32_t my = 0; my < _mbh; ++my) {
                for (uint32_t mx = 0; mx < _mbw; ++mx) {
                    const FilterInfo& f = _filters[size_t(my) * _mbw + mx];
                    const int level = f.level;
                    if (level == 0) {
                        continue;
                    }
                    int interior = level;
                    if (_sharpness) {
                        interior >>= _sharpness > 4 ? 2 : 1;
                        if (interior > 9 - _sharpness) {
                            interior = 9 - _sharpness;
                        }
                    }
                    if (!interior) {
                        interior = 1;
                    }
                    const int hev_threshold = level >= 40 ? 2 : level >= 15 ? 1 : 0;
                    const int mb_edge = (level + 2) * 2 + interior;
                    const int sub_edge = level * 2 + interior;
                    uint8_t* Y = _y.data() + size_t(my) * 16 * ys + size_t(mx) * 16;
                    uint8_t* U = _u.data() + size_t(my) * 8 * cs + size_t(mx) * 8;
                    uint8_t* V = _v.data() + size_t(my) * 8 * cs + size_t(mx) * 8;
                    const ptrdiff_t yss = ptrdiff_t(ys), css = ptrdiff_t(cs);
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
                    _filter_vector(Y, U, V, yss, css, mx > 0, my > 0, f.inner, mb_edge, sub_edge, interior, hev_threshold);
                    continue;
#endif
                    if (_simple) {
                        if (mx > 0) {
                            filter::simple_edge(Y, 1, yss, 16, mb_edge);
                        }
                        if (f.inner) {
                            for (int x = 4; x < 16; x += 4) {
                                filter::simple_edge(Y + x, 1, yss, 16, sub_edge);
                            }
                        }
                        if (my > 0) {
                            filter::simple_edge(Y, yss, 1, 16, mb_edge);
                        }
                        if (f.inner) {
                            for (int y = 4; y < 16; y += 4) {
                                filter::simple_edge(Y + y * yss, yss, 1, 16, sub_edge);
                            }
                        }
                        continue;
                    }
                    if (mx > 0) {
                        filter::macroblock_edge(Y, 1, yss, 16, mb_edge, interior, hev_threshold);
                        filter::macroblock_edge(U, 1, css, 8, mb_edge, interior, hev_threshold);
                        filter::macroblock_edge(V, 1, css, 8, mb_edge, interior, hev_threshold);
                    }
                    if (f.inner) {
                        for (int x = 4; x < 16; x += 4) {
                            filter::subblock_edge(Y + x, 1, yss, 16, sub_edge, interior, hev_threshold);
                        }
                        filter::subblock_edge(U + 4, 1, css, 8, sub_edge, interior, hev_threshold);
                        filter::subblock_edge(V + 4, 1, css, 8, sub_edge, interior, hev_threshold);
                    }
                    if (my > 0) {
                        filter::macroblock_edge(Y, yss, 1, 16, mb_edge, interior, hev_threshold);
                        filter::macroblock_edge(U, css, 1, 8, mb_edge, interior, hev_threshold);
                        filter::macroblock_edge(V, css, 1, 8, mb_edge, interior, hev_threshold);
                    }
                    if (f.inner) {
                        for (int y = 4; y < 16; y += 4) {
                            filter::subblock_edge(Y + y * yss, yss, 1, 16, sub_edge, interior, hev_threshold);
                        }
                        filter::subblock_edge(U + 4 * css, css, 1, 8, sub_edge, interior, hev_threshold);
                        filter::subblock_edge(V + 4 * css, css, 1, 8, sub_edge, interior, hev_threshold);
                    }
                }
            }
        }

        bool _frame(optional<error>& err) noexcept {

            const size_t ys = y_stride(), cs = uv_stride();
            _y.assign(ys * _mbh * 16, 0);
            _u.assign(cs * _mbh * 8, 0);
            _v.assign(cs * _mbh * 8, 0);
            _filters.assign(size_t(_mbw) * _mbh, FilterInfo{});
            // the contexts: subblock modes (4 a macroblock) and non-zero
            // blocks (4 Y, 2 U, 2 V, 1 Y2 a macroblock) above, and to the left
            std::vector<uint8_t> above_modes(size_t(_mbw) * 4, B_DC_PRED);
            std::vector<uint8_t> above_nz(size_t(_mbw) * 9, 0);
            // all zero between macroblocks (_residue and _reconstruct keep it so)
            int16_t coeffs[25][16] = {};
            uint8_t kinds[24];
            for (uint32_t my = 0; my < _mbh; ++my) {
                uint8_t left_modes[4] = {B_DC_PRED, B_DC_PRED, B_DC_PRED, B_DC_PRED};
                uint8_t left_nz[9] = {};
                BoolDecoder& tokens = _tokens[my & (_parts - 1)];
                if (tokens.bad_start()) {
                    return _fail(err, errc::corrupt, "webp: a VP8 partition no encoder writes (its value past its range)", Damage::bad_start);
                }
                for (uint32_t mx = 0; mx < _mbw; ++mx) {
                    MbInfo m;
                    _modes(m, above_modes.data() + size_t(mx) * 4, left_modes);
                    uint8_t* anz = above_nz.data() + size_t(mx) * 9;
                    bool any = false;
                    if (!m.skip) {
                        any = _residue(tokens, m, anz, left_nz, coeffs, kinds);
                    } else {
                        std::memset(kinds, BlockZero, sizeof kinds);
                        // the Y2 context left as it is for B_PRED, which has none (§13.3)
                        std::memset(anz, 0, 8);
                        std::memset(left_nz, 0, 8);
                        if (m.y_mode != B_PRED) {
                            anz[8] = left_nz[8] = 0;
                        }
                    }
                    _reconstruct(int(mx), int(my), m, coeffs, kinds);
                    FilterInfo& f = _filters[size_t(my) * _mbw + mx];
                    f.level = uint8_t(_filter_level(m));
                    f.inner = m.y_mode == B_PRED || any;
                    if (tokens.eof()) {
                        return _fail(err, errc::unexpected_end, "webp: the VP8 data ends before the last macroblock", Damage::overrun);
                    }
                }
                if (_part0.eof()) {
                    return _fail(err, errc::unexpected_end, "webp: the VP8 data ends in the macroblocks' modes", Damage::overrun);
                }
            }
            _loop_filter();
            return true;
        }

        uint64_t _at = 0;
        bool _beyond_encoders = false;
        Damage _damage = Damage::none;
        uint32_t _width = 0, _height = 0, _mbw = 0, _mbh = 0;
        BoolDecoder _part0;
        BoolDecoder _tokens[8];
        unsigned _parts = 1;
        bool _segmentation = false, _update_map = false, _seg_absolute = false;
        int8_t _seg_quant[4] = {}, _seg_filter[4] = {};
        uint8_t _seg_probs[3] = {255, 255, 255};
        bool _simple = false, _lf_delta = false;
        int _level = 0, _sharpness = 0;
        int _ref_delta[4] = {}, _mode_delta[4] = {};
        Segment _segments[4] = {};
        uint8_t _probs[4][8][3][11] = {};
        bool _skip_prob_used = false;
        uint8_t _skip_prob = 0;
        std::vector<uint8_t> _y, _u, _v;
        std::vector<FilterInfo> _filters;
    };
}
