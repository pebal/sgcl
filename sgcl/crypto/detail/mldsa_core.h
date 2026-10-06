//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../secure_zero.h"
#include "keccak.h"
#include "mldsa_poly.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

// ML-DSA's algorithms (FIPS 204 §6–§7) for the three parameter sets: key
// generation from the seed ξ (Algorithm 6), signing (7) and verification
// (8) of the internal interface, the sampling (29–34), the encodings
// (16–28) and the rounding (35–40). Nothing here allocates: the keys'
// states are the caller's (mldsa.h keeps a private key's in plain memory
// of its own, a public key's in a managed object), the temporaries are on
// the stack, and every temporary that held a secret is zeroed before the
// function returns.
//
// Constant time. Key generation and signing compute over secrets with no
// branch, index or division on a value (mldsa_poly.h; the rounding here is
// multiplications by constants and masks). What branches is public by
// FIPS 204's design: a signing round's acceptance (§3.6.3: a rejected round
// reveals nothing but that it was rejected), SampleInBall's indices (c is a
// part of the signature), the rejection sampling of Â (public ρ), and the
// rejection sampling of s1 and s2 from ρ', whose count of rejected bytes,
// as in every implementation of the standard, depends on the seed.
namespace sgcl::crypto::detail::mldsa {
    // The parameter sets of §4 (Table 1)
    struct Params44 {
        static constexpr unsigned k = 4, l = 4, eta = 2, tau = 39, lambda = 128, gamma1_bits = 17, beta = 78, omega = 80;
        static constexpr int32_t gamma2 = (Q - 1) / 88;
        static constexpr const char* name = "sgcl::crypto::mldsa44";
    };

    struct Params65 {
        static constexpr unsigned k = 6, l = 5, eta = 4, tau = 49, lambda = 192, gamma1_bits = 19, beta = 196, omega = 55;
        static constexpr int32_t gamma2 = (Q - 1) / 32;
        static constexpr const char* name = "sgcl::crypto::mldsa65";
    };

    struct Params87 {
        static constexpr unsigned k = 8, l = 7, eta = 2, tau = 60, lambda = 256, gamma1_bits = 19, beta = 120, omega = 75;
        static constexpr int32_t gamma2 = (Q - 1) / 32;
        static constexpr const char* name = "sgcl::crypto::mldsa87";
    };

    // The sizes of Table 2, and the bit widths of the encodings
    template<class P>
    struct Sizes {
        static constexpr int32_t gamma1 = int32_t(1) << P::gamma1_bits;
        static constexpr unsigned z_bits = P::gamma1_bits + 1;                      // BitPack(z, γ1 − 1, γ1)
        static constexpr unsigned w1_bits = P::gamma2 == (Q - 1) / 88 ? 6 : 4;     // (q − 1)/(2γ2) − 1 = 43 or 15
        static constexpr unsigned eta_bits = P::eta == 2 ? 3 : 4;                  // BitPack(s, η, η)
        static constexpr size_t ctilde = P::lambda / 4;
        static constexpr size_t public_key = 32 + 320 * P::k;                      // 1312 / 1952 / 2592
        static constexpr size_t signature = ctilde + 32 * P::l * z_bits + P::omega + P::k;   // 2420 / 3309 / 4627
        static constexpr size_t private_key = 128 + 32 * ((P::k + P::l) * eta_bits + D * P::k);   // 2560 / 4032 / 4896
        static constexpr size_t w1_packed = 32 * P::k * w1_bits;
    };

    template<class P>
    using VecL = std::array<Poly, P::l>;

    template<class P>
    using VecK = std::array<Poly, P::k>;

    // What a public key keeps: its bytes, tr = H(pk), Â (from ρ, in the
    // transform's domain, [0, q)) and t1·2^d transformed ([0, q)): all made
    // once, so that a verification only hashes, samples c and multiplies
    template<class P>
    struct PublicState {
        uint8_t pk[Sizes<P>::public_key];
        uint8_t tr[64];
        Poly a[P::k * P::l];
        VecK<P> t1;
    };

