//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "wide.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__aarch64__) && defined(__AARCH64EL__) && defined(__ARM_NEON) && !defined(SGCL_HASH_PORTABLE)
#define SGCL_HASH_XXH3_NEON 1
#include <arm_neon.h>
#endif

// The engine of XXH3 (xxHash 0.8, the format frozen since 2020), 64 and 128
// bits, with a seed. The algorithm in the terms this file uses:
//
//   - a secret of 192 bytes: the default one below, or, with a seed s other
//     than 0, the default one read as 24 little-endian words, s added to
//     every even word and taken from every odd one. Inputs up to 240 bytes
//     read the default secret and bring the seed in themselves;
//   - inputs up to 240 bytes take one of five paths by length: none, 1–3,
//     4–8, 9–16 (the bytes read as two words that may overlap, one or two
//     multiplications), 17–128 (pairs of 16-byte pieces from both ends
//     toward the middle) and 129–240 (16-byte pieces from the front, the
//     secret read at an offset past the eighth);
//   - longer inputs go through eight 64-bit lanes. A stripe of 64 bytes
//     adds, in lane i, the product of the low and the high half of
//     (data word i XOR secret word i) and, in the neighbouring lane i^1,
//     the data word itself; the secret moves eight bytes a stripe. Sixteen
//     stripes are a block of 1024 bytes, after which every lane is
//     scrambled (xorshift, XOR with the end of the secret, multiplication).
//     A stripe or a block is taken only when at least one byte follows it:
//     the last 64 bytes of the input always go in as one more stripe of
//     their own, read from the end, with the secret at another offset. The
//     lanes are then merged in pairs, each pair one folded product;
//   - the 128-bit hash is the same work with a second merge (other offsets,
//     another start) for its high half on the long path, and paths of its
//     own on the short ones.
//
// The streaming state (Xxh3Stream) keeps what no stripe has taken yet in a
// buffer of four stripes, and takes a stripe only when more bytes come
// after it, so that a hash in pieces is the hash in one call.
namespace sgcl::hash::detail {
    inline constexpr uint32_t Xxh32Prime1 = 0x9E3779B1u;
    inline constexpr uint32_t Xxh32Prime2 = 0x85EBCA77u;
    inline constexpr uint32_t Xxh32Prime3 = 0xC2B2AE3Du;
    inline constexpr uint64_t Xxh64Prime1 = 0x9E3779B185EBCA87ull;
    inline constexpr uint64_t Xxh64Prime2 = 0xC2B2AE3D27D4EB4Full;
    inline constexpr uint64_t Xxh64Prime3 = 0x165667B19E3779F9ull;
    inline constexpr uint64_t Xxh64Prime4 = 0x85EBCA77C2B2AE63ull;
    inline constexpr uint64_t Xxh64Prime5 = 0x27D4EB2F165667C5ull;
    inline constexpr uint64_t Xxh3MixPrime1 = 0x165667919E3779F9ull;
    inline constexpr uint64_t Xxh3MixPrime2 = 0x9FB21C651E98DF25ull;

    inline constexpr size_t Xxh3SecretSize = 192;
    inline constexpr size_t Xxh3StripeSize = 64;
    inline constexpr size_t Xxh3StripesPerBlock = (Xxh3SecretSize - Xxh3StripeSize) / 8;   // 16
    inline constexpr size_t Xxh3BlockSize = Xxh3StripeSize * Xxh3StripesPerBlock;          // 1024
    inline constexpr size_t Xxh3ShortLimit = 240;
    inline constexpr size_t Xxh3BufferSize = 4 * Xxh3StripeSize;
    inline constexpr size_t Xxh3ScrambleAt = Xxh3SecretSize - Xxh3StripeSize;              // 128
    inline constexpr size_t Xxh3LastStripeAt = Xxh3SecretSize - Xxh3StripeSize - 7;        // 121
    inline constexpr size_t Xxh3MergeAt = 11;
    inline constexpr size_t Xxh3MergeHighAt = Xxh3SecretSize - Xxh3StripeSize - 11;       // 117
    inline constexpr size_t Xxh3MidStartAt = 3;
    inline constexpr size_t Xxh3MidLastAt = 136 - 17;                                      // 119

