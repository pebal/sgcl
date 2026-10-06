//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "blake2.h"
#include "bytes.h"
#include "paths.h"
#include "../secure_zero.h"
#include "../../async/parallel.h"
#include "../../core/detail/bytes.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>

// Argon2 (RFC 9106): a memory of m blocks of 1 KiB in p lanes, filled in
// t passes, each block the compression G of the block before it and a
// block chosen from what is filled so far; the tag the long hash H' of the
// lanes' last blocks XORed together. Version 0x13 alone.
//
// G (§3.5) is BLAKE2b's round function over the block seen as an 8x8
// matrix of 16-byte registers, applied to each row and then to each
// column, with the multiplication 2 * lo(a) * lo(b) added to each sum
// (BlaMka): additions, multiplications of 32-bit halves, XORs and
// rotations by constants. The blocks a lane refers to are chosen from
// the password's data in Argon2d and in Argon2id after the first half of
// the first pass (by design: RFC 9106 trades side channels for resistance
// to trade-offs there), and from a counter alone in Argon2i and in the
// first half of Argon2id's first pass.
//
// The lanes of one slice (a quarter of a pass) are independent: they run
// on the scheduler's workers through async::parallel_for when there are
// several and a segment is long enough to be worth a lane; the result is
// the same either way.
namespace sgcl::crypto::detail {
    inline constexpr uint32_t argon2_version = 0x13;
    inline constexpr size_t argon2_block_words = 128;
    inline constexpr uint32_t argon2_sync_points = 4;

    struct Argon2Block {
        uint64_t v[argon2_block_words];
    };

    // BlaMka's G of BLAKE2b's round: a = a + b + 2 lo(a) lo(b)
    SGCL_INLINE_HOT uint64_t argon2_fbla(uint64_t a, uint64_t b) noexcept {
        return a + b + 2 * (uint64_t(uint32_t(a)) * uint64_t(uint32_t(b)));
    }

    // The permutation P over sixteen words, the indices of the words given
    // (a row: 16 consecutive words; a column: pairs 16 apart)
    template<size_t... W>
    SGCL_INLINE_HOT void argon2_p(uint64_t* v) noexcept {
        constexpr size_t w[16] = {W...};
        auto g = [&](size_t a, size_t b, size_t c, size_t d) SGCL_CRYPTO_LAMBDA_INLINE {
            v[a] = argon2_fbla(v[a], v[b]);
            v[d] = std::rotr(v[d] ^ v[a], 32);
            v[c] = argon2_fbla(v[c], v[d]);
            v[b] = std::rotr(v[b] ^ v[c], 24);
            v[a] = argon2_fbla(v[a], v[b]);
            v[d] = std::rotr(v[d] ^ v[a], 16);
            v[c] = argon2_fbla(v[c], v[d]);
            v[b] = std::rotr(v[b] ^ v[c], 63);
        };
        g(w[0], w[4], w[8], w[12]);
        g(w[1], w[5], w[9], w[13]);
        g(w[2], w[6], w[10], w[14]);
        g(w[3], w[7], w[11], w[15]);
        g(w[0], w[5], w[10], w[15]);
        g(w[1], w[6], w[11], w[12]);
        g(w[2], w[7], w[8], w[13]);
        g(w[3], w[4], w[9], w[14]);
    }

    template<size_t I>
    SGCL_INLINE_HOT void argon2_row(uint64_t* r) noexcept {
        [&]<size_t... K>(std::index_sequence<K...>) SGCL_CRYPTO_LAMBDA_INLINE {
            argon2_p<(16 * I + K)...>(r);
        }(std::make_index_sequence<16>());
    }

    // column I: registers I, I + 8, ..., I + 56, each two words
    template<size_t I>
    SGCL_INLINE_HOT void argon2_column(uint64_t* r) noexcept {
        [&]<size_t... K>(std::index_sequence<K...>) SGCL_CRYPTO_LAMBDA_INLINE {
            argon2_p<(2 * I + 16 * (K / 2) + (K % 2))...>(r);
        }(std::make_index_sequence<16>());
    }

