//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "aes_portable.h"
#include "words.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#if SGCL_CRYPTO_ARM64_AES
#include <arm_neon.h>
#endif

// The AES of the module on either path, behind one set of functions: the
// key schedule, single blocks both ways, the counter mode that CTR and GCM
// share (the whole 128-bit block counting for CTR, its last 32 bits for
// GCM), and CBC's decryption (7z's 7zAES), whose blocks are independent as
// the counter mode's are. On arm64 with the crypto extension a round is AESE (AddRoundKey,
// ShiftRows, SubBytes) and AESMC (MixColumns), which the core fuses into one
// operation; eight blocks go through the rounds together, so that the eight
// independent chains fill the pipeline that one chain, each round waiting
// for the last, would leave mostly idle. Elsewhere the bitsliced cipher of
// aes_portable.h does four blocks at a time.
namespace sgcl::crypto::detail {
    // FIPS 197 §5.2, KeyExpansion: the key's words and then each word the
    // XOR of the word Nk before and the last one, the last one rotated,
    // substituted and XORed with a round constant every Nk words (and only
    // substituted at the middle of each group for a 256-bit key). A word is
    // four bytes in their order, held little-endian: RotWord is a rotation
    // right by 8 and the round constant goes in the low byte. The branches
    // depend on the position only, never on the key.
    template<class SubWord>
    inline unsigned aes_expand_key(const unsigned char* key, size_t key_size, unsigned char* round_keys, SubWord sub_word) noexcept {
        const unsigned nk = unsigned(key_size / 4);
        const unsigned rounds = nk + 6;
        uint32_t w[60];
        for (unsigned i = 0; i < nk; ++i) {
            w[i] = load_le32(key + 4 * i);
        }
        uint32_t rcon = 1;
        for (unsigned i = nk; i < 4 * (rounds + 1); ++i) {
            uint32_t t = w[i - 1];
            if (i % nk == 0) {
                t = sub_word(t >> 8 | t << 24) ^ rcon;
                rcon = uint32_t(gf_mul(uint8_t(rcon), 2));
            } else if (nk > 6 && i % nk == 4) {
                t = sub_word(t);
            }
            w[i] = w[i - nk] ^ t;
        }
        for (unsigned i = 0; i < 4 * (rounds + 1); ++i) {
            store_le32(round_keys + 4 * i, w[i]);
        }
        secure_zero(w, sizeof w);
        return rounds;
    }

    // A counter block as two big-endian halves: bytes 0..7 and 8..15
    struct Counter {
        uint64_t hi, lo;
    };

    // n blocks on: the whole block as one 128-bit number (CTR, as Go's
    // cipher.NewCTR and OpenSSL count), or only its last 32 bits, modulo
    // 2^32 (GCM's inc32, SP 800-38D §6.2)
    template<bool Inc32>
    inline Counter counter_add(Counter c, uint64_t n) noexcept {
        if constexpr (Inc32) {
            c.lo = (c.lo & 0xffffffff00000000ull) | uint32_t(uint32_t(c.lo) + uint32_t(n));
        } else {
            uint64_t l = c.lo + n;
            c.hi += uint64_t(l < c.lo);
            c.lo = l;
        }
        return c;
    }

    inline void store_counter(unsigned char* p, Counter c) noexcept {
        store_be64(p, c.hi);
        store_be64(p + 8, c.lo);
    }

#if SGCL_CRYPTO_ARM64_AES
    struct AesEncryptKey {
        std::array<uint8x16_t, 15> rk;
        unsigned rounds;
    };

    // The equivalent inverse cipher's keys (§5.3.5): the round keys in the
    // other order, the inner ones through InvMixColumns, for AESD/AESIMC
    struct AesDecryptKey {
        std::array<uint8x16_t, 15> rk;
    };

    // SubWord with AESE: the word in all four columns, a zero round key;
    // ShiftRows moves bytes between columns that are all the same, so the
    // first column is SubBytes of the word
    inline uint32_t sub_word_arm64(uint32_t w) noexcept {
        uint8x16_t v = vreinterpretq_u8_u32(vdupq_n_u32(w));
        v = vaeseq_u8(v, vdupq_n_u8(0));
        return vgetq_lane_u32(vreinterpretq_u32_u8(v), 0);
    }