    // The default secret: fixed bytes, part of the format
    alignas(64) inline constexpr unsigned char Xxh3Secret[Xxh3SecretSize] = {
        0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
        0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
        0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
        0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
        0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
        0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
        0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
        0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
        0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
        0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
        0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
        0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
    };

    // The secret of a seed: the default one with the seed added to its even
    // words and taken from its odd ones (a seed of 0 gives the default)
    inline void xxh3_derive_secret(unsigned char (&out)[Xxh3SecretSize], uint64_t seed) noexcept {
        for (size_t i = 0; i < Xxh3SecretSize; i += 16) {
            const uint64_t even = load_le64(Xxh3Secret + i) + seed;
            const uint64_t odd = load_le64(Xxh3Secret + i + 8) - seed;
            for (size_t k = 0; k < 8; ++k) {
                out[i + k] = (unsigned char)(even >> (8 * k));
                out[i + 8 + k] = (unsigned char)(odd >> (8 * k));
            }
        }
    }

    // The final mixes: of XXH64 (for the shortest inputs), of XXH3, and the
    // stronger one of the 4–8 path
    inline uint64_t xxh64_avalanche(uint64_t h) noexcept {
        h ^= h >> 33;
        h *= Xxh64Prime2;
        h ^= h >> 29;
        h *= Xxh64Prime3;
        return h ^ (h >> 32);
    }

    inline uint64_t xxh3_avalanche(uint64_t h) noexcept {
        h ^= h >> 37;
        h *= Xxh3MixPrime1;
        return h ^ (h >> 32);
    }

    inline uint64_t xxh3_rrmxmx(uint64_t h, uint64_t length) noexcept {
        h ^= rotate_left(h, 49) ^ rotate_left(h, 24);
        h *= Xxh3MixPrime2;
        h ^= (h >> 35) + length;
        h *= Xxh3MixPrime2;
        return h ^ (h >> 28);
    }

    // Sixteen bytes and sixteen of the secret, the seed on both secret
    // words, folded into one word by one product
    inline uint64_t xxh3_mix16(const unsigned char* p, const unsigned char* s, uint64_t seed) noexcept {
        return fold_product(load_le64(p) ^ (load_le64(s) + seed), load_le64(p + 8) ^ (load_le64(s + 8) - seed));
    }

    // The 64-bit hash of up to 240 bytes, against the default secret
    inline uint64_t xxh3_64_short(const unsigned char* p, size_t n, uint64_t seed) noexcept {
        const unsigned char* s = Xxh3Secret;
        if (n <= 16) {
            if (n > 8) {
                const uint64_t flip_low = (load_le64(s + 24) ^ load_le64(s + 32)) + seed;
                const uint64_t flip_high = (load_le64(s + 40) ^ load_le64(s + 48)) - seed;
                const uint64_t low = load_le64(p) ^ flip_low;
                const uint64_t high = load_le64(p + n - 8) ^ flip_high;
                return xxh3_avalanche(n + swap64(low) + high + fold_product(low, high));
            }
            if (n >= 4) {
                const uint64_t seed_both = seed ^ (uint64_t(swap32(uint32_t(seed))) << 32);
                const uint64_t flip = (load_le64(s + 8) ^ load_le64(s + 16)) - seed_both;
                const uint64_t word = load_le32(p + n - 4) + (uint64_t(load_le32(p)) << 32);
                return xxh3_rrmxmx(word ^ flip, n);
            }
            if (n > 0) {
                const uint32_t word = uint32_t(p[0]) << 16 | uint32_t(p[n >> 1]) << 24 | uint32_t(p[n - 1]) | uint32_t(n) << 8;
                const uint64_t flip = uint64_t(load_le32(s) ^ load_le32(s + 4)) + seed;
                return xxh64_avalanche(uint64_t(word) ^ flip);
            }
            return xxh64_avalanche(seed ^ load_le64(s + 56) ^ load_le64(s + 64));
        }
        uint64_t acc = n * Xxh64Prime1;
        if (n <= 128) {
            // pairs from both ends toward the middle: 16 and n−16 always, then
            // 32 more on each side while the input reaches that far
            if (n > 32) {
                if (n > 64) {
                    if (n > 96) {
                        acc += xxh3_mix16(p + 48, s + 96, seed);
                        acc += xxh3_mix16(p + n - 64, s + 112, seed);
                    }
                    acc += xxh3_mix16(p + 32, s + 64, seed);
                    acc += xxh3_mix16(p + n - 48, s + 80, seed);
                }
                acc += xxh3_mix16(p + 16, s + 32, seed);
                acc += xxh3_mix16(p + n - 32, s + 48, seed);
            }
            acc += xxh3_mix16(p, s, seed);
            acc += xxh3_mix16(p + n - 16, s + 16, seed);
            return xxh3_avalanche(acc);
        }
        for (size_t i = 0; i < 8; ++i) {
            acc += xxh3_mix16(p + 16 * i, s + 16 * i, seed);
        }
        acc = xxh3_avalanche(acc);
        const size_t rounds = n / 16;
        for (size_t i = 8; i < rounds; ++i) {
            acc += xxh3_mix16(p + 16 * i, s + 16 * (i - 8) + Xxh3MidStartAt, seed);
        }
        acc += xxh3_mix16(p + n - 16, s + Xxh3MidLastAt, seed);
        return xxh3_avalanche(acc);
    }