    // out = G(x, y), or out ^= G(x, y) when `xor_into` (the passes after
    // the first, version 0x13)
    inline void argon2_g_portable(Argon2Block& out, const Argon2Block& x, const Argon2Block& y, bool xor_into) noexcept {
        Argon2Block r, z;
        for (size_t i = 0; i < argon2_block_words; ++i) {
            r.v[i] = x.v[i] ^ y.v[i];
        }
        if (xor_into) {
            for (size_t i = 0; i < argon2_block_words; ++i) {
                z.v[i] = r.v[i] ^ out.v[i];
            }
        } else {
            z = r;
        }
        [&]<size_t... I>(std::index_sequence<I...>) SGCL_CRYPTO_LAMBDA_INLINE {
            (argon2_row<I>(r.v), ...);
            (argon2_column<I>(r.v), ...);
        }(std::make_index_sequence<8>());
        for (size_t i = 0; i < argon2_block_words; ++i) {
            out.v[i] = z.v[i] ^ r.v[i];
        }
    }

#if defined(SGCL_CRYPTO_ARM64)
    // G on NEON: a 16-byte register of the 8x8 matrix is a vector of two
    // words, so the permutation of a row (eight consecutive registers) or a
    // column (eight registers 128 bytes apart) is BLAKE2b's round over
    // rows a, b, c, d of two vectors each, the diagonal step turning b, c
    // and d by EXT. BlaMka's product of the low halves is one UMULL of the
    // two words' low halves packed by UZP1; the rotations are XAR of the
    // SHA-3 extension (xor and rotate in one). Two permutations are run
    // side by side, so that one's chain of dependent operations fills the
    // other's latency.
    struct Argon2NeonP {
        uint64x2_t a0, a1, b0, b1, c0, c1, d0, d1;
    };

    SGCL_INLINE_ARM64_SHA3 inline uint64x2_t argon2_fbla_neon(uint64x2_t a, uint64x2_t b) noexcept {
        uint32x4_t lows = vuzp1q_u32(vreinterpretq_u32_u64(a), vreinterpretq_u32_u64(b));
        uint64x2_t prod = vmull_u32(vget_low_u32(lows), vget_high_u32(lows));
        return vaddq_u64(vaddq_u64(a, b), vaddq_u64(prod, prod));
    }

    SGCL_INLINE_ARM64_SHA3 inline void argon2_gb_neon(Argon2NeonP& x) noexcept {
        x.a0 = argon2_fbla_neon(x.a0, x.b0);
        x.a1 = argon2_fbla_neon(x.a1, x.b1);
        x.d0 = vxarq_u64(x.d0, x.a0, 32);
        x.d1 = vxarq_u64(x.d1, x.a1, 32);
        x.c0 = argon2_fbla_neon(x.c0, x.d0);
        x.c1 = argon2_fbla_neon(x.c1, x.d1);
        x.b0 = vxarq_u64(x.b0, x.c0, 24);
        x.b1 = vxarq_u64(x.b1, x.c1, 24);
        x.a0 = argon2_fbla_neon(x.a0, x.b0);
        x.a1 = argon2_fbla_neon(x.a1, x.b1);
        x.d0 = vxarq_u64(x.d0, x.a0, 16);
        x.d1 = vxarq_u64(x.d1, x.a1, 16);
        x.c0 = argon2_fbla_neon(x.c0, x.d0);
        x.c1 = argon2_fbla_neon(x.c1, x.d1);
        x.b0 = vxarq_u64(x.b0, x.c0, 63);
        x.b1 = vxarq_u64(x.b1, x.c1, 63);
    }

