//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of RSA share: OpenSSL 3 as the oracle (keys made by it and
// read from DER, signatures in PKCS #1 v1.5 and PSS made and checked on a
// raw digest, OAEP in both directions, PKCS #1, PKCS #8 and SPKI written by
// it), a key of each size made once per run, and the hex and random helpers
// of ecc_common.h.
#pragma once

#include "ecc_common.h"

#include <openssl/err.h>
#include <openssl/rsa.h>

#include <map>
#include <mutex>
#include <optional>

namespace rsa_test {
    using namespace ecc_test;
    using crypto::hash_id;

    inline const EVP_MD* md_of(hash_id id) {
        switch (id) {
            case hash_id::sha1: return EVP_sha1();
            case hash_id::sha224: return EVP_sha224();
            case hash_id::sha256: return EVP_sha256();
            case hash_id::sha384: return EVP_sha384();
            case hash_id::sha512: return EVP_sha512();
            case hash_id::sha512_256: return EVP_sha512_256();
            case hash_id::sha3_224: return EVP_sha3_224();
            case hash_id::sha3_256: return EVP_sha3_256();
            case hash_id::sha3_384: return EVP_sha3_384();
            case hash_id::sha3_512: return EVP_sha3_512();
        }
        return nullptr;
    }

    inline const hash_id all_hashes[] = {hash_id::sha1, hash_id::sha224, hash_id::sha256, hash_id::sha384, hash_id::sha512,
                                         hash_id::sha512_256, hash_id::sha3_224, hash_id::sha3_256, hash_id::sha3_384, hash_id::sha3_512};

    inline bytes_t ossl_digest(hash_id id, const bytes_t& msg) {
        bytes_t out(EVP_MAX_MD_SIZE);
        unsigned int n = 0;
        EVP_Digest(msg.data(), msg.size(), out.data(), &n, md_of(id), nullptr);
        out.resize(n);
        return out;
    }

