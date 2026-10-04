//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "aes_core.h"
#include "ghash.h"
#include "words.h"
#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// GCM (SP 800-38D §7) with a 96-bit IV: J0 = IV || 0^31 || 1; the text
// encrypted in counter mode from inc32(J0); the tag E(K, J0) XOR
// GHASH(A || pad || C || pad || len(A) || len(C)).
//
// Seal on arm64 and on x86-64's AES-NI (the key's path) is one pass: eight counter blocks through AES, XORed with
// the plaintext, stored, and the eight ciphertext blocks fed to GHASH from
// the registers, the AES of one group overlapping the GHASH of the last in
// the core's window. Open is two passes, deliberately: GHASH over the whole
// ciphertext first, the tag compared, and only then the counter mode that
// writes the plaintext. A one-pass open writes plaintext before it knows
// the tag, and a caller that reads the output after an error (or an output
// that is the input, opened in place) would see the text of a forgery.
namespace sgcl::crypto::detail {
    struct GcmKey {
        AesEncryptKey aes;
        GhashKey ghash;
    };

    SGCL_INLINE_HOT void gcm_setup(GcmKey& k, const unsigned char* key, size_t key_size) noexcept {
        aes_setup(k.aes, key, key_size);
        unsigned char h[16] = {};
        aes_encrypt_block(k.aes, h, h);   // H = E(K, 0^128)
        ghash_init(k.ghash, h, k.aes.path);
        secure_zero(h, sizeof h);
    }

    SGCL_INLINE_HOT Counter gcm_j0(const unsigned char* nonce) noexcept {
        return {load_be64(nonce), uint64_t(load_be32(nonce + 8)) << 32 | 1};
    }

#if defined(SGCL_CRYPTO_ARM64)
    template<unsigned Rounds>
    SGCL_TARGET_ARM64_CRYPTO
    inline void gcm_seal_groups_r(const GcmKey& k, Counter& c, GhashState& s, const unsigned char* in, unsigned char* out, size_t groups) noexcept {
        uint8x16_t rk[15];
        for (unsigned r = 0; r <= Rounds; ++r) {
            rk[r] = k.aes.arm64.rk[r];
        }
        uint64x2_t y = field_vector(s.y);
        for (; groups > 0; --groups) {
            uint8x16_t b[8];
            counter_blocks8<true>(c, b);
            aes_encrypt8<Rounds>(rk, b);
            for (int i = 0; i < 8; ++i) {
                b[i] = veorq_u8(vld1q_u8(in + 16 * i), b[i]);
                vst1q_u8(out + 16 * i, b[i]);
            }
            ghash_eight(k.ghash.arm64, y, b);
            c = counter_add<true>(c, 8);
            in += 128;
            out += 128;
        }
        s.y = field_of(y);
    }

    // Groups of eight blocks encrypted and hashed in one pass
    SGCL_TARGET_ARM64_CRYPTO
    inline void gcm_seal_groups(const GcmKey& k, Counter& c, GhashState& s, const unsigned char* in, unsigned char* out, size_t groups) noexcept {
        switch (k.aes.rounds) {
            case 10: gcm_seal_groups_r<10>(k, c, s, in, out, groups); break;
            case 12: gcm_seal_groups_r<12>(k, c, s, in, out, groups); break;
            default: gcm_seal_groups_r<14>(k, c, s, in, out, groups); break;
        }
    }
#endif

#if defined(SGCL_CRYPTO_X86)
    template<unsigned Rounds>
    SGCL_TARGET_X86_AES
    inline void gcm_seal_groups_x86_r(const GcmKey& k, Counter& c, GhashState& s, const unsigned char* in, unsigned char* out, size_t groups) noexcept {
        __m128i rk[15];
        for (unsigned r = 0; r <= Rounds; ++r) {
            rk[r] = k.aes.x86.rk[r];
        }
        __m128i y = field_vector_x86(s.y);
        for (; groups > 0; --groups) {
            __m128i b[8];
            counter_blocks8_x86<true>(c, b);
            aes_encrypt8_x86<Rounds>(rk, b);
            for (int i = 0; i < 8; ++i) {
                b[i] = _mm_xor_si128(load128(in + 16 * i), b[i]);
                store128(out + 16 * i, b[i]);
            }
            ghash_eight_x86(k.ghash.x86, y, b);
            c = counter_add<true>(c, 8);
            in += 128;
            out += 128;
        }
        s.y = field_of_x86(y);
        for (auto& r : rk) {
            r = _mm_setzero_si128();
        }
    }

