//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "simd.h"
#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace sgcl::codec::detail {
    // The five filters of PNG (PNG 3rd ed., 7.3 and 9), undone: each byte
    // x of a row is stored as x minus a prediction from a, the byte one
    // pixel to the left (bpp bytes back, 0 before the row), b, the byte
    // above (0 in the first row of a pass), and c, the byte above a:
    //   0 None   no prediction
    //   1 Sub    a
    //   2 Up     b
    //   3 Average  floor((a + b) / 2)
    //   4 Paeth  the one of a, b, c nearest to a + b - c (ties to a, then b)
    // bpp: the bytes of a pixel, 1 for pixels smaller than a byte.
    enum : uint8_t {
        FilterNone = 0,
        FilterSub = 1,
        FilterUp = 2,
        FilterAverage = 3,
        FilterPaeth = 4
    };

    inline uint8_t paeth(int a, int b, int c) noexcept {
        const int p = a + b - c;
        const int pa = std::abs(p - a);
        const int pb = std::abs(p - b);
        const int pc = std::abs(p - c);
        if (pa <= pb && pa <= pc) {
            return static_cast<uint8_t>(a);
        }
        return static_cast<uint8_t>(pb <= pc ? b : c);
    }

    // raw (the filtered bytes of the row, the filter byte not among them)
    // into out, `prior` the row above as it was reconstructed (zeros for
    // the first row of a pass); out never overlaps raw or prior. The plain
    // road, a byte at a time
    inline void unfilter_plain(uint8_t type, const uint8_t* raw, const uint8_t* prior, uint8_t* out, size_t n, unsigned bpp) noexcept {
        const size_t lead = bpp < n ? bpp : n;
        switch (type) {
            case FilterNone:
                sgcl::detail::copy_bytes(out, raw, n);
                break;
            case FilterSub:
                sgcl::detail::copy_bytes(out, raw, lead);
                for (size_t i = bpp; i < n; ++i) {
                    out[i] = static_cast<uint8_t>(raw[i] + out[i - bpp]);
                }
                break;
            case FilterUp:
                for (size_t i = 0; i < n; ++i) {
                    out[i] = static_cast<uint8_t>(raw[i] + prior[i]);
                }
                break;
            case FilterAverage:
                for (size_t i = 0; i < lead; ++i) {
                    out[i] = static_cast<uint8_t>(raw[i] + (prior[i] >> 1));
                }
                for (size_t i = bpp; i < n; ++i) {
                    out[i] = static_cast<uint8_t>(raw[i] + ((unsigned(out[i - bpp]) + prior[i]) >> 1));
                }
                break;
            case FilterPaeth:
                // a and c are 0 before the row: the prediction is b
                for (size_t i = 0; i < lead; ++i) {
                    out[i] = static_cast<uint8_t>(raw[i] + prior[i]);
                }
                for (size_t i = bpp; i < n; ++i) {
                    out[i] = static_cast<uint8_t>(raw[i] + paeth(out[i - bpp], prior[i], prior[i - bpp]));
                }
                break;
        }
    }

#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    // The vector road of Sub, Average and Paeth for pixels of 3, 4, 6 and 8
    // bytes (RGB, RGBA, 16-bit gray with alpha, RGB and RGBA): each byte of
    // a row hangs on the byte a pixel before it, so the row goes a pixel at
    // a time, the pixel's bytes in the lanes of one vector. Paeth from its
    // definition (PNG 3rd ed., 9.4): p = a + b - c, and of pa = |p - a| =
    // |b - c|, pb = |p - b| = |a - c|, pc = |p - c| = |a + b - 2c| the least
    // chooses, a before b before c on a tie: 16-bit lanes, the choice as
    // masks, no branch. Row bytes after the last whole pixel go the plain way.
    namespace unfilter_vector {
        // A pixel's bytes in the low bytes of a word (the vector roads are
        // little-endian ones), by loads and stores of fixed widths: 3 as
        // 2 + 1, 6 as 4 + 2, nothing read or written past the pixel
        template<unsigned Bpp>
        inline uint64_t load(const uint8_t* p) noexcept {
            static_assert(Bpp == 3 || Bpp == 4 || Bpp == 6 || Bpp == 8);
            if constexpr (Bpp == 8) {
                uint64_t v;
                std::memcpy(&v, p, 8);
                return v;
            } else if constexpr (Bpp == 4) {
                uint32_t v;
                std::memcpy(&v, p, 4);
                return v;
            } else if constexpr (Bpp == 3) {
                uint16_t v;
                std::memcpy(&v, p, 2);
                return v | uint64_t(p[2]) << 16;
            } else {
                uint32_t v;
                uint16_t w;
                std::memcpy(&v, p, 4);
                std::memcpy(&w, p + 4, 2);
                return v | uint64_t(w) << 32;
            }
        }

        template<unsigned Bpp>
        inline void store(uint8_t* p, uint64_t v) noexcept {
            if constexpr (Bpp == 8) {
                std::memcpy(p, &v, 8);
            } else if constexpr (Bpp == 4) {
                const uint32_t x = uint32_t(v);
                std::memcpy(p, &x, 4);
            } else if constexpr (Bpp == 3) {
                const uint16_t x = uint16_t(v);
                std::memcpy(p, &x, 2);
                p[2] = uint8_t(v >> 16);
            } else {
                const uint32_t x = uint32_t(v);
                const uint16_t y = uint16_t(v >> 32);
                std::memcpy(p, &x, 4);
                std::memcpy(p + 4, &y, 2);
            }
        }
    }