    SGCL_INLINE_ARM64_SHA3 inline void argon2_diagonalize_neon(Argon2NeonP& x) noexcept {
        uint64x2_t t = vextq_u64(x.b0, x.b1, 1);
        x.b1 = vextq_u64(x.b1, x.b0, 1);
        x.b0 = t;
        std::swap(x.c0, x.c1);
        t = vextq_u64(x.d1, x.d0, 1);
        x.d1 = vextq_u64(x.d0, x.d1, 1);
        x.d0 = t;
    }

    SGCL_INLINE_ARM64_SHA3 inline void argon2_undiagonalize_neon(Argon2NeonP& x) noexcept {
        uint64x2_t t = vextq_u64(x.b1, x.b0, 1);
        x.b1 = vextq_u64(x.b0, x.b1, 1);
        x.b0 = t;
        std::swap(x.c0, x.c1);
        t = vextq_u64(x.d0, x.d1, 1);
        x.d1 = vextq_u64(x.d1, x.d0, 1);
        x.d0 = t;
    }

    // registers r[at(0)] .. r[at(7)] of the matrix as one P's rows
    SGCL_INLINE_ARM64_SHA3 inline Argon2NeonP argon2_load_p(const uint64x2_t* r, size_t first, size_t stride) noexcept {
        return {r[first], r[first + stride], r[first + 2 * stride], r[first + 3 * stride],
                r[first + 4 * stride], r[first + 5 * stride], r[first + 6 * stride], r[first + 7 * stride]};
    }

    SGCL_INLINE_ARM64_SHA3 inline void argon2_store_p(uint64x2_t* r, size_t first, size_t stride, const Argon2NeonP& x) noexcept {
        r[first] = x.a0;
        r[first + stride] = x.a1;
        r[first + 2 * stride] = x.b0;
        r[first + 3 * stride] = x.b1;
        r[first + 4 * stride] = x.c0;
        r[first + 5 * stride] = x.c1;
        r[first + 6 * stride] = x.d0;
        r[first + 7 * stride] = x.d1;
    }

    SGCL_INLINE_ARM64_SHA3 inline void argon2_two_p_neon(uint64x2_t* r, size_t first0, size_t first1, size_t stride) noexcept {
        Argon2NeonP x = argon2_load_p(r, first0, stride), y = argon2_load_p(r, first1, stride);
        argon2_gb_neon(x);
        argon2_gb_neon(y);
        argon2_diagonalize_neon(x);
        argon2_diagonalize_neon(y);
        argon2_gb_neon(x);
        argon2_gb_neon(y);
        argon2_undiagonalize_neon(x);
        argon2_undiagonalize_neon(y);
        argon2_store_p(r, first0, stride, x);
        argon2_store_p(r, first1, stride, y);
    }

    SGCL_TARGET_ARM64_SHA3
    inline void argon2_g_neon(Argon2Block& out, const Argon2Block& x, const Argon2Block& y, bool xor_into) noexcept {
        uint64x2_t r[64], z[64];
        for (size_t i = 0; i < 64; ++i) {
            r[i] = veorq_u64(vld1q_u64(x.v + 2 * i), vld1q_u64(y.v + 2 * i));
            z[i] = xor_into ? veorq_u64(r[i], vld1q_u64(out.v + 2 * i)) : r[i];
        }
        for (size_t i = 0; i < 8; i += 2) {
            argon2_two_p_neon(r, 8 * i, 8 * (i + 1), 1);   // rows i and i + 1
        }
        for (size_t i = 0; i < 8; i += 2) {
            argon2_two_p_neon(r, i, i + 1, 8);   // columns i and i + 1
        }
        for (size_t i = 0; i < 64; ++i) {
            vst1q_u64(out.v + 2 * i, veorq_u64(z[i], r[i]));
        }
    }
#endif

#if defined(SGCL_CRYPTO_X86)
    // G on SSE2 (x86-64's minimum, no gate), the NEON path's layout: the
    // product of the low halves is PMULUDQ, which multiplies exactly those;
    // the rotations by 32 a shuffle of the halves, the others shifts.
    // Compiled and checked under the x86-64 target (and run under
    // emulation); not measured on an x86 machine.
    struct Argon2SseP {
        __m128i a0, a1, b0, b1, c0, c1, d0, d1;
    };

