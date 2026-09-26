//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__ARM_NEON) && !defined(SGCL_ENCODING_NO_NEON)
#include <arm_neon.h>
#define SGCL_ENCODING_NEON 1
#endif

namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;
    // The searches of the text formats: the first byte of a run that ends
    // the plain part of a JSON string (a quote, a backslash, a control
    // character or a byte of a UTF-8 sequence), of a CSV field (the
    // separator, a quote, a line ending) or of XML text (< or &). A search
    // loop with an early exit, which the compiler does not vectorise by
    // itself: the one kind of loop where the library's rule lets an
    // intrinsic in. Two forms each: eight bytes at a time in a word
    // (SWAR, every target), and sixteen at a time with NEON where the
    // target has it (SGCL_ENCODING_NO_NEON turns it off, for the A/B
    // comparison). Both return the first such byte, or end.

    // Eight bytes of p as a little-endian word
    inline uint64_t load_word(const char* p) noexcept {
        uint64_t w;
        std::memcpy(&w, p, 8);
        if constexpr (std::endian::native == std::endian::big) {
            w = __builtin_bswap64(w);
        }
        return w;
    }

    inline constexpr uint64_t Ones = 0x0101010101010101ull;
    inline constexpr uint64_t Highs = 0x8080808080808080ull;

    // The high bit of each byte of w that is zero. Exact for the lowest
    // such byte; a byte above it may be flagged by the borrow, which the
    // searches never look at (they take the lowest bit)
    constexpr uint64_t zero_bytes(uint64_t w) noexcept {
        return (w - Ones) & ~w & Highs;
    }

    // The high bit of each byte of w under n (n <= 128), with the same
    // exactness at the lowest one
    constexpr uint64_t bytes_below(uint64_t w, uint8_t n) noexcept {
        return (w - Ones * n) & ~w & Highs;
    }

    constexpr uint64_t bytes_equal(uint64_t w, char c) noexcept {
        return zero_bytes(w ^ (Ones * uint8_t(c)));
    }

    inline const char* first_flagged(const char* p, uint64_t mask) noexcept {
        return p + (std::countr_zero(mask) >> 3);
    }

#ifdef SGCL_ENCODING_NEON
    // The lowest set byte of a 16-byte mask of 0x00/0xFF lanes, as an
    // index; 16 when none
    inline unsigned first_lane(uint8x16_t m) noexcept {
        uint64_t bits = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
        return bits ? unsigned(std::countr_zero(bits) >> 2) : 16u;
    }
#endif

    // The plain part of a JSON string: the first byte that is '"', '\\',
    // under 0x20 or 0x80 and above
    inline const char* json_string_stop(const char* p, const char* end) noexcept {
#ifdef SGCL_ENCODING_NEON
        const uint8x16_t quote = vdupq_n_u8('"');
        const uint8x16_t backslash = vdupq_n_u8('\\');
        const int8x16_t space = vdupq_n_s8(0x20);
        while (end - p >= 16) {
            uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t*>(p));
            // a signed byte under 0x20 is a control character or 0x80 and above
            uint8x16_t m = vorrq_u8(vorrq_u8(vceqq_u8(v, quote), vceqq_u8(v, backslash)), vcltq_s8(vreinterpretq_s8_u8(v), space));
            if (vmaxvq_u8(m)) {
                return p + first_lane(m);
            }
            p += 16;
        }
#endif
        while (end - p >= 8) {
            uint64_t w = load_word(p);
            uint64_t m = bytes_equal(w, '"') | bytes_equal(w, '\\') | bytes_below(w, 0x20) | (w & Highs);
            if (m) {
                return first_flagged(p, m);
            }
            p += 8;
        }
        while (p != end) {
            auto b = uint8_t(*p);
            if (b == '"' || b == '\\' || b < 0x20 || b >= 0x80) {
                return p;
            }
            ++p;
        }
        return end;
    }

    // The next run of UTF-8 inside a JSON string: the first byte under
    // 0x80 (the sequences are checked one by one by the caller); this only
    // skips the ASCII test for a text of other scripts
    inline const char* first_ascii(const char* p, const char* end) noexcept {
        while (end - p >= 8) {
            uint64_t m = ~load_word(p) & Highs;
            if (m) {
                return first_flagged(p, m);
            }
            p += 8;
        }
        while (p != end && uint8_t(*p) >= 0x80) {
            ++p;
        }
        return p;
    }

    // The first of four bytes: a CSV field's separator, quote, '\n' and
    // '\r' (a quoted field asks for its quote alone, passing it four times)
    inline const char* find_any_of(const char* p, const char* end, char a, char b, char c, char d) noexcept {
#ifdef SGCL_ENCODING_NEON
        const uint8x16_t va = vdupq_n_u8(uint8_t(a)), vb = vdupq_n_u8(uint8_t(b));
        const uint8x16_t vc = vdupq_n_u8(uint8_t(c)), vd = vdupq_n_u8(uint8_t(d));
        while (end - p >= 16) {
            uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t*>(p));
            uint8x16_t m = vorrq_u8(vorrq_u8(vceqq_u8(v, va), vceqq_u8(v, vb)), vorrq_u8(vceqq_u8(v, vc), vceqq_u8(v, vd)));
            if (vmaxvq_u8(m)) {
                return p + first_lane(m);
            }
            p += 16;
        }
#endif
        while (end - p >= 8) {
            uint64_t w = load_word(p);
            uint64_t m = bytes_equal(w, a) | bytes_equal(w, b) | bytes_equal(w, c) | bytes_equal(w, d);
            if (m) {
                return first_flagged(p, m);
            }
            p += 8;
        }
        while (p != end && *p != a && *p != b && *p != c && *p != d) {
            ++p;
        }
        return p;
    }
}
