//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "simd.h"
#include "vp8_decoder.h"
#include "vp8_tables.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// The VP8 encoder of lossy WebP (RFC 6386), key frames: the picture to
// planes of Y, U and V (4:2:0, BT.601's limited range), each macroblock
// predicted from what the decoder will have reconstructed (16x16 or 4x4
// luma modes and the chroma modes, each chosen by its distortion and an
// estimate of its bits), its residue through the forward DCT (and the WHT
// of the luma DCs) and quantized, the tokens written with probabilities
// updated for the frame where that saves bits, through the boolean entropy
// encoder of RFC 6386 §7; the normal loop filter at a level from the
// quantizer.
namespace sgcl::codec::detail::vp8 {
    // The boolean entropy encoder (RFC 6386 §7.3): a range and the low end
    // of the interval, the bytes that a carry may still change held back
    class BoolEncoder {
    public:
        std::vector<uint8_t> out;

        SGCL_INLINE_HOT void put(bool bit, unsigned prob) noexcept {
            const uint32_t split = 1 + (((_range - 1) * prob) >> 8);
            if (bit) {
                _bottom += split;
                _range -= split;
            } else {
                _range = split;
            }
            // the range back to 128..255: a bit of the low end shifted out
            // a doubling, a byte out every 8; a bit 31 set before a shift is
            // a carry into the bytes out already
            while (_range < 128) {
                _range <<= 1;
                if (_bottom & 0x80000000u) {
                    _carry();
                }
                _bottom <<= 1;
                if (--_bits == 0) {
                    out.push_back(uint8_t(_bottom >> 24));
                    _bottom &= 0xFFFFFFu;
                    _bits = 8;
                }
            }
        }

        // n bits of v, the most significant first, at even odds
        void literal(uint32_t v, unsigned n) noexcept {
            while (n-- > 0) {
                put((v >> n) & 1, 128);
            }
        }

        // A leaf of a tree (§8.1): the branches on the way to the leaf
        // -value, each at the probability of its node
        void tree(const int8_t* t, const uint8_t* probs, int value) noexcept {
            int path[16];
            int bits[16];
            int depth = 0;
            const int target = -value;
            auto find = [&](auto&& self, int node, int d) -> bool {
                for (int b = 0; b < 2; ++b) {
                    const int next = t[node + b];
                    path[d] = node;
                    bits[d] = b;
                    if (next <= 0) {
                        if (next == target) {
                            depth = d + 1;
                            return true;
                        }
                    } else if (self(self, next, d + 1)) {
                        return true;
                    }
                }
                return false;
            };
            find(find, 0, 0);
            for (int d = 0; d < depth; ++d) {
                put(bits[d] != 0, probs[path[d] >> 1]);
            }
        }

        // The last bits out: 32 zeros at even odds push every pending bit
        // into the bytes (the decoder reads past the end as zeros anyway)
        void finish() noexcept {
            for (int i = 0; i < 32; ++i) {
                put(false, 128);
            }
        }

    private:
        // A carry into the bytes out: 0xFF bytes become 0, the one before
        // them one more
        void _carry() noexcept {
            for (size_t i = out.size(); i-- > 0;) {
                if (++out[i] != 0) {
                    break;
                }
            }
        }

        uint32_t _range = 255;
        uint32_t _bottom = 0;
        int _bits = 24;
    };

    // The bits of a branch taken with probability p of 0, in 1/256 of a bit
    struct BitCosts {
        uint16_t zero[256];
        uint16_t one[256];
        BitCosts() noexcept {
            for (int p = 0; p < 256; ++p) {
                const double q = (p == 0 ? 0.5 : double(p)) / 256.0;
                zero[p] = uint16_t(std::lround(-std::log2(q) * 256));
                one[p] = uint16_t(std::lround(-std::log2(1.0 - q) * 256));
            }
        }
    };

    inline const BitCosts& bit_costs() noexcept {
        static const BitCosts t;
        return t;
    }

    SGCL_INLINE_HOT uint32_t cost(bool bit, unsigned prob) noexcept {
        const BitCosts& t = bit_costs();
        return bit ? t.one[prob] : t.zero[prob];
    }

    // The forward transforms: 4x4 DCT and WHT, the inverses of the
    // decoder's (inverse_dct_add, inverse_wht): its basis is (1, 1, 1, 1),
    // (c, s, −s, −c), (1, −1, −1, 1), (s, −c, c, −s) with c = √2·cos(π/8),
    // s = √2·sin(π/8), each row of norm 2, and the inverse takes 1/8 of
    // Bᵀ·X·B, so the forward is ½·B·r·Bᵀ; c and s in 12-bit fixed point
    inline void forward_dct(const int* r, int16_t* out) noexcept {
        int t[16];
        for (int i = 0; i < 4; ++i) {
            const int* p = r + 4 * i;
            const int a = p[0] + p[3], b = p[1] + p[2], cc = p[1] - p[2], d = p[0] - p[3];
            t[4 * i] = (a + b) * 8;
            t[4 * i + 2] = (a - b) * 8;
            t[4 * i + 1] = (d * 5352 + cc * 2217 + 256) >> 9;
            t[4 * i + 3] = (d * 2217 - cc * 5352 + 256) >> 9;
        }
        for (int i = 0; i < 4; ++i) {
            const int a = t[i] + t[12 + i], b = t[4 + i] + t[8 + i], cc = t[4 + i] - t[8 + i], d = t[i] - t[12 + i];
            out[i] = int16_t((a + b + 8) >> 4);
            out[8 + i] = int16_t((a - b + 8) >> 4);
            out[4 + i] = int16_t((d * 5352 + cc * 2217 + 32768) >> 16);
            out[12 + i] = int16_t((d * 2217 - cc * 5352 + 32768) >> 16);
        }
    }

    // The WHT of the 16 DCs (in raster order of the blocks): ½·H·D·H, H
    // the symmetric Walsh–Hadamard matrix of the decoder's inverse
    inline void forward_wht(const int* dc, int16_t* out) noexcept {
        int t[16];
        for (int i = 0; i < 4; ++i) {
            const int a = dc[i] + dc[12 + i], b = dc[4 + i] + dc[8 + i], cc = dc[4 + i] - dc[8 + i], d = dc[i] - dc[12 + i];
            t[i] = a + b;
            t[4 + i] = cc + d;
            t[8 + i] = a - b;
            t[12 + i] = d - cc;
        }
        for (int i = 0; i < 4; ++i) {
            const int* p = t + 4 * i;
            const int a = p[0] + p[3], b = p[1] + p[2], cc = p[1] - p[2], d = p[0] - p[3];
            const int v[4] = {a + b, cc + d, a - b, d - cc};
            for (int k = 0; k < 4; ++k) {
                out[4 * i + k] = int16_t(v[k] >= 0 ? (v[k] + 1) >> 1 : -((-v[k] + 1) >> 1));
            }
        }
    }

