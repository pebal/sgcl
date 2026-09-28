//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "../secure_zero.h"
#include "keccak.h"
#include "mlkem_poly.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// ML-KEM's algorithms (FIPS 203 §4–§6) over byte arrays, for the three
// parameter sets: the sampling (Algorithms 7 and 8), K-PKE (13–15) and the
// internal ML-KEM (16–18). What the public types in mlkem.h call; nothing
// here allocates: every vector and matrix is on the caller's stack, and
// everything that held a secret (the vectors s, e, y, e1, e2, the
// message, the sponges that read a seed) is zeroed before the function
// returns. The matrix Â is public (it is made from ρ, a part of the
// encapsulation key), and so are its rejection sampling's loops.
namespace sgcl::crypto::detail::mlkem {
    // The parameter sets of §8 (Table 2)
    struct Params512 {
        static constexpr unsigned k = 2, eta1 = 3, eta2 = 2, du = 10, dv = 4;
        static constexpr const char* name = "sgcl::crypto::mlkem512";
    };

    struct Params768 {
        static constexpr unsigned k = 3, eta1 = 2, eta2 = 2, du = 10, dv = 4;
        static constexpr const char* name = "sgcl::crypto::mlkem768";
    };

    struct Params1024 {
        static constexpr unsigned k = 4, eta1 = 2, eta2 = 2, du = 11, dv = 5;
        static constexpr const char* name = "sgcl::crypto::mlkem1024";
    };

    // The sizes of §8 (Table 3)
    template<class P>
    struct Sizes {
        static constexpr size_t ek = 384 * P::k + 32;                          // 800 / 1184 / 1568
        static constexpr size_t dk_pke = 384 * P::k;
        static constexpr size_t dk = 768 * P::k + 96;                          // 1632 / 2400 / 3168
        static constexpr size_t ciphertext = 32 * (P::du * P::k + P::dv);      // 768 / 1088 / 1568
    };

    // A scope's secrets zeroed when it ends, however it ends
    template<class T>
    struct Wipe {
        T& object;

        ~Wipe() {
            secure_zero_object(object);
        }
    };

    // The hash functions of §4.1: H = SHA3-256, J = SHAKE256 of 32 bytes,
    // G = SHA3-512 split in two halves; PRF_η = SHAKE256 of 64·η bytes; XOF
    // = SHAKE128. Each over the pieces given, one after another
    inline void sha3_256(uint8_t out[32], const uint8_t* a, size_t na, const uint8_t* b = nullptr, size_t nb = 0) noexcept {
        KeccakSponge<136> s;
        s.init();
        s.absorb(a, na);
        if (nb) {
            s.absorb(b, nb);
        }
        s.pad(0x06);
        s.squeeze(out, 32);
        secure_zero_object(s);
    }

    inline void sha3_512(uint8_t out[64], const uint8_t* a, size_t na, const uint8_t* b = nullptr, size_t nb = 0) noexcept {
        KeccakSponge<72> s;
        s.init();
        s.absorb(a, na);
        if (nb) {
            s.absorb(b, nb);
        }
        s.pad(0x06);
        s.squeeze(out, 64);
        secure_zero_object(s);
    }

    inline void shake256(uint8_t* out, size_t n, const uint8_t* a, size_t na, const uint8_t* b = nullptr, size_t nb = 0) noexcept {
        KeccakSponge<136> s;
        s.init();
        s.absorb(a, na);
        if (nb) {
            s.absorb(b, nb);
        }
        s.pad(0x1F);
        s.squeeze(out, n);
        secure_zero_object(s);
    }

    // Algorithm 7: a polynomial in the transform's domain, uniform, from
    // the 34 bytes ρ‖j‖i by rejection sampling on SHAKE128 (public data)
    inline void sample_ntt(Poly& a, const uint8_t rho[32], uint8_t j, uint8_t i) noexcept {
        KeccakSponge<168> s;
        s.init();
        s.absorb(rho, 32);
        uint8_t ji[2] = {j, i};
        s.absorb(ji, 2);
        s.pad(0x1F);
        uint8_t block[168];
        size_t at = sizeof block;
        size_t n = 0;
        while (n < N) {
            if (at == sizeof block) {
                s.squeeze(block, sizeof block);
                at = 0;
            }
            uint16_t d1 = uint16_t(block[at] | (uint16_t(block[at + 1] & 0x0F) << 8));
            uint16_t d2 = uint16_t((block[at + 1] >> 4) | (uint16_t(block[at + 2]) << 4));
            at += 3;
            if (d1 < Q) {
                a[n++] = d1;
            }
            if (d2 < Q && n < N) {
                a[n++] = d2;
            }
        }
    }

