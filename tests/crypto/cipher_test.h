//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of the ciphers share (C2: aes, aes_gcm, aes_ctr, chacha20,
// chacha20_poly1305, xchacha20_poly1305): hex, random bytes, and OpenSSL's
// libcrypto as the oracle — EVP_aes_*_gcm, EVP_chacha20_poly1305,
// EVP_aes_*_ctr, EVP_chacha20, EVP_aes_*_ecb, EVP_MAC "POLY1305" — linked
// into the tests only.
#pragma once

#include "tests/types.h"
#include "sgcl/crypto/crypto.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace cipher_test {
    using bytes = std::vector<std::byte>;

    inline bytes hex(const std::string& s) {
        bytes v;
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            v.push_back(std::byte(std::stoul(s.substr(i, 2), nullptr, 16)));
        }
        return v;
    }

    inline std::string to_hex(const std::byte* p, size_t n) {
        static const char digits[] = "0123456789abcdef";
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            s += digits[unsigned(p[i]) >> 4];
            s += digits[unsigned(p[i]) & 15];
        }
        return s;
    }

    inline std::string to_hex(const bytes& v) {
        return to_hex(v.data(), v.size());
    }

    inline std::string to_hex(const sgcl::vector<std::byte>& v) {
        return to_hex(v.data(), v.size());
    }

    template<size_t N>
    std::string to_hex(const sgcl::array<std::byte, N>& a) {
        return to_hex(a.data(), N);
    }

    inline bytes text(const std::string& s) {
        bytes v(s.size());
        std::memcpy(v.data(), s.data(), s.size());
        return v;
    }

    inline bytes random_bytes(std::mt19937_64& rng, size_t n) {
        bytes v(n);
        for (auto& b : v) {
            b = std::byte(rng());
        }
        return v;
    }

    inline bytes concat(const bytes& a, const bytes& b) {
        bytes v = a;
        v.insert(v.end(), b.begin(), b.end());
        return v;
    }

    inline bool all_zero(const std::byte* p, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (p[i] != std::byte(0)) {
                return false;
            }
        }
        return true;
    }

    inline const unsigned char* u8(const bytes& v) {
        return reinterpret_cast<const unsigned char*>(v.data());
    }

    inline unsigned char* u8(bytes& v) {
        return reinterpret_cast<unsigned char*>(v.data());
    }

    // Lengths 0..1024 and a few large ones, for the loops against OpenSSL
    inline std::vector<size_t> lengths(std::mt19937_64& rng, size_t random_count) {
        std::vector<size_t> n;
        for (size_t i = 0; i <= 80; ++i) {
            n.push_back(i);
        }
        // the edges of the paths too: ChaCha20's vector groups from 129,
        // two groups from 449, nine blocks at 576, the AEAD's 7 · 576 pieces
        for (size_t i : {111, 112, 113, 127, 128, 129, 255, 256, 257, 447, 448, 449, 511, 512, 513, 575, 576, 577, 1023, 1024}) {
            n.push_back(i);
        }
        for (size_t i = 0; i < random_count; ++i) {
            n.push_back(rng() % 1025);
        }
        for (size_t i : {size_t(4031), size_t(4032), size_t(4033), size_t(4095), size_t(4096), size_t(4097), size_t(65536 + 13), (size_t(1) << 20) + 5}) {
            n.push_back(i);
        }
        return n;
    }

    // --- OpenSSL ---

    inline const EVP_CIPHER* gcm_cipher(size_t key_size) {
        return key_size == 16 ? EVP_aes_128_gcm() : key_size == 24 ? EVP_aes_192_gcm() : EVP_aes_256_gcm();
    }

    inline const EVP_CIPHER* ctr_cipher(size_t key_size) {
        return key_size == 16 ? EVP_aes_128_ctr() : key_size == 24 ? EVP_aes_192_ctr() : EVP_aes_256_ctr();
    }

    inline const EVP_CIPHER* ecb_cipher(size_t key_size) {
        return key_size == 16 ? EVP_aes_128_ecb() : key_size == 24 ? EVP_aes_192_ecb() : EVP_aes_256_ecb();
    }

    // An AEAD's seal through OpenSSL: ciphertext || tag (16)
    inline bytes ossl_seal(const EVP_CIPHER* c, const bytes& key, const bytes& nonce, const bytes& pt, const bytes& aad) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, c, nullptr, nullptr, nullptr);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, int(nonce.size()), nullptr);
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, u8(key), u8(nonce));
        int len = 0;
        if (!aad.empty()) {
            EVP_EncryptUpdate(ctx, nullptr, &len, u8(aad), int(aad.size()));
        }
        bytes out(pt.size() + 16);
        int n = 0;
        if (!pt.empty()) {
            EVP_EncryptUpdate(ctx, u8(out), &n, u8(pt), int(pt.size()));
        }
        int m = 0;
        EVP_EncryptFinal_ex(ctx, u8(out) + n, &m);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, u8(out) + pt.size());
        EVP_CIPHER_CTX_free(ctx);
        return out;
    }

    // An AEAD's open through OpenSSL: false when the tag does not verify
    inline bool ossl_open(const EVP_CIPHER* c, const bytes& key, const bytes& nonce, const bytes& sealed, const bytes& aad, bytes& pt) {
        if (sealed.size() < 16) {
            return false;
        }
        const size_t n = sealed.size() - 16;
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(ctx, c, nullptr, nullptr, nullptr);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, int(nonce.size()), nullptr);
        EVP_DecryptInit_ex(ctx, nullptr, nullptr, u8(key), u8(nonce));
        int len = 0;
        if (!aad.empty()) {
            EVP_DecryptUpdate(ctx, nullptr, &len, u8(aad), int(aad.size()));
        }
        pt.assign(n, std::byte(0));
        int k = 0;
        if (n > 0) {
            EVP_DecryptUpdate(ctx, u8(pt), &k, u8(sealed), int(n));
        }
        bytes tag(sealed.end() - 16, sealed.end());
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, u8(tag));
        int m = 0;
        int ok = EVP_DecryptFinal_ex(ctx, u8(pt) + k, &m);
        EVP_CIPHER_CTX_free(ctx);
        return ok > 0;
    }

    // A stream cipher (CTR, ChaCha20) or ECB through OpenSSL in one call
    inline bytes ossl_crypt(const EVP_CIPHER* c, const bytes& key, const bytes& iv, const bytes& in, bool encrypt = true) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_CipherInit_ex(ctx, c, nullptr, u8(key), iv.empty() ? nullptr : u8(iv), encrypt ? 1 : 0);
        EVP_CIPHER_CTX_set_padding(ctx, 0);
        bytes out(in.size() + 16);
        int n = 0, m = 0;
        if (!in.empty()) {
            EVP_CipherUpdate(ctx, u8(out), &n, u8(in), int(in.size()));
        }
        EVP_CipherFinal_ex(ctx, u8(out) + n, &m);
        EVP_CIPHER_CTX_free(ctx);
        out.resize(size_t(n + m));
        return out;
    }

    inline bytes ossl_poly1305(const bytes& key, const bytes& msg) {
        EVP_MAC* mac = EVP_MAC_fetch(nullptr, "POLY1305", nullptr);
        EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
        EVP_MAC_init(ctx, u8(key), key.size(), nullptr);
        EVP_MAC_update(ctx, u8(msg), msg.size());
        bytes tag(16);
        size_t n = 0;
        EVP_MAC_final(ctx, u8(tag), &n, 16);
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        return tag;
    }

    // Which path this build tests
    inline const char* path_name() {
#if SGCL_CRYPTO_ARM64_AES
        return "arm64 (AESE/AESMC, PMULL, NEON)";
#elif SGCL_CRYPTO_NEON
        return "NEON ChaCha20, portable AES and GHASH";
#else
        return "portable (bitsliced AES, GHASH on integer products, C++ ChaCha20/Poly1305)";
#endif
    }
}
