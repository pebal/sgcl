//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "cpu.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

// Keccak-f[1600] and the sponge over it (FIPS 202): the permutation of the
// SHA-3 digests and of SHAKE. The state is 25 lanes of 64 bits, lane
// A[x + 5y]; the bytes of the state are the lanes' bytes little-endian,
// lane after lane. Two paths: plain C++, and on arm64 the SHA-3
// instructions of ARMv8.2 (EOR3 for the column sums of θ, RAX1 for its D,
// XAR for θ's XOR with ρ's rotation in one, BCAX for χ), each lane in a
// NEON register of its own, chosen by cpu.h. The round constants and the
// rotation offsets are computed here from the specification's own
// definitions (Algorithms 5 and 2), not typed in.
namespace sgcl::crypto::detail {
    // rc(t) of Algorithm 5: the output bit of an LFSR of degree 8
    constexpr bool keccak_rc_bit(unsigned t) noexcept {
        unsigned r = 1;
        for (unsigned i = 1; i <= t % 255; ++i) {
            r <<= 1;
            if (r & 0x100) {
                r ^= 0x171;   // R[0], R[4], R[5], R[6] ^= R[8], and R[8] dropped
            }
        }
        return r & 1;
    }

    struct KeccakTables {
        uint64_t rc[24];
        unsigned rho[25];   // ρ's rotation of lane x + 5y
        unsigned pi[25];    // where π takes lane x + 5y: lane y + 5((2x + 3y) mod 5)

        constexpr KeccakTables() noexcept
        : rc{}, rho{}, pi{} {
            for (unsigned ir = 0; ir < 24; ++ir) {
                uint64_t c = 0;
                for (unsigned j = 0; j <= 6; ++j) {
                    if (keccak_rc_bit(j + 7 * ir)) {
                        c |= uint64_t(1) << ((1u << j) - 1);
                    }
                }
                rc[ir] = c;
            }
            unsigned x = 1, y = 0;
            for (unsigned t = 0; t < 24; ++t) {
                rho[x + 5 * y] = ((t + 1) * (t + 2) / 2) % 64;
                unsigned nx = y, ny = (2 * x + 3 * y) % 5;
                x = nx;
                y = ny;
            }
            for (unsigned i = 0; i < 25; ++i) {
                unsigned xi = i % 5, yi = i / 5;
                pi[i] = yi + 5 * ((2 * xi + 3 * yi) % 5);
            }
        }
    };

    inline constexpr KeccakTables keccak_tables;

    inline void keccak_permute_portable(uint64_t* a) noexcept {
        for (unsigned round = 0; round < 24; ++round) {
            uint64_t c[5], d[5], b[25];
            for (unsigned x = 0; x < 5; ++x) {
                c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
            }
            for (unsigned x = 0; x < 5; ++x) {
                d[x] = c[(x + 4) % 5] ^ std::rotl(c[(x + 1) % 5], 1);
            }
            for (unsigned i = 0; i < 25; ++i) {
                b[keccak_tables.pi[i]] = std::rotl(a[i] ^ d[i % 5], int(keccak_tables.rho[i]));
            }
            for (unsigned y = 0; y < 25; y += 5) {
                for (unsigned x = 0; x < 5; ++x) {
                    a[y + x] = b[y + x] ^ (~b[y + (x + 1) % 5] & b[y + (x + 2) % 5]);
                }
            }
            a[0] ^= keccak_tables.rc[round];
        }
    }

    inline void keccak_absorb_portable(uint64_t* a, const unsigned char* p, size_t blocks, size_t rate) noexcept {
        for (; blocks != 0; --blocks, p += rate) {
            for (size_t i = 0; i < rate / 8; ++i) {
                a[i] ^= load_le64(p + 8 * i);
            }
            keccak_permute_portable(a);
        }
    }

#if defined(SGCL_CRYPTO_ARM64)
    // The 24 rounds over lanes in registers (the low half of each; the high
    // half carries a copy and is never read). Every index below is a
    // constant of the unrolled fold, so the compiler keeps the 25 lanes in
    // registers rather than in an array.
    SGCL_CRYPTO_INLINE_SHA3
    inline void keccak_rounds_arm64(uint64x2_t (&v)[25]) noexcept {
        for (unsigned round = 0; round < 24; ++round) {
            uint64x2_t c[5], d[5], b[25];
            [&]<size_t... X>(std::index_sequence<X...>) SGCL_CRYPTO_INLINE_SHA3 {
                ((c[X] = veor3q_u64(veor3q_u64(v[X], v[X + 5], v[X + 10]), v[X + 15], v[X + 20])), ...);
                ((d[X] = vrax1q_u64(c[(X + 4) % 5], c[(X + 1) % 5])), ...);
            }(std::make_index_sequence<5>());
            // (the intrinsics are macros that a pack cannot expand through:
            // one lambda call per lane, each with its lane as a constant)
            [&]<size_t... I>(std::index_sequence<I...>) SGCL_CRYPTO_INLINE_SHA3 {
                // θ and ρ in one: (lane ^ D) rotated left by ρ is rotated right by 64 - ρ
                auto theta_rho = [&]<size_t L>(std::integral_constant<size_t, L>) SGCL_CRYPTO_INLINE_SHA3 {
                    b[keccak_tables.pi[L]] = vxarq_u64(v[L], d[L % 5], (64 - keccak_tables.rho[L]) % 64);
                };
                (theta_rho(std::integral_constant<size_t, I>()), ...);
                // χ: b ^ (~b[x + 1] & b[x + 2]) is BCAX(b, b[x + 2], b[x + 1])
                auto chi = [&]<size_t L>(std::integral_constant<size_t, L>) SGCL_CRYPTO_INLINE_SHA3 {
                    v[L] = vbcaxq_u64(b[L], b[L - L % 5 + (L % 5 + 2) % 5], b[L - L % 5 + (L % 5 + 1) % 5]);
                };
                (chi(std::integral_constant<size_t, I>()), ...);
            }(std::make_index_sequence<25>());
            v[0] = veorq_u64(v[0], vdupq_n_u64(keccak_tables.rc[round]));
        }
    }