    // What a private key keeps, in plain memory: its seed, ρ, K, tr, the
    // public key's bytes, Â, and s1, s2, t0 transformed
    template<class P>
    struct PrivateState {
        uint8_t seed[32];
        bool seeded = false;               // false only for a key of skDecode (the tests' vectors without a seed)
        uint8_t rho[32];
        uint8_t key[32];                   // K
        uint8_t tr[64];
        uint8_t pk[Sizes<P>::public_key];
        Poly a[P::k * P::l];
        VecL<P> s1;
        VecK<P> s2, t0;

        ~PrivateState() {
            secure_zero_object(*this);
        }
    };

    // H = SHAKE256 over pieces, and its sponge
    using Shake256 = KeccakSponge<136>;
    using Shake128 = KeccakSponge<168>;

    // Algorithm 30 (RejNTTPoly) for Â[r][s]: SHAKE128(ρ‖s‖r), three bytes a
    // candidate (the top bit dropped), kept below q
    inline void rej_ntt_poly(Poly& a, const uint8_t rho[32], uint8_t s, uint8_t r) noexcept {
        Shake128 h;
        h.init();
        h.absorb(rho, 32);
        const uint8_t sr[2] = {s, r};
        h.absorb(sr, 2);
        h.pad(0x1F);
        uint8_t block[168 * 5];             // five blocks: about 853 bytes are needed on average
        h.squeeze(block, sizeof block);
        size_t at = 0, have = sizeof block;
        size_t j = 0;
        while (j < N) {
            if (at + 3 > have) {
                h.squeeze(block, 168);
                at = 0;
                have = 168;
            }
            const int32_t z = int32_t(block[at]) | int32_t(block[at + 1]) << 8 | int32_t(block[at + 2] & 0x7F) << 16;
            at += 3;
            a[j] = z;
            j += z < Q;
        }
    }

    template<class P>
    inline void expand_a(Poly* a, const uint8_t rho[32]) noexcept {
        for (unsigned r = 0; r < P::k; ++r) {
            for (unsigned s = 0; s < P::l; ++s) {
                rej_ntt_poly(a[r * P::l + s], rho, uint8_t(s), uint8_t(r));
            }
        }
    }

    // Algorithm 31 (RejBoundedPoly): SHAKE256(ρ'‖r), two candidates a byte
    // (its halves), η − b for η = 4 and b < 9, 2 − b mod 5 for η = 2 and
    // b < 15 (the mod 5 a multiplication)
    template<class P>
    inline void rej_bounded_poly(Poly& a, const uint8_t rho[64], uint16_t r) noexcept {
        Shake256 h;
        h.init();
        h.absorb(rho, 64);
        const uint8_t rb[2] = {uint8_t(r), uint8_t(r >> 8)};
        h.absorb(rb, 2);
        h.pad(0x1F);
        uint8_t block[136];
        size_t j = 0;
        while (j < N) {
            h.squeeze(block, sizeof block);
            for (size_t i = 0; i < sizeof block && j < N; ++i) {
                for (unsigned half = 0; half < 2 && j < N; ++half) {
                    const uint32_t b = half ? block[i] >> 4 : block[i] & 15;
                    if constexpr (P::eta == 2) {
                        const uint32_t m5 = b - ((b * 205) >> 10) * 5;    // b mod 5 for b < 16
                        a[j] = 2 - int32_t(m5);
                        j += b < 15;
                    } else {
                        a[j] = 4 - int32_t(b);
                        j += b < 9;
                    }
                }
            }
        }
        secure_zero_object(block);
        secure_zero_object(h);
    }

    template<class P>
    inline void expand_s(VecL<P>& s1, VecK<P>& s2, const uint8_t rho[64]) noexcept {
        for (unsigned r = 0; r < P::l; ++r) {
            rej_bounded_poly<P>(s1[r], rho, uint16_t(r));
        }
        for (unsigned r = 0; r < P::k; ++r) {
            rej_bounded_poly<P>(s2[r], rho, uint16_t(r + P::l));
        }
    }