    inline void aes_setup(AesEncryptKey& k, const unsigned char* key, size_t key_size) noexcept {
        unsigned char round_keys[240];
        k.rounds = aes_expand_key(key, key_size, round_keys, sub_word_arm64);
        for (unsigned r = 0; r <= k.rounds; ++r) {
            k.rk[r] = vld1q_u8(round_keys + 16 * r);
        }
        for (unsigned r = k.rounds + 1; r < 15; ++r) {
            k.rk[r] = vdupq_n_u8(0);
        }
        secure_zero(round_keys, sizeof round_keys);
    }

    inline void aes_setup_decrypt(AesDecryptKey& d, const AesEncryptKey& k) noexcept {
        const unsigned n = k.rounds;
        d.rk[0] = k.rk[n];
        for (unsigned i = 1; i < n; ++i) {
            d.rk[i] = vaesimcq_u8(k.rk[n - i]);
        }
        d.rk[n] = k.rk[0];
        for (unsigned r = n + 1; r < 15; ++r) {
            d.rk[r] = vdupq_n_u8(0);
        }
    }

    inline uint8x16_t aes_encrypt_v(const AesEncryptKey& k, uint8x16_t s) noexcept {
        const unsigned n = k.rounds;
        for (unsigned r = 0; r + 1 < n; ++r) {
            s = vaesmcq_u8(vaeseq_u8(s, k.rk[r]));
        }
        return veorq_u8(vaeseq_u8(s, k.rk[n - 1]), k.rk[n]);
    }

    inline uint8x16_t aes_decrypt_v(const AesDecryptKey& d, unsigned rounds, uint8x16_t s) noexcept {
        for (unsigned r = 0; r + 1 < rounds; ++r) {
            s = vaesimcq_u8(vaesdq_u8(s, d.rk[r]));
        }
        return veorq_u8(vaesdq_u8(s, d.rk[rounds - 1]), d.rk[rounds]);
    }

    // Eight blocks through the rounds together, the round count known at
    // compile time so that the loops unroll and the keys stay in registers
    template<unsigned Rounds>
    inline void aes_encrypt8(const uint8x16_t* rk, uint8x16_t* b) noexcept {
        for (unsigned r = 0; r + 1 < Rounds; ++r) {
            const uint8x16_t key = rk[r];
            for (int i = 0; i < 8; ++i) {
                b[i] = vaesmcq_u8(vaeseq_u8(b[i], key));
            }
        }
        for (int i = 0; i < 8; ++i) {
            b[i] = veorq_u8(vaeseq_u8(b[i], rk[Rounds - 1]), rk[Rounds]);
        }
    }

    // The same for the inverse cipher: AESD (AddRoundKey, InvShiftRows,
    // InvSubBytes) and AESIMC (InvMixColumns) with the equivalent inverse
    // cipher's keys
    template<unsigned Rounds>
    inline void aes_decrypt8(const uint8x16_t* rk, uint8x16_t* b) noexcept {
        for (unsigned r = 0; r + 1 < Rounds; ++r) {
            const uint8x16_t key = rk[r];
            for (int i = 0; i < 8; ++i) {
                b[i] = vaesimcq_u8(vaesdq_u8(b[i], key));
            }
        }
        for (int i = 0; i < 8; ++i) {
            b[i] = veorq_u8(vaesdq_u8(b[i], rk[Rounds - 1]), rk[Rounds]);
        }
    }

