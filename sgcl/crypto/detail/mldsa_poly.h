//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/os.h"
#include "paths.h"

#include <array>
#include <cstddef>
#include <cstdint>

// The ring of ML-DSA (FIPS 204 §2.3, §7.5): polynomials of 256
// coefficients in Z_q, q = 8380417 = 2^23 − 2^13 + 1, modulo X^256 + 1, and
// their number-theoretic transform (Algorithms 41 and 42). A coefficient is
// an int32_t; the transform's products go through Montgomery's reduction
// with R = 2^32, so the constants ζ^BitRev8(k) are kept times R, and a
// pointwise product of two transformed polynomials is a·b·R^-1, which the
// inverse transform's last multiplication puts right (by R/256 instead of
// 1/256). Secrets (s1, s2, t0, y, c·s1, c·s2) go through all of it: no
// branch, no index and no division depends on a value here; a reduction is
// a multiplication and a shift, a conditional addition a mask. The
// constants are computed from ζ = 1753, not typed in.
namespace sgcl::crypto::detail::mldsa {
    inline constexpr int32_t Q = 8380417;
    inline constexpr size_t N = 256;
    inline constexpr unsigned D = 13;              // the bits dropped from t (Power2Round)
    inline constexpr uint32_t QInv = 58728449;     // q^-1 mod 2^32

    using Poly = std::array<int32_t, N>;

    // a·2^-32 mod q in (−q, q), for |a| < 2^31·q
    SGCL_INLINE_HOT constexpr int32_t montgomery(int64_t a) noexcept {
        const int32_t t = int32_t(uint32_t(uint64_t(a)) * QInv);
        return int32_t((a - int64_t(t) * Q) >> 32);
    }

    // a + q for a negative a (a in (−q, q) to [0, q))
    SGCL_INLINE_HOT constexpr int32_t add_q(int32_t a) noexcept {
        return a + ((a >> 31) & Q);
    }

    // a mod q in [0, q) for any int32_t a: a − round(a / 2^23)·q leaves
    // (−q, q) (2^23 is within 2^13 of q), then add_q
    SGCL_INLINE_HOT constexpr int32_t freeze(int32_t a) noexcept {
        const int32_t t = int32_t((int64_t(a) + (1 << 22)) >> 23);
        return add_q(int32_t(int64_t(a) - int64_t(t) * Q));
    }

    // |a| of the representative of a in (−(q−1)/2, (q−1)/2], for a in [0, q)
    SGCL_INLINE_HOT constexpr uint32_t centered_abs(int32_t a) noexcept {
        const int32_t c = a - (((Q - 1) / 2 - a) >> 31 & Q);     // a > (q−1)/2: a − q
        const int32_t s = c >> 31;
        return uint32_t((c ^ s) - s);
    }

    constexpr int64_t power_mod(int64_t b, uint64_t e) noexcept {
        int64_t r = 1;
        b %= Q;
        while (e) {
            if (e & 1) {
                r = r * b % Q;
            }
            b = b * b % Q;
            e >>= 1;
        }
        return r;
    }

    constexpr unsigned bitrev8(unsigned i) noexcept {
        unsigned r = 0;
        for (unsigned k = 0; k < 8; ++k) {
            r |= ((i >> k) & 1) << (7 - k);
        }
        return r;
    }

    // ζ^BitRev8(k)·2^32 mod q, centered in (−q/2, q/2), for k < 256 (k = 0
    // is never read: Algorithm 41 starts at m = 1); and R²/256 mod q, the
    // last factor of the inverse transform
    // (and each times q^-1 mod 2^32, for NEON's Montgomery product)
    struct Tables {
        int32_t zetas[N] = {};
        int32_t zetas_qinv[N] = {};
        int32_t inverse_scale = 0;
        int32_t inverse_scale_qinv = 0;

        constexpr Tables() noexcept {
            constexpr int64_t R = (int64_t(1) << 32) % Q;
            for (unsigned k = 0; k < N; ++k) {
                int64_t z = power_mod(1753, bitrev8(k)) * R % Q;
                zetas[k] = int32_t(z > Q / 2 ? z - Q : z);
                zetas_qinv[k] = int32_t(uint32_t(zetas[k]) * QInv);
            }
            inverse_scale = int32_t(R * R % Q * power_mod(256, Q - 2) % Q);
            inverse_scale_qinv = int32_t(uint32_t(inverse_scale) * QInv);
        }
    };

    inline constexpr Tables tables;

    // Algorithm 41's levels of len from `from` down to `to`, from the m-th
    // constant on
    SGCL_INLINE_HOT void ntt_levels(Poly& w, unsigned from, unsigned to, unsigned& m) noexcept {
        for (unsigned len = from; len >= to; len >>= 1) {
            for (unsigned start = 0; start < N; start += 2 * len) {
                const int64_t z = tables.zetas[++m];
                for (unsigned j = start; j < start + len; ++j) {
                    const int32_t t = montgomery(z * w[j + len]);
                    w[j + len] = w[j] - t;
                    w[j] = w[j] + t;
                }
            }
        }
    }