    // Algorithm 8: a polynomial with small coefficients, the centred
    // binomial distribution of η, from 64·η bytes: each coefficient the
    // count of η bits less the count of the next η. Bit operations only
    template<unsigned Eta>
    inline void sample_cbd(Poly& f, const uint8_t* b) noexcept {
        static_assert(Eta == 2 || Eta == 3);
        for (size_t i = 0; i < N; ++i) {
            unsigned x = 0, y = 0;
            for (unsigned j = 0; j < Eta; ++j) {
                size_t bx = 2 * i * Eta + j, by = 2 * i * Eta + Eta + j;
                x += (b[bx / 8] >> (bx % 8)) & 1;
                y += (b[by / 8] >> (by % 8)) & 1;
            }
            f[i] = sub(uint16_t(x), uint16_t(y));
        }
    }

    // SamplePolyCBD_η(PRF_η(σ, n))
    template<unsigned Eta>
    inline void sample_noise(Poly& f, const uint8_t sigma[32], uint8_t n) noexcept {
        uint8_t prf[64 * Eta];
        shake256(prf, sizeof prf, sigma, 32, &n, 1);
        sample_cbd<Eta>(f, prf);
        secure_zero(prf, sizeof prf);
    }

    template<class P>
    struct Vectors {
        Poly v[P::k];
    };

    template<class P>
    struct Matrix {
        Poly a[P::k][P::k];
    };

    // Â[i][j] = SampleNTT(ρ‖j‖i)
    template<class P>
    inline void expand_matrix(Matrix<P>& m, const uint8_t rho[32]) noexcept {
        for (unsigned i = 0; i < P::k; ++i) {
            for (unsigned j = 0; j < P::k; ++j) {
                sample_ntt(m.a[i][j], rho, uint8_t(j), uint8_t(i));
            }
        }
    }

    // Algorithm 13: K-PKE.KeyGen(d). ek = ByteEncode12(t̂)‖ρ, dk =
    // ByteEncode12(ŝ); the matrix Â made on the way left in `a`, for the
    // keys that keep it
    template<class P>
    inline void pke_keygen(uint8_t* ek, uint8_t* dk, const uint8_t d[32], Matrix<P>& a) noexcept {
        uint8_t in[33];
        std::memcpy(in, d, 32);
        in[32] = uint8_t(P::k);
        uint8_t g[64];
        Wipe<decltype(g)> wipe_g{g};
        sha3_512(g, in, sizeof in);
        secure_zero(in, sizeof in);
        const uint8_t* rho = g;
        const uint8_t* sigma = g + 32;
        expand_matrix<P>(a, rho);
        Vectors<P> s, e;
        Wipe<Vectors<P>> wipe_s{s}, wipe_e{e};
        uint8_t n = 0;
        for (auto& p : s.v) {
            sample_noise<P::eta1>(p, sigma, n++);
            ntt(p);
        }
        for (auto& p : e.v) {
            sample_noise<P::eta1>(p, sigma, n++);
            ntt(p);
        }
        for (unsigned i = 0; i < P::k; ++i) {   // t̂ = Â∘ŝ + ê, in e's place
            for (unsigned j = 0; j < P::k; ++j) {
                multiply_ntts_add(e.v[i], a.a[i][j], s.v[j]);
            }
            byte_encode(ek + 384 * i, e.v[i], 12);
            byte_encode(dk + 384 * i, s.v[i], 12);
        }
        std::memcpy(ek + 384 * P::k, rho, 32);
    }

    template<class P>
    inline void pke_keygen(uint8_t* ek, uint8_t* dk, const uint8_t d[32]) noexcept {
        Matrix<P> a;
        pke_keygen<P>(ek, dk, d, a);
    }