    // The bits of 256 values of `bits` bits each, little-endian (Algorithms
    // 16 and 17: SimpleBitPack, BitPack of b − w), and back
    template<unsigned Bits>
    SGCL_INLINE_HOT void pack(uint8_t* out, const uint32_t* v) noexcept {
        uint64_t acc = 0;
        unsigned n = 0;
        for (size_t i = 0; i < N; ++i) {
            acc |= uint64_t(v[i]) << n;
            n += Bits;
            while (n >= 8) {
                *out++ = uint8_t(acc);
                acc >>= 8;
                n -= 8;
            }
        }
    }

    template<unsigned Bits>
    SGCL_INLINE_HOT void unpack(uint32_t* v, const uint8_t* in) noexcept {
        uint64_t acc = 0;
        unsigned n = 0;
        for (size_t i = 0; i < N; ++i) {
            while (n < Bits) {
                acc |= uint64_t(*in++) << n;
                n += 8;
            }
            v[i] = uint32_t(acc & ((uint64_t(1) << Bits) - 1));
            acc >>= Bits;
            n -= Bits;
        }
    }

    // BitPack(w, a, b): b − w in Bits bits; BitUnpack: b − the value
    template<unsigned Bits>
    SGCL_INLINE_HOT void pack_offset(uint8_t* out, const Poly& w, int32_t b) noexcept {
        uint32_t v[N];
        for (size_t i = 0; i < N; ++i) {
            v[i] = uint32_t(b - w[i]);
        }
        pack<Bits>(out, v);
        secure_zero_object(v);
    }

    template<unsigned Bits>
    SGCL_INLINE_HOT void unpack_offset(Poly& w, const uint8_t* in, int32_t b) noexcept {
        uint32_t v[N];
        unpack<Bits>(v, in);
        for (size_t i = 0; i < N; ++i) {
            w[i] = b - int32_t(v[i]);
        }
        secure_zero_object(v);
    }

    // Algorithm 34 (ExpandMask): y[r] = BitUnpack(H(ρ''‖κ+r), γ1 − 1, γ1)
    template<class P>
    inline void expand_mask(VecL<P>& y, const uint8_t rho[64], uint16_t kappa) noexcept {
        constexpr size_t bytes = 32 * Sizes<P>::z_bits;
        uint8_t v[bytes];
        for (unsigned r = 0; r < P::l; ++r) {
            Shake256 h;
            h.init();
            h.absorb(rho, 64);
            const uint16_t kr = uint16_t(kappa + r);
            const uint8_t kb[2] = {uint8_t(kr), uint8_t(kr >> 8)};
            h.absorb(kb, 2);
            h.pad(0x1F);
            h.squeeze(v, bytes);
            unpack_offset<Sizes<P>::z_bits>(y[r], v, Sizes<P>::gamma1);
            secure_zero_object(h);
        }
        secure_zero_object(v);
    }

    // Algorithm 29 (SampleInBall): τ coefficients ±1 placed by
    // Fisher–Yates from H(c̃) (c is public: it is in the signature)
    template<class P>
    inline void sample_in_ball(Poly& c, const uint8_t* ctilde) noexcept {
        c.fill(0);
        Shake256 h;
        h.init();
        h.absorb(ctilde, Sizes<P>::ctilde);
        h.pad(0x1F);
        uint8_t s[8];
        h.squeeze(s, 8);
        uint64_t signs = 0;
        for (int i = 0; i < 8; ++i) {
            signs |= uint64_t(s[i]) << (8 * i);
        }
        uint8_t block[136];
        size_t at = 8;                      // the rest of the first block, after the signs
        h.squeeze(block + 8, sizeof block - 8);
        for (unsigned i = N - P::tau; i < N; ++i) {
            unsigned j;
            do {
                if (at == sizeof block) {
                    h.squeeze(block, sizeof block);
                    at = 0;
                }
                j = block[at++];
            } while (j > i);
            c[i] = c[j];
            c[j] = 1 - 2 * int32_t(signs & 1);
            signs >>= 1;
        }
    }

    // Algorithm 35 (Power2Round) of r in [0, q): r1 and r0 = r − r1·2^d in
    // (−2^(d−1), 2^(d−1)]
    SGCL_INLINE_HOT void power2round(int32_t r, int32_t& r1, int32_t& r0) noexcept {
        r1 = (r + (1 << (D - 1)) - 1) >> D;
        r0 = r - (r1 << D);
    }