#if defined(SGCL_CODEC_NEON)
    namespace unfilter_vector {
        using Vector = uint8x8_t;

        inline Vector v8(uint64_t x) noexcept {
            return vcreate_u8(x);
        }

        inline uint64_t bits(Vector v) noexcept {
            return vget_lane_u64(vreinterpret_u64_u8(v), 0);
        }

        inline Vector add(Vector x, Vector y) noexcept {
            return vadd_u8(x, y);
        }

        // floor((a + b) / 2), which VHADD is
        inline Vector average(Vector a, Vector b) noexcept {
            return vhadd_u8(a, b);
        }

        // Paeth's predictor of each lane
        inline Vector paeth(Vector a8, Vector b8, Vector c8) noexcept {
            const int16x8_t a = vreinterpretq_s16_u16(vmovl_u8(a8)), b = vreinterpretq_s16_u16(vmovl_u8(b8)), c = vreinterpretq_s16_u16(vmovl_u8(c8));
            const int16x8_t pa = vabdq_s16(b, c);
            const int16x8_t pb = vabdq_s16(a, c);
            const int16x8_t pc = vabsq_s16(vsubq_s16(vaddq_s16(a, b), vaddq_s16(c, c)));
            const uint16x8_t take_a = vandq_u16(vcleq_s16(pa, pb), vcleq_s16(pa, pc));
            const uint16x8_t take_b = vcleq_s16(pb, pc);
            const int16x8_t bc = vbslq_s16(take_b, b, c);
            return vmovn_u16(vreinterpretq_u16_s16(vbslq_s16(take_a, a, bc)));
        }
    }
#else
    namespace unfilter_vector {
        using Vector = __m128i;

        inline Vector v8(uint64_t x) noexcept {
            return _mm_cvtsi64_si128(int64_t(x));
        }

        inline uint64_t bits(Vector v) noexcept {
            return uint64_t(_mm_cvtsi128_si64(v));
        }

        inline Vector add(Vector x, Vector y) noexcept {
            return _mm_add_epi8(x, y);
        }

        // floor((a + b) / 2): PAVGB rounds up, the odd sums' bit taken back
        inline Vector average(Vector a, Vector b) noexcept {
            return _mm_sub_epi8(_mm_avg_epu8(a, b), _mm_and_si128(_mm_xor_si128(a, b), _mm_set1_epi8(1)));
        }

        inline __m128i abs16(__m128i x) noexcept {
            return _mm_max_epi16(x, _mm_sub_epi16(_mm_setzero_si128(), x));
        }

        inline Vector paeth(Vector a8, Vector b8, Vector c8) noexcept {
            const __m128i zero = _mm_setzero_si128();
            const __m128i a = _mm_unpacklo_epi8(a8, zero), b = _mm_unpacklo_epi8(b8, zero), c = _mm_unpacklo_epi8(c8, zero);
            const __m128i pa = abs16(_mm_sub_epi16(b, c));
            const __m128i pb = abs16(_mm_sub_epi16(a, c));
            const __m128i pc = abs16(_mm_sub_epi16(_mm_add_epi16(a, b), _mm_add_epi16(c, c)));
            // x <= y as not (x > y)
            const __m128i not_a = _mm_or_si128(_mm_cmpgt_epi16(pa, pb), _mm_cmpgt_epi16(pa, pc));
            const __m128i not_b = _mm_cmpgt_epi16(pb, pc);
            const __m128i bc = _mm_or_si128(_mm_andnot_si128(not_b, b), _mm_and_si128(not_b, c));
            const __m128i r = _mm_or_si128(_mm_andnot_si128(not_a, a), _mm_and_si128(not_a, bc));
            return _mm_packus_epi16(r, zero);
        }
    }
