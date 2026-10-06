//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aes_core.h"

#include <cstddef>
#include <cstring>

// CBC's encryption (SP 800-38A §6.2): each block the encryption of the
// plaintext XORed with the ciphertext before it, the first with the IV. One
// block waits for the one before, so where decryption (aes_core.h) takes
// eight blocks through the rounds together, encryption runs one chain: the
// round keys are loaded into registers once for the call, and the
// plaintext is XORed into the first round key before the chain needs it —
// AESE XORs its key in first, so AESE(chain, plain ^ rk0) is the first
// round of the encryption of chain ^ plain with the XOR off the chain's
// critical path; the last round key's XOR is moved the same way, the
// chain carried without it. On x86-64 the same with AES-NI. The portable path
// encrypts a block at a time, bitsliced. iv is left as the last ciphertext
// for the next call.
namespace sgcl::crypto::detail {
#if defined(SGCL_CRYPTO_ARM64)
    template<unsigned Rounds>
    SGCL_TARGET_ARM64_CRYPTO
    inline void aes_cbc_encrypt_arm64_r(const AesArm64Key& k, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        uint8x16_t rk[15];
        for (unsigned r = 0; r <= Rounds; ++r) {
            rk[r] = k.rk[r];
        }
        // the chain carried without the last round key: c = ciphertext ^
        // rk[Rounds], so that the next block's first AESE takes it with the
        // plaintext XORed into rk[0] ^ rk[Rounds], and the last round's
        // XOR goes to the store, off the chain's path
        const uint8x16_t first = veorq_u8(rk[0], rk[Rounds]);
        uint8x16_t c = veorq_u8(vld1q_u8(iv), rk[Rounds]);
        for (; n > 0; --n) {
            uint8x16_t s = vaesmcq_u8(vaeseq_u8(c, veorq_u8(vld1q_u8(in), first)));
            for (unsigned r = 1; r + 1 < Rounds; ++r) {
                s = vaesmcq_u8(vaeseq_u8(s, rk[r]));
            }
            c = vaeseq_u8(s, rk[Rounds - 1]);
            vst1q_u8(out, veorq_u8(c, rk[Rounds]));
            in += 16;
            out += 16;
        }
        const uint8x16_t chain = veorq_u8(c, rk[Rounds]);
        vst1q_u8(iv, chain);
        for (auto& r : rk) {
            r = vdupq_n_u8(0);
        }
    }

    SGCL_TARGET_ARM64_CRYPTO
    inline void aes_cbc_encrypt_arm64(const AesArm64Key& k, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        switch (k.rounds) {
            case 10: aes_cbc_encrypt_arm64_r<10>(k, iv, in, out, n); break;
            case 12: aes_cbc_encrypt_arm64_r<12>(k, iv, in, out, n); break;
            default: aes_cbc_encrypt_arm64_r<14>(k, iv, in, out, n); break;
        }
    }
#endif

#if defined(SGCL_CRYPTO_X86)
    template<unsigned Rounds>
    SGCL_TARGET_X86_AES
    inline void aes_cbc_encrypt_x86_r(const AesX86Key& k, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        __m128i rk[15];
        for (unsigned r = 0; r <= Rounds; ++r) {
            rk[r] = k.rk[r];
        }
        __m128i chain = load128(iv);
        for (; n > 0; --n) {
            __m128i s = _mm_xor_si128(chain, _mm_xor_si128(load128(in), rk[0]));
            for (unsigned r = 1; r < Rounds; ++r) {
                s = _mm_aesenc_si128(s, rk[r]);
            }
            chain = _mm_aesenclast_si128(s, rk[Rounds]);
            store128(out, chain);
            in += 16;
            out += 16;
        }
        store128(iv, chain);
        for (auto& r : rk) {
            r = _mm_setzero_si128();
        }
    }

    SGCL_TARGET_X86_AES
    inline void aes_cbc_encrypt_x86(const AesX86Key& k, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        switch (k.rounds) {
            case 10: aes_cbc_encrypt_x86_r<10>(k, iv, in, out, n); break;
            case 12: aes_cbc_encrypt_x86_r<12>(k, iv, in, out, n); break;
            default: aes_cbc_encrypt_x86_r<14>(k, iv, in, out, n); break;
        }
    }
#endif

    inline void aes_cbc_encrypt_portable(const AesPortableKey& k, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        unsigned char block[16];
        for (; n > 0; --n) {
            for (int i = 0; i < 16; ++i) {
                block[i] = (unsigned char)(in[i] ^ iv[i]);
            }
            aes_encrypt_block_portable(k, block, iv);
            std::memcpy(out, iv, 16);
            in += 16;
            out += 16;
        }
        secure_zero(block, sizeof block);
    }

    // n whole blocks from in to out (which may be in), iv the chain
    SGCL_INLINE_HOT void aes_cbc_encrypt(const AesEncryptKey& k, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
#if defined(SGCL_CRYPTO_ARM64)
        if (k.path == AesPath::arm64) {
            aes_cbc_encrypt_arm64(k.arm64, iv, in, out, n);
            return;
        }
#endif
#if defined(SGCL_CRYPTO_X86)
        if (k.path == AesPath::x86) {
            aes_cbc_encrypt_x86(k.x86, iv, in, out, n);
            return;
        }
#endif
        aes_cbc_encrypt_portable(k.portable, iv, in, out, n);
    }
}