    // Algorithm 36 (Decompose) of r in [0, q): r1 = ⌊(r + γ2 − 1)/(2γ2)⌋
    // (a division by a constant: a multiplication and a shift) and r0 = r −
    // r1·2γ2 in (−γ2, γ2], but r1 = 0 and r0 − 1 where r − r0 = q − 1
    template<class P>
    SGCL_INLINE_HOT void decompose(int32_t r, int32_t& r1, int32_t& r0) noexcept {
        constexpr uint32_t two_gamma2 = uint32_t(2 * P::gamma2);
        constexpr int32_t m = (Q - 1) / (2 * P::gamma2);
        int32_t a1 = int32_t((uint32_t(r) + uint32_t(P::gamma2) - 1) / two_gamma2);
        int32_t a0 = r - a1 * int32_t(two_gamma2);
        const int32_t top = -int32_t((uint32_t(a1 ^ m) - 1) >> 31);  // all ones where a1 == m
        a1 &= ~top;
        a0 += top;                                                   // − 1 there
        r1 = a1;
        r0 = a0;
    }

    template<class P>
    SGCL_INLINE_HOT int32_t high_bits(int32_t r) noexcept {
        int32_t r1, r0;
        decompose<P>(r, r1, r0);
        return r1;
    }

    // Algorithm 40 (UseHint), of public values
    template<class P>
    SGCL_INLINE_HOT int32_t use_hint(bool h, int32_t r) noexcept {
        constexpr int32_t m = (Q - 1) / (2 * P::gamma2);
        int32_t r1, r0;
        decompose<P>(r, r1, r0);
        if (!h) {
            return r1;
        }
        if (r0 > 0) {
            return r1 + 1 == m ? 0 : r1 + 1;
        }
        return r1 == 0 ? m - 1 : r1 - 1;
    }

    // w1Encode (Algorithm 28) of HighBits of w ([0, q))
    template<class P>
    inline void w1_encode(uint8_t* out, const VecK<P>& w1) noexcept {
        uint32_t v[N];
        for (unsigned i = 0; i < P::k; ++i) {
            for (size_t j = 0; j < N; ++j) {
                v[j] = uint32_t(w1[i][j]);
            }
            pack<Sizes<P>::w1_bits>(out + i * 32 * Sizes<P>::w1_bits, v);
        }
    }

    // The message representative μ = H(tr ‖ M', 64), M' = 0 ‖ |ctx| ‖ ctx ‖ M
    // (ML-DSA.Sign and Verify, Algorithms 2 and 3)
    inline void message_representative(uint8_t mu[64], const uint8_t tr[64], const uint8_t* context, size_t context_size, const uint8_t* m,
                                       size_t m_size) noexcept {
        Shake256 h;
        h.init();
        h.absorb(tr, 64);
        const uint8_t prefix[2] = {0, uint8_t(context_size)};
        h.absorb(prefix, 2);
        h.absorb(context, context_size);
        h.absorb(m, m_size);
        h.pad(0x1F);
        h.squeeze(mu, 64);
    }

    // The public state of pk's bytes (pkDecode, Algorithm 23): ρ, t1, and
    // what is made of them
    template<class P>
    inline void public_state(PublicState<P>& s, const uint8_t* pk) noexcept {
        std::memcpy(s.pk, pk, Sizes<P>::public_key);
        Shake256 h;
        h.init();
        h.absorb(pk, Sizes<P>::public_key);
        h.pad(0x1F);
        h.squeeze(s.tr, 64);
        expand_a<P>(s.a, pk);
        uint32_t v[N];
        for (unsigned i = 0; i < P::k; ++i) {
            unpack<10>(v, pk + 32 + 320 * i);
            for (size_t j = 0; j < N; ++j) {
                s.t1[i][j] = int32_t(v[j]) << D;
            }
            ntt(s.t1[i]);
            for (auto& c : s.t1[i]) {
                c = freeze(c);
            }
        }
    }

