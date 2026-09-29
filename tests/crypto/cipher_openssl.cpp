//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ciphers against OpenSSL in loops over random keys, nonces, additional
// data and lengths (0..1024 and a few large ones): what we seal OpenSSL
// opens, what OpenSSL seals we open, both give the same bytes; the stream
// ciphers the same keystream whatever the pieces it is asked in (1, 2, 3
// and 7 bytes, and random ones); Poly1305 the same tags on random keys and
// on messages near the edges of its arithmetic.
#include "cipher_test.h"

using namespace cipher_test;

namespace {
    template<class Aead>
    void aead_against_openssl(const EVP_CIPHER* cipher, size_t key_size, uint64_t seed) {
        std::mt19937_64 rng(seed);
        for (size_t n : lengths(rng, 200)) {
            SCOPED_TRACE(n);
            bytes key = random_bytes(rng, key_size);
            bytes nonce = random_bytes(rng, Aead::nonce_size);
            bytes pt = random_bytes(rng, n);
            bytes aad = random_bytes(rng, rng() % 3 == 0 ? 0 : rng() % 300);
            Aead a(key);
            // ours sealed, OpenSSL's sealed: the same bytes
            auto sealed = a.seal(nonce, pt, aad);
            bytes theirs = ossl_seal(cipher, key, nonce, pt, aad);
            ASSERT_EQ(sealed.size(), theirs.size());
            ASSERT_EQ(std::memcmp(sealed.data(), theirs.data(), theirs.size()), 0);
            // OpenSSL opens ours
            bytes ours(sealed.begin(), sealed.end()), back;
            ASSERT_TRUE(ossl_open(cipher, key, nonce, ours, aad, back));
            ASSERT_EQ(back, pt);
            // we open OpenSSL's, into a vector and in place
            auto opened = a.open(nonce, theirs, aad);
            ASSERT_TRUE(opened.has_value());
            ASSERT_EQ(opened->size(), pt.size());
            ASSERT_EQ(std::memcmp(opened->as_slice().data(), pt.data(), n), 0);
            bytes in_place = theirs;
            auto m = a.open_to(in_place, nonce, in_place, aad);
            ASSERT_TRUE(m.has_value());
            ASSERT_EQ(*m, n);
            ASSERT_EQ(std::memcmp(in_place.data(), pt.data(), n), 0);
            // sealed in place: the plaintext at the front of a buffer with
            // room for the tag
            bytes buffer = pt;
            buffer.resize(n + 16);
            EXPECT_EQ(a.seal_to(buffer, nonce, slice<const std::byte>(buffer.data(), n), aad), n + 16);
            ASSERT_EQ(buffer, theirs);
        }
    }

    // A stream cipher in pieces of `step` bytes (0: random pieces) against
    // one call of OpenSSL
    template<class Cipher>
    void stream_in_pieces(Cipher& c, const bytes& in, const bytes& want, size_t step, std::mt19937_64& rng) {
        bytes out(in.size());
        size_t at = 0;
        while (at < in.size()) {
            size_t k = step ? step : 1 + rng() % 200;
            k = std::min(k, in.size() - at);
            c.xor_key_stream(slice<std::byte>(out.data() + at, k), slice<const std::byte>(in.data() + at, k));
            at += k;
        }
        ASSERT_EQ(to_hex(out), to_hex(want));
    }
}

TEST(CryptoOpenssl_Tests, AesGcm128) {
    aead_against_openssl<crypto::aes_gcm>(EVP_aes_128_gcm(), 16, 1);
}

TEST(CryptoOpenssl_Tests, AesGcm192) {
    aead_against_openssl<crypto::aes_gcm>(EVP_aes_192_gcm(), 24, 2);
}

TEST(CryptoOpenssl_Tests, AesGcm256) {
    aead_against_openssl<crypto::aes_gcm>(EVP_aes_256_gcm(), 32, 3);
}

TEST(CryptoOpenssl_Tests, ChachaPoly) {
    aead_against_openssl<crypto::chacha20_poly1305>(EVP_chacha20_poly1305(), 32, 4);
}