    // Two pieces of 16 into a 128-bit accumulator: each half takes the mix
    // of one piece and the sum of the other's two words
    inline void xxh3_mix32(Wide& acc, const unsigned char* a, const unsigned char* b, const unsigned char* s, uint64_t seed) noexcept {
        acc.low += xxh3_mix16(a, s, seed);
        acc.low ^= load_le64(b) + load_le64(b + 8);
        acc.high += xxh3_mix16(b, s + 16, seed);
        acc.high ^= load_le64(a) + load_le64(a + 8);
    }

    inline array<byte, 16> xxh3_128_bytes(Wide h) noexcept {
        array<byte, 16> out;
        for (size_t i = 0; i < 8; ++i) {
            out[i] = byte(h.high >> (56 - 8 * i));
            out[8 + i] = byte(h.low >> (56 - 8 * i));
        }
        return out;
    }

    // The 128-bit hash of up to 240 bytes, against the default secret
    inline Wide xxh3_128_short(const unsigned char* p, size_t n, uint64_t seed) noexcept {
        const unsigned char* s = Xxh3Secret;
        if (n <= 16) {
            if (n > 8) {
                const uint64_t flip_low = (load_le64(s + 32) ^ load_le64(s + 40)) - seed;
                const uint64_t flip_high = (load_le64(s + 48) ^ load_le64(s + 56)) + seed;
                const uint64_t first = load_le64(p);
                uint64_t last = load_le64(p + n - 8);
                Wide m = multiply_wide(first ^ last ^ flip_low, Xxh64Prime1);
                m.low += uint64_t(n - 1) << 54;
                last ^= flip_high;
                m.high += last + uint64_t(uint32_t(last)) * (Xxh32Prime2 - 1);
                m.low ^= swap64(m.high);
                Wide h = multiply_wide(m.low, Xxh64Prime2);
                h.high += m.high * Xxh64Prime2;
                return {xxh3_avalanche(h.low), xxh3_avalanche(h.high)};
            }
            if (n >= 4) {
                const uint64_t seed_both = seed ^ (uint64_t(swap32(uint32_t(seed))) << 32);
                const uint64_t word = load_le32(p) + (uint64_t(load_le32(p + n - 4)) << 32);
                const uint64_t flip = (load_le64(s + 16) ^ load_le64(s + 24)) + seed_both;
                Wide m = multiply_wide(word ^ flip, Xxh64Prime1 + (uint64_t(n) << 2));
                m.high += m.low << 1;
                m.low ^= m.high >> 3;
                m.low ^= m.low >> 35;
                m.low *= Xxh3MixPrime2;
                m.low ^= m.low >> 28;
                m.high = xxh3_avalanche(m.high);
                return m;
            }
            if (n > 0) {
                const uint32_t low = uint32_t(p[0]) << 16 | uint32_t(p[n >> 1]) << 24 | uint32_t(p[n - 1]) | uint32_t(n) << 8;
                const uint32_t high = rotate_left(swap32(low), 13);
                const uint64_t flip_low = uint64_t(load_le32(s) ^ load_le32(s + 4)) + seed;
                const uint64_t flip_high = uint64_t(load_le32(s + 8) ^ load_le32(s + 12)) - seed;
                return {xxh64_avalanche(uint64_t(low) ^ flip_low), xxh64_avalanche(uint64_t(high) ^ flip_high)};
            }
            return {xxh64_avalanche(seed ^ load_le64(s + 64) ^ load_le64(s + 72)), xxh64_avalanche(seed ^ load_le64(s + 80) ^ load_le64(s + 88))};
        }
        Wide acc {n * Xxh64Prime1, 0};
        if (n <= 128) {
            if (n > 32) {
                if (n > 64) {
                    if (n > 96) {
                        xxh3_mix32(acc, p + 48, p + n - 64, s + 96, seed);
                    }
                    xxh3_mix32(acc, p + 32, p + n - 48, s + 64, seed);
                }
                xxh3_mix32(acc, p + 16, p + n - 32, s + 32, seed);
            }
            xxh3_mix32(acc, p, p + n - 16, s, seed);
        } else {
            for (size_t i = 0; i < 4; ++i) {
                xxh3_mix32(acc, p + 32 * i, p + 32 * i + 16, s + 32 * i, seed);
            }
            acc.low = xxh3_avalanche(acc.low);
            acc.high = xxh3_avalanche(acc.high);
            const size_t rounds = n / 32;
            for (size_t i = 4; i < rounds; ++i) {
                xxh3_mix32(acc, p + 32 * i, p + 32 * i + 16, s + Xxh3MidStartAt + 32 * (i - 4), seed);
            }
            xxh3_mix32(acc, p + n - 16, p + n - 32, s + Xxh3MidLastAt - 16, 0 - seed);
        }
        const uint64_t low = acc.low + acc.high;
        const uint64_t high = acc.low * Xxh64Prime1 + acc.high * Xxh64Prime4 + (n - seed) * Xxh64Prime2;
        return {xxh3_avalanche(low), 0 - xxh3_avalanche(high)};
    }