    // Algorithm 14: K-PKE.Encrypt(ek, m, r) into c, with the matrix Â of
    // ek's ρ given (the keys keep it: its sampling was 43 % of an
    // encapsulation of ML-KEM-1024, measured)
    template<class P>
    inline void pke_encrypt(uint8_t* c, const uint8_t* ek, const Matrix<P>& a, const uint8_t m[32], const uint8_t r[32]) noexcept {
        Vectors<P> t;
        for (unsigned i = 0; i < P::k; ++i) {
            byte_decode(t.v[i], ek + 384 * i, 12);
        }
        Vectors<P> y, u;
        Poly v, e2, mu;
        Wipe<Vectors<P>> wipe_y{y}, wipe_u{u};
        Wipe<Poly> wipe_v{v}, wipe_e2{e2}, wipe_mu{mu};
        uint8_t n = 0;
        for (auto& p : y.v) {
            sample_noise<P::eta1>(p, r, n++);
            ntt(p);
        }
        for (unsigned i = 0; i < P::k; ++i) {   // u = NTT⁻¹(Âᵀ∘ŷ) + e1
            Poly acc = {};
            for (unsigned j = 0; j < P::k; ++j) {
                multiply_ntts_add(acc, a.a[j][i], y.v[j]);
            }
            ntt_inverse(acc);
            sample_noise<P::eta2>(u.v[i], r, n++);
            poly_add(u.v[i], acc);
            secure_zero_object(acc);
        }
        sample_noise<P::eta2>(e2, r, n++);
        v = {};
        for (unsigned j = 0; j < P::k; ++j) {    // v = NTT⁻¹(t̂ᵀ∘ŷ) + e2 + μ
            multiply_ntts_add(v, t.v[j], y.v[j]);
        }
        ntt_inverse(v);
        poly_add(v, e2);
        byte_decode(mu, m, 1);
        poly_decompress(mu, 1);
        poly_add(v, mu);
        for (unsigned i = 0; i < P::k; ++i) {
            poly_compress(u.v[i], P::du);
            byte_encode(c + 32 * P::du * i, u.v[i], P::du);
        }
        poly_compress(v, P::dv);
        byte_encode(c + 32 * P::du * P::k, v, P::dv);
    }

    // The same, the matrix made here from ek's ρ
    template<class P>
    inline void pke_encrypt(uint8_t* c, const uint8_t* ek, const uint8_t m[32], const uint8_t r[32]) noexcept {
        Matrix<P> a;
        expand_matrix<P>(a, ek + 384 * P::k);
        pke_encrypt<P>(c, ek, a, m, r);
    }

    // Algorithm 15: K-PKE.Decrypt(dk, c): the message m
    template<class P>
    inline void pke_decrypt(uint8_t m[32], const uint8_t* dk, const uint8_t* c) noexcept {
        Vectors<P> s, u;
        Poly v, w;
        Wipe<Vectors<P>> wipe_s{s}, wipe_u{u};
        Wipe<Poly> wipe_v{v}, wipe_w{w};
        for (unsigned i = 0; i < P::k; ++i) {
            byte_decode(u.v[i], c + 32 * P::du * i, P::du);
            poly_decompress(u.v[i], P::du);
            ntt(u.v[i]);
            byte_decode(s.v[i], dk + 384 * i, 12);
        }
        byte_decode(v, c + 32 * P::du * P::k, P::dv);
        poly_decompress(v, P::dv);
        w = {};
        for (unsigned i = 0; i < P::k; ++i) {    // w = v' − NTT⁻¹(ŝᵀ∘NTT(u'))
            multiply_ntts_add(w, s.v[i], u.v[i]);
        }
        ntt_inverse(w);
        poly_sub(v, w);
        poly_compress(v, 1);
        byte_encode(m, v, 1);
    }

    // Algorithm 16: ML-KEM.KeyGen_internal(d, z): ek, and the full dk =
    // dk_PKE‖ek‖H(ek)‖z (the form of the standard; the public key type
    // keeps the seed and makes this from it)
    template<class P>
    inline void keygen_internal(uint8_t* ek, uint8_t* dk, const uint8_t d[32], const uint8_t z[32], Matrix<P>& a) noexcept {
        pke_keygen<P>(ek, dk, d, a);
        std::memcpy(dk + Sizes<P>::dk_pke, ek, Sizes<P>::ek);
        sha3_256(dk + Sizes<P>::dk_pke + Sizes<P>::ek, ek, Sizes<P>::ek);
        std::memcpy(dk + Sizes<P>::dk_pke + Sizes<P>::ek + 32, z, 32);
    }

    template<class P>
    inline void keygen_internal(uint8_t* ek, uint8_t* dk, const uint8_t d[32], const uint8_t z[32]) noexcept {
        Matrix<P> a;
        keygen_internal<P>(ek, dk, d, z, a);
    }