    SGCL_INLINE_HOT __m128i argon2_fbla_sse2(__m128i a, __m128i b) noexcept {
        __m128i prod = _mm_mul_epu32(a, b);
        return _mm_add_epi64(_mm_add_epi64(a, b), _mm_add_epi64(prod, prod));
    }

    template<int R>
    SGCL_INLINE_HOT __m128i argon2_rotr_sse2(__m128i x) noexcept {
        if constexpr (R == 32) {
            return _mm_shuffle_epi32(x, 0xb1);
        } else {
            return _mm_or_si128(_mm_srli_epi64(x, R), _mm_slli_epi64(x, 64 - R));
        }
    }

    SGCL_INLINE_HOT void argon2_gb_sse2(Argon2SseP& x) noexcept {
        x.a0 = argon2_fbla_sse2(x.a0, x.b0);
        x.a1 = argon2_fbla_sse2(x.a1, x.b1);
        x.d0 = argon2_rotr_sse2<32>(_mm_xor_si128(x.d0, x.a0));
        x.d1 = argon2_rotr_sse2<32>(_mm_xor_si128(x.d1, x.a1));
        x.c0 = argon2_fbla_sse2(x.c0, x.d0);
        x.c1 = argon2_fbla_sse2(x.c1, x.d1);
        x.b0 = argon2_rotr_sse2<24>(_mm_xor_si128(x.b0, x.c0));
        x.b1 = argon2_rotr_sse2<24>(_mm_xor_si128(x.b1, x.c1));
        x.a0 = argon2_fbla_sse2(x.a0, x.b0);
        x.a1 = argon2_fbla_sse2(x.a1, x.b1);
        x.d0 = argon2_rotr_sse2<16>(_mm_xor_si128(x.d0, x.a0));
        x.d1 = argon2_rotr_sse2<16>(_mm_xor_si128(x.d1, x.a1));
        x.c0 = argon2_fbla_sse2(x.c0, x.d0);
        x.c1 = argon2_fbla_sse2(x.c1, x.d1);
        x.b0 = argon2_rotr_sse2<63>(_mm_xor_si128(x.b0, x.c0));
        x.b1 = argon2_rotr_sse2<63>(_mm_xor_si128(x.b1, x.c1));
    }

    // the two words of a register turned across a pair: (lo of hi, hi of lo)
    SGCL_INLINE_HOT __m128i argon2_cross_sse2(__m128i lo, __m128i hi) noexcept {
        return _mm_unpacklo_epi64(_mm_unpackhi_epi64(lo, lo), hi);
    }

    SGCL_INLINE_HOT void argon2_diagonalize_sse2(Argon2SseP& x) noexcept {
        __m128i t = argon2_cross_sse2(x.b0, x.b1);
        x.b1 = argon2_cross_sse2(x.b1, x.b0);
        x.b0 = t;
        std::swap(x.c0, x.c1);
        t = argon2_cross_sse2(x.d1, x.d0);
        x.d1 = argon2_cross_sse2(x.d0, x.d1);
        x.d0 = t;
    }

    SGCL_INLINE_HOT void argon2_undiagonalize_sse2(Argon2SseP& x) noexcept {
        __m128i t = argon2_cross_sse2(x.b1, x.b0);
        x.b1 = argon2_cross_sse2(x.b0, x.b1);
        x.b0 = t;
        std::swap(x.c0, x.c1);
        t = argon2_cross_sse2(x.d0, x.d1);
        x.d1 = argon2_cross_sse2(x.d1, x.d0);
        x.d0 = t;
    }

