//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ciphers' paths against each other in one program: the one this
// processor takes (AESE/PMULL on arm64, AES-NI/PCLMULQDQ and SSE2 or AVX2 on
// x86-64) beside the portable one, on random keys, counters and texts of
// every length around the paths' block groups. tests_crypto_portable runs
// the vectors on the portable path alone; this is where the two meet, and
// where an x86 kernel is held to the plain C++ on a machine without the
// oracle.
#include "cipher_test.h"

#include "sgcl/crypto/detail/chacha_core.h"
#include "sgcl/crypto/detail/gcm_core.h"

#include <random>

using namespace sgcl;
using namespace cipher_test;
namespace d = crypto::detail;

namespace {
    std::vector<size_t> sizes() {
        std::vector<size_t> n;
        for (size_t i = 0; i <= 300; ++i) {
            n.push_back(i);
        }
        for (size_t base : {size_t(511), size_t(512), size_t(575), size_t(576), size_t(577), size_t(1023), size_t(1024), size_t(4096)}) {
            for (size_t e = 0; e < 3; ++e) {
                n.push_back(base + e * 16);
            }
        }
        return n;
    }

    // A GCM key on the portable path whatever the processor has
    d::GcmKey portable_gcm(const bytes& key) {
        d::GcmKey k;
        k.aes.path = d::AesPath::portable;
        k.aes.portable = d::AesPortableKey{};
        d::aes_setup_portable(k.aes.portable, u8(key), key.size());
        k.aes.rounds = k.aes.portable.rounds;
        unsigned char h[16] = {};
        d::aes_encrypt_block(k.aes, h, h);
        d::ghash_init(k.ghash, h, d::AesPath::portable);
        return k;
    }

    d::Counter random_counter(std::mt19937_64& rng, bool near_carry) {
        d::Counter c{rng(), rng()};
        if (near_carry) {
            c.lo = ~uint64_t(0) - (rng() % 12);   // the 64-bit half about to carry
        }
        return c;
    }
}

TEST(CryptoPaths_Tests, WhichPath) {
    std::printf("[ path     ] %s\n", path_name());
    SUCCEED();
}

TEST(CryptoPaths_Tests, AesCounterModesAndBlocks) {
    std::mt19937_64 rng(31);
    for (size_t key_size : {size_t(16), size_t(24), size_t(32)}) {
        const bytes key = random_bytes(rng, key_size);
        d::AesEncryptKey fast;
        d::aes_setup(fast, u8(key), key_size);
        d::AesDecryptKey fast_d;
        d::aes_setup_decrypt(fast_d, fast);
        d::AesPortableKey plain{};
        d::aes_setup_portable(plain, u8(key), key_size);
        for (size_t blocks = 0; blocks <= 40; ++blocks) {
            SCOPED_TRACE(blocks);
            const bytes in = random_bytes(rng, blocks * 16);
            for (bool near_carry : {false, true}) {
                const d::Counter start = random_counter(rng, near_carry);
                bytes a(in.size()), b(in.size());
                d::Counter ca = start, cb = start;
                d::aes_ctr_blocks<false>(fast, ca, u8(in), u8(a), blocks);
                d::aes_ctr_blocks_portable<false>(plain, cb, u8(in), u8(b), blocks);
                ASSERT_EQ(to_hex(a), to_hex(b)) << "CTR " << key_size;
                ASSERT_TRUE(ca.hi == cb.hi && ca.lo == cb.lo);
                ca = cb = start;
                d::aes_ctr_blocks<true>(fast, ca, u8(in), u8(a), blocks);
                d::aes_ctr_blocks_portable<true>(plain, cb, u8(in), u8(b), blocks);
                ASSERT_EQ(to_hex(a), to_hex(b)) << "GCM's counter " << key_size;
            }
            // CBC's decryption, and single blocks both ways
            unsigned char iv_a[16], iv_b[16];
            const bytes iv = random_bytes(rng, 16);
            std::memcpy(iv_a, iv.data(), 16);
            std::memcpy(iv_b, iv.data(), 16);
            bytes a(in.size()), b(in.size());
            d::aes_cbc_decrypt(fast, fast_d, iv_a, u8(in), u8(a), blocks);
            d::aes_cbc_decrypt_portable(plain, iv_b, u8(in), u8(b), blocks);
            ASSERT_EQ(to_hex(a), to_hex(b)) << "CBC " << key_size;
            ASSERT_EQ(std::memcmp(iv_a, iv_b, 16), 0);
            if (blocks >= 1) {
                unsigned char e1[16], e2[16], x1[16], x2[16];
                d::aes_encrypt_block(fast, u8(in), e1);
                d::aes_encrypt_block_portable(plain, u8(in), e2);
                ASSERT_EQ(std::memcmp(e1, e2, 16), 0);
                d::aes_decrypt_block(fast, fast_d, u8(in), x1);
                d::aes_decrypt_block_portable(plain, u8(in), x2);
                ASSERT_EQ(std::memcmp(x1, x2, 16), 0);
            }
        }
    }
}