    // Algorithm 17: ML-KEM.Encaps_internal(ek, m): the shared key K and c,
    // with H(ek) given (the encapsulation key type keeps it)
    template<class P>
    inline void encaps_with_hash(uint8_t key[32], uint8_t* c, const uint8_t* ek, const Matrix<P>& a, const uint8_t h[32], const uint8_t m[32]) noexcept {
        uint8_t g[64];
        Wipe<decltype(g)> wipe_g{g};
        sha3_512(g, m, 32, h, 32);
        std::memcpy(key, g, 32);
        pke_encrypt<P>(c, ek, a, m, g + 32);
    }

    // The same with H(ek) and the matrix computed here
    template<class P>
    inline void encaps_internal(uint8_t key[32], uint8_t* c, const uint8_t* ek, const uint8_t m[32]) noexcept {
        uint8_t h[32];
        sha3_256(h, ek, Sizes<P>::ek);
        Matrix<P> a;
        expand_matrix<P>(a, ek + 384 * P::k);
        encaps_with_hash<P>(key, c, ek, a, h, m);
    }

    // Algorithm 18: ML-KEM.Decaps_internal(dk, c): K' when c encrypts what
    // it decrypts to, else the implicit rejection J(z‖c); both computed,
    // the choice a mask on a comparison of every byte
    template<class P>
    inline void decaps_internal(uint8_t key[32], const uint8_t* dk, const Matrix<P>& a, const uint8_t* c) noexcept {
        const uint8_t* dk_pke = dk;
        const uint8_t* ek = dk + Sizes<P>::dk_pke;
        const uint8_t* h = ek + Sizes<P>::ek;
        const uint8_t* z = h + 32;
        uint8_t m[32];
        Wipe<decltype(m)> wipe_m{m};
        pke_decrypt<P>(m, dk_pke, c);
        uint8_t g[64];
        Wipe<decltype(g)> wipe_g{g};
        sha3_512(g, m, 32, h, 32);
        uint8_t rejected[32];
        Wipe<decltype(rejected)> wipe_r{rejected};
        shake256(rejected, 32, z, 32, c, Sizes<P>::ciphertext);
        uint8_t again[Sizes<P>::ciphertext];   // c′, a function of m′: zeroed too
        Wipe<decltype(again)> wipe_again{again};
        pke_encrypt<P>(again, ek, a, m, g + 32);
        uint8_t choice[2];                      // the comparison and its mask, in memory that is zeroed
        Wipe<decltype(choice)> wipe_choice{choice};
        choice[0] = value_barrier(uint8_t(equal_bytes(c, again, Sizes<P>::ciphertext)));
        choice[1] = value_barrier(uint8_t(0 - choice[0]));   // 0xFF: c was genuine
        for (size_t i = 0; i < 32; ++i) {
            key[i] = uint8_t((g[i] & choice[1]) | (rejected[i] & uint8_t(~choice[1])));
        }
    }

    // The same, the matrix made here from the ρ of the ek inside dk
    template<class P>
    inline void decaps_internal(uint8_t key[32], const uint8_t* dk, const uint8_t* c) noexcept {
        Matrix<P> a;
        expand_matrix<P>(a, dk + Sizes<P>::dk_pke + 384 * P::k);
        decaps_internal<P>(key, dk, a, c);
    }

    // §7.3's check of an expanded decapsulation key given from outside
    // (the tests' semi-expanded keys): its length and H(ek) = h. The keys
    // the public type makes come from a seed and pass it by construction
    template<class P>
    inline bool decapsulation_key_valid(const uint8_t* dk, size_t n) noexcept {
        if (n != Sizes<P>::dk) {
            return false;
        }
        uint8_t h[32];
        sha3_256(h, dk + Sizes<P>::dk_pke, Sizes<P>::ek);
        return std::memcmp(h, dk + Sizes<P>::dk_pke + Sizes<P>::ek, 32) == 0;
    }

    // §7.2's check of an encapsulation key: its length, and every
    // coefficient of t̂ below q — the key's twelve-bit values decoded mod q
    // and encoded again give the same bytes (public data)
    template<class P>
    inline bool encapsulation_key_valid(const uint8_t* ek, size_t n) noexcept {
        if (n != Sizes<P>::ek) {
            return false;
        }
        for (unsigned i = 0; i < P::k; ++i) {
            Poly t;
            byte_decode(t, ek + 384 * i, 12);
            uint8_t again[384];
            byte_encode(again, t, 12);
            if (std::memcmp(again, ek + 384 * i, 384) != 0) {
                return false;
            }
        }
        return true;
    }
}
