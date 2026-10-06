//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "simd.h"
#include "../../core/detail/bytes.h"
#include "../../core/detail/os.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

// The eight transforms of EXIF's orientation over a buffer of w × h pixels
// of B bytes, rows without padding, into another: 1 a copy, 2 mirrored left
// to right, 3 turned by 180 degrees, 4 mirrored top to bottom, 5 transposed
// (output (x, y) the source's (y, x)), 6 turned clockwise, 7 transposed
// across the other diagonal, 8 turned counterclockwise; the sides swapped
// for 5 to 8. image::oriented, flipped and rotated are made of them.
//
// The plain road: each output pixel read from the source by its offset.
// The vector road (NEON, on arm64; elsewhere the plain one): whole rows
// copied for 1 and 4; a row's pixels reversed in registers for 2 and 3;
// the quarter turns 5 to 8 by blocks of 8 × 8 pixels (4 × 4 of 4 bytes, 2 ×
// 2 of 8) transposed in registers, the blocks in tiles of 64 for the cache,
// the bands the blocks leave pixel by pixel. Each kernel is kept only where
// it measured faster than the plain road (pixels of 6 bytes, and the mirror
// of 8, stay on it); both give the same bytes, which the tests hold.
namespace sgcl::codec::detail::orient {
    // The source offset of output pixel (0, 0) and the steps of a pixel
    // to the right and a row down, in bytes
    struct Steps {
        ptrdiff_t base, dx, dy;
    };

    SGCL_INLINE_HOT Steps steps_of(unsigned o, size_t w, size_t h, size_t B) noexcept {
        const ptrdiff_t s = ptrdiff_t(w * B), b = ptrdiff_t(B);
        const ptrdiff_t W = ptrdiff_t(w), H = ptrdiff_t(h);
        switch (o) {
            case 2: return {(W - 1) * b, -b, s};
            case 3: return {(W - 1) * b + (H - 1) * s, -b, -s};
            case 4: return {(H - 1) * s, b, -s};
            case 5: return {0, s, b};
            case 6: return {(H - 1) * s, -s, b};
            case 7: return {(W - 1) * b + (H - 1) * s, -s, -b};
            case 8: return {(W - 1) * b, s, -b};
            default: return {0, b, s};
        }
    }

    // The plain road: pixel by pixel
    template<unsigned B>
    inline void plain(const uint8_t* src, size_t w, size_t h, unsigned o, uint8_t* dst) noexcept {
        const Steps st = steps_of(o, w, h, B);
        const bool swap = o >= 5;
        const size_t ow = swap ? h : w, oh = swap ? w : h;
        for (size_t y = 0; y < oh; ++y) {
            ptrdiff_t at = st.base + ptrdiff_t(y) * st.dy;   // an offset, not a pointer: the last step lands outside
            uint8_t* q = dst + y * ow * B;
            for (size_t x = 0; x < ow; ++x) {
                std::memcpy(q, src + at, B);
                at += st.dx;
                q += B;
            }
        }
    }

#if defined(SGCL_CODEC_NEON)
    // A row's n pixels in the reverse order
    template<unsigned B>
    inline void reverse_row(const uint8_t* src, uint8_t* dst, size_t n) noexcept {
        size_t i = 0;
        if constexpr (B == 1) {
            for (; i + 16 <= n; i += 16) {
                const uint8x16_t v = vrev64q_u8(vld1q_u8(src + n - i - 16));
                vst1q_u8(dst + i, vcombine_u8(vget_high_u8(v), vget_low_u8(v)));
            }
        } else if constexpr (B == 2) {
            for (; i + 8 <= n; i += 8) {
                const uint8x16_t raw = vld1q_u8(src + (n - i - 8) * 2);
                const uint16x8_t v = vrev64q_u16(vreinterpretq_u16_u8(raw));
                vst1q_u8(dst + i * 2, vreinterpretq_u8_u16(vcombine_u16(vget_high_u16(v), vget_low_u16(v))));
            }
        } else if constexpr (B == 3) {
            for (; i + 16 <= n; i += 16) {
                uint8x16x3_t v = vld3q_u8(src + (n - i - 16) * 3);
                for (int k = 0; k < 3; ++k) {
                    const uint8x16_t c = vrev64q_u8(v.val[k]);
                    v.val[k] = vcombine_u8(vget_high_u8(c), vget_low_u8(c));
                }
                vst3q_u8(dst + i * 3, v);
            }
        } else if constexpr (B == 4) {
            for (; i + 4 <= n; i += 4) {
                const uint8x16_t raw = vld1q_u8(src + (n - i - 4) * 4);
                const uint32x4_t v = vrev64q_u32(vreinterpretq_u32_u8(raw));
                vst1q_u8(dst + i * 4, vreinterpretq_u8_u32(vcombine_u32(vget_high_u32(v), vget_low_u32(v))));
            }
        }
        for (; i < n; ++i) {
            std::memcpy(dst + i * B, src + (n - 1 - i) * B, B);
        }
    }