    // CBC's decryption of n whole blocks from in to out (which may be in):
    // each plaintext the decrypted block XORed with the ciphertext before
    // it (the first with iv); iv left as the last ciphertext, for the next
    // call. Encryption chains one block on the last and has no such form.
    template<unsigned Rounds>
    inline void aes_cbc_decrypt_r(const AesDecryptKey& d, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        uint8x16_t rk[15];
        for (unsigned r = 0; r <= Rounds; ++r) {
            rk[r] = d.rk[r];
        }
        uint8x16_t prev = vld1q_u8(iv);
        while (n >= 8) {
            uint8x16_t c[8], b[8];
            for (int i = 0; i < 8; ++i) {
                c[i] = b[i] = vld1q_u8(in + 16 * i);
            }
            aes_decrypt8<Rounds>(rk, b);
            vst1q_u8(out, veorq_u8(b[0], prev));
            for (int i = 1; i < 8; ++i) {
                vst1q_u8(out + 16 * i, veorq_u8(b[i], c[i - 1]));
            }
            prev = c[7];
            in += 128;
            out += 128;
            n -= 8;
        }
        for (; n > 0; --n) {
            uint8x16_t c = vld1q_u8(in);
            uint8x16_t s = c;
            for (unsigned r = 0; r + 1 < Rounds; ++r) {
                s = vaesimcq_u8(vaesdq_u8(s, rk[r]));
            }
            s = veorq_u8(vaesdq_u8(s, rk[Rounds - 1]), rk[Rounds]);
            vst1q_u8(out, veorq_u8(s, prev));
            prev = c;
            in += 16;
            out += 16;
        }
        vst1q_u8(iv, prev);
        for (auto& r : rk) {
            r = vdupq_n_u8(0);
        }
    }

    inline void aes_cbc_decrypt(const AesEncryptKey& k, const AesDecryptKey& d, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        switch (k.rounds) {
            case 10: aes_cbc_decrypt_r<10>(d, iv, in, out, n); break;
            case 12: aes_cbc_decrypt_r<12>(d, iv, in, out, n); break;
            default: aes_cbc_decrypt_r<14>(d, iv, in, out, n); break;
        }
    }

    inline uint8x16_t counter_vector(Counter c) noexcept {
        return vcombine_u8(vrev64_u8(vcreate_u8(c.hi)), vrev64_u8(vcreate_u8(c.lo)));
    }

    // Eight consecutive counter blocks from c, by vector additions: for
    // GCM the block's 32-bit words byte-reversed into native order, 1 added
    // to the last one (modulo 2^32, as inc32 wraps) and reversed back; for
    // CTR the two 64-bit halves the same way, the low one plus 1, when it
    // does not carry into the high one within the eight (a carry, once in
    // 2^61 groups, takes the scalar way). Two vector operations a block
    // where the scalar way costs six, most of them on the vector pipes the
    // rounds need.
    template<bool Inc32>
    inline void counter_blocks8(Counter c, uint8x16_t* b) noexcept {
        if constexpr (Inc32) {
            uint32x4_t v = vreinterpretq_u32_u8(vrev32q_u8(counter_vector(c)));
            const uint32x4_t one = vsetq_lane_u32(1, vdupq_n_u32(0), 3);
            for (int i = 0; i < 8; ++i) {
                b[i] = vrev32q_u8(vreinterpretq_u8_u32(v));
                v = vaddq_u32(v, one);
            }
        } else {
            if (c.lo <= ~uint64_t(0) - 7) {
                uint64x2_t v = vcombine_u64(vcreate_u64(c.hi), vcreate_u64(c.lo));
                const uint64x2_t one = vcombine_u64(vcreate_u64(0), vcreate_u64(1));
                for (int i = 0; i < 8; ++i) {
                    b[i] = vrev64q_u8(vreinterpretq_u8_u64(v));
                    v = vaddq_u64(v, one);
                }
            } else {
                for (int i = 0; i < 8; ++i) {
                    b[i] = counter_vector(counter_add<false>(c, uint64_t(i)));
                }
            }
        }
    }

    // The keystream of n whole blocks XORed into in, to out (which may be
    // in), the counter moved on by n
    template<bool Inc32, unsigned Rounds>
    inline void aes_ctr_blocks_r(const AesEncryptKey& k, Counter& c, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        uint8x16_t rk[15];
        for (unsigned r = 0; r <= Rounds; ++r) {
            rk[r] = k.rk[r];
        }
        while (n >= 8) {
            uint8x16_t b[8];
            counter_blocks8<Inc32>(c, b);
            aes_encrypt8<Rounds>(rk, b);
            for (int i = 0; i < 8; ++i) {
                vst1q_u8(out + 16 * i, veorq_u8(vld1q_u8(in + 16 * i), b[i]));
            }
            c = counter_add<Inc32>(c, 8);
            in += 128;
            out += 128;
            n -= 8;
        }
        for (; n > 0; --n) {
            uint8x16_t s = aes_encrypt_v(k, counter_vector(c));
            vst1q_u8(out, veorq_u8(vld1q_u8(in), s));
            c = counter_add<Inc32>(c, 1);
            in += 16;
            out += 16;
        }
    }