    // The residue of a 4x4 block (source minus prediction) through the
    // forward DCT: the plain road, and the vector road, the same integers
    inline void residue_dct_plain(const uint8_t* src, size_t ss, const uint8_t* pred, size_t ps, int16_t* out) noexcept {
        int r[16];
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                r[4 * y + x] = int(src[y * ss + x]) - int(pred[y * ps + x]);
            }
        }
        forward_dct(r, out);
    }

    // A block of 16 coefficients (raster order) quantized: the levels in
    // zigzag order, the levels times their quantizers back in raster order
    // (what the decoder multiplies), the places before `first` zero; the
    // zigzag index of the last level not zero, -1 for none. (a + bias) / q
    // by a reciprocal of 17 bits, no division: one more where the
    // quotient's fraction is within a quarter of the next step at the
    // largest magnitudes (the decoder multiplies back whatever level is
    // written). The DC rounds to the nearest, the AC with a dead zone (0.42
    // of a step rounds up)
    struct Quantizer4 {
        int16_t q[2];
        uint32_t r[2];
        int bias[2];

        SGCL_INLINE_HOT Quantizer4(int16_t q_dc, int16_t q_ac) noexcept {
            q[0] = q_dc;
            q[1] = q_ac;
            r[0] = (1u << 17) / uint32_t(q_dc) + 1;
            r[1] = (1u << 17) / uint32_t(q_ac) + 1;
            bias[0] = q_dc / 2;
            bias[1] = (q_ac * 107) >> 8;
        }
    };

    inline int quantize_plain(const int16_t* in, int first, const Quantizer4& k, int16_t* levels, int16_t* deq) noexcept {
        int last = -1;
        for (int i = 0; i < 16; ++i) {
            const int z = Zigzag[i];
            const int t = z != 0;
            const int c = in[z];
            const int a = c < 0 ? -c : c;
            int l = int((uint32_t(a + k.bias[t]) * k.r[t]) >> 17);
            l = std::min(l, 2048);
            if (i < first) {
                l = 0;
            }
            levels[i] = int16_t(c < 0 ? -l : l);
            deq[z] = int16_t(levels[i] * k.q[t]);
            last = l ? i : last;
        }
        return last;
    }

    // The squared error of a w × h block (w 4, 8 or 16)
    inline uint32_t sse_plain(const uint8_t* a, size_t as, const uint8_t* b, size_t bs, int w, int h) noexcept {
        uint32_t s = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int d = int(a[y * as + x]) - int(b[y * bs + x]);
                s += uint32_t(d * d);
            }
        }
        return s;
    }