    SGCL_INLINE_HOT void argon2_p_sse2(__m128i* r, size_t first, size_t stride) noexcept {
        Argon2SseP x{r[first], r[first + stride], r[first + 2 * stride], r[first + 3 * stride],
                     r[first + 4 * stride], r[first + 5 * stride], r[first + 6 * stride], r[first + 7 * stride]};
        argon2_gb_sse2(x);
        argon2_diagonalize_sse2(x);
        argon2_gb_sse2(x);
        argon2_undiagonalize_sse2(x);
        r[first] = x.a0;
        r[first + stride] = x.a1;
        r[first + 2 * stride] = x.b0;
        r[first + 3 * stride] = x.b1;
        r[first + 4 * stride] = x.c0;
        r[first + 5 * stride] = x.c1;
        r[first + 6 * stride] = x.d0;
        r[first + 7 * stride] = x.d1;
    }

    inline void argon2_g_sse2(Argon2Block& out, const Argon2Block& x, const Argon2Block& y, bool xor_into) noexcept {
        __m128i r[64], z[64];
        for (size_t i = 0; i < 64; ++i) {
            r[i] = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x.v + 2 * i)),
                                 _mm_loadu_si128(reinterpret_cast<const __m128i*>(y.v + 2 * i)));
            z[i] = xor_into ? _mm_xor_si128(r[i], _mm_loadu_si128(reinterpret_cast<const __m128i*>(out.v + 2 * i))) : r[i];
        }
        for (size_t i = 0; i < 8; ++i) {
            argon2_p_sse2(r, 8 * i, 1);
        }
        for (size_t i = 0; i < 8; ++i) {
            argon2_p_sse2(r, i, 8);
        }
        for (size_t i = 0; i < 64; ++i) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out.v + 2 * i), _mm_xor_si128(z[i], r[i]));
        }
    }
