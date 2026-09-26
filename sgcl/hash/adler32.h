//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "mixin/hasher.h"

#include <cstddef>
#include <cstdint>

// Adler-32 (RFC 1950, section 8.2): the checksum of a zlib stream. Two sums
// modulo 65521, the largest prime below 2^16: a is one plus the bytes, b is
// the sum of every a along the way, and the checksum is b in the high half
// and a in the low.
//
// The loop is written for the compiler to vectorize, which it does, and
// nothing here is an intrinsic. The bytes go in blocks of 32: over a block
// x0…x31, a grows by the sum of the bytes and b by 32 times the a it came
// in with plus Σ (32 − i)·xi, and both sums inside a block are independent
// of each other and of the running values. So the inner loop is two plain
// reductions over 32 bytes, and the running values are touched once a
// block. The modulo is taken once a run of 173 blocks (5536 bytes, the
// largest multiple of 32 within zlib's bound of 5552), where the sums still
// fit 32 bits: the static_assert below works out the worst case, every
// byte 0xFF and a and b one below the modulus coming in.
namespace sgcl::hash {
    namespace detail {
        inline constexpr uint32_t AdlerModulus = 65521;
        inline constexpr size_t AdlerBlock = 32;
        inline constexpr size_t AdlerRunBlocks = 173;

        // The worst case of the sums of one run, in 64 bits
        constexpr bool adler_run_fits() noexcept {
            uint64_t blocks = AdlerRunBlocks;
            uint64_t block_sum = 255 * AdlerBlock;                                  // Σ x over a block
            uint64_t weighted = 255 * (AdlerBlock * (AdlerBlock + 1) / 2);          // Σ (32 − i)·x over a block
            uint64_t earlier = block_sum * (blocks * (blocks - 1) / 2);             // Σ over blocks of the bytes before them
            uint64_t a = AdlerModulus - 1;
            uint64_t b = AdlerModulus - 1 + AdlerBlock * (a * blocks + earlier) + weighted * blocks;
            return earlier <= UINT32_MAX && block_sum * blocks <= UINT32_MAX && weighted * blocks <= UINT32_MAX && b <= UINT32_MAX;
        }
        static_assert(adler_run_fits(), "a run of Adler-32 blocks must fit 32 bits");

        inline uint32_t adler_update(uint32_t a, uint32_t b, const unsigned char* p, size_t n) noexcept {
            while (n >= AdlerBlock) {
                size_t blocks = (n < AdlerRunBlocks * AdlerBlock ? n : AdlerRunBlocks * AdlerBlock) / AdlerBlock;
                n -= blocks * AdlerBlock;
                uint32_t sum = 0;        // the bytes of the run so far
                uint32_t earlier = 0;    // Σ over its blocks of the bytes before each
                uint32_t weighted = 0;   // Σ over its blocks of Σ (32 − i)·xi
                for (size_t k = 0; k < blocks; ++k, p += AdlerBlock) {
                    uint32_t block_sum = 0;
                    uint32_t block_weighted = 0;
                    for (size_t i = 0; i < AdlerBlock; ++i) {
                        block_sum += p[i];
                        block_weighted += uint32_t(AdlerBlock - i) * p[i];
                    }
                    earlier += sum;
                    sum += block_sum;
                    weighted += block_weighted;
                }
                b += uint32_t(AdlerBlock) * (a * uint32_t(blocks) + earlier) + weighted;
                a += sum;
                a %= AdlerModulus;
                b %= AdlerModulus;
            }
            for (size_t i = 0; i < n; ++i) {
                a += p[i];
                b += a;
            }
            a %= AdlerModulus;
            b %= AdlerModulus;
            return (b << 16) | a;
        }
    }

    class adler32 : public mixin::hasher<adler32> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 4;
        static constexpr size_t block_size = 4;

        adler32() noexcept = default;

        // Going on from the checksum of what came before. A value whose
        // halves are not below 65521 is no checksum; it is taken modulo
        static adler32 resume(uint32_t value) noexcept {
            adler32 h;
            h._value = ((value >> 16) % detail::AdlerModulus) << 16 | ((value & 0xffff) % detail::AdlerModulus);
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _value = detail::adler_update(_value & 0xffff, _value >> 16, detail::bytes(data.data()), data.size());
        }

        uint32_t value() const noexcept {
            return _value;
        }

        // The checksum as bytes, the most significant first: what zlib
        // writes at the end of a stream, and Go's Sum
        array<byte, 4> digest() const noexcept {
            return detail::big_endian<4>(_value);
        }

        void reset() noexcept {
            _value = 1;
        }

        // The checksum of A followed by B, from the checksum of A, the
        // checksum of B and the length of B in bytes. Going through B from
        // A's sums instead of from (1, 0) adds a − 1 to every a of the way,
        // so a is a1 + a2 − 1 and b is b1 + b2 + |B|·(a1 − 1), modulo 65521
        static constexpr uint32_t combine(uint32_t first, uint32_t second, uint64_t second_length) noexcept {
            constexpr uint64_t m = detail::AdlerModulus;
            uint64_t a1 = (first & 0xffff) % m;
            uint64_t b1 = (first >> 16) % m;
            uint64_t a2 = (second & 0xffff) % m;
            uint64_t b2 = (second >> 16) % m;
            uint64_t n = second_length % m;
            uint64_t a = (a1 + a2 + m - 1) % m;
            uint64_t b = (b1 + b2 + n * ((a1 + m - 1) % m)) % m;
            return uint32_t(b << 16 | a);
        }

    private:
        uint32_t _value = 1;
    };
}