    // 8 × 8 bytes transposed: r[j] row j in, row j of the transpose out
    inline void transpose8x8(uint8x8_t* r) noexcept {
        const uint8x8x2_t a0 = vtrn_u8(r[0], r[1]), a1 = vtrn_u8(r[2], r[3]), a2 = vtrn_u8(r[4], r[5]), a3 = vtrn_u8(r[6], r[7]);
        const uint16x4x2_t b0 = vtrn_u16(vreinterpret_u16_u8(a0.val[0]), vreinterpret_u16_u8(a1.val[0]));
        const uint16x4x2_t b1 = vtrn_u16(vreinterpret_u16_u8(a0.val[1]), vreinterpret_u16_u8(a1.val[1]));
        const uint16x4x2_t b2 = vtrn_u16(vreinterpret_u16_u8(a2.val[0]), vreinterpret_u16_u8(a3.val[0]));
        const uint16x4x2_t b3 = vtrn_u16(vreinterpret_u16_u8(a2.val[1]), vreinterpret_u16_u8(a3.val[1]));
        const uint32x2x2_t c0 = vtrn_u32(vreinterpret_u32_u16(b0.val[0]), vreinterpret_u32_u16(b2.val[0]));
        const uint32x2x2_t c1 = vtrn_u32(vreinterpret_u32_u16(b1.val[0]), vreinterpret_u32_u16(b3.val[0]));
        const uint32x2x2_t c2 = vtrn_u32(vreinterpret_u32_u16(b0.val[1]), vreinterpret_u32_u16(b2.val[1]));
        const uint32x2x2_t c3 = vtrn_u32(vreinterpret_u32_u16(b1.val[1]), vreinterpret_u32_u16(b3.val[1]));
        r[0] = vreinterpret_u8_u32(c0.val[0]);
        r[1] = vreinterpret_u8_u32(c1.val[0]);
        r[2] = vreinterpret_u8_u32(c2.val[0]);
        r[3] = vreinterpret_u8_u32(c3.val[0]);
        r[4] = vreinterpret_u8_u32(c0.val[1]);
        r[5] = vreinterpret_u8_u32(c1.val[1]);
        r[6] = vreinterpret_u8_u32(c2.val[1]);
        r[7] = vreinterpret_u8_u32(c3.val[1]);
    }