    inline Pkey ossl_generate(unsigned bits) {
        return Pkey(EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", size_t(bits)));
    }

    inline Pkey ossl_private_from_der(const bytes_t& der) {
        const unsigned char* p = der.data();
        return Pkey(d2i_AutoPrivateKey(nullptr, &p, long(der.size())));
    }

    inline Pkey ossl_public_from_spki(const bytes_t& der) {
        const unsigned char* p = der.data();
        return Pkey(d2i_PUBKEY(nullptr, &p, long(der.size())));
    }

    // RSAPrivateKey of PKCS #1: OpenSSL's "type-specific" form
    inline bytes_t ossl_pkcs1_private(const Pkey& key) {
        unsigned char* p = nullptr;
        int n = i2d_PrivateKey(key.p, &p);
        bytes_t out(p, p + (n > 0 ? n : 0));
        OPENSSL_free(p);
        return out;
    }

    // RSAPublicKey of PKCS #1
    inline bytes_t ossl_pkcs1_public(const Pkey& key) {
        unsigned char* p = nullptr;
        int n = i2d_PublicKey(key.p, &p);
        bytes_t out(p, p + (n > 0 ? n : 0));
        OPENSSL_free(p);
        return out;
    }

    // A signature of a raw digest: PKCS #1 v1.5, or PSS with the salt length
    // given (RSA_PSS_SALTLEN_DIGEST, _MAX, or a number) and MGF1 over the
    // same hash
    inline bytes_t ossl_sign(const Pkey& key, hash_id id, const bytes_t& digest, bool pss, int salt = RSA_PSS_SALTLEN_DIGEST) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        EVP_PKEY_sign_init(ctx);
        EVP_PKEY_CTX_set_rsa_padding(ctx, pss ? RSA_PKCS1_PSS_PADDING : RSA_PKCS1_PADDING);
        EVP_PKEY_CTX_set_signature_md(ctx, md_of(id));
        if (pss) {
            EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, salt);
            EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, md_of(id));
        }
        size_t n = 0;
        EVP_PKEY_sign(ctx, nullptr, &n, digest.data(), digest.size());
        bytes_t sig(n);
        if (EVP_PKEY_sign(ctx, sig.data(), &n, digest.data(), digest.size()) != 1) {
            n = 0;
        }
        sig.resize(n);
        EVP_PKEY_CTX_free(ctx);
        return sig;
    }

    inline bool ossl_verify(const Pkey& key, hash_id id, const bytes_t& digest, const bytes_t& sig, bool pss, int salt = RSA_PSS_SALTLEN_AUTO) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        EVP_PKEY_verify_init(ctx);
        EVP_PKEY_CTX_set_rsa_padding(ctx, pss ? RSA_PKCS1_PSS_PADDING : RSA_PKCS1_PADDING);
        EVP_PKEY_CTX_set_signature_md(ctx, md_of(id));
        if (pss) {
            EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, salt);
            EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, md_of(id));
        }
        bool ok = EVP_PKEY_verify(ctx, sig.data(), sig.size(), digest.data(), digest.size()) == 1;
        EVP_PKEY_CTX_free(ctx);
        return ok;
    }

    inline void set_oaep(EVP_PKEY_CTX* ctx, hash_id id, hash_id mgf, const bytes_t& label) {
        EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);
        EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md_of(id));
        EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, md_of(mgf));
        if (!label.empty()) {
            unsigned char* l = static_cast<unsigned char*>(OPENSSL_malloc(label.size()));
            std::memcpy(l, label.data(), label.size());
            EVP_PKEY_CTX_set0_rsa_oaep_label(ctx, l, int(label.size()));
        }
    }

    inline bytes_t ossl_encrypt_oaep(const Pkey& key, hash_id id, hash_id mgf, const bytes_t& msg, const bytes_t& label) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        EVP_PKEY_encrypt_init(ctx);
        set_oaep(ctx, id, mgf, label);
        size_t n = 0;
        EVP_PKEY_encrypt(ctx, nullptr, &n, msg.data(), msg.size());
        bytes_t out(n);
        if (EVP_PKEY_encrypt(ctx, out.data(), &n, msg.data(), msg.size()) != 1) {
            n = 0;
        }
        out.resize(n);
        EVP_PKEY_CTX_free(ctx);
        return out;
    }

    inline std::optional<bytes_t> ossl_decrypt_oaep(const Pkey& key, hash_id id, hash_id mgf, const bytes_t& ct, const bytes_t& label) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        EVP_PKEY_decrypt_init(ctx);
        set_oaep(ctx, id, mgf, label);
        size_t n = 0;
        std::optional<bytes_t> out;
        if (EVP_PKEY_decrypt(ctx, nullptr, &n, ct.data(), ct.size()) == 1) {
            bytes_t pt(n);
            if (EVP_PKEY_decrypt(ctx, pt.data(), &n, ct.data(), ct.size()) == 1) {
                pt.resize(n);
                out = pt;
            }
        }
        EVP_PKEY_CTX_free(ctx);
        ERR_clear_error();
        return out;
    }

    // Whether OpenSSL's full check (primes, d, CRT values) passes the key
    inline bool ossl_check_private(const Pkey& key) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        bool ok = EVP_PKEY_check(ctx) == 1;
        EVP_PKEY_CTX_free(ctx);
        return ok;
    }

    inline BIGNUM* ossl_bn_param(const Pkey& key, const char* name) {
        BIGNUM* b = nullptr;
        EVP_PKEY_get_bn_param(key.p, name, &b);
        return b;
    }

    // Whether d is below λ = lcm(p - 1, q - 1) and e d = 1 mod λ: the
    // smallest d, as FIPS 186-5 B.3.1 and SP 800-56B check it
    inline bool ossl_d_below_lambda(const Pkey& key) {
        BIGNUM *d = ossl_bn_param(key, "d"), *e = ossl_bn_param(key, "e");
        BIGNUM *p = ossl_bn_param(key, "rsa-factor1"), *q = ossl_bn_param(key, "rsa-factor2");
        BN_CTX* ctx = BN_CTX_new();
        BIGNUM *p1 = BN_dup(p), *q1 = BN_dup(q), *g = BN_new(), *phi = BN_new(), *lambda = BN_new(), *rem = BN_new(), *ed = BN_new();
        BN_sub_word(p1, 1);
        BN_sub_word(q1, 1);
        BN_gcd(g, p1, q1, ctx);
        BN_mul(phi, p1, q1, ctx);
        BN_div(lambda, rem, phi, g, ctx);
        BN_mod_mul(ed, e, d, lambda, ctx);
        bool ok = BN_cmp(d, lambda) < 0 && BN_is_one(ed);
        for (BIGNUM* b : {d, e, p, q, p1, q1, g, phi, lambda, rem, ed}) {
            BN_free(b);
        }
        BN_CTX_free(ctx);
        return ok;
    }

    // The raw private operation of OpenSSL: m^d mod n of k bytes
    inline bytes_t ossl_raw_private(const Pkey& key, const bytes_t& m) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        EVP_PKEY_decrypt_init(ctx);
        EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_NO_PADDING);
        size_t n = 0;
        EVP_PKEY_decrypt(ctx, nullptr, &n, m.data(), m.size());
        bytes_t out(n);
        if (EVP_PKEY_decrypt(ctx, out.data(), &n, m.data(), m.size()) != 1) {
            n = 0;
        }
        out.resize(n);
        EVP_PKEY_CTX_free(ctx);
        return out;
    }

    // And the raw public one: m^e mod n
    inline bytes_t ossl_raw_public(const Pkey& key, const bytes_t& m) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        EVP_PKEY_encrypt_init(ctx);
        EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_NO_PADDING);
        size_t n = 0;
        EVP_PKEY_encrypt(ctx, nullptr, &n, m.data(), m.size());
        bytes_t out(n);
        if (EVP_PKEY_encrypt(ctx, out.data(), &n, m.data(), m.size()) != 1) {
            n = 0;
        }
        out.resize(n);
        EVP_PKEY_CTX_free(ctx);
        return out;
    }

    // An OpenSSL key of each size, made once per run, with its PKCS #8
    struct TestKey {
        Pkey ossl;
        bytes_t pkcs8;
    };

    inline const TestKey& test_key(unsigned bits) {
        static std::mutex lock;
        static std::map<unsigned, TestKey> keys;
        std::lock_guard<std::mutex> g(lock);
        auto it = keys.find(bits);
        if (it == keys.end()) {
            TestKey k{ossl_generate(bits), {}};
            k.pkcs8 = ossl_pkcs8(k.ossl);
            it = keys.emplace(bits, std::move(k)).first;
        }
        return it->second;
    }

    inline crypto::rsa::private_key our_key(const TestKey& k) {
        auto key = crypto::rsa::private_key::from_pkcs8_der(view(k.pkcs8));
        if (!key) {
            throw std::runtime_error(std::string(key.error().message().data(), key.error().message().size()));
        }
        return std::move(*key);
    }

    inline crypto::rsa::public_key our_public(const TestKey& k) {
        return our_key(k).public_key();
    }
}