    // t = NTT^-1(Â ∘ NTT(s1)) + s2 in [0, q), of s1 transformed
    template<class P>
    inline void compute_t(VecK<P>& t, const Poly* a, const VecL<P>& s1_hat, const VecK<P>& s2) noexcept {
        for (unsigned i = 0; i < P::k; ++i) {
            dot<P::l>(t[i], a + i * P::l, s1_hat);
            inverse_ntt(t[i]);
            for (size_t j = 0; j < N; ++j) {
                t[i][j] = freeze(t[i][j] + s2[i][j]);
            }
        }
    }

    // pk = ρ ‖ SimpleBitPack(t1, 2^10 − 1), t0 kept, tr = H(pk)
    template<class P>
    inline void finish_key(PrivateState<P>& s, const VecK<P>& t) noexcept {
        std::memcpy(s.pk, s.rho, 32);
        uint32_t v[N];
        for (unsigned i = 0; i < P::k; ++i) {
            for (size_t j = 0; j < N; ++j) {
                int32_t r1, r0;
                power2round(t[i][j], r1, r0);
                v[j] = uint32_t(r1);
                s.t0[i][j] = r0;
            }
            pack<10>(s.pk + 32 + 320 * i, v);
            ntt(s.t0[i]);
        }
        Shake256 h;
        h.init();
        h.absorb(s.pk, Sizes<P>::public_key);
        h.pad(0x1F);
        h.squeeze(s.tr, 64);
    }

    // Algorithm 6 (ML-DSA.KeyGen_internal) of the seed ξ
    template<class P>
    inline void keygen(PrivateState<P>& s, const uint8_t xi[32]) noexcept {
        std::memcpy(s.seed, xi, 32);
        s.seeded = true;
        uint8_t expanded[128];
        Shake256 h;
        h.init();
        h.absorb(xi, 32);
        const uint8_t kl[2] = {uint8_t(P::k), uint8_t(P::l)};
        h.absorb(kl, 2);
        h.pad(0x1F);
        h.squeeze(expanded, 128);
        secure_zero_object(h);
        std::memcpy(s.rho, expanded, 32);
        std::memcpy(s.key, expanded + 96, 32);
        expand_a<P>(s.a, s.rho);
        expand_s<P>(s.s1, s.s2, expanded + 32);
        secure_zero_object(expanded);
        for (auto& p : s.s1) {
            ntt(p);
        }
        VecK<P> t;
        compute_t<P>(t, s.a, s.s1, s.s2);
        for (auto& p : s.s2) {
            ntt(p);
        }
        finish_key<P>(s, t);
        secure_zero_object(t);
    }

    // skDecode (Algorithm 25) for the tests' vectors of expanded keys: the
    // state of sk, its public key computed (t = A·s1 + s2); false for s1 or
    // s2 coefficients out of [−η, η]
    template<class P>
    inline bool decode_private(PrivateState<P>& s, const uint8_t* sk) noexcept {
        s.seeded = false;
        std::memset(s.seed, 0, 32);
        std::memcpy(s.rho, sk, 32);
        std::memcpy(s.key, sk + 32, 32);
        const uint8_t* p = sk + 128;
        constexpr unsigned eb = Sizes<P>::eta_bits;
        bool ok = true;
        for (auto& v : s.s1) {
            unpack_offset<eb>(v, p, int32_t(P::eta));
            p += 32 * eb;
        }
        for (auto& v : s.s2) {
            unpack_offset<eb>(v, p, int32_t(P::eta));
            p += 32 * eb;
        }
        for (auto& v : s.s1) {
            for (auto c : v) {
                ok &= c >= -int32_t(P::eta) && c <= int32_t(P::eta);
            }
        }
        for (auto& v : s.s2) {
            for (auto c : v) {
                ok &= c >= -int32_t(P::eta) && c <= int32_t(P::eta);
            }
        }
        expand_a<P>(s.a, s.rho);
        for (auto& v : s.s1) {
            ntt(v);
        }
        VecK<P> t;
        compute_t<P>(t, s.a, s.s1, s.s2);
        for (auto& v : s.s2) {
            ntt(v);
        }
        finish_key<P>(s, t);
        secure_zero_object(t);
        ok &= std::memcmp(s.tr, sk + 64, 64) == 0;
        return ok;
    }