    SGCL_TARGET_X86_AES
    inline void gcm_seal_groups_x86(const GcmKey& k, Counter& c, GhashState& s, const unsigned char* in, unsigned char* out, size_t groups) noexcept {
        switch (k.aes.rounds) {
            case 10: gcm_seal_groups_x86_r<10>(k, c, s, in, out, groups); break;
            case 12: gcm_seal_groups_x86_r<12>(k, c, s, in, out, groups); break;
            default: gcm_seal_groups_x86_r<14>(k, c, s, in, out, groups); break;
        }
    }
#endif

    // The last bytes that do not fill a block: one block of keystream,
    // as much of it as there is text
    SGCL_INLINE_HOT void gcm_ctr_tail(const GcmKey& k, Counter c, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        unsigned char block[16] = {};
        sgcl::detail::copy_bytes(block, in, n);
        aes_ctr_blocks<true>(k.aes, c, block, block, 1);
        sgcl::detail::copy_bytes(out, block, n);
        secure_zero(block, sizeof block);
    }

    // n bytes of in sealed into out (n + 16 bytes: the text, then the
    // tag); out may be in
    inline void gcm_seal(const GcmKey& k, const unsigned char* nonce, const unsigned char* in, size_t n, const unsigned char* aad, size_t aad_size, unsigned char* out) noexcept {
        const Counter j0 = gcm_j0(nonce);
        unsigned char mask[16];
        store_counter(mask, j0);
        aes_encrypt_block(k.aes, mask, mask);
        Counter c = counter_add<true>(j0, 1);
        GhashState s;
        ghash_start(s);
        ghash_padded(k.ghash, s, aad, aad_size);
        size_t blocks = n / 16;
        size_t done = 0;
#if defined(SGCL_CRYPTO_ARM64)
        if (k.aes.path == AesPath::arm64) {
            size_t groups = blocks / 8;
            gcm_seal_groups(k, c, s, in, out, groups);
            done = groups * 128;
            blocks -= groups * 8;
        }
#endif
#if defined(SGCL_CRYPTO_X86)
        if (k.aes.path == AesPath::x86) {
            size_t groups = blocks / 8;
            gcm_seal_groups_x86(k, c, s, in, out, groups);
            done = groups * 128;
            blocks -= groups * 8;
        }
#endif
        aes_ctr_blocks<true>(k.aes, c, in + done, out + done, blocks);
        ghash_blocks(k.ghash, s, out + done, blocks);
        done += blocks * 16;
        if (done < n) {
            gcm_ctr_tail(k, c, in + done, out + done, n - done);
            ghash_padded(k.ghash, s, out + done, n - done);
        }
        ghash_lengths(k.ghash, s, aad_size, n);
        unsigned char tag[16];
        ghash_finish(s, tag);
        for (int i = 0; i < 16; ++i) {
            out[n + i] = (unsigned char)(tag[i] ^ mask[i]);
        }
        secure_zero(mask, sizeof mask);
        secure_zero_object(s);
    }

    // n bytes of ciphertext and their tag checked, then opened into out
    // (which may be in); false, with out's n bytes zeroed, when the tag
    // does not match
    inline bool gcm_open(const GcmKey& k, const unsigned char* nonce, const unsigned char* in, size_t n, const unsigned char* tag, const unsigned char* aad, size_t aad_size, unsigned char* out) noexcept {
        const Counter j0 = gcm_j0(nonce);
        unsigned char want[16];
        store_counter(want, j0);
        aes_encrypt_block(k.aes, want, want);
        GhashState s;
        ghash_start(s);
        ghash_padded(k.ghash, s, aad, aad_size);
        ghash_padded(k.ghash, s, in, n);
        ghash_lengths(k.ghash, s, aad_size, n);
        unsigned char sum[16];
        ghash_finish(s, sum);
        for (int i = 0; i < 16; ++i) {
            want[i] ^= sum[i];
        }
        const bool ok = equal_bytes(want, tag, 16);
        secure_zero(want, sizeof want);
        secure_zero_object(s);
        if (!ok) {
            secure_zero(out, n);
            return false;
        }
        Counter c = counter_add<true>(j0, 1);
        const size_t blocks = n / 16;
        aes_ctr_blocks<true>(k.aes, c, in, out, blocks);
        if (blocks * 16 < n) {
            gcm_ctr_tail(k, c, in + blocks * 16, out + blocks * 16, n - blocks * 16);
        }
        return true;
    }
}