    // The eight lanes of the long path
    struct Xxh3Lanes {
        uint64_t acc[8] = {Xxh32Prime3, Xxh64Prime1, Xxh64Prime2, Xxh64Prime3, Xxh64Prime4, Xxh32Prime2, Xxh64Prime5, Xxh32Prime1};

        // One stripe of 64 bytes against the secret at s
        void stripe(const unsigned char* p, const unsigned char* s) noexcept {
            for (size_t i = 0; i < 8; ++i) {
                const uint64_t data = load_le64(p + 8 * i);
                const uint64_t keyed = data ^ load_le64(s + 8 * i);
                acc[i ^ 1] += data;
                acc[i] += (keyed & 0xffffffffu) * (keyed >> 32);
            }
        }

        // After a block: every lane shaken and multiplied
        void scramble(const unsigned char* s) noexcept {
            for (size_t i = 0; i < 8; ++i) {
                uint64_t a = acc[i];
                a ^= a >> 47;
                a ^= load_le64(s + 8 * i);
                acc[i] = a * Xxh32Prime1;
            }
        }

        // `count` stripes from p, the first of them stripe `first` of its
        // block; a block completed is scrambled. Every stripe given here has
        // a byte after it. The index of the next stripe in its block
        size_t stripes(const unsigned char* p, size_t count, size_t first, const unsigned char* secret) noexcept {
#if defined(SGCL_HASH_XXH3_NEON)
            // The lanes in four vectors of two, the whole run of stripes:
            // a stripe is four loads, four XORs, four narrowings of each
            // half and four widening products, and the words swapped
            // within each pair for the neighbour's sum. The compiler does
            // not find this in the loop of stripe() (it keeps eight scalar
            // multiply-adds)
            uint64x2_t a[4];
            for (size_t j = 0; j < 4; ++j) {
                a[j] = vld1q_u64(acc + 2 * j);
            }
            for (size_t k = 0; k < count; ++k) {
                const unsigned char* d = p + Xxh3StripeSize * k;
                const unsigned char* s = secret + 8 * first;
                for (size_t j = 0; j < 4; ++j) {
                    const uint64x2_t data = vreinterpretq_u64_u8(vld1q_u8(d + 16 * j));
                    const uint64x2_t keyed = veorq_u64(data, vreinterpretq_u64_u8(vld1q_u8(s + 16 * j)));
                    const uint64x2_t product = vmull_u32(vmovn_u64(keyed), vshrn_n_u64(keyed, 32));
                    a[j] = vaddq_u64(vaddq_u64(a[j], vextq_u64(data, data, 1)), product);
                }
                if (++first == Xxh3StripesPerBlock) {
                    for (size_t j = 0; j < 4; ++j) {
                        vst1q_u64(acc + 2 * j, a[j]);
                    }
                    scramble(secret + Xxh3ScrambleAt);
                    for (size_t j = 0; j < 4; ++j) {
                        a[j] = vld1q_u64(acc + 2 * j);
                    }
                    first = 0;
                }
            }
            for (size_t j = 0; j < 4; ++j) {
                vst1q_u64(acc + 2 * j, a[j]);
            }
#else
            for (size_t k = 0; k < count; ++k) {
                stripe(p + Xxh3StripeSize * k, secret + 8 * first);
                if (++first == Xxh3StripesPerBlock) {
                    scramble(secret + Xxh3ScrambleAt);
                    first = 0;
                }
            }
#endif
            return first;
        }

