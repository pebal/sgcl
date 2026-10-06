//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "wide.h"

#include <cstddef>
#include <cstdint>

#include "../../core/detail/bytes.h"

// The engines of XXH32 and XXH64 (the xxHash specification, formats fixed
// since 2014 and 2016), with a seed. Both go the same way:
//
//   - four lanes start from the seed (seed + P1 + P2, seed + P2, seed,
//     seed - P1) when the input holds one stripe at least (16 bytes for
//     XXH32, 32 for XXH64); a stripe is four words, each added into its
//     lane times P2, the lane rotated (13 or 31) and multiplied by P1;
//   - the lanes are joined by rotations (1, 7, 12, 18) and, in XXH64, each
//     lane merged in once more; an input shorter than a stripe starts from
//     seed + P5 instead;
//   - the length is added (XXH32 its low 32 bits), the bytes after the last
//     stripe go in by words and then one by one, and an avalanche of
//     shifts and multiplications ends it.
//
// The streaming state keeps the four lanes and up to a stripe of bytes not
// yet taken; a hash in pieces is the hash in one call.
//
// No vector path: XXH32's four lanes in one NEON vector are one chain of a
// multiply-add, a rotation and a multiplication per stripe, slower than the
// four scalar chains side by side (5.2 against 7.4 GB/s on Apple M2), and
// NEON has no 64-bit multiplication for XXH64's. x86's SSE4.1 would be the
// same single chain.
namespace sgcl::hash::detail {
    inline constexpr uint32_t Xxh32Prime1 = 0x9E3779B1u;
    inline constexpr uint32_t Xxh32Prime2 = 0x85EBCA77u;
    inline constexpr uint32_t Xxh32Prime3 = 0xC2B2AE3Du;
    inline constexpr uint32_t Xxh32Prime4 = 0x27D4EB2Fu;
    inline constexpr uint32_t Xxh32Prime5 = 0x165667B1u;
    inline constexpr uint64_t Xxh64Prime1 = 0x9E3779B185EBCA87ull;
    inline constexpr uint64_t Xxh64Prime2 = 0xC2B2AE3D27D4EB4Full;
    inline constexpr uint64_t Xxh64Prime3 = 0x165667B19E3779F9ull;
    inline constexpr uint64_t Xxh64Prime4 = 0x85EBCA77C2B2AE63ull;
    inline constexpr uint64_t Xxh64Prime5 = 0x27D4EB2F165667C5ull;

    SGCL_INLINE_HOT uint64_t xxh64_avalanche(uint64_t h) noexcept {
        h ^= h >> 33;
        h *= Xxh64Prime2;
        h ^= h >> 29;
        h *= Xxh64Prime3;
        return h ^ (h >> 32);
    }

    SGCL_INLINE_HOT uint32_t xxh32_avalanche(uint32_t h) noexcept {
        h ^= h >> 15;
        h *= Xxh32Prime2;
        h ^= h >> 13;
        h *= Xxh32Prime3;
        return h ^ (h >> 16);
    }

    SGCL_INLINE_HOT uint32_t xxh32_round(uint32_t acc, uint32_t lane) noexcept {
        return rotate_left(uint32_t(acc + lane * Xxh32Prime2), 13) * Xxh32Prime1;
    }

    SGCL_INLINE_HOT uint64_t xxh64_round(uint64_t acc, uint64_t lane) noexcept {
        return rotate_left(uint64_t(acc + lane * Xxh64Prime2), 31) * Xxh64Prime1;
    }

    SGCL_INLINE_HOT uint64_t xxh64_merge(uint64_t acc, uint64_t lane) noexcept {
        return (acc ^ xxh64_round(0, lane)) * Xxh64Prime1 + Xxh64Prime4;
    }

    // A value the compiler may not look through: the four lanes of XXH32
    // stay four scalar chains, which clang would otherwise fold into one
    // vector (one chain of a multiply-add, a rotation and a multiplication,
    // 5.2 GB/s on Apple M2, against 7.4 for the four chains side by side)
    SGCL_INLINE_HOT uint32_t xxh32_opaque(uint32_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        __asm__("" : "+r"(v));
#endif
        return v;
    }