#if defined(SGCL_CODEC_NEON)
    namespace fdct4 {
        SGCL_INLINE_HOT void transpose(int32x4_t& a, int32x4_t& b, int32x4_t& c, int32x4_t& d) noexcept {
            const int32x4x2_t ab = vtrnq_s32(a, b), cd = vtrnq_s32(c, d);
            a = vcombine_s32(vget_low_s32(ab.val[0]), vget_low_s32(cd.val[0]));
            b = vcombine_s32(vget_low_s32(ab.val[1]), vget_low_s32(cd.val[1]));
            c = vcombine_s32(vget_high_s32(ab.val[0]), vget_high_s32(cd.val[0]));
            d = vcombine_s32(vget_high_s32(ab.val[1]), vget_high_s32(cd.val[1]));
        }

        SGCL_INLINE_HOT int32x4_t row(const uint8_t* s, const uint8_t* p) noexcept {
            uint32_t a, b;
            std::memcpy(&a, s, 4);
            std::memcpy(&b, p, 4);
            const int16x8_t d = vreinterpretq_s16_u16(vsubl_u8(vcreate_u8(a), vcreate_u8(b)));
            return vmovl_s16(vget_low_s16(d));
        }
    }

    inline void residue_dct(const uint8_t* src, size_t ss, const uint8_t* pred, size_t ps, int16_t* out) noexcept {
        using namespace fdct4;
        // rows; then lane n of vector k is element (n, k): pass 1 along each row
        int32x4_t c0 = row(src, pred), c1 = row(src + ss, pred + ps), c2 = row(src + 2 * ss, pred + 2 * ps), c3 = row(src + 3 * ss, pred + 3 * ps);
        transpose(c0, c1, c2, c3);
        {
            const int32x4_t a = vaddq_s32(c0, c3), b = vaddq_s32(c1, c2), cc = vsubq_s32(c1, c2), d = vsubq_s32(c0, c3);
            const int32x4_t r256 = vdupq_n_s32(256);
            c0 = vshlq_n_s32(vaddq_s32(a, b), 3);
            c2 = vshlq_n_s32(vsubq_s32(a, b), 3);
            c1 = vshrq_n_s32(vaddq_s32(vmlaq_n_s32(vmulq_n_s32(d, 5352), cc, 2217), r256), 9);
            c3 = vshrq_n_s32(vaddq_s32(vmlsq_n_s32(vmulq_n_s32(d, 2217), cc, 5352), r256), 9);
        }
        // lane k of vector n now (n, k): pass 2 down each column
        transpose(c0, c1, c2, c3);
        const int32x4_t a = vaddq_s32(c0, c3), b = vaddq_s32(c1, c2), cc = vsubq_s32(c1, c2), d = vsubq_s32(c0, c3);
        const int32x4_t r8 = vdupq_n_s32(8), r32k = vdupq_n_s32(32768);
        const int32x4_t o0 = vshrq_n_s32(vaddq_s32(vaddq_s32(a, b), r8), 4);
        const int32x4_t o2 = vshrq_n_s32(vaddq_s32(vsubq_s32(a, b), r8), 4);
        const int32x4_t o1 = vshrq_n_s32(vaddq_s32(vmlaq_n_s32(vmulq_n_s32(d, 5352), cc, 2217), r32k), 16);
        const int32x4_t o3 = vshrq_n_s32(vaddq_s32(vmlsq_n_s32(vmulq_n_s32(d, 2217), cc, 5352), r32k), 16);
        vst1q_s16(out, vcombine_s16(vmovn_s32(o0), vmovn_s32(o1)));
        vst1q_s16(out + 8, vcombine_s16(vmovn_s32(o2), vmovn_s32(o3)));
    }

    inline int quantize(const int16_t* in, int first, const Quantizer4& k, int16_t* levels, int16_t* deq) noexcept {
        const int16x8_t lo = vld1q_s16(in), hi = vld1q_s16(in + 8);
        // lane 0 the DC's quantizer, the rest the AC's
        const uint16x8_t bias_lo = vsetq_lane_u16(uint16_t(k.bias[0]), vdupq_n_u16(uint16_t(k.bias[1])), 0);
        const uint16x8_t bias_hi = vdupq_n_u16(uint16_t(k.bias[1]));
        const uint32x4_t r_first = vsetq_lane_u32(k.r[0], vdupq_n_u32(k.r[1]), 0);
        const uint32x4_t r_ac = vdupq_n_u32(k.r[1]);
        const uint16x8_t a_lo = vaddq_u16(vreinterpretq_u16_s16(vabsq_s16(lo)), bias_lo);
        const uint16x8_t a_hi = vaddq_u16(vreinterpretq_u16_s16(vabsq_s16(hi)), bias_hi);
        auto div = [](uint16x8_t a, uint32x4_t r_low, uint32x4_t r_high) {
            const uint32x4_t l = vshrq_n_u32(vmulq_u32(vmovl_u16(vget_low_u16(a)), r_low), 17);
            const uint32x4_t h = vshrq_n_u32(vmulq_u32(vmovl_u16(vget_high_u16(a)), r_high), 17);
            return vminq_u16(vcombine_u16(vmovn_u32(l), vmovn_u32(h)), vdupq_n_u16(2048));
        };
        uint16x8_t l_lo = div(a_lo, r_first, r_ac), l_hi = div(a_hi, r_ac, r_ac);
        if (first) {
            l_lo = vsetq_lane_u16(0, l_lo, 0);
        }
        // the signs back
        const int16x8_t s_lo = vbslq_s16(vcltzq_s16(lo), vnegq_s16(vreinterpretq_s16_u16(l_lo)), vreinterpretq_s16_u16(l_lo));
        const int16x8_t s_hi = vbslq_s16(vcltzq_s16(hi), vnegq_s16(vreinterpretq_s16_u16(l_hi)), vreinterpretq_s16_u16(l_hi));
        const int16x8_t q_lo = vsetq_lane_s16(k.q[0], vdupq_n_s16(k.q[1]), 0);
        vst1q_s16(deq, vmulq_s16(s_lo, q_lo));
        vst1q_s16(deq + 8, vmulq_s16(s_hi, vdupq_n_s16(k.q[1])));
        // the zigzag: bytes of the 16 levels gathered
        static constexpr uint8_t order[16] = {0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15};
        static constexpr uint8_t bytes[32] = {
            2 * order[0], 2 * order[0] + 1, 2 * order[1], 2 * order[1] + 1, 2 * order[2], 2 * order[2] + 1, 2 * order[3], 2 * order[3] + 1,
            2 * order[4], 2 * order[4] + 1, 2 * order[5], 2 * order[5] + 1, 2 * order[6], 2 * order[6] + 1, 2 * order[7], 2 * order[7] + 1,
            2 * order[8], 2 * order[8] + 1, 2 * order[9], 2 * order[9] + 1, 2 * order[10], 2 * order[10] + 1, 2 * order[11], 2 * order[11] + 1,
            2 * order[12], 2 * order[12] + 1, 2 * order[13], 2 * order[13] + 1, 2 * order[14], 2 * order[14] + 1, 2 * order[15], 2 * order[15] + 1};
        const uint8x16x2_t table = {{vreinterpretq_u8_s16(s_lo), vreinterpretq_u8_s16(s_hi)}};
        const int16x8_t z_lo = vreinterpretq_s16_u8(vqtbl2q_u8(table, vld1q_u8(bytes)));
        const int16x8_t z_hi = vreinterpretq_s16_u8(vqtbl2q_u8(table, vld1q_u8(bytes + 16)));
        vst1q_s16(levels, z_lo);
        vst1q_s16(levels + 8, z_hi);
        // the last level not zero: the largest index among those set
        static constexpr int16_t index_lo[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        static constexpr int16_t index_hi[8] = {9, 10, 11, 12, 13, 14, 15, 16};
        const int16x8_t m = vmaxq_s16(vandq_s16(vreinterpretq_s16_u16(vtstq_s16(z_lo, z_lo)), vld1q_s16(index_lo)),
                                      vandq_s16(vreinterpretq_s16_u16(vtstq_s16(z_hi, z_hi)), vld1q_s16(index_hi)));
        return int(vmaxvq_s16(m)) - 1;
    }

    inline uint32_t sse(const uint8_t* a, size_t as, const uint8_t* b, size_t bs, int w, int h) noexcept {
        uint32x4_t acc = vdupq_n_u32(0);
        if (w == 16) {
            for (int y = 0; y < h; ++y) {
                const uint8x16_t d = vabdq_u8(vld1q_u8(a + y * as), vld1q_u8(b + y * bs));
                const uint16x8_t l = vmull_u8(vget_low_u8(d), vget_low_u8(d)), hh = vmull_u8(vget_high_u8(d), vget_high_u8(d));
                acc = vpadalq_u16(vpadalq_u16(acc, l), hh);
            }
        } else if (w == 8) {
            for (int y = 0; y < h; ++y) {
                const uint8x8_t d = vabd_u8(vld1_u8(a + y * as), vld1_u8(b + y * bs));
                acc = vpadalq_u16(acc, vmull_u8(d, d));
            }
        } else {
            return sse_plain(a, as, b, bs, w, h);
        }
        return vaddvq_u32(acc);
    }
#else
    SGCL_INLINE_HOT void residue_dct(const uint8_t* src, size_t ss, const uint8_t* pred, size_t ps, int16_t* out) noexcept {
        residue_dct_plain(src, ss, pred, ps, out);
    }

    SGCL_INLINE_HOT int quantize(const int16_t* in, int first, const Quantizer4& k, int16_t* levels, int16_t* deq) noexcept {
        return quantize_plain(in, first, k, levels, deq);
    }

    SGCL_INLINE_HOT uint32_t sse(const uint8_t* a, size_t as, const uint8_t* b, size_t bs, int w, int h) noexcept {
        return sse_plain(a, as, b, bs, w, h);
    }
#endif

    // The settings of a frame from the quality: the quantizer index, the
    // filter level and the weights of the decisions
    struct Vp8Settings {
        int q = 0;            // the base quantizer index, 0..127
        int filter = 0;       // the loop filter level, 0..63
        int lambda16 = 0;     // the weight of a bit against squared error, ×256: 16x16 and chroma
        int lambda4 = 0;      // the same for 4x4 decisions
        int16_t y1[2], y2[2], uv[2];   // DC and AC quantizers
        bool i4 = true;       // 4x4 modes tried
    };

    // The quantizer index of a quality (0..100): calibrated so that the
    // pictures come out near the PSNR of cwebp at the same -q on photos
    inline int quality_to_q(int quality) noexcept {
        static const uint8_t table[11] = {105, 74, 63, 56, 49, 42, 35, 32, 23, 11, 0};
        const int i = quality / 10, f = quality % 10;
        if (i >= 10) {
            return table[10];
        }
        return (table[i] * (10 - f) + table[i + 1] * f + 5) / 10;
    }

    inline Vp8Settings settings_of(int quality) noexcept {
        Vp8Settings s;
        s.q = quality_to_q(quality);
        auto dc_q = [](int i) { return DcQuant[std::clamp(i, 0, 127)]; };
        auto ac_q = [](int i) { return AcQuant[std::clamp(i, 0, 127)]; };
        s.y1[0] = dc_q(s.q);
        s.y1[1] = ac_q(s.q);
        s.uv[0] = std::min<int16_t>(dc_q(s.q), 132);
        s.uv[1] = ac_q(s.q);
        s.y2[0] = int16_t(dc_q(s.q) * 2);
        s.y2[1] = std::max<int16_t>(int16_t(ac_q(s.q) * 155 / 100), 8);
        const int qa = s.y1[1];
        // the weights of a bit (in squared error) and the filter level,
        // found by the rate and PSNR of photos against cwebp's
        s.lambda16 = std::max(1, qa * qa * 3 / 256);
        s.lambda4 = std::max(1, qa * qa * 6 / 256);
        s.filter = std::clamp((qa * 7 + 8) / 16, 0, 63);
        return s;
    }

    // The encoder of a frame
    class Encoder {
    public:
        // The VP8 bitstream of w × h ARGB pixels (w, h up to 16383) at the
        // quality (0..100), appended to out
        void encode(const uint32_t* argb, uint32_t w, uint32_t h, int quality, std::vector<uint8_t>& out) {
            _w = w;
            _h = h;
            _mbw = (w + 15) / 16;
            _mbh = (h + 15) / 16;
            _s = settings_of(std::clamp(quality, 0, 100));
            _q_y1 = Quantizer4(_s.y1[0], _s.y1[1]);
            _q_y2 = Quantizer4(_s.y2[0], _s.y2[1]);
            _q_uv = Quantizer4(_s.uv[0], _s.uv[1]);
            _planes(argb);
            _analyse();
            _write(out);
        }

    private:
        struct Mb {
            uint8_t y_mode = DC_PRED;
            uint8_t uv_mode = DC_PRED;
            bool skip = false;
            uint8_t sub[16] = {};
            int16_t levels[25][16] = {};   // in zigzag order: 16 Y, 4 U, 4 V, Y2
        };

        // The picture as planes of whole macroblocks (the edges repeated):
        // the source; the reconstruction made as the decoder makes it
        void _planes(const uint32_t* argb) {
            _ys = size_t(_mbw) * 16;
            _cs = size_t(_mbw) * 8;
            _sy.assign(_ys * _mbh * 16, 0);
            _su.assign(_cs * _mbh * 8, 0);
            _sv.assign(_cs * _mbh * 8, 0);
            const uint32_t W = _mbw * 16, H = _mbh * 16;
            auto px = [&](uint32_t x, uint32_t y) {
                return argb[size_t(std::min(y, _h - 1)) * _w + std::min(x, _w - 1)];
            };
            for (uint32_t y = 0; y < H; ++y) {
                uint8_t* row = _sy.data() + size_t(y) * _ys;
                for (uint32_t x = 0; x < W; ++x) {
                    const uint32_t v = px(x, y);
                    const int r = (v >> 16) & 0xff, g = (v >> 8) & 0xff, b = v & 0xff;
                    row[x] = uint8_t((16829 * r + 33039 * g + 6416 * b + (16 << 16) + 32768) >> 16);
                }
            }
            for (uint32_t y = 0; y < H / 2; ++y) {
                for (uint32_t x = 0; x < W / 2; ++x) {
                    int r = 0, g = 0, b = 0;
                    for (uint32_t k = 0; k < 4; ++k) {
                        const uint32_t v = px(2 * x + (k & 1), 2 * y + (k >> 1));
                        r += (v >> 16) & 0xff;
                        g += (v >> 8) & 0xff;
                        b += v & 0xff;
                    }
                    const int u = (-9714 * r - 19071 * g + 28785 * b + (128 << 18) + (1 << 17)) >> 18;
                    const int vv = (28785 * r - 24103 * g - 4682 * b + (128 << 18) + (1 << 17)) >> 18;
                    _su[size_t(y) * _cs + x] = clamp255(u);
                    _sv[size_t(y) * _cs + x] = clamp255(vv);
                }
            }
            _ry.assign(_sy.size(), 0);
            _ru.assign(_su.size(), 0);
            _rv.assign(_sv.size(), 0);
        }

        // The cost of a block's tokens (§13) in 1/256 bits under the
        // probabilities of the analysis: levels in zigzag order from first;
        // each coefficient's token from the tables (_level_cost), the
        // branch of the end of block where it is coded
        uint32_t _block_cost(int plane, int ctx, int first, const int16_t* levels) const noexcept {
            int last = 15;
            while (last >= first && levels[last] == 0) {
                --last;
            }
            uint32_t c = 0;
            bool prev_zero = false;
            for (int i = first; i <= last; ++i) {
                const int band = Bands[i];
                if (!prev_zero) {
                    c += _eob_cost[plane][band][ctx][1];
                }
                const int v = levels[i] < 0 ? -levels[i] : levels[i];
                const uint16_t* lc = _level_cost[plane][band][ctx];
                c += v < LevelTable ? lc[v] : lc[LevelTable - 1] + _cat6_extra(v);
                ctx = v == 0 ? 0 : v > 1 ? 2 : 1;
                prev_zero = v == 0;
            }
            if (last < 15 && !prev_zero) {
                c += _eob_cost[plane][Bands[last + 1]][ctx][0];
            }
            return c;
        }

        static constexpr int LevelTable = 68;   // the levels whose cost is tabled: 0..66, and 67 for CAT6's path

        // The cost of a CAT6 level's extra bits past those of 67
        static uint32_t _cat6_extra(int v) noexcept {
            const uint8_t* pc = Pcats[5];
            const int extra = std::min(v, 2048 + 66) - CatBase[5];
            uint32_t c = 0;
            for (int k = 0; k < 11; ++k) {
                c += cost((extra >> (10 - k)) & 1, pc[k]);
            }
            // the table's entry of 67 holds the path and extra bits of 0
            uint32_t zero = 0;
            for (int k = 0; k < 11; ++k) {
                zero += cost(false, pc[k]);
            }
            return c - zero;
        }

        // The cost of a magnitude v >= 1 after the "not zero" branch
        static uint32_t _value_cost(int v, const uint8_t* p) noexcept {
            if (v == 1) {
                return cost(false, p[2]);
            }
            uint32_t c = cost(true, p[2]);
            if (v <= 4) {
                c += cost(false, p[3]);
                if (v == 2) {
                    return c + cost(false, p[4]);
                }
                return c + cost(true, p[4]) + cost(v == 4, p[5]);
            }
            c += cost(true, p[3]);
            int cat;
            if (v <= 10) {
                c += cost(false, p[6]);
                cat = v <= 6 ? 0 : 1;
                c += cost(cat == 1, p[7]);
            } else {
                c += cost(true, p[6]);
                if (v <= 34) {
                    c += cost(false, p[8]);
                    cat = v <= 18 ? 2 : 3;
                    c += cost(cat == 3, p[9]);
                } else {
                    c += cost(true, p[8]);
                    cat = v <= 66 ? 4 : 5;
                    c += cost(cat == 5, p[10]);
                }
            }
            const uint8_t* pc = Pcats[cat];
            int n = 0;
            while (pc[n]) {
                ++n;
            }
            const int extra = std::min(v, 2048 + 66) - CatBase[cat];
            for (int k = 0; k < n; ++k) {
                c += cost((extra >> (n - 1 - k)) & 1, pc[k]);
            }
            return c;
        }

        // The tables of the costs under _probs: a level at each place and
        // context (its token, its extra bits and its sign), the end of
        // block or not; the modes
        void _cost_tables() noexcept {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 8; ++j) {
                    for (int k = 0; k < 3; ++k) {
                        const uint8_t* p = _probs[i][j][k];
                        _eob_cost[i][j][k][0] = uint16_t(cost(false, p[0]));
                        _eob_cost[i][j][k][1] = uint16_t(cost(true, p[0]));
                        _level_cost[i][j][k][0] = uint16_t(cost(false, p[1]));
                        for (int v = 1; v < LevelTable; ++v) {
                            _level_cost[i][j][k][v] = uint16_t(cost(true, p[1]) + _value_cost(v, p) + 256);
                        }
                    }
                }
            }
            for (int a = 0; a < 10; ++a) {
                for (int l = 0; l < 10; ++l) {
                    for (int m = 0; m < 10; ++m) {
                        _bmode_cost[a][l][m] = uint16_t(_tree_cost(BmodeTree, KfBmodeProbs[a][l], m));
                    }
                }
            }
            for (int m = 0; m < 5; ++m) {
                _ymode_cost[m] = uint16_t(_tree_cost(KfYmodeTree, KfYmodeProbs, m));
            }
            for (int m = 0; m < 4; ++m) {
                _uvmode_cost[m] = uint16_t(_tree_cost(UvModeTree, KfUvModeProbs, m));
            }
        }

        // A block of 16 coefficients quantized into levels (zigzag order),
        // dequantized back into deq (raster); the index of the last level
        // not zero, -1 for none. dc_bias and ac_bias in 1/256 of the
        // quantizer: how far below a half the rounding goes
        // The edges of a macroblock's prediction as the decoder has them:
        // above (P, 16, 4 above-right) and left of the luma, A and L of each
        // chroma plane
        struct Edges {
            uint8_t above[21];
            uint8_t left[16];
            uint8_t ca[2][9];
            uint8_t cl[2][8];
        };

        void _edges(uint32_t mx, uint32_t my, Edges& e) const noexcept {
            const bool top = my == 0, leftmost = mx == 0;
            const uint8_t* Y = _ry.data() + size_t(my) * 16 * _ys + size_t(mx) * 16;
            if (top) {
                std::memset(e.above, 127, sizeof e.above);
            } else {
                std::memcpy(e.above + 1, Y - _ys, 16);
                e.above[0] = leftmost ? 129 : Y[-ptrdiff_t(_ys) - 1];
                if (mx + 1 < _mbw) {
                    std::memcpy(e.above + 17, Y - _ys + 16, 4);
                } else {
                    std::memset(e.above + 17, e.above[16], 4);
                }
            }
            for (int r = 0; r < 16; ++r) {
                e.left[r] = leftmost ? 129 : Y[ptrdiff_t(r * _ys) - 1];
            }
            for (int c = 0; c < 2; ++c) {
                const uint8_t* C = (c == 0 ? _ru.data() : _rv.data()) + size_t(my) * 8 * _cs + size_t(mx) * 8;
                if (top) {
                    std::memset(e.ca[c], 127, 9);
                } else {
                    std::memcpy(e.ca[c] + 1, C - _cs, 8);
                    e.ca[c][0] = leftmost ? 129 : C[-ptrdiff_t(_cs) - 1];
                }
                for (int r = 0; r < 8; ++r) {
                    e.cl[c][r] = leftmost ? 129 : C[ptrdiff_t(r * _cs) - 1];
                }
            }
        }

        // The dequantized block added to the prediction: what the decoder
        // will have
        static void _add(const int16_t* deq, int last, uint8_t* dst, size_t stride) noexcept {
            if (last < 0) {
                return;
            }
            if (last == 0) {
                dc_only_add(deq[0], dst, ptrdiff_t(stride));
            } else {
                inverse_dct_add(deq, dst, ptrdiff_t(stride));
            }
        }

        // The 16x16 luma of mode `mode`: levels of the 16 Y blocks (from 1)
        // and of Y2 into m, the reconstruction into rec (stride 16), the
        // score (distortion ×256 plus λ times bits)
        uint64_t _try16(uint8_t mode, const Edges& e, bool top, bool leftmost, const uint8_t* src, uint8_t* rec, Mb& m, const uint8_t* anz,
                        const uint8_t* lnz) const noexcept {
            Decoder::_predict_block<16>(mode, rec, 16, e.above + 1, e.left, top, leftmost);
            int16_t coeffs[16][16];
            int dc[16];
            for (int b = 0; b < 16; ++b) {
                const int r = b >> 2, c = b & 3;
                residue_dct(src + r * 4 * _ys + c * 4, _ys, rec + r * 64 + c * 4, 16, coeffs[b]);
                dc[b] = coeffs[b][0];
            }
            int16_t y2[16], y2deq[16];
            forward_wht(dc, y2);
            const int y2last = quantize(y2, 0, _q_y2, m.levels[24], y2deq);
            int16_t dcs[16];
            inverse_wht(y2deq, dcs);
            uint32_t bits = _ymode_cost[mode];
            bits += _block_cost(1, anz[8] + lnz[8], 0, m.levels[24]);
            (void)y2last;
            uint8_t a[4] = {anz[0], anz[1], anz[2], anz[3]}, l[4] = {lnz[0], lnz[1], lnz[2], lnz[3]};
            for (int b = 0; b < 16; ++b) {
                const int r = b >> 2, c = b & 3;
                int16_t deq[16];
                int last = quantize(coeffs[b], 1, _q_y1, m.levels[b], deq);
                deq[0] = dcs[b];
                bits += _block_cost(0, a[c] + l[r], 1, m.levels[b]);
                a[c] = l[r] = last >= 1;
                if (last < 1 && deq[0] != 0) {
                    last = 0;
                } else if (last < 1) {
                    last = -1;
                }
                _add(deq, last, rec + r * 64 + c * 4, 16);
            }
            const uint32_t d = sse(src, _ys, rec, 16, 16, 16);
            return uint64_t(d) * 256 + uint64_t(bits) * uint64_t(_s.lambda16);
        }

        // The cost of a leaf of a tree under probs
        static uint32_t _tree_cost(const int8_t* t, const uint8_t* probs, int value) noexcept {
            const int target = -value;
            uint32_t found = 0;
            auto walk = [&](auto&& self, int node, uint32_t acc) -> bool {
                for (int b = 0; b < 2; ++b) {
                    const int next = t[node + b];
                    const uint32_t c = acc + cost(b != 0, probs[node >> 1]);
                    if (next <= 0) {
                        if (next == target) {
                            found = c;
                            return true;
                        }
                    } else if (self(self, next, c)) {
                        return true;
                    }
                }
                return false;
            };
            walk(walk, 0, 0);
            return found;
        }

        // The 4x4 luma: each subblock's mode chosen in turn, its
        // reconstruction kept for the next
        uint64_t _try4(const uint8_t* above_modes, const uint8_t* left_modes, const Edges& e, const uint8_t* src, uint8_t* rec, Mb& m,
                       const uint8_t* anz, const uint8_t* lnz) const noexcept {
            uint8_t am[4] = {above_modes[0], above_modes[1], above_modes[2], above_modes[3]};
            uint8_t lm[4] = {left_modes[0], left_modes[1], left_modes[2], left_modes[3]};
            uint8_t a[4] = {anz[0], anz[1], anz[2], anz[3]}, l[4] = {lnz[0], lnz[1], lnz[2], lnz[3]};
            // the subblocks' modes chosen by λ4; the macroblock's score, to
            // set against the 16x16 modes', by λ16 like theirs
            uint64_t distortion = 0;
            uint64_t bits_total = _ymode_cost[B_PRED];
            for (int i = 0; i < 16; ++i) {
                const int r = i >> 2, c = i & 3;
                uint8_t* B = rec + r * 64 + c * 4;
                uint8_t A[9];
                uint8_t L[4];
                if (r == 0) {
                    std::memcpy(A, e.above + c * 4, 9);
                } else {
                    A[0] = c == 0 ? e.left[r * 4 - 1] : B[-16 - 1];
                    std::memcpy(A + 1, B - 16, 4);
                    if (c < 3) {
                        std::memcpy(A + 5, B - 16 + 4, 4);
                    } else {
                        std::memcpy(A + 5, e.above + 17, 4);
                    }
                }
                for (int k = 0; k < 4; ++k) {
                    L[k] = c == 0 ? e.left[r * 4 + k] : B[k * 16 - 1];
                }
                const uint8_t* S = src + r * 4 * _ys + c * 4;
                uint64_t best = UINT64_MAX;
                uint8_t pick = B_DC_PRED;
                int16_t best_levels[16];
                uint8_t best_pixels[16];
                int best_last = -1;
                uint32_t best_d = 0, best_bits = 0;
                for (uint8_t mode = 0; mode < 10; ++mode) {
                    uint8_t pred[16];
                    Decoder::_predict_sub(mode, pred, 4, A + 1, L);
                    int16_t coeffs[16], levels[16], deq[16];
                    residue_dct(S, _ys, pred, 4, coeffs);
                    const int last = quantize(coeffs, 0, _q_y1, levels, deq);
                    _add(deq, last, pred, 4);
                    const uint32_t d = sse(S, _ys, pred, 4, 4, 4);
                    const uint32_t bits = _bmode_cost[am[c]][lm[r]][mode] + _block_cost(3, a[c] + l[r], 0, levels);
                    const uint64_t score = uint64_t(d) * 256 + uint64_t(bits) * uint64_t(_s.lambda4);
                    if (score < best) {
                        best = score;
                        pick = mode;
                        std::memcpy(best_levels, levels, sizeof levels);
                        std::memcpy(best_pixels, pred, 16);
                        best_last = last;
                        best_d = d;
                        best_bits = bits;
                    }
                }
                distortion += best_d;
                bits_total += best_bits;
                m.sub[i] = pick;
                std::memcpy(m.levels[i], best_levels, sizeof best_levels);
                for (int k = 0; k < 4; ++k) {
                    std::memcpy(B + k * 16, best_pixels + 4 * k, 4);
                }
                am[c] = lm[r] = pick;
                a[c] = l[r] = best_last >= 0;
            }
            return distortion * 256 + bits_total * uint64_t(_s.lambda16);
        }

        // The chroma: the mode of least score for U and V together
        uint64_t _try_uv(uint8_t mode, const Edges& e, bool top, bool leftmost, const uint8_t* su, const uint8_t* sv, uint8_t* ru, uint8_t* rv, Mb& m,
                         const uint8_t* anz, const uint8_t* lnz) const noexcept {
            uint32_t bits = _uvmode_cost[mode];
            uint32_t d = 0;
            for (int c = 0; c < 2; ++c) {
                const uint8_t* S = c == 0 ? su : sv;
                uint8_t* R = c == 0 ? ru : rv;
                Decoder::_predict_block<8>(mode, R, 8, e.ca[c] + 1, e.cl[c], top, leftmost);
                uint8_t a[2] = {anz[4 + c * 2], anz[5 + c * 2]}, l[2] = {lnz[4 + c * 2], lnz[5 + c * 2]};
                for (int b = 0; b < 4; ++b) {
                    const int r = b >> 1, cc = b & 1;
                    int16_t coeffs[16], deq[16];
                    residue_dct(S + r * 4 * _cs + cc * 4, _cs, R + r * 32 + cc * 4, 8, coeffs);
                    int16_t* levels = m.levels[16 + c * 4 + b];
                    const int last = quantize(coeffs, 0, _q_uv, levels, deq);
                    bits += _block_cost(2, a[cc] + l[r], 0, levels);
                    a[cc] = l[r] = last >= 0;
                    _add(deq, last, R + r * 32 + cc * 4, 8);
                }
                d += sse(S, _cs, R, 8, 8, 8);
            }
            return uint64_t(d) * 256 + uint64_t(bits) * uint64_t(_s.lambda16);
        }

        // Every macroblock's modes and levels decided, the reconstruction
        // made
        void _analyse() {
            std::memcpy(_probs, DefaultCoeffProbs, sizeof _probs);
            _cost_tables();
            _mbs.assign(size_t(_mbw) * _mbh, Mb{});
            std::vector<uint8_t> above_modes(size_t(_mbw) * 4, B_DC_PRED);
            std::vector<uint8_t> above_nz(size_t(_mbw) * 9, 0);
            for (uint32_t my = 0; my < _mbh; ++my) {
                uint8_t left_modes[4] = {B_DC_PRED, B_DC_PRED, B_DC_PRED, B_DC_PRED};
                uint8_t left_nz[9] = {};
                for (uint32_t mx = 0; mx < _mbw; ++mx) {
                    Mb& m = _mbs[size_t(my) * _mbw + mx];
                    Edges e;
                    _edges(mx, my, e);
                    const bool top = my == 0, leftmost = mx == 0;
                    const uint8_t* src = _sy.data() + size_t(my) * 16 * _ys + size_t(mx) * 16;
                    uint8_t* anz = above_nz.data() + size_t(mx) * 9;
                    // luma: the best 16x16 mode, then 4x4
                    uint64_t best = UINT64_MAX;
                    Mb trial;
                    uint8_t rec[256], best_rec[256];
                    for (uint8_t mode = DC_PRED; mode <= TM_PRED; ++mode) {
                        const uint64_t s = _try16(mode, e, top, leftmost, src, rec, trial, anz, left_nz);
                        if (s < best) {
                            best = s;
                            m.y_mode = mode;
                            std::memcpy(m.levels, trial.levels, sizeof m.levels);
                            std::memcpy(best_rec, rec, 256);
                        }
                    }
                    if (_s.i4) {
                        const uint64_t s = _try4(above_modes.data() + size_t(mx) * 4, left_modes, e, src, rec, trial, anz, left_nz);
                        if (s < best) {
                            best = s;
                            m.y_mode = B_PRED;
                            std::memcpy(m.sub, trial.sub, 16);
                            std::memcpy(m.levels, trial.levels, 16 * sizeof m.levels[0]);
                            std::memset(m.levels[24], 0, sizeof m.levels[24]);
                            std::memcpy(best_rec, rec, 256);
                        }
                    }
                    uint8_t* Y = _ry.data() + size_t(my) * 16 * _ys + size_t(mx) * 16;
                    for (int r = 0; r < 16; ++r) {
                        std::memcpy(Y + r * _ys, best_rec + r * 16, 16);
                    }
                    // chroma
                    const uint8_t* su = _su.data() + size_t(my) * 8 * _cs + size_t(mx) * 8;
                    const uint8_t* sv = _sv.data() + size_t(my) * 8 * _cs + size_t(mx) * 8;
                    uint8_t ru[64], rv[64], best_u[64], best_v[64];
                    uint64_t best_uv = UINT64_MAX;
                    int16_t uv_levels[8][16];
                    for (uint8_t mode = DC_PRED; mode <= TM_PRED; ++mode) {
                        const uint64_t s = _try_uv(mode, e, top, leftmost, su, sv, ru, rv, trial, anz, left_nz);
                        if (s < best_uv) {
                            best_uv = s;
                            m.uv_mode = mode;
                            std::memcpy(uv_levels, trial.levels[16], sizeof uv_levels);
                            std::memcpy(best_u, ru, 64);
                            std::memcpy(best_v, rv, 64);
                        }
                    }
                    std::memcpy(m.levels[16], uv_levels, sizeof uv_levels);
                    uint8_t* U = _ru.data() + size_t(my) * 8 * _cs + size_t(mx) * 8;
                    uint8_t* V = _rv.data() + size_t(my) * 8 * _cs + size_t(mx) * 8;
                    for (int r = 0; r < 8; ++r) {
                        std::memcpy(U + r * _cs, best_u + r * 8, 8);
                        std::memcpy(V + r * _cs, best_v + r * 8, 8);
                    }
                    // the contexts as the decoder carries them
                    _contexts(m, anz, left_nz, above_modes.data() + size_t(mx) * 4, left_modes);
                }
            }
        }

        // After a macroblock: the non-zero contexts and the subblock modes
        // as the decoder carries them; the skip flag
        void _contexts(Mb& m, uint8_t* anz, uint8_t* lnz, uint8_t* am, uint8_t* lm) noexcept {
            bool any = false;
            for (int b = 0; b < 25 && !any; ++b) {
                for (int i = 0; i < 16; ++i) {
                    if (m.levels[b][i]) {
                        any = true;
                        break;
                    }
                }
            }
            m.skip = !any;
            const bool i16 = m.y_mode != B_PRED;
            if (m.skip) {
                std::memset(anz, 0, 8);
                std::memset(lnz, 0, 8);
                if (i16) {
                    anz[8] = lnz[8] = 0;
                }
            } else {
                auto nz = [&](int b, int first) {
                    for (int i = first; i < 16; ++i) {
                        if (m.levels[b][i]) {
                            return uint8_t(1);
                        }
                    }
                    return uint8_t(0);
                };
                if (i16) {
                    anz[8] = lnz[8] = nz(24, 0);
                }
                for (int y = 0; y < 4; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        anz[x] = lnz[y] = nz(y * 4 + x, i16 ? 1 : 0);
                    }
                }
                for (int c = 0; c < 2; ++c) {
                    for (int y = 0; y < 2; ++y) {
                        for (int x = 0; x < 2; ++x) {
                            anz[4 + c * 2 + x] = lnz[4 + c * 2 + y] = nz(16 + c * 4 + y * 2 + x, 0);
                        }
                    }
                }
            }
            if (i16) {
                const uint8_t b_mode = m.y_mode == DC_PRED ? B_DC_PRED : m.y_mode == V_PRED ? B_VE_PRED : m.y_mode == H_PRED ? B_HE_PRED : B_TM_PRED;
                std::memset(am, b_mode, 4);
                std::memset(lm, b_mode, 4);
            } else {
                for (int i = 0; i < 16; ++i) {
                    am[i & 3] = m.sub[i];
                    lm[i >> 2] = m.sub[i];
                }
            }
        }

        // The tokens of a block (§13), the mirror of the decoder's _block;
        // stats counts the branches taken instead of writing them
        template<class Put>
        static void _tokens(int plane, int ctx, int first, const int16_t* levels, const uint8_t (*probs)[8][3][11], Put&& put) noexcept {
            int last = 15;
            while (last >= first && levels[last] == 0) {
                --last;
            }
            bool prev_zero = false;
            for (int i = first; i <= last; ++i) {
                const int band = Bands[i];
                const uint8_t* p = probs[plane][band][ctx];
                if (!prev_zero) {
                    put(plane, band, ctx, 0, true, p[0]);
                }
                const int v = levels[i] < 0 ? -levels[i] : levels[i];
                if (v == 0) {
                    put(plane, band, ctx, 1, false, p[1]);
                    ctx = 0;
                    prev_zero = true;
                    continue;
                }
                put(plane, band, ctx, 1, true, p[1]);
                if (v == 1) {
                    put(plane, band, ctx, 2, false, p[2]);
                } else {
                    put(plane, band, ctx, 2, true, p[2]);
                    if (v <= 4) {
                        put(plane, band, ctx, 3, false, p[3]);
                        if (v == 2) {
                            put(plane, band, ctx, 4, false, p[4]);
                        } else {
                            put(plane, band, ctx, 4, true, p[4]);
                            put(plane, band, ctx, 5, v == 4, p[5]);
                        }
                    } else {
                        put(plane, band, ctx, 3, true, p[3]);
                        int cat;
                        if (v <= 10) {
                            put(plane, band, ctx, 6, false, p[6]);
                            cat = v <= 6 ? 0 : 1;
                            put(plane, band, ctx, 7, cat == 1, p[7]);
                        } else {
                            put(plane, band, ctx, 6, true, p[6]);
                            if (v <= 34) {
                                put(plane, band, ctx, 8, false, p[8]);
                                cat = v <= 18 ? 2 : 3;
                                put(plane, band, ctx, 9, cat == 3, p[9]);
                            } else {
                                put(plane, band, ctx, 8, true, p[8]);
                                cat = v <= 66 ? 4 : 5;
                                put(plane, band, ctx, 10, cat == 5, p[10]);
                            }
                        }
                        const uint8_t* pc = Pcats[cat];
                        int n = 0;
                        while (pc[n]) {
                            ++n;
                        }
                        const int extra = v - CatBase[cat];
                        for (int k = 0; k < n; ++k) {
                            put(-1, 0, 0, 0, (extra >> (n - 1 - k)) & 1, pc[k]);
                        }
                    }
                }
                put(-1, 0, 0, 0, levels[i] < 0, 128);
                ctx = v > 1 ? 2 : 1;
                prev_zero = false;
            }
            if (last < 15) {
                const int band = Bands[last + 1];
                if (!prev_zero) {
                    put(plane, band, ctx, 0, false, probs[plane][band][ctx][0]);
                }
            }
        }

        // Every macroblock's tokens in order, through put
        template<class Put>
        void _all_tokens(Put&& put) const noexcept {
            std::vector<uint8_t> above_nz(size_t(_mbw) * 9, 0);
            for (uint32_t my = 0; my < _mbh; ++my) {
                uint8_t lnz[9] = {};
                for (uint32_t mx = 0; mx < _mbw; ++mx) {
                    const Mb& m = _mbs[size_t(my) * _mbw + mx];
                    uint8_t* anz = above_nz.data() + size_t(mx) * 9;
                    const bool i16 = m.y_mode != B_PRED;
                    if (m.skip) {
                        std::memset(anz, 0, 8);
                        std::memset(lnz, 0, 8);
                        if (i16) {
                            anz[8] = lnz[8] = 0;
                        }
                        continue;
                    }
                    auto block = [&](int plane, int ctx, int first, const int16_t* levels) {
                        _tokens(plane, ctx, first, levels, _probs, put);
                        for (int i = first; i < 16; ++i) {
                            if (levels[i]) {
                                return uint8_t(1);
                            }
                        }
                        return uint8_t(0);
                    };
                    int first = 0, plane = 3;
                    if (i16) {
                        anz[8] = lnz[8] = block(1, anz[8] + lnz[8], 0, m.levels[24]);
                        first = 1;
                        plane = 0;
                    }
                    for (int y = 0; y < 4; ++y) {
                        for (int x = 0; x < 4; ++x) {
                            anz[x] = lnz[y] = block(plane, anz[x] + lnz[y], first, m.levels[y * 4 + x]);
                        }
                    }
                    for (int c = 0; c < 2; ++c) {
                        for (int y = 0; y < 2; ++y) {
                            for (int x = 0; x < 2; ++x) {
                                const int k = 4 + c * 2;
                                anz[k + x] = lnz[k + y] = block(2, anz[k + x] + lnz[k + y], 0, m.levels[16 + c * 4 + y * 2 + x]);
                            }
                        }
                    }
                }
            }
        }

        // The probabilities of the frame: each branch's from what it took,
        // where writing it costs less than it saves
        void _update_probs(bool (&updated)[4][8][3][11]) noexcept {
            uint32_t counts[4][8][3][11][2] = {};
            _all_tokens([&](int plane, int band, int ctx, int node, bool bit, unsigned) {
                if (plane >= 0) {
                    ++counts[plane][band][ctx][node][bit];
                }
            });
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 8; ++j) {
                    for (int k = 0; k < 3; ++k) {
                        for (int l = 0; l < 11; ++l) {
                            updated[i][j][k][l] = false;
                            const uint32_t c0 = counts[i][j][k][l][0], c1 = counts[i][j][k][l][1];
                            const uint8_t old = DefaultCoeffProbs[i][j][k][l];
                            const uint8_t up = CoeffUpdateProbs[i][j][k][l];
                            if (c0 + c1 == 0) {
                                continue;
                            }
                            const int p = std::clamp(int((uint64_t(c0) * 256 + (c0 + c1) / 2) / (c0 + c1)), 1, 255);
                            const int64_t before = int64_t(c0) * cost(false, old) + int64_t(c1) * cost(true, old) + cost(false, up);
                            const int64_t after = int64_t(c0) * cost(false, unsigned(p)) + int64_t(c1) * cost(true, unsigned(p)) + cost(true, up) + 8 * 256;
                            if (after < before) {
                                _probs[i][j][k][l] = uint8_t(p);
                                updated[i][j][k][l] = true;
                            }
                        }
                    }
                }
            }
        }

        void _write(std::vector<uint8_t>& out) {
            bool updated[4][8][3][11];
            _update_probs(updated);
            // the skip probability from the macroblocks
            uint32_t skipped = 0;
            for (const Mb& m : _mbs) {
                skipped += m.skip;
            }
            const uint32_t total = uint32_t(_mbs.size());
            const bool use_skip = skipped > 0;
            const unsigned skip_prob = use_skip ? unsigned(std::clamp(int((uint64_t(total - skipped) * 256) / total), 1, 254)) : 0;
            BoolEncoder h;
            h.literal(0, 1);   // color space
            h.literal(0, 1);   // clamping type
            h.literal(0, 1);   // no segmentation
            h.literal(0, 1);   // the normal loop filter
            h.literal(uint32_t(_s.filter), 6);
            h.literal(0, 3);   // sharpness
            h.literal(0, 1);   // no deltas of the filter level
            h.literal(0, 2);   // one token partition
            h.literal(uint32_t(_s.q), 7);
            for (int i = 0; i < 5; ++i) {
                h.literal(0, 1);   // no delta of the quantizer index
            }
            h.literal(0, 1);   // refresh_entropy_probs
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 8; ++j) {
                    for (int k = 0; k < 3; ++k) {
                        for (int l = 0; l < 11; ++l) {
                            h.put(updated[i][j][k][l], CoeffUpdateProbs[i][j][k][l]);
                            if (updated[i][j][k][l]) {
                                h.literal(_probs[i][j][k][l], 8);
                            }
                        }
                    }
                }
            }
            h.literal(use_skip ? 1 : 0, 1);
            if (use_skip) {
                h.literal(skip_prob, 8);
            }
            // the modes
            std::vector<uint8_t> above_modes(size_t(_mbw) * 4, B_DC_PRED);
            for (uint32_t my = 0; my < _mbh; ++my) {
                uint8_t left_modes[4] = {B_DC_PRED, B_DC_PRED, B_DC_PRED, B_DC_PRED};
                for (uint32_t mx = 0; mx < _mbw; ++mx) {
                    const Mb& m = _mbs[size_t(my) * _mbw + mx];
                    uint8_t* am = above_modes.data() + size_t(mx) * 4;
                    if (use_skip) {
                        h.put(m.skip, skip_prob);
                    }
                    h.tree(KfYmodeTree, KfYmodeProbs, m.y_mode);
                    if (m.y_mode == B_PRED) {
                        for (int i = 0; i < 16; ++i) {
                            h.tree(BmodeTree, KfBmodeProbs[am[i & 3]][left_modes[i >> 2]], m.sub[i]);
                            am[i & 3] = left_modes[i >> 2] = m.sub[i];
                        }
                    } else {
                        const uint8_t b_mode = m.y_mode == DC_PRED ? B_DC_PRED : m.y_mode == V_PRED ? B_VE_PRED : m.y_mode == H_PRED ? B_HE_PRED : B_TM_PRED;
                        std::memset(am, b_mode, 4);
                        std::memset(left_modes, b_mode, 4);
                    }
                    h.tree(UvModeTree, KfUvModeProbs, m.uv_mode);
                }
            }
            h.finish();
            // the tokens
            BoolEncoder t;
            _all_tokens([&](int, int, int, int, bool bit, unsigned prob) { t.put(bit, prob); });
            t.finish();
            // the frame tag, the start code and the size, then the partitions
            const uint32_t first = uint32_t(h.out.size());
            const uint32_t tag = 0 /* key frame */ | 0 << 1 /* version */ | 1 << 4 /* shown */ | first << 5;
            const uint8_t head[10] = {uint8_t(tag), uint8_t(tag >> 8), uint8_t(tag >> 16), 0x9d, 0x01, 0x2a,
                                      uint8_t(_w), uint8_t(_w >> 8), uint8_t(_h), uint8_t(_h >> 8)};
            out.insert(out.end(), head, head + 10);
            out.insert(out.end(), h.out.begin(), h.out.end());
            out.insert(out.end(), t.out.begin(), t.out.end());
        }

        uint32_t _w = 0, _h = 0, _mbw = 0, _mbh = 0;
        size_t _ys = 0, _cs = 0;
        Vp8Settings _s;
        Quantizer4 _q_y1{4, 4}, _q_y2{8, 8}, _q_uv{4, 4};
        std::vector<uint8_t> _sy, _su, _sv, _ry, _ru, _rv;
        std::vector<Mb> _mbs;
        uint8_t _probs[4][8][3][11];
        uint16_t _level_cost[4][8][3][LevelTable];
        uint16_t _eob_cost[4][8][3][2];
        uint16_t _bmode_cost[10][10][10];
        uint16_t _ymode_cost[5];
        uint16_t _uvmode_cost[4];
    };
}

namespace sgcl::codec::detail {
    using Vp8Encoder = vp8::Encoder;
}