        // Four lanes' pairs into one word, from `start`
        uint64_t merge(const unsigned char* s, uint64_t start) const noexcept {
            uint64_t h = start;
            for (size_t i = 0; i < 4; ++i) {
                h += fold_product(acc[2 * i] ^ load_le64(s + 16 * i), acc[2 * i + 1] ^ load_le64(s + 16 * i + 8));
            }
            return xxh3_avalanche(h);
        }

        uint64_t result64(const unsigned char* secret, uint64_t n) const noexcept {
            return merge(secret + Xxh3MergeAt, n * Xxh64Prime1);
        }

        Wide result128(const unsigned char* secret, uint64_t n) const noexcept {
            return {merge(secret + Xxh3MergeAt, n * Xxh64Prime1), merge(secret + Xxh3MergeHighAt, ~(n * Xxh64Prime2))};
        }
    };

    // The lanes after all of an input longer than 240 bytes
    inline Xxh3Lanes xxh3_long(const unsigned char* p, size_t n, const unsigned char* secret) noexcept {
        Xxh3Lanes lanes;
        const size_t whole = (n - 1) / Xxh3StripeSize;   // the stripes with a byte after them
        lanes.stripes(p, whole, 0, secret);
        lanes.stripe(p + n - Xxh3StripeSize, secret + Xxh3LastStripeAt);
        return lanes;
    }

    // The secret an input longer than 240 bytes reads: the default for a
    // seed of 0, else the seed's, made into `room`
    inline const unsigned char* xxh3_secret(uint64_t seed, unsigned char (&room)[Xxh3SecretSize]) noexcept {
        if (seed == 0) {
            return Xxh3Secret;
        }
        xxh3_derive_secret(room, seed);
        return room;
    }

    // The same with the seed's secret made already (maphash keeps its
    // process's)
    inline uint64_t xxh3_64(const unsigned char* p, size_t n, uint64_t seed, const unsigned char* secret) noexcept {
        if (n <= Xxh3ShortLimit) {
            return xxh3_64_short(p, n, seed);
        }
        return xxh3_long(p, n, secret).result64(secret, n);
    }

    inline uint64_t xxh3_64(const unsigned char* p, size_t n, uint64_t seed) noexcept {
        if (n <= Xxh3ShortLimit) {
            return xxh3_64_short(p, n, seed);
        }
        unsigned char room[Xxh3SecretSize];
        const unsigned char* secret = xxh3_secret(seed, room);
        return xxh3_long(p, n, secret).result64(secret, n);
    }

    inline Wide xxh3_128(const unsigned char* p, size_t n, uint64_t seed) noexcept {
        if (n <= Xxh3ShortLimit) {
            return xxh3_128_short(p, n, seed);
        }
        unsigned char room[Xxh3SecretSize];
        const unsigned char* secret = xxh3_secret(seed, room);
        return xxh3_long(p, n, secret).result128(secret, n);
    }

    // The streaming state of both widths. A plain value: no pointer to
    // itself (the secret is chosen at each use), so a copy is a branch
    class Xxh3Stream {
    public:
        Xxh3Stream() noexcept = default;