TEST(CryptoPaths_Tests, GcmSealAndOpen) {
    std::mt19937_64 rng(32);
    for (size_t key_size : {size_t(16), size_t(24), size_t(32)}) {
        const bytes key = random_bytes(rng, key_size);
        d::GcmKey fast;
        d::gcm_setup(fast, u8(key), key_size);
        const d::GcmKey plain = portable_gcm(key);
        for (size_t n : sizes()) {
            SCOPED_TRACE(n);
            const bytes nonce = random_bytes(rng, 12), in = random_bytes(rng, n), aad = random_bytes(rng, rng() % 70);
            bytes a(n + 16), b(n + 16);
            d::gcm_seal(fast, u8(nonce), u8(in), n, u8(aad), aad.size(), u8(a));
            d::gcm_seal(plain, u8(nonce), u8(in), n, u8(aad), aad.size(), u8(b));
            ASSERT_EQ(to_hex(a), to_hex(b)) << key_size;
            bytes back(n);
            ASSERT_TRUE(d::gcm_open(fast, u8(nonce), u8(a), n, u8(a) + n, u8(aad), aad.size(), u8(back)));
            ASSERT_EQ(to_hex(back), to_hex(in));
            if (n > 0) {
                a[rng() % n] ^= std::byte(1);
                ASSERT_FALSE(d::gcm_open(fast, u8(nonce), u8(a), n, u8(a) + n, u8(aad), aad.size(), u8(back)));
            }
        }
    }
}

TEST(CryptoPaths_Tests, GhashOnRandomBlocks) {
    std::mt19937_64 rng(33);
    for (int round = 0; round < 40; ++round) {
        const bytes h = random_bytes(rng, 16), data = random_bytes(rng, 16 * (rng() % 40));
        d::GhashKey plain, fast;
        d::ghash_init(plain, u8(h), d::AesPath::portable);
        d::AesEncryptKey probe;
        const bytes key = random_bytes(rng, 16);
        d::aes_setup(probe, u8(key), 16);   // the path this processor takes
        d::ghash_init(fast, u8(h), probe.path);
        d::GhashState sa, sb;
        d::ghash_start(sa);
        d::ghash_start(sb);
        d::ghash_blocks(plain, sa, u8(data), data.size() / 16);
        d::ghash_blocks(fast, sb, u8(data), data.size() / 16);
        unsigned char ta[16], tb[16];
        d::ghash_finish(sa, ta);
        d::ghash_finish(sb, tb);
        ASSERT_EQ(std::memcmp(ta, tb, 16), 0) << round;
    }
}

TEST(CryptoPaths_Tests, ChachaKeystream) {
    std::mt19937_64 rng(34);
    for (size_t n : sizes()) {
        SCOPED_TRACE(n);
        const bytes key = random_bytes(rng, 32), nonce = random_bytes(rng, 12), in = random_bytes(rng, n);
        d::ChachaState s;
        d::chacha_load(s, u8(key), u8(nonce));
        const uint32_t counter = uint32_t(rng() % 1000);
        bytes fast(n), plain(n);
        d::chacha_xor(s, counter, u8(in), u8(fast), n);
        unsigned char block[64];
        for (size_t at = 0; at < n; at += 64) {
            d::chacha_block(s, counter + uint32_t(at / 64), block);
            for (size_t i = at; i < n && i < at + 64; ++i) {
                plain[i] = std::byte(uint8_t(in[i]) ^ block[i - at]);
            }
        }
        ASSERT_EQ(to_hex(fast), to_hex(plain));
        // the AEAD's first blocks: block 0 and the text's first
        unsigned char ks[576], one[64];
        const size_t made = d::chacha_first_blocks(s, n, ks);
        for (size_t b = 0; b < made / 64; ++b) {
            d::chacha_block(s, uint32_t(b), one);
            ASSERT_EQ(std::memcmp(ks + 64 * b, one, 64), 0) << b;
        }
    }
}