#endif

    namespace unfilter_vector {
        template<unsigned Bpp>
        inline size_t row(uint8_t type, const uint8_t* raw, const uint8_t* prior, uint8_t* out, size_t n) noexcept {
            size_t i = Bpp;
            Vector a = v8(load<Bpp>(out));
            switch (type) {
                case 1:   // Sub
                    for (; i + Bpp <= n; i += Bpp) {
                        a = add(v8(load<Bpp>(raw + i)), a);
                        store<Bpp>(out + i, bits(a));
                    }
                    break;
                case 3:   // Average
                    for (; i + Bpp <= n; i += Bpp) {
                        a = add(v8(load<Bpp>(raw + i)), average(a, v8(load<Bpp>(prior + i))));
                        store<Bpp>(out + i, bits(a));
                    }
                    break;
                default: {   // Paeth
                    Vector c = v8(load<Bpp>(prior));
                    for (; i + Bpp <= n; i += Bpp) {
                        const Vector b = v8(load<Bpp>(prior + i));
                        a = add(v8(load<Bpp>(raw + i)), paeth(a, b, c));
                        store<Bpp>(out + i, bits(a));
                        c = b;
                    }
                    break;
                }
            }
            return i;
        }
    }

    // The row after its first pixel, a whole pixel a step; returns where
    // it stopped. bpp: 3, 4, 6 or 8
    inline size_t unfilter_row_vector(uint8_t type, const uint8_t* raw, const uint8_t* prior, uint8_t* out, size_t n, unsigned bpp) noexcept {
        switch (bpp) {
            case 3: return unfilter_vector::row<3>(type, raw, prior, out, n);
            case 4: return unfilter_vector::row<4>(type, raw, prior, out, n);
            case 6: return unfilter_vector::row<6>(type, raw, prior, out, n);
            default: return unfilter_vector::row<8>(type, raw, prior, out, n);
        }
    }
#endif

    inline void unfilter(uint8_t type, const uint8_t* raw, const uint8_t* prior, uint8_t* out, size_t n, unsigned bpp) noexcept {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        if ((type == FilterSub || type == FilterAverage || type == FilterPaeth) && (bpp == 3 || bpp == 4 || bpp == 6 || bpp == 8) && n > bpp) {
            // the first pixel as the plain road makes it (a and c are 0)
            unfilter_plain(type, raw, prior, out, bpp, bpp);
            const size_t done = unfilter_row_vector(type, raw, prior, out, n, bpp);
            for (size_t i = done; i < n; ++i) {
                const uint8_t a = out[i - bpp];
                switch (type) {
                    case FilterSub: out[i] = uint8_t(raw[i] + a); break;
                    case FilterAverage: out[i] = uint8_t(raw[i] + ((unsigned(a) + prior[i]) >> 1)); break;
                    default: out[i] = uint8_t(raw[i] + paeth(a, prior[i], prior[i - bpp])); break;
                }
            }
            return;
        }
#endif
        unfilter_plain(type, raw, prior, out, n, bpp);
    }

    // The other way: the row x filtered by `type` into out, `prior` the
    // row above (zeros for the first row); out never overlaps x or prior
    inline void filter(uint8_t type, const uint8_t* x, const uint8_t* prior, uint8_t* out, size_t n, unsigned bpp) noexcept {
        const size_t lead = bpp < n ? bpp : n;
        switch (type) {
            case FilterNone:
                sgcl::detail::copy_bytes(out, x, n);
                break;
            case FilterSub:
                sgcl::detail::copy_bytes(out, x, lead);
                for (size_t i = bpp; i < n; ++i) {
                    out[i] = static_cast<uint8_t>(x[i] - x[i - bpp]);
                }
                break;
            case FilterUp:
                for (size_t i = 0; i < n; ++i) {
                    out[i] = static_cast<uint8_t>(x[i] - prior[i]);
                }
                break;
            case FilterAverage:
                for (size_t i = 0; i < lead; ++i) {
                    out[i] = static_cast<uint8_t>(x[i] - (prior[i] >> 1));
                }
                for (size_t i = bpp; i < n; ++i) {
                    out[i] = static_cast<uint8_t>(x[i] - ((unsigned(x[i - bpp]) + prior[i]) >> 1));
                }
                break;
            case FilterPaeth:
                for (size_t i = 0; i < lead; ++i) {
                    out[i] = static_cast<uint8_t>(x[i] - prior[i]);
                }
                for (size_t i = bpp; i < n; ++i) {
                    out[i] = static_cast<uint8_t>(x[i] - paeth(x[i - bpp], prior[i], prior[i - bpp]));
                }
                break;
        }
    }
}
