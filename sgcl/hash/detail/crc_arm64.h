//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "crc.h"

// The arm64 path of the CRCs: folding by carry-less multiplication (PMULL)
// for everything from 128 bytes up, and the CRC-32 instructions of ARMv8
// below that. It is compiled where the target has both features, which the
// default arm64-apple-macos target does, so there is no detection at run
// time and no flag in the build. SGCL_HASH_PORTABLE takes it out, for the
// test that the two paths agree and for nothing else. The choice is made
// per translation unit from the target's flags, so every unit of a program
// must be built for the same target and with the same setting of the macro:
// on Linux on arm64, where the default target may lack the two features, one
// file built with -march=armv8-a+crc+crypto and another without would give
// the inline functions below two bodies (an ODR violation) — build the whole
// program with one -march.
//
// Why folding for CRC-32 too, where there is an instruction: crc32x takes
// eight bytes a cycle at best, one after another, since each step needs the
// register the last one left. Folding keeps four independent 16-byte lanes
// in flight and multiplies each forward by a constant, which is what the
// compiler cannot find on its own in a table loop. So one loop serves all
// four CRCs, four constants apiece; the instruction is left for inputs
// under 128 bytes and for turning the last 16 bytes of a fold into a
// register. CRC-64 has no instruction, and there slicing by eight does both.
//
// The arithmetic. A lane holds 16 bytes of the message, its first eight
// bytes (a little-endian word) the higher-degree half: L0·x^64 + L1. The
// same bytes D bits later in the message are congruent, mod P, to
// L0·(x^(64+D) mod P) + L1·(x^D mod P), two products of at most 127 bits,
// which is again 16 bytes and is XORed into the lane D bits on. A
// carry-less product of two reflected words comes out one place low (bit
// m stands for x^(126-m)), so the constants are x^(D+63) and x^(D-1); for a
// 32-bit CRC they are the 32-bit remainders placed in the top half of the
// word, which is the same polynomial in a 64-bit reflected frame. The
// loop moves each lane 512 bits (four lanes of 128) on; at the end the
// first three lanes are moved 384, 256 and 128 bits onto the fourth. The
// start register goes into the first bytes: processing a message from
// register r is processing it from zero with r XORed into its first
// bytes.
#if defined(__aarch64__) && defined(__AARCH64EL__) && defined(__ARM_FEATURE_CRC32) && defined(__ARM_FEATURE_AES) && !defined(SGCL_HASH_PORTABLE)
#define SGCL_HASH_ARM64 1

#include <arm_acle.h>
#include <arm_neon.h>

namespace sgcl::hash::detail {
    struct FoldKeys {
        uint64_t lo512, hi512;   // a lane four lanes on, in the loop
        uint64_t lo384, hi384;   // the lanes onto the last at the end
        uint64_t lo256, hi256;
        uint64_t lo128, hi128;
    };

    template<class T, T Poly>
    constexpr uint64_t fold_key(unsigned e) noexcept {
        return uint64_t(crc_xpow<T, Poly>(e)) << (64 - RegisterBits<T>);
    }

    template<class T, T Poly>
    inline constexpr FoldKeys fold_keys = {
        fold_key<T, Poly>(512 + 63), fold_key<T, Poly>(512 - 1),
        fold_key<T, Poly>(384 + 63), fold_key<T, Poly>(384 - 1),
        fold_key<T, Poly>(256 + 63), fold_key<T, Poly>(256 - 1),
        fold_key<T, Poly>(128 + 63), fold_key<T, Poly>(128 - 1),
    };

    inline uint8x16_t fold_lane(uint8x16_t v, uint64_t lo, uint64_t hi) noexcept {
        poly64x2_t w = vreinterpretq_p64_u8(v);
        poly128_t a = vmull_p64(vgetq_lane_p64(w, 0), poly64_t(lo));
        poly128_t b = vmull_p64(vgetq_lane_p64(w, 1), poly64_t(hi));
        return veorq_u8(vreinterpretq_u8_p128(a), vreinterpretq_u8_p128(b));
    }