    // The quarter turns (o 5 to 8) by blocks of K × K output pixels, each
    // from K source rows of K pixels: output row i of a block is source
    // column c + i (c + K − 1 − i when the columns run backwards, 7 and 8),
    // output column j source row j of the block's rows (counted from the
    // bottom for 6 and 7)
    template<unsigned B>
    inline void turned(const uint8_t* src, size_t w, size_t h, unsigned o, uint8_t* dst) noexcept {
        constexpr size_t K = B == 4 ? 4 : B == 8 ? 2 : 8;
        constexpr size_t T = 64;
        const size_t ow = h, oh = w;
        const bool col_rev = o == 7 || o == 8;
        const bool row_rev = o == 6 || o == 7;
        const size_t bw = ow / K * K, bh = oh / K * K;
        for (size_t ty = 0; ty < bh; ty += T) {
            for (size_t tx = 0; tx < bw; tx += T) {
                const size_t ye = std::min(bh, ty + T), xe = std::min(bw, tx + T);
                for (size_t oy = ty; oy < ye; oy += K) {
                    const size_t c = col_rev ? w - oy - K : oy;
                    for (size_t ox = tx; ox < xe; ox += K) {
                        const uint8_t* rows[K];
                        for (size_t j = 0; j < K; ++j) {
                            const size_t sy = row_rev ? h - 1 - (ox + j) : ox + j;
                            rows[j] = src + (sy * w + c) * B;
                        }
                        uint8_t* out = dst + (oy * ow + ox) * B;
                        if constexpr (B == 1) {
                            uint8x8_t r[8];
                            for (size_t j = 0; j < 8; ++j) {
                                r[j] = vld1_u8(rows[j]);
                            }
                            transpose8x8(r);
                            for (size_t i = 0; i < 8; ++i) {
                                vst1_u8(out + i * ow, r[col_rev ? 7 - i : i]);
                            }
                        } else if constexpr (B == 2) {
                            uint8x8_t lo[8], hi[8];
                            for (size_t j = 0; j < 8; ++j) {
                                const uint8x8x2_t v = vld2_u8(rows[j]);
                                lo[j] = v.val[0];
                                hi[j] = v.val[1];
                            }
                            transpose8x8(lo);
                            transpose8x8(hi);
                            for (size_t i = 0; i < 8; ++i) {
                                const size_t k = col_rev ? 7 - i : i;
                                vst2_u8(out + i * ow * 2, (uint8x8x2_t{{lo[k], hi[k]}}));
                            }
                        } else if constexpr (B == 3) {
                            uint8x8_t p[3][8];
                            for (size_t j = 0; j < 8; ++j) {
                                const uint8x8x3_t v = vld3_u8(rows[j]);
                                p[0][j] = v.val[0];
                                p[1][j] = v.val[1];
                                p[2][j] = v.val[2];
                            }
                            transpose8x8(p[0]);
                            transpose8x8(p[1]);
                            transpose8x8(p[2]);
                            for (size_t i = 0; i < 8; ++i) {
                                const size_t k = col_rev ? 7 - i : i;
                                vst3_u8(out + i * ow * 3, (uint8x8x3_t{{p[0][k], p[1][k], p[2][k]}}));
                            }
                        } else if constexpr (B == 4) {
                            const uint32x4_t r0 = vreinterpretq_u32_u8(vld1q_u8(rows[0]));
                            const uint32x4_t r1 = vreinterpretq_u32_u8(vld1q_u8(rows[1]));
                            const uint32x4_t r2 = vreinterpretq_u32_u8(vld1q_u8(rows[2]));
                            const uint32x4_t r3 = vreinterpretq_u32_u8(vld1q_u8(rows[3]));
                            const uint32x4x2_t ab = vtrnq_u32(r0, r1), cd = vtrnq_u32(r2, r3);
                            const uint32x4_t t[4] = {vcombine_u32(vget_low_u32(ab.val[0]), vget_low_u32(cd.val[0])),
                                                     vcombine_u32(vget_low_u32(ab.val[1]), vget_low_u32(cd.val[1])),
                                                     vcombine_u32(vget_high_u32(ab.val[0]), vget_high_u32(cd.val[0])),
                                                     vcombine_u32(vget_high_u32(ab.val[1]), vget_high_u32(cd.val[1]))};
                            for (size_t i = 0; i < 4; ++i) {
                                vst1q_u8(out + i * ow * 4, vreinterpretq_u8_u32(t[col_rev ? 3 - i : i]));
                            }
                        } else {
                            static_assert(B == 8);
                            const uint64x2_t r0 = vreinterpretq_u64_u8(vld1q_u8(rows[0]));
                            const uint64x2_t r1 = vreinterpretq_u64_u8(vld1q_u8(rows[1]));
                            const uint64x2_t t0 = vcombine_u64(vget_low_u64(r0), vget_low_u64(r1));
                            const uint64x2_t t1 = vcombine_u64(vget_high_u64(r0), vget_high_u64(r1));
                            vst1q_u8(out, vreinterpretq_u8_u64(col_rev ? t1 : t0));
                            vst1q_u8(out + ow * 8, vreinterpretq_u8_u64(col_rev ? t0 : t1));
                        }
                    }
                }
            }
        }
        // the bands to the right of and below the blocks
        auto pixel = [&](size_t x, size_t y) {
            const size_t sx = col_rev ? w - 1 - y : y;
            const size_t sy = row_rev ? h - 1 - x : x;
            std::memcpy(dst + (y * ow + x) * B, src + (sy * w + sx) * B, B);
        };
        for (size_t y = 0; y < bh; ++y) {
            for (size_t x = bw; x < ow; ++x) {
                pixel(x, y);
            }
        }
        for (size_t y = bh; y < oh; ++y) {
            for (size_t x = 0; x < ow; ++x) {
                pixel(x, y);
            }
        }
    }
#endif