#endif

    SGCL_INLINE_HOT void argon2_g(Argon2Block& out, const Argon2Block& x, const Argon2Block& y, bool xor_into) noexcept {
#if defined(SGCL_CRYPTO_X86)
        argon2_g_sse2(out, x, y, xor_into);
        return;
#endif
#if defined(SGCL_CRYPTO_ARM64)
        if (sgcl::detail::cpu::sha3()) {
            argon2_g_neon(out, x, y, xor_into);
            return;
        }
#endif
        argon2_g_portable(out, x, y, xor_into);
    }

    // H' of §3.3: a hash of any length T from BLAKE2b, LE32(T) || in first
    inline void argon2_hash_long(unsigned char* out, uint32_t t, const unsigned char* in, size_t n,
                                 const unsigned char* in2 = nullptr, size_t n2 = 0) noexcept {
        unsigned char tl[4];
        store_le32(tl, t);
        Blake2State<Blake2bTraits> h;
        if (t <= 64) {
            h.init(t, nullptr, 0, nullptr, 0, nullptr, 0);
            h.update(tl, 4);
            h.update(in, n);
            h.update(in2, n2);
            h.finish(out);
            secure_zero_object(h);
            return;
        }
        // r = ceil(T / 32) - 2 hashes of 64 bytes, the first 32 bytes of each
        // out, then the last T - 32 r bytes
        uint32_t r = (t + 31) / 32 - 2;
        unsigned char v[64];
        h.init(64, nullptr, 0, nullptr, 0, nullptr, 0);
        h.update(tl, 4);
        h.update(in, n);
        h.update(in2, n2);
        h.finish(v);
        std::memcpy(out, v, 32);
        for (uint32_t i = 1; i < r; ++i) {
            h.init(64, nullptr, 0, nullptr, 0, nullptr, 0);
            h.update(v, 64);
            h.finish(v);
            std::memcpy(out + 32 * i, v, 32);
        }
        uint32_t last = t - 32 * r;
        h.init(last, nullptr, 0, nullptr, 0, nullptr, 0);
        h.update(v, 64);
        h.finish(out + 32 * r);
        secure_zero(v, sizeof v);
        secure_zero_object(h);
    }

    struct Argon2Params {
        const unsigned char* password;
        size_t password_size;
        const unsigned char* salt;
        size_t salt_size;
        const unsigned char* secret;
        size_t secret_size;
        const unsigned char* ad;
        size_t ad_size;
        uint32_t variant;   // y: 0 d, 1 i, 2 id
        uint32_t memory;    // m, KiB
        uint32_t passes;    // t
        uint32_t lanes;     // p
    };

    // The memory of a run: plain memory from the system, never managed (it
    // holds what the password makes, and may be gigabytes), zeroed before
    // it is freed; ::operator new as the module's secrets have it
    struct Argon2Memory {
        Argon2Block* blocks = nullptr;
        size_t count = 0;

        explicit Argon2Memory(size_t n) noexcept : blocks(static_cast<Argon2Block*>(::operator new(n * sizeof(Argon2Block), std::align_val_t(64)))), count(n) {
        }

        Argon2Memory(const Argon2Memory&) = delete;
        Argon2Memory& operator=(const Argon2Memory&) = delete;

        ~Argon2Memory() {
            secure_zero(blocks, count * sizeof(Argon2Block));
            ::operator delete(blocks, count * sizeof(Argon2Block), std::align_val_t(64));
        }
    };

    class Argon2Run {
    public:
        // m' = 4 p floor(m / 4p) blocks, q = m' / p of them a lane
        explicit Argon2Run(const Argon2Params& p) noexcept
        : _p(p),
          _q(4 * (p.memory / (4 * p.lanes))),
          _segment(_q / argon2_sync_points),
          _memory(size_t(_q) * p.lanes) {
        }

        // The tag, tag_size bytes (at least 4), into tag
        void run(unsigned char* tag, uint32_t tag_size) noexcept {
            _tag_size = tag_size;
            _first_blocks();
            const bool parallel = _p.lanes > 1 && _segment >= 64;
            for (uint32_t pass = 0; pass < _p.passes; ++pass) {
                for (uint32_t slice = 0; slice < argon2_sync_points; ++slice) {
                    if (parallel) {
                        async::parallel_for(_p.lanes, [&](uint32_t lane) noexcept { _segment_fill(pass, lane, slice); },
                                            {.lanes = _p.lanes, .grain = 1});
                    } else {
                        for (uint32_t lane = 0; lane < _p.lanes; ++lane) {
                            _segment_fill(pass, lane, slice);
                        }
                    }
                }
            }
            // the final block: the lanes' last blocks XORed, through H'
            Argon2Block c = _block(0, _q - 1);
            for (uint32_t lane = 1; lane < _p.lanes; ++lane) {
                const Argon2Block& b = _block(lane, _q - 1);
                for (size_t i = 0; i < argon2_block_words; ++i) {
                    c.v[i] ^= b.v[i];
                }
            }
            unsigned char bytes_c[1024];
            for (size_t i = 0; i < argon2_block_words; ++i) {
                store_le64(bytes_c + 8 * i, c.v[i]);
            }
            argon2_hash_long(tag, tag_size, bytes_c, sizeof bytes_c);
            secure_zero(bytes_c, sizeof bytes_c);
            secure_zero_object(c);
        }

    private:
        Argon2Params _p;
        uint32_t _tag_size = 32;
        uint32_t _q;         // columns: blocks a lane
        uint32_t _segment;   // blocks a segment
        Argon2Memory _memory;

        SGCL_INLINE_HOT Argon2Block& _block(uint32_t lane, uint32_t column) noexcept {
            return _memory.blocks[size_t(lane) * _q + column];
        }

        // H0 of §3.2, then the first two blocks of every lane
        void _first_blocks() noexcept {
            unsigned char h0[72];
            Blake2State<Blake2bTraits> h;
            h.init(64, nullptr, 0, nullptr, 0, nullptr, 0);
            auto le = [&](uint32_t x) {
                unsigned char b[4];
                store_le32(b, x);
                h.update(b, 4);
            };
            le(_p.lanes);
            le(_tag_size);
            le(_p.memory);
            le(_p.passes);
            le(argon2_version);
            le(_p.variant);
            le(uint32_t(_p.password_size));
            h.update(_p.password, _p.password_size);
            le(uint32_t(_p.salt_size));
            h.update(_p.salt, _p.salt_size);
            le(uint32_t(_p.secret_size));
            h.update(_p.secret, _p.secret_size);
            le(uint32_t(_p.ad_size));
            h.update(_p.ad, _p.ad_size);
            h.finish(h0);
            secure_zero_object(h);
            unsigned char block[1024];
            for (uint32_t lane = 0; lane < _p.lanes; ++lane) {
                for (uint32_t j = 0; j < 2; ++j) {
                    store_le32(h0 + 64, j);
                    store_le32(h0 + 68, lane);
                    argon2_hash_long(block, 1024, h0, 72);
                    Argon2Block& b = _block(lane, j);
                    for (size_t i = 0; i < argon2_block_words; ++i) {
                        b.v[i] = load_le64(block + 8 * i);
                    }
                }
            }
            secure_zero(block, sizeof block);
            secure_zero(h0, sizeof h0);
        }

        // One segment: the blocks of `lane` in `slice` of `pass`
        void _segment_fill(uint32_t pass, uint32_t lane, uint32_t slice) noexcept {
            const bool independent = _p.variant == 1 || (_p.variant == 2 && pass == 0 && slice < 2);
            Argon2Block address, input, zero;
            if (independent) {
                std::memset(&zero, 0, sizeof zero);
                std::memset(&input, 0, sizeof input);
                input.v[0] = pass;
                input.v[1] = lane;
                input.v[2] = slice;
                input.v[3] = uint64_t(_q) * _p.lanes;
                input.v[4] = _p.passes;
                input.v[5] = _p.variant;
            }
            auto next_addresses = [&] {
                ++input.v[6];
                Argon2Block t;
                argon2_g(t, zero, input, false);
                argon2_g(address, zero, t, false);
            };
            uint32_t start = 0;
            if (pass == 0 && slice == 0) {
                start = 2;   // the first two blocks of a lane are H0's
                if (independent) {
                    next_addresses();
                }
            }
            uint32_t column = slice * _segment + start;
            uint32_t prev = column == 0 ? _q - 1 : column - 1;
            for (uint32_t index = start; index < _segment; ++index, ++column, prev = column - 1) {
                uint64_t rand;
                if (independent) {
                    if (index % argon2_block_words == 0) {
                        next_addresses();
                    }
                    rand = address.v[index % argon2_block_words];
                } else {
                    rand = _block(lane, prev).v[0];
                }
                // §3.4.1.2: the lane, then the block within the reference area
                uint32_t ref_lane = uint32_t((rand >> 32) % _p.lanes);
                if (pass == 0 && slice == 0) {
                    ref_lane = lane;
                }
                const bool same = ref_lane == lane;
                uint64_t area;
                if (pass == 0) {
                    area = same ? uint64_t(slice) * _segment + index - 1 : uint64_t(slice) * _segment - (index == 0 ? 1 : 0);
                } else {
                    area = same ? uint64_t(_q) - _segment + index - 1 : uint64_t(_q) - _segment - (index == 0 ? 1 : 0);
                }
                uint64_t j1 = uint32_t(rand);
                uint64_t x = (j1 * j1) >> 32;
                uint64_t y = (area * x) >> 32;
                uint64_t relative = area - 1 - y;
                uint64_t first = pass != 0 && slice != argon2_sync_points - 1 ? uint64_t(slice + 1) * _segment : 0;
                uint32_t ref = uint32_t((first + relative) % _q);
                argon2_g(_block(lane, column), _block(lane, prev), _block(ref_lane, ref), pass != 0);
            }
            if (independent) {
                secure_zero_object(address);
                secure_zero_object(input);
            }
        }
    };
}
