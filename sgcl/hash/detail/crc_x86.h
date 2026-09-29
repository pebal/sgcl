//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "crc.h"
#include "crc_fold.h"
#include "../../core/detail/cpu.h"

// The x86-64 path of the CRCs: the folding of crc_arm64.h on PCLMULQDQ
// (the cpu::aes() gate: AES-NI with PCLMULQDQ) for 128 bytes and more, four
// independent 16-byte lanes 64 bytes a step; the 16 bytes a fold leaves,
// and the tail, by slicing by eight. x86 has a CRC-32 instruction for
// Castagnoli's polynomial alone (SSE 4.2), and the fold serves every
// polynomial, so the path is the same for all four. SGCL_HASH_PORTABLE
// takes it out.
#if defined(SGCL_CPU_X86) && !defined(SGCL_HASH_PORTABLE)
#define SGCL_HASH_X86 1

#include <immintrin.h>

namespace sgcl::hash::detail {
    SGCL_INLINE_X86_AES
    inline __m128i fold_lane_x86(__m128i v, uint64_t lo, uint64_t hi) noexcept {
        const __m128i k = _mm_set_epi64x(int64_t(hi), int64_t(lo));
        return _mm_xor_si128(_mm_clmulepi64_si128(v, k, 0x00), _mm_clmulepi64_si128(v, k, 0x11));
    }

    // The whole 64-byte blocks of p[0, n) (n >= 64) folded into 16 bytes
    // whose CRC from a zero register is the register after them; p and n
    // are moved past the blocks
    SGCL_TARGET_X86_AES
    inline __m128i crc_fold_x86(uint64_t reg, const unsigned char*& p, size_t& n, const FoldKeys& k) noexcept {
        auto load = [](const unsigned char* q) SGCL_INLINE_X86_AES {
            return _mm_loadu_si128(reinterpret_cast<const __m128i*>(q));
        };
        __m128i x0 = _mm_xor_si128(load(p), _mm_cvtsi64_si128(int64_t(reg)));
        __m128i x1 = load(p + 16);
        __m128i x2 = load(p + 32);
        __m128i x3 = load(p + 48);
        p += 64;
        n -= 64;
        for (; n >= 64; p += 64, n -= 64) {
            x0 = _mm_xor_si128(fold_lane_x86(x0, k.lo512, k.hi512), load(p));
            x1 = _mm_xor_si128(fold_lane_x86(x1, k.lo512, k.hi512), load(p + 16));
            x2 = _mm_xor_si128(fold_lane_x86(x2, k.lo512, k.hi512), load(p + 32));
            x3 = _mm_xor_si128(fold_lane_x86(x3, k.lo512, k.hi512), load(p + 48));
        }
        return _mm_xor_si128(_mm_xor_si128(fold_lane_x86(x0, k.lo384, k.hi384), fold_lane_x86(x1, k.lo256, k.hi256)),
                             _mm_xor_si128(fold_lane_x86(x2, k.lo128, k.hi128), x3));
    }

    // p[0, n), n >= 128: folded, then the 16 bytes left and the tail by
    // slicing by eight
    template<class T, T Poly>
    SGCL_TARGET_X86_AES
    inline T crc_update_x86(T reg, const unsigned char* p, size_t n) noexcept {
        const __m128i v = crc_fold_x86(uint64_t(reg), p, n, fold_keys<T, Poly>);
        unsigned char folded[16];
        _mm_storeu_si128(reinterpret_cast<__m128i*>(folded), v);
        reg = crc_update_portable<T, Poly>(T(0), folded, 16);
        return crc_update_portable<T, Poly>(reg, p, n);
    }
}
#endif