    // Stripes of 16 bytes into the four lanes of XXH32: `count` stripes from
    // p. Four independent chains of a multiply-add, a rotation and a
    // multiplication, in four registers.
    SGCL_INLINE_HOT void xxh32_stripes(uint32_t (&v)[4], const unsigned char* p, size_t count) noexcept {
        uint32_t a = v[0], b = v[1], c = v[2], d = v[3];
        for (; count; --count, p += 16) {
            a = xxh32_opaque(xxh32_round(a, load_le32(p)));
            b = xxh32_opaque(xxh32_round(b, load_le32(p + 4)));
            c = xxh32_opaque(xxh32_round(c, load_le32(p + 8)));
            d = xxh32_opaque(xxh32_round(d, load_le32(p + 12)));
        }
        v[0] = a;
        v[1] = b;
        v[2] = c;
        v[3] = d;
    }

    SGCL_INLINE_HOT void xxh64_stripes(uint64_t (&v)[4], const unsigned char* p, size_t count) noexcept {
        uint64_t a = v[0], b = v[1], c = v[2], d = v[3];
        for (; count; --count, p += 32) {
            a = xxh64_round(a, load_le64(p));
            b = xxh64_round(b, load_le64(p + 8));
            c = xxh64_round(c, load_le64(p + 16));
            d = xxh64_round(d, load_le64(p + 24));
        }
        v[0] = a;
        v[1] = b;
        v[2] = c;
        v[3] = d;
    }

    SGCL_INLINE_HOT void xxh32_start(uint32_t (&v)[4], uint32_t seed) noexcept {
        v[0] = seed + Xxh32Prime1 + Xxh32Prime2;
        v[1] = seed + Xxh32Prime2;
        v[2] = seed;
        v[3] = seed - Xxh32Prime1;
    }

    SGCL_INLINE_HOT void xxh64_start(uint64_t (&v)[4], uint64_t seed) noexcept {
        v[0] = seed + Xxh64Prime1 + Xxh64Prime2;
        v[1] = seed + Xxh64Prime2;
        v[2] = seed;
        v[3] = seed - Xxh64Prime1;
    }