    // Whole blocks of `Rate` bytes XORed into the state and permuted, the
    // state kept in registers from the first block to the last
    template<size_t Rate>
    SGCL_CRYPTO_TARGET_SHA3
    void keccak_absorb_arm64(uint64_t* a, const unsigned char* p, size_t blocks) noexcept {
        uint64x2_t v[25];
        [&]<size_t... I>(std::index_sequence<I...>) SGCL_CRYPTO_INLINE_SHA3 {
            ((v[I] = vdupq_n_u64(a[I])), ...);
        }(std::make_index_sequence<25>());
        for (; blocks != 0; --blocks, p += Rate) {
            [&]<size_t... I>(std::index_sequence<I...>) SGCL_CRYPTO_INLINE_SHA3 {
                ((v[I] = veorq_u64(v[I], vdupq_n_u64(load_le64(p + 8 * I)))), ...);
            }(std::make_index_sequence<Rate / 8>());
            keccak_rounds_arm64(v);
        }
        [&]<size_t... I>(std::index_sequence<I...>) SGCL_CRYPTO_INLINE_SHA3 {
            auto store = [&]<size_t L>(std::integral_constant<size_t, L>) SGCL_CRYPTO_INLINE_SHA3 {
                a[L] = vgetq_lane_u64(v[L], 0);
            };
            (store(std::integral_constant<size_t, I>()), ...);
        }(std::make_index_sequence<25>());
    }

    SGCL_CRYPTO_TARGET_SHA3
    inline void keccak_permute_arm64(uint64_t* a) noexcept {
        uint64x2_t v[25];
        [&]<size_t... I>(std::index_sequence<I...>) SGCL_CRYPTO_INLINE_SHA3 {
            ((v[I] = vdupq_n_u64(a[I])), ...);
        }(std::make_index_sequence<25>());
        keccak_rounds_arm64(v);
        [&]<size_t... I>(std::index_sequence<I...>) SGCL_CRYPTO_INLINE_SHA3 {
            auto store = [&]<size_t L>(std::integral_constant<size_t, L>) SGCL_CRYPTO_INLINE_SHA3 {
                a[L] = vgetq_lane_u64(v[L], 0);
            };
            (store(std::integral_constant<size_t, I>()), ...);
        }(std::make_index_sequence<25>());
    }
#endif

    inline void keccak_permute(uint64_t* a) noexcept {
#if defined(SGCL_CRYPTO_ARM64)
        if (cpu::sha3()) {
            keccak_permute_arm64(a);
            return;
        }
#endif
        keccak_permute_portable(a);
    }

    template<size_t Rate>
    void keccak_absorb(uint64_t* a, const unsigned char* p, size_t blocks) noexcept {
#if defined(SGCL_CRYPTO_ARM64)
        if (cpu::sha3()) {
            keccak_absorb_arm64<Rate>(a, p, blocks);
            return;
        }
#endif
        keccak_absorb_portable(a, p, blocks, Rate);
    }

    // The sponge of rate `Rate` bytes (capacity 200 - Rate): absorbing
    // until pad(), then squeezing. pos is the byte of the current block,
    // absorbed so far or squeezed so far.
    template<size_t Rate>
    struct KeccakSponge {
        static_assert(Rate % 8 == 0 && Rate < 200);
        static constexpr size_t rate = Rate;

        uint64_t a[25];
        uint32_t pos;

        void init() noexcept {
            std::memset(a, 0, sizeof a);
            pos = 0;
        }

        // n bytes XORed into the state from byte `at` on (at + n <= Rate)
        void xor_in(size_t at, const unsigned char* p, size_t n) noexcept {
            for (size_t i = 0; i < n; ++i, ++at) {
                a[at / 8] ^= uint64_t(p[i]) << (8 * (at % 8));
            }
        }

        void absorb(const unsigned char* p, size_t n) noexcept {
            if (pos != 0) {
                size_t take = std::min(n, Rate - pos);
                xor_in(pos, p, take);
                pos += uint32_t(take);
                p += take;
                n -= take;
                if (pos < Rate) {
                    return;
                }
                keccak_permute(a);
                pos = 0;
            }
            if (n >= Rate) {
                keccak_absorb<Rate>(a, p, n / Rate);
                p += n / Rate * Rate;
                n %= Rate;
            }
            if (n != 0) {
                xor_in(0, p, n);
                pos = uint32_t(n);
            }
        }

        // The domain bits and pad10*1 (the suffix 01 of SHA-3 makes the
        // first byte 0x06, the 1111 of SHAKE 0x1F), then the permutation:
        // the first block of output is ready at pos 0
        void pad(unsigned char first) noexcept {
            a[pos / 8] ^= uint64_t(first) << (8 * (pos % 8));
            a[(Rate - 1) / 8] ^= uint64_t(0x80) << (8 * ((Rate - 1) % 8));
            keccak_permute(a);
            pos = 0;
        }

        void squeeze(unsigned char* out, size_t n) noexcept {
            while (n != 0) {
                if (pos == Rate) {
                    keccak_permute(a);
                    pos = 0;
                }
                size_t take = std::min(n, Rate - pos);
                for (size_t i = 0; i < take; ++i, ++pos) {
                    out[i] = static_cast<unsigned char>(a[pos / 8] >> (8 * (pos % 8)));
                }
                out += take;
                n -= take;
            }
        }
    };
}