    // The whole 64-byte blocks of p[0, n) (n >= 64) folded into 16 bytes
    // whose CRC from a zero register is the register after them; p and n
    // are moved past the blocks, fewer than 64 bytes left
    inline uint8x16_t crc_fold(uint64_t reg, const unsigned char*& p, size_t& n, const FoldKeys& k) noexcept {
        uint8x16_t x0 = veorq_u8(vld1q_u8(p), vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(reg), vcreate_u64(0))));
        uint8x16_t x1 = vld1q_u8(p + 16);
        uint8x16_t x2 = vld1q_u8(p + 32);
        uint8x16_t x3 = vld1q_u8(p + 48);
        p += 64;
        n -= 64;
        for (; n >= 64; p += 64, n -= 64) {
            x0 = veorq_u8(fold_lane(x0, k.lo512, k.hi512), vld1q_u8(p));
            x1 = veorq_u8(fold_lane(x1, k.lo512, k.hi512), vld1q_u8(p + 16));
            x2 = veorq_u8(fold_lane(x2, k.lo512, k.hi512), vld1q_u8(p + 32));
            x3 = veorq_u8(fold_lane(x3, k.lo512, k.hi512), vld1q_u8(p + 48));
        }
        return veorq_u8(veorq_u8(fold_lane(x0, k.lo384, k.hi384), fold_lane(x1, k.lo256, k.hi256)),
                        veorq_u8(fold_lane(x2, k.lo128, k.hi128), x3));
    }

    // The CRC-32 instructions, IEEE or Castagnoli by the polynomial
    template<uint32_t Poly>
    struct Crc32Instructions;

    template<>
    struct Crc32Instructions<0xEDB88320u> {
        static uint32_t word(uint32_t r, uint64_t w) noexcept { return __crc32d(r, w); }
        static uint32_t half(uint32_t r, uint32_t w) noexcept { return __crc32w(r, w); }
        static uint32_t quarter(uint32_t r, uint16_t w) noexcept { return __crc32h(r, w); }
        static uint32_t byte(uint32_t r, uint8_t w) noexcept { return __crc32b(r, w); }
    };

    template<>
    struct Crc32Instructions<0x82F63B78u> {
        static uint32_t word(uint32_t r, uint64_t w) noexcept { return __crc32cd(r, w); }
        static uint32_t half(uint32_t r, uint32_t w) noexcept { return __crc32cw(r, w); }
        static uint32_t quarter(uint32_t r, uint16_t w) noexcept { return __crc32ch(r, w); }
        static uint32_t byte(uint32_t r, uint8_t w) noexcept { return __crc32cb(r, w); }
    };

    // Words of eight, then the last seven bytes as four, two and one: no
    // loop over the bytes of the end
    template<uint32_t Poly>
    inline uint32_t crc32_instructions(uint32_t reg, const unsigned char* p, size_t n) noexcept {
        using I = Crc32Instructions<Poly>;
        for (; n >= 8; p += 8, n -= 8) {
            uint64_t w;
            std::memcpy(&w, p, 8);
            reg = I::word(reg, w);
        }
        if (n & 4) {
            uint32_t w;
            std::memcpy(&w, p, 4);
            reg = I::half(reg, w);
            p += 4;
        }
        if (n & 2) {
            uint16_t w;
            std::memcpy(&w, p, 2);
            reg = I::quarter(reg, w);
            p += 2;
        }
        if (n & 1) {
            reg = I::byte(reg, *p);
        }
        return reg;
    }

    template<uint32_t Poly>
    inline uint32_t crc32_update_arm64(uint32_t reg, const unsigned char* p, size_t n) noexcept {
        if (n >= 128) {
            uint8x16_t v = crc_fold(reg, p, n, fold_keys<uint32_t, Poly>);
            uint64x2_t w = vreinterpretq_u64_u8(v);
            using I = Crc32Instructions<Poly>;
            reg = I::word(I::word(0, vgetq_lane_u64(w, 0)), vgetq_lane_u64(w, 1));
        }
        return crc32_instructions<Poly>(reg, p, n);
    }

    template<uint64_t Poly>
    inline uint64_t crc64_update_arm64(uint64_t reg, const unsigned char* p, size_t n) noexcept {
        if (n >= 128) {
            uint8x16_t v = crc_fold(reg, p, n, fold_keys<uint64_t, Poly>);
            unsigned char folded[16];
            vst1q_u8(folded, v);
            reg = crc_update_portable<uint64_t, Poly>(0, folded, 16);
        }
        return crc_update_portable<uint64_t, Poly>(reg, p, n);
    }
}
#endif

namespace sgcl::hash::detail {
    // The register after p[0, n) by the fastest path the target has
    template<class T, T Poly>
    inline T crc_update(T reg, const unsigned char* p, size_t n) noexcept {
#if defined(SGCL_HASH_ARM64)
        if constexpr (sizeof(T) == 4) {
            return crc32_update_arm64<Poly>(reg, p, n);
        } else {
            return crc64_update_arm64<Poly>(reg, p, n);
        }
#else
        return crc_update_portable<T, Poly>(reg, p, n);
#endif
    }
}