    // Algorithm 42's levels of len from `from` up to `to`, from the m-th
    // constant down
    SGCL_INLINE_HOT void inverse_levels(Poly& w, unsigned from, unsigned to, unsigned& m) noexcept {
        for (unsigned len = from; len <= to; len <<= 1) {
            for (unsigned start = 0; start < N; start += 2 * len) {
                const int64_t z = -int64_t(tables.zetas[--m]);
                for (unsigned j = start; j < start + len; ++j) {
                    const int32_t t = w[j];
                    w[j] = t + w[j + len];                 // below 256q < 2^31 after the eight levels
                    w[j + len] = montgomery(z * (t - w[j + len]));
                }
            }
        }
    }

    // Algorithm 41, in place: coefficients of |a| < q in, below 9q out
    SGCL_INLINE_HOT void ntt_portable(Poly& w) noexcept {
        unsigned m = 0;
        ntt_levels(w, 128, 1, m);
    }

    // Algorithm 42, in place, times R: coefficients of |a| < q in (a
    // pointwise product's), (−q, q) out, the product's R^-1 undone
    SGCL_INLINE_HOT void inverse_ntt_portable(Poly& w) noexcept {
        unsigned m = N;
        inverse_levels(w, 1, 128, m);
        const int64_t f = tables.inverse_scale;
        for (auto& c : w) {
            c = montgomery(f * c);
        }
    }

#if defined(SGCL_CRYPTO_NEON)
    // Montgomery's product of four lanes by z (zq = z·q^-1 mod 2^32): the
    // high halves of a·z and of t·q, t = a·zq mod 2^32, doubled by vqdmulh
    // and their difference halved; equal low halves make it exact, the
    // scalar montgomery()'s value lane for lane
    SGCL_INLINE_HOT int32x4_t montgomery4(int32x4_t a, int32x4_t z, int32x4_t zq) noexcept {
        const int32x4_t hi = vqdmulhq_s32(a, z);
        const int32x4_t t = vmulq_s32(a, zq);
        return vhsubq_s32(hi, vqdmulhq_s32(t, vdupq_n_s32(Q)));
    }

    // The transform on NEON: the six levels of len 128 to 4, four
    // butterflies an instruction, then the last two in scalar code
    SGCL_INLINE_HOT void ntt(Poly& w) noexcept {
        unsigned m = 0;
        for (unsigned len = 128; len >= 4; len >>= 1) {
            for (unsigned start = 0; start < N; start += 2 * len) {
                ++m;
                const int32x4_t z = vdupq_n_s32(tables.zetas[m]), zq = vdupq_n_s32(tables.zetas_qinv[m]);
                for (unsigned j = start; j < start + len; j += 4) {
                    const int32x4_t a = vld1q_s32(&w[j]);
                    const int32x4_t t = montgomery4(vld1q_s32(&w[j + len]), z, zq);
                    vst1q_s32(&w[j + len], vsubq_s32(a, t));
                    vst1q_s32(&w[j], vaddq_s32(a, t));
                }
            }
        }
        ntt_levels(w, 2, 1, m);
    }

    SGCL_INLINE_HOT void inverse_ntt(Poly& w) noexcept {
        unsigned m = N;
        inverse_levels(w, 1, 2, m);
        for (unsigned len = 4; len < N; len <<= 1) {
            for (unsigned start = 0; start < N; start += 2 * len) {
                --m;
                const int32x4_t z = vdupq_n_s32(-tables.zetas[m]);
                const int32x4_t zq = vdupq_n_s32(int32_t(0u - uint32_t(tables.zetas_qinv[m])));
                for (unsigned j = start; j < start + len; j += 4) {
                    const int32x4_t t = vld1q_s32(&w[j]);
                    const int32x4_t b = vld1q_s32(&w[j + len]);
                    vst1q_s32(&w[j], vaddq_s32(t, b));
                    vst1q_s32(&w[j + len], montgomery4(vsubq_s32(t, b), z, zq));
                }
            }
        }
        const int32x4_t f = vdupq_n_s32(tables.inverse_scale), fq = vdupq_n_s32(tables.inverse_scale_qinv);
        for (size_t j = 0; j < N; j += 4) {
            vst1q_s32(&w[j], montgomery4(vld1q_s32(&w[j]), f, fq));
        }
    }
#else
    SGCL_INLINE_HOT void ntt(Poly& w) noexcept {
        ntt_portable(w);
    }

    SGCL_INLINE_HOT void inverse_ntt(Poly& w) noexcept {
        inverse_ntt_portable(w);
    }
#endif

    // The pointwise product of transformed polynomials, R^-1 times
    SGCL_INLINE_HOT void pointwise(Poly& out, const Poly& a, const Poly& b) noexcept {
        for (size_t i = 0; i < N; ++i) {
            out[i] = montgomery(int64_t(a[i]) * b[i]);
        }
    }

    // Σ a[i] ∘ b[i] over the l polynomials, R^-1 times: the products summed
    // in 64 bits and reduced once (a's coefficients below q, b's below 9q:
    // at most 7·9·q² < 2^31·q)
    template<size_t L>
    SGCL_INLINE_HOT void dot(Poly& out, const Poly* a, const std::array<Poly, L>& b) noexcept {
        for (size_t j = 0; j < N; ++j) {
            int64_t acc = 0;
            for (size_t i = 0; i < L; ++i) {
                acc += int64_t(a[i][j]) * b[i][j];
            }
            out[j] = montgomery(acc);
        }
    }
}