    // Algorithm 7 (ML-DSA.Sign_internal) of μ and rnd: the signature into
    // sig (Sizes<P>::signature bytes)
    template<class P>
    inline void sign(uint8_t* sig, const PrivateState<P>& s, const uint8_t mu[64], const uint8_t rnd[32]) noexcept {
        using S = Sizes<P>;
        uint8_t rhopp[64];
        {
            Shake256 h;
            h.init();
            h.absorb(s.key, 32);
            h.absorb(rnd, 32);
            h.absorb(mu, 64);
            h.pad(0x1F);
            h.squeeze(rhopp, 64);
            secure_zero_object(h);
        }
        struct Round {
            VecL<P> y, y_hat, z;
            VecK<P> w, w1, cs2, ct0;
            Poly c;
            uint8_t w1_packed[S::w1_packed];
        };
        Round r;
        uint16_t kappa = 0;
        for (;;) {
            expand_mask<P>(r.y, rhopp, kappa);
            kappa = uint16_t(kappa + P::l);
            for (unsigned i = 0; i < P::l; ++i) {
                r.y_hat[i] = r.y[i];
                ntt(r.y_hat[i]);
            }
            for (unsigned i = 0; i < P::k; ++i) {
                dot<P::l>(r.w[i], s.a + i * P::l, r.y_hat);
                inverse_ntt(r.w[i]);
                for (size_t j = 0; j < N; ++j) {
                    r.w[i][j] = add_q(r.w[i][j]);
                    r.w1[i][j] = high_bits<P>(r.w[i][j]);
                }
            }
            w1_encode<P>(r.w1_packed, r.w1);
            uint8_t* ctilde = sig;
            {
                Shake256 h;
                h.init();
                h.absorb(mu, 64);
                h.absorb(r.w1_packed, S::w1_packed);
                h.pad(0x1F);
                h.squeeze(ctilde, S::ctilde);
            }
            sample_in_ball<P>(r.c, ctilde);
            ntt(r.c);
            // z = y + c·s1, ||z|| < γ1 − β
            uint32_t bad = 0;
            for (unsigned i = 0; i < P::l; ++i) {
                pointwise(r.z[i], r.c, s.s1[i]);
                inverse_ntt(r.z[i]);
                for (size_t j = 0; j < N; ++j) {
                    r.z[i][j] = freeze(r.y[i][j] + r.z[i][j]);
                    bad |= uint32_t(int32_t(S::gamma1 - P::beta) - 1 - int32_t(centered_abs(r.z[i][j]))) >> 31;
                }
            }
            // r0 = LowBits(w − c·s2), ||r0|| < γ2 − β
            for (unsigned i = 0; i < P::k; ++i) {
                pointwise(r.cs2[i], r.c, s.s2[i]);
                inverse_ntt(r.cs2[i]);
                for (size_t j = 0; j < N; ++j) {
                    r.cs2[i][j] = freeze(r.w[i][j] - r.cs2[i][j]);     // now w − c·s2
                    int32_t r1, r0;
                    decompose<P>(r.cs2[i][j], r1, r0);
                    const int32_t s0 = r0 >> 31;
                    bad |= uint32_t(int32_t(P::gamma2 - P::beta) - 1 - ((r0 ^ s0) - s0)) >> 31;
                }
            }
            // c·t0, ||c·t0|| < γ2, and the hints of −c·t0 to w − c·s2 + c·t0
            unsigned ones = 0;
            for (unsigned i = 0; i < P::k; ++i) {
                pointwise(r.ct0[i], r.c, s.t0[i]);
                inverse_ntt(r.ct0[i]);
                for (size_t j = 0; j < N; ++j) {
                    const int32_t ct0 = freeze(r.ct0[i][j]);
                    bad |= uint32_t(int32_t(P::gamma2) - 1 - int32_t(centered_abs(ct0))) >> 31;
                    const int32_t v = r.cs2[i][j];                               // w − c·s2
                    const int32_t u = freeze(v + ct0);                           // w − c·s2 + c·t0
                    const int32_t diff = high_bits<P>(u) ^ high_bits<P>(v);
                    const int32_t h = int32_t(uint32_t(diff | -diff) >> 31);
                    r.ct0[i][j] = h;                                             // the hint bit, in place
                    ones += unsigned(h);
                }
            }
            if (bad != 0 || ones > P::omega) {
                continue;                   // a rejected round: public, as FIPS 204 has it
            }
            // sigEncode (Algorithm 26)
            uint8_t* out = sig + S::ctilde;
            for (unsigned i = 0; i < P::l; ++i) {
                Poly zc;
                for (size_t j = 0; j < N; ++j) {
                    const int32_t a = r.z[i][j];
                    zc[j] = a - (((Q - 1) / 2 - a) >> 31 & Q);
                }
                pack_offset<S::z_bits>(out, zc, S::gamma1);
                out += 32 * S::z_bits;
            }
            std::memset(out, 0, P::omega + P::k);
            unsigned index = 0;
            for (unsigned i = 0; i < P::k; ++i) {
                for (unsigned j = 0; j < N; ++j) {
                    if (r.ct0[i][j]) {      // the hints are public: they are the signature's
                        out[index++] = uint8_t(j);
                    }
                }
                out[P::omega + i] = uint8_t(index);
            }
            break;
        }
        secure_zero_object(r);
        secure_zero_object(rhopp);
    }