// Additional data long enough for GHASH's groups of eight and Poly1305's
// pieces, with texts of every length around them
TEST(CryptoOpenssl_Tests, LongAdditionalData) {
    std::mt19937_64 rng(5);
    for (size_t aad_size : {size_t(127), size_t(128), size_t(129), size_t(4096 + 3), size_t(70000)}) {
        for (size_t n : {size_t(0), size_t(1), size_t(4096), size_t(4099)}) {
            bytes key = random_bytes(rng, 32), nonce = random_bytes(rng, 12), pt = random_bytes(rng, n), aad = random_bytes(rng, aad_size);
            crypto::aes_gcm g(key);
            auto s = g.seal(nonce, pt, aad);
            EXPECT_EQ(to_hex(s), to_hex(ossl_seal(EVP_aes_256_gcm(), key, nonce, pt, aad)));
            crypto::chacha20_poly1305 c(key);
            auto t = c.seal(nonce, pt, aad);
            EXPECT_EQ(to_hex(t), to_hex(ossl_seal(EVP_chacha20_poly1305(), key, nonce, pt, aad)));
        }
    }
}

// Nonces of all zeros and all ones: inc32 counts in the 32 bits after the
// nonce and must not carry into it
TEST(CryptoOpenssl_Tests, AesGcmExtremeNonces) {
    std::mt19937_64 rng(6);
    for (const char* nh : {"000000000000000000000000", "ffffffffffffffffffffffff", "fffffffffffffffffffffffe"}) {
        for (size_t n : {size_t(0), size_t(200), size_t(1000)}) {
            bytes key = random_bytes(rng, 16), nonce = hex(nh), pt = random_bytes(rng, n), aad = random_bytes(rng, 20);
            crypto::aes_gcm g(key);
            EXPECT_EQ(to_hex(g.seal(nonce, pt, aad)), to_hex(ossl_seal(EVP_aes_128_gcm(), key, nonce, pt, aad)));
        }
    }
}

TEST(CryptoOpenssl_Tests, AesBlockBothWays) {
    std::mt19937_64 rng(7);
    for (size_t key_size : {size_t(16), size_t(24), size_t(32)}) {
        for (int i = 0; i < 300; ++i) {
            bytes key = random_bytes(rng, key_size), block = random_bytes(rng, 16);
            crypto::aes a(key);
            sgcl::array<std::byte, 16> in;
            std::memcpy(in.data(), block.data(), 16);
            auto ct = a.encrypt_block(in);
            ASSERT_EQ(to_hex(ct), to_hex(ossl_crypt(ecb_cipher(key_size), key, {}, block, true)));
            auto pt = a.decrypt_block(in);
            ASSERT_EQ(to_hex(pt), to_hex(ossl_crypt(ecb_cipher(key_size), key, {}, block, false)));
        }
    }
}

// CTR from random counters and from counters about to carry out of the low
// word and out of the whole block (both wrap as OpenSSL wraps)
TEST(CryptoOpenssl_Tests, AesCtr) {
    std::mt19937_64 rng(8);
    std::vector<bytes> ivs;
    for (int i = 0; i < 6; ++i) {
        ivs.push_back(random_bytes(rng, 16));
    }
    ivs.push_back(hex("0000000000000000fffffffffffffff0"));
    ivs.push_back(hex("fffffffffffffffffffffffffffffff9"));
    ivs.push_back(hex("00000000000000000000000000000000"));
    for (size_t key_size : {size_t(16), size_t(24), size_t(32)}) {
        for (auto& iv : ivs) {
            for (size_t n : {size_t(0), size_t(1), size_t(15), size_t(16), size_t(17), size_t(127), size_t(128), size_t(129), size_t(300), size_t(1024), size_t(5000)}) {
                bytes key = random_bytes(rng, key_size), in = random_bytes(rng, n);
                bytes want = ossl_crypt(ctr_cipher(key_size), key, iv, in);
                for (size_t step : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(7), n ? n : size_t(1)}) {
                    crypto::aes_ctr c(key, iv);
                    stream_in_pieces(c, in, want, step, rng);
                }
            }
        }
    }
}

// seek(block) against OpenSSL started from the counter block + block
TEST(CryptoOpenssl_Tests, AesCtrSeek) {
    std::mt19937_64 rng(9);
    bytes key = random_bytes(rng, 16), iv = hex("0102030405060708fffffffffffffffe");
    crypto::aes_ctr c(key, iv);
    for (uint64_t block : {uint64_t(0), uint64_t(1), uint64_t(2), uint64_t(3), uint64_t(1000)}) {
        bytes in = random_bytes(rng, 100);
        bytes start = iv;
        // iv + block as a 128-bit big-endian number
        uint64_t lo = crypto::detail::load_be64(u8(iv) + 8), hi = crypto::detail::load_be64(u8(iv));
        uint64_t l = lo + block;
        hi += l < lo;
        crypto::detail::store_be64(u8(start), hi);
        crypto::detail::store_be64(u8(start) + 8, l);
        c.seek(block);
        bytes out(100);
        c.xor_key_stream(out, in);
        EXPECT_EQ(to_hex(out), to_hex(ossl_crypt(EVP_aes_128_ctr(), key, start, in)));
    }
}