    template<bool Inc32>
    inline void aes_ctr_blocks(const AesEncryptKey& k, Counter& c, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        switch (k.rounds) {
            case 10: aes_ctr_blocks_r<Inc32, 10>(k, c, in, out, n); break;
            case 12: aes_ctr_blocks_r<Inc32, 12>(k, c, in, out, n); break;
            default: aes_ctr_blocks_r<Inc32, 14>(k, c, in, out, n); break;
        }
    }

    inline void aes_encrypt_block(const AesEncryptKey& k, const unsigned char* in, unsigned char* out) noexcept {
        vst1q_u8(out, aes_encrypt_v(k, vld1q_u8(in)));
    }

    inline void aes_decrypt_block(const AesEncryptKey& k, const AesDecryptKey& d, const unsigned char* in, unsigned char* out) noexcept {
        vst1q_u8(out, aes_decrypt_v(d, k.rounds, vld1q_u8(in)));
    }
#else
    using AesEncryptKey = AesPortableKey;

    // The bitsliced inverse cipher runs on the round keys as they are
    struct AesDecryptKey {};

    inline void aes_setup(AesEncryptKey& k, const unsigned char* key, size_t key_size) noexcept {
        unsigned char round_keys[240];
        unsigned rounds = aes_expand_key(key, key_size, round_keys, sub_word_portable);
        aes_portable_load_round_keys(k, round_keys, rounds);
        for (unsigned r = rounds + 1; r < 15; ++r) {
            k.rk[r] = {};
        }
        secure_zero(round_keys, sizeof round_keys);
    }

    inline void aes_setup_decrypt(AesDecryptKey&, const AesEncryptKey&) noexcept {
    }

    template<bool Inc32>
    inline void aes_ctr_blocks(const AesEncryptKey& k, Counter& c, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        unsigned char block[64];
        while (n > 0) {
            size_t m = n < 4 ? n : 4;
            for (size_t i = 0; i < 4; ++i) {
                store_counter(block + 16 * i, counter_add<Inc32>(c, uint64_t(i)));
            }
            aes_portable_encrypt4(k, block, block);
            for (size_t i = 0; i < 16 * m; ++i) {
                out[i] = (unsigned char)(in[i] ^ block[i]);
            }
            c = counter_add<Inc32>(c, m);
            in += 16 * m;
            out += 16 * m;
            n -= m;
        }
        secure_zero(block, sizeof block);
    }

    inline void aes_encrypt_block(const AesEncryptKey& k, const unsigned char* in, unsigned char* out) noexcept {
        unsigned char block[64] = {};
        std::memcpy(block, in, 16);
        aes_portable_encrypt4(k, block, block);
        std::memcpy(out, block, 16);
        secure_zero(block, sizeof block);
    }

    inline void aes_decrypt_block(const AesEncryptKey& k, const AesDecryptKey&, const unsigned char* in, unsigned char* out) noexcept {
        unsigned char block[64] = {};
        std::memcpy(block, in, 16);
        aes_portable_decrypt4(k, block, block);
        std::memcpy(out, block, 16);
        secure_zero(block, sizeof block);
    }

    // CBC's decryption, four blocks at a time through the bitsliced cipher
    inline void aes_cbc_decrypt(const AesEncryptKey& k, const AesDecryptKey&, unsigned char* iv, const unsigned char* in, unsigned char* out, size_t n) noexcept {
        unsigned char c[64], b[64] = {};
        while (n > 0) {
            size_t m = n < 4 ? n : 4;
            std::memcpy(c, in, 16 * m);
            std::memcpy(b, in, 16 * m);
            aes_portable_decrypt4(k, b, b);
            for (size_t i = 0; i < 16 * m; ++i) {
                out[i] = (unsigned char)(b[i] ^ (i < 16 ? iv[i] : c[i - 16]));
            }
            std::memcpy(iv, c + 16 * (m - 1), 16);
            in += 16 * m;
            out += 16 * m;
            n -= m;
        }
        secure_zero(b, sizeof b);
    }
#endif
}