    // The transform o (1..8) of the w × h pixels of B bytes at src into dst
    template<unsigned B>
    inline void run(const uint8_t* src, size_t w, size_t h, unsigned o, uint8_t* dst) noexcept {
        const size_t row = w * B;
        if (o == 1 || o < 1 || o > 8) {
            sgcl::detail::copy_bytes(dst, src, row * h);
            return;
        }
        if (o == 4) {
            for (size_t y = 0; y < h; ++y) {
                sgcl::detail::copy_bytes(dst + y * row, src + (h - 1 - y) * row, row);
            }
            return;
        }
#if defined(SGCL_CODEC_NEON)
        if constexpr (B <= 4) {
            if (o == 2 || o == 3) {
                for (size_t y = 0; y < h; ++y) {
                    reverse_row<B>(src + (o == 2 ? y : h - 1 - y) * row, dst + y * row, w);
                }
                return;
            }
        }
        if constexpr (B != 6) {
            if (o >= 5) {
                turned<B>(src, w, h, o, dst);
                return;
            }
        }
#endif
        plain<B>(src, w, h, o, dst);
    }

    // The same for a pixel of `bytes` bytes (1, 2, 3, 4, 6 or 8)
    inline void apply(const uint8_t* src, size_t w, size_t h, unsigned bytes, unsigned o, uint8_t* dst) noexcept {
        switch (bytes) {
            case 1: run<1>(src, w, h, o, dst); break;
            case 2: run<2>(src, w, h, o, dst); break;
            case 3: run<3>(src, w, h, o, dst); break;
            case 4: run<4>(src, w, h, o, dst); break;
            case 6: run<6>(src, w, h, o, dst); break;
            default: run<8>(src, w, h, o, dst); break;
        }
    }

    // The plain road for any of them, for the tests
    inline void apply_plain(const uint8_t* src, size_t w, size_t h, unsigned bytes, unsigned o, uint8_t* dst) noexcept {
        switch (bytes) {
            case 1: plain<1>(src, w, h, o, dst); break;
            case 2: plain<2>(src, w, h, o, dst); break;
            case 3: plain<3>(src, w, h, o, dst); break;
            case 4: plain<4>(src, w, h, o, dst); break;
            case 6: plain<6>(src, w, h, o, dst); break;
            default: plain<8>(src, w, h, o, dst); break;
        }
    }
}