// OpenSSL's ChaCha20 takes a 16-byte IV: the 32-bit counter little-endian,
// then the 12-byte nonce
TEST(CryptoOpenssl_Tests, Chacha20) {
    std::mt19937_64 rng(10);
    for (int round = 0; round < 12; ++round) {
        bytes key = random_bytes(rng, 32), nonce = random_bytes(rng, 12);
        uint32_t counter = round < 10 ? uint32_t(rng() % 1000) : 0xfffffff0u;
        bytes iv(16);
        crypto::detail::store_le32(u8(iv), counter);
        std::memcpy(iv.data() + 4, nonce.data(), 12);
        for (size_t n : {0, 1, 63, 64, 65, 128, 129, 255, 256, 257, 448, 449, 575, 576, 577, 700, 1024, 4031, 4032, 4033}) {
            if (n > ((uint64_t(1) << 32) - counter) * 64) {
                continue;   // past the keystream's end: CryptoContract_Tests has that case
            }
            bytes in = random_bytes(rng, n);
            bytes want = ossl_crypt(EVP_chacha20(), key, iv, in);
            for (size_t step : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(7), n ? n : size_t(1)}) {
                crypto::chacha20 c(key, nonce);
                c.seek(counter);
                stream_in_pieces(c, in, want, step, rng);
            }
        }
    }
}

TEST(CryptoOpenssl_Tests, Poly1305RandomAndEdges) {
    std::mt19937_64 rng(11);
    for (int i = 0; i < 2000; ++i) {
        bytes key = random_bytes(rng, 32);
        size_t n = i < 1000 ? size_t(i % 100) : size_t(rng() % 2000);
        bytes msg = random_bytes(rng, n);
        // keys and messages of all ones and zeros: the accumulator near p
        if (i % 7 == 0) {
            std::fill(key.begin(), key.end(), std::byte(0xff));
        }
        if (i % 11 == 0) {
            std::fill(msg.begin(), msg.end(), std::byte(0xff));
        }
        if (i % 13 == 0) {
            std::fill(key.begin() + 16, key.end(), std::byte(0));
        }
        bytes tag(16);
        crypto::detail::Poly1305 p;
        p.init(u8(key));
        // fed in pieces
        size_t at = 0;
        while (at < n) {
            size_t k = std::min<size_t>(1 + rng() % 40, n - at);
            p.update(u8(msg) + at, k);
            at += k;
        }
        p.finish(u8(tag));
        ASSERT_EQ(to_hex(tag), to_hex(ossl_poly1305(key, msg))) << i;
    }
}

// GCM's counter wraps in its last 32 bits (inc32) and never carries into the
// nonce: a counter near 2^32 through the eight-block path and the single
// blocks, against each block encrypted on its own from counters made here
TEST(CryptoOpenssl_Tests, GcmCounterWrapsInItsLastWord) {
    std::mt19937_64 rng(12);
    for (size_t key_size : {size_t(16), size_t(32)}) {
        bytes key = random_bytes(rng, key_size);
        crypto::detail::AesEncryptKey k;
        crypto::detail::aes_setup(k, u8(key), key_size);
        for (uint32_t start : {0xfffffff0u, 0xfffffffcu, 0xffffffffu}) {
            const size_t blocks = 21;
            bytes in = random_bytes(rng, 16 * blocks), out(16 * blocks);
            crypto::detail::Counter c = {0x0102030405060708ull, 0x090a0b0c00000000ull | start};
            crypto::detail::aes_ctr_blocks<true>(k, c, u8(in), u8(out), blocks);
            EXPECT_EQ(c.hi, 0x0102030405060708ull);
            EXPECT_EQ(c.lo, 0x090a0b0c00000000ull | uint32_t(start + blocks));
            for (size_t i = 0; i < blocks; ++i) {
                bytes block = hex("0102030405060708090a0b0c");
                block.resize(16);
                crypto::detail::store_be32(u8(block) + 12, uint32_t(start + i));
                bytes ks = ossl_crypt(ecb_cipher(key_size), key, {}, block, true);
                for (size_t j = 0; j < 16; ++j) {
                    ASSERT_EQ(out[16 * i + j], in[16 * i + j] ^ ks[j]) << start << " " << i;
                }
            }
        }
    }
}