        explicit Xxh3Stream(uint64_t seed) noexcept
        : _seed(seed) {
        }

        void update(const unsigned char* p, size_t n) noexcept {
            _total += n;
            size_t room = Xxh3BufferSize - _buffered;
            if (n <= room) {   // nothing is taken while no byte follows it
                std::memcpy(_buffer + _buffered, p, n);
                _buffered += n;
                return;
            }
            const unsigned char* secret = _secret_for_lanes();
            if (_buffered > 0) {   // the buffer filled up and more follows: its four stripes go in
                std::memcpy(_buffer + _buffered, p, room);
                p += room;
                n -= room;
                _stripe = _lanes.stripes(_buffer, 4, _stripe, secret);
                _buffered = 0;
            }
            if (n > Xxh3BufferSize) {   // whole stripes straight from the input, each with a byte after it
                const size_t count = (n - 1) / Xxh3StripeSize;
                _stripe = _lanes.stripes(p, count, _stripe, secret);
                p += count * Xxh3StripeSize;
                n -= count * Xxh3StripeSize;
                // the last stripe taken, where the final stripe of a short
                // tail reaches back into
                std::memcpy(_buffer + Xxh3BufferSize - Xxh3StripeSize, p - Xxh3StripeSize, Xxh3StripeSize);
            }
            std::memcpy(_buffer, p, n);
            _buffered = n;
        }

        uint64_t value64() const noexcept {
            if (_total <= Xxh3ShortLimit) {
                return xxh3_64_short(_buffer, _buffered, _seed);
            }
            unsigned char room[Xxh3SecretSize];
            const unsigned char* secret = _secret_for_result(room);
            return _finish(secret).result64(secret, _total);
        }

        Wide value128() const noexcept {
            if (_total <= Xxh3ShortLimit) {
                return xxh3_128_short(_buffer, _buffered, _seed);
            }
            unsigned char room[Xxh3SecretSize];
            const unsigned char* secret = _secret_for_result(room);
            return _finish(secret).result128(secret, _total);
        }

        void reset() noexcept {
            *this = Xxh3Stream(_seed);
        }

    private:
        // The lanes as they would be if the input ended here: the stripes
        // still in the buffer with a byte after them, then the last 64
        // bytes, which may begin in the stripe taken before the buffer
        Xxh3Lanes _finish(const unsigned char* secret) const noexcept {
            Xxh3Lanes lanes = _lanes;
            const size_t count = (_buffered - 1) / Xxh3StripeSize;
            lanes.stripes(_buffer, count, _stripe, secret);
            if (_buffered >= Xxh3StripeSize) {
                lanes.stripe(_buffer + _buffered - Xxh3StripeSize, secret + Xxh3LastStripeAt);
            } else {
                unsigned char last[Xxh3StripeSize];
                const size_t before = Xxh3StripeSize - _buffered;
                std::memcpy(last, _buffer + Xxh3BufferSize - before, before);
                std::memcpy(last + before, _buffer, _buffered);
                lanes.stripe(last, secret + Xxh3LastStripeAt);
            }
            return lanes;
        }

        // The seed's secret, made once, when the first stripe is taken
        const unsigned char* _secret_for_lanes() noexcept {
            if (_seed == 0) {
                return Xxh3Secret;
            }
            if (!_derived) {
                xxh3_derive_secret(_secret, _seed);
                _derived = true;
            }
            return _secret;
        }

        // The same for a result, which may come before any stripe was taken
        // (241–256 bytes, all in the buffer)
        const unsigned char* _secret_for_result(unsigned char (&room)[Xxh3SecretSize]) const noexcept {
            if (_seed == 0) {
                return Xxh3Secret;
            }
            if (_derived) {
                return _secret;
            }
            xxh3_derive_secret(room, _seed);
            return room;
        }

        Xxh3Lanes _lanes;
        uint64_t _seed = 0;
        uint64_t _total = 0;      // bytes taken in
        size_t _buffered = 0;     // bytes in the buffer no stripe has taken
        size_t _stripe = 0;       // the index of the next stripe in its block
        bool _derived = false;    // _secret holds the seed's secret
        unsigned char _buffer[Xxh3BufferSize];
        unsigned char _secret[Xxh3SecretSize];
    };
}