    // Algorithm 8 (ML-DSA.Verify_internal) of μ (sigDecode, Algorithms 27
    // and 22, its checks included): every value here is public
    template<class P>
    inline bool verify(const PublicState<P>& s, const uint8_t mu[64], const uint8_t* sig) noexcept {
        using S = Sizes<P>;
        const uint8_t* ctilde = sig;
        const uint8_t* zp = sig + S::ctilde;
        const uint8_t* hp = zp + 32 * P::l * S::z_bits;
        // HintBitUnpack (Algorithm 21): the indices of each polynomial's
        // ones increasing, the counts not decreasing up to ω, the rest zero
        uint8_t hint[P::k][N] = {};
        unsigned index = 0;
        for (unsigned i = 0; i < P::k; ++i) {
            const unsigned end = hp[P::omega + i];
            if (end < index || end > P::omega) {
                return false;
            }
            const unsigned first = index;
            while (index < end) {
                if (index > first && hp[index - 1] >= hp[index]) {
                    return false;
                }
                hint[i][hp[index]] = 1;
                ++index;
            }
        }
        for (unsigned i = index; i < P::omega; ++i) {
            if (hp[i] != 0) {
                return false;
            }
        }
        VecL<P> z;
        for (unsigned i = 0; i < P::l; ++i) {
            unpack_offset<S::z_bits>(z[i], zp + 32 * S::z_bits * i, S::gamma1);
            for (auto c : z[i]) {
                const int32_t m = c >> 31;
                if (((c ^ m) - m) >= S::gamma1 - int32_t(P::beta)) {
                    return false;
                }
            }
            ntt(z[i]);
        }
        Poly c;
        sample_in_ball<P>(c, ctilde);
        ntt(c);
        VecK<P> w1;
        for (unsigned i = 0; i < P::k; ++i) {
            Poly w;
            for (size_t j = 0; j < N; ++j) {
                int64_t acc = 0;
                for (unsigned l = 0; l < P::l; ++l) {
                    acc += int64_t(s.a[i * P::l + l][j]) * z[l][j];
                }
                acc -= int64_t(c[j]) * s.t1[i][j];
                w[j] = montgomery(acc);
            }
            inverse_ntt(w);
            for (size_t j = 0; j < N; ++j) {
                w1[i][j] = use_hint<P>(hint[i][j] != 0, add_q(w[j]));
            }
        }
        uint8_t packed[S::w1_packed];
        w1_encode<P>(packed, w1);
        uint8_t again[S::ctilde];
        Shake256 h;
        h.init();
        h.absorb(mu, 64);
        h.absorb(packed, S::w1_packed);
        h.pad(0x1F);
        h.squeeze(again, S::ctilde);
        return std::memcmp(again, ctilde, S::ctilde) == 0;
    }
}