    // The lanes joined into one word
    SGCL_INLINE_HOT uint32_t xxh32_join(uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept {
        return rotate_left(a, 1) + rotate_left(b, 7) + rotate_left(c, 12) + rotate_left(d, 18);
    }

    SGCL_INLINE_HOT uint64_t xxh64_join(uint64_t a, uint64_t b, uint64_t c, uint64_t d) noexcept {
        uint64_t h = rotate_left(a, 1) + rotate_left(b, 7) + rotate_left(c, 12) + rotate_left(d, 18);
        h = xxh64_merge(h, a);
        h = xxh64_merge(h, b);
        h = xxh64_merge(h, c);
        return xxh64_merge(h, d);
    }

    // The end of both, from the joined lanes (or the seed's start for a
    // short input) with the length added: the bytes after the last stripe,
    // by words and then one by one, and the avalanche
    SGCL_INLINE_HOT uint32_t xxh32_tail(uint32_t h, const unsigned char* p, size_t n) noexcept {
        for (; n >= 4; n -= 4, p += 4) {
            h = rotate_left(uint32_t(h + load_le32(p) * Xxh32Prime3), 17) * Xxh32Prime4;
        }
        for (; n; --n, ++p) {
            h = rotate_left(uint32_t(h + *p * Xxh32Prime5), 11) * Xxh32Prime1;
        }
        return xxh32_avalanche(h);
    }

    SGCL_INLINE_HOT uint64_t xxh64_tail(uint64_t h, const unsigned char* p, size_t n) noexcept {
        for (; n >= 8; n -= 8, p += 8) {
            h ^= xxh64_round(0, load_le64(p));
            h = rotate_left(h, 27) * Xxh64Prime1 + Xxh64Prime4;
        }
        if (n >= 4) {
            h ^= uint64_t(load_le32(p)) * Xxh64Prime1;
            h = rotate_left(h, 23) * Xxh64Prime2 + Xxh64Prime3;
            p += 4;
            n -= 4;
        }
        for (; n; --n, ++p) {
            h ^= *p * Xxh64Prime5;
            h = rotate_left(h, 11) * Xxh64Prime1;
        }
        return xxh64_avalanche(h);
    }

    // The one-shot forms: the lanes in registers, the stripes, the tail
    inline uint32_t xxh32(const unsigned char* p, size_t n, uint32_t seed) noexcept {
        uint32_t h;
        const size_t stripes = n / 16;
        if (stripes) {
            uint32_t v[4];
            xxh32_start(v, seed);
            xxh32_stripes(v, p, stripes);
            h = xxh32_join(v[0], v[1], v[2], v[3]);
        } else {
            h = seed + Xxh32Prime5;
        }
        return xxh32_tail(h + uint32_t(n), p + stripes * 16, n % 16);
    }

    inline uint64_t xxh64(const unsigned char* p, size_t n, uint64_t seed) noexcept {
        uint64_t h;
        const size_t stripes = n / 32;
        if (stripes) {
            uint64_t v[4];
            xxh64_start(v, seed);
            xxh64_stripes(v, p, stripes);
            h = xxh64_join(v[0], v[1], v[2], v[3]);
        } else {
            h = seed + Xxh64Prime5;
        }
        return xxh64_tail(h + n, p + stripes * 32, n % 32);
    }

    // The streaming state of both: the lanes, the seed, the bytes taken in
    // and up to one stripe not yet taken. A plain value.
    template<class Word, size_t Stripe>
    class XxhStream {
    public:
        XxhStream() noexcept {
            _start();
        }

        SGCL_INLINE_HOT explicit XxhStream(Word seed) noexcept
        : _seed(seed) {
            _start();
        }

        void update(const unsigned char* p, size_t n) noexcept {
            _total += n;
            if (_buffered + n < Stripe) {
                sgcl::detail::copy_bytes(_buffer + _buffered, p, n);
                _buffered += n;
                return;
            }
            if (_buffered) {
                const size_t room = Stripe - _buffered;
                sgcl::detail::copy_bytes(_buffer + _buffered, p, room);
                _stripes(_buffer, 1);
                p += room;
                n -= room;
                _buffered = 0;
            }
            const size_t count = n / Stripe;
            _stripes(p, count);
            p += count * Stripe;
            n -= count * Stripe;
            sgcl::detail::copy_bytes(_buffer, p, n);
            _buffered = n;
        }

        SGCL_INLINE_HOT Word value() const noexcept {
            if constexpr (Stripe == 16) {
                uint32_t h = _total >= Stripe ? xxh32_join(_lanes[0], _lanes[1], _lanes[2], _lanes[3]) : _seed + Xxh32Prime5;
                return xxh32_tail(h + uint32_t(_total), _buffer, _buffered);
            } else {
                uint64_t h = _total >= Stripe ? xxh64_join(_lanes[0], _lanes[1], _lanes[2], _lanes[3]) : _seed + Xxh64Prime5;
                return xxh64_tail(h + _total, _buffer, _buffered);
            }
        }

        SGCL_INLINE_HOT void reset() noexcept {
            _start();
            _total = 0;
            _buffered = 0;
        }

    private:
        SGCL_INLINE_HOT void _start() noexcept {
            if constexpr (Stripe == 16) {
                xxh32_start(_lanes, _seed);
            } else {
                xxh64_start(_lanes, _seed);
            }
        }

        SGCL_INLINE_HOT void _stripes(const unsigned char* p, size_t count) noexcept {
            if constexpr (Stripe == 16) {
                xxh32_stripes(_lanes, p, count);
            } else {
                xxh64_stripes(_lanes, p, count);
            }
        }

        Word _lanes[4];
        Word _seed = 0;
        uint32_t _buffered = 0;   // bytes in the buffer, fewer than a stripe
        uint64_t _total = 0;      // bytes taken in
        unsigned char _buffer[Stripe];
    };

    using Xxh32Stream = XxhStream<uint32_t, 16>;
    using Xxh64Stream = XxhStream<uint64_t, 32>;
}
